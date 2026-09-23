import importlib.util
import math
import pathlib
import unittest

import numpy as np


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "cook_sky_panorama", ROOT / "tools" / "cook-sky-panorama.py")
COOKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COOKER)


class EnvironmentShTests(unittest.TestCase):
    def test_constant_hdr_radiance_integrates_to_pi_radiance(self):
        radiance = np.array([0.25, 2.0, 11.0], dtype=np.float32)
        source = np.broadcast_to(radiance, (64, 128, 3)).copy()
        coefficients = COOKER.diffuse_irradiance_sh9(source)
        for direction in ((0, 1, 0), (1, 0, 0), (0, -1, 0), (1, 2, 3)):
            actual = COOKER.evaluate_diffuse_irradiance_sh9(coefficients, direction)
            np.testing.assert_allclose(actual, radiance * math.pi, rtol=2e-4, atol=2e-4)

    def test_upper_hdr_hemisphere_has_expected_lambert_response(self):
        height, width = 128, 256
        source = np.zeros((height, width, 3), dtype=np.float32)
        source[:height // 2, :, 0] = 3.5
        coefficients = COOKER.diffuse_irradiance_sh9(source)
        up = COOKER.evaluate_diffuse_irradiance_sh9(coefficients, (0, 1, 0))[0]
        side = COOKER.evaluate_diffuse_irradiance_sh9(coefficients, (1, 0, 0))[0]
        down = COOKER.evaluate_diffuse_irradiance_sh9(coefficients, (0, -1, 0))[0]
        self.assertAlmostEqual(up, 3.5 * math.pi, delta=0.003)
        self.assertAlmostEqual(side, 3.5 * math.pi * 0.5, delta=0.003)
        self.assertAlmostEqual(down, 0.0, delta=0.003)

    def test_projection_rejects_invalid_source_instead_of_baking_fake_light(self):
        source = np.ones((4, 8, 3), dtype=np.float32)
        source[0, 0, 0] = np.nan
        with self.assertRaises(ValueError):
            COOKER.diffuse_irradiance_sh9(source)


if __name__ == "__main__":
    unittest.main()
