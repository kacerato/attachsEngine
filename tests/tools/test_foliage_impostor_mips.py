"""Regressoes do atlas de impostores: tiles nao podem virar placas nem vazar."""

import importlib.util
import pathlib
import unittest

import numpy as np


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "bake_foliage_impostors", ROOT / "tools/bake-foliage-impostors.py"
)
BAKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BAKER)


class ImpostorMipChainTest(unittest.TestCase):
    def test_cobertura_de_um_quarto_para_em_8x8(self):
        atlas = np.zeros((32, 32, 4), dtype=np.float32)
        atlas[:, :8, :3] = [0.1, 0.7, 0.2]
        atlas[:, :8, 3] = 1.0

        levels = BAKER.build_mip_chain(atlas, tile_size=32, minimum_tile=8)

        self.assertEqual([(width, height) for width, height, _ in levels],
                         [(32, 32), (16, 16), (8, 8)])
        tail = np.frombuffer(levels[-1][2], dtype=np.uint8).reshape(8, 8, 4)
        coverage = float((tail[..., 3] >= 128).mean())
        self.assertAlmostEqual(coverage, 0.25)

    def test_tile_vazio_permanece_transparente_e_sem_cor(self):
        atlas = np.zeros((16, 32, 4), dtype=np.float32)
        atlas[:, :8, :3] = [1.0, 1.0, 1.0]  # RGB invisivel no primeiro tile.
        atlas[0:8, 8:16, :3] = [0.2, 0.6, 0.1]
        atlas[0:8, 8:16, 3] = 1.0

        levels = BAKER.build_mip_chain(atlas, tile_size=16, minimum_tile=8)

        tail = np.frombuffer(levels[-1][2], dtype=np.uint8).reshape(8, 16, 4)
        empty = tail[:, 8:16]
        self.assertTrue(np.all(empty == 0))

    def test_parametros_invalidos_falham_explicitamente(self):
        atlas = np.zeros((16, 16, 4), dtype=np.float32)
        with self.assertRaises(ValueError):
            BAKER.build_mip_chain(atlas, tile_size=10, minimum_tile=5)
        with self.assertRaises(ValueError):
            BAKER.build_mip_chain(atlas, tile_size=16, minimum_tile=6)

    def test_normal_mips_preservam_direcao_cobertura_e_formato_linear(self):
        atlas = np.zeros((16, 16, 4), dtype=np.float32)
        atlas[:, :8, :3] = [0.0, 1.0, 0.0]
        atlas[:, :8, 3] = 1.0

        levels = BAKER.build_normal_mip_chain(atlas, tile_size=16, minimum_tile=4)

        self.assertEqual([(width, height) for width, height, _ in levels],
                         [(16, 16), (8, 8), (4, 4)])
        tail = np.frombuffer(levels[-1][2], dtype=np.uint8).reshape(4, 4, 4)
        self.assertAlmostEqual(float((tail[..., 3] >= 128).mean()), 0.5)
        covered = tail[..., 3] >= 128
        decoded = tail[covered, :3].astype(np.float32) / 255.0 * 2.0 - 1.0
        decoded /= np.maximum(np.linalg.norm(decoded, axis=1, keepdims=True), 1e-8)
        self.assertTrue(np.all(decoded[:, 1] > 0.999))
        self.assertTrue(np.all(np.abs(decoded[:, (0, 2)]) < 0.01))

    def test_normal_tile_vazio_codifica_neutro_com_alpha_zero(self):
        atlas = np.zeros((16, 16, 4), dtype=np.float32)
        levels = BAKER.build_normal_mip_chain(atlas, tile_size=16, minimum_tile=4)
        base = np.frombuffer(levels[0][2], dtype=np.uint8).reshape(16, 16, 4)
        self.assertTrue(np.all(base[..., 3] == 0))
        self.assertTrue(np.all(base[..., 0:2] == 128))
        self.assertTrue(np.all(base[..., 2] == 255))


if __name__ == "__main__":
    unittest.main()
