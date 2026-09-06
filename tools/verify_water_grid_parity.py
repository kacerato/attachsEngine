"""Confere a grade de água assada contra o contrato que a gera.

A malha da água existe em três lugares que precisam concordar:

  1. `tools/water_geometry.py` — o contrato offline;
  2. `samples/*/Imported/scene.aemap` — o que foi realmente assado;
  3. `native/renderer/water_grid.{h,cpp}` — o construtor de runtime.

Os testes nativos prendem 3 contra 1 comparando valores fixos. Este script
prende 2 contra 1, lendo o pacote assado e conferindo vértice a vértice. Com os
dois elos, trocar a malha assada pela construída deixa de ser um salto de fé.

    python tools/verify_water_grid_parity.py [samples/ocean]
"""
from __future__ import annotations

import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from water_geometry import graded_water_axis  # noqa: E402

MAGIC = 0x504D_4541  # "AEMP" em little-endian, igual a MapPackageMagic
HEADER_SIZE = 144
VERTEX_STRIDE = 48
# renderer::MapMaterialWaterCameraGrid — a flag que marca a grade relativa à
# câmera, e a única cujo uv1 carrega (espaçamento, alcance) em vez de uv.
# MapMaterialRecord: 4 índices + 4 fatores + 4 emissivos + 5 escalares antes das
# flags, e três u32 depois — 80 bytes com `flags` no deslocamento 68.
MATERIAL_STRIDE = 80
MATERIAL_FLAGS_OFFSET = 68


def material_flag_water_camera_grid() -> int:
    """Lê o valor da flag do cabeçalho C++, em vez de duplicá-lo aqui.

    Repetir a constante neste arquivo criaria um quarto lugar para ela
    divergir, que é exatamente o problema que o script existe para pegar.
    """
    header = pathlib.Path(__file__).resolve().parents[1] / "native/renderer/map_package.h"
    for line in header.read_text(encoding="utf-8").splitlines():
        if "MapMaterialWaterCameraGrid" in line and "=" in line:
            value = line.split("=")[1].split(";")[0].strip()
            if value.startswith("1u <<") or value.startswith("1u<<"):
                return 1 << int(value.split("<<")[1].strip().rstrip("u"))
            return int(value.rstrip("u"), 0)
    raise SystemExit("MapMaterialWaterCameraGrid não encontrada em map_package.h")


def decode(path: pathlib.Path):
    data = path.read_bytes()
    magic, version, header_size, stride = struct.unpack_from("<4I", data, 0)
    if magic != MAGIC or header_size != HEADER_SIZE or stride != VERTEX_STRIDE:
        raise SystemExit(f"{path}: cabeçalho inesperado (magic={magic:#x} v={version})")
    material_count, draw_count, vertex_count = struct.unpack_from("<3I", data, 20)
    material_offset, draw_offset, vertex_offset = struct.unpack_from("<3Q", data, 48)
    return {
        "data": data, "version": version, "materialCount": material_count,
        "drawCount": draw_count, "vertexCount": vertex_count,
        "materialOffset": material_offset, "drawOffset": draw_offset,
        "vertexOffset": vertex_offset,
    }


def water_vertices(package, flag: int):
    """Vértices do primeiro draw cujo material é a grade de água."""
    data = package["data"]
    # MapDrawRecord: v3 tem 108 bytes, v1/v2 têm 96. Os quatro primeiros campos
    # (firstIndex, indexCount, vertexOffset, materialIndex) não mudaram.
    draw_stride = 108 if package["version"] >= 3 else 96
    for index in range(package["drawCount"]):
        base = package["drawOffset"] + index * draw_stride
        _, _, vertex_offset, material_index = struct.unpack_from("<4I", data, base)
        if material_index >= package["materialCount"]:
            continue
        flags = struct.unpack_from(
            "<I", data,
            package["materialOffset"] + material_index * MATERIAL_STRIDE + MATERIAL_FLAGS_OFFSET)[0]
        if flags & flag:
            return vertex_offset
    return None


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "samples/ocean")
    package_path = root / "Imported" / "scene.aemap"
    if not package_path.is_file():
        raise SystemExit(f"pacote não encontrado: {package_path}")

    flag = material_flag_water_camera_grid()
    package = decode(package_path)
    data = package["data"]

    # Os mesmos parâmetros que tools/build-ocean-demo.py usa para assar. Se o
    # cozimento passar a variá-los, eles saem do pacote em vez de daqui.
    segments, near_extent, far_extent = 256, 384.0, 8000.0
    axis, spacing = graded_water_axis(segments, near_extent, far_extent)
    stride = segments + 1

    start = water_vertices(package, flag)
    if start is None:
        # Sem flag legível, cai para a suposição de que a água abre o buffer —
        # é como o cozimento a escreve hoje.
        start = 0
        print("aviso: nenhum draw com a flag de grade; assumindo início do buffer")

    mismatches = 0
    checked = 0
    for row in (0, 1, 64, 128, 192, segments - 1, segments):
        for column in (0, 1, 64, 128, 192, segments - 1, segments):
            index = start + row * stride + column
            base = package["vertexOffset"] + index * VERTEX_STRIDE
            x, y, z = struct.unpack_from("<3f", data, base)
            baked_spacing, baked_extent = struct.unpack_from("<2f", data, base + 36)
            expected = (axis[column], 0.0, axis[row], max(spacing[row], spacing[column]), far_extent)
            actual = (x, y, z, baked_spacing, baked_extent)
            checked += 1
            if any(abs(a - b) > 1e-2 for a, b in zip(actual, expected)):
                mismatches += 1
                print(f"  divergência em ({row},{column}): assado={actual} contrato={expected}")

    print(f"{checked - mismatches}/{checked} vértices conferem com o contrato offline")
    if mismatches:
        raise SystemExit(f"{mismatches} vértices divergem: o cozimento e o contrato saíram de sincronia")
    print("paridade confirmada: aemap assado == tools/water_geometry.py")


if __name__ == "__main__":
    main()
