"""Formato binário dos assets da interface — o lado que escreve.

O lado que lê é `native/ui/ui_font.cpp` e `native/ui/ui_icon_atlas.cpp`, e os dois
arquivos precisam concordar campo a campo. Por isso o formato está descrito aqui,
uma vez, e não repetido em cada ferramenta.

**Little-endian explícito, nunca um despejo de struct do compilador** — é a mesma
disciplina de `renderer/texture_payload.h`. Um despejo de struct carrega
alinhamento e ordem de bytes do host que gerou o arquivo, e o aparelho que lê não
é o mesmo que escreveu.

**Os pixels vão dentro do arquivo.** Um par PNG + JSON exigiria um decodificador
de PNG e um de JSON no runtime para dois assets que são carregados uma vez, no
início, e nunca mudam. O PNG e o JSON continuam sendo gerados, como artefato de
inspeção — eles é que permitem olhar o atlas e conferir uma métrica sem abrir o
aparelho —, mas não é deles que a engine lê.

## AEUF v1 — atlas de fonte com campo de distância

    u32   'AEUF'  (0x46554541)
    u32   versão = 1
    u32   largura, altura do atlas          (canal único, R8)
    u32   emPixels                          (corpo em que o campo foi medido)
    f32   spreadPixels                      (alcance do campo, em texels)
    u32   primeiroGlifo, númeroDeGlifos, númeroDePesos
    por peso:
      f32 ascent, descent, capHeight        (em unidades de em)
      por glifo:
        f32 advance, bearingX, bearingY, sizeX, sizeY   (em unidades de em)
        u16 atlasX, atlasY, atlasW, atlasH              (texels; tudo zero = sem desenho)
    u8    pixels[largura * altura]

## AEUI v1 — atlas de ícones

    u32   'AEUI'  (0x49554541)
    u32   versão = 1
    u32   largura, altura do atlas          (RGBA8, alfa direto)
    u32   númeroDeÍcones
    por ícone (na ordem do enum gerado em native/ui/ui_icon_id.h):
      u16 atlasX, atlasY, atlasW, atlasH
    u8    pixels[largura * altura * 4]

O ícone é identificado por ÍNDICE, não por nome. O nome vive no enum gerado, o
que transforma um ícone removido em erro de compilação em vez de um retângulo
vazio que ninguém nota até a tela abrir.
"""

from __future__ import annotations

import struct
from pathlib import Path

FONT_MAGIC = 0x46554541  # 'AEUF' em little-endian
ICON_MAGIC = 0x49554541  # 'AEUI'
VERSION = 1


class BinaryWriter:
    """Escritor little-endian explícito."""

    def __init__(self) -> None:
        self._parts: list[bytes] = []

    def u32(self, value: int) -> None:
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError(f"u32 fora de faixa: {value}")
        self._parts.append(struct.pack("<I", value))

    def u16(self, value: int) -> None:
        if not 0 <= value <= 0xFFFF:
            raise ValueError(f"u16 fora de faixa: {value}")
        self._parts.append(struct.pack("<H", value))

    def f32(self, value: float) -> None:
        self._parts.append(struct.pack("<f", float(value)))

    def raw(self, data: bytes) -> None:
        self._parts.append(data)

    def to_bytes(self) -> bytes:
        return b"".join(self._parts)


def write_font(
    destination: Path,
    atlas_size: tuple[int, int],
    em_pixels: int,
    spread_pixels: float,
    first_glyph: int,
    glyph_count: int,
    weights: list[dict],
    pixels: bytes,
) -> int:
    """`weights` na ordem em que o runtime as indexa: regular, medium, semibold."""
    width, height = atlas_size
    if len(pixels) != width * height:
        raise ValueError(f"pixels R8 nao batem: {len(pixels)} != {width * height}")

    writer = BinaryWriter()
    writer.u32(FONT_MAGIC)
    writer.u32(VERSION)
    writer.u32(width)
    writer.u32(height)
    writer.u32(em_pixels)
    writer.f32(spread_pixels)
    writer.u32(first_glyph)
    writer.u32(glyph_count)
    writer.u32(len(weights))

    for weight in weights:
        writer.f32(weight["ascent"])
        writer.f32(weight["descent"])
        writer.f32(weight["capHeight"])
        glyphs = weight["glyphs"]
        if len(glyphs) != glyph_count:
            raise ValueError("todo peso precisa da mesma tabela de glifos")
        for glyph in glyphs:
            writer.f32(glyph["advance"])
            writer.f32(glyph["bearing"][0])
            writer.f32(glyph["bearing"][1])
            writer.f32(glyph["size"][0])
            writer.f32(glyph["size"][1])
            for value in glyph["atlas"]:
                writer.u16(value)

    writer.raw(pixels)
    data = writer.to_bytes()
    destination.write_bytes(data)
    return len(data)


def write_icons(destination: Path, atlas_size: tuple[int, int], rects: list[tuple[int, int, int, int]],
                pixels: bytes) -> int:
    width, height = atlas_size
    if len(pixels) != width * height * 4:
        raise ValueError(f"pixels RGBA8 nao batem: {len(pixels)} != {width * height * 4}")

    writer = BinaryWriter()
    writer.u32(ICON_MAGIC)
    writer.u32(VERSION)
    writer.u32(width)
    writer.u32(height)
    writer.u32(len(rects))
    for rect in rects:
        for value in rect:
            writer.u16(value)
    writer.raw(pixels)
    data = writer.to_bytes()
    destination.write_bytes(data)
    return len(data)


def cpp_identifier(name: str) -> str:
    """`editor/select-box` -> `EditorSelectBox`."""
    parts = [chunk for chunk in name.replace("/", "-").split("-") if chunk]
    return "".join(part[:1].upper() + part[1:] for part in parts)
