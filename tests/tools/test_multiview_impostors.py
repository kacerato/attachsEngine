import importlib.util
import pathlib
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('baker', ROOT / 'tools/bake-foliage-impostors.py')
BAKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BAKER)


class MultiviewTests(unittest.TestCase):
    def test_raster_preserves_linear_material_tint_and_world_normal(self):
        positions = np.array([[-1,-1,0],[1,-1,0],[0,1,0]], dtype=float)
        normals = np.tile([0.,1.,0.], (3,1))
        colors = np.tile([0.25,0.5,0.75,1.], (3,1))
        albedo = np.ones((4,4,4))
        color, normal = BAKER.rasterize_impostor(
            positions, normals, np.zeros((3,2)), np.array([[0,1,2]]), albedo,
            0.5, 16, np.array([1.,0,0]), np.array([0.,1,0]),
            np.array([0.,0,1]), 1, 2, colors, np.array([0.5,0.5,0.5,1]))
        covered = color[...,3] > 0.5
        self.assertTrue(covered.any())
        expected = BAKER.linear_to_srgb(np.array([0.125,0.25,0.375]))
        np.testing.assert_allclose(color[covered,:3], np.tile(expected,(covered.sum(),1)), atol=1e-6)
        np.testing.assert_allclose(normal[covered,:3], np.tile([0.,1.,0.],(covered.sum(),1)), atol=1e-6)

    def test_color_mips_filter_in_linear_space(self):
        atlas = np.ones((16,16,4), dtype=np.float32)
        atlas[:,::2,:3] = 0
        levels = BAKER.build_mip_chain(atlas,16,8)
        tail = np.frombuffer(levels[-1][2],dtype=np.uint8).reshape(8,8,4)
        self.assertTrue(np.all(np.abs(tail[...,:3].astype(int)-188)<=1))

    def test_rebake_preserves_source_geometry_and_material_prefix(self):
        package = BAKER.Package((ROOT/'samples/dirt-road/Imported/scene.aemap').read_bytes())
        old_vertices = bytes(package.vertices)
        old_materials = list(package.materials)
        package.remove_baked_suffix()
        self.assertEqual(bytes(package.vertices), old_vertices[:len(package.vertices)])
        self.assertEqual(package.materials, old_materials[:len(package.materials)])
        self.assertFalse(any(package.material_flags(i)&BAKER.MATERIAL_IMPOSTOR
                             for i in range(package.material_count)))
        roundtrip = BAKER.Package(package.serialize())
        self.assertEqual(roundtrip.vertex_count, package.vertex_count)
        self.assertEqual(roundtrip.indices, package.indices)


if __name__ == '__main__':
    unittest.main()
