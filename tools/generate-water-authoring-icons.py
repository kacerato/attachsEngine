"""Deterministic vector source + antialiased atlas inputs for water authoring."""
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[1]/'assets/astra-visual/icons'
paths={
 'surface': [[(12,25),(28,17),(44,25),(60,17),(76,25)],[(12,43),(28,35),(44,43),(60,35),(76,43)],[(12,61),(28,53),(44,61),(60,53),(76,61)]],
 'route': [[(18,72),(16,54),(36,42),(56,46),(72,26),(72,12)],[(30,72),(29,59),(46,54),(66,56),(84,30),(84,12)]],
 'physics': [[(28,15),(58,15),(68,25),(68,47),(38,47),(28,37),(28,15)],[(28,15),(38,25),(68,25)],[(38,25),(38,47)],[(12,64),(30,58),(48,64),(66,58),(84,64)],[(48,75),(48,90)],[(40,82),(48,90),(56,82)]],
 'layers': [[(12,26),(48,10),(84,26),(48,42),(12,26)],[(12,43),(48,59),(84,43)],[(12,60),(48,76),(84,60)]],
 'points': [[(12,70),(32,42),(62,52),(84,18)],[(48,8),(48,28)],[(38,18),(58,18)]],
 'flow': [[(10,25),(32,15),(55,25),(80,15)],[(66,9),(80,15),(72,27)],[(10,52),(32,42),(55,52),(80,42)],[(66,36),(80,42),(72,54)]],
}
def arc(cx,cy,r,start=0,end=360,n=32):
 return [(round(cx+r*math.cos(math.radians(start+(end-start)*i/n)),3),round(cy+r*math.sin(math.radians(start+(end-start)*i/n)),3)) for i in range(n+1)]

editor={
 'select':[[(22,12),(26,78),(43,60),(58,85),(70,77),(55,53),(80,50),(22,12)]],
 'move':[[(48,10),(48,86)],[(10,48),(86,48)],[(36,23),(48,10),(60,23)],[(36,73),(48,86),(60,73)],[(23,36),(10,48),(23,60)],[(73,36),(86,48),(73,60)]],
 'rotate':[arc(48,48,30,-40,275),[(30,12),(51,17),(41,34)]],
 'scale':[[(18,42),(18,78),(54,78),(54,42),(18,42)],[(53,43),(82,14)],[(60,14),(82,14),(82,36)]],
 'orbit':[arc(48,48,32),[(16,48),(80,48)],[(48,16),(30,32),(25,48),(30,64),(48,80),(66,64),(71,48),(66,32),(48,16)]],
 'pan':[[(25,52),(25,26),(33,22),(38,26),(38,12),(45,9),(51,14),(51,27),(55,19),(61,19),(65,25),(65,32),(70,27),(76,30),(77,56),(71,78),(43,85),(19,63),(14,51),(19,45),(25,52)],[(38,27),(38,45)],[(51,27),(51,45)],[(65,32),(65,47)]],
 'zoom':[arc(40,40,25),[(59,59),(84,84)]],
 'play':[[(28,14),(78,48),(28,82),(28,14)]],
 'stop':[[(24,24),(72,24),(72,72),(24,72),(24,24)]],
 'object':[[(48,12),(80,29),(80,67),(48,85),(16,67),(16,29),(48,12)],[(16,29),(48,47),(80,29)],[(48,47),(48,85)]],
 'folder':[[(12,26),(37,26),(44,35),(84,35),(84,76),(12,76),(12,26)]],
 'sun':[arc(48,48,18)]+[[(48+26*math.cos(a),48+26*math.sin(a)),(48+37*math.cos(a),48+37*math.sin(a))] for a in [i*math.pi/4 for i in range(8)]],
 'settings':[[(18,24),(78,24)],[(18,48),(78,48)],[(18,72),(78,72)],[(35,14),(35,34)],[(62,38),(62,58)],[(42,62),(42,82)]],
 'add':[[(48,17),(48,79)],[(17,48),(79,48)]],
 'more':[arc(48,y,3) for y in [24,48,72]],
 'chevron':[[(24,36),(48,60),(72,36)]],
 'frame':[[(15,37),(15,15),(37,15)],[(59,15),(81,15),(81,37)],[(81,59),(81,81),(59,81)],[(37,81),(15,81),(15,59)]],
 'grid':[[(16,16),(80,16),(80,80),(16,80),(16,16)],[(37,16),(37,80)],[(59,16),(59,80)],[(16,37),(80,37)],[(16,59),(80,59)]],
 'camera':[[(14,32),(31,32),(37,22),(60,22),(66,32),(82,32),(82,76),(14,76),(14,32)],arc(48,53,15)],
 'undo':[[(34,20),(14,38),(34,56)],[(14,38),(53,38),(73,48),(79,63),(74,79)]],
 'redo':[[(62,20),(82,38),(62,56)],[(82,38),(43,38),(23,48),(17,63),(22,79)]],
 'eye':[[(10,48),(27,30),(48,24),(69,30),(86,48),(69,66),(48,72),(27,66),(10,48)],arc(48,48,12)],
 'eye-off':[[(10,48),(27,30),(48,24),(69,30),(86,48),(69,66),(48,72),(27,66),(10,48)],[(14,14),(82,82)]],
}
catalog_path=root/'named/catalog.json';catalog=json.loads(catalog_path.read_text(encoding='utf-8'))
items=[('water',name,lines) for name,lines in paths.items()]+[('editor',name,lines) for name,lines in editor.items()]
for group,name,lines in items:
 leaf='author-'+name
 image=Image.new('RGBA',(384,384));draw=ImageDraw.Draw(image)
 for points in lines:
  scaled=[(x*4,y*4) for x,y in points]
  draw.line(scaled,fill=(20,20,20,255),width=16,joint='curve')
  for x,y in (scaled[0],scaled[-1]): draw.ellipse((x-8,y-8,x+8,y+8),fill=(20,20,20,255))
 if name=='points':
  for x,y in lines[0]: draw.ellipse((x*4-14,y*4-14,x*4+14,y*4+14),fill=(20,20,20,255))
 destination=root/'named'/group;destination.mkdir(parents=True,exist_ok=True)
 image.resize((96,96),Image.Resampling.LANCZOS).save(destination/f'{leaf}.png')
 source=root/'vector'/group;source.mkdir(parents=True,exist_ok=True)
 strokes=''.join('<polyline points="'+' '.join(f'{x},{y}' for x,y in line)+'"/>' for line in lines)
 if group=='water' and name=='points': strokes+=''.join(f'<circle cx="{x}" cy="{y}" r="3.5" fill="#141414" stroke="none"/>' for x,y in lines[0])
 (source/f'{leaf}.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96"><g fill="none" stroke="#141414" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">'+strokes+'</g></svg>',encoding='utf-8')
 catalog['icons'][group+'/'+leaf]={'source':'vector/'+group+'/'+leaf+'.svg'}
catalog_path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
