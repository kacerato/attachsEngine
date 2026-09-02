import importlib.util
import pathlib
import struct
import unittest

import numpy as np


ROOT = pathlib.Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


upgrade = load("upgrade_aemap_lod", ROOT / "tools" / "upgrade_aemap_lod.py")
cook = upgrade.COOK


def align(value):
    return (value + 15) & ~15


def fixture(material_flags=0):
    # Dense continuous grid plus one distant triangle: the latter enlarges the
    # primitive bounds enough for the deterministic grid to produce useful LOD
    # candidates without any artificial test-only simplifier setting.
    side = 20
    position = np.array([[x * 0.4, y * 0.4, 0.0]
                         for y in range(side) for x in range(side)], dtype=np.float32)
    position = np.concatenate((position, np.array([[100, 0, 0], [101, 0, 0],
                                                   [100, 1, 0]], dtype=np.float32)))
    normal = np.tile([0, 0, 1], (len(position), 1)).astype(np.float32)
    tangent = np.zeros((len(position), 4), dtype=np.float32)
    tangent[:, 0] = tangent[:, 3] = 1.0
    uv0 = position[:, :2] / 8.0
    uv1 = uv0.copy()
    color = np.ones((len(position), 4), dtype=np.float32)
    indices = []
    for y in range(side - 1):
        for x in range(side - 1):
            a = y * side + x
            b = a + 1
            c = a + side
            d = c + 1
            indices.extend((a, b, c, b, d, c))
    indices.extend((side * side, side * side + 1, side * side + 2))
    vertex_bytes = b"".join(cook.vertex_record(*values)
                            for values in zip(position, normal, tangent, uv0, uv1, color))
    index_bytes = struct.pack(f"<{len(indices)}I", *indices)
    texture = bytes(16)
    material = bytearray(80)
    struct.pack_into("<4I", material, 0, *([0xFFFFFFFF] * 4))
    struct.pack_into("<I", material, 68, material_flags)
    model = np.identity(4, dtype=np.float32).reshape(16, order="F")
    minimum = position.min(axis=0)
    maximum = position.max(axis=0)
    center = (minimum + maximum) * 0.5
    radius = float(np.linalg.norm(position - center, axis=1).max())
    draw = struct.pack(upgrade.LEGACY_DRAW_FORMAT, 0, len(indices), 0, 0,
                       *model, *center, radius)
    texture_offset = upgrade.HEADER_SIZE
    material_offset = align(texture_offset + len(texture))
    draw_offset = align(material_offset + len(material))
    vertex_offset = align(draw_offset + len(draw))
    index_offset = align(vertex_offset + len(vertex_bytes))
    payload = bytearray(index_offset + len(index_bytes))
    header = struct.pack(
        upgrade.HEADER_FORMAT, upgrade.MAP_MAGIC, upgrade.SOURCE_VERSION,
        upgrade.HEADER_SIZE, upgrade.VERTEX_STRIDE, 1, 1, 1, len(position), len(indices), 0,
        texture_offset, material_offset, draw_offset, vertex_offset, index_offset,
        *minimum, *maximum, 0.0, 2.0, -10.0, 0.0, 0.0, 0.1, 1000.0,
        len(indices) // 3, 0, 0)
    payload[:upgrade.HEADER_SIZE] = header
    payload[texture_offset:texture_offset + len(texture)] = texture
    payload[material_offset:material_offset + len(material)] = material
    payload[draw_offset:draw_offset + len(draw)] = draw
    payload[vertex_offset:vertex_offset + len(vertex_bytes)] = vertex_bytes
    payload[index_offset:] = index_bytes
    return bytes(payload), vertex_bytes, index_bytes


class AemapLodUpgradeTests(unittest.TestCase):
    def test_preserves_lod_zero_geometry_and_reuses_source_vertices(self):
        source, source_vertices, source_indices = fixture()
        output, report = upgrade.upgrade_package(source, world_cell_size=512)
        header = struct.unpack_from(upgrade.HEADER_FORMAT, output)
        self.assertEqual(header[1], upgrade.TARGET_VERSION)
        self.assertEqual(header[7], report["level0Vertices"])
        self.assertEqual(header[7], len(source_vertices) // upgrade.VERTEX_STRIDE)
        self.assertGreater(report["lodGroups"], 0)
        self.assertGreater(header[6], report["sourceDraws"])
        self.assertEqual(report["lodTrianglesByLevel"]["0"], report["level0Triangles"])
        target_vertices = output[header[13]:header[13] + header[7] * upgrade.VERTEX_STRIDE]
        self.assertEqual(target_vertices, source_vertices)
        # This fixture is one sub-8192-triangle group, so its level-0 stream is
        # not even spatially reordered and remains byte-identical.
        target_level0_indices = output[header[14]:header[14] + len(source_indices)]
        self.assertEqual(target_level0_indices, source_indices)

    def test_alpha_tested_geometry_is_a_lod_target_but_blend_is_not(self):
        alpha_source, _, _ = fixture(cook.MATERIAL_ALPHA_MASK)
        _, alpha_report = upgrade.upgrade_package(alpha_source, world_cell_size=512)
        self.assertGreater(alpha_report["lodGroups"], 0)
        blend_source, _, _ = fixture(cook.MATERIAL_BLEND)
        _, blend_report = upgrade.upgrade_package(blend_source, world_cell_size=512)
        self.assertEqual(blend_report["lodGroups"], 0)
        self.assertEqual(blend_report["draws"], 1)

    def test_rejects_wrong_source_version_without_writing(self):
        source, _, _ = fixture()
        invalid = bytearray(source)
        struct.pack_into("<I", invalid, 4, upgrade.TARGET_VERSION + 1)
        with self.assertRaisesRegex(ValueError, "AEMAP-2 or AEMAP-3"):
            upgrade.upgrade_package(bytes(invalid))


if __name__ == "__main__":
    unittest.main()
