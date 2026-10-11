"""Astra IK component marks (N04): two-bone, chain and foot grounding."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)


def chain(points, width=2.8, radius=2.5):
    shapes = [("poly", mark.bar(a, b, width)) for a, b in zip(points, points[1:])]
    shapes += [("circle", (point, radius)) for point in points]
    return shapes


def target(center, radius=4.2):
    x, y = center
    ring = [("poly", mark.bar((x - radius, y), (x + radius, y), 1.6)), ("poly", mark.bar((x, y - radius), (x, y + radius), 1.6))]
    return ring + [("circle", (center, 2.2))]


two_bone = chain([(5, 27), (12, 11), (22, 20)]) + target((27, 9))
two_bone += [("poly", [(24, 17), (26, 13), (27.5, 15.5)])]
ik_chain = chain([(4, 28), (8, 19), (14, 13), (21, 10)], 2.4, 2.2) + target((27, 5), 3.6)
grounding = chain([(9, 4), (13, 15), (11, 24)]) + [("poly", [(11, 24), (21, 24), (21, 27), (11, 27)])]
grounding += [("poly", mark.bar((3, 30), (29, 30), 2.0))]

named = root / "assets/astra-visual/icons/named"
catalog = named / "catalog.json"
data = json.loads(catalog.read_text(encoding="utf-8"))
for name, geometry in (("ik-two-bone", two_bone), ("ik-chain", ik_chain), ("foot-grounding", grounding)):
    mark.render(geometry).save(named / f"component/{name}.png")
    data["icons"][f"component/{name}"] = dict(category="component", name=name, dark_ui_ready=True,
                                               generation="ik-components-v1",
                                               source="tools/generate-ik-component-icons.py; Astra mark primitives")
catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
