"""N01 interval and cue-group glyphs using the production Astra primitives."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mark", root / "tools/generate-astra-mark-icons.py")
mark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mark)
shapes = {
    "region": [("poly", [(3, 5), (10, 5), (10, 9), (7, 9), (7, 23), (10, 23), (10, 27), (3, 27)]),
               ("poly", [(29, 5), (22, 5), (22, 9), (25, 9), (25, 23), (22, 23), (22, 27), (29, 27)]),
               ("poly", [(11, 11), (21, 11), (21, 21), (11, 21)]),
               ("poly", [(11, 3), (21, 3), (21, 7), (11, 7)])],
    "curve-compare": [("poly", [(3, 6), (7, 6), (7, 25), (29, 25), (29, 29), (3, 29)]),
                      ("poly", [(8, 20), (14, 10), (18, 10), (24, 20), (29, 20), (29, 23), (22, 23), (16, 14), (11, 23), (8, 23)]),
                      ("poly", [(8, 10), (12, 10), (12, 13), (8, 13)]),
                      ("poly", [(17, 18), (21, 18), (21, 21), (17, 21)]),
                      ("poly", [(25, 9), (29, 9), (29, 12), (25, 12)])],
    "curve-normalize": [("poly", [(4, 3), (4, 29), (8, 29), (8, 3)]),
                        ("poly", [(12, 8), (21, 8), (21, 5), (29, 10), (21, 15), (21, 12), (12, 12)]),
                        ("poly", [(29, 20), (20, 20), (20, 17), (12, 22), (20, 27), (20, 24), (29, 24)])],
    "cycle": [("poly", [(4, 5), (24, 5), (24, 11), (20, 11), (20, 9), (8, 9), (8, 14), (4, 14)]),
              ("poly", [(16, 10), (28, 10), (22, 17)]),
              ("poly", [(28, 27), (8, 27), (8, 21), (12, 21), (12, 23), (24, 23), (24, 18), (28, 18)]),
              ("poly", [(4, 22), (10, 15), (16, 22)])],
    "arrange": [("poly", [(4, 5), (28, 5), (28, 8), (4, 8)]),
                ("poly", [(5, 13), (9, 9), (13, 13), (9, 17)]),
                ("poly", [(12, 21), (16, 17), (20, 21), (16, 25)]),
                ("poly", [(19, 13), (23, 9), (27, 13), (23, 17)]),
                ("poly", [(4, 27), (28, 27), (28, 30), (4, 30)])],
    "selection-set": [("poly", [(3, 3), (13, 3), (13, 7), (7, 7), (7, 13), (3, 13)]),
                      ("poly", [(19, 3), (29, 3), (29, 13), (25, 13), (25, 7), (19, 7)]),
                      ("poly", [(3, 19), (7, 19), (7, 25), (13, 25), (13, 29), (3, 29)]),
                      ("poly", [(25, 19), (29, 19), (29, 29), (19, 29), (19, 25), (25, 25)]),
                      ("poly", [(11, 11), (16, 8), (21, 11), (21, 17), (16, 20), (11, 17)])],
    "pose-tools": [("poly", [(3, 4), (12, 4), (12, 8), (7, 8), (7, 24), (12, 24), (12, 28), (3, 28)]),
                   ("poly", [(20, 4), (29, 4), (29, 28), (20, 28), (20, 24), (25, 24), (25, 8), (20, 8)]),
                   ("poly", [(12, 12), (20, 12), (23, 16), (20, 20), (12, 20), (9, 16)])],
    "interval": [("poly", [(3, 5), (10, 5), (10, 9), (7, 9), (7, 23), (10, 23), (10, 27), (3, 27)]),
                 ("poly", [(29, 5), (22, 5), (22, 9), (25, 9), (25, 23), (22, 23), (22, 27), (29, 27)]),
                 ("poly", [(11, 14), (21, 14), (21, 18), (11, 18)])],
    "cue-group": [("poly", [(5, 6), (14, 6), (14, 17), (9, 13), (5, 17)]),
                  ("poly", [(18, 6), (27, 6), (27, 17), (22, 13), (18, 17)]),
                  ("poly", [(5, 22), (27, 22), (27, 26), (5, 26)])],
}
named = root / "assets/astra-visual/icons/named"
path = named / "catalog.json"
data = json.loads(path.read_text(encoding="utf-8"))
for name, geometry in shapes.items():
    mark.render(geometry).save(named / "animation" / f"{name}.png")
    data["icons"][f"animation/{name}"] = dict(category="animation", name=name,
        dark_ui_ready=True, generation="animation-timeline-v1",
        source="tools/generate-animation-timeline-icons.py; Astra mark primitives")
path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
