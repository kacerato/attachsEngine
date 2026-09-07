#!/usr/bin/env python3
"""Fatia as folhas de ícone do ASTRA em PNGs individuais.

As folhas são grades, mas NÃO são grades uniformes: uma linha tem seis ícones e
a seguinte tem oito. Por isso a detecção é feita em duas projeções — primeiro as
faixas horizontais que contêm desenho, depois as colunas dentro de cada faixa —
em vez de dividir a imagem em células de tamanho fixo. Uma grade presumida
recortaria metade de um ícone assim que uma linha tivesse contagem diferente.

**A máscara vem do canal alfa, não da cor.** As folhas já chegam com fundo
transparente, e os ícones têm branco DENTRO deles (a mão, o preenchimento das
setas, o miolo da câmera). Qualquer detecção por "quase branco" apagaria esses
miolos e ainda por cima leria o fundo transparente como preto, porque descartar
o alfa deixa RGB zerado.

Partes de um mesmo ícone são reunidas por TAMANHO, nunca por dilatação. Uma
dilatação precisa de um raio, e nenhum raio funciona: o vão entre o gizmo de três
eixos e a mira ao lado dele mede 19 px, e o vão entre as linhas 3 e 4 de uma das
folhas mede 20 px — os dois menores do que o raio necessário para juntar os três
pontos de um menu. O critério que separa os casos não é a distância, é a largura
somada: três bolinhas somam a largura de um ícone, dois ícones somam o dobro.

Saída: PNGs numerados (A01, A02, ...) mais uma folha de contato com os rótulos,
que é o artefato que permite mapear para nomes semânticos sem adivinhação.

Uso:
    python tools/slice-icon-sheet.py --sheet A folha1.png folha2.png ...
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

# Alfa acima disto conta como desenho. Baixo de propósito: a franja anti-aliased
# de um contorno preto é o que define a silhueta, e cortá-la encolheria o
# recorte para dentro do traço.
ALPHA_THRESHOLD = 16
# Um grupo pode crescer até este múltiplo do ícone típico da folha. Acima disso
# ele deixou de ser um ícone e virou dois.
MERGE_EXTENT_RATIO = 1.30
# E só atravessa vãos até esta fração do ícone típico.
MERGE_GAP_RATIO = 0.40
# Faixas e colunas menores que isto são poeira de compressão, não ícone.
MINIMUM_EXTENT = 24
# Margem em volta do recorte apertado, em fração do lado maior. Ela existe para
# que o traço não encoste na borda da textura e vaze no filtro bilinear do atlas.
PADDING_RATIO = 0.06

EXPORT_SIZES = (96, 64, 48, 32, 24)


def ink_mask(image: Image.Image) -> np.ndarray:
    return np.asarray(image, dtype=np.uint8)[..., 3] > ALPHA_THRESHOLD


def runs(occupied: np.ndarray, minimum: int) -> list[tuple[int, int]]:
    """Intervalos contíguos de True, descartando os curtos demais."""
    spans: list[tuple[int, int]] = []
    start = None
    for index, value in enumerate(occupied):
        if value and start is None:
            start = index
        elif not value and start is not None:
            if index - start >= minimum:
                spans.append((start, index))
            start = None
    if start is not None and len(occupied) - start >= minimum:
        spans.append((start, len(occupied)))
    return spans


def merge_spans(spans: list[tuple[int, int]], typical: float) -> list[tuple[int, int]]:
    """Funde partes adjacentes enquanto o resultado ainda tem cara de um ícone.

    É a regra que separa "três bolinhas de um menu" de "dois ícones vizinhos":
    as duas situações têm vãos parecidos, mas extensões somadas muito diferentes.
    Serve aos dois eixos — nas linhas e nas colunas a pergunta é a mesma.
    """
    if not spans:
        return []
    merged = [spans[0]]
    for span in spans[1:]:
        start, end = merged[-1]
        gap = span[0] - end
        if gap <= typical * MERGE_GAP_RATIO and (span[1] - start) <= typical * MERGE_EXTENT_RATIO:
            merged[-1] = (start, span[1])
        else:
            merged.append(span)
    return merged


def median_extent(spans: list[tuple[int, int]]) -> float:
    """Escala da folha: quanto mede um ícone típico neste eixo."""
    return float(np.median([end - start for start, end in spans])) if spans else 0.0


def tight_bounds(mask: np.ndarray, top: int, bottom: int, left: int, right: int):
    """Recorte apertado do desenho REAL na célula, sem a dilatação."""
    window = mask[top:bottom, left:right]
    rows = np.nonzero(window.any(axis=1))[0]
    columns = np.nonzero(window.any(axis=0))[0]
    if rows.size == 0 or columns.size == 0:
        return None
    return (
        top + int(rows[0]),
        top + int(rows[-1]) + 1,
        left + int(columns[0]),
        left + int(columns[-1]) + 1,
    )


def square_canvas(icon: Image.Image, padding_ratio: float) -> Image.Image:
    """Centra o ícone num quadrado, para que todos compartilhem a mesma caixa.

    Sem isto, uma seta larga e um ponto pequeno viriam com proporções diferentes
    e o mesmo tamanho de desenho na interface daria pesos visuais distintos.
    """
    side = max(icon.width, icon.height)
    side = int(round(side * (1.0 + padding_ratio * 2.0)))
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(icon, ((side - icon.width) // 2, (side - icon.height) // 2), icon)
    return canvas


def slice_sheet(path: Path, letter: str, output: Path) -> list[dict]:
    image = Image.open(path).convert("RGBA")
    mask = ink_mask(image)

    # A projeção de linha soma a largura inteira, então partes empilhadas de um
    # ícone raramente criam faixa própria: algum vizinho da mesma linha cobre o
    # vão. A fusão por tamanho fica como rede de segurança para quando não cobre.
    raw_bands = runs(mask.any(axis=1), 4)
    bands = merge_spans(raw_bands, median_extent(raw_bands))
    bands = [(top, bottom) for top, bottom in bands if bottom - top >= MINIMUM_EXTENT]
    if not bands:
        return []

    raw_columns = [span for top, bottom in bands
                   for span in runs(mask[top:bottom, :].any(axis=0), 4)]
    typical = median_extent(raw_columns)
    if typical <= 0.0:
        return []

    entries: list[dict] = []
    index = 0
    for top, bottom in bands:
        columns = merge_spans(runs(mask[top:bottom, :].any(axis=0), 4), typical)
        for left, right in columns:
            if right - left < MINIMUM_EXTENT:
                continue
            bounds = tight_bounds(mask, top, bottom, left, right)
            if bounds is None:
                continue
            index += 1
            label = f"{letter}{index:02d}"
            tight_top, tight_bottom, tight_left, tight_right = bounds
            crop = image.crop((tight_left, tight_top, tight_right, tight_bottom))
            icon = square_canvas(crop, PADDING_RATIO)

            icon.save(output / f"{label}.png")
            for size in EXPORT_SIZES:
                icon.resize((size, size), Image.LANCZOS).save(output / f"{label}-{size}.png")
            entries.append(
                {
                    "label": label,
                    "sheet": path.name,
                    "source": [tight_left, tight_top, tight_right, tight_bottom],
                    "square": icon.width,
                }
            )
    return entries


def contact_sheet(entries: list[dict], directory: Path, destination: Path, columns: int = 6) -> None:
    """Folha numerada: é o artefato que torna o mapeamento semântico revisável."""
    cell, label_height, gap = 128, 26, 10
    rows = (len(entries) + columns - 1) // columns
    width = columns * (cell + gap) + gap
    height = rows * (cell + label_height + gap) + gap
    # Fundo claro porque os ícones são pretos com detalhe lima: sobre preto o
    # contorno sumiria e a folha deixaria de servir para conferir a silhueta.
    sheet = Image.new("RGB", (width, height), (244, 244, 244))
    draw = ImageDraw.Draw(sheet)
    for position, entry in enumerate(entries):
        column, row = position % columns, position // columns
        x = gap + column * (cell + gap)
        y = gap + row * (cell + label_height + gap)
        icon = Image.open(directory / f"{entry['label']}.png").convert("RGBA")
        icon.thumbnail((cell, cell), Image.LANCZOS)
        sheet.paste(icon, (x + (cell - icon.width) // 2, y + (cell - icon.height) // 2), icon)
        draw.text((x + 4, y + cell + 8), entry["label"], fill=(20, 20, 20))
    sheet.save(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheets", nargs="+", type=Path)
    parser.add_argument("--sheet", required=True, help="Letra inicial (A, B, C, ...)")
    parser.add_argument("--output", type=Path, default=Path("assets/astra-visual/icons/sliced"))
    arguments = parser.parse_args()

    arguments.output.mkdir(parents=True, exist_ok=True)
    catalogue: list[dict] = []
    for offset, sheet in enumerate(arguments.sheets):
        letter = chr(ord(arguments.sheet) + offset)
        entries = slice_sheet(sheet, letter, arguments.output)
        contact_sheet(entries, arguments.output, arguments.output / f"contact-{letter}.png")
        catalogue.extend(entries)
        print(f"{sheet.name}: {len(entries)} icones como {letter}01..{letter}{len(entries):02d}")

    (arguments.output / "sliced.json").write_text(
        json.dumps(catalogue, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    print(f"total: {len(catalogue)} icones em {arguments.output}")


if __name__ == "__main__":
    main()
