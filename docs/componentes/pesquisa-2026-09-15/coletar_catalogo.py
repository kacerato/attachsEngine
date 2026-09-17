"""Read-only reference collection. Does not import/build/test Astra.

Reference source stays in build/component-reference-research (not in the report).
Output contains identifiers, type relations, member metadata and source links,
never Unity implementation bodies. Requires tree-sitter 0.25.2 and
tree-sitter-c-sharp 0.23.1 installed in the research parser-libs directory.
"""
from pathlib import Path
import concurrent.futures as cf
import hashlib, html, json, re, sys, tarfile, urllib.request, zipfile

ROOT=Path(__file__).resolve().parents[3]
CACHE=ROOT/'build/component-reference-research'
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(CACHE/'parser-libs'))
from tree_sitter import Language,Parser
import tree_sitter_c_sharp
parser=Parser(Language(tree_sitter_c_sharp.language()))
REV='a2a4a31aee6dfb63c2ef36eea79d817a6e31349b'
GREV='feb4de2d9a93a4ae10d287d3a6d7003d08ea3e53'
UREV='4349121947c0f924de5ec82110ebf6cd53f77fcb'

def fetch(url,path):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():return path.read_bytes()
    req=urllib.request.Request(url,headers={'User-Agent':'Astra-reference-research/1.0'})
    data=urllib.request.urlopen(req,timeout=35).read()
    path.write_bytes(data)
    return data

def graphics():
    prefixes=['Packages/com.unity.render-pipelines.core/','Packages/com.unity.render-pipelines.universal/','Packages/com.unity.render-pipelines.high-definition/','Packages/com.unity.visualeffectgraph/','Packages/com.unity.shadergraph/']
    def gt(sha,name,recursive=False):
        d=json.loads(fetch(f'https://api.github.com/repos/Unity-Technologies/Graphics/git/trees/{sha}'+('?recursive=1' if recursive else ''),CACHE/(name+'.json')))
        if d.get('truncated'):raise RuntimeError('Incomplete subtree '+name)
        return d['tree']
    root=gt(GREV,'graphics-root')
    packages=gt(next(x['sha'] for x in root if x['path']=='Packages'),'graphics-packages')
    paths=[]
    for prefix in prefixes:
        name=prefix.split('/')[1]
        sha=next(x['sha'] for x in packages if x['path']==name)
        subtree=gt(sha,name+'-tree',True)
        paths += [prefix+x['path'] for x in subtree if x['type']=='blob' and ((x['path'].startswith('Runtime/') and x['path'].endswith('.cs')) or x['path']=='package.json')]
    def one(path):
        try:return path,fetch(f'https://raw.githubusercontent.com/Unity-Technologies/Graphics/{GREV}/{path}',CACHE/'graphics'/path)
        except Exception as e:return path,str(e)
    results=list(cf.ThreadPoolExecutor(max_workers=8).map(one,paths))
    errors=[(p,b) for p,b in results if isinstance(b,str)]
    print('Graphics runtime files',len(paths),'errors',len(errors),flush=True)
    if errors:(OUT/'collection-errors.json').write_text(json.dumps(errors,indent=2))
    return [(p,b) for p,b in results if isinstance(b,bytes)]

types={}; manifests=[]; failures=[]
def txt(n,b):return b[n.start_byte:n.end_byte].decode('utf-8','replace') if n else ''
def children(n,kind):return [c for c in n.named_children if c.type==kind]
def parse_file(path,b,package,version,source):
    tree=parser.parse(b)
    using_names=re.findall(r'^\s*using\s+([\w.]+)\s*;',b.decode('utf-8','replace'),re.M)
    if tree.root_node.has_error:failures.append({'path':path,'package':package,'reason':'parser reports syntax/preprocessor recovery; inspect linked source'})
    def walk(node,namespace='',outer=''):
        if node.type in ('namespace_declaration','file_scoped_namespace_declaration'):
            name=txt(node.child_by_field_name('name'),b)
            for c in node.named_children:walk(c,(namespace+'.'+name).strip('.'),outer)
            return
        if node.type in ('class_declaration','struct_declaration','enum_declaration'):
            name=txt(node.child_by_field_name('name'),b)
            full='.'.join(x for x in [namespace,outer,name] if x)
            mods=[txt(c,b) for c in children(node,'modifier')]
            attrs=' '.join(txt(c,b) for c in children(node,'attribute_list'))
            base=children(node,'base_list')
            bases=[txt(c,b) for c in base[0].named_children] if base else []
            body=node.child_by_field_name('body')
            if not body:return
            record=types.setdefault((package,full),dict(name=name,fullName=full,namespace=namespace,package=package,version=version,kind=node.type.replace('_declaration',''),bases=[],modifiers=[],attributes=[],sources=[],members=[],methods=[],enumValues=[],requires=[]))
            record['bases']=list(dict.fromkeys(record['bases']+bases));record['modifiers']=list(dict.fromkeys(record['modifiers']+mods))
            record['usingNamespaces']=list(dict.fromkeys(record.get('usingNamespaces',[])+using_names))
            record['sources'].append({'url':source,'path':path,'line':node.start_point.row+1})
            for an in re.findall(r'\[\s*([\w.]+)',attrs):
                if an not in record['attributes']:record['attributes'].append(an)
            for req in re.findall(r'RequireComponent\s*\(([^\]]+)\)',attrs):record['requires']+=re.findall(r'typeof\s*\(\s*([\w.]+)',req)
            menu=re.search(r'AddComponentMenu\s*\(\s*"([^"]*)"',attrs)
            if menu:record['addMenu']=menu.group(1)
            for m in body.named_children:
                if m.type in ('class_declaration','struct_declaration','enum_declaration'):
                    walk(m,namespace,'.'.join(x for x in [outer,name] if x));continue
                if m.type=='enum_member_declaration':
                    record['enumValues'].append(txt(m.child_by_field_name('name'),b));continue
                if m.type=='method_declaration' and any(txt(c,b)=='public' for c in children(m,'modifier')):
                    record['methods'].append(dict(name=txt(m.child_by_field_name('name'),b),returns=txt(m.child_by_field_name('returns'),b),parameters=txt(m.child_by_field_name('parameters'),b),line=m.start_point.row+1,source=source));continue
                if m.type not in ('property_declaration','field_declaration','event_field_declaration'):continue
                mm=[txt(c,b) for c in children(m,'modifier')]
                aa=' '.join(txt(c,b) for c in children(m,'attribute_list'))
                public='public' in mm;serialized=('SerializeField' in aa or 'SerializeReference' in aa)
                if not(public or serialized):continue
                attrNames=re.findall(r'\[\s*([\w.]+)',aa)
                if m.type=='property_declaration':
                    names=[txt(m.child_by_field_name('name'),b)];typ=txt(m.child_by_field_name('type'),b)
                    acc=m.child_by_field_name('accessors')
                    if not acc:acc=next(iter(children(m,'accessor_list')),None)
                    access=txt(acc,b)
                    write=bool(re.search(r'\b(set|init)\b',access)) and not bool(re.search(r'\b(private|protected|internal)\s+(set|init)\b',access))
                    read=bool(re.search(r'\bget\b',access)) or bool(children(m,'arrow_expression_clause'))
                    mode='property';exposure='API; not necessarily serialized or shown in Inspector'
                else:
                    dec=next(iter(children(m,'variable_declaration')),None)
                    if not dec:continue
                    typ=txt(dec.child_by_field_name('type'),b)
                    names=[txt(c.child_by_field_name('name'),b) for c in children(dec,'variable_declarator')]
                    read=True;write=not any(x in mm for x in ['const','readonly']);mode='field'
                    exposure='serialized candidate' if serialized or (public and 'static' not in mm and 'NonSerialized' not in aa and 'readonly' not in mm and 'const' not in mm) else 'API only'
                for nm in names:
                    record['members'].append(dict(name=nm,type=typ,kind=mode,public=public,serializedAttribute=serialized,exposure=exposure,read=read,write=write,static='static' in mm,hidden='HideInInspector' in aa,attributes=attrNames,source=source,line=m.start_point.row+1))
            return
        scoped=next((c for c in node.named_children if c.type=='file_scoped_namespace_declaration'),None)
        if scoped:namespace=txt(scoped.child_by_field_name('name'),b)
        for c in node.named_children:walk(c,namespace,outer)
    walk(tree.root_node)

def excluded(path):return bool(re.search(r'(^|/)(Editor|Tests?|Samples[^/]*|Documentation[^/]*|DocCodeExamples|Examples?)(/|$)',path,re.I))

def collect():
    with zipfile.ZipFile(CACHE/'unity.zip') as z:
        for n in z.namelist():
            p=n.split('/',1)[-1]
            if p.endswith('.cs') and (p.startswith('Runtime/') or p.startswith('Modules/')) and not excluded(p):
                parse_file(p,z.read(n),'UnityEngine','6000.0',f'https://github.com/Unity-Technologies/UnityCsReference/blob/{REV}/{p}')
    manifests.append(dict(package='UnityEngine',version='6000.0',revision=REV,source='https://github.com/Unity-Technologies/UnityCsReference',dependencies={}))
    for archive in sorted((CACHE/'packages').glob('*.tgz')):
        with tarfile.open(archive,'r:gz') as tf:
            manifest=json.load(tf.extractfile('package/package.json'))
            pkg=manifest['name'];ver=manifest['version']
            manifests.append(dict(package=pkg,version=ver,sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),source=f'https://download.packages.unity.com/{pkg}/-/{pkg}-{ver}.tgz',dependencies=manifest.get('dependencies',{})))
            for n in tf.getmembers():
                if n.isfile() and n.name.endswith('.cs') and ('/Runtime/' in n.name or (pkg=='com.unity.inputsystem' and n.name.startswith('package/InputSystem/'))) and not excluded(n.name):
                    parse_file(n.name,tf.extractfile(n).read(),pkg,ver,f'https://docs.unity3d.com/Packages/{pkg}@'+'.'.join(ver.split('.')[:2])+'/api/index.html')
    data=fetch(f'https://api.github.com/repos/Unity-Technologies/uGUI/zipball/{UREV}',CACHE/'ugui-6000.zip')
    with zipfile.ZipFile(CACHE/'ugui-6000.zip') as z:
        for n in z.namelist():
            p=n.split('/',1)[-1]
            if p.endswith('.cs') and '/Runtime/' in p and not excluded(p):parse_file(p,z.read(n),'com.unity.ugui','2.0 / branch 6000.0',f'https://github.com/Unity-Technologies/uGUI/blob/{UREV}/{p}')
    manifests.append(dict(package='com.unity.ugui',version='2.0 / branch 6000.0',revision=UREV,source=f'https://github.com/Unity-Technologies/uGUI/tree/{UREV}',dependencies={}))
    gfx=graphics()
    gm={}
    for p,b in gfx:
        if p.endswith('package.json'):
            m=json.loads(b);gm[m['name']]=m
            if p.count('/')==2:manifests.append(dict(package=m['name'],version=m['version'],revision=GREV,source=f'https://github.com/Unity-Technologies/Graphics/tree/{GREV}/'+p.rsplit('/',1)[0],dependencies=m.get('dependencies',{})))
    for p,b in gfx:
        pkg=p.split('/')[1]
        if p.endswith('.cs') and not excluded(p):parse_file(p,b,pkg,gm.get(pkg,{}).get('version','6000.0 branch'),f'https://github.com/Unity-Technologies/Graphics/blob/{GREV}/{p}')
    for r in types.values():
        r['members']=list({(m['name'],m['kind'],m['type']):m for m in r['members']}.values())
        r['requires']=list(dict.fromkeys(r['requires']))
    (CACHE/'tipos-extraidos-20260915.json').write_text(json.dumps(dict(date='2026-09-15',manifests=manifests,types=list(types.values()),parserWarnings=failures),ensure_ascii=False,indent=2),encoding='utf-8')
    print('Types',len(types),'syntax recovery warnings',len(failures),flush=True)

if __name__=='__main__':collect()
