"""Deterministic authored glTF fixture: hinged panel, three transform clips (no skin)."""
import json, math, struct
from pathlib import Path
buf=bytearray(); views=[]; accessors=[]
def accessor(values, fmt, kind, component, low=None, high=None):
    while len(buf)%4: buf.append(0)
    start=len(buf)
    for value in values: buf.extend(struct.pack("<"+fmt, *value))
    views.append({"buffer":0,"byteOffset":start,"byteLength":len(buf)-start})
    entry={"bufferView":len(views)-1,"componentType":component,"count":len(values),"type":kind}
    if low is not None: entry.update(min=low,max=high)
    accessors.append(entry); return len(accessors)-1
vertices=[(0,0,-3),(80,0,-3),(80,100,-3),(0,100,-3),(0,0,3),(80,0,3),(80,100,3),(0,100,3)]
positions=accessor(vertices,"3f","VEC3",5126,[0,0,-3],[80,100,3])
triangles=[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5]
indices=accessor([(x,) for x in triangles],"H","SCALAR",5123)
times=accessor([(0.,),(1.,)],"f","SCALAR",5126,[0],[1])
animations=[]
for name, angle in [("Fechada",0),("Meia abertura",math.pi/4),("Aberta",math.pi/2)]:
    q=(0,math.sin(angle/2),0,math.cos(angle/2))
    rotations=accessor([q,q],"4f","VEC4",5126)
    animations.append({"name":name,"samplers":[{"input":times,"output":rotations,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":1,"path":"rotation"}}]})
model={"asset":{"version":"2.0","generator":"Astra acceptance fixture"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"Mecanismo","children":[1]},{"name":"Painel","mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":positions},"indices":indices,"material":0}]}],"materials":[{"name":"Painel","doubleSided":True,"pbrMetallicRoughness":{"baseColorFactor":[.25,.7,.4,1],"metallicFactor":0,"roughnessFactor":.8}}],"animations":animations,"buffers":[{"byteLength":len(buf)}],"bufferViews":views,"accessors":accessors}
js=json.dumps(model,separators=(",",":")).encode();js+=b" "*((-len(js))%4);buf+=bytes((-len(buf))%4)
glb=struct.pack("<III",0x46546c67,2,12+8+len(js)+8+len(buf))+struct.pack("<II",len(js),0x4e4f534a)+js+struct.pack("<II",len(buf),0x004e4942)+buf
Path(__file__).with_name("Mechanism.glb").write_bytes(glb)
