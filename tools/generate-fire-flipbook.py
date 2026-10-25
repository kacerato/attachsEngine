#!/usr/bin/env python3
"""Folha de fogo 4x4 (16 quadros) para validar a Texture Sheet Animation.

Gerada por matemática, sem arquivo de pacote (a licença da Asset Store não
permite redistribuir as folhas do RPG VFX Bundle). Cada quadro é uma chama que
sobe e tremula: ruído de valor rolando para cima, recortado por uma silhueta
que afina com a altura; cor do branco-amarelo do miolo ao vermelho da ponta,
alfa reto. Mesmo binário -> mesmos bytes (semente fixa).

    python tools/generate-fire-flipbook.py examples/particles/fogo-folha-4x4.png
"""
import sys

import numpy as np
from PIL import Image

TILE = 128
GRID = 4


def value_noise(shape, scale, seed):
    rng = np.random.default_rng(seed)
    gh, gw = int(shape[0] / scale) + 3, int(shape[1] / scale) + 3
    grid = rng.random((gh, gw))
    ys = np.arange(shape[0]) / scale
    xs = np.arange(shape[1]) / scale
    y0 = np.floor(ys).astype(int)
    x0 = np.floor(xs).astype(int)
    fy = ys - y0
    fx = xs - x0
    fy = fy * fy * (3 - 2 * fy)
    fx = fx * fx * (3 - 2 * fx)
    a = grid[y0][:, x0]
    b = grid[y0][:, x0 + 1]
    c = grid[y0 + 1][:, x0]
    d = grid[y0 + 1][:, x0 + 1]
    top = a + (b - a) * fx[None, :]
    bottom = c + (d - c) * fx[None, :]
    return top + (bottom - top) * fy[:, None]


def main(out):
    # Ruído alto, rolado para cima a cada quadro (a chama sobe).
    tall = TILE * 3
    noise = value_noise((tall, TILE), 18.0, 7) * 0.65 + value_noise((tall, TILE), 7.0, 11) * 0.35
    sheet = np.zeros((TILE * GRID, TILE * GRID, 4), dtype=np.float32)
    ys, xs = np.mgrid[0:TILE, 0:TILE]
    height = 1.0 - ys / (TILE - 1)            # 0 embaixo, 1 em cima
    centred = (xs - (TILE - 1) / 2) / (TILE / 2)
    for frame in range(GRID * GRID):
        offset = int(frame * TILE * 2 / (GRID * GRID))
        n = noise[tall - TILE - offset: tall - offset, :]
        sway = 0.08 * np.sin(frame / 16.0 * 2 * np.pi + height * 5.0)
        width = 0.55 * (1.0 - height) ** 0.8 + 0.06
        silhouette = np.clip(1.0 - np.abs(centred - sway) / np.maximum(width, 1e-3), 0, 1)
        base = np.clip(height * 6.0, 0, 1)    # entra suave no pé
        flame = np.clip(silhouette * base * (0.55 + 0.9 * n) - height * 0.55, 0, 1)
        flame = np.clip(flame * 1.8, 0, 1)
        heat = flame ** 0.7
        heat = heat * (1.0 - 0.45 * height)      # a ponta esfria para o vermelho
        r = np.clip(0.75 + 0.25 * heat, 0, 1)
        g = np.clip(0.12 + 0.75 * heat ** 2.2, 0, 1)
        b = np.clip(0.02 + 0.55 * heat ** 5.0, 0, 1)
        a = np.clip(flame * 1.15, 0, 1)
        gy, gx = divmod(frame, GRID)
        tile = sheet[gy * TILE:(gy + 1) * TILE, gx * TILE:(gx + 1) * TILE]
        tile[..., 0], tile[..., 1], tile[..., 2], tile[..., 3] = r, g, b, a
    Image.fromarray((sheet * 255 + 0.5).astype(np.uint8), "RGBA").save(out, optimize=True)
    print(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "examples/particles/fogo-folha-4x4.png")
