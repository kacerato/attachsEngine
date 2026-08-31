"""Cook a legacy photographic Radiance HDR panorama into Aether environment assets.

This is an offline importer. Runtime consumes only AETX/AEEN and never needs
FFmpeg, network access, or an HDR image decoder.

This tool is retained for AEEN v1 migration tests and authored HDRI workflows.
It is not the active visible-sky pipeline for the Dirt Road sample; use
``cook-sky-panorama.py`` for that project-owned AEEN v2 resource. Running this
tool against ``samples/dirt-road/Imported`` intentionally replaces the active
environment and therefore requires an explicit output path.
"""
import argparse
import hashlib
import json
import math
import pathlib
import struct
import subprocess
import urllib.request

import numpy as np

SOURCE_URL = "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/4k/sunset_forest_4k.hdr"
SOURCE_PAGE = "https://polyhaven.com/a/sunset_forest"
SOURCE_SHA256 = "471ca0f29b0af12a3e128d2b963b207124ffb68d730e68eadf899d978f1ee8d1"
WIDTH, HEIGHT = 4096, 2048


def resize_linear(data):
    height, width, channels = data.shape
    if height == 1 and width == 1:
        return data
    if height == 1:
        return data.reshape(1, width // 2, 2, channels).mean(axis=2)
    if width == 1:
        return data.reshape(height // 2, 2, 1, channels).mean(axis=1)
    return data.reshape(height // 2, 2, width // 2, 2, channels).mean(axis=(1, 3))


def direction_from_pixel(x, y, width, height):
    phi = ((x + .5) / width - .5) * 2 * math.pi
    theta = ((y + .5) / height) * math.pi
    return np.array([math.sin(theta) * math.cos(phi), math.cos(theta),
                     math.sin(theta) * math.sin(phi)], dtype=np.float32)


def write_texture(path, levels):
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", 0x58544541, 1, WIDTH, HEIGHT, 5,
                                 len(levels), len(payload)) + payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--cache", type=pathlib.Path,
                        default=pathlib.Path("build/environment/source"))
    parser.add_argument("--out", type=pathlib.Path, required=True,
                        help="Explicit output directory; never defaults over the active sample sky")
    parser.add_argument("--sun-intensity", type=float, default=2.1)
    parser.add_argument("--ambient-strength", type=float, default=.28)
    parser.add_argument("--exposure", type=float, default=.72)
    args = parser.parse_args()
    args.cache.mkdir(parents=True, exist_ok=True)
    args.out.mkdir(parents=True, exist_ok=True)

    source = args.cache / "sunset_forest_4k.hdr"
    if not source.exists():
        urllib.request.urlretrieve(SOURCE_URL, source)
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if source_hash != SOURCE_SHA256:
        raise ValueError("Sunset Forest source checksum mismatch")

    raw = args.cache / "sunset_forest_4k.gbrpf32le"
    if not raw.exists():
        subprocess.run([args.ffmpeg, "-v", "error", "-i", str(source), "-frames:v", "1",
                        "-f", "rawvideo", "-pix_fmt", "gbrpf32le", str(raw)], check=True)
    expected = WIDTH * HEIGHT * 3 * 4
    if raw.stat().st_size != expected:
        raise ValueError(f"Unexpected decoded HDR size: {raw.stat().st_size}, expected {expected}")
    planar = np.fromfile(raw, dtype="<f4").reshape(3, HEIGHT, WIDTH)
    # FFmpeg gbrp plane order is G, B, R.
    rgb = np.stack([planar[2], planar[0], planar[1]], axis=-1)
    if not np.isfinite(rgb).all() or rgb.min() < 0:
        raise ValueError("HDR source contains invalid radiance")

    # Locate the broadest bright region after an 8x8 linear reduction. This is
    # less sensitive to isolated fireflies than selecting the hottest texel.
    probe = rgb.reshape(HEIGHT // 8, 8, WIDTH // 8, 8, 3).mean(axis=(1, 3))
    luminance = probe @ np.array([.2126, .7152, .0722], dtype=np.float32)
    sun_y, sun_x = np.unravel_index(int(np.argmax(luminance)), luminance.shape)
    sun_rgb = probe[sun_y, sun_x]
    sun_luminance = max(float(luminance[sun_y, sun_x]), 1e-5)
    sun_color = np.clip(sun_rgb / sun_luminance, .15, 4.0)
    sun_direction = direction_from_pixel(sun_x, sun_y, probe.shape[1], probe.shape[0])

    # Solid-angle weighted average describes the diffuse environment tint.
    row_weight = np.sin((np.arange(HEIGHT, dtype=np.float32) + .5) * math.pi / HEIGHT)
    average = (rgb * row_weight[:, None, None]).sum(axis=(0, 1)) / (row_weight.sum() * WIDTH)
    average_luminance = max(float(average @ np.array([.2126, .7152, .0722])), 1e-5)
    ambient_color = np.clip(average / average_luminance, .15, 4.0)

    levels = []
    mip = rgb
    while True:
        alpha = np.ones((*mip.shape[:2], 1), dtype=np.float32)
        levels.append(np.concatenate([mip, alpha], axis=-1).astype("<f2").tobytes())
        if mip.shape[0] == 1 and mip.shape[1] == 1:
            break
        mip = resize_linear(mip)
    environment = args.out / "environment.aetex"
    write_texture(environment, levels)

    metadata = struct.pack(
        "<4I16f", 0x4E454541, 1, 80, 0,
        *sun_direction, args.sun_intensity,
        *sun_color, math.radians(.27),
        *ambient_color, args.ambient_strength,
        args.exposure, 0.0, float(len(levels) - 1), 0.0)
    (args.out / "environment.aeenv").write_bytes(metadata)

    manifest_path = args.out.parent / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    manifest["environment"] = {
        "asset": "sunset_forest", "author": "Andreas Mischok", "license": "CC0-1.0",
        "source": SOURCE_PAGE, "download": SOURCE_URL, "sourceSha256": source_hash,
        "resolution": [WIDTH, HEIGHT], "encoding": "RGBA16F linear, full mip chain",
        "sunDirection": sun_direction.tolist(), "sunColor": sun_color.tolist(),
        "sunIntensity": args.sun_intensity, "ambientColor": ambient_color.tolist(),
        "ambientStrength": args.ambient_strength, "exposure": args.exposure,
    }
    manifest["outputs"] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(args.out.iterdir()) if path.suffix in (".aetex", ".aemap", ".aeenv")
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    print(json.dumps(manifest["environment"], indent=2))


if __name__ == "__main__":
    main()
