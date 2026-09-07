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

Saída: um PNG de canal único e um JSON com as métricas por peso e por glifo, em
unidades de em — o mesmo espaço que ui/ui_text.h consome.

Uso:
    python tools/bake-font-atlas.py
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FIRST_GLYPH = 32
LAST_GLYPH = 126
# Corpo em que o glifo é rasterizado antes de virar distância. Não é o corpo em
# que ele será desenhado: é a resolução do campo.
EM_PIXELS = 40
# Alcance do campo em pixels do atlas. O shader precisa de alguns texels de cada
# lado da borda para interpolar; muito pouco serrilha, muito desperdiça área.
SPREAD_PIXELS = 5
# Supersampling da rasterização antes da transformada. Sem ele a borda entra no
# campo já quantizada em pixels inteiros e o texto fica "gordo" em corpos altos.
SUPERSAMPLE = 4
ATLAS_PADDING = 1
WEIGHTS = {"regular": 400, "medium": 500, "semibold": 600}
OPTICAL_SIZE = 14


def squared_distance_1d(row: np.ndarray) -> np.ndarray:
    """Transformada de distância 1D exata (Felzenszwalb–Huttenlocher)."""
    length = row.shape[0]
    output = np.empty(length, dtype=np.float64)
    vertices = np.zeros(length, dtype=np.int32)
    boundaries = np.empty(length + 1, dtype=np.float64)
    rightmost = 0
    vertices[0] = 0
    boundaries[0] = -np.inf
    boundaries[1] = np.inf
    for q in range(1, length):
        while True:
            p = vertices[rightmost]
            intersection = ((row[q] + q * q) - (row[p] + p * p)) / (2.0 * q - 2.0 * p)
            if intersection <= boundaries[rightmost]:
                rightmost -= 1
                if rightmost < 0:
                    rightmost = 0
                    break
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
    infinity = 1e20
    field = np.where(mask, 0.0, infinity)
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
    outside_distance = np.sqrt(squared_distance_transform(inside))
    inside_distance = np.sqrt(squared_distance_transform(~inside))
    signed = np.where(inside, inside_distance - 0.5, -(outside_distance - 0.5))
    return np.clip(signed / (2.0 * spread) + 0.5, 0.0, 1.0)


def load_face(path: Path, weight: int) -> ImageFont.FreeTypeFont:
    face = ImageFont.truetype(str(path), EM_PIXELS * SUPERSAMPLE)
    # Eixos na ordem que a fonte declara: tamanho óptico e depois peso.
    face.set_variation_by_axes([float(OPTICAL_SIZE), float(weight)])
    return face


def render_glyph(face: ImageFont.FreeTypeFont, character: str):
    """Cobertura em escala de cinza mais a posição do desenho na caixa."""
    box = face.getbbox(character)
    if box is None:
        return None
    left, top, right, bottom = box
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        return None
    margin = SPREAD_PIXELS * SUPERSAMPLE
    canvas = Image.new("L", (width + margin * 2, height + margin * 2), 0)
    ImageDraw.Draw(canvas).text((margin - left, margin - top), character, fill=255, font=face)
    return canvas, left - margin, top - margin


def bake(font_path: Path, output_directory: Path, atlas_name: str) -> dict:
    glyphs: list[dict] = []
    tiles: list[np.ndarray] = []

    for weight_name, weight_value in WEIGHTS.items():
        face = load_face(font_path, weight_value)
        ascent, descent = face.getmetrics()
        em = float(EM_PIXELS * SUPERSAMPLE)
        for code in range(FIRST_GLYPH, LAST_GLYPH + 1):
            character = chr(code)
            advance = face.getlength(character) / em
            rendered = render_glyph(face, character)
            if rendered is None:
                # Espaço e afins: sem desenho, só avanço. Continuam no JSON
                # porque medir uma frase sem eles daria largura errada.
                glyphs.append(
                    {
                        "weight": weight_name,
                        "code": code,
                        "advance": advance,
                        "rect": None,
                    }
                )
                continue
            canvas, offset_x, offset_y = rendered
            coverage = np.asarray(canvas, dtype=np.float64) / 255.0
            # A redução acontece ANTES da transformada: o campo é medido na
            # resolução final, e não reamostrado depois, o que borraria a borda.
            reduced = np.asarray(
                Image.fromarray((coverage * 255).astype(np.uint8), "L").resize(
                    (max(1, canvas.width // SUPERSAMPLE), max(1, canvas.height // SUPERSAMPLE)),
                    Image.LANCZOS,
                ),
                dtype=np.float64,
            ) / 255.0
            field = signed_field(reduced, SPREAD_PIXELS)
            tiles.append((field * 255.0).astype(np.uint8))
            glyphs.append(
                {
                    "weight": weight_name,
                    "code": code,
                    "advance": advance,
                    "rect": len(tiles) - 1,
                    "bearing": [offset_x / em, offset_y / em],
                    "size": [
                        field.shape[1] * SUPERSAMPLE / em,
                        field.shape[0] * SUPERSAMPLE / em,
                    ],
                }
            )
        metrics = {
            "ascent": ascent / em,
            "descent": descent / em,
        }
        for glyph in glyphs:
            if glyph["weight"] == weight_name:
                glyph.setdefault("_metrics", metrics)

    # Empacotamento por prateleiras, alto primeiro. Não é ótimo e não precisa
    # ser: são 285 glifos assados uma vez, e uma prateleira simples já fica
    # acima de 80% de ocupação com uma largura escolhida perto da raiz da área.
    order = sorted(range(len(tiles)), key=lambda index: -tiles[index].shape[0])
    total_area = sum(tile.shape[0] * tile.shape[1] for tile in tiles)
    width = 1
    while width * width < total_area * 1.35:
        width *= 2
    positions: dict[int, tuple[int, int]] = {}
    cursor_x, cursor_y, shelf_height = 0, 0, 0
    for index in order:
        tile = tiles[index]
        tile_width = tile.shape[1] + ATLAS_PADDING
        tile_height = tile.shape[0] + ATLAS_PADDING
        if cursor_x + tile_width > width:
            cursor_x = 0
            cursor_y += shelf_height
            shelf_height = 0
        positions[index] = (cursor_x, cursor_y)
        cursor_x += tile_width
        shelf_height = max(shelf_height, tile_height)
    height = 1
    while height < cursor_y + shelf_height:
        height *= 2

    atlas = np.zeros((height, width), dtype=np.uint8)
    for index, (x, y) in positions.items():
        tile = tiles[index]
        atlas[y : y + tile.shape[0], x : x + tile.shape[1]] = tile

    output_directory.mkdir(parents=True, exist_ok=True)
    Image.fromarray(atlas, "L").save(output_directory / f"{atlas_name}.png")

    catalogue = {
        "atlas": f"{atlas_name}.png",
        "size": [width, height],
        "emPixels": EM_PIXELS,
        "spreadPixels": SPREAD_PIXELS,
        "opticalSize": OPTICAL_SIZE,
        "font": "Inter Variable (SIL OFL 1.1)",
        "firstGlyph": FIRST_GLYPH,
        "lastGlyph": LAST_GLYPH,
        "weights": {},
    }
    for weight_name in WEIGHTS:
        entries = [glyph for glyph in glyphs if glyph["weight"] == weight_name]
        metrics = next((entry["_metrics"] for entry in entries if "_metrics" in entry), {})
        catalogue["weights"][weight_name] = {
            "ascent": metrics.get("ascent", 0.0),
            "descent": metrics.get("descent", 0.0),
            "glyphs": {},
        }
        for entry in entries:
            record = {"advance": entry["advance"]}
            if entry["rect"] is not None:
                x, y = positions[entry["rect"]]
                tile = tiles[entry["rect"]]
                record["atlas"] = [x, y, int(tile.shape[1]), int(tile.shape[0])]
                record["bearing"] = entry["bearing"]
                record["size"] = entry["size"]
            catalogue["weights"][weight_name]["glyphs"][str(entry["code"])] = record

    (output_directory / f"{atlas_name}.json").write_text(
        json.dumps(catalogue, indent=1, ensure_ascii=False), encoding="utf-8"
    )
    occupancy = total_area / float(width * height)
    print(f"atlas {width}x{height}, {len(tiles)} glifos, ocupacao {occupancy * 100:.1f}%")
    return catalogue


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path("assets/astra-visual/fonts")
    parser.add_argument("--font", type=Path, default=root / "Inter-Variable.woff2")
    parser.add_argument("--output", type=Path, default=root)
    parser.add_argument("--name", default="astra-inter-sdf")
    arguments = parser.parse_args()
    bake(arguments.font, arguments.output, arguments.name)


if __name__ == "__main__":
    main()
