"""Preserved source package: angular container with a document identity tab."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
geometry = [
    ("poly", [(3, 12), (15, 6), (27, 12), (15, 18)]),
    ("poly", [(3, 15), (13, 20), (13, 29), (3, 24)]),
    ("poly", [(16, 20), (27, 15), (27, 24), (16, 29)]),
    ("poly", [(19, 2), (25, 2), (29, 6), (29, 11), (25, 13), (19, 10)]),
]
named = root / "assets/astra-visual/icons/named"
(named / "assets").mkdir(exist_ok=True)
mark.render(geometry).save(named / "assets/source-package.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["assets/source-package"] = dict(category="assets", name="source-package",
    dark_ui_ready=True, generation="source-package-v1",
    source="tools/generate-source-package-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
