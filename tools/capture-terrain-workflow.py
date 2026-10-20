"""Run the compiled production editor UI fixture and save lossless PNG evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
executable, output = args.exe.resolve(), args.output.resolve()
if not executable.is_file():
    parser.error('Compile aether_terrain_workflow_preview first')
output.mkdir(parents=True, exist_ok=True)
scenarios = ['fx-edit', 'fx-paint', 'fx-transform-move', 'fx-transform-rotate',
             'fx-transform-scale', 'fx-navigation', 'fx-applied-paint',
             'fx-applied-navigation']
captures = []
for width, height in [(655, 300), (1280, 720)]:
    subprocess.run([str(executable), str(output), str(width), str(height)], cwd=root, check=True)
    for scenario in scenarios:
        ppm = output / f'{scenario}-{width}x{height}.ppm'
        png = ppm.with_suffix('.png')
        with Image.open(ppm) as image:
            if image.size != (width, height):
                raise ValueError(f'Unexpected capture dimensions: {ppm}')
            image.save(png, format='PNG')
        captures.append(dict(scenario=scenario, width=width, height=height,
                             png_sha256=hashlib.sha256(png.read_bytes()).hexdigest()))
(output / 'manifest.json').write_text(json.dumps(dict(
    evidence='Production full editor UI software raster with actual document, material publication and selection; no 3D/device claim',
    captures=captures), indent=2) + '\n', encoding='utf-8')
print('16 full production UI captures; no resizing; assertions and manifest saved.')
