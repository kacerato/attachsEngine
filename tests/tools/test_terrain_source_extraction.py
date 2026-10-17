"""Archive ordering/header regressions and extraction boundary, without assets."""
import gzip
import importlib.util
import io
import json
import tarfile
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('terrain_extract', Path(__file__).resolve().parents[2] / 'tools/extract-terrain-packages.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def package(entries):
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode='w') as archive:
        for index, (name, data) in enumerate(entries):
            guid = f'{index + 1:032x}'
            for kind, content in [('asset', data), ('pathname', name.encode()), ('asset.meta', b'guid: ' + guid.encode())]:
                member = tarfile.TarInfo(guid + '/' + kind)
                member.size = len(content)
                archive.addfile(member, io.BytesIO(content))
    data = gzip.compress(raw.getvalue())
    # Unity packages use optional gzip header fields, which tarfile's r|gz
    # decoder mishandles on Python 3.12. gzip.GzipFile must decode this header.
    return data[:3] + bytes([data[3] | 4]) + data[4:10] + b'\x04\x00UE00' + data[10:]


class TerrainSourceExtraction(unittest.TestCase):
    def test_selective_and_nested_sources_with_asset_before_pathname(self):
        nested = package([('Assets/Inner.cs', b'class Inner {}')])
        data = package([('Assets/Source.cs', b'class Source {}'), ('Assets/Large.png', b'not decoded'),
                        ('Assets/Nested.unitypackage', nested)])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'out'
            MODULE.source_extract(io.BytesIO(data), root, 'fixture.unitypackage')
            manifest = json.loads((root / 'source-manifest.json').read_text(encoding='utf-8'))
            self.assertEqual((root / 'Assets/Source.cs').read_bytes(), b'class Source {}')
            self.assertFalse((root / 'Assets/Large.png').exists())
            self.assertFalse((root / 'Assets/Nested.unitypackage').exists())
            self.assertFalse((root / '.entries').exists())
            self.assertFalse(manifest['entries'][1]['extracted'])
            child = root / manifest['entries'][2]['nested_sources']
            self.assertEqual((child / 'Assets/Inner.cs').read_bytes(), b'class Inner {}')
            self.assertTrue((child / 'Assets/Inner.cs.meta').exists())

    def test_traversal_is_rejected_before_output_is_written(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'out'
            for name in ('../escape.cs', 'Assets/../../escape.cs', '/escape.cs', 'C:/escape.cs'):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    MODULE.source_extract(io.BytesIO(package([(name, b'class Unsafe {}')])), root, 'fixture')
                self.assertFalse(root.exists())


if __name__ == '__main__':
    unittest.main()
