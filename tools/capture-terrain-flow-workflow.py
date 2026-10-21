#!/usr/bin/env python3
"""Capture four full production editor material contexts with actual selected root."""
import argparse,hashlib,io,json,subprocess
from pathlib import Path
SCENARIOS={'material':('material-stream','material-lava','material-rain','material-spec-height'),'search':('stream-search-sha',)}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--exe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--repo',type=Path,default=Path.cwd());p.add_argument('--keep-ppm',action='store_true');p.add_argument('--set',choices=tuple(SCENARIOS),default='material');args=p.parse_args()
    from PIL import Image
    exe=args.exe.resolve();output=args.output.resolve();repo=args.repo.resolve()
    if not exe.is_file():p.error('Compile the prepared full-editor overlay separately first')
    output.mkdir(parents=True,exist_ok=True);captures=[]
    for width,height in ((655,300),(1280,720)):
        completed=subprocess.run([str(exe),str(output),str(width),str(height),args.set],cwd=repo,capture_output=True,text=True,encoding='utf-8',errors='replace')
        if completed.returncode:raise RuntimeError(f'Full UI fixture exit{completed.returncode}: {completed.stdout}\n{completed.stderr}')
        for scenario in SCENARIOS[args.set]:
            ppm=output/f'{scenario}-{width}x{height}.ppm';png=ppm.with_suffix('.png')
            with Image.open(ppm) as image:
                if image.size!=(width,height):raise ValueError(f'Unexpected size for {ppm}')
                raw=io.BytesIO();image.save(raw,format='PNG')
                if not png.exists() or png.read_bytes()!=raw.getvalue():png.write_bytes(raw.getvalue())
            captures.append(dict(scenario=scenario,width=width,height=height,png=png.name,png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(),stdout=completed.stdout))
            if not args.keep_ppm:ppm.unlink()
    manifest=dict(evidence='Full production editor UI, actual published terrain root/selection, pending material draft prepared with real catalog sources; software UI raster only, no 3D/device claim',executable=str(exe),executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),captures=captures)
    (output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(str(len(captures))+' full editor UI captures saved; no 3D/device claim')
if __name__=='__main__':main()
