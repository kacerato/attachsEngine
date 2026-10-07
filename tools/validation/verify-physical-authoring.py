"""Record or verify the evidence of the physical authoring block.

Hashes prove association/integrity, not visual correctness. PNG review is recorded
separately in REPORT.md. Android authorship and host Jolt queries are distinct
gates; this tool never presents the latter as an Android query measurement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[2]
evidence = repo / 'docs/validacao/ui-universal-2026-10-06/physical-authoring'
base = '6349ee60d799ea191d42b3d5c97934e454001d38'
p = argparse.ArgumentParser()
p.add_argument('--record', action='store_true')
args = p.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def command(*words):
    return subprocess.run(words, cwd=repo, check=True, capture_output=True,
                          encoding='utf-8').stdout.strip()

def file(name):
    return evidence / name

def equal(*names):
    assert len({sha(file(n)) for n in names}) == 1, names

assert command('git', 'remote', 'get-url', 'origin') == 'https://github.com/kacerato/attachsEngine.git'
protected = ['native/renderer', 'native/rhi', 'native/platform/android',
             'assets/shaders', 'android/app/src']
assert not command('git', 'diff', '--name-only', base, '--', *protected)
roadmap = repo / 'docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md'
assert sha(roadmap) == '61727152c2e8485b5c9f8b59252074d4fda5856d18a5971b865069d8317d6b29'
assert '109/109 scenarios passed' in file('gui-regression.log').read_text(encoding='utf-8-sig')
assert '9/9 scenarios passed' in file('focused-tests-final.log').read_text(encoding='utf-8-sig')
assert 'BUILD SUCCESSFUL' in file('android-build.log').read_text(encoding='utf-8-sig')
assert 'Success' in file('install.log').read_text(encoding='utf-8-sig')
apk_hash = 'df56401d0173757585628f5097c8eb1ca388c66654f1a23bf10d714a0e084c28'
for apk in [repo / 'android/app/build/outputs/apk/release/app-release.apk',
            repo / 'build/physical-authoring-installed.apk']:
    assert sha(apk) == apk_hash, apk
equal('device-before.aescene', 'device-preview.aescene', 'device-undo.aescene')
equal('device-applied.aescene', 'device-redo.aescene', 'device-project/scenes/editor.aescene')
equal('device-budget-before.aescene', 'device-budget-cancel.aescene',
      'device-budget-cancel-early.aescene', 'device-budget-undo.aescene')
equal('device-budget-applied.aescene', 'device-budget-redo.aescene',
      'device-budget-project/scenes/editor.aescene')
assert sha(file('device-budget-cancel-assets.astra')) == sha(repo / 'build/PhysicalBudget20261006/.astra/assets.astra')
host = json.loads(file('host-budget-final.json').read_text(encoding='utf-8'))
assert host['triangles'] == 99372 and host['parts'] > 0
assert host['cancelledWithoutResult'] and host['lateCancelledWithoutResult']
assert host['deadlineStatus'] == 'budget-rejected'
assert host['deadlineMs'] > 1000  # Measured cooperative overshoot is retained.
assert host['thermalValidated'] is False
device = json.loads(file('device-budget-ready-run/memory.json').read_text(encoding='utf-8'))
assert device['maximumSampledPssKiB'] == max(s['totalPssKiB'] for s in device['samples'])
assert device['thermalValidated'] is False
probe = repo / 'build/editor-host/aether_gui_preview.exe'
for project, pose, before in [('device-project', '0.6', 'device-before.aescene'),
                              ('device-budget-project', 'authored', None)]:
    words = [str(probe), 'verify-physical-authoring-project', str(file(project)), pose]
    if before:
        words.append(str(file(before)))
    result = command(*words)
    assert 'PHYSICAL_AUTHORING_DEVICE:' in result

ledger_path = file('acceptance.json')
if args.record:
    # Existing documents preserve previous gates. This manifest concerns this
    # block's engine files, raw acceptance files, and editable source archives.
    sources = command('git', 'diff', '--name-only', '--', 'native', 'tests/native').splitlines()
    sources += ['tools/validation/measure-convex-budget.py',
                'tools/validation/sample-physical-device.py',
                'tools/validation/verify-physical-authoring.py']
    tracked = {path.relative_to(repo).as_posix(): sha(path) for path in evidence.rglob('*')
               if path.is_file() and path.name != 'acceptance.json'
               and '/.astra/cache/' not in path.as_posix()
               and '/.astra/code/' not in path.as_posix()}
    tracked.update({name: sha(repo / name) for name in sources})
    ledger = {
        'repo': 'https://github.com/kacerato/attachsEngine.git', 'base': base,
        'branchAtAcceptance': command('git', 'branch', '--show-current'),
        'block': 'U01/U02/U04/U05/U09 physical authoring under documented budgets',
        'engineComplete': False, 'originalUiRoadmapComplete': False,
        'apkSha256': apk_hash, 'protectedSourceDirectories': protected,
        'host': {'regression': '109/109', 'finalFocused': '9/9',
                 'reopenedAndroidProjectsJoltQueried': 2, 'budget': host},
        'android': {'model': 'POCO F7 / 25053PC47G', 'android': 16,
                    'releaseInstalled': True, 'previewIsolated': True,
                    'oneUndoRedoExact': True, 'coldReopenSettings': True,
                    'earlyCancellationSceneAndRegistryUnchanged': True,
                    'largeSourceTriangles': 99372,
                    'maximumSampledWholeEditorPssKiB': device['maximumSampledPssKiB'],
                    'physicsQueryTelemetryOnAndroid': False, 'thermalValidated': False},
        'visualReview': {'method': 'every retained PNG inspected individually',
                         'captures': sorted(path.relative_to(evidence).as_posix()
                                            for path in evidence.rglob('*.png')),
                         'videoRecorded': False, 'details': 'REPORT.md'},
        'limits': {'triangles': 100000, 'sources': 128, 'parts': 32,
                   'hullVertices': 64, 'voxels': 400000,
                   'poseSnapshotVertices': 300000, 'deadline': 'cooperative, not hard',
                   'collisionPose': 'static snapshot, no live animated recooking',
                   'volumeError': 'not a distance or cavity preservation guarantee',
                   'largeFixture': 'closed subdivided convex cube; not thermal/concavity breadth'},
        'remainingBlocks': ['U03/U06/U07/U08 locomotion and representation',
                            'U10/R0-R12 composable UI', 'full visual ProBuilder authoring',
                            'U12/U13/U14 SDK/replay/network/broad budgets'],
        'sha256': tracked,
    }
    ledger_path.write_text(json.dumps(ledger, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
else:
    ledger = json.loads(ledger_path.read_text(encoding='utf-8'))
    for name, expected in ledger['sha256'].items():
        assert sha(repo / name) == expected, name
print('PHYSICAL_AUTHORING_ACCEPTANCE: host, installed APK, source/archive integrity, '
      'Android history/cancellation, reopened Jolt, sampled budgets and retained limits verified.')
