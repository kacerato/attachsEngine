"""40 ícones originais para a proposta; não altera o catálogo/atlas do aplicativo."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import html
import json
import math

OUT = Path(__file__).resolve().parent / 'icones'
OUT.mkdir(exist_ok=True)

def circle(x, y, radius):
    return [(x+radius*math.cos(i*math.tau/64), y+radius*math.sin(i*math.tau/64)) for i in range(65)]

def rect(x, y, w, h):
    return [(x,y),(x+w,y),(x+w,y+h),(x,y+h),(x,y)]

cube = [[(16,4),(27,10),(27,22),(16,28),(5,22),(5,10),(16,4)],[(5,10),(16,16),(27,10)],[(16,16),(16,28)]]
icons = {
 'component-stack': [rect(4,4,10,10),rect(18,4,10,10),rect(4,18,10,10),[(18,23),(28,23)],[(23,18),(23,28)]],
 'dependency': [rect(3,4,8,8),rect(21,20,8,8),rect(3,20,8,8),[(7,12),(7,16),(25,16),(25,20)],[(7,16),(7,20)]],
 'prefab': cube+[[(11,9),(16,6),(21,9)]],
 'variant': [[(6,5),(17,5),(17,16),(6,16),(6,5)],[(17,10),(25,10),(25,26),(11,26),(11,16)],[(21,21),(28,21)],[(25,17),(25,25)]],
 'resource': [[(7,4),(20,4),(26,10),(26,28),(7,28),(7,4)],[(20,4),(20,10),(26,10)],circle(16,19,4)],
 'override': [rect(4,6,16,20),[(13,20),(16,14),(25,5),(28,8),(19,17),(13,20)],[(21,9),(24,12)]],
 'link': [[(13,10),(16,7),(21,7),(25,11),(25,16),(22,19)],[(19,22),(16,25),(11,25),(7,21),(7,16),(10,13)],[(12,20),(20,12)]],
 'broken-link': [[(13,10),(16,7),(21,7),(25,11),(25,16),(22,19)],[(19,22),(16,25),(11,25),(7,21),(7,16),(10,13)],[(13,3),(10,6)],[(25,22),(28,25)],[(12,16),(16,12)],[(17,21),(21,17)]],
 'rigidbody': [rect(7,7,18,18),[(11,4),(21,4)],[(16,11),(16,22)],[(12,18),(16,22),(20,18)]],
 'collider': [rect(6,6,20,20),[(3,10),(3,3),(10,3)],[(22,3),(29,3),(29,10)],[(29,22),(29,29),(22,29)],[(10,29),(3,29),(3,22)]],
 'sensor': [circle(16,16,4),[(4,11),(2,16),(4,21)],[(8,12),(6,16),(8,20)],[(24,12),(26,16),(24,20)],[(28,11),(30,16),(28,21)]],
 'joint': [circle(10,11,5),circle(22,21,5),[(13,15),(19,18)],[(3,4),(7,8)],[(25,25),(29,29)]],
 'character': [circle(16,7,4),[(10,14),(22,14),(24,22)],[(10,14),(8,22)],[(16,14),(16,22),(11,29)],[(16,22),(21,29)]],
 'input-action': [rect(3,7,26,18),[(7,16),(15,16)],[(11,12),(11,20)],circle(23,13,1.4),circle(20,19,1.4)],
 'signal': [circle(7,16,3),[(12,9),(17,16),(12,23)],[(20,9),(27,16),(20,23)]],
 'timer': [circle(16,18,10),[(12,3),(20,3)],[(16,3),(16,8)],[(16,12),(16,18),(21,21)],[(25,6),(28,9)]],
 'material': [circle(16,16,11),[(9,8),(11,13),(10,21),(14,27)],[(16,5),(21,10),(23,18),(22,25)]],
 'shader': [rect(10,10,12,12),[(3,6),(8,6),(13,10)],[(3,26),(8,26),(13,22)],[(22,16),(28,16)],[(15,13),(18,16),(15,19)]],
 'skeleton': [circle(16,5,3),[(16,8),(16,20)],[(6,13),(16,11),(26,13)],[(6,13),(4,21)],[(26,13),(28,21)],[(16,20),(10,27)],[(16,20),(22,27)],circle(16,15,2)],
 'animation': [rect(3,5,22,21),[(8,9),(8,22),(19,16),(8,9)],[(28,9),(28,28),(7,28)]],
 'timeline': [[(4,8),(28,8)],[(4,16),(28,16)],[(4,24),(28,24)],[(11,3),(11,29)],[(11,12),(15,16),(11,20),(7,16),(11,12)],rect(18,21,7,6)],
 'particles': [circle(8,24,3),[(10,21),(17,14)],circle(21,7,2),circle(26,18,2),circle(9,9,1.5),[(17,23),(19,25)],[(22,25),(25,28)]],
 'terrain': [[(3,26),(11,9),(18,20),(23,13),(30,26),(3,26)],[(8,15),(11,18),(14,15)],[(5,28),(27,28)]],
 'spline': [[(4,25),(7,25),(10,21),(12,11),(16,6),(21,7),(24,15),(28,15)],circle(4,25,2),circle(16,6,2),circle(28,15,2)],
 'navigation': [[(4,26),(4,7),(12,7),(12,17),(23,17),(23,5)],[(19,9),(23,5),(27,9)],circle(4,26,2),[(16,24),(28,24)]],
 'sprite': [rect(4,4,24,24),[(8,23),(13,16),(17,20),(21,13),(25,23)],circle(11,10,2)],
 'tilemap': [rect(4,4,24,24),[(4,12),(28,12)],[(4,20),(28,20)],[(12,4),(12,28)],[(20,4),(20,28)],[(13,13),(19,19)],[(19,13),(13,19)]],
 'canvas': [rect(3,5,26,22),rect(7,10,9,12),[(20,11),(25,11)],[(20,16),(25,16)],[(20,21),(25,21)]],
 'text': [[(5,6),(27,6)],[(5,6),(5,10)],[(27,6),(27,10)],[(16,6),(16,26)],[(11,26),(21,26)]],
 'audio-source': [[(4,12),(10,12),(18,5),(18,27),(10,20),(4,20),(4,12)],[(23,10),(26,16),(23,22)],[(27,5),(31,16),(27,27)]],
 'audio-mixer': [[(7,4),(7,28)],[(16,4),(16,28)],[(25,4),(25,28)],rect(4,9,6,5),rect(13,19,6,5),rect(22,7,6,5)],
 'localization': [circle(16,16,12),[(4,16),(28,16)],[(16,4),(10,10),(9,16),(10,22),(16,28),(22,22),(23,16),(22,10),(16,4)],[(7,10),(25,10)],[(7,22),(25,22)]],
 'network': [circle(16,6,3),circle(6,25,3),circle(26,25,3),[(16,9),(16,17),(6,17),(6,22)],[(16,17),(26,17),(26,22)]],
 'xr': [[(3,10),(29,10),(29,23),(22,25),(17,20),(15,20),(10,25),(3,23),(3,10)],circle(9,16,3),circle(23,16,3),[(8,7),(24,7)]],
 'documentation': [[(16,8),(11,5),(3,5),(3,25),(11,25),(16,28),(21,25),(29,25),(29,5),(21,5),(16,8),(16,28)],[(7,11),(12,11)],[(7,16),(12,16)],[(20,11),(25,11)],[(20,16),(25,16)]],
 'search-all': [circle(13,13,8),[(19,19),(28,28)],[(10,13),(16,13)],[(13,10),(13,16)]],
 'dock': [rect(3,4,26,24),[(3,10),(29,10)],[(10,10),(10,28)],[(10,22),(29,22)],[(22,10),(22,22)]],
 'pin': [[(10,4),(22,4),(20,10),(25,17),(7,17),(12,10),(10,4)],[(16,17),(16,29)]],
 'layers': [[(3,11),(16,4),(29,11),(16,18),(3,11)],[(3,17),(16,24),(29,17)],[(3,23),(16,30),(29,23)]],
 'export-game': [rect(4,12,17,16),[(13,19),(28,4)],[(18,4),(28,4),(28,14)],[(8,16),(8,24),(15,24)]],
}
assert len(icons) == 40
font_path = Path('C:/Windows/Fonts/segoeui.ttf')
font = ImageFont.truetype(str(font_path), 16) if font_path.exists() else ImageFont.load_default()
sheet = Image.new('RGB', (1280,800), '#20252d')
sheet_draw = ImageDraw.Draw(sheet)
catalog = {'status':'Proposta, fora do atlas de runtime', 'grid':32, 'stroke':1.75, 'icons':{}}
cards=[]
for i,(name,strokes) in enumerate(icons.items()):
    raster=Image.new('RGBA',(1024,1024))
    draw=ImageDraw.Draw(raster)
    for stroke in strokes:
        points=[(x*32,y*32) for x,y in stroke]
        draw.line(points,fill=(236,241,238,255),width=56,joint='curve')
        for x,y in points:
            draw.ellipse((x-28,y-28,x+28,y+28),fill=(236,241,238,255))
    raster=raster.resize((512,512),Image.Resampling.LANCZOS)
    raster.save(OUT/(name+'.png'))
    geometry=''.join('<polyline points="'+' '.join(f'{x:.4f},{y:.4f}' for x,y in p)+'"/>' for p in strokes)
    svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" role="img" aria-label="'+name+'"><g fill="none" stroke="#ecf1ee" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round">'+geometry+'</g></svg>'
    (OUT/(name+'.svg')).write_text(svg,encoding='utf-8')
    x,y=(i%8)*160,(i//8)*160
    thumb=raster.resize((64,64),Image.Resampling.LANCZOS)
    sheet.paste(thumb,(x+48,y+28),thumb)
    sheet_draw.text((x+80,y+114),name,fill='#cdd6df',font=font,anchor='mm')
    catalog['icons']['proposal/'+name]={'source':name+'.svg','raster':name+'.png','dark_ui_ready':True}
    cards.append(f'<article><img src="{name}.svg" alt="{html.escape(name)}"><h2>{name}</h2><a href="{name}.svg">SVG</a> · <a href="{name}.png">PNG 512</a></article>')
sheet.save(OUT/'prancha.png')
(OUT/'catalogo.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(OUT/'galeria.html').write_text('''<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Ícones Astra — expansão</title><style>body{background:#171c23;color:#edf1f4;font:16px system-ui;margin:32px}a{color:#a3e873}h1{font-size:30px}p{color:#b9c3cf;max-width:850px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px}article{background:#252c35;padding:24px 12px;text-align:center;border-radius:9px}article img{width:64px;height:64px}h2{font-size:14px;font-weight:500}article a{font-size:13px}</style><a href="../README.md">← Plano Astra</a><h1>40 ícones · novas capacidades</h1><p>Desenhos originais para a expansão. Grade de 32 unidades, traço de 1,75. SVG editável e PNG transparente de 512 px. Proposta de design; não registrados no atlas do aplicativo.</p><main>'''+''.join(cards)+'</main></html>',encoding='utf-8')
print(f'{len(icons)} SVG + {len(icons)} PNG; galeria, catálogo e prancha gerados')
