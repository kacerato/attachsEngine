#!/usr/bin/env python3
"""Capture22 production material UI scenarios at655x300 and1280x720; no build."""
from __future__ import annotations
import argparse,hashlib,io,json,re,subprocess,sys
from pathlib import Path
SCENARIOS=('stream-main','stream-empty','stream-flow','stream-foam','stream-height','stream-uv','stream-map','stream-source','lava-main','lava-flow','lava-distortion','lava-colors','lava-height','lava-uv','lava-map','lava-source','rain-main','rain-empty','rain-map','rain-source','spec-height','spec-coincident')
SIZES=((655,300),(1280,720))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def publish(path,raw):
    if path.exists() and path.read_bytes()==raw:return
    staged=path.with_suffix(path.suffix+'.staged');staged.write_bytes(raw);staged.replace(path)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo',type=Path,default=Path.cwd())
    p.add_argument('--exe',type=Path)
    p.add_argument('--output',type=Path)
    p.add_argument('--scenario',action='append',choices=SCENARIOS,help='Repeat to select a subset; default all22')
    p.add_argument('--include-bottom',action='store_true',help='Also capture the actual final scroll position when nonzero')
    p.add_argument('--scroll-fraction',type=float,action='append',default=[],help='Additional scroll fractions0..1, useful for long packed-map pages')
    p.add_argument('--keep-ppm',action='store_true')
    p.add_argument('--plan',action='store_true',help='Print capture plan without executing or writing files')
    args=p.parse_args();repo=args.repo.resolve();scenarios=args.scenario or list(SCENARIOS)
    plan=[dict(scenario=s,width=w,height=h,scroll=0) for w,h in SIZES for s in scenarios]
    if any(x<0 or x>1 for x in args.scroll_fraction):p.error('--scroll-fraction must be between0 and1')
    if args.plan:print(json.dumps(plan,indent=2));return 0
    if not args.exe or not args.output:p.error('--exe and --output are required unless --plan')
    exe=args.exe.resolve();output=args.output.resolve()
    if not exe.is_file():p.error('Compile the production aether_terrain_surface_preview target separately first')
    if not (repo/'tests/native/terrain_surface_preview_main.cpp').is_file():p.error('--repo must point to the Astra checkout')
    if output==repo/'assets' or repo/'assets' in output.parents:p.error('Evidence output must stay outside active assets')
    from PIL import Image
    output.mkdir(parents=True,exist_ok=True);captures=[];failures=[]
    source_names=('tests/native/terrain_surface_preview_main.cpp','native/editor/editor_terrain_screen.cpp','native/editor/editor_terrain_streams_lava_controls.h','assets/terrain/library/catalog.json','assets/astra-visual/ui/astra-ui-icons.aeui')
    sources={n:digest(repo/n) for n in source_names}
    def run(s,w,h,scroll,tag):
        ppm=output/f'{s}-{w}x{h}{tag}.ppm';png=ppm.with_suffix('.png')
        command=[str(exe),str(ppm),str(w),str(h),str(scroll),s]
        completed=subprocess.run(command,cwd=repo,capture_output=True,text=True,encoding='utf-8',errors='replace')
        entry=dict(scenario=s,width=w,height=h,scroll=scroll,argv=command,exit_code=completed.returncode,stdout=completed.stdout,stderr=completed.stderr)
        if completed.returncode:
            failures.append(entry);print(f'FAIL {s} {w}x{h}: exit{completed.returncode}',file=sys.stderr);return None
        if not ppm.is_file():raise RuntimeError(f'Fixture returned success without output: {ppm}')
        with Image.open(ppm) as im:
            if im.size!=(w,h):raise ValueError(f'Unexpected dimensions {im.size}: {ppm}')
            data=io.BytesIO();im.save(data,format='PNG');publish(png,data.getvalue())
        entry.update(png=png.name,png_sha256=digest(png));captures.append(entry)
        if not args.keep_ppm:ppm.unlink()
        match=re.search(r'scroll\s+[-0-9.]+/([-0-9.]+)',completed.stdout)
        print(f'PASS {s} {w}x{h}{tag}')
        return float(match.group(1)) if match else None
    for item in plan:
        s,w,h=item['scenario'],item['width'],item['height'];limit=run(s,w,h,0,'')
        fractions=sorted(set(args.scroll_fraction+([1.] if args.include_bottom else [])))
        if limit and limit>0:
            for fraction in fractions:
                if fraction>0:run(s,w,h,limit*fraction,'-bottom' if fraction==1 else f'-scroll-{round(fraction*1000):03d}')
    manifest=dict(evidence='Production terrain editor UI software raster and routed reachability assertions; no 3D terrain/device claim',executable=str(exe),executable_sha256=digest(exe),source_sha256=sources,planned_top_captures=len(plan),captures=captures,failures=failures)
    publish(output/'manifest.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(f'{len(captures)} captures; {len(failures)} failures; manifest saved')
    return 1 if failures else 0
if __name__=='__main__':raise SystemExit(main())
