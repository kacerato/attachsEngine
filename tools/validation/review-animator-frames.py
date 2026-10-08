"""Decode every frame of the mechanism acceptance video; retain all PTS and hashes.

Contact sheets are evidence for human review, not an automated acceptance result.
The green-panel bounds are a visual diagnostic, not a frame-rate measurement.
"""
import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("video", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--frames", type=Path, required=True)
parser.add_argument("--ffmpeg", required=True)
parser.add_argument("--ffprobe", required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
args.frames.mkdir(parents=True, exist_ok=True)
probe = json.loads(subprocess.check_output([
    args.ffprobe, "-v", "error", "-select_streams", "v:0", "-show_frames",
    "-show_entries", "frame=best_effort_timestamp_time", "-of", "json", str(args.video)]))
pts = [float(frame["best_effort_timestamp_time"]) for frame in probe["frames"]]
subprocess.run([args.ffmpeg, "-v", "error", "-y", "-i", str(args.video),
                "-fps_mode", "passthrough", str(args.frames / "%05d.png")], check=True)
paths = sorted(args.frames.glob("*.png"))
assert len(paths) == len(pts) > 0, (len(paths), len(pts))
rows = []
sheets = []
roi = (680, 230, 1400, 740)
for start in range(0, len(paths), 30):
    sheet = Image.new("RGB", (2000, 1410), (16, 18, 22))
    draw = ImageDraw.Draw(sheet)
    for j, path in enumerate(paths[start:start + 30]):
        i = start + j
        with Image.open(path) as source:
            im = source.convert("RGB")
        assert im.size == (1920, 886), im.size
        crop = im.crop(roi)
        mask = Image.new("L", crop.size)
        mask.putdata([255 if g > r * 1.35 and g > b * 1.15 and g > 70 else 0
                      for r, g, b in crop.getdata()])
        bounds = mask.getbbox()
        rows.append({"frame": i, "pts_seconds": pts[i],
                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                     "panel_width": bounds[2] - bounds[0] if bounds else 0,
                     "panel_height": bounds[3] - bounds[1] if bounds else 0})
        x, y = (j % 5) * 400, (j // 5) * 235
        draw.text((x + 5, y + 2), f"{i:04d} | {pts[i]:.3f}s", fill="white")
        sheet.paste(crop.resize((300, 212)), (x + 45, y + 20))
    out = args.output / f"sheet-{len(sheets):02d}.png"
    sheet.save(out)
    sheets.append(out.name)
with (args.output / "frames.csv").open("w", newline="", encoding="utf-8") as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0])
    writer.writeheader()
    writer.writerows(rows)
(args.output / "extraction.json").write_text(json.dumps({
    "video_sha256": hashlib.sha256(args.video.read_bytes()).hexdigest(),
    "decoded_frames": len(paths), "extracted_frames": len(paths), "skipped_frames": 0,
    "duration_last_pts": pts[-1], "roi": roi, "sheets": sheets,
    "review_status": "extracted; human visual review still required"}, indent=2), encoding="utf-8")
print(json.dumps({"frames": len(paths), "sheets": len(sheets), "last_pts": pts[-1]}))
