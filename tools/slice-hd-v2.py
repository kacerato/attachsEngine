#!/usr/bin/env python3
"""Fatia as folhas HD v2 em icones individuais, quadrados e centrados.

As folhas sao desenhadas SOBRE fundo escuro, com brilho externo e alfa. Por
isso o corte nao e por grade fixa: as celulas nao tem passo uniforme e o brilho
de um icone chega perto do vizinho. Um rotulamento por componente conexa no
alfa acha a extensao real de cada desenho, inclusive o halo, que faz parte dele.

O recorte vira um QUADRADO centrado no maior lado. Icones de larguras
diferentes esticados para a mesma celula teriam pesos visuais diferentes na
barra; manter a proporcao e centrar e o que faz uma fileira parecer alinhada.
"""
from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image

SCALE = 4
# Duas leituras do alfa, de proposito. O HALO conta como parte do desenho e
# entra no recorte; a CAIXA vem so do corpo solido. Medir a caixa pelo halo
# encolhe o simbolo dentro da celula -- o brilho e largo, e um icone que ocupa
# metade do botao parece um icone menor, nao um icone com brilho.
ALPHA = 40
SOLID = 150
MINIMUM_CELLS = 400
# Bandas de linha: as folhas tem fileiras, mas com alturas diferentes por folha.
# Arredondar o centro vertical por esta altura agrupa a fileira sem exigir uma
# grade.
ROW_BAND = 150


def components(path: Path) -> list[tuple[int, int, int, int]]:
    alpha = np.asarray(Image.open(path).convert("RGBA"))[..., 3]
    # A componente e achada pelo HALO, senao um icone cujo corpo solido tem duas
    # partes separadas -- o alto-falante e suas ondas, o quadro e o triangulo do
    # passo -- viraria dois icones. A CAIXA sai do corpo solido dentro dela.
    small = alpha[::SCALE, ::SCALE]
    mask = small > ALPHA
    solid = small > SOLID
    height, width = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    found: list[tuple[int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if not mask[y, x] or seen[y, x]:
                continue
            queue = deque([(y, x)])
            seen[y, x] = True
            top = bottom = None
            left = right = None
            area = 0
            halo = [y, y, x, x]
            while queue:
                cy, cx = queue.popleft()
                area += 1
                halo[0] = min(halo[0], cy); halo[1] = max(halo[1], cy)
                halo[2] = min(halo[2], cx); halo[3] = max(halo[3], cx)
                if solid[cy, cx]:
                    top = cy if top is None else min(top, cy)
                    bottom = cy if bottom is None else max(bottom, cy)
                    left = cx if left is None else min(left, cx)
                    right = cx if right is None else max(right, cx)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < height and 0 <= nx < width and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            queue.append((ny, nx))
            if area < MINIMUM_CELLS:
                continue
            if top is None:
                top, bottom, left, right = halo[0], halo[1], halo[2], halo[3]
            found.append((left * SCALE, top * SCALE, (right + 1) * SCALE, (bottom + 1) * SCALE))
    found.sort(key=lambda box: (round((box[1] + box[3]) / 2 / ROW_BAND), box[0]))
    return found


def square(image: Image.Image, box: tuple[int, int, int, int], margin: float = 0.08) -> Image.Image:
    left, top, right, bottom = box
    side = max(right - left, bottom - top)
    side = int(side * (1 + margin * 2))
    cx = (left + right) // 2
    cy = (top + bottom) // 2
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    crop = image.crop((cx - side // 2, cy - side // 2, cx - side // 2 + side, cy - side // 2 + side))
    canvas.paste(crop, (0, 0))
    return canvas


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path("assets/astra-visual/icons/source/hd-v2")
    parser.add_argument("--source", type=Path, default=root)
    parser.add_argument("--output", type=Path, default=root / "sliced")
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)

    total = 0
    for sheet in sorted(arguments.source.glob("sheet-*.png")):
        image = Image.open(sheet).convert("RGBA")
        for index, box in enumerate(components(sheet)):
            square(image, box).save(arguments.output / f"{sheet.stem}-{index:02d}.png")
            total += 1
        print(f"{sheet.name}: {index + 1}")
    print(f"total {total}")


if __name__ == "__main__":
    main()
