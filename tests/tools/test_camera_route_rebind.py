import importlib.util
import pathlib
import struct
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "rebind_camera_route", ROOT / "tools" / "rebind-camera-route.py")
route = importlib.util.module_from_spec(spec)
spec.loader.exec_module(route)


def fixture(fingerprint=0x1122334455667788, count=2):
    header = struct.pack("<4IQ2I", route.ROUTE_MAGIC, route.ROUTE_VERSION,
                         route.ROUTE_HEADER_SIZE, 0, fingerprint, 120, count)
    return header + bytes(count * route.ROUTE_SAMPLE_SIZE)


class CameraRouteRebindTests(unittest.TestCase):
    def test_changes_only_scene_identity_and_preserves_every_sample_byte(self):
        source = fixture()
        rebound, report = route.rebind_route(source, 0x1122334455667788,
                                             0x8877665544332211)
        self.assertEqual(rebound[:16], source[:16])
        self.assertEqual(rebound[24:], source[24:])
        self.assertEqual(struct.unpack_from("<Q", rebound, 16)[0], 0x8877665544332211)
        self.assertEqual(report["tickCount"], 2)

    def test_rejects_wrong_source_or_malformed_length(self):
        with self.assertRaisesRegex(ValueError, "source fingerprint"):
            route.rebind_route(fixture(), 1, 2)
        with self.assertRaisesRegex(ValueError, "length"):
            route.rebind_route(fixture()[:-1], 0x1122334455667788, 2)


if __name__ == "__main__":
    unittest.main()
