#!/usr/bin/env python3
"""Property address and driver DAG glyphs in the existing Astra mark language.

Run once with pack-icon-atlas.py after coordinating generated headers with the
native build. Concepts are distinct at 22dp: sliders/key versus branched flow.
"""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("astra_mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
bar = lambda a, b, width=3: ("poly", mark.bar(a, b, width))
diamond = lambda x, y, r: ("poly", [(x, y-r), (x+r, y), (x, y+r), (x-r, y)])
glyphs = {
    "property": [bar((4, 7), (28, 7)), bar((4, 16), (28, 16)), bar((4, 25), (28, 25)),
                 diamond(11, 7, 4.5), diamond(23, 16, 4.5), diamond(16, 25, 4.5)],
    "driver": [bar((5, 7), (14, 16)), bar((5, 25), (14, 16)), bar((14, 16), (28, 16)),
               diamond(5, 7, 4), diamond(5, 25, 4), diamond(17, 16, 5),
               ("poly", [(25, 9), (31, 16), (25, 23), (25, 18), (22, 18), (22, 14), (25, 14)])],
}
named = root / "assets/astra-visual/icons/named"
(named / "animation").mkdir(exist_ok=True)
path = named / "catalog.json"
catalog = json.loads(path.read_text(encoding="utf-8"))
for name, shapes in glyphs.items():
    mark.render(shapes).save(named / "animation" / f"{name}.png")
    catalog["icons"][f"animation/{name}"] = dict(category="animation", name=name,
        dark_ui_ready=True, generation="typed-properties-drivers-v1",
        source="tools/generate-animation-property-icons.py; Astra mark primitives")
path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
