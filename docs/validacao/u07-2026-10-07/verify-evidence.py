"""Verify evidence integrity and acceptance gates; do not rerun engine suites."""
import hashlib,json,xml.etree.ElementTree as ET
from pathlib import Path
here=Path(__file__).resolve().parent
root=here.parents[2]
def read(name): return (here/name).read_text(encoding='utf-8-sig')
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def matches(path,expected):
    data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()==expected:return True
    # Git autocrlf changes UTF-8 text on checkout; binary evidence stays exact.
    if b'\0' in data:return False
    try:data.decode('utf-8')
    except UnicodeDecodeError:return False
    lf=data.replace(b'\r\n',b'\n')
    return expected in {hashlib.sha256(lf).hexdigest(),hashlib.sha256(lf.replace(b'\n',b'\r\n')).hexdigest()}
record=json.loads(read('acceptance.json'))
assert record['status']=='accepted' and record['repository']=='https://github.com/kacerato/attachsEngine.git'
for group in ['sourceFiles','projectFiles','evidenceFiles']:
    for relative,expected in record[group].items():
        assert matches(root/relative,expected),f'Changed/missing {relative}'
for log,phrase in [('host-motion-scenarios.log','12/12 scenarios passed'),
                  ('gui-motion-regression-retry.log','121/121 scenarios passed'),
                  ('sdk-motion-compile-final.log','1 passaram, 0 falharam'),
                  ('android-startup-build.log','BUILD SUCCESSFUL'),
                  ('android-public-build.log','BUILD SUCCESSFUL'),
                  ('install-motion.log','Success'),('install-public-motion.log','Success'),
                  ('archive-motion-saved-inspection.log','skins=6 animations=2 environments=1'),
                  ('public-play-reopen.log','U07 READY:')]: assert phrase in read(log),log
store=ET.fromstring(read('project-store-tests.xml'))
assert store.attrib['tests']=='7' and store.attrib['failures']==store.attrib['errors']=='0'
total=0
for name,count,pages in [('locomotion',300,38),('camera-collision',227,29)]:
    review=json.loads(read(name+'-accepted-review.json'))
    assert review['decodedFrames']==len(review['frames'])==count
    assert [f['frame'] for f in review['frames']]==list(range(1,count+1))
    assert len(review['pages'])==pages and review['review'].startswith('complete:')
    assert review['reviewedFrameRange']==[1,count]
    assert review['sampling'].startswith('none:')
    assert sha(here/review['video'])==review['videoSha256']
    assert review['apkSha256']==record['android']['validation']['apkSha256']
    covered=[]
    for page in review['pages']:
        assert page['reviewed'] and page['observations']
        assert sha(here/(name+'-accepted-contacts')/page['file'])==page['sha256']
        covered.extend(range(page['from'],page['to']+1))
    assert covered==list(range(1,count+1));total+=count
assert total==527
manifests=[]
for name,manifest,installed in [('validation','apk-motion-manifest.json','installed-motion-hash.log'),
                               ('public','apk-public-motion-manifest.json','installed-public-motion-hash.log')]:
    apk=json.loads(read(manifest));manifests.append(apk)
    assert apk['bundledProjects']==['u07'] and len(apk['files'])==21 and apk['exactSourceBytes']
    assert apk['apkSha256']==record['android'][name]['apkSha256'] and apk['apkSha256'] in read(installed)
assert manifests[0]['nativeLibrarySha256']==manifests[1]['nativeLibrarySha256']
before=read('user-projects-before.log').splitlines();after=read('user-projects-after.log').splitlines()
assert before and sorted(before)==sorted(after)
assert sorted(read('public-projects-after.log').splitlines())==['CameraVirtual-20261007','U07Laboratorio','teste']
contracts=json.loads(read('contracts-current.json'))
assert contracts['check_passed'] and contracts['csharp_api_checked']
assert contracts['registered_types']==56 and contracts['property_declarations']==423 and contracts['generated_accessor_lambdas']==846
print('U07 verified: source/project/evidence hashes, 12/121/7/SDK gates, 527 frames/67 reviewed pages, single bundled project, installed APK identities and preserved user files.')
