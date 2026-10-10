"""Source/local identity branches meeting at an explicit choice diamond."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
geometry = [
    ("poly", [(3, 3), (9, 3), (9, 9), (3, 9)]),
    ("poly", [(23, 3), (29, 3), (29, 9), (23, 9)]),
    ("poly", [(5, 10), (8, 10), (8, 14), (13, 19), (11, 22), (5, 16)]),
    ("poly", [(24, 10), (27, 10), (27, 16), (21, 22), (19, 19), (24, 14)]),
    ("poly", [(16, 16), (24, 24), (16, 32), (8, 24)]),
]
named = root / "assets/astra-visual/icons/named"
(named / "assets").mkdir(exist_ok=True)
mark.render(geometry).save(named / "assets/source-conflict.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["assets/source-conflict"] = dict(category="assets", name="source-conflict",
    dark_ui_ready=True, generation="source-conflict-v1",
    source="tools/generate-source-conflict-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
