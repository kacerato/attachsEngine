import importlib.util
import pathlib
import struct
import unittest
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('map_cooker', ROOT/'tools/cook-gltf-map.py')
cooker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cooker)


class AssetAccessorTests(unittest.TestCase):
    def read(self, payload, component, kind='SCALAR', count=3, normalized=False):
        gltf = {'bufferViews': [{}], 'accessors': [{
            'bufferView': 0, 'componentType': component, 'type': kind,
            'count': count, 'normalized': normalized}]}
        return cooker.accessor(gltf, payload, 0)

    def test_unsigned_short_indices(self):
        np.testing.assert_array_equal(self.read(struct.pack('<3H',0,4096,65535),5123),[0,4096,65535])

    def test_normalized_signed_attribute(self):
        np.testing.assert_allclose(self.read(struct.pack('<3h',-32768,0,32767),5122,normalized=True),[-1,0,1])

    def test_normalized_unsigned_attribute(self):
        np.testing.assert_allclose(self.read(bytes([0,128,255]),5121,normalized=True),[0,128/255,1])

    def test_normalized_index_type_rejected(self):
        with self.assertRaises(ValueError):
            self.read(struct.pack('<3I',0,1,2),5125,normalized=True)
