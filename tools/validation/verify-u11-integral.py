"""Verify the retained U11 evidence. --write records the completed acceptance.

Visual review is recorded in the frame/screen ledgers by the reviewer; this script
does not infer visual acceptance from an extracted image count.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--write', action='store_true')
args = p.parse_args()
repo = Path(__file__).resolve().parents[2]
root = repo / 'docs/validacao/ui-universal-2026-10-06/u11-integral'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return (root / path).read_text(encoding='utf-8', errors='replace')


apk = sha(repo / 'android/app/build/outputs/apk/release/app-release.apk')
assert apk == sha(repo / 'build/u11-installed-final.apk')
assert apk == '74cc3895afa18fc7f0bb31ebb65e5a91616e812baf3a0f26711cafa0bf5b7011'
assert 'BUILD SUCCESSFUL' in read('android-build-final.log')
assert '[39/39]' in read('host-build-final.log')
assert '5/5 scenarios passed' in read('focused-tests-final.log')
assert '100/100 scenarios passed' in read('gui-regression-final.log')
assert 'Success' in read('install-final.log')
assert 'U11_DEVICE_PROJECT:' in read('retained-face-reopen.log')
assert '1.0999' in read('retained-vertex-reopen.log')
assert sha(root/'device-vertex-undo.aescene') == sha(root/'device-face-project/scenes/editor.aescene')
assert sha(root/'device-vertex-redo.aescene') == sha(root/'device-vertex-project/scenes/editor.aescene') == sha(root/'device-final.aescene')
assert sha(root/'device-undo.aescene') == sha(repo/'build/U11Integral20261006/scenes/editor.aescene')
assert sha(root/'device-redo.aescene') == sha(root/'device-face-project/scenes/editor.aescene')
original = sha(repo/'docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md')
assert original == '61727152c2e8485b5c9f8b59252074d4fda5856d18a5971b865069d8317d6b29'

source_paths = '''native/editor/editor_collider_topology.h
native/editor/editor_collider_authoring.cpp
native/editor/editor_collider_authoring_ui.inl
native/editor/editor_pick_mesh.h
native/editor/editor_component_visuals.h
native/editor/editor_session.cpp
native/editor/editor_session.h
native/editor/editor_screen.cpp
native/editor/editor_screen.h
native/editor/editor_play_scene.h
native/runtime/scene_physics_diagnostic.cpp
native/runtime/scene_physics.h
native/runtime/scene_dynamic_motor.cpp
native/physics/character_motor.cpp
native/physics/character_motor.h
native/physics/jolt_bridge.cpp
native/physics/jolt_bridge.h
native/resources/convex_bake.cpp
native/resources/convex_bake.h
native/ui/ui_icon_id.h
native/ui/ui_icon_id.cpp
native/CMakeLists.txt
tests/native/test_u11_integral.cpp
tests/native/gui_preview_main.cpp
tools/validation/U11Touch.java
tools/validation/review-u11-frames.py
tools/generate-collider-authoring-icons.py'''.splitlines()

screens = {
    '23-final-character-authoring.png': 'Current Jolt capsule; pre-step Character, no fake COM or ground.',
    '27-final-character-overlay-inspect.png': 'Actual Play camera, white capsule, green support owner 9; Character authority; no Body COM.',
    '28-final-character-filter-details.png': 'Collision filter removes capsule, support remains; More shows cost/cache state.',
    '29-final-body-authoring.png': 'Blue visual, white edited hull, yellow actual COM, green query probes; owner 2.',
    '30-final-body-play.png': 'Play camera alignment; COM .604/0/1.249; actual motor support owner 9.',
    '32-final-curved-conversion.png': 'Explicit approximation/conversion; 114 vertices, 128 faces, 224 triangles.',
    '33-final-vertex-selected.png': 'Actual vertex picking, coordinates and local grips.',
    '34-final-vertex-edited.png': 'Native numeric edit Y=1.1; validated hull, visual sphere preserved.',
    '35-final-vertex-applied.png': 'Published immutable resource, same Collider UID3, real Save.',
    '37-final-scale256.png': '256 actual Colisores, solver COM/probes, useful viewport.',
    '38-final-scale256-cost.png': '253 cylinders + box + sphere + capsule; 256 probes; budget 9600; one authoring build; costs.',
    '39-final-diagnostic-closed.png': 'Route closed, diagnostic overlays removed; existing Inspector restored.',
    '41-final-face-selection.png': 'Final APK BVH face picking; 8 vertices/6 faces; X=1.25.',
    '42-final-gesture-released.png': 'After UP X=3.192/Y=0/Z=-.5, real validated hull, target preserved.',
    '43-final-completed.png': 'Saved authoring, no draft; discarded gestures did not change persisted scene.',
}
reviews = []
for directory, video in [('frames-final', 'gestures-final.mp4'), ('frames-release-final', 'gestures-release-final.mp4')]:
    v = json.loads(read(directory+'/review.json'))
    assert v['skipped_frames'] == 0 and v['video_sha256'] == sha(root/video)
    assert v['apk_sha256'] == apk and v['decoded_frames'] == v['extracted_frames']
    assert 'visually inspected' in v['review_status']
    reviews.append({'video': video, 'review': directory+'/review.json', 'frames': v['decoded_frames'], 'sha256': v['video_sha256']})
assert sum(v['frames'] for v in reviews) == 530
preservation = json.loads(read('performance-preservation.json'))
assert preservation['unchanged_paths'] == 266 and preservation['renderer_shader_checked'] == 210
assert not preservation['renderer_shader_changed']

if args.write:
    git = lambda *a: subprocess.check_output(['git', *a], cwd=repo, text=True).strip()
    result = {
        'format': 1, 'date': '2026-10-06', 'package': 'U11 — editor e diagnóstico',
        'status': 'accepted_complete', 'u11_complete': True, 'remaining_u11': [], 'engine_complete': False,
        'repository': {'remote': git('remote', 'get-url', 'origin'), 'branch': git('branch', '--show-current'),
                       'head_before_publication': git('rev-parse', 'HEAD'), 'publication': 'local checkout; no commit/push in this block; unrelated user changes preserved'},
        'plan': 'docs/planos/ui-universal/U11-FECHAMENTO-INTEGRAL-2026-10-06.md', 'original_roadmap_sha256': original,
        'capabilities': {'new_component_types': 0, 'new_schema_properties': 0, 'new_integrated_icons': 3,
                         'faces_vertices': 'full geometry -> BVH source identity -> isolated draft/XYZ/local grips -> Jolt -> immutable GLB/real registry -> same UID/owner -> history/archive',
                         'diagnostics': 'GameWorld authority; actual Jolt COM/probes/current Character capsule; accepted motor support or labeled query; cached authoring; 10Hz Play with actual camera',
                         'lifecycle': 'exclusive capture, second pointer, Cancel/focus/selection/project/Play, stale/error refusal'},
        'host': {'focused': {'passed': 5, 'total': 5, 'log': 'focused-tests-final.log'},
                 'regression': {'passed': 100, 'total': 100, 'log': 'gui-regression-final.log'},
                 'build': 'host-build-final.log', 'source_preservation': 'performance-preservation.json',
                 'device_archive_solver': ['retained-face-reopen.log', 'retained-vertex-reopen.log'],
                 'managed_boundary': 'ABI44 layout unchanged; C# query identity has prior U11 acceptance. No new SDK expansion claimed.'},
        'android': {'device': 'POCO F7 / Xiaomi 25053PC47G / onyx', 'os': 'Android 16', 'orientation': 'landscape',
                    'resolution': [2772, 1280], 'package': 'dev.aether.editor', 'build': 'Release / arm64-v8a',
                    'build_log': 'android-build-final.log', 'install_log': 'install-final.log',
                    'built_apk_sha256': apk, 'installed_base_apk_sha256': sha(repo/'build/u11-installed-final.apk'),
                    'ui_captures': [{'file': n, 'sha256': sha(root/n), 'reviewed': True, 'accepted': True, 'observation': note} for n, note in screens.items()],
                    'save_history': {n: sha(root/(n+'.aescene')) for n in ['device-undo', 'device-redo', 'device-vertex-undo', 'device-vertex-redo', 'device-final']},
                    'runtime_boundary': 'Physical Play COM/support/capsule observed; edited Android archives reimported with real registry/Jolt raycast on host. No physical script query probe or sustained FPS/thermal claim.'},
        'video': {'final_apk_frames_reviewed': 530, 'skipped_frames': 0, 'records': reviews,
                  'intermediate_frames_reviewed': 478, 'intermediate_record': 'frames-gestures/review.json',
                  'method': 'Every decoded frame extracted without sampling; every indexed viewport/coordinate cell visually inspected, repeated frames retained, PTS/hash/phase/review per frame. Full boundary frames inspected. ROI compression differences are auxiliary, not standalone state proof. Second gesture cuts before UP in first final recording; separate release recording supplies complete UP/hold.'},
        'contract_limits': {'source_triangles': 100000, 'topology_draw_triangles': 800, 'topology_draw_points': 512,
                            'component_draw_segments': 9600, 'scale_parts_accepted': 256,
                            'curved_primitives': 'explicit polygon approximation/conversion to Mesh',
                            'concave': 'static mesh preserves cavities; dynamic concave requires U04 decomposition',
                            'measurements': 'single host/device samples, not sustained FPS or thermal guarantees'},
        'prior_vertical_chain_evidence': ['../query-identity/acceptance.json', '../occlusion/acceptance.json', '../collider-handles/acceptance.json', '../body-owners/acceptance.json'],
        'icon_atlas_sha256': sha(repo/'assets/astra-visual/ui/astra-ui-icons.aeui'),
        'source_hashes': {n: sha(repo/n) for n in source_paths},
    }
    (root/'acceptance.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
else:
    result = json.loads(read('acceptance.json'))
    assert result['u11_complete'] and not result['remaining_u11']
    for n, digest in result['source_hashes'].items():
        assert sha(repo/n) == digest, n
    for screenshot in result['android']['ui_captures']:
        assert sha(root/screenshot['file']) == screenshot['sha256']
print('U11 ACCEPTED: 100/100 host; installed APK matches; 530 final frames reviewed; byte-exact history; retained native solver evidence.')
