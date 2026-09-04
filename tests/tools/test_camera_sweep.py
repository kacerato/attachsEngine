import importlib.util
import math
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('sweep', ROOT / 'tools/generate-camera-sweep.py')
SWEEP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SWEEP)


class CameraSweepTests(unittest.TestCase):
    def test_runtime_header_and_closed_segment_boundaries(self):
        a, b = [0, 1, 2, 0, 0], [10, 4, 2, 1, .5]
        data = SWEEP.encode_sweep([a, b], 100, 120, 123)
        self.assertEqual(struct.unpack_from('<4IQ2I', data), (0x54524541, 1, 32, 0, 123, 120, 200))
        self.assertEqual(len(data), 32 + 200 * 20)
        self.assertEqual(struct.unpack_from('<5f', data, 32), tuple(a))
        self.assertEqual(struct.unpack_from('<5f', data, 32 + 100 * 20), tuple(b))
        self.assertLess(abs(struct.unpack_from('<5f', data, len(data)-20)[0]), .004)

    def test_yaw_takes_short_path_across_wrap(self):
        data = SWEEP.encode_sweep([[0,0,0,3.1,0], [0,0,0,-3.1,0]], 10, 120, 1)
        midpoint = struct.unpack_from('<5f', data, 32 + 5 * 20)
        self.assertAlmostEqual(midpoint[3], math.pi, places=5)

    def test_invalid_input_is_not_published(self):
        for poses, frames in [([[0]*5], 10), ([[0]*5]*2, 0),
                              ([[0]*5]*2, 200000), ([[math.nan]*5]*2, 10)]:
            with self.assertRaises(ValueError):
                SWEEP.encode_sweep(poses, frames, 120, 1)


if __name__ == '__main__':
    unittest.main()
