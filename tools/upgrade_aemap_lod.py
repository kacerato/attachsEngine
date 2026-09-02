"""Upgrade an AEMAP-2 package to AEMAP-3 with deterministic geometry LODs.

This migration exists for already-cooked projects whose original glTF is not
available. It preserves every level-0 vertex byte and every source triangle,
then appends only alternate index streams for optional coarse geometry.
Textures and materials are copied unchanged. New imports should continue to
use cook-gltf-map.py directly.
"""

import argparse
import hashlib
import importlib.util
import json
import math
import pathlib
import struct
import tempfile

import numpy as np


MAP_MAGIC = 0x504D4541
SOURCE_VERSION = 2
TARGET_VERSION = 3
HEADER_SIZE = 144
VERTEX_STRIDE = 48
LEGACY_DRAW_STRIDE = 96
DRAW_STRIDE = 108
TEXTURE_STRIDE = 16
MATERIAL_STRIDE = 80
MATERIAL_FLAGS_OFFSET = 68
LOD_GROUP_TRIANGLES = 2048
LOD_GROUP_WORLD_SIZE = 64.0
HEADER_FORMAT = "<10I5Q6f7f3I"
LEGACY_DRAW_FORMAT = "<4I16f4f"
TARGET_DRAW_FORMAT = "<4I16f4fIfI"


def _load_cooker():
    path = pathlib.Path(__file__).with_name("cook-gltf-map.py")
    spec = importlib.util.spec_from_file_location("aether_cook_gltf_map", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


COOK = _load_cooker()


def align(value, alignment=16):
    return (value + alignment - 1) & ~(alignment - 1)


def _checked_section(payload, offset, count, stride, label):
    if offset < HEADER_SIZE or offset % 16 != 0 or count < 0:
        raise ValueError(f"invalid {label} section")
    size = count * stride
    if offset > len(payload) or size > len(payload) - offset:
        raise ValueError(f"truncated {label} section")
    return payload[offset:offset + size]


def _parse_source_package(payload):
    if len(payload) < HEADER_SIZE:
        raise ValueError("AEMAP header is truncated")
    header = struct.unpack_from(HEADER_FORMAT, payload)
    magic, version, header_size, vertex_stride = header[:4]
    if magic != MAP_MAGIC or version not in (SOURCE_VERSION, TARGET_VERSION) or \
            header_size != HEADER_SIZE or vertex_stride != VERTEX_STRIDE:
        raise ValueError("expected a packed AEMAP-2 or AEMAP-3 package")

    texture_count, material_count, draw_count, vertex_count, index_count = header[4:9]
    texture_offset, material_offset, draw_offset, vertex_offset, index_offset = header[10:15]
    if not material_count or not draw_count or not vertex_count or not index_count:
        raise ValueError("empty AEMAP geometry is not upgradeable")
    textures = _checked_section(payload, texture_offset, texture_count, TEXTURE_STRIDE, "texture")
    materials = _checked_section(payload, material_offset, material_count, MATERIAL_STRIDE, "material")
    source_draw_stride = DRAW_STRIDE if version == TARGET_VERSION else LEGACY_DRAW_STRIDE
    draw_bytes = _checked_section(payload, draw_offset, draw_count, source_draw_stride, "draw")
    vertices = _checked_section(payload, vertex_offset, vertex_count, VERTEX_STRIDE, "vertex")
    index_bytes = _checked_section(payload, index_offset, index_count, 4, "index")
    if index_offset + index_count * 4 != len(payload):
        raise ValueError("AEMAP index section must end at EOF")
    if not (texture_offset + len(textures) <= material_offset and
            material_offset + len(materials) <= draw_offset and
            draw_offset + len(draw_bytes) <= vertex_offset and
            vertex_offset + len(vertices) <= index_offset):
        raise ValueError("AEMAP sections overlap or are out of order")
    if header[28] != index_count // 3 or index_count % 3:
        raise ValueError("AEMAP triangle count is inconsistent")

    if version == TARGET_VERSION:
        all_draws = [struct.unpack_from(TARGET_DRAW_FORMAT, draw_bytes, index * DRAW_STRIDE)
                     for index in range(draw_count)]
        # Rebuild only the authoritative source geometry. Stored coarse levels
        # are derived data and must never be simplified recursively.
        draws = [draw[:24] for draw in all_draws if draw[-3] == 0]
    else:
        draws = [struct.unpack_from(LEGACY_DRAW_FORMAT, draw_bytes,
                                    index * LEGACY_DRAW_STRIDE)
                 for index in range(draw_count)]
    if not draws:
        raise ValueError("AEMAP package has no level-zero source draws")
    indices = np.frombuffer(index_bytes, dtype="<u4")
    vertex_dtype = np.dtype([
        ("position", "<f4", (3,)), ("normal", "<i2", (4,)),
        ("tangent", "<i2", (4,)), ("uv0", "<f4", (2,)),
        ("uv1", "<f4", (2,)), ("color", "u1", (4,)),
    ])
    if vertex_dtype.itemsize != VERTEX_STRIDE:
        raise AssertionError("packed AEMAP vertex dtype drifted")
    decoded_vertices = np.frombuffer(vertices, dtype=vertex_dtype)
    return {
        "header": header, "source_version": version,
        "textures": textures, "materials": materials,
        "draw_bytes": draw_bytes, "draws": draws, "vertices": vertices,
        "decoded_vertices": decoded_vertices, "index_bytes": index_bytes,
        "indices": indices,
    }


def _signed_u32(value):
    return value if value < 0x80000000 else value - 0x100000000


def _draw_indices(package, draw):
    first_index, index_count, vertex_offset, material_index = draw[:4]
    if material_index >= len(package["materials"]) // MATERIAL_STRIDE:
        raise ValueError("draw references a missing material")
    if not index_count or index_count % 3 or first_index > len(package["indices"]) or \
            index_count > len(package["indices"]) - first_index:
        raise ValueError("draw has an invalid index range")
    return package["indices"][first_index:first_index + index_count].copy()


def _source_primitive(package, draw, encoded_indices=None):
    vertex_offset = draw[2]
    encoded = (_draw_indices(package, draw) if encoded_indices is None
               else np.asarray(encoded_indices, dtype=np.uint32)).astype(np.int64)
    resolved = encoded + _signed_u32(vertex_offset)
    if np.any(resolved < 0) or np.any(resolved >= len(package["decoded_vertices"])):
        raise ValueError("draw references a missing vertex")
    unique, local_indices = np.unique(resolved, return_inverse=True)
    source = package["decoded_vertices"][unique]
    position = source["position"].astype(np.float32)
    normal = np.clip(source["normal"][:, :3].astype(np.float32) / 32767.0, -1.0, 1.0)
    tangent = np.clip(source["tangent"].astype(np.float32) / 32767.0, -1.0, 1.0)
    uv0 = source["uv0"].astype(np.float32)
    uv1 = source["uv1"].astype(np.float32)
    color = source["color"].astype(np.float32) / 255.0
    return (position, normal, tangent, uv0, uv1, color, local_indices.astype(np.uint32),
            unique.astype(np.uint32))


def _linear_transform(model):
    matrix = np.asarray(model, dtype=np.float64).reshape((4, 4), order="F")
    linear = matrix[:3, :3]
    translation = matrix[:3, 3]
    if not np.all(np.isfinite(linear)) or not np.all(np.isfinite(translation)):
        raise ValueError("draw transform contains a non-finite value")
    return linear, translation


def _morton_codes(points):
    minimum = points.min(axis=0)
    extent = points.max(axis=0) - minimum
    normalized = np.divide(points - minimum, extent, out=np.zeros_like(points), where=extent > 1.0e-9)
    quantized = np.rint(np.clip(normalized, 0.0, 1.0) * 1023.0).astype(np.uint32)
    codes = np.zeros(len(points), dtype=np.uint32)
    for bit in range(10):
        codes |= ((quantized[:, 0] >> bit) & 1) << (bit * 3)
        codes |= ((quantized[:, 1] >> bit) & 1) << (bit * 3 + 1)
        codes |= ((quantized[:, 2] >> bit) & 1) << (bit * 3 + 2)
    return codes


def _spatial_chunks(package, draw, target_triangles=LOD_GROUP_TRIANGLES,
                    world_cell_size=LOD_GROUP_WORLD_SIZE):
    """Split one opaque draw into spatially coherent, exact triangle streams.

    The runtime selects one LOD for each returned chunk, so a large terrain or
    forest primitive can stay detailed around the camera while distant cells
    simplify independently.  Chunk level 0 only reorders complete triangles;
    it never changes an index, winding or vertex byte.
    """
    encoded = _draw_indices(package, draw).reshape((-1, 3))
    resolved = encoded.astype(np.int64) + _signed_u32(draw[2])
    positions = package["decoded_vertices"][resolved]["position"].astype(np.float64)
    local_centroids = positions.mean(axis=1)
    linear, translation = _linear_transform(draw[4:20])
    world_centroids = local_centroids @ linear.T + translation
    cell_coordinates = np.floor(world_centroids / world_cell_size).astype(np.int64)
    cells = {}
    for triangle_index, coordinates in enumerate(cell_coordinates):
        cells.setdefault(tuple(coordinates), []).append(triangle_index)
    chunks = []
    for key in sorted(cells):
        cell_indices = np.asarray(cells[key], dtype=np.int64)
        if len(cell_indices) > target_triangles:
            local_order = np.argsort(_morton_codes(world_centroids[cell_indices]), kind="stable")
            cell_indices = cell_indices[local_order]
        for first in range(0, len(cell_indices), target_triangles):
            chunks.append(encoded[cell_indices[first:first + target_triangles]].reshape(-1))
    return chunks


def _world_bounds(position, model):
    linear, translation = _linear_transform(model)
    world = position.astype(np.float64) @ linear.T + translation
    minimum = world.min(axis=0)
    maximum = world.max(axis=0)
    center = (minimum + maximum) * 0.5
    radius = float(np.linalg.norm(world - center, axis=1).max())
    return center.astype(np.float32), radius


def _maximum_world_scale(model):
    # Exact operator norm of the linear transform.  Multiplying the local
    # clustering error by this value is conservative under non-uniform scale,
    # rotation and shear; max(column length) is not conservative under shear.
    linear, _ = _linear_transform(model)
    return float(np.linalg.svd(linear, compute_uv=False)[0])


def upgrade_package(payload, target_triangles=LOD_GROUP_TRIANGLES,
                    world_cell_size=LOD_GROUP_WORLD_SIZE):
    """Return ``(aemap3_bytes, report)`` without mutating the source payload."""
    if not 256 <= target_triangles <= 32768:
        raise ValueError("target triangle count must be in [256, 32768]")
    if not 8.0 <= world_cell_size <= 512.0:
        raise ValueError("world cell size must be in [8, 512]")
    package = _parse_source_package(payload)
    header = package["header"]
    original_draw_count = len(package["draws"])
    original_vertex_count = header[7]
    original_index_count = sum(draw[1] for draw in package["draws"])
    target_draws = []
    target_indices = []
    draws_by_level = {}
    triangles_by_level = {}
    lod_groups = 0
    level0_draws = 0
    next_lod_group_id = 0

    for draw in package["draws"]:
        material_index = draw[3]
        material_flags = struct.unpack_from(
            "<I", package["materials"], material_index * MATERIAL_STRIDE + MATERIAL_FLAGS_OFFSET)[0]
        # Only true blend keeps its source draw intact: ordering participates
        # in composition. Alpha-tested vegetation still writes depth and is a
        # primary LOD/overdraw target, so it is safely chunked and simplified.
        render_order_sensitive = (material_flags & COOK.MATERIAL_BLEND) != 0
        chunks = [_draw_indices(package, draw)] if render_order_sensitive else \
            _spatial_chunks(package, draw, target_triangles, world_cell_size)
        for encoded_chunk in chunks:
            position, normal, tangent, uv0, uv1, color, local_indices, source_vertices = \
                _source_primitive(package, draw, encoded_chunk)
            center, radius = _world_bounds(position, draw[4:20])
            local_extent = position.astype(np.float64).max(axis=0) - \
                position.astype(np.float64).min(axis=0)
            local_radius = float(np.linalg.norm(local_extent) * 0.5)
            levels = COOK.simplify_primitive_levels(
                position, normal, tangent, uv0, uv1, color, local_indices,
                material_flags, local_radius, reuse_source_vertices=True)
            group_id = next_lod_group_id
            next_lod_group_id += 1
            if len(levels) > 1:
                lod_groups += 1
            world_scale = _maximum_world_scale(draw[4:20])
            for level in levels:
                first_index = len(target_indices)
                if level["level"] == 0:
                    # Exact source triangle stream (possibly Morton-reordered
                    # as whole triangles), still indexing untouched vertices.
                    new_indices = [int(index) for index in encoded_chunk]
                    vertex_offset = draw[2]
                    level0_draws += 1
                else:
                    representatives = source_vertices[level["sourceVertexIndices"]]
                    new_indices = [int(index) for index in representatives[level["indices"]]]
                    vertex_offset = 0
                target_indices.extend(new_indices)
                world_error = float(level["geometricError"] * world_scale)
                target_draws.append(struct.pack(
                    TARGET_DRAW_FORMAT, first_index, len(new_indices), vertex_offset, material_index,
                    *draw[4:20], *center, radius, level["level"], world_error, group_id))
                draws_by_level[level["level"]] = draws_by_level.get(level["level"], 0) + 1
                triangles_by_level[level["level"]] = (
                    triangles_by_level.get(level["level"], 0) + len(new_indices) // 3)

    target_vertex_count = original_vertex_count
    target_index_count = len(target_indices)
    texture_offset = HEADER_SIZE
    material_offset = align(texture_offset + len(package["textures"]))
    draw_offset = align(material_offset + len(package["materials"]))
    vertex_offset = align(draw_offset + len(target_draws) * DRAW_STRIDE)
    index_offset = align(vertex_offset + target_vertex_count * VERTEX_STRIDE)
    total_size = index_offset + target_index_count * 4
    output = bytearray(total_size)

    # Preserve every header field not owned by the format/layout migration.
    output[:HEADER_SIZE] = payload[:HEADER_SIZE]
    struct.pack_into("<I", output, 4, TARGET_VERSION)
    struct.pack_into("<III", output, 24, len(target_draws), target_vertex_count,
                     target_index_count)
    struct.pack_into("<5Q", output, 40, texture_offset, material_offset, draw_offset,
                     vertex_offset, index_offset)
    struct.pack_into("<I", output, 132, target_index_count // 3)
    output[texture_offset:texture_offset + len(package["textures"])] = package["textures"]
    output[material_offset:material_offset + len(package["materials"])] = package["materials"]
    output[draw_offset:draw_offset + len(target_draws) * DRAW_STRIDE] = b"".join(target_draws)
    vertex_payload = package["vertices"]
    output[vertex_offset:vertex_offset + len(vertex_payload)] = vertex_payload
    struct.pack_into(f"<{len(target_indices)}I", output, index_offset, *target_indices)

    report = {
        "sourceVersion": package["source_version"],
        "targetVersion": TARGET_VERSION,
        "preservesLevel0": True,
        "level0Contract": "exact-vertices-and-triangles-spatially-reordered",
        "lodStrategy": "source-vertex-grid-clustering-v1",
        "lodGroups": lod_groups,
        "lodTrianglesPerGroup": target_triangles,
        "lodWorldCellSize": world_cell_size,
        "sourceDraws": original_draw_count,
        "level0Draws": level0_draws,
        "level0Vertices": original_vertex_count,
        "level0Triangles": original_index_count // 3,
        "draws": len(target_draws),
        "vertices": target_vertex_count,
        "triangles": target_index_count // 3,
        "lodDrawsByLevel": {str(key): value for key, value in sorted(draws_by_level.items())},
        "lodTrianglesByLevel": {str(key): value for key, value in sorted(triangles_by_level.items())},
    }
    return bytes(output), report


def _atomic_write(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = pathlib.Path(stream.name)
            stream.write(payload)
            stream.flush()
        temporary.replace(path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def update_manifest(path, imported_directory, report):
    manifest = json.loads(path.read_text(encoding="utf8"))
    source_is_v2 = manifest.get("version") == SOURCE_VERSION and manifest.get("format") == "AEMAP-2"
    source_is_preserved_v3 = (manifest.get("version") == TARGET_VERSION and
                              manifest.get("format") == "AEMAP-3" and
                              manifest.get("geometryLod", {}).get("preservesLevel0") is True)
    if not source_is_v2 and not source_is_preserved_v3:
        raise ValueError("manifest does not describe an upgradeable AEMAP source")
    manifest["version"] = TARGET_VERSION
    manifest["format"] = "AEMAP-3"
    statistics = manifest.setdefault("statistics", {})
    for key in ("draws", "vertices", "triangles", "sourceDraws", "level0Draws",
                "level0Vertices", "level0Triangles", "lodGroups", "lodDrawsByLevel",
                "lodTrianglesByLevel", "lodTrianglesPerGroup", "lodWorldCellSize"):
        statistics[key] = report[key]
    manifest["geometryLod"] = {
        "version": 1,
        "sourceFormat": manifest.get("geometryLod", {}).get("sourceFormat", "AEMAP-2"),
        "preservesLevel0": report["preservesLevel0"],
        "level0Contract": report["level0Contract"],
        "strategy": report["lodStrategy"],
    }
    manifest["outputs"] = {
        asset.name: hashlib.sha256(asset.read_bytes()).hexdigest()
        for asset in sorted(imported_directory.iterdir()) if asset.is_file()
    }
    _atomic_write(path, (json.dumps(manifest, indent=2) + "\n").encode("utf8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path, help="source AEMAP-2 file")
    parser.add_argument("--out", type=pathlib.Path,
                        help="target AEMAP-3 file (defaults to replacing source atomically)")
    parser.add_argument("--manifest", type=pathlib.Path,
                        help="optional AEMAP-2 manifest to upgrade after the package")
    parser.add_argument("--triangles-per-group", type=int, default=LOD_GROUP_TRIANGLES,
                        help="spatial LOD cell triangle budget (256..32768)")
    parser.add_argument("--world-cell-size", type=float, default=LOD_GROUP_WORLD_SIZE,
                        help="spatial LOD cell size in world units (8..512)")
    args = parser.parse_args()
    output_path = args.out or args.source
    upgraded, report = upgrade_package(args.source.read_bytes(), args.triangles_per_group,
                                       args.world_cell_size)
    _atomic_write(output_path, upgraded)
    if args.manifest:
        update_manifest(args.manifest, output_path.parent, report)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
