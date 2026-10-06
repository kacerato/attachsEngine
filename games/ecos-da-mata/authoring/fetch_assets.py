from pathlib import Path
import importlib.util,json
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1]; REPO=ROOT.parents[1]
spec=importlib.util.spec_from_file_location('fetch',REPO/'tools/fetch-highlevel-game-assets.py'); mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
models=['pine_tree_01','fir_tree_01','fern_02','dead_tree_trunk','rock_moss_set_01']
def fetch(name):
    path=ROOT/'project/Assets'/f'{name}.glb'
    if path.exists(): return {'model':name,'license':'CC0','page':'https://polyhaven.com/a/'+name,'size':path.stat().st_size}
    r=mod.fetch(name,path);print(name,r['size'],flush=True);return r
with ThreadPoolExecutor(max_workers=3) as pool: records=list(pool.map(fetch,models))
(ROOT/'sources.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
files=json.loads(mod.get('https://api.polyhaven.com/files/forest_ground_04'))
folder=ROOT/'sources/forest_ground_04';folder.mkdir(parents=True,exist_ok=True)
for role,name,fmt in [('Diffuse','base.jpg','jpg'),('nor_gl','normal.png','png'),('arm','arm.png','png')]:
    path=folder/name
    if not path.exists():
        item=files[role]['1k'][fmt];path.write_bytes(mod.get(item['url'],item['md5']))
print('forest ground ready',flush=True)
