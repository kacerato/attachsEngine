"""Mips que preservam cobertura para materiais alpha-tested.

Um box filter preserva a MEDIA do alpha, não a fração de texels acima do
cutoff -- e é só essa fração que o runtime enxerga num material alpha-tested.
Numa cadeia de mips de folhagem as duas divergem rápido: medido no cooker, a
cobertura de um atlas de galhos cai a **zero** por volta do mip 5, ou seja, a
vegetação desaparece com a distância.

Estes testes exercitam as funções puras (sem astcenc, sem glTF, sem aparelho).
"""
import importlib.util, pathlib, unittest
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("cook", ROOT / "tools/cook-gltf-map.py")
cook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cook)

CUTOFF = 0.5


def rgba_from_alpha(alpha):
    image = np.zeros(alpha.shape + (4,), dtype=np.float32)
    image[..., :3] = 0.3
    image[..., 3] = alpha
    return image


def antialiased_foliage(seed=11, size=256, blobs=26):
    """Folhas com borda anti-aliased: histograma de alpha fino, como um atlas
    real exportado de um renderizador. O caso binário puro existe no teste do
    pior caso, separado, porque tem comportamento de quantização diferente."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    alpha = np.zeros((size, size), dtype=np.float32)
    for _ in range(blobs):
        cy, cx = rng.uniform(20, size - 20), rng.uniform(20, size - 20)
        radius = rng.uniform(9, 22)
        distance = np.sqrt((yy - cy) ** 2 + ((xx - cx) * 2.1) ** 2)
        alpha = np.maximum(alpha, np.clip((radius - distance) / 2.5, 0, 1))
    return alpha


def binary_branches(seed=7, size=256, strokes=40):
    rng = np.random.default_rng(seed)
    alpha = np.zeros((size, size), dtype=np.float32)
    for _ in range(strokes):
        y, x = rng.integers(0, size - 6), rng.integers(0, size - 6)
        alpha[y:y + 3, x:x + 90] = 1.0
    return alpha


class AlphaCoverageTest(unittest.TestCase):
    def test_coverage_conta_texels_nao_media_de_alpha(self):
        alpha = np.array([[0.0, 0.4], [0.6, 1.0]], dtype=np.float32)
        self.assertAlmostEqual(cook.alpha_coverage(alpha, CUTOFF), 0.5)
        # A média é 0,5 e a cobertura também aqui por coincidência; o que o
        # teste fixa é que alpha logo abaixo do cutoff não conta.
        self.assertAlmostEqual(cook.alpha_coverage(np.full((4, 4), 0.49, np.float32), CUTOFF), 0.0)
        self.assertAlmostEqual(cook.alpha_coverage(np.full((4, 4), 0.50, np.float32), CUTOFF), 1.0)

    def test_box_filter_sozinho_apaga_a_folhagem(self):
        """O defeito que motivou esta fatia, fixado como regressão."""
        image = rgba_from_alpha(antialiased_foliage())
        target = cook.alpha_coverage(image[..., 3], CUTOFF)
        current = image
        for _ in range(6):
            current = cook.halve(current)
        self.assertGreater(target, 0.05, "a fixture precisa ter cobertura real no mip 0")
        self.assertEqual(cook.alpha_coverage(current[..., 3], CUTOFF), 0.0,
                         "sem correção, o mip 6 perde toda a cobertura")

    def test_cadeia_corrigida_preserva_cobertura_ate_o_mip_pequeno(self):
        image = rgba_from_alpha(antialiased_foliage())
        target = cook.alpha_coverage(image[..., 3], CUTOFF)
        current = image
        for mip in range(1, 7):
            current = cook.halve(current)
            corrected = cook.scale_alpha_to_coverage(current[..., 3], CUTOFF, target)
            coverage = cook.alpha_coverage(corrected, CUTOFF)
            self.assertGreaterEqual(coverage, target * 0.95,
                                    f"mip {mip} perdeu cobertura: {coverage:.4f} < {target:.4f}")
            self.assertLessEqual(coverage, target * 1.35,
                                 f"mip {mip} inflou a cobertura: {coverage:.4f}")

    def test_nunca_devolve_menos_cobertura_que_o_alvo(self):
        """Erra sempre para o lado de preservar texels.

        Com alpha quase binário a cobertura é uma escada e o alvo cai entre dois
        degraus: nenhuma escala o atinge exatamente. Escolher o degrau de baixo
        removeria vegetação -- o defeito que esta função corrige.
        """
        image = rgba_from_alpha(binary_branches())
        target = cook.alpha_coverage(image[..., 3], CUTOFF)
        current = image
        for mip in range(1, 8):
            current = cook.halve(current)
            corrected = cook.scale_alpha_to_coverage(current[..., 3], CUTOFF, target)
            self.assertGreaterEqual(cook.alpha_coverage(corrected, CUTOFF), target,
                                    f"mip {mip} ficou abaixo do alvo")

    def test_cobertura_degenerada_sai_intacta(self):
        # Nada a preservar em textura totalmente transparente ou totalmente
        # opaca; escalar só amplificaria ruído.
        transparent = np.zeros((16, 16), dtype=np.float32)
        opaque = np.ones((16, 16), dtype=np.float32)
        for alpha, target in ((transparent, 0.0), (opaque, 1.0)):
            result = cook.scale_alpha_to_coverage(alpha, CUTOFF, target)
            np.testing.assert_array_equal(result, alpha)

    def test_filtro_alpha_weighted_nao_vaza_cor_do_fundo_transparente(self):
        image = np.zeros((2, 2, 4), dtype=np.float32)
        image[..., :3] = [1.0, 1.0, 1.0]
        image[0, 0] = [0.1, 0.6, 0.2, 1.0]

        reduced = cook.halve_alpha_weighted(image)

        np.testing.assert_allclose(reduced[0, 0, :3], [0.1, 0.6, 0.2], atol=1e-6)
        self.assertAlmostEqual(float(reduced[0, 0, 3]), 0.25)

    def test_tail_de_cobertura_inrepresentavel_e_rejeitado(self):
        target = 0.25
        self.assertTrue(cook.coverage_mip_is_representable(target, 5 / 16))
        self.assertFalse(cook.coverage_mip_is_representable(target, 2 / 4))
        self.assertFalse(cook.coverage_mip_is_representable(target, 1.0))


class CoverageSemanticsTest(unittest.TestCase):
    """A semântica vem do material, nunca do nome da textura ou da cena."""

    @staticmethod
    def gltf(materials):
        return {"materials": materials}

    @staticmethod
    def material(texture, alpha_mode="OPAQUE", cutoff=None):
        entry = {"pbrMetallicRoughness": {"baseColorTexture": {"index": texture}},
                 "alphaMode": alpha_mode}
        if cutoff is not None:
            entry["alphaCutoff"] = cutoff
        return entry

    def test_mask_explicito_carrega_cobertura(self):
        gltf = self.gltf([self.material(3, "MASK", 0.35)])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf), {3: 0.35})

    def test_mask_sem_cutoff_usa_o_padrao_do_gltf(self):
        gltf = self.gltf([self.material(0, "MASK")])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf), {0: 0.5})

    def test_opaque_e_blend_nao_carregam_cobertura(self):
        gltf = self.gltf([self.material(1, "OPAQUE"), self.material(2, "BLEND")])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf), {})

    def test_blend_reconhecido_como_atlas_de_recorte_carrega_cobertura(self):
        # Mesma promoção que material_record aplica: um atlas de folhas
        # exportado como BLEND continua sendo cobertura na prática.
        gltf = self.gltf([self.material(4, "BLEND", 0.4)])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf, inferred_cutouts={4}), {4: 0.4})

    def test_cutoffs_divergentes_na_mesma_textura_usam_o_menor(self):
        # Nenhuma cadeia de mips é correta para dois cutoffs. O menor preserva
        # mais texels: folhagem um pouco densa demais custa fragmentos, folhagem
        # de menos remove árvores.
        gltf = self.gltf([self.material(5, "MASK", 0.7), self.material(5, "MASK", 0.25)])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf), {5: 0.25})

    def test_material_sem_textura_base_e_ignorado(self):
        gltf = self.gltf([{"alphaMode": "MASK"}, {"pbrMetallicRoughness": {}, "alphaMode": "MASK"}])
        self.assertEqual(cook.coverage_alpha_cutoffs(gltf), {})


class CookTextureIntegrationTest(unittest.TestCase):
    """Fiação de cook_texture, sem astcenc: o encoder é substituído por um stub
    que devolve os bytes RGBA do nível, então o teste enxerga exatamente o que
    seria comprimido."""

    def setUp(self):
        self.captured = []
        self.original = cook.astc_level

        def stub(astcenc, cache, name, mip, rgba, srgb, quality, jobs):
            self.captured.append(rgba.copy())
            return rgba.tobytes()

        cook.astc_level = stub
        self.addCleanup(lambda: setattr(cook, "astc_level", self.original))

    def cook(self, alpha, coverage_cutoff):
        import io, tempfile
        from PIL import Image
        rgba = np.zeros(alpha.shape + (4,), dtype=np.uint8)
        rgba[..., :3] = 180
        rgba[..., 3] = np.rint(alpha * 255).astype(np.uint8)
        buffer = io.BytesIO()
        Image.fromarray(rgba, "RGBA").save(buffer, format="PNG")
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            return cook.cook_texture(None, path, path, "t", buffer.getvalue(), True, False,
                                     "-medium", 1, coverage_cutoff)

    def test_textura_de_cobertura_reporta_cobertura_por_mip_e_nao_toca_o_mip_zero(self):
        alpha = antialiased_foliage(size=64, blobs=8)
        width, height, levels, coverage_by_mip = self.cook(alpha, CUTOFF)
        self.assertEqual((width, height), (64, 64))
        self.assertEqual(len(coverage_by_mip), levels)
        target = cook.alpha_coverage(alpha, CUTOFF)
        self.assertAlmostEqual(coverage_by_mip[0], target, places=2,
                               msg="o mip 0 é a referência e não pode ser reescalado")
        for mip, coverage in enumerate(coverage_by_mip[1:], start=1):
            self.assertGreater(coverage, 0.0, f"mip {mip} perdeu toda a cobertura")

    def test_sem_semantica_de_cobertura_o_alpha_continua_box_filter_puro(self):
        # Estradas, decalques e superfícies opacas não podem mudar por causa
        # desta fatia: a correção é dirigida por semântica de material. A
        # referência é a cadeia box-filtrada calculada aqui, não outra execução
        # do próprio cooker -- comparar o cooker consigo mesmo não provaria nada.
        alpha = antialiased_foliage(size=64, blobs=8)
        _, _, levels, coverage_by_mip = self.cook(alpha, None)
        self.assertEqual(coverage_by_mip, [], "sem cutoff não há cobertura a reportar")

        quantized = np.rint(alpha * 255).astype(np.uint8).astype(np.float32) / 255.0
        reference = rgba_from_alpha(quantized)
        for mip in range(levels):
            expected = np.rint(np.clip(reference[..., 3], 0, 1) * 255).astype(np.uint8)
            np.testing.assert_array_equal(self.captured[mip][..., 3], expected,
                                          f"mip {mip} divergiu do box filter puro")
            reference = cook.halve(reference)

    def test_a_correcao_muda_os_mips_mas_nunca_o_nivel_base(self):
        alpha = antialiased_foliage(size=64, blobs=8)
        self.cook(alpha, None)
        plain = [level[..., 3].copy() for level in self.captured]
        self.captured.clear()
        self.cook(alpha, CUTOFF)
        corrected = [level[..., 3].copy() for level in self.captured]

        np.testing.assert_array_equal(plain[0], corrected[0],
                                      "o mip 0 é a referência de cobertura e sai intacto")
        self.assertTrue(any(not np.array_equal(a, b) for a, b in zip(plain[1:], corrected[1:])),
                        "a correção precisa alterar algum mip, senão não está ligada")


    def test_cozimento_interrompe_antes_de_publicar_mip_sem_cobertura(self):
        # Uma textura de faixas de 1 px torna-se irrepresentavel cedo. O cooker
        # conserva o ultimo mip valido; falhar o import inteiro impediria um
        # fallback seguro que a amostragem Vulkan oferece por maxLod.
        alpha = np.zeros((64, 64), dtype=np.float32)
        alpha[::16, :] = 1.0
        _, _, levels, coverage_by_mip = self.cook(alpha, 0.95)
        self.assertEqual(levels, len(coverage_by_mip))
        self.assertGreaterEqual(levels, 1)
        self.assertTrue(all(value > 0.0 for value in coverage_by_mip))

    def test_cadeia_para_no_ultimo_mip_com_cobertura_representavel(self):
        alpha = np.zeros((64, 64), dtype=np.float32)
        alpha[:, :16] = 1.0
        _, _, levels, coverage_by_mip = self.cook(alpha, CUTOFF)
        self.assertGreater(levels, 1)
        self.assertLess(levels, 7, "o tail 1x1 nao pode virar um card opaco")
        self.assertLessEqual(abs(coverage_by_mip[-1] - 0.25), 0.25 * 0.35)


if __name__ == "__main__":
    unittest.main()
