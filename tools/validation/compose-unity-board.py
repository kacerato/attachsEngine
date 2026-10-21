#!/usr/bin/env python3
"""Prancha de comparação Unity × aparelho para a validação visual.

Cada linha da prancha é um par: o frame de referência de um vídeo da Unity (com
link e timestamp) e a captura do POCO no mesmo assunto. O arquivo de entrada é
um JSON com o título e as linhas:

  {"title": "...", "rows": [{"unity": "ref.png", "device": "cap.png",
                             "unity_caption": "Brackeys 5:40", "device_caption": "POCO F7 · Astra Dev 181",
                             "verdict": "o que bate e o que não bate"}]}

Uso: python tools/validation/compose-unity-board.py board.json saida.png
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CELL_W, CELL_H = 960, 540
MARGIN, CAPTION, VERDICT = 24, 44, 64
BACKGROUND = (20, 23, 28)
INK = (232, 240, 237)
MUTED = (150, 160, 158)
ACCENT = (198, 239, 98)


def font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for candidate in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            continue
    return ImageFont.load_default()


def fit(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGB")
    scale = min(CELL_W / image.width, CELL_H / image.height)
    resized = image.resize((max(1, int(image.width * scale)), max(1, int(image.height * scale))), Image.LANCZOS)
    cell = Image.new("RGB", (CELL_W, CELL_H), (12, 14, 17))
    cell.paste(resized, ((CELL_W - resized.width) // 2, (CELL_H - resized.height) // 2))
    return cell


def main() -> None:
    spec_path, output = Path(sys.argv[1]), Path(sys.argv[2])
    spec = json.loads(spec_path.read_text(encoding="utf-8"))
    base = spec_path.parent
    rows = spec["rows"]
    row_h = CELL_H + CAPTION + VERDICT + MARGIN
    width = MARGIN * 3 + CELL_W * 2
    height = 90 + row_h * len(rows) + MARGIN
    board = Image.new("RGB", (width, height), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((MARGIN, 26), spec["title"], fill=INK, font=font(34))
    draw.text((MARGIN + CELL_W + MARGIN, 34), "esquerda: Unity (vídeo) · direita: Astra no POCO F7", fill=MUTED, font=font(22))
    y = 90
    for row in rows:
        for column, key in enumerate(("unity", "device")):
            x = MARGIN + column * (CELL_W + MARGIN)
            board.paste(fit(base / row[key]), (x, y))
            draw.rectangle((x, y, x + CELL_W - 1, y + CELL_H - 1), outline=(60, 66, 72), width=2)
            caption = row.get(f"{key}_caption", "")
            draw.text((x, y + CELL_H + 8), caption, fill=ACCENT if column else INK, font=font(24))
        draw.text((MARGIN, y + CELL_H + CAPTION + 4), row.get("verdict", ""), fill=MUTED, font=font(22))
        y += row_h
    output.parent.mkdir(parents=True, exist_ok=True)
    board.save(output)
    print(f"prancha {width}x{height}: {output}")


if __name__ == "__main__":
    main()
