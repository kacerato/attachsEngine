"""M13-M15 actual-source staging contracts; no production catalog mutation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("flow_assets", ROOT / "tools/prepare-terrain-flow-textures.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PACKAGE = ROOT / "build/terrain-packages/MicroSplat - Terrain Collection v3.9.41"


@unittest.skipUnless((PACKAGE / "manifest.json").exists(), "requires supplied originals")
class TerrainFlowAssets(unittest.TestCase):
    def test_original_bytes_lossless_rgba_packing_and_noop(self):
        live = ROOT / "assets/terrain/library/catalog.json"
        live_before = live.read_bytes()
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="flow-assets-", dir=ROOT / "build") as temporary:
            output = Path(temporary)
            records = MODULE.prepare(PACKAGE, output)
            self.assertEqual(len(records), 4)
            for record in records:
                original = (PACKAGE / record["source_path"]).read_bytes()
                self.assertEqual((output / record["source"]).read_bytes(), original)
                self.assertEqual(hashlib.sha256(original).hexdigest(), record["id"])
                runtime = (output / record["runtime"]).read_bytes()
                self.assertEqual(hashlib.sha256(runtime).hexdigest(), record["runtime_id"])
                self.assertEqual(record["runtime_id"], record["runtime_sha256"])
                self.assertEqual(record["profiles"], {})
                self.assertEqual(record["color_space"], "linear")
                self.assertTrue(record["raw_channels_preserved"])
                with Image.open(PACKAGE / record["source_path"]) as source, Image.open(output / record["runtime"]) as png:
                    self.assertEqual(png.size, source.size)
                    self.assertEqual(png.mode, "RGBA")
                    self.assertEqual(png.tobytes(), source.convert("RGBA").tobytes())
                with Image.open(output / record["thumbnail"]) as thumb:
                    self.assertEqual(thumb.size, (160, 160))
            self.assertEqual(records[0]["source_roles"], {"normal": "RG/linear/raw-packed", "foam": "B/linear", "stream_normal_foam": "RGB/linear/raw-packed"})
            self.assertEqual(records[1]["source_roles"]["ripple_direction"], "GB/linear")
            self.assertEqual(records[2]["source_roles"]["crust_darkening"], "A/linear")
            def snapshot():
                return {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()}
            before = snapshot()
            MODULE.prepare(PACKAGE, output)
            self.assertEqual(before, snapshot(), "second run writes nothing")
            catalog_path = output / "catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["materials"] = [{"id": "preserved", "unknown": 7}]
            catalog["textures"] = [records[0]]
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            MODULE.prepare(PACKAGE, output)
            after = json.loads(catalog_path.read_text(encoding="utf-8"))
            self.assertEqual(len(after["textures"]), 4)
            self.assertEqual(after["materials"], catalog["materials"])
            # A GUID collision is rejected without changing any output.
            after["textures"][0]["id"] = "conflicting-content"
            catalog_path.write_text(json.dumps(after), encoding="utf-8")
            before = snapshot()
            with self.assertRaisesRegex(ValueError, "Conflicting source identity"):
                MODULE.prepare(PACKAGE, output)
            self.assertEqual(before, snapshot())
            # A file collision also refuses the whole append transaction.
            catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
            (output / records[-1]["runtime"]).write_bytes(b"conflict")
            before = snapshot()
            with self.assertRaisesRegex(ValueError, "Existing asset differs"):
                MODULE.prepare(PACKAGE, output)
            self.assertEqual(before, snapshot())
        self.assertEqual(live.read_bytes(), live_before)


if __name__ == "__main__":
    unittest.main()
