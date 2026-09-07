#!/usr/bin/env python3
"""Empacota os ícones nomeados num atlas e gera o enum que a engine usa.

O identificador do ícone é um ÍNDICE, não uma string. O nome vive no enum
gerado em `native/ui/ui_icon_id.h`, o que faz um ícone renomeado ou removido
virar erro de compilação — e não um retângulo vazio que só aparece quando
alguém abre aquela tela no aparelho.

Células de tamanho uniforme, e não empacotamento apertado: dois ícones com a
mesma altura de célula têm o mesmo peso visual na barra, e é isso que faz uma
fileira de ferramentas parecer alinhada. Um empacotamento por prateleiras
economizaria área e devolveria a inconsistência que `square_canvas` removeu no
fatiamento.

**Os ícones são recoloridos para fundo escuro.** As folhas foram desenhadas como
adesivos para fundo claro: 60 a 75% da área de cada ícone é preenchimento preto,
com o detalhe em branco por dentro. Sobre os painéis do editor, que são pretos,
esse corpo simplesmente desaparece e sobra o detalhe solto — um sol vira um ponto
lima, um cubo vira três riscos. A recoloração inverte a LUMINÂNCIA dos pixels que
não são lima: o corpo preto vira claro e o detalhe branco vira escuro, o que
devolve a silhueta e mantém as linhas internas legíveis. O lima é preservado
porque ele não é luminância, é a identidade da marca.

Isso muda a aparência em relação às folhas de origem, e de propósito: um ícone
que não se vê não é um ícone. A tintura em tempo de execução continua funcionando
por multiplicação — branco deixa intacto, e o quase-preto da pastilha lima ativa
inverte a polaridade de novo, que é o que os masters mostram.

**Sem mipmaps.** As células são vizinhas no atlas e um mip alto mistura o
contorno de um ícone com o do lado — o clássico sangramento de atlas. A célula
de 96 px cobre o maior desenho da interface (48 dp numa tela 2×), então a
minificação é suave e a ampliação não acontece. Se algum dia um ícone precisar
aparecer bem maior, a resposta é uma célula maior, não uma cadeia de mips.

Uso:
    python tools/pack-icon-atlas.py
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


import numpy as np
from PIL import Image

from ui_asset_format import cpp_identifier, write_icons

CELL = 96
PADDING = 2
ATLAS_WIDTH = 1024

# A marca entra no mesmo atlas, mas NÃO na grade: um logotipo tem proporção
# própria e espremê-lo num quadrado de 96 seria desenhar outra marca. Ele
# também não passa pela recoloração — o branco do logotipo é o branco dele, e
# inverter a luminância o deixaria preto sobre um painel preto.
BRAND = [
    ("brand/wordmark", "astra-wordmark.png", 56),
    ("brand/mark", "astra-mark.png", 96),
]


def recolour_for_dark_ui(icon: Image.Image) -> Image.Image:
    """Inverte a luminância dos pixels que não são lima, preservando o alfa."""
    pixels = np.asarray(icon, dtype=np.int32)
    red, green, blue, alpha = (pixels[..., channel] for channel in range(4))
    # O lima da marca é o único matiz que o sistema visual usa; qualquer coisa
    # verde-clara e pouco azul é ele, inclusive as bordas anti-aliased.
    lime = (green > 150) & (blue < 120) & (red > 120)
    luminance = (red * 299 + green * 587 + blue * 114) // 1000
    inverted = np.clip(255 - luminance, 0, 255)
    result = pixels.copy()
    for channel in range(3):
        result[..., channel] = np.where(lime, pixels[..., channel], inverted)
    return Image.fromarray(result.astype(np.uint8), "RGBA")

HEADER_PREAMBLE = """// GERADO por tools/pack-icon-atlas.py — não edite à mão.
//
// O índice de cada ícone é a posição dele no atlas binário `astra-ui-icons.aeui`.
// Os dois são produzidos na mesma execução: mudar a ordem aqui sem regerar o
// binário faria a interface desenhar o ícone errado, em silêncio.
//
// `kUiNoIcon` é zero e não indexa nada — é o valor de "sem ícone", pela mesma
// razão que `kUiNoImage` existe em ui_draw_list.h.
#pragma once

#include "core/base.h"

namespace ae::ui {

enum class UiIcon : u32 {
  None = 0,
"""

# Chaves literais em profusão: montado por substituição de marcador, não por
# str.format, que exigiria dobrar cada chave de C++ e tornaria o texto ilegível.
HEADER_EPILOGUE = """};

inline constexpr u32 kUiIconCount = @COUNT@;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
"""

SOURCE_TEMPLATE = """// GERADO por tools/pack-icon-atlas.py — não edite à mão.
#include "ui/ui_icon_id.h"

namespace ae::ui {
namespace {
constexpr const char *kNames[] = {
    "none",
@NAMES@};
} // namespace

const char *uiIconName(UiIcon icon) noexcept {
  const u32 index = static_cast<u32>(icon);
  return index <= kUiIconCount ? kNames[index] : "unknown";
}

} // namespace ae::ui
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    icons_root = Path("assets/astra-visual/icons")
    parser.add_argument("--named", type=Path, default=icons_root / "named")
    parser.add_argument("--output", type=Path, default=Path("assets/astra-visual/ui"))
    parser.add_argument("--header", type=Path, default=Path("native/ui/ui_icon_id.h"))
    parser.add_argument("--source", type=Path, default=Path("native/ui/ui_icon_id.cpp"))
    parser.add_argument("--name", default="astra-ui-icons")
    arguments = parser.parse_args()

    catalogue = json.loads((arguments.named / "catalog.json").read_text(encoding="utf-8"))
    # Ordem alfabética do nome de catálogo: estável entre execuções e entre
    # máquinas, ao contrário da ordem de varredura do sistema de arquivos.
    names = sorted(catalogue["icons"].keys())

    columns = max(1, ATLAS_WIDTH // (CELL + PADDING))
    rows = (len(names) + columns - 1) // columns
    height_total = rows * (CELL + PADDING) + PADDING
    atlas = Image.new("RGBA", (ATLAS_WIDTH, height_total), (0, 0, 0, 0))

    rects: list[tuple[int, int, int, int]] = []
    for index, name in enumerate(names):
        category, _, leaf = name.partition("/")
        source = arguments.named / category / f"{leaf}.png"
        # A recoloração acontece ANTES da redução: inverter depois misturaria a
        # franja anti-aliased do contorno com o fundo transparente e deixaria um
        # halo claro em volta de cada ícone.
        icon = recolour_for_dark_ui(Image.open(source).convert("RGBA"))
        icon = icon.resize((CELL, CELL), Image.LANCZOS)
        x = PADDING + (index % columns) * (CELL + PADDING)
        y = PADDING + (index // columns) * (CELL + PADDING)
        atlas.paste(icon, (x, y))
        rects.append((x, y, CELL, CELL))

    # Prateleira da marca, logo abaixo da grade de ícones.
    brand_root = Path("assets/astra-visual/brand")
    brand_images = []
    shelf_height = 0
    for _, filename, height in BRAND:
        image = Image.open(brand_root / filename).convert("RGBA")
        width = max(1, round(image.width * height / image.height))
        brand_images.append(image.resize((width, height), Image.LANCZOS))
        shelf_height = max(shelf_height, height)
    if brand_images:
        shelf_y = height_total
        grown = Image.new("RGBA", (ATLAS_WIDTH, height_total + shelf_height + PADDING),
                          (0, 0, 0, 0))
        grown.paste(atlas, (0, 0))
        atlas = grown
        cursor_x = PADDING
        for image in brand_images:
            atlas.paste(image, (cursor_x, shelf_y))
            rects.append((cursor_x, shelf_y, image.width, image.height))
            cursor_x += image.width + PADDING
        names = names + [name for name, _, _ in BRAND]

    arguments.output.mkdir(parents=True, exist_ok=True)
    atlas.save(arguments.output / f"{arguments.name}.png")
    binary_size = write_icons(
        arguments.output / f"{arguments.name}.aeui",
        (ATLAS_WIDTH, atlas.height),
        rects,
        atlas.tobytes(),
    )

    entries = []
    identifiers = []
    for index, name in enumerate(names, start=1):
        identifier = cpp_identifier(name)
        identifiers.append(identifier)
        entries.append(f"  {identifier} = {index},  // {name}")
    header = (
        HEADER_PREAMBLE
        + "\n".join(entries)
        + "\n"
        + HEADER_EPILOGUE.replace("@COUNT@", str(len(names)))
    )
    arguments.header.write_text(header, encoding="utf-8")
    arguments.source.write_text(
        SOURCE_TEMPLATE.replace(
            "@NAMES@", "".join(f'    "{name}",\n' for name in names)
        ),
        encoding="utf-8",
    )

    occupancy = sum(rect[2] * rect[3] for rect in rects) / float(ATLAS_WIDTH * atlas.height)
    print(
        f"atlas {ATLAS_WIDTH}x{atlas.height}, {len(rects)} entradas, ocupacao "
        f"{occupancy * 100:.1f}%, binario {binary_size / 1024:.0f} KiB"
    )
    print(f"enum: {arguments.header} ({len(identifiers)} entradas)")


if __name__ == "__main__":
    main()
