"""Astra rig mirror and retarget marks; run only in the atlas integration window."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)


def chain(points):
    shapes = [("poly", mark.bar(a, b, 2.8)) for a, b in zip(points, points[1:])]
    shapes += [("circle", (point, 2.5)) for point in points]
    return shapes


mirror = chain([(4, 27), (9, 17), (4, 6)]) + chain([(28, 27), (23, 17), (28, 6)])
mirror += [("poly", [(15, y), (17, y), (17, y + 4), (15, y + 4)]) for y in (3, 10, 17, 24)]
retarget = chain([(4, 26), (4, 17), (9, 8)]) + chain([(23, 27), (28, 17), (23, 4)])
retarget += [("poly", [(10, 15), (16, 15), (16, 11), (22, 17), (16, 23), (16, 19), (10, 19)])]

named = root / "assets/astra-visual/icons/named"
(named / "animation").mkdir(exist_ok=True)
catalog = named / "catalog.json"
data = json.loads(catalog.read_text(encoding="utf-8"))
for name, geometry in (("rig-mirror", mirror), ("retarget", retarget)):
    mark.render(geometry).save(named / f"animation/{name}.png")
    data["icons"][f"animation/{name}"] = dict(
        category="animation", name=name, dark_ui_ready=True,
        generation="rig-authoring-v1",
        source="tools/generate-rig-authoring-icons.py; Astra mark primitives",
    )
catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
