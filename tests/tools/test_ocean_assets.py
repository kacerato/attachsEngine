import hashlib
import json
import pathlib
import struct
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "ocean"
IMPORTED = SAMPLE / "Imported"


class OceanAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((SAMPLE / "manifest.json").read_text(encoding="utf8"))

    def test_manifest_hashes_license_and_hdr_provenance(self):
        self.assertEqual((self.manifest["format"], self.manifest["scene"]),
                         ("AEMAP-3", "ocean-gpu"))
        outputs = self.manifest["outputs"]
        self.assertEqual({path.name for path in IMPORTED.iterdir() if path.is_file()},
                         set(outputs))
        for name, expected in outputs.items():
            self.assertEqual(hashlib.sha256((IMPORTED / name).read_bytes()).hexdigest(),
                             expected, name)

        environment = self.manifest["environment"]
        self.assertEqual(environment["license"], "CC0-1.0")
        self.assertEqual(environment["environmentResourceVersion"], 3)
        self.assertEqual(environment["specularRepresentation"],
                         "octahedral-ggx-prefiltered-rgba16f")
        source = ROOT / environment["sourcePath"]
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         environment["sourceSha256"])
        license_text = (SAMPLE / "LICENSE.txt").read_text(encoding="utf8")
        self.assertIn("Hausdorf Clear Sky", license_text)
        self.assertIn("CC0 1.0", license_text)

    def test_water_normals_are_uniform_rgba8_with_complete_mips(self):
        for name, expected_size in (("texture_000.aetex", 256),
                                    ("texture_000-fallback.aetex", 128)):
            payload = (IMPORTED / name).read_bytes()
            magic, version, width, height, encoding, mip_count, data_size = struct.unpack_from(
                "<6IQ", payload)
            self.assertEqual((magic, version, width, height, encoding),
                             (0x58544541, 1, expected_size, expected_size, 4), name)
            self.assertEqual(mip_count, expected_size.bit_length(), name)
            expected_bytes = sum((max(1, expected_size >> level) ** 2) * 4
                                 for level in range(mip_count))
            self.assertEqual((data_size, len(payload)), (expected_bytes, 32 + expected_bytes), name)

    def test_ocean_mesh_has_depth_and_mobile_bounded_density(self):
        grid = self.manifest["grid"]
        bathymetry = self.manifest["bathymetry"]
        self.assertLessEqual(grid["segments"], 256)
        self.assertLessEqual(bathymetry["segments"], 128)
        self.assertGreater(bathymetry["depthRange"][1], bathymetry["depthRange"][0])
        self.assertGreaterEqual(grid["extent"], 300.0)


if __name__ == "__main__":
    unittest.main()
