"""Material object preview: authored cube inside angular view corners."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
geometry = [
    ("poly", [(3, 11), (3, 3), (11, 3), (11, 6), (6, 6), (6, 11)]),
    ("poly", [(21, 3), (29, 3), (29, 11), (26, 11), (26, 6), (21, 6)]),
    ("poly", [(3, 21), (6, 21), (6, 26), (11, 26), (11, 29), (3, 29)]),
    ("poly", [(26, 21), (29, 21), (29, 29), (21, 29), (21, 26), (26, 26)]),
    ("poly", [(16, 7), (24, 12), (16, 17), (8, 12)]),
    ("poly", [(8, 14), (15, 19), (15, 26), (8, 21)]),
    ("poly", [(17, 19), (24, 14), (24, 21), (17, 26)]),
]
named = root / "assets/astra-visual/icons/named"
(named / "materials").mkdir(exist_ok=True)
mark.render(geometry).save(named / "materials/object-preview.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["materials/object-preview"] = dict(category="materials", name="object-preview",
    dark_ui_ready=True, generation="material-preview-v1",
    source="tools/generate-material-preview-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
