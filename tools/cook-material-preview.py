"""Offline sample import, not a runtime dependency. Requires Pillow, numpy and astcenc 5.7.
Preserves original 8K resolution; linear-light albedo mips and normalized normal mips.
Outputs versioned, tightly packed AETX mip chains, with reproducible source hashes.
"""
import argparse, hashlib, json, math, pathlib, struct, subprocess, urllib.request
import numpy as np
from PIL import Image

SOURCES = {
    "albedo": ("diff", "94e2d607ae4e5fdd517ee183b6d5a9cd"),
    "normal": ("nor_gl", "8f2275830e64d10cfcc57402545025e8"),
    "arm": ("arm", "94c7d7970fbdb91eac368fe4a914fb39"),
}

def write_texture(path, width, height, encoding, levels):
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", 0x58544541, 1, width, height, encoding, len(levels), len(payload)) + payload)

def resize_linear(data):
    # Exact area reduction for the power-of-two inputs; no gamma-space averaging.
    h, w, c = data.shape
    if h == 1 and w == 1:
        return data
    if h == 1: return data.reshape(1, w//2, 2, c).mean(axis=2)
    if w == 1: return data.reshape(h//2, 2, 1, c).mean(axis=1)
    return data.reshape(h//2, 2, w//2, 2, c).mean(axis=(1,3))

def srgb_decode(x): return np.where(x <= .04045, x/12.92, ((x+.055)/1.055)**2.4)
def srgb_encode(x): return np.where(x <= .0031308, x*12.92, 1.055*np.maximum(x,0)**(1/2.4)-.055)
def normalize(x): return x / np.maximum(np.linalg.norm(x,axis=-1,keepdims=True),1e-8)

def studio(d):
    # Original analytic HDR studio. Rectangular emitters in angular coordinates.
    result = np.broadcast_to(np.array([.055,.065,.085],np.float32),d.shape).copy()
    for direction, color, size in [
        ((-1,1,-1),(7,6.5,5.5),(.48,.7)),
        ((1,.3,-.4),(2.4,3.2,4.5),(.16,.85)),
        ((0,1,1),(4.5,4.5,4.5),(.7,.25))]:
        axis=normalize(np.array(direction,np.float32))
        side=normalize(np.cross(axis,np.array([0,1,0],np.float32)))
        up=np.cross(side,axis)
        z=np.sum(d*axis,axis=-1)
        x=np.sum(d*side,axis=-1)/np.maximum(z,.001)
        y=np.sum(d*up,axis=-1)/np.maximum(z,.001)
        soft=np.clip((size[0]-np.abs(x))/.06,0,1)*np.clip((size[1]-np.abs(y))/.06,0,1)*(z>0)
        result += soft[...,None]*np.array(color,np.float32)
    return result

def hammersley(i,n):
    r=0.; f=.5; v=i
    while v: r += (v&1)*f; v>>=1; f*=.5
    return i/n,r

def bake_environment(out):
    # GGX prefiltered lat-long, alpha = perceptual roughness squared.
    levels=[]
    for mip in range(9):
        w=max(1,256>>mip); h=max(1,128>>mip)
        u,v=np.meshgrid((np.arange(w)+.5)/w,(np.arange(h)+.5)/h)
        phi=(u-.5)*2*np.pi; theta=v*np.pi
        n=np.stack([np.sin(theta)*np.cos(phi),np.cos(theta),np.sin(theta)*np.sin(phi)],-1)
        tangent=normalize(np.cross(np.broadcast_to([0.,1.,0.],n.shape),n))
        bitangent=np.cross(n,tangent)
        total=np.zeros_like(n); weight=np.zeros((h,w,1))
        rough=mip/8; alpha=max(.001,rough*rough)
        for i in range(256 if mip else 1):
            x,y=hammersley(i,256)
            c=np.sqrt((1-y)/(1+(alpha*alpha-1)*y)); s=np.sqrt(max(0,1-c*c))
            half=tangent*(math.cos(2*np.pi*x)*s)+bitangent*(math.sin(2*np.pi*x)*s)+n*c
            light=2*np.sum(n*half,axis=-1,keepdims=True)*half-n
            nl=np.maximum(np.sum(n*light,axis=-1,keepdims=True),0)
            total+=studio(light)*nl; weight+=nl
        rgb=total/np.maximum(weight,1e-8)
        rgba=np.concatenate([rgb,np.ones((h,w,1))],-1)
        levels.append(rgba.astype("<f2").tobytes())
    write_texture(out/"studio.aetex",256,128,5,levels)
    # Split-sum BRDF integral, exact correlated Smith visibility.
    size=128
    nv,r=np.meshgrid((np.arange(size)+.5)/size,(np.arange(size)+.5)/size)
    view=np.stack([np.sqrt(1-nv*nv),np.zeros_like(nv),nv],-1)
    a=np.zeros_like(nv); b=a.copy(); alpha=r*r; alpha2=alpha*alpha
    for i in range(512):
        x,y=hammersley(i,512); c=np.sqrt((1-y)/(1+(alpha2-1)*y)); s=np.sqrt(1-c*c)
        half=np.stack([math.cos(2*np.pi*x)*s,math.sin(2*np.pi*x)*s,c],-1)
        vh=np.maximum(np.sum(view*half,axis=-1),0)
        light=2*vh[...,None]*half-view; nl=np.maximum(light[...,2],0)
        vis=.5/np.maximum(nl*np.sqrt(nv*nv*(1-alpha2)+alpha2)+nv*np.sqrt(nl*nl*(1-alpha2)+alpha2),1e-6)
        weight=4*vis*nl*vh/np.maximum(c,1e-6)
        f=(1-vh)**5
        a+=(1-f)*weight; b+=f*weight
    rgba=np.stack([a/512,b/512,np.zeros_like(a),np.ones_like(a)],-1)
    write_texture(out/"brdf.aetex",size,size,5,[rgba.astype("<f2").tobytes()])

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--astcenc",required=True)
    parser.add_argument("--cache",type=pathlib.Path,default=pathlib.Path("build/material-preview/source"))
    parser.add_argument("--out",type=pathlib.Path,default=pathlib.Path("samples/material-preview/Imported"))
    parser.add_argument("--environment-only",action="store_true")
    args=parser.parse_args(); args.cache.mkdir(parents=True,exist_ok=True); args.out.mkdir(parents=True,exist_ok=True)
    if args.environment_only:
        bake_environment(args.out)
        manifest_path=args.out.parent/"manifest.json"
        manifest=json.loads(manifest_path.read_text())
        manifest["outputs"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(args.out.glob("*.aetex"))}
        manifest_path.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf8")
        return
    records=[]
    for name,(suffix,expected) in SOURCES.items():
        url=f"https://dl.polyhaven.org/file/ph-assets/Textures/jpg/8k/metal_plate_02/metal_plate_02_{suffix}_8k.jpg"
        path=args.cache/f"{name}.jpg"
        if not path.exists(): urllib.request.urlretrieve(url,path)
        if hashlib.md5(path.read_bytes()).hexdigest()!=expected: raise ValueError(f"Source checksum mismatch: {name}")
        records.append(dict(map=name,url=url,md5=expected,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        data=np.asarray(Image.open(path).convert("RGB"),dtype=np.float32)/255
        if data.shape!=(8192,8192,3): raise ValueError("8K source dimensions required")
        if name=="albedo": data=srgb_decode(data)
        if name=="normal": data=data*2-1
        levels=[]; fallback=[]; mip=0
        while True:
            encoded=srgb_encode(data) if name=="albedo" else (normalize(data)*.5+.5 if name=="normal" else data)
            rgb=np.rint(np.clip(encoded,0,1)*255).astype(np.uint8)
            h,w=rgb.shape[:2]; png=args.cache/f"{name}-{mip}.png"; astc=png.with_suffix(".astc")
            Image.fromarray(rgb).save(png)
            subprocess.run([args.astcenc,"-cs" if name=="albedo" else "-cl",str(png),str(astc),"6x6","-medium","-j","4","-silent"],check=True)
            blob=astc.read_bytes()
            if blob[:4]!=bytes.fromhex("13aba15c"): raise ValueError("Invalid ASTC file")
            levels.append(blob[16:])
            if w<=1024:
                rgba=np.concatenate([rgb,np.full((h,w,1),255,np.uint8)],-1)
                fallback.append(rgba.tobytes())
            print(f"{name} mip={mip} {w}x{h}",flush=True)
            if w==1: break
            data=resize_linear(data); mip+=1
        write_texture(args.out/f"{name}.aetex",8192,8192,1 if name=="albedo" else 2,levels)
        write_texture(args.out/f"{name}-fallback.aetex",1024,1024,3 if name=="albedo" else 4,fallback)
    bake_environment(args.out)
    manifest=dict(version=1,asset="metal_plate_02",license="CC0-1.0",source="https://polyhaven.com/a/metal_plate_02",sources=records,
                  encoder="astcenc 5.7.0 medium 6x6",outputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(args.out.glob("*.aetex"))})
    (args.out.parent/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf8")

if __name__=="__main__": main()
