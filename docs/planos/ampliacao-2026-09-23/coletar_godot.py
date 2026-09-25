"""Inventário documental, sem instalar Godot ou modificar a engine.

Lê XMLs do tag oficial fixado, publica apenas metadados de API e proveniência.
As relações de tipos NÃO representam dependências obrigatórias de composição.
"""
from pathlib import Path
import collections
import hashlib
import json
import re
import tarfile
import urllib.request
import xml.etree.ElementTree as ET

OUT = Path(__file__).resolve().parent
TAG = '4.5-stable'

def get(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Astra-documentation-research'}), timeout=90)

def collect():
    with get(f'https://api.github.com/repos/godotengine/godot/git/ref/tags/{TAG}') as response:
        ref = json.load(response)
    sha = ref['object']['sha']
    if ref['object']['type'] == 'tag':
        with get(f'https://api.github.com/repos/godotengine/godot/git/tags/{sha}') as response:
            sha = json.load(response)['object']['sha']
    url = f'https://codeload.github.com/godotengine/godot/tar.gz/{sha}'
    records = []
    licenses = {}
    with get(url) as response, tarfile.open(fileobj=response, mode='r|gz') as archive:
        for entry in archive:
            path = entry.name.split('/', 1)[-1]
            if path in ('LICENSE.txt', 'doc/LICENSE.md', 'doc/LICENSE.txt'):
                licenses[path] = archive.extractfile(entry).read().decode('utf-8')
            if not (entry.isfile() and path.endswith('.xml') and
                    (path.startswith('doc/classes/') or re.match(r'modules/[^/]+/doc_classes/', path))):
                continue
            raw = archive.extractfile(entry).read()
            root = ET.fromstring(raw)
            if root.tag != 'class':
                continue
            methods = []
            for method in root.findall('./methods/method'):
                result = method.find('return')
                methods.append({**method.attrib, 'return': result.attrib if result is not None else {},
                                'params': [p.attrib for p in method.findall('param')]})
            records.append({
                'name': root.get('name'), 'base': root.get('inherits'),
                'flags': root.attrib,
                'source': f'https://github.com/godotengine/godot/blob/{sha}/{path}',
                'documentation': f"https://docs.godotengine.org/en/4.5/classes/class_{root.get('name').lower()}.html",
                'sha256': hashlib.sha256(raw).hexdigest(), 'path': path,
                'properties': [p.attrib for p in root.findall('./members/member')],
                'themeProperties': [p.attrib for p in root.findall('./theme_items/theme_item')],
                'methods': methods,
                'signals': [{**s.attrib, 'params': [p.attrib for p in s.findall('param')]} for s in root.findall('./signals/signal')],
                'constants': [p.attrib for p in root.findall('./constants/constant')],
            })
    index = {r['name']: r for r in records}
    unresolved = []
    for record in records:
        chain, base = [], record['base']
        while base:
            if base in chain:
                raise ValueError(f'Cycle: {record["name"]}')
            chain.append(base)
            if base not in index:
                unresolved.append({'type': record['name'], 'base': base})
                break
            base = index[base]['base']
        record['ancestors'] = chain
        record['category'] = 'Node' if 'Node' in chain or record['name'] == 'Node' else 'Resource' if 'Resource' in chain or record['name'] == 'Resource' else 'Service / support'
        references = set()
        for prop in record['properties'] + record['themeProperties']:
            references.update(w for w in re.findall(r'[A-Za-z_][A-Za-z_0-9]*', prop.get('type', '')) if w in index)
        record['propertyTypeReferences'] = sorted(references)
    records.sort(key=lambda r: r['name'].casefold())
    manifest = {
        'researchDate': '2026-09-23', 'version': TAG, 'revision': sha, 'archive': url,
        'scope': 'Todos os XMLs de doc/classes e modules/*/doc_classes do commit fixado; metadados, sem prosa ou código runtime.',
        'counts': {'types': len(records), 'categories': dict(collections.Counter(r['category'] for r in records)),
                   'declaredProperties': sum(len(r['properties']) for r in records),
                   'themeProperties': sum(len(r['themeProperties']) for r in records),
                   'methods': sum(len(r['methods']) for r in records), 'signals': sum(len(r['signals']) for r in records)},
        'unresolvedBases': unresolved,
        'limits': ['Node não implica classe concreta anexável.', 'Inclui tipos de editor, módulos e plataformas opcionais.',
                   'Relação de tipo de propriedade não é RequireComponent nem requisito pai/filho.',
                   'XML não fornece todas as faixas, unidades, restrições de backend ou campos serializados.',
                   'Ausência de default/setter não autoriza inferir valor zero ou somente leitura.',
                   'Sem plugins de terceiros e sem alegação de equivalência Astra.'],
    }
    (OUT / 'godot-api.json').write_text(json.dumps({'manifest': manifest, 'types': records}, ensure_ascii=False, separators=(',', ':'))+'\n', encoding='utf-8')
    (OUT / 'godot-manifesto.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    for path, content in licenses.items():
        (OUT / ('GODOT-'+path.replace('/', '-'))).write_text(content, encoding='utf-8')
    print(json.dumps(manifest, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    collect()
