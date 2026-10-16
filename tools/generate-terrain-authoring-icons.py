#!/usr/bin/env python3
"""Generate additive terrain authoring icons for Astra's vector/atlas pipeline.

The SVG and 96 px transparent PNG share the same geometric primitives. Source
marks use the existing terrain family's dark stroke; pack-icon-atlas.py applies
the normal dark-UI recolouring and produces the matching runtime enum and atlas.
Run this script from any working directory, then run tools/pack-icon-atlas.py
from the repository root.
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / "assets/astra-visual/icons"
SIZE = 96
SCALE = 4
STROKE = 4
INK = (20, 20, 20, 255)

# Keep each silhouette useful at the editor's 24-32 px toolbar sizes. No text,
# decorative background, or image-generation result is required by the runtime.
MARKS = {
    "path-points": {"title":"Edit path points","lines":[[(14,74),(35,44),(61,56),(83,20)],[(9,69),(19,69),(19,79),(9,79),(9,69)],[(30,39),(40,39),(40,49),(30,49),(30,39)],[(56,51),(66,51),(66,61),(56,61),(56,51)],[(78,15),(88,15),(88,25),(78,25),(78,15)]]},
    "path-profile": {"title":"Road and shoulder profile","lines":[[(9,75),(30,45),(66,45),(87,75)],[(9,86),(87,86)],[(30,23),(66,23)],[(30,17),(30,29)],[(66,17),(66,29)]]},
    "rounded-brush": {"title":"Rounded volume brush","lines":[[(20,16),(76,16),(84,24),(84,72),(76,80),(20,80),(12,72),(12,24),(20,16)],[(30,34),(66,34),(66,62),(30,62),(30,34)]]},
    "brush-axes": {"title":"Independent brush axes","lines":[[(46,48),(46,12)],[(46,48),(82,67)],[(46,48),(13,71)],[(37,23),(46,12),(55,23)],[(70,64),(82,67),(79,55)],[(25,67),(13,71),(17,58)]]},

    "raise": {"title":"Raise terrain","lines":[[(12,76),(35,68),(61,68),(84,76)],[(48,65),(48,16)],[(29,35),(48,16),(67,35)]]},
    "lower": {"title":"Lower terrain","lines":[[(12,76),(35,68),(61,68),(84,76)],[(48,16),(48,60)],[(29,41),(48,60),(67,41)]]},
    "flatten": {"title":"Flatten terrain","lines":[[(12,74),(30,50),(66,50),(84,74)],[(22,25),(74,25)]]},
    "smooth": {"title":"Smooth terrain","lines":[[(10,65),(20,65),(30,48),(40,37),(50,37),(60,48),(70,65),(87,65)],[(10,80),(87,80)]]},
    "dig": {"title":"Dig volume","lines":[[(12,48),(27,48),(35,70),(61,70),(69,48),(84,48)],[(48,12),(48,52)],[(34,38),(48,52),(62,38)]]},
    "paint": {"title":"Paint terrain","lines":[[(16,76),(33,58),(45,66),(25,86),(16,76)],[(33,58),(61,23),(76,35),(45,66)],[(61,23),(70,12),(86,24),(76,35)]]},
    "path": {"title":"Terrain path","lines":[[(16,86),(20,69),(47,54),(55,43),(37,31),(44,13)],[(45,86),(51,71),(75,57),(79,41),(63,28),(70,13)]]},
    "noise": {"title":"Terrain noise","lines":[[(8,74),(23,42),(36,58),(51,18),(64,48),(75,36),(88,74)],[(18,83),(78,83)]]},
    "terrace": {"title":"Terrain terraces","lines":[[(10,80),(30,80),(30,60),(50,60),(50,40),(70,40),(70,20),(87,20)],[(10,89),(87,89)]]},
    "restore": {"title":"Restore terrain","lines":[[(19,34),(19,14),(40,14)],[(20,15),(53,15),(77,36),(81,62),(63,82),(35,82),(17,67)],[(30,63),(44,43),(57,61),(66,53)]]},
    "sharpen": {"title":"Sharpen terrain","lines":[[(10,80),(38,43),(48,14),(58,43),(86,80)],[(48,39),(48,82)],[(26,87),(71,87)]]},
    "hole": {"title":"Terrain holes","lines":[[(9,45),(48,22),(87,45),(48,70),(9,45)],[(27,45),(48,33),(69,45),(48,58),(27,45)],[(27,45),(27,64),(48,77),(69,64),(69,45)]]},
    "solid-base": {
        "title": "Solid terrain base",
        "lines": [
            [(10, 43), (27, 33), (38, 39), (53, 18), (64, 32),
             (76, 30), (86, 43), (48, 64), (10, 43)],
            [(10, 43), (10, 69), (48, 90), (86, 69), (86, 43)],
            [(48, 64), (48, 90)],
            [(25, 51), (39, 48), (51, 39), (65, 43)],
        ],
    },
    "surface": {
        "title": "Terrain surface mesh",
        "lines": [
            [(8, 48), (48, 24), (88, 48), (48, 72), (8, 48)],
            [(21, 40), (61, 64)],
            [(35, 32), (75, 56)],
            [(21, 56), (61, 32)],
            [(35, 64), (75, 40)],
        ],
    },
    "manage": {
        "title": "Manage terrain chunks",
        "lines": [
            [(10, 27), (27, 18), (44, 27), (27, 36), (10, 27)],
            [(10, 27), (10, 38), (27, 47), (44, 38), (44, 27)],
            [(27, 36), (27, 47)],
            [(52, 27), (69, 18), (86, 27), (69, 36), (52, 27)],
            [(52, 27), (52, 38), (69, 47), (86, 38), (86, 27)],
            [(69, 36), (69, 47)],
            [(10, 62), (27, 53), (44, 62), (27, 71), (10, 62)],
            [(10, 62), (10, 73), (27, 82), (44, 73), (44, 62)],
            [(27, 71), (27, 82)],
            [(52, 62), (69, 53), (86, 62), (69, 71), (52, 62)],
            [(52, 62), (52, 73), (69, 82), (86, 73), (86, 62)],
            [(69, 71), (69, 82)],
        ],
    },
    "edit": {
        "title": "Edit terrain",
        "lines": [
            [(8, 75), (23, 50), (34, 61), (44, 44)],
            [(8, 75), (44, 87), (79, 72), (70, 62)],
            [(17, 69), (32, 74), (45, 69)],
            [(42, 63), (48, 43), (76, 15), (87, 26), (59, 54),
             (42, 63)],
            [(48, 43), (59, 54)],
            [(70, 21), (81, 32)],
        ],
    },
}


def write_icon(name: str, mark: dict) -> None:
    vector = ICONS / "vector/terrain" / f"{name}.svg"
    raster = ICONS / "named/terrain" / f"{name}.png"
    vector.parent.mkdir(parents=True, exist_ok=True)
    raster.parent.mkdir(parents=True, exist_ok=True)
    polylines = "\n".join(
        '    <polyline points="' + " ".join(f"{x},{y}" for x, y in line) + '"/>'
        for line in mark["lines"]
    )
    vector.write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">\n'
        f'  <title>{mark["title"]}</title>\n'
        '  <g fill="none" stroke="#141414" stroke-width="4" '
        'stroke-linecap="round" stroke-linejoin="round">\n'
        f"{polylines}\n  </g>\n</svg>\n",
        encoding="utf-8",
    )
    image = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE))
    draw = ImageDraw.Draw(image)
    radius = STROKE * SCALE / 2
    for line in mark["lines"]:
        points = [(x * SCALE, y * SCALE) for x, y in line]
        draw.line(points, fill=INK, width=STROKE * SCALE, joint="curve")
        # The same round joins/caps as the editable SVG, including sharp peaks.
        for x, y in points:
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=INK)
    image.resize((SIZE, SIZE), Image.Resampling.LANCZOS).save(raster, optimize=True)


def main() -> None:
    catalog_path = ICONS / "named/catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    for name, mark in MARKS.items():
        write_icon(name, mark)
        catalog["icons"][f"terrain/{name}"] = {
            "source": f"vector/terrain/{name}.svg",
            "generator": "tools/generate-terrain-authoring-icons.py",
        }
    for i,name in enumerate(("grass","rock","soil","path")):
        catalog["icons"][f"terrain/sample-{name}"]={"raster":f"../../../terrain/layer-{i}.png","dark_ui_ready":True,"source":"assets/terrain/SOURCES.md"}
    catalog_path.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print("Generated: " + ", ".join(f"terrain/{name}" for name in MARKS))


if __name__ == "__main__":
    main()
