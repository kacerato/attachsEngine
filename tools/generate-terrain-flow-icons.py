#!/usr/bin/env python3
"""Prepare editable Astra FX vectors + 384px masters, staging only (no atlas)."""
from pathlib import Path
import argparse, hashlib, json, math, re
from PIL import Image, ImageDraw
from generated_output import write_text_if_changed, publish_staged_if_changed
ROOT=Path(__file__).resolve().parents[1]
NAMESPACE='zzzz-terrain-fx' # Sort after all published texture thumbnails.
def ellipse(cx,cy,rx,ry):
    return [(round(cx+rx*math.cos(n*math.pi/24),3),round(cy+ry*math.sin(n*math.pi/24),3)) for n in range(49)]
MARKS={
 'wetness':('TerrainWetness','Umidade',[
  [(43,10),(37,19),(27,32),(21,43),(20,53),(23,63),(30,71),(40,75),(50,74),(59,69),(65,60),(67,50),(64,40),(56,27),(43,10)],
  [(30,49),(30,55),(34,61),(40,64)],
  [(77,22),(71,31),(69,38),(72,44),(78,46),(84,42),(86,36),(83,30),(77,22)],
  [(15,84),(72,84)],
 ]),
 'puddle':('TerrainPuddle','Poca',[
  ellipse(48,65,35,17),ellipse(48,65,19,8),
  [(48,9),(37,25),(34,34),(37,42),(44,47),(52,47),(59,42),(62,34),(59,25),(48,9)],
  [(79,42),(84,38)],[(12,43),(17,47)],
 ]),
 'stream':('TerrainStream','Correnteza',[
  [(12,21),(27,21),(37,28),(47,30),(58,26),(69,19),(83,19)],[(74,10),(83,19),(74,28)],
  [(12,48),(25,48),(37,55),(49,57),(60,53),(71,46),(84,46)],[(75,37),(84,46),(75,55)],
  [(12,75),(25,75),(37,82),(49,84),(60,80),(71,73),(84,73)],[(75,64),(84,73),(75,82)],
 ]),
 'lava':('TerrainLava','Lava',[
  [(49,9),(49,27),(43,38),(34,32),(27,43),(23,55),(26,68),(34,78),(47,84),(60,82),(71,74),(77,62),(76,49),(68,35),(64,48),(56,40),(59,27),(49,9)],
  [(48,51),(38,65),(39,73),(46,78),(54,76),(59,69),(56,61),(48,51)],
  [(12,88),(31,88)],[(66,88),(85,88)],
 ]),
}
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--output',type=Path,default=ROOT/'build/terrain-flows-next/icons')
 args=parser.parse_args();output=args.output.resolve()
 active=(ROOT/'assets/astra-visual/icons').resolve()
 if output==active or active in output.parents:parser.error('Output must be staging, outside active icons')
 output.mkdir(parents=True,exist_ok=True)
 header=ROOT/'native/ui/ui_icon_id.h';catalog=ROOT/'assets/astra-visual/icons/named/catalog.json'
 current=json.loads(catalog.read_text(encoding='utf-8'))
 count=int(re.search(r'kUiIconCount = (\d+)',header.read_text(encoding='utf-8')).group(1))
 keys=[f'{NAMESPACE}/{name}' for name in sorted(MARKS)]
 published_header=header.read_text(encoding='utf-8')
 if any(name>=keys[0] for name in current['icons'] if name not in keys):raise ValueError('New namespace must follow all other published names to preserve enum IDs')
 patch={};manifest=[]
 for index,name in enumerate(sorted(MARKS),count+1):
  public,title,lines=MARKS[name];key=f'{NAMESPACE}/{name}'
  if key in current['icons']:
   existing=re.search(r'\bZzzzTerrainFx'+name.capitalize()+r'\s*=\s*(\d+)',published_header)
   if not existing or current['icons'][key].get('generator')!='tools/generate-terrain-flow-icons.py':raise ValueError('Published FX identity conflicts with generator')
   index=int(existing.group(1))
  svg=output/f'vector/{key}.svg';png=output/f'named/{key}.png'
  svg.parent.mkdir(parents=True,exist_ok=True);png.parent.mkdir(parents=True,exist_ok=True)
  paths='\n'.join('    <polyline points="'+' '.join(f'{x},{y}' for x,y in line)+'"/>' for line in lines)
  write_text_if_changed(svg,'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96" width="384" height="384">\n'+f'  <title>{title}</title>\n'+'  <g fill="none" stroke="#141414" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">\n'+paths+'\n  </g>\n</svg>\n')
  image=Image.new('RGBA',(384,384));draw=ImageDraw.Draw(image)
  for line in lines:
   points=[(round(x*4),round(y*4)) for x,y in line]
   draw.line(points,fill=(20,20,20,255),width=16,joint='curve')
   for x,y in points:draw.ellipse((x-8,y-8,x+8,y+8),fill=(20,20,20,255))
  staged=png.with_suffix('.png.staged');image.save(staged,format='PNG',optimize=True);publish_staged_if_changed(staged,png)
  enum='ZzzzTerrainFx'+name.title()
  patch[key]={'source':f'vector/{key}.svg','generator':'tools/generate-terrain-flow-icons.py','append_after_brand':True,'concept':public,'label':title}
  manifest.append({'concept':public,'key':key,'enum':enum,'predicted_id':index,'master_dimensions':[384,384],'atlas_cell_dimensions':[96,96],'svg_sha256':hashlib.sha256(svg.read_bytes()).hexdigest(),'png_sha256':hashlib.sha256(png.read_bytes()).hexdigest()})
 write_text_if_changed(output/'catalog-append.json',json.dumps({'icons':patch},ensure_ascii=False,indent=2)+'\n')
 write_text_if_changed(output/'manifest.json',json.dumps({'published_count':count,'baseline_header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'baseline_catalog_sha256':hashlib.sha256(catalog.read_bytes()).hexdigest(),'icons':manifest},ensure_ascii=False,indent=2)+'\n')
 write_text_if_changed(output/'README.md',"# Astra FX icons staging\n\nFour editable SVG and RGBA PNG masters at 384x384, using Snow family stroke 4/96, rounded joins, #141414 and transparent background. Atlas remains 96px by the existing packer; masters are never replaced with reduced exports.\n\nNo active catalog, header, PNG or atlas changed. Merge only the icons object in catalog-append.json after checking baseline hashes; copy vector/named trees to existing icons root. Then parent may run pack-icon-atlas.py after UI capture. Namespace zzzz-terrain-fx sorts after existing zzz texture entries, preserving all IDs; manifest IDs are predictions until actual atlas generation. No fake runtime/UI capability implied by these art assets.\n\nConcepts: TerrainWetness (R), TerrainPuddle (G), TerrainStream (B), TerrainLava (A). UI confirms these names; generated enums use ZzzzTerrainFx prefix.\n")
 print('Prepared 4 SVG + 4 full-resolution masters; IDs '+','.join(str(i['predicted_id']) for i in manifest)+'; no atlas generated.')
if __name__=='__main__':main()
