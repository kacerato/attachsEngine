"""Split the three AI-authored material sheets and derive mobile PBR maps."""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parent / "assets/new-game-materials"
SURFACES = {
    "mercado-nexus": (
        ("nexus-pavement", .72, 1.9, .06),
        ("nexus-titanium", .46, 1.5, .62),
        ("nexus-fabric", .9, 2.0, 0),
        ("nexus-rubber", .84, 2.25, 0),
    ),
    "farol-abissal": (
        ("abyss-ceramic", .68, 2.0, 0),
        ("abyss-steel", .62, 1.7, .48),
        ("abyss-teak", .86, 2.1, 0),
        ("abyss-basalt", .94, 2.45, 0),
    ),
    "expresso-tita": (
        ("titan-granite", .9, 2.2, 0),
        ("titan-armor", .48, 1.55, .7),
        ("titan-walnut", .6, 1.85, 0),
        ("titan-coal", .96, 2.55, .08),
    ),
}


def seamless(pixels: np.ndarray, band: int = 72) -> np.ndarray:
    result = pixels.astype(np.float32).copy()
    ramp = np.square(np.linspace(1, 0, band, dtype=np.float32))[:, None]
    for offset in range(band):
        opposite = result.shape[1] - 1 - offset
        average = (result[:, offset] + result[:, opposite]) * .5
        weight = ramp[offset]
        result[:, offset] = result[:, offset] * (1 - weight) + average * weight
        result[:, opposite] = result[:, opposite] * (1 - weight) + average * weight
    for offset in range(band):
        opposite = result.shape[0] - 1 - offset
        average = (result[offset] + result[opposite]) * .5
        weight = ramp[offset, 0]
        result[offset] = result[offset] * (1 - weight) + average * weight
        result[opposite] = result[opposite] * (1 - weight) + average * weight
    return np.clip(result, 0, 255).astype(np.uint8)


def write_maps(name: str, source: Image.Image, roughness: float,
               normal_strength: float, metallic: float) -> None:
    base = seamless(np.asarray(source.resize((1024, 1024), Image.Resampling.LANCZOS).convert("RGB")))
    Image.fromarray(base, "RGB").save(ROOT / f"{name}-albedo.png", optimize=True)
    Image.fromarray(base, "RGB").save(ROOT / f"{name}-base.jpg", quality=94,
                                       subsampling=0, optimize=True, progressive=True)
    luminance = np.asarray(Image.fromarray(base).convert("L"), dtype=np.float32) / 255
    broad = np.asarray(Image.fromarray((luminance * 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(10)), dtype=np.float32) / 255
    height = np.clip(.5 + (luminance - broad) * 1.85, 0, 1)
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * normal_strength
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * normal_strength
    normal = np.dstack((-dx, dy, np.ones_like(height)))
    normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
    Image.fromarray(np.clip((normal * .5 + .5) * 255, 0, 255).astype(np.uint8), "RGB").save(
        ROOT / f"{name}-normal.png", optimize=True)
    cavities = np.maximum(0, broad - luminance)
    ao = np.clip(1 - cavities * 1.35, .52, 1)
    detail = np.clip(np.abs(luminance - broad) * 1.5, 0, .14)
    rough = np.clip(roughness + detail - (luminance - .5) * .05, 0, 1)
    metal = np.full_like(luminance, metallic)
    arm = np.dstack((ao, rough, metal))
    Image.fromarray(np.clip(arm * 255, 0, 255).astype(np.uint8), "RGB").save(
        ROOT / f"{name}-arm.png", optimize=True)


if __name__ == "__main__":
    for game, surfaces in SURFACES.items():
        sheet = Image.open(ROOT / f"{game}-materials.png").convert("RGB")
        width, height = sheet.size
        boxes = ((0, 0, width // 2, height // 2),
                 (width // 2, 0, width, height // 2),
                 (0, height // 2, width // 2, height),
                 (width // 2, height // 2, width, height))
        for (name, roughness, normal, metallic), box in zip(surfaces, boxes):
            write_maps(name, sheet.crop(box), roughness, normal, metallic)
            print(game, name, "1024x1024 base + normal + ARM")
