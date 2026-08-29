import hashlib
import importlib.util
import json
import math
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "dirt-road"
IMPORTED = SAMPLE / "Imported"


def _load_cook_procedural_sky():
    spec = importlib.util.spec_from_file_location(
        "cook_procedural_sky", ROOT / "tools" / "cook-procedural-sky.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class DirtRoadAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((SAMPLE / "manifest.json").read_text(encoding="utf8"))

    def test_manifest_identity_license_and_all_hashes(self):
        self.assertEqual(self.manifest["version"], 1)
        self.assertEqual(self.manifest["format"], "AEMAP-1")
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
        self.assertEqual(header[:4], (0x504D4541, 1, 144, 72))
        texture_count, material_count, draw_count = header[4:7]
        vertex_count, index_count = header[7:9]
        offsets = header[10:15]
        triangle_count = header[-3]
        self.assertEqual((texture_count, material_count, draw_count), (70, 26, 27))
        self.assertEqual((vertex_count, index_count, triangle_count), (424849, 1023327, 341109))
        self.assertTrue(all(offset % 16 == 0 for offset in offsets))
        self.assertEqual(len(payload), offsets[-1] + index_count * 4)

    def test_cutout_materials_write_depth_and_soft_surfaces_stay_blended(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        material_offset = header[11]
        flags = [struct.unpack_from("<I", payload, material_offset + index * 80 + 68)[0]
                 for index in range(header[5])]
        for index in (0, 1, 6, 8, 9, 10, 11, 12):
            self.assertEqual(flags[index] & 0x11, 0x10, index)
        for index in (14, 15, 16, 24):
            self.assertEqual(flags[index] & 0x11, 0x01, index)

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
            else:
                self.assertLessEqual(max(width, height), 4096, path.name)
                # 1/2 are ASTC LDR material maps; 5 is the RGB9E5 HDR sky/IBL.
                self.assertIn(encoding, (1, 2, 5), path.name)

    def test_environment_aeenv_v2_structure_and_sun_direction(self):
        payload = (IMPORTED / "environment.aeenv").read_bytes()
        self.assertEqual(len(payload), 224)
        magic, version, size, reserved = struct.unpack_from("<4I", payload)
        self.assertEqual((magic, version, size, reserved), (0x4E454541, 2, 224, 0))
        values = struct.unpack_from("<52f", payload, 16)
        self.assertTrue(all(math.isfinite(v) for v in values), values)
        sun_direction = values[0:3]
        self.assertAlmostEqual(sum(c * c for c in sun_direction) ** .5, 1.0, places=5)
        sun_intensity, exposure = values[3], values[12]
        self.assertGreater(sun_intensity, 0)
        self.assertGreater(exposure, 0)

    def test_environment_texture_is_small_and_not_the_old_photographic_hdri(self):
        # The environment used to be an 89.5 MiB photographed "sunset forest"
        # HDRI, mismatched with the procedural blue sky it was reflecting.
        # It is now baked from the same analytic sky the dome renders, so it
        # should be small (a few hundred KiB at most, not tens of MiB).
        path = IMPORTED / "environment.aetex"
        self.assertLess(path.stat().st_size, 2 * 1024 * 1024, path.name)
        magic, version, width, height, encoding, mip_count, payload_size = \
            struct.unpack_from("<6IQ", path.read_bytes())
        self.assertEqual((magic, version, encoding), (0x58544541, 1, 5))
        self.assertLessEqual(max(width, height), 256)

    def test_procedural_sky_sh_projection_matches_uniform_sphere_identity(self):
        # Exercises the same cosine-lobe SH projection used to bake
        # environment.aeenv against the textbook result (a uniform sky of
        # unit radiance must integrate to a flat pi irradiance everywhere,
        # and every band above l=0 must vanish by orthogonality) so a wrong
        # quadrature or cosine-lobe factor fails the suite, not just a
        # visual review on hardware.
        module = _load_cook_procedural_sky()
        module.self_test()


if __name__ == "__main__":
    unittest.main()
