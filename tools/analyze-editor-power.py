"""Read device battery counters from Perfetto; never attribute them to the app."""
import csv
import io
import json
import re
import subprocess
import sys
from pathlib import Path

root, processor = map(Path, sys.argv[1:3])
query = """SELECT c.ts, t.name, c.value FROM counter c
JOIN counter_track t ON t.id=c.track_id
WHERE t.name GLOB 'batt.*' OR t.name GLOB 'power.rails.*'
ORDER BY c.ts, t.name"""
runs = []
for trace in sorted(root.glob("*/power.pftrace")):
    result = subprocess.run([sys.executable, str(processor), "query", str(trace), query],
                            capture_output=True, text=True, check=True)
    trace.with_name("power-counters.csv").write_text(result.stdout, encoding="utf-8")
    trace.with_name("power-analysis-log.txt").write_text(result.stderr, encoding="utf-8")
    tracks = {}
    for row in csv.DictReader(io.StringIO(result.stdout)):
        tracks.setdefault(row["name"], []).append((int(row["ts"]), float(row["value"])))
    entry = {"run": trace.parent.name, "scope": "whole device, battery HAL",
             "isolated_app_power": False, "power_rails_available": any(
                 name.startswith("power.rails.") for name in tracks)}
    powered = []
    for stage in ("before", "after"):
        snapshot = trace.with_name(f"battery-{stage}.txt")
        if snapshot.exists():
            powered.append(bool(re.search(r"(?:AC|USB|Wireless|Dock) powered:\s*true",
                                          snapshot.read_text(encoding="utf-8-sig"))))
    entry["unplugged_at_endpoints"] = len(powered) == 2 and not any(powered)
    power = tracks.get("batt.power_mw", [])
    if len(power) > 1 and power[-1][0] > power[0][0]:
        elapsed = (power[-1][0]-power[0][0])/1e9
        # Zero-order hold: no fabricated samples across variable poll intervals.
        energy_mj = sum(abs(a[1])*(b[0]-a[0])/1e9 for a, b in zip(power, power[1:]))
        entry.update(samples=len(power), elapsed_s=elapsed, mean_battery_power_mw=energy_mj/elapsed,
                     battery_energy_j=energy_mj/1000, power_measured=True)
    else:
        entry["power_measured"] = False
    charge = tracks.get("batt.charge_uah", [])
    if charge:
        entry.update(charge_start_uah=charge[0][1], charge_end_uah=charge[-1][1],
                     charge_delta_uah=charge[-1][1]-charge[0][1])
    entry["valid_discharge_comparison"] = entry["unplugged_at_endpoints"] and entry.get("charge_delta_uah", 0) < 0
    entry["tracks"] = {name: {"samples": len(values), "min": min(v for _, v in values),
                              "max": max(v for _, v in values)} for name, values in tracks.items()}
    trace.with_name("power-summary.json").write_text(json.dumps(entry, indent=2), encoding="utf-8")
    runs.append(entry)
summary = {"runs": runs, "power_source": "Perfetto battery current and voltage counters",
           "scope": "whole device; not isolated GPU/app power"}
aggregates = {}
for variant in ("A", "B"):
    paired = [run for run in runs if run["run"].endswith(variant) and run.get("power_measured")
              and run.get("valid_discharge_comparison")]
    if paired:
        duration = sum(run["elapsed_s"] for run in paired)
        energy = sum(run["battery_energy_j"] for run in paired)
        aggregates[variant] = {"runs": len(paired), "elapsed_s": duration,
                               "energy_j": energy, "mean_battery_power_w": energy/duration}
summary["aggregates"] = aggregates
if "A" in aggregates and "B" in aggregates:
    summary["observed_power_reduction_percent"] = 100 * (1-aggregates["B"]["mean_battery_power_w"] /
                                                          aggregates["A"]["mean_battery_power_w"])
(root / "power-comparison.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
print(json.dumps({k: v for k, v in summary.items() if k != "runs"}, indent=2))
