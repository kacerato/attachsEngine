"""Prancha Unity × POCO a partir de um JSON (mesmo formato da prancha da V-2).

{"title": "...", "rows": [{"unity": "rel.jpg", "device": "rel.jpg", "unity_caption": "...",
  "device_caption": "...", "verdict": "..."}]}
Caminhos relativos ao JSON. Uso: python tools/build-comparison-board.py prancha.json saida.jpg
"""
import json
import os
import sys
import textwrap

from PIL import Image, ImageDraw, ImageFont

CELL_W, CELL_H, PAD, CAPTION_H, VERDICT_H, TITLE_H = 900, 416, 16, 34, 58, 64
BACKGROUND, INK, MUTED, ACCENT = (18, 20, 24), (236, 238, 242), (160, 166, 176), (164, 230, 60)


def font(size, bold=False):
    for name in (('segoeuib.ttf' if bold else 'segoeui.ttf'), 'arial.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def fit(path):
    image = Image.open(path).convert('RGB')
    scale = min(CELL_W / image.width, CELL_H / image.height)
    image = image.resize((max(1, int(image.width * scale)), max(1, int(image.height * scale))), Image.LANCZOS)
    cell = Image.new('RGB', (CELL_W, CELL_H), (0, 0, 0))
    cell.paste(image, ((CELL_W - image.width) // 2, (CELL_H - image.height) // 2))
    return cell


def main():
    spec_path, output = sys.argv[1], sys.argv[2]
    spec = json.load(open(spec_path, encoding='utf-8'))
    base = os.path.dirname(os.path.abspath(spec_path))
    rows = spec['rows']
    row_h = CELL_H + CAPTION_H + VERDICT_H + PAD
    board = Image.new('RGB', (PAD * 3 + CELL_W * 2, TITLE_H + row_h * len(rows) + PAD), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((PAD, 18), spec['title'], font=font(28, True), fill=INK)
    small, verdict_font = font(18), font(19)
    for index, row in enumerate(rows):
        top = TITLE_H + index * row_h
        for column, key in enumerate(('unity', 'device')):
            left = PAD + column * (CELL_W + PAD)
            if row.get(key):
                board.paste(fit(os.path.join(base, row[key])), (left, top))
            else:
                draw.rectangle((left, top, left + CELL_W, top + CELL_H), outline=MUTED)
                draw.text((left + 20, top + CELL_H // 2), 'sem frame de referência (ver README)', font=small, fill=MUTED)
            caption = row.get(key + '_caption', '')
            draw.text((left, top + CELL_H + 6), caption, font=small, fill=ACCENT if column else MUTED)
        lines = textwrap.wrap(row.get('verdict', ''), 170)[:2]
        for line_index, line in enumerate(lines):
            draw.text((PAD, top + CELL_H + CAPTION_H + 4 + line_index * 24), line, font=verdict_font, fill=INK)
    board.save(output, quality=88)
    print(output, board.size)


if __name__ == '__main__':
    main()
