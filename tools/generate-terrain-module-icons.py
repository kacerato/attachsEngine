#!/usr/bin/env python3
"""Add Geo/global-map marks to Astra's existing editable vector icon family.

Names append after the existing catalog to preserve every published enum ID.
The 4x raster master and SVG use identical geometry; only the atlas is 96px.
"""
from pathlib import Path
import json
from PIL import Image, ImageDraw
from generated_output import write_text_if_changed, publish_staged_if_changed

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / 'assets/astra-visual/icons'
MARKS = {
    'strata': ('Terrain geological strata', [
        [(12,26),(35,18),(59,26),(84,18),(84,72),(60,80),(35,72),(12,80),(12,26)],
        [(12,40),(35,32),(59,40),(84,32)],
        [(12,54),(35,46),(59,54),(84,46)],
        [(12,68),(35,60),(59,68),(84,60)],
    ]),
    'global-maps': ('Whole terrain map overlay', [
        [(12,28),(48,10),(84,28),(48,46),(12,28)],
        [(12,28),(12,68),(48,86),(84,68),(84,28)],
        [(48,46),(48,86)],
        [(30,19),(66,37)],[(30,37),(66,19)],
        [(12,50),(48,68),(84,50)],
    ]),
}

def main():
    catalog_path = ICONS / 'named/catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    for name, (title, lines) in MARKS.items():
        svg = ICONS / f'vector/zz-terrain-module/{name}.svg'
        png = ICONS / f'named/zz-terrain-module/{name}.png'
        svg.parent.mkdir(parents=True, exist_ok=True)
        png.parent.mkdir(parents=True, exist_ok=True)
        paths = '\n'.join('    <polyline points="'+' '.join(f'{x},{y}' for x,y in line)+'"/>' for line in lines)
        write_text_if_changed(svg, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">\n'
            f'  <title>{title}</title>\n'
            '  <g fill="none" stroke="#141414" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">\n'
            f'{paths}\n  </g>\n</svg>\n')
        image = Image.new('RGBA', (384,384))
        draw = ImageDraw.Draw(image)
        for line in lines:
            points = [(x*4,y*4) for x,y in line]
            draw.line(points, fill=(20,20,20,255), width=16, joint='curve')
            for x,y in points:
                draw.ellipse((x-8,y-8,x+8,y+8), fill=(20,20,20,255))
        staged = png.with_suffix('.png.staged')
        image.save(staged, format='PNG', optimize=True)
        publish_staged_if_changed(staged,png)
        catalog['icons'][f'zz-terrain-module/{name}'] = {
            'source': f'vector/zz-terrain-module/{name}.svg',
            'generator': 'tools/generate-terrain-module-icons.py',
            'append_after_brand': True,
            'concept': 'terrain-geological-strata' if name=='strata' else 'terrain-global-map-domain',
        }
    write_text_if_changed(catalog_path,json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
    print('Generated Geo strata and whole-terrain map marks; existing IDs preserved.')

if __name__ == '__main__':
    main()
