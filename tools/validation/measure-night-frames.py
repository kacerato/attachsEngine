"""V-2: mede a noite de frames da Unity e de capturas do POCO do mesmo jeito.

Uso: py tools/validation/measure-night-frames.py saida.json imagem[:x0,y0,x1,y1] ...

O retângulo opcional recorta o viewport (vídeos com o editor da Unity, capturas
do editor da Astra). Para cada imagem e recorte:
  - luminância relativa média (sRGB linearizado, Rec. 709) e o valor sRGB 0-255
    equivalente, que é o "brilho médio" percebido na tela;
  - céu = terço de cima, chão = terço de baixo, com média e cor sRGB média;
  - sombra = média dos 10% mais escuros; brilho = percentil 99;
  - azul/vermelho da média (> 1 é noite fria).
"""
import json
import sys

import numpy as np
from PIL import Image


def linear(srgb):
    c = srgb / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def to_srgb8(value):
    v = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
    return round(float(v) * 255.0, 1)


def region(pixels):
    lin = linear(pixels.astype(np.float64))
    lum = lin @ np.array([0.2126, 0.7152, 0.0722])
    mean_rgb = lin.reshape(-1, 3).mean(axis=0)
    flat = np.sort(lum.reshape(-1))
    darkest = flat[: max(1, flat.size // 10)].mean()
    return {
        "luminancia": round(float(lum.mean()), 5),
        "srgb_medio": to_srgb8(lum.mean()),
        "cor_srgb": [to_srgb8(c) for c in mean_rgb],
        "azul_vermelho": round(float(mean_rgb[2] / max(mean_rgb[0], 1e-6)), 2),
        "sombra_srgb": to_srgb8(darkest),
        "p99_srgb": to_srgb8(flat[int(flat.size * 0.99)]),
    }


def measure(spec):
    path, _, box = spec.partition(":")
    image = np.asarray(Image.open(path).convert("RGB"))
    if box:
        x0, y0, x1, y1 = (int(v) for v in box.split(","))
        image = image[y0:y1, x0:x1]
    third = image.shape[0] // 3
    return {
        "arquivo": path.replace("\\", "/").split("/")[-1],
        "recorte": box or "inteiro",
        "quadro": region(image),
        "ceu": region(image[:third]),
        "chao": region(image[-third:]),
    }


def main():
    out, specs = sys.argv[1], sys.argv[2:]
    rows = [measure(spec) for spec in specs]
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(rows, f, ensure_ascii=False, indent=2)
        f.write("\n")
    for row in rows:
        q, c, g = row["quadro"], row["ceu"], row["chao"]
        print(f'{row["arquivo"]:<60} quadro {q["srgb_medio"]:>5} céu {c["srgb_medio"]:>5} '
              f'chão {g["srgb_medio"]:>5} sombra {q["sombra_srgb"]:>5} p99 {q["p99_srgb"]:>5} '
              f'B/R {q["azul_vermelho"]:>4}')


if __name__ == "__main__":
    main()
