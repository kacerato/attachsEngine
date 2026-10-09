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

import numpy as np
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("video", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--frames", type=Path, required=True)
parser.add_argument("--ffmpeg", required=True)
parser.add_argument("--ffprobe", required=True)
parser.add_argument("--stream", action="store_true", help="Decode every RGB24 frame through a bounded pipe; keep sheets and hashes, without writing all full frames")
parser.add_argument("--retain", type=int, nargs="*", default=[], help="Zero-based full-resolution frame numbers to retain in streaming mode")
parser.add_argument("--size", nargs=2, type=int, default=(1920, 886), metavar=("WIDTH", "HEIGHT"))
parser.add_argument("--roi", nargs=4, type=int, default=(680, 230, 1400, 740), metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
args = parser.parse_args()
if not (0 <= args.roi[0] < args.roi[2] <= args.size[0] and 0 <= args.roi[1] < args.roi[3] <= args.size[1]):
    parser.error("ROI must be non-empty and inside the declared video size")
args.output.mkdir(parents=True, exist_ok=True)
args.frames.mkdir(parents=True, exist_ok=True)
probe = json.loads(subprocess.check_output([
    args.ffprobe, "-v", "error", "-select_streams", "v:0", "-show_frames",
    "-show_entries", "frame=best_effort_timestamp_time", "-of", "json", str(args.video)]))
pts = [float(frame["best_effort_timestamp_time"]) for frame in probe["frames"]]
assert pts, "Video has no decoded timestamps"
if any(i < 0 or i >= len(pts) for i in args.retain):
    parser.error("Retained frame number is outside the video")


def decoded_frames():
    if not args.stream:
        subprocess.run([args.ffmpeg, "-v", "error", "-y", "-i", str(args.video),
                        "-fps_mode", "passthrough", str(args.frames / "%05d.png")], check=True)
        paths = sorted(args.frames.glob("*.png"))
        assert len(paths) == len(pts), (len(paths), len(pts))
        for path in paths:
            with Image.open(path) as source:
                yield source.convert("RGB"), hashlib.sha256(path.read_bytes()).hexdigest()
        return
    width, height = args.size
    byte_count = width * height * 3
    with (args.output / "decoder.log").open("wb") as errors:
        process = subprocess.Popen([args.ffmpeg, "-v", "error", "-i", str(args.video),
                                    "-fps_mode", "passthrough", "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"],
                                   stdout=subprocess.PIPE, stderr=errors)
        try:
            for i in range(len(pts)):
                pixels = bytearray()
                while len(pixels) < byte_count:
                    part = process.stdout.read(byte_count - len(pixels))
                    if not part:
                        raise RuntimeError(f"Decoder stopped inside frame {i}: {len(pixels)}/{byte_count} bytes")
                    pixels.extend(part)
                raw = bytes(pixels)
                frame = Image.frombytes("RGB", (width, height), raw)
                if i in args.retain:
                    frame.save(args.frames / f"{i + 1:05d}.png")
                yield frame, hashlib.sha256(raw).hexdigest()
            assert not process.stdout.read(1), "Decoder produced more frames than the timestamp inventory"
            if process.wait() != 0:
                raise RuntimeError("Video decoder failed; see decoder.log")
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait()
            process.stdout.close()

rows = []
sheets = []
roi = tuple(args.roi)
for i, (im, frame_hash) in enumerate(decoded_frames()):
        j = i % 30
        if j == 0:
            sheet = Image.new("RGB", (2000, 1410), (16, 18, 22))
            draw = ImageDraw.Draw(sheet)
        assert im.size == tuple(args.size), im.size
        crop = im.crop(roi)
        colours = np.asarray(crop, dtype=np.float32)
        r, g, b = (colours[:, :, c] for c in range(3))
        yy, xx = np.nonzero((g > r * 1.35) & (g > b * 1.15) & (g > 70))
        bounds = (int(xx.min()), int(yy.min()), int(xx.max()) + 1, int(yy.max()) + 1) if len(xx) else None
        rows.append({"frame": i, "pts_seconds": pts[i],
                     "sha256": frame_hash,
                     "panel_width": bounds[2] - bounds[0] if bounds else 0,
                     "panel_height": bounds[3] - bounds[1] if bounds else 0})
        x, y = (j % 5) * 400, (j // 5) * 235
        draw.text((x + 5, y + 2), f"{i:04d} | {pts[i]:.3f}s", fill="white")
        crop.thumbnail((390, 212))
        sheet.paste(crop, (x + (400 - crop.width) // 2, y + 20))
        if j == 29 or i + 1 == len(pts):
            out = args.output / f"sheet-{len(sheets):02d}.png"
            sheet.save(out)
            sheets.append(out.name)
assert len(rows) == len(pts), "Some decoded frames were omitted from sheets"
with (args.output / "frames.csv").open("w", newline="", encoding="utf-8") as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0])
    writer.writeheader()
    writer.writerows(rows)
(args.output / "extraction.json").write_text(json.dumps({
    "video_sha256": hashlib.sha256(args.video.read_bytes()).hexdigest(),
    "decoded_frames": len(rows), "extracted_frames": len(rows), "skipped_frames": 0,
    "frame_hash_format": "RGB24 bytes" if args.stream else "PNG file bytes",
    "full_frames_retained": args.retain if args.stream else "all",
    "duration_last_pts": pts[-1], "roi": roi, "sheets": sheets,
    "review_status": "extracted; human visual review still required"}, indent=2), encoding="utf-8")
print(json.dumps({"frames": len(rows), "sheets": len(sheets), "last_pts": pts[-1]}))
