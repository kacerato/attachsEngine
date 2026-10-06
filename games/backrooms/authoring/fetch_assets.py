"""Fetch photographed CC0 PBR surfaces, verifying upstream checksums."""
from pathlib import Path
import hashlib, json, requests
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
SESSION = requests.Session()
SESSION.headers['User-Agent'] = 'Astra-content-production/1.0'
ASSETS = ['dirty_carpet', 'decrepit_wallpaper', 'dirty_tiles', 'yellow_plaster']

def fetch(asset):
    folder = ROOT/'sources'/asset
    folder.mkdir(parents=True, exist_ok=True)
    r = SESSION.get('https://api.polyhaven.com/files/'+asset, timeout=45)
    r.raise_for_status()
    files = r.json()
    (folder/'metadata.json').write_text(json.dumps(files, indent=2), encoding='utf-8')
    records = []
    for role, name, preferred in [('Diffuse','base.jpg','jpg'), ('nor_gl','normal.png','png'), ('arm','arm.png','png')]:
        choices = files[role]['2k']
        fmt = preferred if preferred in choices else next(iter(choices))
        item = choices[fmt]
        dest = folder/name
        if not dest.exists() or hashlib.md5(dest.read_bytes()).hexdigest() != item['md5']:
            response = SESSION.get(item['url'], timeout=60)
            response.raise_for_status()
            data = response.content
            if hashlib.md5(data).hexdigest() != item['md5']:
                raise ValueError('Checksum mismatch: '+item['url'])
            dest.write_bytes(data)
        records.append({'role':role, 'source':item['url'], 'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
    print('Fetched', asset, flush=True)
    return {'asset':asset,'license':'CC0','page':'https://polyhaven.com/a/'+asset,'resolution':'2k','files':records}

if __name__ == '__main__':
    with ThreadPoolExecutor(max_workers=3) as pool:
        records = list(pool.map(fetch, ASSETS))
    (ROOT/'sources.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
