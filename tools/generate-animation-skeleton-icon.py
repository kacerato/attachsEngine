"""Author a skin-joint icon in Astra's existing solid angular icon language."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
shapes = []
for a, b in [((16, 6), (16, 16)), ((16, 16), (6, 26)), ((16, 16), (26, 26))]:
    shapes.append(("poly", mark.bar(a, b, 3)))
for x, y in [(16, 6), (16, 16), (6, 26), (26, 26)]:
    shapes += mark.ring(x, y, 4, 2)
named = root / "assets/astra-visual/icons/named"
mark.render(shapes).save(named / "animation/skeleton.png")
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
data["icons"]["animation/skeleton"] = dict(category="animation", name="skeleton", dark_ui_ready=True,
    generation="animation-skeleton-v1", source="tools/generate-animation-skeleton-icon.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
