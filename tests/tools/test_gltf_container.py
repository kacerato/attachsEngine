"""Contrato unico de entrada para glTF ZIP e GLB."""

import importlib.util
import io
import json
import pathlib
import struct
import unittest
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("cook", ROOT / "tools/cook-gltf-map.py")
COOK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COOK)


def make_glb(document, binary):
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * ((-len(encoded)) & 3)
    binary += b"\0" * ((-len(binary)) & 3)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    return (struct.pack("<III", 0x46546C67, 2, total) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(binary), 0x004E4942) + binary)


class GlbSourceNormalizationTest(unittest.TestCase):
    def test_imagem_embutida_vira_uri_no_zip_interno(self):
        image = b"not-a-real-png"
        document = {
            "asset": {"version": "2.0"},
            "buffers": [{"byteLength": len(image)}],
            "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(image)}],
            "images": [{"bufferView": 0, "mimeType": "image/png"}],
        }

        normalized = COOK.glb_as_source_zip(make_glb(document, image), b"license")

        with zipfile.ZipFile(io.BytesIO(normalized)) as archive:
            self.assertEqual(archive.read("image_000.png"), image)
            self.assertEqual(archive.read("license.txt"), b"license")
            gltf = json.loads(archive.read("scene.gltf"))
            self.assertEqual(gltf["buffers"][0]["uri"], "scene.bin")
            self.assertEqual(gltf["images"][0], {"uri": "image_000.png"})

    def test_multiplos_buffers_falham_antes_do_cozimento(self):
        document = {
            "asset": {"version": "2.0"},
            "buffers": [{"byteLength": 0}, {"byteLength": 0}],
        }
        with self.assertRaisesRegex(ValueError, "mais de um buffer"):
            COOK.glb_as_source_zip(make_glb(document, b""), b"license")


if __name__ == "__main__":
    unittest.main()


class AuthoringSceneTest(unittest.TestCase):
    def test_trs_parent_and_child_world(self):
        doc={"scenes":[{"nodes":[0]}],"nodes":[{"translation":[10,0,0],"scale":[2,2,2],"children":[1]}, {"translation":[1,3,0]}]}
        nodes=COOK.scene_nodes(doc)
        self.assertEqual(nodes[1][1][12:15].tolist(), [12,6,0])

    def test_shared_mesh_has_independent_objects_and_derived_draws(self):
        doc={"scenes":[{"nodes":[0,1]}],"nodes":[{"name":"Tree A","mesh":0},{"name":"Tree B","mesh":0}],"meshes":[{"name":"Tree","primitives":[{}]}]}
        catalog=COOK.authoring_catalog(doc,"forest",{0:[0,1,2],1:[3,4,5]})
        a,b=catalog["objects"]
        self.assertNotEqual(a["id"],b["id"])
        self.assertEqual(a["mesh"],b["mesh"])
        self.assertEqual(a["draws"],[0,1,2])
        self.assertEqual(len(catalog["meshes"]),1)
        self.assertEqual(a["id"],COOK.authoring_catalog(doc,"forest",{})["objects"][0]["id"])

    def test_cycle_rejected(self):
        with self.assertRaises(ValueError):
            COOK.scene_nodes({"scenes":[{"nodes":[0]}],"nodes":[{"children":[0]}]})
