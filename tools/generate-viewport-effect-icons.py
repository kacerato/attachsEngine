#!/usr/bin/env python3
"""Gera os ícones autorais das opções visuais do viewport Astra."""

from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1] / "assets/astra-visual/icons"
OUTPUT = ROOT / "viewport-v1"
OUTPUT.mkdir(exist_ok=True)

SIZE = 1024
STROKE = 54
INK = (236, 241, 238, 255)


def point(x: float, y: float) -> tuple[float, float]:
    return x * 32, y * 32


def line(draw: ImageDraw.ImageDraw, points: list[tuple[float, float]], width: int = STROKE) -> None:
    scaled = [point(x, y) for x, y in points]
    draw.line(scaled, fill=INK, width=width, joint="curve")
    radius = width // 2
    for x, y in (scaled[0], scaled[-1]):
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=INK)


def circle(draw: ImageDraw.ImageDraw, cx: float, cy: float, radius: float, width: int = STROKE) -> None:
    draw.ellipse((*(point(cx - radius, cy - radius)), *(point(cx + radius, cy + radius))),
                 outline=INK, width=width)


def sun(draw: ImageDraw.ImageDraw, cx: float = 16, cy: float = 16, radius: float = 5) -> None:
    circle(draw, cx, cy, radius)
    for index in range(8):
        angle = index * math.pi / 4
        line(draw, [(cx + 8 * math.cos(angle), cy + 8 * math.sin(angle)),
                    (cx + 12 * math.cos(angle), cy + 12 * math.sin(angle))], 46)


def make(name: str, painter) -> None:
    image = Image.new("RGBA", (SIZE, SIZE))
    painter(ImageDraw.Draw(image))
    image = image.resize((512, 512), Image.Resampling.LANCZOS)
    image.save(OUTPUT / f"{name}.png")


make("scene-lighting", lambda draw: sun(draw))


def paint_effects(draw: ImageDraw.ImageDraw) -> None:
    circle(draw, 13, 18, 7)
    line(draw, [(18, 11), (23, 6), (26, 9)], 46)
    line(draw, [(22, 7), (27, 12)], 46)
    line(draw, [(7, 27), (26, 27)], 46)


make("scene-effects", paint_effects)


def paint_sky(draw: ImageDraw.ImageDraw) -> None:
    line(draw, [(3, 23), (29, 23)], 46)
    line(draw, [(5, 20), (10, 16), (15, 19), (20, 13), (27, 19)], 46)
    circle(draw, 23, 8, 3.5, 46)


make("sky", paint_sky)


def paint_fog(draw: ImageDraw.ImageDraw) -> None:
    line(draw, [(5, 10), (22, 10)], 52)
    line(draw, [(9, 16), (28, 16)], 52)
    line(draw, [(4, 22), (20, 22)], 52)
    line(draw, [(12, 27), (27, 27)], 52)


make("fog", paint_fog)


def paint_post(draw: ImageDraw.ImageDraw) -> None:
    circle(draw, 16, 16, 11, 46)
    line(draw, [(16, 5), (16, 27)], 42)
    line(draw, [(8, 9), (24, 23)], 42)
    circle(draw, 16, 16, 3, 42)


make("post-processing", paint_post)

catalog_path = ROOT / "named/catalog.json"
catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
for leaf in ("scene-lighting", "scene-effects", "sky", "fog", "post-processing"):
    catalog["icons"][f"lighting/{leaf}"] = {
        "source": f"viewport-v1/{leaf}.png",
        "raster": f"../viewport-v1/{leaf}.png",
        "dark_ui_ready": True,
        "generation": "viewport-v1",
    }
catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("5 ícones de viewport gerados")
