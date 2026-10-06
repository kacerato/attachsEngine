"""Summarize the paired device runs; never average percentiles or invent power."""
import json
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

root = Path(sys.argv[1])
runs = []
windows = {"A": [], "B": []}
for folder in sorted(root.iterdir()):
    if not folder.is_dir() or not (folder / "windows.json").exists():
        continue
    variant = folder.name[-1]
    if variant not in windows:
        continue
    values = json.loads((folder / "windows.json").read_text(encoding="utf-8-sig"))
    values = values if isinstance(values, list) else [values]
    context = json.loads((folder / "context.json").read_text(encoding="utf-8-sig"))
    surface = json.loads((folder / "surface.json").read_text(encoding="utf-8-sig"))
    elapsed = sum(w["elapsed_ms"] for w in values)
    entry = dict(run=folder.name, variant=variant, windows=len(values),
                 engine_fps=600 * len(values) * 1000 / elapsed,
                 surface_continuous=surface["continuous"],
                 displayed_fps=surface["summary"]["displayedFps"] if surface["continuous"] else None,
                 context=context)
    for metric in ("gpu_frame_ms", "thread_cpu_ms", "gpu_opaque_ms", "gpu_post_ms", "gpu_ui_ms"):
        entry[metric] = dict(mean=sum(w[metric]["mean"] for w in values)/len(values),
                             worst_window_p95=max(w[metric]["p95"] for w in values),
                             worst_window_p99=max(w[metric]["p99"] for w in values))
    entry["visible_triangles"] = sorted({w["visible_triangles"] for w in values})
    if all("scene_reused_frames" in w for w in values):
        entry["scene_reused_frames"] = sum(w["scene_reused_frames"] for w in values)
        entry["scene_reuse_ratio"] = entry["scene_reused_frames"] / (600 * len(values))
    entry["thermal_pressure"] = sorted({w["thermal_pressure"] for w in values})
    entry["render_scale_range"] = [min(w["render_scale_min"] for w in values),
                                    max(w["render_scale_max"] for w in values)]
    entry["memory"] = {metric: dict(min=min(w[metric] for w in values),
                                    max=max(w[metric] for w in values))
                       for metric in ("ram_rss_bytes", "gpu_engine_used_bytes", "gpu_buffer_used_bytes",
                                      "gpu_texture_used_bytes", "gpu_render_target_used_bytes")
                       if all(metric in w for w in values)}
    power_summary = folder / "power-summary.json"
    if power_summary.exists():
        entry["power"] = json.loads(power_summary.read_text(encoding="utf-8"))
    for stage in ("before", "after"):
        battery = (folder / f"battery-{stage}.txt").read_text(encoding="utf-8-sig")
        match = re.search(r"temperature:\s*(-?\d+)", battery)
        entry[f"battery_{stage}_c"] = int(match[1])/10 if match else None
    runs.append(entry)
    windows[variant].extend(values)

summary = {}
for variant, values in windows.items():
    if values:
        summary[variant] = dict(
            frames=600*len(values),
            gpu_mean_ms=sum(w["gpu_frame_ms"]["mean"] for w in values)/len(values),
            gpu_worst_window_p95_ms=max(w["gpu_frame_ms"]["p95"] for w in values),
            cpu_mean_ms=sum(w["thread_cpu_ms"]["mean"] for w in values)/len(values),
            engine_fps=600*len(values)*1000/sum(w["elapsed_ms"] for w in values))
images = []
a_folders = [root / r["run"] for r in runs if r["variant"] == "A"]
for run in runs:
    if run["variant"] != "B" or not a_folders:
        continue
    a = np.asarray(Image.open(a_folders[0]/"screen.png").convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(root/run["run"]/"screen.png").convert("RGB"), dtype=np.int16)
    if a.shape != b.shape:
        raise ValueError("Captures have different dimensions")
    delta = np.abs(a-b)
    images.append(dict(run=run["run"], reference=a_folders[0].name,
                       max_channel_difference=int(delta.max()),
                       changed_pixels=int(np.count_nonzero(np.max(delta, axis=2))),
                       changed_pixels_gt3=int(np.count_nonzero(np.max(delta, axis=2)>3)),
                       mean_channel_difference=float(delta.mean())))
result = dict(runs=runs, aggregates=summary, image_comparisons=images,
              percentile_method="worst window percentile, not aggregate percentile",
              power_measured=bool(runs) and all(r.get("power", {}).get("power_measured", False) for r in runs),
              power_discharge_valid=bool(runs) and all(r.get("power", {}).get("valid_discharge_comparison", False) for r in runs),
              power_scope="whole device, not isolated app/GPU")
if all(v in summary for v in ("A", "B")):
    result["gpu_reduction_percent"] = 100*(1-summary["B"]["gpu_mean_ms"]/summary["A"]["gpu_mean_ms"])
    result["engine_fps_change_percent"] = 100*(summary["B"]["engine_fps"]/summary["A"]["engine_fps"]-1)
(root / "comparison.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps({k:v for k,v in result.items() if k != "runs"}, indent=2))
