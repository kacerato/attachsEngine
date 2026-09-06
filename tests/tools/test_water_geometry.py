import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from water_geometry import graded_water_axis


class WaterGeometryTests(unittest.TestCase):
    def test_extent_symmetry_and_near_density(self):
        axis, spacing = graded_water_axis(256, 384, 8000)
        self.assertEqual((axis[0], axis[-1], axis[128]), (-8000, 8000, 0))
        for i in range(257):
            self.assertAlmostEqual(axis[i], -axis[256-i])
            self.assertGreater(spacing[i], 0)
        for i in range(65, 192):
            self.assertEqual(spacing[i], 3)
        self.assertTrue(all(a < b for a, b in zip(axis, axis[1:])))

    def test_spacing_bounds_every_incident_edge(self):
        axis, spacing = graded_water_axis(64, 128, 4000)
        for i in range(64):
            edge = axis[i+1]-axis[i]
            self.assertGreaterEqual(spacing[i], edge)
            self.assertGreaterEqual(spacing[i+1], edge)

    def test_rejects_invalid_authoring(self):
        for args in [(7, 384, 8000), (15, 384, 8000), (64, 0, 10),
                     (64, 100, 10), (64, 1, float("nan"))]:
            with self.assertRaises(ValueError):
                graded_water_axis(*args)


if __name__ == "__main__":
    unittest.main()
