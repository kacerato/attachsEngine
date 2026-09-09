"""Ícones originais Astra: fontes vetoriais e PNGs transparentes de 512 px.

Geometria própria, sem imagens ou código Godot. O atlas usa os mesmos IDs
existentes; a resolução de origem não aumenta as texturas residentes no mobile.
"""
from pathlib import Path
import json
import math
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1] / 'assets/astra-visual/icons'
OUTPUT = ROOT / 'hd-v1'
OUTPUT.mkdir(exist_ok=True)

def arc(cx, cy, r, start=0, end=360):
    return [(cx+r*math.cos(math.radians(start+(end-start)*i/64)),
             cy+r*math.sin(math.radians(start+(end-start)*i/64))) for i in range(65)]

paths = {
    'select': [[(8,5),(25,17),(17,19),(13,27),(8,5)]],
    'move': [[(16,4),(16,28)],[(4,16),(28,16)],[(12,8),(16,4),(20,8)],[(12,24),(16,28),(20,24)],[(8,12),(4,16),(8,20)],[(24,12),(28,16),(24,20)]],
    'rotate': [arc(16,16,10,-50,255),[(10,4),(15,6),(12,11)]],
    'scale': [[(5,15),(5,27),(17,27),(17,15),(5,15)],[(17,15),(27,5)],[(20,5),(27,5),(27,12)]],
    'object': [[(16,4),(27,10),(27,22),(16,28),(5,22),(5,10),(16,4)],[(5,10),(16,16),(27,10)],[(16,16),(16,28)]],
    'folder': [[(4,10),(4,7),(12,7),(15,10),(28,10),(25,26),(4,26),(4,10),(28,10)]],
    'camera': [[(4,9),(22,9),(22,24),(4,24),(4,9)],[(22,13),(29,9),(29,24),(22,20)]],
    'play': [[(10,5),(26,16),(10,27),(10,5)]],
    'stop': [[(7,7),(25,7),(25,25),(7,25),(7,7)]],
    'add': [[(16,6),(16,26)],[(6,16),(26,16)]],
    'more': [arc(16,y,1) for y in (7,16,25)],
    'chevron': [[(8,12),(16,20),(24,12)]],
    'frame': [[(4,12),(4,4),(12,4)],[(20,4),(28,4),(28,12)],[(28,20),(28,28),(20,28)],[(12,28),(4,28),(4,20)]],
    'grid': [[(5,5),(27,5),(27,27),(5,27),(5,5)],[(12,5),(12,27)],[(20,5),(20,27)],[(5,12),(27,12)],[(5,20),(27,20)]],
    'zoom': [arc(13,13,8),[(19,19),(28,28)]],
    'orbit': [arc(16,16,11),[(5,16),(27,16)],[(16,5),(10,11),(9,16),(10,21),(16,27),(22,21),(23,16),(22,11),(16,5)]],
    'pan': [[(10,16),(10,8),(13,7),(15,9),(15,5),(18,5),(19,11),(22,9),(24,11),(24,14),(27,13),(28,16),(25,26),(15,28),(6,20),(5,17),(7,15),(10,18)]],
    'undo': [[(11,5),(4,12),(11,19)],[(4,12),(18,12),(24,15),(27,21),(25,27)]],
    'redo': [[(21,5),(28,12),(21,19)],[(28,12),(14,12),(8,15),(5,21),(7,27)]],
    'settings': [[(5,y),(27,y)] for y in (8,16,24)],
    'eye': [[(3,16),(9,10),(16,8),(23,10),(29,16),(23,22),(16,24),(9,22),(3,16)],arc(16,16,4)],
    'eye-off': [[(3,16),(9,10),(16,8),(23,10),(29,16),(23,22),(16,24),(9,22),(3,16)],[(5,4),(27,28)]],
    'sun': [arc(16,16,5)]+[[(16+8*math.cos(i*math.pi/4),16+8*math.sin(i*math.pi/4)),(16+12*math.cos(i*math.pi/4),16+12*math.sin(i*math.pi/4))] for i in range(8)],
}
paths['settings'] += [[(x,y-3),(x,y+3)] for x,y in ((11,8),(21,16),(14,24))]
catalog_path = ROOT / 'named/catalog.json'
catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
for name, strokes in paths.items():
    image = Image.new('RGBA', (1024,1024))
    draw = ImageDraw.Draw(image)
    for stroke in strokes:
        points = [(x*32,y*32) for x,y in stroke]
        draw.line(points, fill=(236,241,238,255), width=56, joint='curve')
        for x,y in (points[0],points[-1]):
            draw.ellipse((x-28,y-28,x+28,y+28), fill=(236,241,238,255))
    image.resize((512,512),Image.Resampling.LANCZOS).save(OUTPUT / (name+'.png'))
    geometry = ''.join('<polyline points="'+' '.join(f'{x:.4f},{y:.4f}' for x,y in stroke)+'"/>' for stroke in strokes)
    (OUTPUT / (name+'.svg')).write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><g fill="none" stroke="#ecf1ee" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round">'+geometry+'</g></svg>',encoding='utf-8')
    key='editor/author-'+name
    if key not in catalog['icons']:
        raise RuntimeError('Identificador ausente: '+key)
    catalog['icons'][key]={'source':'hd-v1/'+name+'.svg','raster':'../hd-v1/'+name+'.png','dark_ui_ready':True}
catalog_path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'{len(paths)} ícones originais PNG 512x512 e fontes SVG')
