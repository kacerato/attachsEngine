#!/usr/bin/env python3
"""Icones no idioma da MARCA Astra.

A marca e duas cores e nada mais: lima sobre quase-preto, formas solidas, sem
contorno e sem gradiente. Quatro tracos dela aparecem em todo desenho daqui:

  - forma SOLIDA, nunca traco fino. A silhueta e o icone;
  - varredura que AFINA ate a ponta, como o anel que abraca a estrela;
  - lado CONCAVO onde uma ponta nasce, como as pontas da estrela;
  - um circulo cheio como acento, quando o desenho pede um centro.

Os icones saem em LIMA sobre transparente. Nos paineis escuros eles aparecem
como sao; na pastilha acesa a tintura da interface multiplica pelo quase-preto e
devolve a relacao da marca invertida -- preto sobre lima. E por isso que este
conjunto nao exige nenhuma mudanca na tela.

Sem brilho externo, de proposito: o atlas tem celula de 96 px e um halo largo
vira sangramento entre celulas vizinhas.

Uso:
    python tools/generate-astra-mark-icons.py
    python tools/pack-icon-atlas.py
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

LIME = (201, 250, 4, 255)
CLEAR = (0, 0, 0, 0)
UNITS = 32
SUPERSAMPLE = 24
OUTPUT = Path("assets/astra-visual/icons/mark-v1")


def bez(p0, p1, p2, n=20):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
            for t in (i / n for i in range(n + 1))]


def arc(cx, cy, r, a0, a1, n=48):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def taper(points, w0, w1):
    """Varredura cuja espessura vai de w0 a w1 -- o gesto do anel da marca."""
    left, right = [], []
    count = max(1, len(points) - 1)
    for index, point in enumerate(points):
        width = (w0 + (w1 - w0) * index / count) / 2
        before = points[max(0, index - 1)]
        after = points[min(len(points) - 1, index + 1)]
        dx, dy = after[0] - before[0], after[1] - before[1]
        length = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / length, dx / length
        left.append((point[0] + nx * width, point[1] + ny * width))
        right.append((point[0] - nx * width, point[1] - ny * width))
    return left + right[::-1]


def bar(a, b, width):
    return taper([a, b], width, width)


def capsule(a, b, width):
    """Barra com as pontas arredondadas."""
    return [("poly", bar(a, b, width)), ("circle", (a, width / 2)), ("circle", (b, width / 2))]


def rounded(x0, y0, x1, y1, r):
    return (bez((x0 + r, y0), (x0, y0), (x0, y0 + r)) + bez((x0, y1 - r), (x0, y1), (x0 + r, y1))
            + bez((x1 - r, y1), (x1, y1), (x1, y1 - r)) + bez((x1, y0 + r), (x1, y0), (x1 - r, y0)))


def ring(cx, cy, outer, inner):
    return [("poly", arc(cx, cy, outer, 0, 360)), ("punch", ((cx, cy), inner))]


def rays(cx, cy, r0, r1, count, w0, w1, phase=0.0):
    shapes = []
    for k in range(count):
        a = math.radians(phase + k * 360 / count)
        shapes.append(("poly", taper([(cx + r0 * math.cos(a), cy + r0 * math.sin(a)),
                                      (cx + r1 * math.cos(a), cy + r1 * math.sin(a))], w0, w1)))
    return shapes


def corners(x0, y0, x1, y1, arm, width):
    """Quatro cantos solidos -- o enquadramento, sem a moldura inteira."""
    shapes = []
    for cx, sx in ((x0, 1), (x1, -1)):
        for cy, sy in ((y0, 1), (y1, -1)):
            shapes += capsule((cx, cy + sy * arm), (cx, cy), width)
            shapes += capsule((cx, cy), (cx + sx * arm, cy), width)
    return shapes


def chevron(cx, cy, size, width, direction=1):
    return [("poly", taper([(cx - direction * size * 0.5, cy - size),
                            (cx + direction * size * 0.5, cy),
                            (cx - direction * size * 0.5, cy + size)], width, width))]


def arrow(tip, back, width, head):
    """Varredura que afina e termina numa ponta cheia."""
    dx, dy = tip[0] - back[0], tip[1] - back[1]
    length = math.hypot(dx, dy) or 1.0
    ux, uy = dx / length, dy / length
    base = (tip[0] - ux * head, tip[1] - uy * head)
    nx, ny = -uy, ux
    return [("poly", taper([back, base], width * 0.55, width)),
            ("poly", [tip, (base[0] + nx * head * 0.72, base[1] + ny * head * 0.72),
                      (base[0] - nx * head * 0.72, base[1] - ny * head * 0.72)])]


def star4(cx, cy, outer, waist, rot=0.0):
    points = []
    for k in range(4):
        a = math.radians(rot + k * 90)
        b = math.radians(rot + k * 90 + 45)
        c = math.radians(rot + (k + 1) * 90)
        points += bez((cx + outer * math.cos(a), cy + outer * math.sin(a)),
                      (cx + waist * math.cos(b), cy + waist * math.sin(b)),
                      (cx + outer * math.cos(c), cy + outer * math.sin(c)), 14)
    return points


def document(x0, y0, x1, y1, fold):
    # O canto dobrado ja esta no contorno. Recortar o triangulo de novo tiraria
    # a dobra duas vezes e deixaria uma mordida no lugar dela.
    return [("poly", [(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)])]


def cube(cx, cy, r):
    return [(cx, cy - r), (cx + r * 0.87, cy - r * 0.5), (cx + r * 0.87, cy + r * 0.5),
            (cx, cy + r), (cx - r * 0.87, cy + r * 0.5), (cx - r * 0.87, cy - r * 0.5)]


def icons() -> dict[str, list]:
    out: dict[str, list] = {}

    out["select"] = [
        ("poly", [(8, 4), (25, 17.5)] + bez((25, 17.5), (18, 18), (16.5, 20.5)) + [(13.5, 28)]
                 + bez((13.5, 28), (9, 18), (8, 4))),
        ("poly", taper(arc(8, 4, 9.5, -58, -30), 2.6, 0.4)),
    ]
    out["move"] = [("circle", ((16, 16), 3.2))] + rays(16, 16, 4.5, 13.5, 4, 6.0, 0.6, -90)
    out["rotate"] = [("poly", taper(arc(16, 16, 10.5, 135, 400), 0.8, 5.2)),
                     ("circle", ((16, 16), 3.0))]
    out["scale"] = [("poly", rounded(4.0, 16.5, 15.5, 28.0, 2.6))] \
        + arrow((28.0, 4.0), (17.0, 15.0), 5.2, 7.2)
    out["object"] = [("poly", cube(16, 16, 12.5)),
                     ("punch_poly", [(16, 9.5), (22, 13), (16, 16.5), (10, 13)])]
    out["camera"] = [("poly", rounded(3.5, 9, 21.5, 25, 3.2)),
                     ("punch", ((12.5, 17), 4.6)),
                     ("circle", ((12.5, 17), 2.2)),
                     ("poly", [(23.5, 13.5), (29, 9.5), (29, 24.5), (23.5, 20.5)])]
    out["frame"] = corners(6, 6, 26, 26, 6.5, 3.4) + [("circle", ((16, 16), 2.6))]
    out["grid"] = [("poly", bar((5, y), (27, y), 3.0)) for y in (9, 16, 23)] \
        + [("poly", bar((x, 5), (x, 27), 3.0)) for x in (9, 16, 23)]
    out["orbit"] = [("circle", ((16, 16), 7.5)),
                    ("poly", taper(arc(16, 16, 11.8, 160, 425), 2.6, 5.4))]
    # Mao aberta com TRES dedos e polegar: quatro dedos a 20 px viram uma mancha,
    # e o gesto que importa e a palma.
    out["pan"] = [("poly", rounded(9.5, 13.0, 22.5, 28.0, 5.5))] \
        + capsule((13.0, 15.0), (13.0, 8.0), 4.4) + capsule((18.0, 15.0), (18.0, 6.5), 4.4) \
        + capsule((10.5, 19.0), (5.0, 15.5), 4.4)
    out["zoom"] = ring(14, 14, 9.5, 5.0) + [("poly", taper([(20.5, 20.5), (28, 28)], 4.6, 3.0))]
    out["eye"] = [("poly", bez((3, 16), (16, 3.5), (29, 16)) + bez((29, 16), (16, 28.5), (3, 16))),
                  ("punch", ((16, 16), 5.2)), ("circle", ((16, 16), 2.4))]
    out["eye-off"] = out["eye"] + [("punch_poly", bar((5.5, 27), (26.5, 5), 5.0)),
                                   ("poly", taper([(6, 26.5), (26, 5.5)], 3.4, 3.4))]
    out["folder"] = [("poly", [(3.5, 9)] + bez((3.5, 9), (3.5, 6.5), (6, 6.5))
                              + [(12, 6.5), (15, 9.5), (26, 9.5)]
                              + bez((26, 9.5), (28.5, 9.5), (28.5, 12)) + [(28.5, 23)]
                              + bez((28.5, 23), (28.5, 25.5), (26, 25.5)) + [(6, 25.5)]
                              + bez((6, 25.5), (3.5, 25.5), (3.5, 23)))]
    out["more"] = [("circle", ((16, y), 2.6)) for y in (7.5, 16, 24.5)]
    out["play"] = [("poly", bez((9, 4.5), (10, 16), (9, 27.5)) + bez((9, 27.5), (18, 22), (27, 16))
                            + bez((27, 16), (18, 10), (9, 4.5)))]
    out["stop"] = [("poly", rounded(7, 7, 25, 25, 4.5))]
    # A marca funciona grande; o icone vive a 20 px. Abaixo de ~3 unidades a
    # varredura some, entao a ponta fina fica so onde ela e o gesto -- nunca
    # onde ela e o desenho inteiro.
    # Meia-volta de espessura CONSTANTE e uma ponta cheia que continua a curva.
    # A ponta que afina e o gesto da marca, mas aqui ela e a informacao: sem
    # ponta, desfazer e refazer viram a mesma meia-lua.
    # Cauda curva e ponta cheia SEPARADA da curva.
    #
    # Com a ponta encostando na meia-volta as duas formas se fundiam num borrao
    # e desfazer ficava igual a refazer. O que distingue os dois e para onde a
    # ponta aponta, entao ela precisa de ar em volta.
    out["undo"] = [("poly", taper(bez((26.5, 25.0), (24.0, 11.0), (13.0, 11.5)), 5.0, 4.2)),
                   ("poly", [(4.5, 11.5), (14.0, 6.0), (14.0, 17.0)])]
    out["redo"] = [("poly", taper(bez((5.5, 25.0), (8.0, 11.0), (19.0, 11.5)), 5.0, 4.2)),
                   ("poly", [(27.5, 11.5), (18.0, 6.0), (18.0, 17.0)])]
    out["settings"] = rays(16, 16, 6.0, 13.0, 8, 6.4, 4.6, 22.5) \
        + [("poly", arc(16, 16, 8.6, 0, 360)), ("punch", ((16, 16), 3.6))]
    out["sun"] = [("circle", ((16, 16), 6.4))] + rays(16, 16, 9.0, 14.5, 8, 3.4, 0.6)
    out["add"] = [("poly", bar((16, 6), (16, 26), 4.4)), ("poly", bar((6, 16), (26, 16), 4.4))]
    out["chevron"] = chevron(16, 16, 6.5, 4.0, -1)

    # Salvar: o corpo do disquete com a etiqueta vazada e o obturador em cima.
    # Salvar e uma SETA para dentro de uma bandeja, e nao um disquete.
    #
    # O disquete precisa de duas janelas vazadas dentro de um corpo de vinte
    # pixels; qualquer uma larga o bastante para se ver deixa trilhos finos nas
    # laterais, e o desenho passa a ler como a letra H. Foi o que aconteceu, e
    # foi visto na barra. A seta sao formas cheias e sobrevive ao tamanho.
    out["save"] = [("poly", bar((16, 4), (16, 15), 5.2)),
                   ("poly", [(16, 23), (7.5, 12.5), (24.5, 12.5)]),
                   ("poly", bar((4.5, 26), (27.5, 26), 4.4)),
                   ("poly", bar((4.5, 19.5), (4.5, 26), 4.4)),
                   ("poly", bar((27.5, 19.5), (27.5, 26), 4.4))]
    # Buscar: o anel e o cabo, o mesmo gesto da lupa da barra de camera.
    out["search"] = ring(14, 14, 9.5, 5.0) + [("poly", taper([(20.5, 20.5), (28, 28)], 4.6, 3.0))]
    out["component-add"] = [("poly", rounded(4, 4, 28, 28, 6.0)),
                            ("punch_poly", bar((16, 9.5), (16, 22.5), 4.0)),
                            ("punch_poly", bar((9.5, 16), (22.5, 16), 4.0))]
    out["character"] = [("circle", ((16, 7.0), 4.2)),
                        ("poly", taper([(16, 11.5), (16, 20.5)], 7.0, 4.4))] \
        + capsule((16, 20), (10.5, 28), 3.4) + capsule((16, 20), (21.5, 28), 3.4) \
        + capsule((16, 13.5), (7.5, 17.5), 3.2) + capsule((16, 13.5), (24.5, 17.5), 3.2)
    out["collider"] = ring(16, 16, 11.0, 6.6) \
        + [("circle", (p, 2.6)) for p in ((16, 5), (16, 27), (5, 16), (27, 16))]
    out["joint"] = [("circle", ((8.5, 23.5), 5.2)), ("circle", ((23.5, 8.5), 5.2)),
                    ("poly", bar((11.5, 20.5), (20.5, 11.5), 4.0))]
    out["look"] = ring(16, 16, 10.0, 6.0) + [("circle", ((16, 16), 2.8))] \
        + rays(16, 16, 11.5, 15.0, 4, 3.2, 2.4, -90)
    out["physics"] = [("poly", cube(16, 16, 12.0)), ("punch", ((16, 16), 5.6)),
                      ("circle", ((16, 16), 3.2))]
    out["code"] = [("poly", taper([(12.5, 6.5), (4.5, 16), (12.5, 25.5)], 3.8, 3.8)),
                   ("poly", taper([(19.5, 6.5), (27.5, 16), (19.5, 25.5)], 3.8, 3.8))]
    # Cupula em cima e a luz se abrindo para baixo. Com o cone apontando para
    # cima o desenho virava uma seta, que e outra coisa.
    out["light"] = [("poly", bar((16, 3), (16, 8), 2.6)),
                    ("poly", [(7.0, 17.5), (25.0, 17.5), (20.0, 7.5), (12.0, 7.5)]),
                    ("poly", taper([(11.0, 21), (9.0, 26)], 3.0, 1.0)),
                    ("poly", taper([(16.0, 21.5), (16.0, 27.5)], 3.4, 1.0)),
                    ("poly", taper([(21.0, 21), (23.0, 26)], 3.0, 1.0))]
    out["file"] = document(7, 4, 25, 28, 6.5)
    out["particles"] = [("circle", ((11, 20), 4.6)), ("circle", ((21.5, 11.5), 3.2)),
                        ("circle", ((23, 22), 2.2)), ("poly", star4(11, 9, 5.2, 1.6, -90))]
    out["water-surface"] = [("poly", taper(bez((3.5, y), (11, y - 5), (16, y))
                                           + bez((16, y), (21, y + 5), (28.5, y)), 3.8, 3.8))
                            for y in (10.5, 17, 23.5)]
    out["water-physics"] = [("poly", taper(bez((3.5, y), (11, y - 4.5), (16, y))
                                           + bez((16, y), (21, y + 4.5), (28.5, y)), 3.6, 3.6))
                            for y in (19, 25)] + [("circle", ((16, 9.5), 5.4))]
    out["water-layers"] = [("poly", rounded(4.5, y, 27.5, y + 5.0, 2.2)) for y in (5.5, 13.5, 21.5)]
    out["water-route"] = [("poly", taper(bez((5, 25), (10, 6), (16, 16))
                                         + bez((16, 16), (22, 26), (27, 7)), 3.6, 3.6)),
                          ("circle", ((5, 25), 3.6)), ("circle", ((27, 7), 3.6))]
    return out


def render(shapes, size=512) -> Image.Image:
    canvas = Image.new("RGBA", (UNITS * SUPERSAMPLE, UNITS * SUPERSAMPLE), CLEAR)
    pen = ImageDraw.Draw(canvas)

    def scale(point):
        return (point[0] * SUPERSAMPLE, point[1] * SUPERSAMPLE)

    for kind, data in shapes:
        if kind == "poly":
            pen.polygon([scale(p) for p in data], fill=LIME)
        elif kind == "punch_poly":
            pen.polygon([scale(p) for p in data], fill=CLEAR)
        elif kind == "circle":
            (cx, cy), r = data
            pen.ellipse([scale((cx - r, cy - r)), scale((cx + r, cy + r))], fill=LIME)
        elif kind == "punch":
            (cx, cy), r = data
            pen.ellipse([scale((cx - r, cy - r)), scale((cx + r, cy + r))], fill=CLEAR)
    return canvas.resize((size, size), Image.LANCZOS)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    drawings = icons()
    for name, shapes in drawings.items():
        render(shapes).save(OUTPUT / f"{name}.png")
    print(f"{len(drawings)} icones em {OUTPUT}")


if __name__ == "__main__":
    main()
