#!/usr/bin/env python3
"""Prepare editable Astra FX vectors + 384px masters, staging only (no atlas)."""
from pathlib import Path
import argparse, hashlib, json, math, re
from PIL import Image, ImageDraw
from generated_output import write_text_if_changed, publish_staged_if_changed
from ui_asset_format import cpp_identifier
ROOT=Path(__file__).resolve().parents[1]
NAMESPACE='zzzz-terrain-fx' # Legacy keys remain stable; identities() appends new concepts.
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
 'rain':('TerrainRain','Chuva e ondulações',[
  [(30,9),(23,21),(21,28),(24,34),(30,37),(36,34),(39,28),(37,21),(30,9)],
  [(66,17),(60,27),(58,33),(60,39),(66,42),(72,39),(74,33),(72,27),(66,17)],
  [(47,39),(44,47)],[(18,44),(15,51)],[(82,44),(79,51)],
  ellipse(48,70,35,14),ellipse(48,70,19,7),
 ]),
 'specular-height':('TerrainSpecularHeight','Brilho por altura',[
  [(15,83),(15,14)],[(7,24),(15,14),(23,24)],
  [(29,79),(84,79)],[(29,62),(68,62)],[(29,45),(49,45)],
  [(68,12),(72,26),(86,30),(72,34),(68,48),(64,34),(50,30),(64,26),(68,12)],
  [(83,48),(83,57)],[(79,52),(87,52)],
 ]),
}

def identities(current,published_header):
 """Resolve published concepts first; only genuinely new names append."""
 enums=dict(re.findall(r'\b(\w+)\s*=\s*(\d+),',published_header))
 maximum=max(current['icons'],default='');next_id=max(map(int,enums.values()),default=0)+1
 existing={};result={};append_prefix=None
 for key,metadata in current['icons'].items():
  if metadata.get('generator')=='tools/generate-terrain-flow-icons.py':
   concept=metadata.get('concept')
   if concept in existing:raise ValueError('Duplicate published FX concept')
   existing[concept]=key
 for name in sorted(MARKS):
  public=MARKS[name][0];key=existing.get(public)
  if key is None:
   key=f'{NAMESPACE}/{name}'
   if key in current['icons']:raise ValueError('Published FX identity conflicts with generator')
   if key<=maximum:
    if append_prefix is None:
     leading=max((len(n)-len(n.lstrip('z')) for n in current['icons']),default=4)
     append_prefix='z'*(leading+1)+'-terrain-fx'
    key=f'{append_prefix}/{name}'
   if key<=maximum or key in current['icons']:raise ValueError('New FX identity must append after published names')
   index=next_id;next_id+=1
  else:
   enum=cpp_identifier(key)
   if enum not in enums:raise ValueError('Published FX name is missing its stable enum')
   index=int(enums[enum])
  result[name]=(key,index,cpp_identifier(key))
 return result
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--output',type=Path,default=ROOT/'build/terrain-flows-next/icons')
 args=parser.parse_args();output=args.output.resolve()
 active=(ROOT/'assets/astra-visual/icons').resolve()
 if output==active or active in output.parents:parser.error('Output must be staging, outside active icons')
 header=ROOT/'native/ui/ui_icon_id.h';catalog=ROOT/'assets/astra-visual/icons/named/catalog.json'
 current=json.loads(catalog.read_text(encoding='utf-8'))
 count=int(re.search(r'kUiIconCount = (\d+)',header.read_text(encoding='utf-8')).group(1))
 published_header=header.read_text(encoding='utf-8')
 resolved=identities(current,published_header)
 output.mkdir(parents=True,exist_ok=True)
 patch={};manifest=[]
 for name in sorted(MARKS):
  public,title,lines=MARKS[name];key,index,enum=resolved[name]
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
  patch[key]={'source':f'vector/{key}.svg','generator':'tools/generate-terrain-flow-icons.py','append_after_brand':True,'concept':public,'label':title}
  manifest.append({'concept':public,'key':key,'enum':enum,'predicted_id':index,'master_dimensions':[384,384],'atlas_cell_dimensions':[96,96],'svg_sha256':hashlib.sha256(svg.read_bytes()).hexdigest(),'png_sha256':hashlib.sha256(png.read_bytes()).hexdigest()})
 write_text_if_changed(output/'catalog-append.json',json.dumps({'icons':patch},ensure_ascii=False,indent=2)+'\n')
 write_text_if_changed(output/'manifest.json',json.dumps({'published_count':count,'baseline_header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'baseline_catalog_sha256':hashlib.sha256(catalog.read_bytes()).hexdigest(),'icons':manifest},ensure_ascii=False,indent=2)+'\n')
 write_text_if_changed(output/'README.md',"# Astra FX icons staging\n\nSix editable SVG and RGBA PNG masters at 384x384, using Snow family stroke 4/96, rounded joins, #141414 and transparent background. Atlas remains 96px by the existing packer; masters are never replaced with reduced exports.\n\nNo active catalog, header, PNG or atlas changed. Merge only the icons object in catalog-append.json after checking baseline hashes; copy vector/named trees to existing icons root. Then parent may run pack-icon-atlas.py after UI capture. Published concept keys are reused; new concepts choose a namespace after the current maximum, preserving all IDs; manifest IDs are predictions until actual atlas generation. No fake runtime/UI capability implied by these art assets.\n\nConcepts: TerrainWetness (R), TerrainPuddle (G), TerrainStream (B), TerrainLava (A), TerrainRain and TerrainSpecularHeight. UI confirms these names; generated enums use ZzzzTerrainFx prefix.\n")
 print('Prepared '+str(len(MARKS))+' SVG + '+str(len(MARKS))+' full-resolution masters; IDs '+','.join(str(i['predicted_id']) for i in manifest)+'; no atlas generated.')
if __name__=='__main__':main()
