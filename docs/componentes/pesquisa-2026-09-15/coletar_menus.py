"""Inventory literal GameObject menu registrations in pinned reference archives."""
from pathlib import Path
import json,re,tarfile,zipfile
OUT=Path(__file__).resolve().parent;ROOT=OUT.parents[2];CACHE=ROOT/'build/component-reference-research'
rows=[]
def scan(path,b,pkg,url):
    s=b.decode('utf-8','replace')
    for m in re.finditer(r'\[\s*(?:UnityEditor\.)?MenuItem\s*\(\s*"(GameObject/[^"\r\n]+)"([^\]]*)\]',s):
        # A validator is a separate registration, not a creation command.
        if re.match(r'\s*,\s*true\b',m.group(2)):continue
        method=re.search(r'\b(?:void|GameObject)\s+(\w+)\s*\(',s[m.end():m.end()+500])
        rows.append(dict(menu=m.group(1),method=method.group(1) if method else 'registration',package=pkg,path=path,line=s[:m.start()].count('\n')+1,source=url))
for archive,repo,rev in [('unity.zip','UnityCsReference','a2a4a31aee6dfb63c2ef36eea79d817a6e31349b'),('ugui-6000.zip','uGUI','4349121947c0f924de5ec82110ebf6cd53f77fcb')]:
    with zipfile.ZipFile(CACHE/archive) as z:
        for n in z.namelist():
            if n.endswith('.cs'):
                p=n.split('/',1)[-1];scan(p,z.read(n),repo,f'https://github.com/Unity-Technologies/{repo}/blob/{rev}/{p}')
for p in (CACHE/'packages').glob('*.tgz'):
    with tarfile.open(p) as z:
        m=json.load(z.extractfile('package/package.json'))
        for n in z.getmembers():
            if n.isfile() and n.name.endswith('.cs') and not re.search(r'/(Samples~|Tests|Documentation~)/',n.name):scan(n.name,z.extractfile(n).read(),m['name'],f'https://download.packages.unity.com/{m["name"]}/-/{m["name"]}-{m["version"]}.tgz')
rows.sort(key=lambda r:(r['package'],r['menu']))
(OUT/'menus-gameobject.json').write_text(json.dumps(dict(date='2026-09-15',scope='Literal MenuItem GameObject paths in UnityCsReference, uGUI and downloaded packages. Excludes native registrations, dynamic/concatenated paths and Graphics Editor sources not collected. Some commands operate on existing objects instead of creating them.',entries=rows),ensure_ascii=False,indent=2),encoding='utf-8')
print('Literal GameObject menu registrations:',len(rows))
