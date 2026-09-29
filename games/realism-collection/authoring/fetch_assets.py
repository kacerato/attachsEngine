"""Download source PBR assets. Writes only inside this game collection."""
from pathlib import Path
import hashlib, importlib.util, json, time, urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
CACHE = ROOT / 'sources'
spec = importlib.util.spec_from_file_location('pack', REPO / 'tools/fetch-highlevel-game-assets.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
HEADERS = {'User-Agent': 'Astra-content-production/1.0 (offline game authoring)'}

def get(url):
    for attempt in range(3):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=60).read()
        except Exception:
            if attempt == 2: raise
            time.sleep(2)

def checked(item, dest):
    if dest.exists() and hashlib.md5(dest.read_bytes()).hexdigest() == item['md5']:
        return dest.read_bytes()
    data = get(item['url'])
    if hashlib.md5(data).hexdigest() != item['md5']: raise ValueError(item['url'])
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return data

def metadata(asset):
    dest = CACHE / 'metadata' / (asset + '.json')
    if not dest.exists():
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(get('https://api.polyhaven.com/files/' + asset))
    return json.loads(dest.read_bytes())

MODELS = ['portable_generator', 'industrial_storage_cart', 'modular_industrial_pipes_01',
          'wooden_crate_01', 'rock_07', 'concrete_road_barrier',
          'antique_ceramic_vase_01', 'stone_01', 'concrete_cat_statue']
SURFACES = ['quarry_wall', 'worn_shutter', 'concrete_floor_02', 'wood_planks', 'sandstone_blocks_05']

def model(asset):
    files = metadata(asset)
    package = files['gltf']['2k']['gltf']
    folder = CACHE / asset
    source = json.loads(checked(package, folder / 'source.gltf'))
    included = {p: checked(item, folder / p) for p, item in package['include'].items()}
    output = folder / (asset + '.glb')
    output.write_bytes(pack.pack_glb(source, included))
    return dict(asset=asset, kind='model', resolution='2k', url='https://polyhaven.com/a/'+asset,
                license='CC0', sha256=hashlib.sha256(output.read_bytes()).hexdigest(), bytes=output.stat().st_size)

def surface(asset):
    files = metadata(asset)
    folder = CACHE / asset
    for role, dest in [('Diffuse','base.jpg'),('nor_gl','normal.png'),('arm','arm.png')]:
        choices = files[role]['2k']
        fmt = 'jpg' if role == 'Diffuse' else 'png'
        if fmt not in choices: raise ValueError(f'{asset} {role}: missing {fmt}')
        checked(choices[fmt], folder / dest)
    return dict(asset=asset, kind='surface', resolution='2k', url='https://polyhaven.com/a/'+asset, license='CC0')

if __name__ == '__main__':
    # Source names are resolved from the actual catalog, not fabricated URLs.
    catalog = json.loads(get('https://api.polyhaven.com/assets?t=textures'))
    replacement = {'worn_shutter': ['rusty_metal', 'rusty_metal_02'],
                   'wood_planks': ['wood_planks_dirt', 'wood_planks_grey'],
                   'sandstone_blocks_05': ['sandstone_blocks_04', 'sandstone_blocks_01']}
    for i, name in enumerate(SURFACES):
        if name not in catalog:
            SURFACES[i] = next((v for v in replacement.get(name,[]) if v in catalog), '')
            if not SURFACES[i]: raise ValueError('No verified texture for '+name)
    records = []
    with ThreadPoolExecutor(max_workers=3) as pool:
        for record in pool.map(model, MODELS):
            records.append(record); print(record['asset'], record['bytes'], flush=True)
        for record in pool.map(surface, SURFACES):
            records.append(record); print(record['asset'], flush=True)
    (CACHE/'manifest.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    (CACHE/'surfaces.json').write_text(json.dumps(SURFACES), encoding='utf-8')
