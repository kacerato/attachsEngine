import hashlib
import json
import pathlib
import struct
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "ocean"
IMPORTED = SAMPLE / "Imported"


class OceanAssetsTests(unittest.TestCase):
    def test_boat_resources_and_dynamic_geometry_are_consistent(self):
        payload = (IMPORTED / 'scene.aemap').read_bytes()
        header = struct.unpack_from('<10I5Q6f7f3I',payload)
        textures, materials, draws, vertex_count = header[4:8]
        self.assertGreater(textures,1)
        for material in range(5,materials):
            offset=header[11]+material*80
            for texture in struct.unpack_from('<4I',payload,offset):
                self.assertTrue(texture==0xffffffff or 0<texture<textures)
            self.assertTrue(struct.unpack_from('<I',payload,offset+68)[0] & (1<<6))
        for draw in range(5,draws):
            record=struct.unpack_from('<4I16f4fIfI',payload,header[12]+draw*108)
            self.assertEqual(record[24],0)
            self.assertGreater(record[23],0)
            for index in struct.unpack_from(f'<{record[1]}I',payload,header[14]+record[0]*4):
                self.assertLess(index+record[2],vertex_count)

    def test_validation_bodies_have_unique_material_bindings_and_no_static_collision(self):
        bodies = self.manifest["validationBodies"]
        self.assertEqual(len(bodies), 3)
        self.assertEqual(len({body["id"] for body in bodies}), 3)
        payload = (IMPORTED / "scene.aemap").read_bytes()
        material_count, draw_count = struct.unpack_from("<2I", payload, 20)
        self.assertEqual(material_count, 8)
        self.assertEqual(draw_count, 5 + self.manifest['boat']['drawCount'])
        material_offset = struct.unpack_from("<Q", payload, 48)[0]
        for body in bodies:
            flags = struct.unpack_from("<I", payload, material_offset + body["materialIndex"] * 80 + 68)[0]
            self.assertTrue(flags & (1 << 6))
            self.assertTrue(all(value > 0 for value in body["halfExtent"]))

    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((SAMPLE / "manifest.json").read_text(encoding="utf8"))

    def test_draw_index_slices_partition_the_index_buffer(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        draw_count = struct.unpack_from("<I", payload, 24)[0]
        index_count = struct.unpack_from("<I", payload, 32)[0]
        draw_offset = struct.unpack_from("<Q", payload, 56)[0]
        cursor = 0
        for draw in range(draw_count):
            first, count = struct.unpack_from("<2I", payload, draw_offset + draw * 108)
            self.assertEqual(first, cursor)
            cursor += count
        self.assertEqual(cursor, index_count)

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
        self.assertIn(environment["sourceUrl"], license_text)
        self.assertIn("CC0 1.0", license_text)

    def test_sky_resolution_and_complete_mips_match_manifest(self):
        payload = (IMPORTED / "environment.aetex").read_bytes()
        magic, version, width, height, encoding, mip_count, data_size = struct.unpack_from(
            "<6IQ", payload)
        self.assertEqual((magic, version), (0x58544541, 1))
        self.assertEqual([width, height], self.manifest["environment"]["resolution"])
        self.assertEqual(width, height * 2)
        self.assertEqual(mip_count, width.bit_length())
        expected_bytes = sum(max(1, width >> level) * max(1, height >> level) * 4
                             for level in range(mip_count))
        self.assertEqual((data_size, len(payload)), (expected_bytes, 32 + expected_bytes))

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
