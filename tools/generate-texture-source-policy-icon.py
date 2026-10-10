"""TIFF source interpretation: retained source sheet and three effective channels."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
geometry = [
    ("poly", [(3, 3), (20, 3), (28, 11), (28, 29), (3, 29)]),
    ("punch_poly", [(7, 7), (18, 7), (24, 13), (24, 25), (7, 25)]),
    ("poly", [(18, 3), (21, 3), (21, 10), (28, 10), (28, 13), (18, 13)]),
    ("poly", [(9, 11), (12, 11), (12, 23), (9, 23)]),
    ("poly", [(14, 15), (17, 15), (17, 23), (14, 23)]),
    ("poly", [(19, 19), (22, 19), (22, 23), (19, 23)]),
]
named = root / "assets/astra-visual/icons/named"
(named / "textures").mkdir(exist_ok=True)
mark.render(geometry).save(named / "textures/source-policy.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["textures/source-policy"] = dict(category="textures", name="source-policy",
    dark_ui_ready=True, generation="texture-source-policy-v1",
    source="tools/generate-texture-source-policy-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
