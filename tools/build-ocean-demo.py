"""Build the deterministic native ocean validation scene.

The output is runtime-ready AEMAP/AETX data: Android never parses this script,
JSON geometry, or source images. The surface itself stays static; MapMaterialWater
causes the shared WaterProfile to displace it in the Vulkan vertex shader.
"""
import hashlib
import json
import math
import pathlib
import shutil
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "samples" / "ocean" / "Imported"
SOURCE_ENV = ROOT / "samples" / "dirt-road" / "Imported"
MAGIC, VERSION, HEADER_SIZE, VERTEX_STRIDE, DRAW_STRIDE = 0x504D4541, 3, 144, 48, 108
INVALID_TEXTURE, MATERIAL_WATER = 0xFFFFFFFF, 1 << 9
MAGIC_AETX, AETX_RGBA8_UNORM = 0x58544541, 4


def align(value, boundary=16):
    return (value + boundary - 1) // boundary * boundary


def snorm(value):
    return max(-32767, min(32767, round(value * 32767)))


def vertex(x, y, z, extent, normal=(0.0, 1.0, 0.0)):
    uv = (x / (extent * 2) + .5, z / (extent * 2) + .5)
    return struct.pack("<3f4h4h2f2f4B", x, y, z,
                       snorm(normal[0]), snorm(normal[1]), snorm(normal[2]), 0,
                       snorm(1), snorm(0), snorm(0), snorm(1),
                       *uv, *uv, 255, 255, 255, 255)


def water_normal_level(size):
    """Periodic slope spectrum, authored offline and free of square-cell seams."""
    # Integer frequency vectors make both axes exactly periodic. Non-harmonic
    # directions and weights prevent the repeated checker appearance of one
    # scrolling normal map while keeping this deterministic and project-owned.
    spectrum = ((3, 5, .44), (-7, 2, .28), (11, -9, .16),
                (17, 13, .075), (-29, 19, .035))
    pixels = []
    for row in range(size):
        v = (row + .5) / size
        for column in range(size):
            u = (column + .5) / size
            slope_x = slope_z = 0.0
            for frequency_x, frequency_z, weight in spectrum:
                phase = 2.0 * math.pi * (frequency_x * u + frequency_z * v)
                cosine = math.cos(phase)
                scale = weight / math.sqrt(frequency_x ** 2 + frequency_z ** 2)
                slope_x += frequency_x * scale * cosine
                slope_z += frequency_z * scale * cosine
            length = math.sqrt(slope_x * slope_x + slope_z * slope_z + 1.0)
            nx, nz = -slope_x / length, -slope_z / length
            pixels.append((round((nx * .5 + .5) * 255),
                           round((nz * .5 + .5) * 255), 255, 255))
    return pixels


def downsample_normals(pixels, size):
    next_size = max(1, size // 2)
    result = []
    for row in range(next_size):
        for column in range(next_size):
            sx = sz = 0.0
            for offset_y in range(2 if size > 1 else 1):
                for offset_x in range(2 if size > 1 else 1):
                    pixel = pixels[min(size - 1, row * 2 + offset_y) * size +
                                   min(size - 1, column * 2 + offset_x)]
                    sx += pixel[0] / 255.0 * 2.0 - 1.0
                    sz += pixel[1] / 255.0 * 2.0 - 1.0
            samples = 4.0 if size > 1 else 1.0
            sx, sz = sx / samples, sz / samples
            result.append((round((sx * .5 + .5) * 255),
                           round((sz * .5 + .5) * 255), 255, 255))
    return result, next_size


def write_water_normal(path, size):
    levels = []
    pixels = water_normal_level(size)
    level_size = size
    while True:
        levels.append(bytes(channel for pixel in pixels for channel in pixel))
        if level_size == 1:
            break
        pixels, level_size = downsample_normals(pixels, level_size)
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", MAGIC_AETX, 1, size, size,
                                 AETX_RGBA8_UNORM, len(levels), len(payload)) + payload)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Three metres per vertex. The fallback WaterProfile documents and obeys
    # the corresponding spatial frequency limit.
    segments, extent = 256, 384.0
    step = extent * 2.0 / segments
    water_vertices = bytearray()
    for row in range(segments + 1):
        z = -extent + row * step
        for column in range(segments + 1):
            water_vertices += vertex(-extent + column * step, 0.0, z, extent)
    water_indices = []
    stride = segments + 1
    for row in range(segments):
        for column in range(segments):
            a = row * stride + column
            b, c, d = a + 1, a + stride, a + stride + 1
            water_indices.extend((a, c, b, b, c, d))

    # A cena anterior terminava no plano da água: o shader lia depth=far e não
    # havia conteúdo pelo qual provar espessura/refração. Esta batimetria é uma
    # superfície renderizada comum, mais densa perto da câmera que um único
    # quad mas muito mais barata que a malha de ondas.
    floor_segments = 96
    floor_step = extent * 2.0 / floor_segments
    floor_vertices = bytearray()
    for row in range(floor_segments + 1):
        z = -extent + row * floor_step
        for column in range(floor_segments + 1):
            x = -extent + column * floor_step
            depth = max(2.25, min(32.0, 5.0 + 0.021 * (z + 34.0) +
                                  0.7 * math.sin(x * 0.018) * math.sin(z * 0.014)))
            floor_vertices += vertex(x, -depth, z, extent)
    floor_indices = []
    floor_stride = floor_segments + 1
    for row in range(floor_segments):
        for column in range(floor_segments):
            a = row * floor_stride + column
            b, c, d = a + 1, a + floor_stride, a + floor_stride + 1
            floor_indices.extend((a, c, b, b, c, d))
    vertices = water_vertices + floor_vertices
    indices = water_indices + floor_indices

    texture = struct.pack("<4I", 1 | 2, 0, 0, 0)
    water_material = struct.pack("<4I4f4f4ff3I",
        0, INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE,
        1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0,
        .22, 0.0, 1.0, 1.0, .5, MATERIAL_WATER, 0, 0)
    floor_material = struct.pack("<4I4f4f4ff3I",
        INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE,
        .34, .27, .17, 1.0, 0.0, 0.0, 0.0, 1.0,
        .78, 0.0, 1.0, .12, .5, 0, 0, 0)
    material = water_material + floor_material
    identity = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
    radius = math.sqrt(extent * extent * 2.0 + 1.0)
    water_draw = struct.pack("<4I16f4fIfI", 0, len(water_indices), 0, 0, *identity,
                             0.0, 0.0, 0.0, radius, 0, 0.0, 0)
    floor_draw = struct.pack("<4I16f4fIfI", len(water_indices), len(floor_indices),
                             len(water_vertices) // VERTEX_STRIDE, 1, *identity,
                             0.0, -16.0, 0.0, radius, 0, 0.0, 1)
    draw = water_draw + floor_draw

    texture_offset = HEADER_SIZE
    material_offset = align(texture_offset + len(texture))
    draw_offset = align(material_offset + len(material))
    vertex_offset = align(draw_offset + len(draw))
    index_offset = align(vertex_offset + len(vertices))
    package = bytearray(index_offset + len(indices) * 4)
    header = struct.pack("<10I5Q6f7f3I", MAGIC, VERSION, HEADER_SIZE, VERTEX_STRIDE,
        1, 2, 2, len(vertices) // VERTEX_STRIDE, len(indices), 0,
        texture_offset, material_offset, draw_offset, vertex_offset, index_offset,
        -extent, -33.0, -extent, extent, 1.0, extent,
        0.0, 12.0, -34.0, 0.0, 0.28, .1, 1800.0, len(indices) // 3, 0, 0)
    assert len(header) == HEADER_SIZE and len(draw) == DRAW_STRIDE * 2
    package[:len(header)] = header
    package[texture_offset:texture_offset + len(texture)] = texture
    package[material_offset:material_offset + len(material)] = material
    package[draw_offset:draw_offset + len(draw)] = draw
    package[vertex_offset:vertex_offset + len(vertices)] = vertices
    package[index_offset:] = struct.pack(f"<{len(indices)}I", *indices)
    # Mirror the high-value AEMAP draw invariants here so the generated sample
    # cannot reach an APK with swapped binary fields again.
    for draw_index in range(2):
        record = struct.unpack_from("<4I16f4fIfI", package,
                                    draw_offset + draw_index * DRAW_STRIDE)
        first_index, index_count, vertex_base, material_index = record[:4]
        lod_level, geometric_error, lod_group = record[-3:]
        assert material_index < 2
        assert first_index + index_count <= len(indices)
        assert vertex_base < len(vertices) // VERTEX_STRIDE
        assert lod_level == 0 and geometric_error == 0.0 and lod_group < 2
    (OUT / "scene.aemap").write_bytes(package)

    write_water_normal(OUT / "texture_000.aetex", 256)
    write_water_normal(OUT / "texture_000-fallback.aetex", 128)
    copies = {
        "environment.aeenv": "environment.aeenv",
        "environment.aetex": "environment.aetex",
        "environment-specular.aetex": "environment-specular.aetex",
        "environment-brdf.aetex": "environment-brdf.aetex",
    }
    for destination, source in copies.items():
        shutil.copyfile(SOURCE_ENV / source, OUT / destination)
    outputs = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(OUT.iterdir()) if p.is_file()}
    manifest = {"version": 1, "format": "AEMAP-3", "scene": "ocean-gpu",
                "grid": {"segments": segments, "extent": extent},
                "bathymetry": {"segments": floor_segments, "depthRange": [2.25, 32.0]},
                "outputs": outputs}
    ocean = OUT.parent
    (ocean / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    (ocean / "LICENSE.txt").write_text(
        "Aether native ocean validation scene and procedural water normal spectrum:\n"
        "Copyright (c) the Aether project owner.\n\n"
        "Sky environment: \"Hausdorf Clear Sky\" by Grzegorz Wronkowski, downloaded from\n"
        "Poly Haven (https://polyhaven.com/a/hausdorf_clear_sky), licensed CC0 1.0.\n",
        encoding="utf8")
    print(f"ocean: {len(vertices)//VERTEX_STRIDE} vertices, {len(indices)//3} triangles, {len(package)} bytes")


if __name__ == "__main__":
    main()
