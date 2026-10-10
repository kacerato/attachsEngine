"""Temporal cue glyphs using Astra's existing angular mark primitives."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
shapes = {
    "event": [("poly", [(18, 3), (7, 18), (14, 18), (12, 29), (26, 12), (19, 12)])],
    "marker": [("poly", [(7, 4), (25, 4), (25, 28), (16, 21), (7, 28)]),
               ("punch_poly", [(12, 8), (20, 8), (20, 13), (12, 13)])],
}
named = root / "assets/astra-visual/icons/named"
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
for name, geometry in shapes.items():
    mark.render(geometry).save(named / "animation" / f"{name}.png")
    data["icons"][f"animation/{name}"] = dict(category="animation", name=name,
        dark_ui_ready=True, generation="animation-cues-v1",
        source="tools/generate-animation-cue-icons.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
