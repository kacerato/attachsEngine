"""Rig-to-resource binding in Astra's existing solid angular mark language."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
shapes = [("poly", [(2, 4), (12, 4), (16, 8), (16, 15), (12, 15), (12, 9), (6, 9), (6, 23), (12, 23), (12, 27), (2, 27)])]
for a, b in [((22, 7), (22, 16)), ((22, 16), (16, 26)), ((22, 16), (29, 26)), ((11, 16), (19, 16))]:
    shapes.append(("poly", mark.bar(a, b, 3)))
for x, y in [(22, 7), (22, 16), (16, 26), (29, 26)]:
    shapes += mark.ring(x, y, 3, 1.5)
named = root / "assets/astra-visual/icons/named"
(named / "assets").mkdir(exist_ok=True)
mark.render(shapes).save(named / "assets/rig-binding.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["assets/rig-binding"] = dict(category="assets", name="rig-binding", dark_ui_ready=True,
    generation="rig-binding-v1", source="tools/generate-rig-binding-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
