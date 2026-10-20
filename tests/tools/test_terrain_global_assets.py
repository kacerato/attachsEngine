"""Original Geo append integrity and logical-icon/physical-atlas contracts."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class TerrainGlobalAssets(unittest.TestCase):
    def test_append_original_pair_preserves_existing_catalog_and_is_idempotent(self):
        from PIL import Image
        library = ROOT / "assets/terrain/library"
        current = json.loads((library / "catalog.json").read_text(encoding="utf-8"))
        pair = current["textures"][-2:]
        self.assertEqual([t["guid"] for t in pair], ["863f19b183b2443859f959a96f7dce32", "192aaea3237044f608d663c9884e0c70"])
        initial = dict(current, textures=current["textures"][:-2])
        self.assertEqual(len(initial["textures"]), 146)
        self.assertEqual(len(initial["materials"]), 82)
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            package, output = folder / "package", folder / "library"
            output.mkdir()
            (output / "catalog.json").write_text(json.dumps(initial), encoding="utf-8")
            entries = []
            for texture in pair:
                raw = (library / texture["source"]).read_bytes()
                source = package / texture["source_path"]
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(raw)
                entries.append(dict(path=texture["source_path"], guid=texture["guid"], sha256=texture["id"], bytes=len(raw)))
            (package / "manifest.json").write_text(json.dumps(dict(package=pair[0]["package"], sha256=pair[0]["archive_sha256"], entries=entries)), encoding="utf-8")
            command = [sys.executable, str(ROOT / "tools/prepare-terrain-global-textures.py"), "--package", str(package), "--output", str(output)]
            subprocess.run(command, check=True, capture_output=True)
            after = json.loads((output / "catalog.json").read_text(encoding="utf-8"))
            self.assertEqual(after, current)
            before = {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()}
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(before, {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()})
            for texture in pair:
                self.assertEqual(hashlib.sha256((output / texture["source"]).read_bytes()).hexdigest(), texture["id"])
                with Image.open(output / texture["source"]) as image:
                    self.assertEqual(image.size, (1, 256))
                with Image.open(output / texture["thumbnail"]) as image:
                    self.assertEqual(image.size, (1, 160))

    def test_second_original_conflict_does_not_publish_first_pair_member(self):
        library = ROOT / "assets/terrain/library"
        current = json.loads((library / "catalog.json").read_text(encoding="utf-8"))
        pair = current["textures"][-2:]
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            package, output = folder / "package", folder / "library"
            output.mkdir()
            (output / "catalog.json").write_text(json.dumps(dict(current, textures=current["textures"][:-2])), encoding="utf-8")
            entries = []
            for texture in pair:
                raw = (library / texture["source"]).read_bytes()
                source = package / texture["source_path"]
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(raw)
                entries.append(dict(path=texture["source_path"], guid=texture["guid"], sha256=texture["id"], bytes=len(raw)))
            (package / "manifest.json").write_text(json.dumps(dict(package=pair[0]["package"], sha256=pair[0]["archive_sha256"], entries=entries)), encoding="utf-8")
            conflict = output / pair[1]["source"]
            conflict.parent.mkdir(parents=True, exist_ok=True)
            conflict.write_bytes(b"conflicting original")
            before = {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()}
            result = subprocess.run([sys.executable, str(ROOT / "tools/prepare-terrain-global-textures.py"), "--package", str(package), "--output", str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(before, {p.relative_to(output): (p.read_bytes(), p.stat().st_mtime_ns) for p in output.rglob("*") if p.is_file()})

    def test_appended_icons_keep_ids_and_matching_physical_rectangles(self):
        catalog = json.loads((ROOT / "assets/astra-visual/icons/named/catalog.json").read_text(encoding="utf-8"))["icons"]
        header = (ROOT / "native/ui/ui_icon_id.h").read_text(encoding="utf-8")
        entries = re.findall(r"^\s+(\w+) = (\d+),\s*// (.+)$", header, re.M)
        ids = {identifier: int(value) for identifier, value in re.findall(r"^\s+(\w+) = (\d+),", header, re.M)}
        named = {name: int(value) for _, value, name in entries}
        # Compare the historical contract as well as the explicit brand/new IDs.
        old_header = subprocess.check_output(["git", "show", "HEAD:native/ui/ui_icon_id.h"], cwd=ROOT, text=True, encoding="utf-8")
        old_ids = {name: int(value) for name, value in re.findall(r"^\s+(\w+) = (\d+),", old_header, re.M)}
        for identifier, value in old_ids.items():
            self.assertEqual(ids.get(identifier), value, identifier)
        self.assertEqual([ids[k] for k in ("BrandWordmark", "BrandMark", "ZzTerrainModuleGlobalMaps", "ZzTerrainModuleStrata")], [478, 479, 480, 481])
        binary = (ROOT / "assets/astra-visual/ui/astra-ui-icons.aeui").read_bytes()
        magic, version, width, height, count = struct.unpack_from("<5I", binary)
        self.assertEqual((magic, version, count), (0x49554541, 1, 481))
        self.assertEqual(len(binary), 20 + count * 8 + width * height * 4)
        for physical, name in enumerate(sorted(catalog)):
            rect = struct.unpack_from("<4H", binary, 20 + (named[name] - 1) * 8)
            self.assertEqual(rect, (2 + (physical % (2048 // 98)) * 98, 2 + (physical // (2048 // 98)) * 98, 96, 96), name)
        # Brands retain their separately packed wide/square rectangles, not a grid cell.
        for name in ("brand/wordmark", "brand/mark"):
            x, y, w, h = struct.unpack_from("<4H", binary, 20 + (named[name] - 1) * 8)
            self.assertGreater(w, 0)
            self.assertGreater(h, 0)
            self.assertLessEqual(x + w, width)
            self.assertLessEqual(y + h, height)
        self.assertEqual({name for name, meta in catalog.items() if meta.get("append_after_brand")}, {"zz-terrain-module/global-maps", "zz-terrain-module/strata"})


if __name__ == "__main__":
    unittest.main()
