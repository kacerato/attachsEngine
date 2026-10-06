"""Plot collected thermal/power signals; no simulated or extrapolated samples."""
import csv
import json
import re
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

run = Path(sys.argv[1])
context = json.loads((run / "context.json").read_text(encoding="utf-8-sig"))
thermal = []
for line in (run / "thermal.jsonl").read_text(encoding="utf-8-sig").splitlines():
    sample = json.loads(line)
    match = re.search(r"temperature:\s*(-?\d+)", sample["battery"])
    if match:
        thermal.append((sample["elapsedSeconds"], int(match[1]) / 10,
                        bool(re.search(r"(?:AC|USB|Wireless|Dock) powered:\s*true", sample["battery"]))))
if len(thermal) < 2:
    raise ValueError("At least two measured thermal samples are required")
power = []
with (run / "power-counters.csv").open(encoding="utf-8") as stream:
    for row in csv.DictReader(stream):
        if row["name"] == "batt.power_mw":
            power.append((int(row["ts"]), abs(float(row["value"])) / 1000))
if len(power) < 2:
    raise ValueError("Measured battery power counters are required")
fig, axes = plt.subplots(2, 1, figsize=(10, 6), layout="constrained")
axes[0].plot([t / 60 for t, _, _ in thermal], [c for _, c, _ in thermal], color="#b64828", linewidth=2)
axes[0].set_ylabel("Temperatura da bateria (°C)")
axes[0].set_title(f"POCO F7 · Sponza · editor parado · mesma qualidade · alvo {context['target_fps']} FPS", loc="left")
axes[1].plot([(t - power[0][0]) / 6e10 for t, _ in power], [w for _, w in power], color="#25728b", linewidth=1)
axes[1].set_ylabel("Potência do aparelho (W)")
axes[1].set_xlabel("Tempo de coleta (minutos)")
for axis in axes:
    axis.grid(alpha=.2)
    axis.spines[["top", "right"]].set_visible(False)
fig.text(.5, -.025, "Bateria ≠ SoC. Potência inclui tela e rádios. Sinais reais; origem de cada coletor independente.", ha="center", fontsize=9)
fig.savefig(run / "endurance.png", dpi=170, bbox_inches="tight")
fig.savefig(run / "endurance.pdf", bbox_inches="tight")
result = dict(samples=len(thermal), elapsed_s=thermal[-1][0]-thermal[0][0],
              battery_start_c=thermal[0][1], battery_end_c=thermal[-1][1],
              battery_min_c=min(c for _, c, _ in thermal), battery_max_c=max(c for _, c, _ in thermal),
              charger_seen_in_thermal_samples=any(p for _, _, p in thermal),
              scope="static editor; battery temperature, not SoC; whole-device power")
(run / "endurance-summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
