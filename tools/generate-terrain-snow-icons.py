#!/usr/bin/env python3
"""Add Snow coverage/lighting/residency marks to Astra's existing editable vector icon family.

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
    'coverage': ('Snow coverage', [
        [(48,10),(48,86)],[(15,29),(81,67)],[(15,67),(81,29)],
        [(36,18),(48,30),(60,18)],[(36,78),(48,66),(60,78)],
        [(17,43),(32,39),(28,24)],[(68,72),(64,57),(79,53)],
        [(17,53),(32,57),(28,72)],[(68,24),(64,39),(79,43)],
    ]),
    'lighting': ('Snow lighting and sparkle', [
        [(35,30),(35,84)],[(12,44),(58,70)],[(12,70),(58,44)],
        [(25,34),(35,44),(45,34)],[(25,80),(35,70),(45,80)],
        [(70,10),(74,24),(88,28),(74,32),(70,46),(66,32),(52,28),(66,24),(70,10)],
    ]),
    'residency': ('Texture residency ceiling', [
        [(12,12),(84,12),(84,84),(12,84),(12,12)],
        [(26,26),(70,26),(70,70),(26,70),(26,26)],
        [(38,38),(58,38),(58,58),(38,58),(38,38)],
        [(12,48),(26,48)],[(70,48),(84,48)],
        [(48,12),(48,26)],[(48,70),(48,84)],
    ]),
}

def main():
    catalog_path = ICONS / 'named/catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    for name, (title, lines) in MARKS.items():
        svg = ICONS / f'vector/zz-terrain-snow/{name}.svg'
        png = ICONS / f'named/zz-terrain-snow/{name}.png'
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
        catalog['icons'][f'zz-terrain-snow/{name}'] = {
            'source': f'vector/zz-terrain-snow/{name}.svg',
            'generator': 'tools/generate-terrain-snow-icons.py',
            'append_after_brand': True,
            'concept': f'terrain-snow-{name}',
        }
    write_text_if_changed(catalog_path,json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
    print('Generated Snow coverage, lighting and texture-residency marks; existing IDs preserved.')

if __name__ == '__main__':
    main()
