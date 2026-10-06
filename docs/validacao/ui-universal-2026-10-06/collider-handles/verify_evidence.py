"""Verify recorded acceptance without executing the engine or using ADB.

Run from any directory. Comparisons inspect native serializer output; this
script never constructs or modifies a scene archive. Screenshots are manually
reviewed in device-reviewed.json, not inferred to pass from their existence.
"""
import hashlib
import json
import re
from pathlib import Path

out = Path(__file__).resolve().parent
root = out.parents[3]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read(name):
    return (out / name).read_text(encoding="utf-8-sig", errors="replace")

def record(name, value):
    (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def collider_change(before, after, entity, expected_indices):
    a, b = read(before).splitlines(), read(after).splitlines()
    assert len(a) == len(b)
    changes = [(old, new) for old, new in zip(a, b) if old != new]
    assert len(changes) == 1, (before, after, "only one entity may change")
    old, new = changes[0]
    assert old.startswith(str(entity) + " ") and new.startswith(str(entity) + " ")
    pattern = r'("astra\.physics\.collider" 8 ")([^"\n]+)(")'
    original, edited = re.search(pattern, old), re.search(pattern, new)
    assert original and edited
    # Everything outside the selected Collider payload is byte-identical:
    # object TRS, renderer resource/material, component UID and ownership header.
    assert re.sub(pattern, r'\1COLLIDER\3', old) == re.sub(pattern, r'\1COLLIDER\3', new)
    values_a, values_b = original[2].split(), edited[2].split()
    assert len(values_a) == len(values_b)
    indices = [n for n, (x, y) in enumerate(zip(values_a, values_b)) if x != y]
    assert indices == expected_indices, (before, after, indices, expected_indices)
    return {"entity": entity, "changed_payload_indices": indices,
            "before": values_a, "after": values_b, "scene_sha256": sha(out / after)}

assert "3/3 scenarios passed" in read("focused-touch-layout.log")
assert "95/95 scenarios passed" in read("regression.log")
assert "Linking CXX executable aether_gui_tests.exe" in read("host-build-touch-layout.log")
assert "BUILD SUCCESSFUL" in read("android-build-touch-layout.log")
assert "Success" in read("install-final.log")
assert "LaunchState: COLD" in read("cold-reopen.log")
apk = root / "android/app/build/outputs/apk/release/app-release.apk"
apk_hash = sha(apk)
assert apk_hash in read("installed-final-sha256.log").lower()
original = out / "project/scenes/editor.aescene"
assert sha(original) == sha(out / "final-before.aescene") == sha(out / "final-undone.aescene")
assert sha(out / "final-center.aescene") == sha(out / "final-undo-rotation.aescene")
assert sha(out / "final-size.aescene") == sha(out / "final-undo-center.aescene")
combined = sha(out / "final-combined.aescene")
for name in ("final-redone.aescene", "final-reopened.aescene", "final-cylinder-undone.aescene",
             "final-sphere-undone.aescene", "final-capsule-undone.aescene", "final-completed.aescene"):
    assert sha(out / name) == combined, name
changes = {
    "box_size": collider_change("final-before.aescene", "final-size.aescene", 4, [1]),
    "box_center": collider_change("final-size.aescene", "final-center.aescene", 4, [6]),
    "box_rotation": collider_change("final-center.aescene", "final-combined.aescene", 4, [11]),
    "cylinder_height": collider_change("final-combined.aescene", "final-cylinder-height.aescene", 5, [5]),
    "cylinder_radius": collider_change("final-combined.aescene", "final-cylinder-radius.aescene", 5, [4]),
    "sphere_radius": collider_change("final-combined.aescene", "final-sphere-radius.aescene", 6, [4]),
    "capsule_height": collider_change("final-combined.aescene", "final-capsule-height.aescene", 7, [5]),
    "capsule_radius": collider_change("final-combined.aescene", "final-capsule-radius.aescene", 7, [4]),
}
protected = json.loads(read("performance-before.json"))
after = [{"path": item["path"], "sha256": sha(root / item["path"])} for item in protected]
assert protected == after, "Protected performance/runtime sources changed; investigate before closing"
record("performance-after.json", after)
plan = root / "docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md"
assert sha(plan) == "61727152c2e8485b5c9f8b59252074d4fda5856d18a5971b865069d8317d6b29"
review = json.loads(read("device-reviewed.json"))
assert review["apk_sha256"] == apk_hash and len(review["steps"]) >= 19
assert all(step["accepted"] for step in review["steps"])
for step in review["steps"]:
    step["screenshot_sha256"] = sha(out / step["screenshot"])
sources = ["native/editor/editor_collider_handles.h", "native/editor/editor_screen.cpp",
           "native/editor/editor_screen.h", "native/editor/editor_session.cpp", "native/editor/editor_session.h",
           "native/CMakeLists.txt", "tests/native/test_collider_handles.cpp", "tests/native/gui_preview_main.cpp"]
data = {
    "block": "U11 Collider viewport dimensions and local pose handles", "status": "closed",
    "repository": "https://github.com/kacerato/attachsEngine", "branch": "codex/gameplay-runtime",
    "publication": "local; no commit or push in this continuation",
    "native": {"passed": 95, "total": 95, "focused_cases": 3, "real_jolt": True,
               "criteria": ["typed dimensions for four primitives; capsule radius/height independent",
                   "mesh local pose opt-in, no fake mesh dimension editing", "nonuniform/sheared hierarchy",
                   "reflected affine ray math; authored negative scale remains unsupported",
                   "perspective/orthographic, singular and near-axial rejection",
                   "actual pointer/Inspector/property/history/archive/Jolt chain",
                   "single Undo, Cancel, lifecycle and second pointer capture",
                   "rotation crossing pi without a jump", "occlusion, eye, Select and overlays OFF",
                   "800x400 real UI raster, distinct 32px grip targets and actual reversible drag"]},
    "android": {"device": "Xiaomi 25053PC47G / onyx", "apk_sha256": apk_hash,
                "installed_base_matches": True, "project": "UI Collider Handles v1",
                "original_scene_sha256": sha(original), "combined_scene_sha256": combined,
                "scene_changes": changes, "review": review,
                "runtime_boundary": "physical authoring/save/history/cold reopen; Jolt behavior accepted on host, no new physical Play claim"},
    "host_capture": {"path": "host-compact-final.png", "sha256": sha(out / "host-compact-final.png"),
                     "review": "Actual native editor UI raster reviewed; useful viewport, separated grips and leader lines; no GPU/FPS proof"},
    "protected_sources": {"count": len(protected), "changed": 0},
    "source_hashes": {name: sha(root / name) for name in sources},
    "original_roadmap_sha256": sha(plan),
    "remaining_u11": ["faces/vertices authoring", "visual/collision/COM/support/authority diagnostics",
                      "distribution/shape budgets and scaled mobile acceptance"],
    "video": "No new video; individual real device captures inspected; no frame-by-frame video claim",
    "engine_complete": False,
}
record("acceptance.json", data)
progress = root / "docs/planos/ui-universal/PROGRESSO.json"
state = json.loads(progress.read_text(encoding="utf-8-sig"))
state["extensions"]["collider_handles_2026_10_06"] = {
    "plan": "docs/planos/ui-universal/U11-ALCAS-COLLIDER-2026-10-06.md",
    "evidence": "docs/validacao/ui-universal-2026-10-06/collider-handles/acceptance.json",
    "status": "closed block: typed dimensions/local pose handles, history/persistence/Jolt accepted on host and physical Android authoring; wider U11 and universal roadmap remain open",
}
progress.write_text(json.dumps(state, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"Accepted {data['native']['passed']}/95 host scenarios, {len(review['steps'])} physical captures, "
      f"{len(changes)} isolated physical edits, {len(protected)} protected sources unchanged; APK {apk_hash}")
