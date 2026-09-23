"""Prepare the AI-authored sample surfaces for mobile PBR import.

The source PNGs are kept beside the outputs.  This step makes their borders
tile cleanly, writes a compact sRGB base colour map, and derives tangent-space
normal plus occlusion/roughness/metallic maps for the bundled GLB kit.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parent / "assets/highlevel-textures"
SURFACES = {
    "concrete": {"roughness": 0.86, "normal": 2.1, "metallic": 0.0},
    "steel": {"roughness": 0.56, "normal": 1.45, "metallic": 0.16},
    "wood": {"roughness": 0.82, "normal": 1.8, "metallic": 0.0},
    "rock": {"roughness": 0.92, "normal": 2.35, "metallic": 0.0},
}


def seamless(pixels: np.ndarray, band: int = 96) -> np.ndarray:
    """Blend opposite borders without flattening the centre of the material."""
    result = pixels.astype(np.float32).copy()
    ramp = np.square(np.linspace(1.0, 0.0, band, dtype=np.float32))[:, None, None]
    for offset in range(band):
        opposite = result.shape[1] - 1 - offset
        average = (result[:, offset] + result[:, opposite]) * 0.5
        weight = ramp[offset]
        result[:, offset] = result[:, offset] * (1 - weight) + average * weight
        result[:, opposite] = result[:, opposite] * (1 - weight) + average * weight
    ramp = ramp[:, 0]
    for offset in range(band):
        opposite = result.shape[0] - 1 - offset
        average = (result[offset] + result[opposite]) * 0.5
        weight = ramp[offset]
        result[offset] = result[offset] * (1 - weight) + average * weight
        result[opposite] = result[opposite] * (1 - weight) + average * weight
    return np.clip(result, 0, 255).astype(np.uint8)


def prepare(name: str, settings: dict[str, float]) -> None:
    source = Image.open(ROOT / f"{name}-albedo.png").convert("RGB")
    source.thumbnail((1024, 1024), Image.Resampling.LANCZOS)
    if source.size != (1024, 1024):
        source = source.resize((1024, 1024), Image.Resampling.LANCZOS)
    base = seamless(np.asarray(source))
    Image.fromarray(base, "RGB").save(ROOT / f"{name}-base.jpg", quality=94,
                                       subsampling=0, optimize=True, progressive=True)

    # Height is luminance with large-scale illumination removed.  Wrapped
    # finite differences make the derived normal tile at the same boundary.
    luminance = np.asarray(Image.fromarray(base).convert("L"), dtype=np.float32) / 255.0
    broad = np.asarray(Image.fromarray((luminance * 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(10)), dtype=np.float32) / 255.0
    height = np.clip(0.5 + (luminance - broad) * 1.8, 0, 1)
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * settings["normal"]
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * settings["normal"]
    normal = np.dstack((-dx, dy, np.ones_like(height)))
    normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
    normal = np.clip((normal * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    Image.fromarray(normal, "RGB").save(ROOT / f"{name}-normal.png", optimize=True)

    cavities = np.maximum(0, broad - luminance)
    occlusion = np.clip(1 - cavities * 1.25, 0.58, 1)
    detail = np.clip(np.abs(luminance - broad) * 1.4, 0, 0.12)
    roughness = np.clip(settings["roughness"] + detail - (luminance - 0.5) * 0.05, 0, 1)
    metallic = np.full_like(luminance, settings["metallic"])
    if name == "steel":
        # Dark exposed islands read as metal; the blue paint itself stays mostly dielectric.
        metallic = np.clip(settings["metallic"] + np.maximum(0, 0.42 - luminance) * 1.35, 0, 0.72)
    arm = np.dstack((occlusion, roughness, metallic))
    Image.fromarray(np.clip(arm * 255, 0, 255).astype(np.uint8), "RGB").save(
        ROOT / f"{name}-arm.png", optimize=True)


if __name__ == "__main__":
    for surface, values in SURFACES.items():
        prepare(surface, values)
        print(surface, "1024x1024 base + normal + ARM")
