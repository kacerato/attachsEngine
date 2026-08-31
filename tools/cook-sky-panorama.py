"""Cook a project-owned sky panorama into the portable AETX/AEEN resources.

The source image is an offline authoring asset. Runtime receives a compact sRGB
mip chain plus versioned lighting metadata and never decodes PNG or runs cloud
noise per fragment.
"""

import argparse
import hashlib
import json
import math
import pathlib
import struct

import numpy as np
from PIL import Image


MAGIC_AETX = 0x58544541
MAGIC_AEEN = 0x4E454541
OUTPUT_WIDTH = 1024
OUTPUT_HEIGHT = 512


def srgb_to_linear(value):
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value):
    value = np.clip(value, 0.0, 1.0)
    return np.where(value <= 0.0031308, value * 12.92,
                    1.055 * value ** (1.0 / 2.4) - 0.055)


def resize_linear(image, width, height):
    channels = [
        np.asarray(
            Image.fromarray(image[:, :, channel], mode="F").resize(
                (width, height), Image.Resampling.LANCZOS
            ),
            dtype=np.float32,
        )
        for channel in range(3)
    ]
    return np.maximum(np.stack(channels, axis=-1), 0.0)


def downsample_box(image):
    height, width, channels = image.shape
    if height == 1 and width == 1:
        return image
    if height == 1:
        return image.reshape(1, width // 2, 2, channels).mean(axis=2)
    if width == 1:
        return image.reshape(height // 2, 2, 1, channels).mean(axis=1)
    return image.reshape(height // 2, 2, width // 2, 2, channels).mean(axis=(1, 3))


def make_horizontal_seam_safe(image, seam_width=32):
    result = image.copy()
    for offset in range(seam_width):
        left = result[:, offset, :].copy()
        right = result[:, -1 - offset, :].copy()
        average = (left + right) * 0.5
        weight = ((seam_width - offset) / seam_width) ** 2
        result[:, offset, :] = left * (1.0 - weight) + average * weight
        result[:, -1 - offset, :] = right * (1.0 - weight) + average * weight
    return result


def make_poles_spherical(image, pole_width=48):
    """Collapse longitude at the two singularities of an equirectangular map.

    Every texel on the first/last row describes the same direction. AI/photo
    panoramas often violate that rule, which turns small clouds into long
    streaks when the camera pitches toward a pole. Blend each polar band to
    its spherical row average offline; runtime cost remains one texture read.
    """
    result = image.copy()
    for offset in range(pole_width):
        weight = ((pole_width - offset) / pole_width) ** 2
        for row in (offset, image.shape[0] - 1 - offset):
            average = result[row, :, :].mean(axis=0, keepdims=True)
            result[row, :, :] = result[row, :, :] * (1.0 - weight) + average * weight
    return result


def encoded_level(linear):
    rgb = np.rint(linear_to_srgb(linear) * 255.0).astype(np.uint8)
    alpha = np.full((*rgb.shape[:2], 1), 255, dtype=np.uint8)
    return np.concatenate([rgb, alpha], axis=-1).tobytes()


def direction_from_uv(u, v):
    phi = (u - 0.5) * 2.0 * math.pi
    theta = v * math.pi
    return [math.sin(theta) * math.cos(phi), math.cos(theta),
            math.sin(theta) * math.sin(phi)]


def write_texture(path, levels):
    payload = b"".join(levels)
    # Encoding 3 is VK_FORMAT_R8G8B8A8_SRGB. The GPU performs the only sRGB
    # decode; the swapchain performs the matching display encode.
    header = struct.pack("<6IQ", MAGIC_AETX, 1, OUTPUT_WIDTH, OUTPUT_HEIGHT,
                         3, len(levels), len(payload))
    path.write_bytes(header + payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=pathlib.Path,
                        default=pathlib.Path(
                            "samples/dirt-road/Source/day-clouds-panorama-v1.png"))
    parser.add_argument("--out", type=pathlib.Path,
                        default=pathlib.Path("samples/dirt-road/Imported"))
    parser.add_argument("--sun-u", type=float, default=0.786)
    parser.add_argument("--sun-v", type=float, default=0.274)
    parser.add_argument("--sun-intensity", type=float, default=2.6)
    parser.add_argument("--ambient-strength", type=float, default=0.56)
    parser.add_argument("--exposure", type=float, default=1.0)
    args = parser.parse_args()

    if not args.source.is_file():
        raise FileNotFoundError(args.source)
    args.out.mkdir(parents=True, exist_ok=True)
    source_hash = hashlib.sha256(args.source.read_bytes()).hexdigest()
    with Image.open(args.source) as source_image:
        source_image.load()
        if source_image.width * 2 != source_image.height * 4:
            raise ValueError("Sky source must use an exact 2:1 equirectangular aspect")
        source_srgb = np.asarray(source_image.convert("RGB"), dtype=np.float32) / 255.0

    linear = resize_linear(srgb_to_linear(source_srgb), OUTPUT_WIDTH, OUTPUT_HEIGHT)
    linear = make_horizontal_seam_safe(linear)
    linear = make_poles_spherical(linear)
    if not np.isfinite(linear).all():
        raise ValueError("Sky source contains invalid values")

    levels = []
    mip = linear
    while True:
        levels.append(encoded_level(mip))
        if mip.shape[0] == 1 and mip.shape[1] == 1:
            break
        mip = downsample_box(mip)
    environment_path = args.out / "environment.aetex"
    write_texture(environment_path, levels)

    row_weight = np.sin((np.arange(OUTPUT_HEIGHT, dtype=np.float32) + 0.5) *
                        math.pi / OUTPUT_HEIGHT)
    average = ((linear * row_weight[:, None, None]).sum(axis=(0, 1)) /
               (row_weight.sum() * OUTPUT_WIDTH))
    luminance = max(float(average @ np.array([0.2126, 0.7152, 0.0722],
                                             dtype=np.float32)), 1e-5)
    # Preserve the panorama's daylight tint without letting a blue sky turn all
    # shadowed vegetation monochromatically blue. This is an irradiance color,
    # not an arbitrary scene-specific post-process grade.
    ambient_color = np.clip((average / luminance) * 0.6 + 0.4, 0.15, 4.0)
    sun_direction = direction_from_uv(args.sun_u, args.sun_v)
    sun_color = [1.08, 0.99, 0.88]
    sky_zenith_cloud_coverage = [0.08, 0.30, 0.72, 0.46]
    sky_horizon_cloud_density = [0.62, 0.79, 1.08, 0.88]
    ground_color_saturation = [0.75, 0.82, 0.60, 1.0]
    cloud_light_wind_speed = [1.35, 1.42, 1.52, 0.0035]
    metadata = struct.pack(
        "<4I32f", MAGIC_AEEN, 2, 144, 0,
        *sun_direction, args.sun_intensity,
        *sun_color, math.radians(0.27),
        *ambient_color, args.ambient_strength,
        args.exposure, 0.0, float(len(levels) - 1), 0.0,
        *sky_zenith_cloud_coverage,
        *sky_horizon_cloud_density,
        *ground_color_saturation,
        *cloud_light_wind_speed,
    )
    metadata_path = args.out / "environment.aeenv"
    metadata_path.write_bytes(metadata)

    manifest_path = args.out.parent / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    manifest["environment"] = {
        "asset": "day-clouds-panorama-v1",
        "authoring": "OpenAI image generation, curated for Aether",
        "sourcePath": args.source.as_posix(),
        "sourceSha256": source_hash,
        "resolution": [OUTPUT_WIDTH, OUTPUT_HEIGHT],
        "encoding": "RGBA8 sRGB, seam/pole-safe equirectangular full mip chain",
        "sunDirection": sun_direction,
        "sunColor": sun_color,
        "sunIntensity": args.sun_intensity,
        "ambientColor": ambient_color.tolist(),
        "ambientStrength": args.ambient_strength,
        "exposure": args.exposure,
        "groundBounceColor": ground_color_saturation[:3],
        "saturation": ground_color_saturation[3],
        "environmentResourceVersion": 2,
    }
    manifest["outputs"] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(args.out.iterdir())
        if path.suffix in (".aetex", ".aemap", ".aeenv")
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    print(json.dumps(manifest["environment"], indent=2))


if __name__ == "__main__":
    main()
