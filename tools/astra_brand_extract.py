"""Fatia os masters da marca ASTRA em ativos utilizáveis pelo shell.

As imagens de origem chegam com alfa real, então o recorte é feito por
componentes conexos do canal alfa em vez de coordenadas fixas: se o master for
regerado com outro enquadramento, os recortes continuam corretos.

    python tools/astra_brand_extract.py
"""
from __future__ import annotations

import json
import pathlib
import sys
from collections import deque

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[1]
VISUAL = ROOT / "assets" / "astra-visual"
ALPHA_FLOOR = 24          # abaixo disso o pixel é fundo, não desenho
GAP = 14                  # folga em px que ainda une duas partes do mesmo ícone


def components(alpha, width, height, step):
    """Rótulos de componentes conexos sobre uma grade reduzida por `step`."""
    gw, gh = width // step, height // step
    solid = bytearray(gw * gh)
    for gy in range(gh):
        for gx in range(gw):
            px, py = gx * step, gy * step
            if alpha[py * width + px] >= ALPHA_FLOOR:
                solid[gy * gw + gx] = 1
    seen = bytearray(gw * gh)
    boxes = []
    for start in range(gw * gh):
        if not solid[start] or seen[start]:
            continue
        seen[start] = 1
        queue = deque([start])
        x0 = x1 = start % gw
        y0 = y1 = start // gw
        while queue:
            cell = queue.popleft()
            cx, cy = cell % gw, cell // gw
            x0, x1 = min(x0, cx), max(x1, cx)
            y0, y1 = min(y0, cy), max(y1, cy)
            for dx in range(-1, 2):
                for dy in range(-1, 2):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < gw and 0 <= ny < gh:
                        n = ny * gw + nx
                        if solid[n] and not seen[n]:
                            seen[n] = 1
                            queue.append(n)
        boxes.append((x0 * step, y0 * step, (x1 + 1) * step, (y1 + 1) * step))
    return boxes


def merge(boxes, gap):
    """Une caixas que se tocam dentro de `gap` — partes do mesmo desenho."""
    merged = list(boxes)
    changed = True
    while changed:
        changed = False
        for i in range(len(merged)):
            for j in range(i + 1, len(merged)):
                a, b = merged[i], merged[j]
                if (a[0] - gap < b[2] and b[0] - gap < a[2]
                        and a[1] - gap < b[3] and b[1] - gap < a[3]):
                    merged[i] = (min(a[0], b[0]), min(a[1], b[1]),
                                 max(a[2], b[2]), max(a[3], b[3]))
                    del merged[j]
                    changed = True
                    break
            if changed:
                break
    return merged


def clean(image):
    """Zera o halo semitransparente do master.

    O PNG de origem traz uma névoa de alfa baixo por todo o quadro; sem
    limpá-la, `getbbox` devolve a imagem inteira e todo recorte sai errado.
    """
    alpha = image.split()[3].point(lambda v: v if v >= ALPHA_FLOOR else 0)
    image.putalpha(alpha)
    return image


def trim(image):
    box = clean(image.copy()).getbbox()
    return image.crop(box) if box else image


def write(image, path, widths):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    for width in widths:
        height = max(1, round(image.height * width / image.width))
        sized = image.resize((width, height), Image.LANCZOS)
        sized.save(path.with_name(f"{path.stem}-{width}{path.suffix}"))


def green_column_runs(image, floor=6):
    """Colunas ocupadas pelo verde de marca — separa o mark do wordmark.

    O contorno preto encosta o quadrado no "A", então não há coluna vazia para
    cortar. O bloco verde da esquerda é o único marco estável do desenho.
    """
    pixels = image.load()
    runs, start = [], None
    for x in range(image.width):
        hit = 0
        for y in range(0, image.height, 3):
            r, g, b, a = pixels[x, y]
            if a > 128 and g > 200 and r > 150 and b < 120 and g - b > 90:
                hit += 1
        if hit and start is None:
            start = x
        elif not hit and start is not None:
            if x - start > floor:
                runs.append((start, x))
            start = None
    if start is not None:
        runs.append((start, image.width))
    return runs


def split_lockup(report):
    master = clean(Image.open(VISUAL / "brand" / "astra-lockup-master.png").convert("RGBA"))
    lockup = trim(master)
    write(lockup, VISUAL / "brand" / "astra-lockup.png", [512, 1024])

    runs = green_column_runs(lockup)
    if not runs:
        raise SystemExit("lockup sem bloco verde: master fora do padrão")
    # A borda preta do quadrado vive fora do verde e encosta no "A". Corta-se
    # na coluna mais vazia logo à direita do verde: é a garganta entre os dois.
    pixels = lockup.load()
    lo = runs[0][1]
    hi = min(lockup.width, lo + round(lockup.height * 0.30))
    def density(x):
        return sum(1 for y in range(lockup.height) if pixels[x, y][3] >= ALPHA_FLOOR)
    cut = min(range(lo, hi), key=density) + 1

    mark = trim(lockup.crop((0, 0, cut, lockup.height)))
    write(mark, VISUAL / "brand" / "astra-mark.png", [48, 64, 96, 128, 192, 256, 512])

    wordmark = trim(lockup.crop((cut, 0, lockup.width, lockup.height)))
    write(wordmark, VISUAL / "brand" / "astra-wordmark.png", [256, 512, 1024])

    report["lockup"] = {"size": list(lockup.size)}
    report["mark"] = {"size": list(mark.size), "cut": cut}
    report["wordmark"] = {"size": list(wordmark.size)}


# A folha chega em três fileiras; os nomes seguem a leitura visual, esquerda
# para direita, e viram as chaves usadas pelo shell.
ICON_NAMES = [
    "project-new", "project-open", "project-import",
    "history-revert", "search", "settings",
    "asset-mesh", "play", "capture-export", "back",
]


def split_icons(report):
    sheet = clean(Image.open(VISUAL / "icons" / "source" / "astra-icon-sheet.png").convert("RGBA"))
    alpha = sheet.split()[3].tobytes()
    boxes = merge(components(alpha, sheet.width, sheet.height, 3), 20)
    boxes = [b for b in boxes if (b[2] - b[0]) > 60 and (b[3] - b[1]) > 60]
    rows: list[list[tuple[int, int, int, int]]] = []
    for box in sorted(boxes, key=lambda b: b[1]):
        placed = False
        for row in rows:
            if abs(row[0][1] - box[1]) < (box[3] - box[1]) * 0.6:
                row.append(box)
                placed = True
                break
        if not placed:
            rows.append([box])
    ordered = [b for row in rows for b in sorted(row, key=lambda b: b[0])]
    report["icons"] = {"found": len(ordered), "named": len(ICON_NAMES)}
    catalog = {}
    for index, box in enumerate(ordered):
        name = ICON_NAMES[index] if index < len(ICON_NAMES) else f"icon-{index:02d}"
        icon = trim(sheet.crop(box))
        side = max(icon.size)
        square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        square.paste(icon, ((side - icon.width) // 2, (side - icon.height) // 2))
        write(square, VISUAL / "icons" / "png" / f"{name}.png", [24, 32, 48, 64, 96])
        catalog[name] = {"source": list(box), "square": side}
    (VISUAL / "icons" / "catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n", encoding="utf-8")


def split_glyph(report):
    """Isola o cometa de dentro do quadrado: é a silhueta de fundo das telas.

    O desenho e a moldura do quadrado são ambos pretos. A moldura toca a borda
    da imagem, então um preenchimento a partir das bordas a remove e sobra só
    o glifo — sem depender de traçar o contorno à mão.
    """
    mark = Image.open(VISUAL / "brand" / "astra-mark.png").convert("RGBA")
    w, h = mark.size
    pixels = mark.load()
    dark = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            dark[y * w + x] = 1 if (a >= 128 and max(r, g, b) < 110) else 0

    outside = bytearray(w * h)
    queue = deque()
    for x in range(w):
        for y in (0, h - 1):
            if dark[y * w + x] and not outside[y * w + x]:
                outside[y * w + x] = 1
                queue.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if dark[y * w + x] and not outside[y * w + x]:
                outside[y * w + x] = 1
                queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h:
                i = ny * w + nx
                if dark[i] and not outside[i]:
                    outside[i] = 1
                    queue.append((nx, ny))

    # Branco puro com alfa: a tela tinge o glifo no tom que precisar.
    glyph = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    out = glyph.load()
    kept = 0
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if dark[i] and not outside[i]:
                out[x, y] = (255, 255, 255, 255)
                kept += 1
    glyph = trim(glyph)
    write(glyph, VISUAL / "brand" / "astra-glyph.png", [256, 512, 1024])
    report["glyph"] = {"size": list(glyph.size), "pixels": kept}


# Os masters de tela trazem o rotulo "ASTRA PROJECT" queimado sobre a foto, e o
# shell desenha esse rotulo por conta propria. Em vez de apagar a faixa — o que
# deixa emenda visivel em ceu liso — a capa e recortada abaixo dela: o card
# preenche por cobertura, entao a perda de altura nao aparece.
KICKER_HEIGHT = 56


def cut_samples(report):
    """Recorta as capas das cenas dos masters, sem o rotulo queimado."""
    out = VISUAL / "samples"
    out.mkdir(parents=True, exist_ok=True)
    projects = Image.open(VISUAL / "reference" / "screen-03-projects.png").convert("RGB")
    names = ["forest-test", "water-lab", "backroom-demo", "vehicle-sandbox"]
    for index, name in enumerate(names):
        x = 295 + index * 339
        projects.crop((x, 344 + KICKER_HEIGHT, x + 317, 628)).save(out / f"thumb-{name}.png")
    # A tela de carregamento nunca teve rotulo, entao a capa larga sai inteira.
    loading = Image.open(VISUAL / "reference" / "screen-02-project-loading.png").convert("RGB")
    loading.crop((407, 332, 407 + 440, 332 + 296)).save(out / "thumb-backroom-demo-wide.png")
    report["samples"] = names


def main():
    report = {}
    split_lockup(report)
    split_glyph(report)
    cut_samples(report)
    split_icons(report)
    json.dump(report, sys.stdout, indent=2)
    print()


if __name__ == "__main__":
    main()
