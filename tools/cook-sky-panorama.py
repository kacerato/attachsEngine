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
import subprocess

import numpy as np
from PIL import Image


MAGIC_AETX = 0x58544541
MAGIC_AEEN = 0x4E454541
OUTPUT_WIDTH = 1024
OUTPUT_HEIGHT = 512
SPECULAR_SIZE = 256
SPECULAR_MIP_LEVELS = 9
BRDF_SIZE = 128


def srgb_to_linear(value):
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value):
    value = np.clip(value, 0.0, 1.0)
    return np.where(value <= 0.0031308, value * 12.92,
                    1.055 * value ** (1.0 / 2.4) - 0.055)


def load_linear_source(path, ffmpeg="ffmpeg", ffprobe="ffprobe"):
    """Load LDR through Pillow or HDR/EXR through the offline FFmpeg toolchain."""
    if path.suffix.lower() not in (".hdr", ".exr"):
        with Image.open(path) as source_image:
            source_image.load()
            if source_image.width != source_image.height * 2:
                raise ValueError("Sky source must use an exact 2:1 equirectangular aspect")
            source_srgb = np.asarray(source_image.convert("RGB"), dtype=np.float32) / 255.0
        return srgb_to_linear(source_srgb)

    probe = subprocess.run(
        [ffprobe, "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "csv=p=0:s=x", str(path)],
        check=True, capture_output=True, text=True)
    width, height = (int(value) for value in probe.stdout.strip().split("x"))
    if width != height * 2:
        raise ValueError("Sky source must use an exact 2:1 equirectangular aspect")
    decoded = subprocess.run(
        [ffmpeg, "-v", "error", "-i", str(path), "-frames:v", "1",
         "-f", "rawvideo", "-pix_fmt", "gbrpf32le", "pipe:1"],
        check=True, capture_output=True).stdout
    expected = width * height * 3 * 4
    if len(decoded) != expected:
        raise ValueError(f"Unexpected decoded HDR size: {len(decoded)}, expected {expected}")
    planes = np.frombuffer(decoded, dtype="<f4").reshape(3, height, width)
    linear = np.stack([planes[2], planes[0], planes[1]], axis=-1)
    if not np.isfinite(linear).all() or linear.min() < 0.0:
        raise ValueError("Sky source contains invalid radiance")
    return linear


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


def write_texture(path, levels, width=OUTPUT_WIDTH, height=OUTPUT_HEIGHT, encoding=3):
    payload = b"".join(levels)
    # Encoding 3 is RGBA8 sRGB; encoding 5 is RGBA16F linear HDR.
    header = struct.pack("<6IQ", MAGIC_AETX, 1, width, height,
                         encoding, len(levels), len(payload))
    path.write_bytes(header + payload)


def normalize(value):
    return value / np.maximum(np.linalg.norm(value, axis=-1, keepdims=True), 1e-8)


def octahedral_directions(size):
    """Texel-center directions for the y-up octahedral projection used by GLSL."""
    coordinate = ((np.arange(size, dtype=np.float32) + 0.5) / size) * 2.0 - 1.0
    x, z = np.meshgrid(coordinate, coordinate)
    y = 1.0 - np.abs(x) - np.abs(z)
    folded_x = (1.0 - np.abs(z)) * np.where(x < 0.0, -1.0, 1.0)
    folded_z = (1.0 - np.abs(x)) * np.where(z < 0.0, -1.0, 1.0)
    lower = y < 0.0
    x = np.where(lower, folded_x, x)
    z = np.where(lower, folded_z, z)
    return normalize(np.stack([x, y, z], axis=-1))


def sample_equirectangular(image, directions):
    """Bilinear, horizontally wrapped sampling in linear light (offline only)."""
    height, width, _ = image.shape
    u = (np.arctan2(directions[..., 2], directions[..., 0]) /
         (2.0 * math.pi) + 0.5) * width - 0.5
    v = (np.arccos(np.clip(directions[..., 1], -1.0, 1.0)) /
         math.pi) * height - 0.5
    x0 = np.floor(u).astype(np.int64)
    y0 = np.floor(v).astype(np.int64)
    tx = (u - x0)[..., None]
    ty = (v - y0)[..., None]
    x0 %= width
    x1 = (x0 + 1) % width
    y0 = np.clip(y0, 0, height - 1)
    y1 = np.clip(y0 + 1, 0, height - 1)
    top = image[y0, x0] * (1.0 - tx) + image[y0, x1] * tx
    bottom = image[y1, x0] * (1.0 - tx) + image[y1, x1] * tx
    return top * (1.0 - ty) + bottom * ty


def radical_inverse(index):
    result, fraction = 0.0, 0.5
    while index:
        result += (index & 1) * fraction
        index >>= 1
        fraction *= 0.5
    return result


def bake_specular_environment(source):
    """GGX-prefiltered octahedral radiance; roughness is encoded by mip level."""
    levels = []
    for mip_index in range(SPECULAR_MIP_LEVELS):
        size = max(1, SPECULAR_SIZE >> mip_index)
        normal = octahedral_directions(size)
        roughness = mip_index / (SPECULAR_MIP_LEVELS - 1)
        if mip_index == 0:
            radiance = sample_equirectangular(source, normal)
        else:
            # Robust tangent frame at the poles. Sampling happens at cook time;
            # no trigonometry or convolution remains in the runtime shader.
            helper = np.zeros_like(normal)
            helper[..., 1] = 1.0
            near_pole = np.abs(normal[..., 1]) > 0.999
            helper[near_pole] = (1.0, 0.0, 0.0)
            tangent = normalize(np.cross(helper, normal))
            bitangent = np.cross(normal, tangent)
            alpha = max(0.001, roughness * roughness)
            total = np.zeros_like(normal, dtype=np.float32)
            weight = np.zeros((*normal.shape[:2], 1), dtype=np.float32)
            sample_count = 128 if mip_index <= 4 else 64
            for sample_index in range(sample_count):
                xi_x = sample_index / sample_count
                xi_y = radical_inverse(sample_index)
                cosine = math.sqrt((1.0 - xi_y) /
                                   (1.0 + (alpha * alpha - 1.0) * xi_y))
                sine = math.sqrt(max(0.0, 1.0 - cosine * cosine))
                azimuth = 2.0 * math.pi * xi_x
                half_vector = (tangent * (math.cos(azimuth) * sine) +
                               bitangent * (math.sin(azimuth) * sine) +
                               normal * cosine)
                light = 2.0 * np.sum(normal * half_vector, axis=-1, keepdims=True) * half_vector - normal
                no_l = np.maximum(np.sum(normal * light, axis=-1, keepdims=True), 0.0)
                total += sample_equirectangular(source, light) * no_l
                weight += no_l
            radiance = total / np.maximum(weight, 1e-8)
        rgba = np.concatenate([radiance, np.ones((*radiance.shape[:2], 1), dtype=np.float32)], axis=-1)
        levels.append(rgba.astype("<f2").tobytes())
    return levels


def bake_brdf_lut(sample_count=512):
    """Split-sum GGX BRDF integral shared by every material in the scene."""
    no_v, roughness = np.meshgrid(
        (np.arange(BRDF_SIZE, dtype=np.float32) + 0.5) / BRDF_SIZE,
        (np.arange(BRDF_SIZE, dtype=np.float32) + 0.5) / BRDF_SIZE)
    view = np.stack([np.sqrt(np.maximum(0.0, 1.0 - no_v * no_v)),
                     np.zeros_like(no_v), no_v], axis=-1)
    scale = np.zeros_like(no_v)
    bias = np.zeros_like(no_v)
    alpha = roughness * roughness
    alpha_squared = alpha * alpha
    for sample_index in range(sample_count):
        xi_x = sample_index / sample_count
        xi_y = radical_inverse(sample_index)
        cosine = np.sqrt((1.0 - xi_y) /
                         (1.0 + (alpha_squared - 1.0) * xi_y))
        sine = np.sqrt(np.maximum(0.0, 1.0 - cosine * cosine))
        half_vector = np.stack([math.cos(2.0 * math.pi * xi_x) * sine,
                                math.sin(2.0 * math.pi * xi_x) * sine,
                                cosine], axis=-1)
        vo_h = np.maximum(np.sum(view * half_vector, axis=-1), 0.0)
        light = 2.0 * vo_h[..., None] * half_vector - view
        no_l = np.maximum(light[..., 2], 0.0)
        visibility = 0.5 / np.maximum(
            no_l * np.sqrt(no_v * no_v * (1.0 - alpha_squared) + alpha_squared) +
            no_v * np.sqrt(no_l * no_l * (1.0 - alpha_squared) + alpha_squared), 1e-6)
        weight = 4.0 * visibility * no_l * vo_h / np.maximum(cosine, 1e-6)
        fresnel = (1.0 - vo_h) ** 5
        scale += (1.0 - fresnel) * weight
        bias += fresnel * weight
    rgba = np.stack([scale / sample_count, bias / sample_count,
                     np.zeros_like(scale), np.ones_like(scale)], axis=-1)
    return rgba.astype("<f2").tobytes()


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
    parser.add_argument("--detect-sun", action="store_true",
                        help="derive sun direction/color from the broadest HDR highlight")
    parser.add_argument("--ambient-strength", type=float, default=0.56)
    parser.add_argument("--ambient-saturation", type=float, default=0.62,
                        help="saturacao cromatica da irradiancia global (0..2)")
    parser.add_argument("--ground-bounce", type=float, nargs=3,
                        default=(0.75, 0.82, 0.60), metavar=("R", "G", "B"),
                        help="cor linear RGB da irradiancia vinda do solo")
    parser.add_argument("--exposure", type=float, default=1.0)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    parser.add_argument("--asset", default="day-clouds-panorama-v1")
    parser.add_argument("--authoring", default="Aether project-owned source")
    parser.add_argument("--license", default="Project owned")
    parser.add_argument("--source-url", default="")
    parser.add_argument("--width", type=int, default=OUTPUT_WIDTH,
                        choices=(512, 1024, 2048, 4096),
                        help="panorama width; reflection probe resolution stays independent")
    args = parser.parse_args()
    width, height = args.width, args.width // 2

    if not 0.0 <= args.ambient_saturation <= 2.0:
        raise ValueError("ambient saturation must be between 0 and 2")
    if any(not math.isfinite(value) or value < 0.0 for value in args.ground_bounce):
        raise ValueError("ground bounce must contain three finite non-negative values")

    if not args.source.is_file():
        raise FileNotFoundError(args.source)
    args.out.mkdir(parents=True, exist_ok=True)
    source_hash = hashlib.sha256(args.source.read_bytes()).hexdigest()
    linear = resize_linear(load_linear_source(args.source, args.ffmpeg, args.ffprobe),
                           width, height)
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
    write_texture(environment_path, levels, width, height)
    specular_levels = bake_specular_environment(linear)
    write_texture(args.out / "environment-specular.aetex", specular_levels,
                  SPECULAR_SIZE, SPECULAR_SIZE, 5)
    write_texture(args.out / "environment-brdf.aetex", [bake_brdf_lut()],
                  BRDF_SIZE, BRDF_SIZE, 5)

    row_weight = np.sin((np.arange(height, dtype=np.float32) + 0.5) *
                        math.pi / height)
    average = ((linear * row_weight[:, None, None]).sum(axis=(0, 1)) /
               (row_weight.sum() * width))
    luminance = max(float(average @ np.array([0.2126, 0.7152, 0.0722],
                                             dtype=np.float32)), 1e-5)
    # Preserve the panorama's daylight tint without letting a blue sky turn all
    # shadowed vegetation monochromatically blue. This is an irradiance color,
    # not an arbitrary scene-specific post-process grade.
    ambient_color = np.clip((average / luminance) * 0.6 + 0.4, 0.15, 4.0)
    if args.detect_sun:
        probe = linear.reshape(height // 4, 4, width // 4, 4, 3).mean(axis=(1, 3))
        probe_luminance = probe @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
        sun_y, sun_x = np.unravel_index(int(np.argmax(probe_luminance)), probe_luminance.shape)
        sun_u = (sun_x + .5) / probe.shape[1]
        sun_v = (sun_y + .5) / probe.shape[0]
        sun_rgb = probe[sun_y, sun_x]
        sun_luminance = max(float(probe_luminance[sun_y, sun_x]), 1e-5)
        sun_color = np.clip(sun_rgb / sun_luminance, .15, 4.0).tolist()
    else:
        sun_u, sun_v = args.sun_u, args.sun_v
        sun_color = [1.08, 0.99, 0.88]
    sun_direction = direction_from_uv(sun_u, sun_v)
    sky_zenith_cloud_coverage = [0.08, 0.30, 0.72, 0.46]
    sky_horizon_cloud_density = [0.62, 0.79, 1.08, 0.88]
    ground_color_saturation = [*args.ground_bounce, args.ambient_saturation]
    cloud_light_wind_speed = [1.35, 1.42, 1.52, 0.0035]
    metadata = struct.pack(
        "<4I32f8I", MAGIC_AEEN, 3, 176, 0,
        *sun_direction, args.sun_intensity,
        *sun_color, math.radians(0.27),
        *ambient_color, args.ambient_strength,
        args.exposure, 0.0, float(len(levels) - 1), 0.0,
        *sky_zenith_cloud_coverage,
        *sky_horizon_cloud_density,
        *ground_color_saturation,
        *cloud_light_wind_speed,
        1, SPECULAR_SIZE, SPECULAR_SIZE, SPECULAR_MIP_LEVELS,
        BRDF_SIZE, BRDF_SIZE, 1, 3,
    )
    metadata_path = args.out / "environment.aeenv"
    metadata_path.write_bytes(metadata)

    manifest_path = args.out.parent / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    manifest["environment"] = {
        "asset": args.asset,
        "authoring": args.authoring,
        "license": args.license,
        "sourceUrl": args.source_url,
        "sourcePath": args.source.as_posix(),
        "sourceSha256": source_hash,
        "resolution": [width, height],
        "encoding": "RGBA8 sRGB, seam/pole-safe equirectangular full mip chain",
        "sunDirection": sun_direction,
        "sunColor": sun_color,
        "sunIntensity": args.sun_intensity,
        "ambientColor": ambient_color.tolist(),
        "ambientStrength": args.ambient_strength,
        "exposure": args.exposure,
        "groundBounceColor": ground_color_saturation[:3],
        "saturation": ground_color_saturation[3],
        "environmentResourceVersion": 3,
        "specularRepresentation": "octahedral-ggx-prefiltered-rgba16f",
        "specularResolution": [SPECULAR_SIZE, SPECULAR_SIZE],
        "specularMipLevels": SPECULAR_MIP_LEVELS,
        "brdfLut": [BRDF_SIZE, BRDF_SIZE],
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
