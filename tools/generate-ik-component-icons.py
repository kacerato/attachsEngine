"""Astra IK component marks (N04): solvers, grounding and pack C effects."""
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

look_at = chain([(6, 28), (8, 20), (11, 13)]) + [("circle", ((13, 9), 3.2))] + target((26, 8), 3.6)
aim = chain([(5, 26), (12, 19)]) + [("poly", mark.bar((12, 19), (22, 12), 3.2))] + target((27, 7), 3.4)
limit = chain([(5, 26), (15, 16)]) + [("poly", mark.bar((15, 16), (27, 12), 2.4)), ("poly", mark.bar((15, 16), (25, 25), 2.4))]
grounder = chain([(16, 4), (16, 13)]) + chain([(16, 13), (9, 22), (9, 27)], 2.4, 2.2) + chain([(16, 13), (23, 22), (23, 27)], 2.4, 2.2)
grounder += [("poly", mark.bar((3, 30), (29, 30), 2.0))]
finger = chain([(7, 27), (9, 18), (15, 12), (22, 13)], 2.6, 2.3)
offset = target((10, 22), 3.6) + target((24, 9), 3.6) + [("poly", mark.bar((13, 19), (21, 12), 1.8))]
twist = chain([(4, 16), (28, 16)]) + [("poly", mark.bar((10, 9), (14, 23), 1.6)), ("poly", mark.bar((18, 9), (22, 23), 1.6))]

# Pacote C: efeitos procedurais, interacao e corpo inteiro.
def arrow(a, b, width=1.8):
    (x0, y0), (x1, y1) = a, b
    dx, dy = x1 - x0, y1 - y0
    n = max((dx * dx + dy * dy) ** 0.5, 1e-6)
    ux, uy = dx / n, dy / n
    head = [(x1, y1), (x1 - ux * 4.5 - uy * 3, y1 - uy * 4.5 + ux * 3), (x1 - ux * 4.5 + uy * 3, y1 - uy * 4.5 - ux * 3)]
    return [("poly", mark.bar(a, (x1 - ux * 3, y1 - uy * 3), width)), ("poly", head)]


hit_reaction = chain([(8, 28), (12, 18), (17, 9)]) + arrow((29, 6), (20, 12)) + [("poly", mark.bar((24, 16), (28, 20), 1.4))]
recoil = [("poly", mark.bar((6, 20), (24, 14), 3.6))] + chain([(6, 20), (10, 28)], 2.4, 2.2) + arrow((22, 24), (12, 27), 1.6) + [("poly", mark.bar((27, 8), (29, 4), 1.4))]
inertia = chain([(5, 26), (12, 20)]) + chain([(12, 20), (20, 19), (27, 23)], 2.2, 2.0) + [("poly", mark.bar((15, 12), (24, 11), 1.2)), ("poly", mark.bar((17, 8), (26, 7), 1.2))]
amplifier = chain([(6, 27), (14, 16)]) + [("poly", mark.bar((14, 16), (20, 6), 2.2)), ("poly", mark.bar((14, 16), (27, 9), 2.2))] + arrow((21, 15), (27, 12), 1.4)
body_tilt = chain([(12, 28), (16, 17), (21, 7)]) + [("poly", mark.bar((3, 30), (29, 30), 2.0))] + arrow((6, 12), (10, 6), 1.4)
hand_poser = chain([(8, 28), (10, 17)], 2.6, 2.3) + chain([(10, 17), (8, 9)], 2.0, 1.8) + chain([(10, 17), (14, 8)], 2.0, 1.8) + chain([(10, 17), (19, 11)], 2.0, 1.8)
hand_poser += [("poly", mark.bar((22, 20), (29, 20), 1.4)), ("poly", mark.bar((22, 25), (29, 25), 1.4))]
penetration = chain([(4, 26), (12, 16)]) + [("poly", mark.bar((12, 16), (19, 14), 2.6))] + [("poly", [(22, 3), (26, 3), (26, 29), (22, 29)])] + target((19, 14), 3.0)
interaction_object = [("poly", [(10, 14), (24, 14), (24, 28), (10, 28)])] + [("poly", mark.bar((13, 14), (13, 7), 1.6)), ("poly", mark.bar((21, 14), (21, 7), 1.6)), ("poly", mark.bar((13, 7), (21, 7), 1.6))]
interaction_system = chain([(4, 28), (10, 18), (16, 14)]) + arrow((16, 14), (22, 12), 1.6) + [("poly", [(23, 14), (30, 14), (30, 21), (23, 21)])]
full_body = [("circle", ((16, 5), 3.0))] + chain([(16, 9), (16, 17)]) + chain([(16, 11), (8, 15), (4, 10)], 2.0, 1.8) + chain([(16, 11), (24, 15), (28, 10)], 2.0, 1.8)
full_body += chain([(16, 17), (11, 23), (10, 29)], 2.0, 1.8) + chain([(16, 17), (21, 23), (24, 29)], 2.0, 1.8)

fabrik_root = [("circle", ((16, 22), 3.4))] + chain([(16, 22), (9, 14), (5, 6)], 2.0, 1.8) + chain([(16, 22), (16, 12), (17, 4)], 2.0, 1.8)
fabrik_root += chain([(16, 22), (23, 14), (28, 7)], 2.0, 1.8) + [("poly", mark.bar((10, 29), (22, 29), 1.6))]
body_by_targets = [("circle", ((16, 6), 3.2))] + chain([(16, 10), (16, 20)]) + chain([(16, 13), (9, 17)], 2.0, 1.8) + chain([(16, 13), (23, 17)], 2.0, 1.8)
body_by_targets += target((5, 22), 3.2) + target((27, 22), 3.2) + chain([(16, 20), (12, 29)], 2.0, 1.8) + chain([(16, 20), (20, 29)], 2.0, 1.8)

root_motion = chain([(9, 6), (9, 15)]) + chain([(9, 15), (5, 23), (7, 29)], 2.0, 1.8) + chain([(9, 15), (13, 23), (16, 29)], 2.0, 1.8)
root_motion += arrow((17, 13), (29, 13), 2.0) + [("poly", mark.bar((3, 31), (29, 31), 1.6))]

named = root / "assets/astra-visual/icons/named"
catalog = named / "catalog.json"
data = json.loads(catalog.read_text(encoding="utf-8"))
for name, geometry in (("ik-two-bone", two_bone), ("ik-chain", ik_chain), ("foot-grounding", grounding),
                       ("ik-look-at", look_at), ("ik-aim", aim), ("rotation-limit", limit), ("grounder", grounder),
                       ("finger-curl", finger), ("ik-offset", offset), ("twist-relaxer", twist),
                       ("hit-reaction", hit_reaction), ("recoil", recoil), ("inertia", inertia), ("amplifier", amplifier),
                       ("body-tilt", body_tilt), ("hand-poser", hand_poser), ("penetration-avoidance", penetration),
                       ("interaction-object", interaction_object), ("interaction-system", interaction_system),
                       ("full-body-ik", full_body), ("fabrik-root", fabrik_root), ("body-by-targets", body_by_targets), ("root-motion", root_motion)):
    mark.render(geometry).save(named / f"component/{name}.png")
    data["icons"][f"component/{name}"] = dict(category="component", name=name, dark_ui_ready=True,
                                               generation="ik-components-v1",
                                               source="tools/generate-ik-component-icons.py; Astra mark primitives")
catalog.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
