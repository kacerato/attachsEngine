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
