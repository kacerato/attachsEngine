#!/usr/bin/env python3
"""Prancha lado a lado: frame da Unity (esquerda) × captura do POCO (direita).

Uso: python montar_prancha.py saida.png "titulo" unity.png poco.png [nota]
Os dois quadros são reduzidos à mesma altura; o título e a nota vão no topo.
"""
import sys
from PIL import Image, ImageDraw, ImageFont


def font(size):
    for name in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    out, title, left, right = sys.argv[1:5]
    note = sys.argv[5] if len(sys.argv) > 5 else ""
    height = 720
    images = []
    for path in (left, right):
        image = Image.open(path).convert("RGB")
        width = round(image.width * height / image.height)
        images.append(image.resize((width, height), Image.LANCZOS))
    gap, top = 16, 92
    board = Image.new("RGB", (images[0].width + images[1].width + gap * 3, height + top + gap), (21, 25, 30))
    draw = ImageDraw.Draw(board)
    draw.text((gap, 12), title, fill=(232, 240, 237), font=font(30))
    if note:
        draw.text((gap, 52), note, fill=(160, 170, 168), font=font(20))
    x = gap
    for label, image in zip(("Unity", "Astra · POCO F7"), images):
        board.paste(image, (x, top))
        draw.rectangle((x, top, x + 190, top + 34), fill=(21, 25, 30))
        draw.text((x + 10, top + 4), label, fill=(198, 239, 98), font=font(22))
        x += image.width + gap
    board.save(out)


if __name__ == "__main__":
    main()
