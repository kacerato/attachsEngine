"""Export the authored static world as standard GLB, with shared meshes and PBR textures."""
from pathlib import Path
import importlib.util,json,struct,math,hashlib,copy
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('forest',ROOT/'authoring/build_project.py');game=importlib.util.module_from_spec(spec);spec.loader.exec_module(game)
project=ROOT/'project';out=ROOT/'exports';out.mkdir(exist_ok=True)
doc={'asset':{'version':'2.0','generator':'Astra Ecos da Mata authoring'},'bufferViews':[],'accessors':[],'images':[],'samplers':[],'textures':[],'materials':[],'meshes':[],'nodes':[]};binary=bytearray();meshes={};extensions=set()
for path in sorted((project/'Assets').glob('*.glb')):
    raw=path.read_bytes();n=struct.unpack_from('<I',raw,12)[0];source=json.loads(raw[20:20+n]);blob=raw[28+n:];binary.extend(b'\0'*(-len(binary)%4));offset=len(binary);binary.extend(blob);base={key:len(doc[key]) for key in ['bufferViews','accessors','images','samplers','textures','materials','meshes']}
    for v in source.get('bufferViews',[]):
        v=copy.deepcopy(v);v['byteOffset']=v.get('byteOffset',0)+offset;v['buffer']=0;doc['bufferViews'].append(v)
    for a in source.get('accessors',[]):
        a=copy.deepcopy(a)
        if 'bufferView' in a:a['bufferView']+=base['bufferViews']
        doc['accessors'].append(a)
    for im in source.get('images',[]):im=copy.deepcopy(im);im['bufferView']+=base['bufferViews'];doc['images'].append(im)
    doc['samplers'].extend(source.get('samplers',[]))
    for tex in source.get('textures',[]):
        tex=copy.deepcopy(tex)
        if 'source' in tex:tex['source']+=base['images']
        if 'sampler' in tex:tex['sampler']+=base['samplers']
        doc['textures'].append(tex)
    def remap_texture(value):
        if isinstance(value,dict):
            for key,item in value.items():
                if key.endswith('Texture') and isinstance(item,dict):item['index']+=base['textures']
                else:remap_texture(item)
        elif isinstance(value,list):
            for item in value:remap_texture(item)
    for mat in source.get('materials',[]):mat=copy.deepcopy(mat);remap_texture(mat);doc['materials'].append(mat)
    for mesh in source['meshes']:
        mesh=copy.deepcopy(mesh)
        for p in mesh['primitives']:
            p['attributes']={k:v+base['accessors'] for k,v in p['attributes'].items()};p['indices']+=base['accessors']
            if 'material' in p:p['material']+=base['materials']
        doc['meshes'].append(mesh)
    guid=game.base.guid('fonte:Assets/'+path.name)
    for node in source['nodes']:
        if 'mesh' in node:
            m=source['meshes'][node['mesh']]
            for i,p in enumerate(m['primitives']):
                identity=game.base.guid('glb:'+guid+':'+node['name']+'/'+m['name']+'#'+str(i))
                # Scene mesh records represent individual imported primitives.
                doc['meshes'].append({'name':m['name']+' #'+str(i),'primitives':[doc['meshes'][base['meshes']+node['mesh']]['primitives'][i]]});meshes[identity]=len(doc['meshes'])-1
    extensions.update(source.get('extensionsUsed',[]))
for e in game.s.entities:
    id,parent,kind,name,pos,rot,scale,components,static=e
    x,y,z=[math.radians(v)/2 for v in rot];cx,cy,cz=math.cos(x),math.cos(y),math.cos(z);sx,sy,sz=math.sin(x),math.sin(y),math.sin(z)
    node={'name':name,'translation':list(pos),'rotation':[sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz],'scale':list(scale)}
    for c in components:
        if c[0]=='astra.render.mesh':
            identity=c[2].split()[14]
            if identity in meshes and id not in game.hidden:node['mesh']=meshes[identity]
    doc['nodes'].append(node)
    if parent:doc['nodes'][parent-1].setdefault('children',[]).append(id-1)
doc['scenes']=[{'name':'Reserva de Vale Frio','nodes':[0]}];doc['scene']=0;doc['buffers']=[{'byteLength':len(binary)}];doc['extensionsUsed']=sorted(extensions)
binary.extend(b'\0'*(-len(binary)%4));encoded=json.dumps(doc,separators=(',',':'),ensure_ascii=False).encode();encoded+=b' '*(-len(encoded)%4)
path=out/'forest-world.glb';path.write_bytes(struct.pack('<4sII',b'glTF',2,28+len(encoded)+len(binary))+struct.pack('<I4s',len(encoded),b'JSON')+encoded+struct.pack('<I4s',len(binary),b'BIN\0')+binary)
print('EXPORTED',path,'bytes',path.stat().st_size,'mesh instances',sum('mesh' in n for n in doc['nodes']))
