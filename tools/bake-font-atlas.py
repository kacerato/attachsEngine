#!/usr/bin/env python3
"""Assa o atlas SDF da interface a partir do Inter Variable.

Por que SDF e não bitmap por tamanho: a interface do editor usa rótulo de 11 px,
campo de 14, corpo de 15, nome de 17 e ainda precisa crescer com a escala de
acessibilidade e com a densidade do aparelho. Um atlas por tamanho seria uma
tabela nova a cada corpo novo; um campo de distância é assado uma vez e o shader
resolve qualquer corpo com a mesma amostra.

Por que um único `opsz`: o Inter tem eixo de tamanho óptico (14–32) e o atlas é
size-agnostic, então é preciso escolher. Escolhido 14, que é o desenho para
texto pequeno, porque TODO texto real da interface é pequeno — o título grande
dos mockups é a marca ASTRA, que é imagem, não texto.

O campo de distância é exato, não aproximado: a transformada usa o algoritmo de
Felzenszwalb–Huttenlocher, que dá distância euclidiana quadrática verdadeira em
tempo linear. Uma aproximação por chanfro erra nas diagonais, e diagonal é
justamente o que mais aparece em "A", "V", "x" e "/".

**As métricas saem relativas à LINHA DE BASE.** O Pillow trabalha com origem no
topo do ascendente, que é conveniente para desenhar uma caixa e inútil para
alinhar dois corpos diferentes na mesma linha — que é o que o Inspector faz o
tempo todo, com rótulo de 11 e valor de 14 lado a lado. A conversão acontece aqui,
uma vez, e não em cada consumidor.

Saída: o binário `.aeuf` que a engine lê, mais um PNG e um JSON de inspeção.

Uso:
    python tools/bake-font-atlas.py
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from ui_asset_format import write_font

FIRST_GLYPH = 32
LAST_GLYPH = 126
GLYPH_COUNT = LAST_GLYPH - FIRST_GLYPH + 1
# Corpo em que o glifo é rasterizado antes de virar distância. Não é o corpo em
# que ele será desenhado: é a resolução do campo.
EM_PIXELS = 40
# Alcance do campo em pixels do atlas. O shader precisa de alguns texels de cada
# lado da borda para interpolar; muito pouco serrilha, muito desperdiça área.
SPREAD_PIXELS = 5.0
# Supersampling da rasterização antes da transformada. Sem ele a borda entra no
# campo já quantizada em pixels inteiros e o texto fica "gordo" em corpos altos.
SUPERSAMPLE = 4
ATLAS_PADDING = 1
# A ordem é o índice que o runtime usa. Mudá-la muda o significado do binário.
WEIGHT_ORDER = (("regular", 400), ("medium", 500), ("semibold", 600))
OPTICAL_SIZE = 14


def squared_distance_1d(row: np.ndarray) -> np.ndarray:
    """Transformada de distância 1D exata (Felzenszwalb–Huttenlocher)."""
    length = row.shape[0]
    output = np.empty(length, dtype=np.float64)
    vertices = np.zeros(length, dtype=np.int32)
    boundaries = np.empty(length + 1, dtype=np.float64)
    rightmost = 0
    boundaries[0] = -np.inf
    boundaries[1] = np.inf
    for q in range(1, length):
        while True:
            p = vertices[rightmost]
            intersection = ((row[q] + q * q) - (row[p] + p * p)) / (2.0 * q - 2.0 * p)
            if intersection <= boundaries[rightmost] and rightmost > 0:
                rightmost -= 1
            else:
                break
        rightmost += 1
        vertices[rightmost] = q
        boundaries[rightmost] = intersection
        boundaries[rightmost + 1] = np.inf
    rightmost = 0
    for q in range(length):
        while boundaries[rightmost + 1] < q:
            rightmost += 1
        p = vertices[rightmost]
        output[q] = (q - p) * (q - p) + row[p]
    return output


def squared_distance_transform(mask: np.ndarray) -> np.ndarray:
    """Distância euclidiana quadrática até o pixel True mais próximo."""
    field = np.where(mask, 0.0, 1e20)
    for y in range(field.shape[0]):
        field[y, :] = squared_distance_1d(field[y, :])
    for x in range(field.shape[1]):
        field[:, x] = squared_distance_1d(field[:, x])
    return field


def signed_field(coverage: np.ndarray, spread: float) -> np.ndarray:
    """Campo com 0.5 na borda, crescendo para dentro do traço.

    Meio texel de deslocamento em cada lado: a borda real fica ENTRE o último
    pixel de dentro e o primeiro de fora, e ignorar isso engorda o traço em um
    pixel inteiro no corpo mais alto.
    """
    inside = coverage >= 0.5
    if not inside.any():
        return np.zeros_like(coverage)
    outside_distance = np.sqrt(squared_distance_transform(inside))
    inside_distance = np.sqrt(squared_distance_transform(~inside))
    signed = np.where(inside, inside_distance - 0.5, -(outside_distance - 0.5))
    return np.clip(signed / (2.0 * spread) + 0.5, 0.0, 1.0)


def load_face(path: Path, weight: int) -> ImageFont.FreeTypeFont:
    face = ImageFont.truetype(str(path), EM_PIXELS * SUPERSAMPLE)
    face.set_variation_by_axes([float(OPTICAL_SIZE), float(weight)])
    return face


def render_glyph(face: ImageFont.FreeTypeFont, character: str):
    """Cobertura em cinza mais o canto do desenho, em pixels de supersampling."""
    box = face.getbbox(character)
    if box is None:
        return None
    left, top, right, bottom = box
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        return None
    margin = int(SPREAD_PIXELS) * SUPERSAMPLE
    canvas = Image.new("L", (width + margin * 2, height + margin * 2), 0)
    ImageDraw.Draw(canvas).text((margin - left, margin - top), character, fill=255, font=face)
    return canvas, left - margin, top - margin


def reduce_to_field(canvas: Image.Image) -> np.ndarray:
    """Reduz e SÓ ENTÃO mede a distância.

    A ordem importa: medir na resolução do supersampling e reamostrar o campo
    depois borraria a borda com o filtro de redução, que é exatamente a
    informação que o campo existe para preservar.
    """
    reduced = canvas.resize(
        (max(1, canvas.width // SUPERSAMPLE), max(1, canvas.height // SUPERSAMPLE)),
        Image.LANCZOS,
    )
    coverage = np.asarray(reduced, dtype=np.float64) / 255.0
    return signed_field(coverage, SPREAD_PIXELS)


def bake(font_path: Path, output_directory: Path, name: str) -> None:
    em = float(EM_PIXELS * SUPERSAMPLE)
    tiles: list[np.ndarray] = []
    weights: list[dict] = []

    for weight_name, weight_value in WEIGHT_ORDER:
        face = load_face(font_path, weight_value)
        ascent_pixels, descent_pixels = face.getmetrics()
        ascent = ascent_pixels / em
        # Altura da caixa alta medida no "H" real da fonte, não presumida. Os
        # masters do ASTRA foram medidos por ela, e uma constante chutada aqui
        # deslocaria todo rótulo em relação ao mockup.
        cap_box = face.getbbox("H")
        cap_height = ascent - (cap_box[1] / em) if cap_box else 0.72

        glyphs: list[dict] = []
        for code in range(FIRST_GLYPH, LAST_GLYPH + 1):
            character = chr(code)
            record = {
                "advance": face.getlength(character) / em,
                "bearing": [0.0, 0.0],
                "size": [0.0, 0.0],
                "atlas": (0, 0, 0, 0),
            }
            rendered = render_glyph(face, character)
            if rendered is not None:
                canvas, offset_x, offset_y = rendered
                field = reduce_to_field(canvas)
                tiles.append(((field * 255.0).astype(np.uint8), record))
                record["bearing"] = [
                    offset_x / em,
                    # Do topo do ascendente para a linha de base: negativo é
                    # acima dela, que é onde quase todo glifo começa.
                    offset_y / em - ascent,
                ]
                record["size"] = [
                    field.shape[1] * SUPERSAMPLE / em,
                    field.shape[0] * SUPERSAMPLE / em,
                ]
            glyphs.append(record)

        weights.append(
            {
                "name": weight_name,
                "ascent": ascent,
                "descent": descent_pixels / em,
                "capHeight": cap_height,
                "glyphs": glyphs,
            }
        )

    # Empacotamento por prateleiras, alto primeiro. Não é ótimo e não precisa
    # ser: são poucas centenas de glifos assados uma vez, e uma prateleira
    # simples já passa de 60% com a largura escolhida perto da raiz da área.
    order = sorted(range(len(tiles)), key=lambda index: -tiles[index][0].shape[0])
    total_area = sum(tile.shape[0] * tile.shape[1] for tile, _ in tiles)
    width = 1
    while width * width < total_area * 1.35:
        width *= 2
    cursor_x, cursor_y, shelf_height = 0, 0, 0
    placements: list[tuple[int, int]] = [(0, 0)] * len(tiles)
    for index in order:
        tile = tiles[index][0]
        if cursor_x + tile.shape[1] + ATLAS_PADDING > width:
            cursor_x = 0
            cursor_y += shelf_height
            shelf_height = 0
        placements[index] = (cursor_x, cursor_y)
        cursor_x += tile.shape[1] + ATLAS_PADDING
        shelf_height = max(shelf_height, tile.shape[0] + ATLAS_PADDING)
    height = 1
    while height < cursor_y + shelf_height:
        height *= 2

    atlas = np.zeros((height, width), dtype=np.uint8)
    for index, (tile, record) in enumerate(tiles):
        x, y = placements[index]
        atlas[y : y + tile.shape[0], x : x + tile.shape[1]] = tile
        record["atlas"] = (x, y, tile.shape[1], tile.shape[0])

    output_directory.mkdir(parents=True, exist_ok=True)
    Image.fromarray(atlas, "L").save(output_directory / f"{name}.png")
    binary_size = write_font(
        output_directory / f"{name}.aeuf",
        (width, height),
        EM_PIXELS,
        SPREAD_PIXELS,
        FIRST_GLYPH,
        GLYPH_COUNT,
        weights,
        atlas.tobytes(),
    )
    (output_directory / f"{name}.json").write_text(
        json.dumps(
            {
                "atlas": [width, height],
                "emPixels": EM_PIXELS,
                "spreadPixels": SPREAD_PIXELS,
                "opticalSize": OPTICAL_SIZE,
                "font": "Inter Variable (SIL OFL 1.1)",
                "firstGlyph": FIRST_GLYPH,
                "glyphCount": GLYPH_COUNT,
                "weights": [
                    {
                        "name": weight["name"],
                        "ascent": weight["ascent"],
                        "descent": weight["descent"],
                        "capHeight": weight["capHeight"],
                    }
                    for weight in weights
                ],
            },
            indent=1,
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    occupancy = total_area / float(width * height)
    print(
        f"atlas {width}x{height}, {len(tiles)} glifos, ocupacao {occupancy * 100:.1f}%, "
        f"binario {binary_size / 1024:.0f} KiB"
    )
    for weight in weights:
        print(
            f"  {weight['name']}: ascent {weight['ascent']:.3f} descent "
            f"{weight['descent']:.3f} capHeight {weight['capHeight']:.3f}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    fonts = Path("assets/astra-visual/fonts")
    parser.add_argument("--font", type=Path, default=fonts / "Inter-Variable.woff2")
    parser.add_argument("--output", type=Path, default=Path("assets/astra-visual/ui"))
    parser.add_argument("--name", default="astra-ui-font")
    arguments = parser.parse_args()
    bake(arguments.font, arguments.output, arguments.name)


if __name__ == "__main__":
    main()
