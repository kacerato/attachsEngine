"""No production assets are touched: only temporary generated-output fixtures."""
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from generated_output import publish_staged_if_changed, write_text_if_changed


class GeneratedOutputTests(unittest.TestCase):
    def test_normalized_text_retains_bytes_and_mtime(self):
        with tempfile.TemporaryDirectory() as folder:
            destination = Path(folder) / "generated.h"
            original = b"// generated\r\nvalue\r\n"
            destination.write_bytes(original)
            os.utime(destination, ns=(1000000000, 2000000000))
            stamp = destination.stat().st_mtime_ns
            for _ in range(2):
                self.assertFalse(write_text_if_changed(destination, "// generated\nvalue\n"))
                self.assertEqual(destination.read_bytes(), original)
                self.assertEqual(destination.stat().st_mtime_ns, stamp)
            self.assertTrue(write_text_if_changed(destination, "// changed\n"))
            self.assertEqual(destination.read_bytes(), b"// changed\n")
            self.assertEqual(list(Path(folder).glob("*.tmp")), [])

    def test_binary_noop_removes_stage_and_retains_destination(self):
        with tempfile.TemporaryDirectory() as folder:
            destination = Path(folder) / "atlas.aeui"
            staged = Path(folder) / "atlas.tmp.aeui"
            # Span the comparison block boundary and exercise same-size changes.
            original = b"a" * (1024 * 1024 + 8)
            destination.write_bytes(original)
            os.utime(destination, ns=(1000000000, 2000000000))
            stamp = destination.stat().st_mtime_ns
            for _ in range(2):
                staged.write_bytes(original)
                self.assertFalse(publish_staged_if_changed(staged, destination))
                self.assertFalse(staged.exists())
                self.assertEqual(destination.stat().st_mtime_ns, stamp)
            changed = original[:-1] + b"b"
            staged.write_bytes(changed)
            self.assertTrue(publish_staged_if_changed(staged, destination))
            self.assertEqual(destination.read_bytes(), changed)
            self.assertFalse(staged.exists())

    def test_first_publication_creates_both_output_kinds(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.assertTrue(write_text_if_changed(root / "new.h", "header\n"))
            staged = root / "new.tmp.png"
            staged.write_bytes(b"png fixture")
            self.assertTrue(publish_staged_if_changed(staged, root / "new.png"))
            self.assertEqual((root / "new.png").read_bytes(), b"png fixture")


if __name__ == "__main__":
    unittest.main()
