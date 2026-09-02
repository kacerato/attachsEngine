import hashlib
import json
import pathlib
import struct
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "dirt-road"
IMPORTED = SAMPLE / "Imported"


class DirtRoadAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((SAMPLE / "manifest.json").read_text(encoding="utf8"))

    def test_manifest_identity_license_and_all_hashes(self):
        self.assertEqual(self.manifest["version"], 3)
        self.assertEqual(self.manifest["format"], "AEMAP-3")
        self.assertEqual(self.manifest["license"], "CC-BY-4.0")
        self.assertEqual(self.manifest["author"], "99.Miles")
        self.assertIn("CC-BY-4.0", (SAMPLE / "LICENSE.txt").read_text(encoding="utf8"))
        actual_names = {path.name for path in IMPORTED.iterdir() if path.is_file()}
        self.assertEqual(actual_names, set(self.manifest["outputs"]))
        for name, expected in self.manifest["outputs"].items():
            self.assertEqual(hashlib.sha256((IMPORTED / name).read_bytes()).hexdigest(), expected, name)

    def test_aemap_counts_offsets_and_exact_size(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        self.assertEqual(header[:4], (0x504D4541, 3, 144, 48))
        texture_count, material_count, draw_count = header[4:7]
        vertex_count, index_count = header[7:9]
        offsets = header[10:15]
        triangle_count = header[-3]
        self.assertEqual((texture_count, material_count, draw_count), (70, 26, 1118))
        self.assertEqual((vertex_count, index_count, triangle_count),
                         (424849, 1599780, 533260))
        self.assertTrue(all(offset % 16 == 0 for offset in offsets))
        self.assertEqual(offsets[-1], (offsets[-2] + vertex_count * 48 + 15) & ~15)
        self.assertEqual(len(payload), offsets[-1] + index_count * 4)

    def test_aemap_v3_has_spatial_lod_groups_without_duplicating_source_vertices(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        draw_count, vertex_count = header[6], header[7]
        draw_offset = header[12]
        draws = [struct.unpack_from("<4I16f4fIfI", payload, draw_offset + index * 108)
                 for index in range(draw_count)]
        levels = [draw[-3] for draw in draws]
        groups = {}
        for draw in draws:
            groups.setdefault(draw[-1], []).append(draw)
        self.assertEqual({str(level): levels.count(level) for level in sorted(set(levels))},
                         self.manifest["statistics"]["lodDrawsByLevel"])
        self.assertEqual(sum(len(group) > 1 for group in groups.values()),
                         self.manifest["statistics"]["lodGroups"])
        self.assertEqual(vertex_count, self.manifest["statistics"]["level0Vertices"],
                         "coarse levels reuse source vertices instead of duplicating them")
        for group in groups.values():
            errors = [draw[-2] for draw in group]
            self.assertEqual(errors[0], 0.0)
            self.assertEqual(errors, sorted(errors))

    def test_texture_payloads_have_complete_mips_and_bounded_fallback(self):
        for path in IMPORTED.glob("*.aetex"):
            data = path.read_bytes()
            magic, version, width, height, encoding, mip_count, payload_size = struct.unpack_from("<6IQ", data)
            self.assertEqual((magic, version), (0x58544541, 1), path.name)
            self.assertEqual(len(data), 32 + payload_size, path.name)
            self.assertGreater(mip_count, 0, path.name)
            if "fallback" in path.name:
                self.assertLessEqual(max(width, height), 512, path.name)
                self.assertIn(encoding, (3, 4), path.name)
            elif path.name == "environment.aetex":
                self.assertEqual((width, height, encoding), (1024, 512, 3), path.name)
                # At an equirectangular pole every longitude is the same
                # direction. Keeping the row constant prevents polar streaks.
                first_level = data[32:32 + width * height * 4]
                top = first_level[:width * 4]
                bottom = first_level[(height - 1) * width * 4:height * width * 4]
                self.assertEqual(len(set(struct.iter_unpack("<I", top))), 1)
                self.assertEqual(len(set(struct.iter_unpack("<I", bottom))), 1)
            elif path.name == "environment-specular.aetex":
                self.assertEqual((width, height, encoding, mip_count), (256, 256, 5, 9), path.name)
                values = np.frombuffer(data, dtype="<f2", offset=32)
                self.assertTrue(np.isfinite(values).all(), path.name)
                self.assertGreater(float(values.max()), 0.1, path.name)
            elif path.name == "environment-brdf.aetex":
                self.assertEqual((width, height, encoding, mip_count), (128, 128, 5, 1), path.name)
                values = np.frombuffer(data, dtype="<f2", offset=32).reshape((-1, 4))
                self.assertTrue(np.isfinite(values).all(), path.name)
                self.assertTrue(np.all(values[:, :2] >= 0.0), path.name)
                self.assertTrue(np.all(values[:, :2] <= 1.1), path.name)
            else:
                self.assertLessEqual(max(width, height), 4096, path.name)
                self.assertIn(encoding, (1, 2), path.name)

    def test_environment_v3_serializes_lighting_and_prefiltered_map_description(self):
        payload = (IMPORTED / "environment.aeenv").read_bytes()
        magic, version, byte_size, reserved = struct.unpack_from("<4I", payload)
        self.assertEqual((magic, version, byte_size, reserved),
                         (0x4E454541, 3, 176, 0))
        self.assertEqual(len(payload), byte_size)
        values = struct.unpack_from("<32f", payload, 16)
        self.assertGreater(values[3], 0.0)   # sun intensity
        self.assertGreater(values[11], 0.0)  # ambient strength
        self.assertGreaterEqual(values[27], 1.0)  # neutral-or-vibrant grade
        description = struct.unpack_from("<8I", payload, 144)
        self.assertEqual(description, (1, 256, 256, 9, 128, 128, 1, 3))
        self.assertEqual(self.manifest["environment"]["environmentResourceVersion"], 3)

    def test_runtime_hud_keeps_digit_order_and_glyphs_left_to_right(self):
        shader_dir = ROOT / "native" / "rhi" / "shaders"
        vertex = (shader_dir / "runtime_hud.vert").read_text(encoding="utf8")
        fragment = (shader_dir / "runtime_hud.frag").read_text(encoding="utf8")
        self.assertIn("localPosition=sourcePosition", vertex)
        self.assertNotIn("determinant(upright)", vertex)
        self.assertIn("(2u-cell.x)", fragment)

        glyphs = (31599, 11415, 29671, 29647, 23497,
                  31183, 31215, 29257, 31727, 31695)
        def rows(digit):
            return tuple("".join("#" if glyphs[digit] >> ((4-y)*3+(2-x)) & 1 else "."
                                 for x in range(3)) for y in range(5))
        self.assertEqual(rows(2), ("###", "..#", "###", "#..", "###"))
        self.assertEqual(rows(7), ("###", "..#", "..#", "..#", "..#"))


if __name__ == "__main__":
    unittest.main()
