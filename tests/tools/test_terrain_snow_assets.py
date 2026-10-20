"""Stage actual supplied snow originals without touching the production catalog."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("snow_textures", ROOT / "tools/prepare-terrain-snow-textures.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PACKAGE = ROOT / "build/terrain-packages/MicroSplat - Terrain Collection v3.9.41"


@unittest.skipUnless((PACKAGE / "manifest.json").exists(), "requires actual supplied package")
class TerrainSnowAssets(unittest.TestCase):
    def test_real_originals_lossless_roles_idempotency_and_existing_core_dedup(self):
        live = ROOT / "assets/terrain/library/catalog.json"
        live_before = live.read_bytes()
        build = ROOT / "build"
        build.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="snow-assets-test-", dir=build) as folder:
            output = Path(folder)
            records = MODULE.prepare(PACKAGE, output)
            self.assertEqual(len(records), 4)
            for record in records:
                source = (PACKAGE / record["source_path"]).read_bytes()
                self.assertEqual((output / record["source"]).read_bytes(), source)
                self.assertEqual(hashlib.sha256(source).hexdigest(), record["id"])
                runtime = (output / record["runtime"]).read_bytes()
                self.assertEqual(hashlib.sha256(runtime).hexdigest(), record["runtime_id"])
                self.assertEqual(record["runtime_id"], record["runtime_sha256"])
                self.assertEqual(record["profiles"], {})
                with Image.open(PACKAGE / record["source_path"]) as original, Image.open(output / record["runtime"]) as png:
                    self.assertEqual(png.mode, "RGBA")
                    self.assertEqual(png.size, original.size)
                    self.assertEqual(png.tobytes(), original.convert("RGBA").tobytes())
                with Image.open(output / record["thumbnail"]) as thumbnail:
                    self.assertEqual(thumbnail.size, (160, 160))
            normal = records[1]
            self.assertEqual(normal["packing"], "normal=GA;smoothness=R;ao=B")
            self.assertTrue(normal["normal_ga_raw_preserved"])
            self.assertEqual(records[0]["source_roles"], {"albedo": "RGB/sRGB", "height": "A/linear"})
            def snapshot():
                return {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()}
            before = snapshot()
            MODULE.prepare(PACKAGE, output)
            self.assertEqual(before, snapshot(), "second run writes nothing")
            # Simulate core Perlin already supplied by another importer.
            catalog_path = output / "catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            core = dict(records[-1])
            for key in ("runtime_id", "source_roles", "packing", "normal_ga_raw_preserved"):
                core.pop(key)
            catalog["textures"] = [core]
            catalog["materials"] = [{"id": "retained-material", "provenance": {"unsupported": 7}}]
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            MODULE.prepare(PACKAGE, output)
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            self.assertEqual(len(catalog["textures"]), 4)
            self.assertEqual(catalog["textures"][0]["guid"], records[-1]["guid"])
            self.assertEqual(catalog["materials"], [{"id": "retained-material", "provenance": {"unsupported": 7}}])
            # Conflicts refuse publication before changing any other output.
            (output / normal["runtime"]).write_bytes(b"conflicting asset")
            before = snapshot()
            with self.assertRaisesRegex(ValueError, "Existing asset differs"):
                MODULE.prepare(PACKAGE, output)
            self.assertEqual(before, snapshot())
        self.assertEqual(live.read_bytes(), live_before, "production catalog remains frozen")


if __name__ == "__main__":
    unittest.main()
