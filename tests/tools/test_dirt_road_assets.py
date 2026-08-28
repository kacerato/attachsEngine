import hashlib
import json
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "dirt-road"
IMPORTED = SAMPLE / "Imported"


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
                self.assertIn(encoding, (1, 2), path.name)


if __name__ == "__main__":
    unittest.main()
