from pathlib import Path
import json,struct,numpy as np,fast_simplification
from scipy.spatial import cKDTree
ROOT=Path(__file__).resolve().parents[1]
def optimize(name):
    path=ROOT/'project/Assets'/f'{name}.glb'; original=ROOT/'sources'/f'{name}-original.glb';original.parent.mkdir(exist_ok=True,parents=True)
    if not original.exists():path.replace(original)
    raw=original.read_bytes();length=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+length]);binary=memoryview(raw)[28+length:]
    data=bytearray();views=[];access=[]
    def add(a,typ):
        a=np.ascontiguousarray(a);data.extend(b'\0'*(-len(data)%4));idx=len(views);views.append({'buffer':0,'byteOffset':len(data),'byteLength':a.nbytes});data.extend(a.tobytes());acc={'bufferView':idx,'componentType':5126 if a.dtype==np.float32 else 5125,'count':len(a),'type':typ}
        if typ=='VEC3':acc.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        access.append(acc);return len(access)-1
    def read(idx):
        a=doc['accessors'][idx];v=doc['bufferViews'][a['bufferView']];c={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];offset=v.get('byteOffset',0)+a.get('byteOffset',0);return np.ndarray((a['count'],c),dtype=dtype,buffer=binary,offset=offset,strides=(v.get('byteStride',np.dtype(dtype).itemsize*c),np.dtype(dtype).itemsize)).copy()
    # Use the first tree variation. Other variations are not authored into the game.
    doc['meshes']=doc['meshes'][:1];doc['nodes']=[{'name':name,'mesh':0}];doc['scenes']=[{'nodes':[0]}];doc['scene']=0
    for p in doc['meshes'][0]['primitives']:
        attrs={k:read(v) for k,v in p['attributes'].items()};faces=read(p['indices']).reshape(-1,3);positions=attrs['POSITION'];before=len(faces)
        if before>12000:
            # Collapse only after selecting deterministic distributed leaf clusters for extreme needle geometry.
            if before>300000:
                # Keep complete connected quads (each needle is two adjacent triangles), never isolated points.
                take=np.arange(0,len(faces)//2,max(1,len(faces)//2//5000))*2
                faces=faces[np.column_stack((take,take+1)).reshape(-1)]
                centers=positions[faces.reshape(-1,6)].mean(axis=1)
                expanded=positions.copy()
                for f,center in zip(faces.reshape(-1,6),centers):
                    expanded[f]=center+(positions[f]-center)*4
                attrs["POSITION"]=expanded
                keep,inv=np.unique(faces,return_inverse=True);attrs={k:a[keep] for k,a in attrs.items()};positions=attrs['POSITION'];faces=inv.reshape(-1,3)
            else:
                result,newfaces=fast_simplification.simplify(positions.astype(np.float64),faces.astype(np.int32),target_count=10000)
                nearest=cKDTree(positions).query(result)[1];attrs={k:a[nearest] for k,a in attrs.items()};attrs['POSITION']=result.astype(np.float32);faces=newfaces
        p['attributes']={k:add(a.astype(np.float32),{2:'VEC2',3:'VEC3',4:'VEC4'}[a.shape[1]]) for k,a in attrs.items()};p['indices']=add(faces.reshape(-1).astype(np.uint32),'SCALAR')
        print(name,before,'->',len(faces),flush=True)
    for im in doc.get('images',[]):
        v=doc['bufferViews'][im['bufferView']];data.extend(b'\0'*(-len(data)%4));im['bufferView']=len(views);blob=binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];views.append({'buffer':0,'byteOffset':len(data),'byteLength':len(blob)});data.extend(blob)
    doc['bufferViews']=views;doc['accessors']=access;doc['buffers']=[{'byteLength':len(data)}];data.extend(b'\0'*(-len(data)%4));encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
    path.write_bytes(struct.pack('<4sII',b'glTF',2,28+len(encoded)+len(data))+struct.pack('<I4s',len(encoded),b'JSON')+encoded+struct.pack('<I4s',len(data),b'BIN\0')+data);print('SAVED',path.stat().st_size,flush=True)
if __name__=='__main__':
    import sys
    for n in sys.argv[1:]:optimize(n)


