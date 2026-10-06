"""Check retained acceptance evidence; visual judgments are recorded separately.

This does not infer Android Play or complete U05/U09 support from host results.
"""
import hashlib
import json
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
root = repo / 'docs/validacao/ui-universal-2026-10-06/collision-regeneration'
record = json.loads((root / 'acceptance.json').read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def log(name):
    return (root / name).read_text(encoding='utf-8', errors='replace')


assert record['status'] == 'accepted_bounded_block'
assert record['engine_complete'] is False
assert record['u05_complete'] is False and record['u09_complete'] is False
assert sha(repo / 'android/app/build/outputs/apk/release/app-release.apk') == record['apk_sha256']
assert sha(repo / 'build/collision-recipe-installed-final.apk') == record['apk_sha256']
assert 'BUILD SUCCESSFUL' in log('android-compact-label-build.log')
assert 'Success' in log('install-final.log')
assert '105/105 scenarios passed' in log('gui-regression-device-fix.log')
assert '5/5 scenarios passed' in log('focused-tests-final.log')
assert 'COLLISION_RECIPE_DEVICE:' in log('retained-project-reopen.log')
assert sha(root / 'device-before-regeneration.aescene') == sha(root / 'device-preview-final.aescene')
assert sha(root / 'device-before-regeneration.aescene') == sha(root / 'device-undo.aescene')
assert sha(root / 'device-applied.aescene') == sha(root / 'device-redo.aescene')
assert sha(root / 'device-applied.aescene') == sha(root / 'device-project/scenes/editor.aescene')
assert sha(root / 'device-applied.aescene') != sha(root / 'device-before-regeneration.aescene')
assert sha(repo / 'docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md') == record['original_roadmap_sha256']
for item in record['reviewed_screens']:
    assert item['reviewed'] and item['apk_sha256'] == record['apk_sha256']
    assert sha(root / item['file']) == item['sha256']
for item in record['source_files']:
    assert sha(repo / item['path']) == item['sha256'], item['path']
preserved = json.loads((root / 'renderer-preservation.json').read_text(encoding='utf-8-sig'))
assert preserved['verified'] == 210 and not preserved['changed']
print('ACCEPTED: persisted recipe, regeneration, prefab publication, exact Android history/archive; retained host Jolt. U05/U09 and engine remain open.')
