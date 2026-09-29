"""Collect official image URLs, never download image files. Run from repository root."""
import concurrent.futures as cf
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import threading
from urllib.parse import urljoin, urlparse

import requests

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
MANUAL = 'https://docs.unity3d.com/6000.0/Documentation/Manual/'
CAT = ROOT / 'docs/componentes/pesquisa-2026-09-15/catalogo-unity.json'
local = threading.local()


def norm(s):
    return re.sub('[^a-z0-9]', '', s.lower())


def fetch(url):
    if not hasattr(local, 'session'):
        local.session = requests.Session()
        local.session.headers['User-Agent'] = 'UnityVisualReference/1.0'
    try:
        r = local.session.get(url, timeout=(8, 22))
        return {'url': url, 'status': r.status_code, 'body': r.text if r.ok else ''}
    except requests.RequestException as e:
        return {'url': url, 'status': 0, 'body': '', 'error': type(e).__name__}


class Tags(HTMLParser):
    def __init__(self, base):
        super().__init__()
        self.base = base
        self.images = []
        self.links = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == 'img' and a.get('src'):
            url = urljoin(self.base, a['src'])
            if urlparse(url).hostname == 'docs.unity3d.com' and not any(x in url.lower() for x in ['logo', 'favicon', 'icon-arrow']):
                self.images.append({'url': url, 'alt': a.get('alt', '')[:160]})
        if tag == 'a' and a.get('href'):
            self.links.append({'url': urljoin(self.base, a['href']), 'title': a.get('title', '')})


def extract(page):
    body = page['body']
    # Exclude navigation logos, package-site footer and toolbar.
    if '<h1' in body:
        body = body[body.index('<h1'):]
    if 'id="_content"' in body:
        body = body.split('id="_content"')[0]
    p = Tags(page['url'])
    p.feed(body)
    seen = set()
    return [dict(i, page=page['url']) for i in p.images if not (i['url'] in seen or seen.add(i['url']))]


def main():
    cat = json.loads(CAT.read_text(encoding='utf-8'))
    components = [t for t in cat['types'] if t.get('category') == 'Component']
    records = []
    for t in components:
        records.append({k: t.get(k) for k in ['id', 'name', 'fullName', 'package', 'version', 'abstract', 'obsolete', 'menuStatus']})
        records[-1].update(group='Componentes', source=t['sources'][0]['url'], images=[], pages=[], candidates=[])

    extra = [
        ('Inspector — componentes', 'Inspector', 'InspectorManageComponents'),
        ('Inspector — GameObject e Transform', 'Inspector', 'Components'),
        ('Inspector — edição de componentes', 'Inspector', 'UsingComponents'),
        ('Inspector — referências de objetos', 'Inspector', 'InspectorReferences'),
        ('Inspector — seleção de cores', 'Inspector', 'InspectorColorPicker'),
        ('Inspector — curvas', 'Inspector', 'InspectorCurves'),
        ('Inspector — arrays', 'Inspector', 'InspectorArray'),
        ('Inspector — opções e modo Debug', 'Inspector', 'InspectorOptions'),
        ('Inspector — janela focada', 'Inspector', 'InspectorFocused'),
        ('Inspector — ícones dos objetos', 'Inspector', 'InspectorAssignIcons'),
        ('Inspector — assets e materiais', 'Inspector', 'InspectorItems'),
        ('Editor completo', 'Inspector', 'UsingTheEditor'),
        ('Hierarchy — objetos da cena', 'Objetos', 'hierarchy-reference'),
        ('GameObject — composição', 'Objetos', 'class-GameObject'),
        ('Objetos primitivos 3D', 'Objetos', 'PrimitiveObjects'),
        ('Prefab — instância e recurso', 'Objetos', 'Prefabs'),
        ('Prefab — edição', 'Objetos', 'EditingInPrefabMode'),
        ('Terrain — objeto de terreno', 'Objetos', 'terrain-UsingTerrains'),
        ('Sprites — objetos 2D', 'Objetos', 'sprite/renderer/renderer-landing'),
        ('Tilemap — criação de objetos 2D', 'Objetos', 'tilemaps/work-with-tilemaps/create-tilemap'),
        ('Canvas — objetos de interface', 'Objetos', 'UICanvas'),
    ]
    for name, group, slug in extra:
        records.append(dict(id='visual::'+slug, name=name, fullName=name, package='UnityEngine', version='6000.0', group=group, images=[], pages=[], candidates=[MANUAL+slug+'.html'], source=MANUAL+slug+'.html'))

    packages = {t['package']: re.match(r'\d+\.\d+', t['version']).group() for t in components if t['package'] != 'UnityEngine'}
    toc_urls = {'UnityEngine': MANUAL+'docdata/toc.json'}
    toc_urls.update({p: f'https://docs.unity3d.com/Packages/{p}@{v}/manual/toc.html' for p, v in packages.items()})
    indexes = {}
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        futures = {pool.submit(fetch, u): p for p, u in toc_urls.items()}
        for f in cf.as_completed(futures):
            p = futures[f]
            page = f.result()
            entries = []
            if p == 'UnityEngine' and page['status'] == 200:
                def walk(node):
                    link = node.get('link')
                    if link and link != 'null':
                        entries.append((node.get('title', ''), MANUAL+link+'.html'))
                    for c in node.get('children') or []:
                        walk(c)
                walk(json.loads(page['body'].lstrip('\ufeff')))
            else:
                parser = Tags(page['url'])
                parser.feed(page['body'])
                entries = [(x['title'], x['url']) for x in parser.links if '/manual/' in x['url'] and x['url'].endswith('.html')]
            indexes[p] = entries
            print('Index', p, page['status'], len(entries), flush=True)

    # URP 17 moved its manual into the Unity 6 manual.
    if not indexes.get('com.unity.render-pipelines.universal'):
        indexes['com.unity.render-pipelines.universal'] = [(t, u) for t, u in indexes['UnityEngine'] if '/urp/' in u]

    overrides = {
        'Transform': ['class-Transform'], 'RectTransform': ['class-RectTransform'],
        'Camera': ['class-Camera', 'CamerasOverview'], 'Light': ['class-Light'],
        'MeshRenderer': ['class-MeshRenderer'], 'SkinnedMeshRenderer': ['class-SkinnedMeshRenderer'],
        'SpriteRenderer': ['sprite/renderer/renderer-reference'], 'Terrain': ['terrain-UsingTerrains'],
        'Tilemap': ['tilemaps/work-with-tilemaps/tilemap-reference'],
        'TilemapRenderer': ['tilemaps/work-with-tilemaps/tilemap-renderer-reference'],
        'ParticleSystem': ['PartSysMainModule'], 'Animator': ['class-Animator'],
        'AudioSource': ['class-AudioSource'], 'UIDocument': ['UIE-create-ui-document-component'],
    }
    for rec in records:
        if rec['group'] != 'Componentes':
            continue
        n = norm(rec['name'])
        matches = []
        for title, url in indexes.get(rec['package'], []):
            stem = norm(url.rsplit('/', 1)[-1].split('.html')[0])
            title_n = norm(title)
            if stem in [n, 'class'+n, 'script'+n] or title_n in [n, n+'component', n+'componentreference', n+'reference', n+'componentproperties', n+'components']:
                matches.append(url)
        if rec['package'] == 'UnityEngine':
            matches = [MANUAL+s+'.html' for s in overrides.get(rec['name'], [])] + matches
            if not matches and not rec.get('abstract'):
                matches = [MANUAL+'class-'+rec['name']+'.html']
        rec['candidates'] = list(dict.fromkeys(matches))[:4]

    urls = sorted({u for r in records for u in r['candidates']})
    print('Pages to inspect:', len(urls), flush=True)
    pages = {}
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        for i, page in enumerate(pool.map(fetch, urls)):
            pages[page['url']] = page
            if i % 40 == 0:
                print('Pages inspected', i, '/', len(urls), flush=True)
    for r in records:
        for u in r.pop('candidates'):
            page = pages[u]
            r['pages'].append({k: page[k] for k in ['url', 'status']})
            if page['status'] == 200:
                r['images'].extend(extract(page))
        r['images'] = list({im['url']: im for im in r['images']}.values())
        r['coverage'] = 'Imagem oficial encontrada' if r['images'] else 'Sem imagem localizada nas páginas consultadas'

    # Historical references are explicitly versioned, never labeled as Unity 6 UI.
    historical = {}
    for r in records:
        if not r['images'] and r['package']=='UnityEngine' and r['group']=='Componentes' and not r.get('abstract'):
            historical.setdefault(MANUAL.replace('6000.0', '2022.3')+'class-'+r['name']+'.html', []).append(r)
    inspector = next(r for r in records if r['name']=='Inspector — componentes')
    historical[MANUAL.replace('6000.0', '2019.4')+'UsingTheInspector.html'] = [inspector]
    historical[MANUAL.replace('6000.0', '2022.3')+'UsingTheEditor.html'] = [next(r for r in records if r['name']=='Editor completo')]
    print('Historical pages:', len(historical), flush=True)
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        for page in pool.map(fetch, historical):
            for r in historical[page['url']]:
                r['pages'].append({k: page[k] for k in ['url', 'status']})
                if page['status']==200:
                    r['images'].extend(extract(page))
    for r in records:
        for im in r['images']:
            path = urlparse(im['page']).path
            im['documentationVersion'] = '6000.0' if '/6000.0/' in path else ('2022.3' if '/2022.3/' in path else ('2019.4' if '/2019.4/' in path else 'Pacote '+r['version']))
        r['coverage'] = 'Imagem oficial encontrada' if r['images'] else 'Sem imagem localizada nas páginas consultadas'

    images = sorted({im['url'] for r in records for im in r['images']})
    def check(url):
        try:
            response = local.session.head(url, timeout=(8, 15), allow_redirects=True) if hasattr(local, 'session') else requests.head(url, timeout=(8, 15), allow_redirects=True)
            return url, response.status_code, response.headers.get('Content-Type', '')
        except requests.RequestException:
            return url, 0, ''
    with cf.ThreadPoolExecutor(max_workers=8) as pool:
        checks = {u: {'status': s, 'contentType': c} for u, s, c in pool.map(check, images)}
    for r in records:
        for im in r['images']:
            im.update(checks[im['url']])
        r['unavailableImages'] = [im for im in r['images'] if im['status'] in (404, 410)]
        r['images'] = [im for im in r['images'] if im['status'] not in (404, 410)]
        r['coverage'] = 'Imagem oficial encontrada' if r['images'] else 'Sem imagem localizada nas páginas consultadas'
    images = sorted({im['url'] for r in records for im in r['images']})
    result = dict(date='2026-09-26', reference=cat['reference'], records=records, packages=cat['manifests'], stats=dict(components=len(components), records=len(records), uniqueImages=len(images), componentsWithImages=sum(bool(r['images']) for r in records if r['group']=='Componentes'), imageHeadOK=sum(c['status']==200 for c in checks.values())), limits=['Imagens oficiais remotas; precisam de internet. Não foram baixadas.', '575 fichas reproduzem a classificação do inventário; incluem bases, abstratos, obsoletos e tipos ocultos.', 'A imagem pode ser exemplo, diagrama ou captura do Inspector. Não é captura local do Unity.', 'Ausência de imagem significa não localizada nas páginas consultadas; não prova ausência em toda a documentação.', 'Objetos cobrem páginas de GameObject, primitivas, prefabs, terreno, sprites, tilemap e Canvas; não todos os objetos possíveis de scripts ou plugins.', 'Capturas complementares da documentação Unity 2022.3 e 2019.4 são identificadas por imagem; não comprovam a aparência atual da Unity 6.', 'A documentação versionada pode reutilizar capturas de versões anteriores.'])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/'catalogo.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result['stats']), flush=True)


if __name__ == '__main__':
    main()
