import hashlib
import json
import pathlib
import struct
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "samples" / "dirt-road"
IMPORTED = SAMPLE / "Imported"


class DirtRoadAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((SAMPLE / "manifest.json").read_text(encoding="utf8"))

    def test_manifest_identity_license_and_all_hashes(self):
        self.assertEqual(self.manifest["version"], 3)
        self.assertEqual(self.manifest["format"], "AEMAP-3")
        self.assertEqual(self.manifest["license"], "CC-BY-4.0")
        self.assertEqual(self.manifest["author"], "99.Miles")
        self.assertIn("CC-BY-4.0", (SAMPLE / "LICENSE.txt").read_text(encoding="utf8"))
        actual_names = {path.name for path in IMPORTED.iterdir() if path.is_file()}
        self.assertEqual(actual_names, set(self.manifest["outputs"]))
        for name, expected in self.manifest["outputs"].items():
            self.assertEqual(hashlib.sha256((IMPORTED / name).read_bytes()).hexdigest(), expected, name)

    def test_aemap_counts_offsets_and_exact_size(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        self.assertEqual(header[:4], (0x504D4541, 3, 144, 48))
        texture_count, material_count, draw_count = header[4:7]
        vertex_count, index_count = header[7:9]
        offsets = header[10:15]
        triangle_count = header[-3]
        # Contagens vêm do manifesto, não de literais: o pacote passa por dois
        # programas offline (o cooker e o baker de impostores) e um número
        # copiado à mão aqui só diria que alguém esqueceu de atualizar o teste.
        # O manifesto é conferido byte a byte pelo Gradle no empacotamento.
        statistics = self.manifest["statistics"]
        self.assertEqual((texture_count, material_count, draw_count),
                         (statistics["textures"], statistics["materials"], statistics["draws"]))
        self.assertEqual((vertex_count, triangle_count),
                         (statistics["vertices"], statistics["triangles"]))
        self.assertEqual(index_count, triangle_count * 3)
        self.assertTrue(all(offset % 16 == 0 for offset in offsets))
        self.assertEqual(offsets[-1], (offsets[-2] + vertex_count * 48 + 15) & ~15)
        self.assertEqual(len(payload), offsets[-1] + index_count * 4)

    def test_aemap_v3_has_spatial_lod_groups_without_duplicating_source_vertices(self):
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        draw_count, vertex_count = header[6], header[7]
        draw_offset = header[12]
        draws = [struct.unpack_from("<4I16f4fIfI", payload, draw_offset + index * 108)
                 for index in range(draw_count)]
        levels = [draw[-3] for draw in draws]
        groups = {}
        for draw in draws:
            groups.setdefault(draw[-1], []).append(draw)
        self.assertEqual({str(level): levels.count(level) for level in sorted(set(levels))},
                         self.manifest["statistics"]["lodDrawsByLevel"])
        self.assertEqual(sum(len(group) > 1 for group in groups.values()),
                         self.manifest["statistics"]["lodGroups"])
        # Os níveis simplificados continuam sem duplicar um único vértice de
        # origem; os quatro vértices por impostor são a única adição, e são
        # geometria nova (um quad), não uma cópia da malha que ele substitui.
        impostors = self.manifest["impostors"]
        self.assertEqual(vertex_count,
                         self.manifest["statistics"]["level0Vertices"] +
                         impostors["impostorVertices"],
                         "coarse levels reuse source vertices instead of duplicating them")
        self.assertEqual(impostors["impostorVertices"], impostors["groups"] * 4)
        for group in groups.values():
            errors = [draw[-2] for draw in group]
            self.assertEqual(errors[0], 0.0)
            self.assertEqual(errors, sorted(errors))

    def test_texture_payloads_have_complete_mips_and_bounded_fallback(self):
        for path in IMPORTED.glob("*.aetex"):
            data = path.read_bytes()
            magic, version, width, height, encoding, mip_count, payload_size = struct.unpack_from("<6IQ", data)
            self.assertEqual((magic, version), (0x58544541, 1), path.name)
            self.assertEqual(len(data), 32 + payload_size, path.name)
            self.assertGreater(mip_count, 0, path.name)
            if "fallback" in path.name:
                self.assertLessEqual(max(width, height), 512, path.name)
                self.assertIn(encoding, (3, 4), path.name)
            elif path.name == "environment.aetex":
                self.assertEqual((width, height, encoding), (1024, 512, 3), path.name)
                # At an equirectangular pole every longitude is the same
                # direction. Keeping the row constant prevents polar streaks.
                first_level = data[32:32 + width * height * 4]
                top = first_level[:width * 4]
                bottom = first_level[(height - 1) * width * 4:height * width * 4]
                self.assertEqual(len(set(struct.iter_unpack("<I", top))), 1)
                self.assertEqual(len(set(struct.iter_unpack("<I", bottom))), 1)
            elif path.name == "environment-specular.aetex":
                self.assertEqual((width, height, encoding, mip_count), (256, 256, 5, 9), path.name)
                values = np.frombuffer(data, dtype="<f2", offset=32)
                self.assertTrue(np.isfinite(values).all(), path.name)
                self.assertGreater(float(values.max()), 0.1, path.name)
            elif path.name == "environment-brdf.aetex":
                self.assertEqual((width, height, encoding, mip_count), (128, 128, 5, 1), path.name)
                values = np.frombuffer(data, dtype="<f2", offset=32).reshape((-1, 4))
                self.assertTrue(np.isfinite(values).all(), path.name)
                self.assertTrue(np.all(values[:, :2] >= 0.0), path.name)
                self.assertTrue(np.all(values[:, :2] <= 1.1), path.name)
            elif path.name == self.manifest["impostors"]["atlas"]:
                # Único atlas RGBA8 do caminho principal, e de propósito: o
                # astcenc que comprime as outras 70 texturas não está
                # disponível na máquina que assa impostores, e o AETX já aceita
                # encoding 3 sem caminho especial no runtime. Trocar por ASTC
                # 6x6 dividiria estes bytes por ~4 e nada mais muda.
                impostors = self.manifest["impostors"]
                self.assertEqual([width, height], impostors["atlasSize"], path.name)
                self.assertEqual(encoding, 3, path.name)
                self.assertEqual(width & (width - 1), 0, path.name)
                self.assertEqual(height & (height - 1), 0, path.name)
                # O tail 2x2/1x1 não representa uma copa esparsa e a converte
                # em card opaco. Cada tile termina no nível seguro declarado.
                self.assertEqual(mip_count, impostors["maximumSafeLod"] + 1,
                                 path.name)
                self.assertEqual(impostors["tile"] >> impostors["maximumSafeLod"],
                                 impostors["minimumMipTile"], path.name)
                self.assertEqual(impostors["mipIsolation"],
                                 "per-tile-alpha-weighted", path.name)
                self.assertEqual(payload_size, impostors["atlasBytes"], path.name)
                tiles = (width // impostors["tile"]) * (height // impostors["tile"])
                self.assertGreaterEqual(tiles, impostors["groups"], path.name)
            else:
                self.assertLessEqual(max(width, height), 4096, path.name)
                self.assertIn(encoding, (1, 2), path.name)

    def test_baked_impostors_close_each_lod_chain_with_one_camera_facing_quad(self):
        """O impostor troca centenas de cards alfa por um quad; o contrato é
        que ele seja o nível MAIS grosseiro do grupo (nunca substitui a malha de
        perto), que seja um quad de verdade (6 índices) e que os vértices
        cheguem centrados na origem — a rotação de yaw do vertex shader só é
        válida em torno do centro, e o volume de cull do runtime é calculado
        antes dela."""
        impostors = self.manifest["impostors"]
        payload = (IMPORTED / "scene.aemap").read_bytes()
        header = struct.unpack_from("<10I5Q6f7f3I", payload)
        material_count, draw_count = header[5], header[6]
        material_offset, draw_offset, vertex_offset = header[11], header[12], header[13]
        materials = [struct.unpack_from("<4I4f4f4ff3I", payload, material_offset + index * 80)
                     for index in range(material_count)]
        impostor_materials = [index for index, material in enumerate(materials)
                              if material[-3] & (1 << 8)]
        self.assertEqual(len(impostor_materials), 1, "um único material de impostor")
        material = materials[impostor_materials[0]]
        self.assertEqual(material[-3], (1 << 4) | (1 << 5) | (1 << 8),
                         "alpha mask + double sided + impostor, nunca blend")
        self.assertEqual(material[0], header[4] - 1, "usa o último registro de textura (o atlas)")
        self.assertEqual(material[13], 0.0, "folhagem é dielétrica: metallic = 0")
        self.assertEqual(material[16], 0.5, "cutoff do recorte por alfa")

        draws = [struct.unpack_from("<4I16f4fIfI", payload, draw_offset + index * 108)
                 for index in range(draw_count)]
        by_group = {}
        for draw in draws:
            by_group.setdefault(draw[26], []).append(draw)
        impostor_draws = [draw for draw in draws if draw[3] == impostor_materials[0]]
        self.assertEqual(len(impostor_draws), impostors["groups"])
        indices = np.frombuffer(payload, dtype="<u4", offset=header[14], count=header[8])
        for draw in impostor_draws:
            self.assertEqual(draw[1], 6, "um quad, dois triângulos")
            self.assertEqual(draw[2], 0, "índices do impostor são absolutos")
            group = by_group[draw[26]]
            self.assertEqual(draw[24], max(other[24] for other in group),
                             "o impostor é o nível mais grosseiro do grupo")
            coarser = max(other[25] for other in group if other[24] < draw[24])
            self.assertGreater(draw[25], coarser,
                               "erro geométrico estritamente maior que o nível anterior")
            corners = sorted(set(indices[draw[0]:draw[0] + 6].tolist()))
            self.assertEqual(len(corners), 4, "quatro vértices por impostor")
            positions = np.array([struct.unpack_from("<3f", payload, vertex_offset + corner * 48)
                                  for corner in corners])
            self.assertTrue(np.allclose(positions.mean(axis=0), 0.0, atol=1e-4),
                            "quad centrado na origem local")
            self.assertTrue(np.allclose(positions[:, 2], 0.0), "quad é plano em Z local")

    def test_impostor_vertex_shader_turns_position_normal_and_tangent_together(self):
        """Girar só a posição deixaria o quad encarando a câmera enquanto a
        iluminação continuaria vinda da direção em que ele foi assado."""
        vertex = (ROOT / "native" / "rhi" / "shaders" / "dirt_road.vert").read_text(encoding="utf8")
        self.assertIn("frame.materialFlags.x & 256u", vertex)
        for rotated in ("modelPosition=faceCamera*inPosition",
                        "modelNormal=faceCamera*inNormal",
                        "modelTangent=faceCamera*inTangent.xyz"):
            self.assertIn(rotated, vertex)
        self.assertIn("normalize(normalMatrix*modelNormal)", vertex)
        self.assertIn("normalize(linear*modelTangent)", vertex)

    def test_environment_v3_serializes_lighting_and_prefiltered_map_description(self):
        payload = (IMPORTED / "environment.aeenv").read_bytes()
        magic, version, byte_size, reserved = struct.unpack_from("<4I", payload)
        self.assertEqual((magic, version, byte_size, reserved),
                         (0x4E454541, 3, 176, 0))
        self.assertEqual(len(payload), byte_size)
        values = struct.unpack_from("<32f", payload, 16)
        self.assertGreater(values[3], 0.0)   # sun intensity
        self.assertGreater(values[11], 0.0)  # ambient strength
        self.assertGreaterEqual(values[27], 1.0)  # neutral-or-vibrant grade
        description = struct.unpack_from("<8I", payload, 144)
        self.assertEqual(description, (1, 256, 256, 9, 128, 128, 1, 3))
        self.assertEqual(self.manifest["environment"]["environmentResourceVersion"], 3)

    def test_runtime_hud_keeps_digit_order_and_glyphs_left_to_right(self):
        shader_dir = ROOT / "native" / "rhi" / "shaders"
        vertex = (shader_dir / "runtime_hud.vert").read_text(encoding="utf8")
        fragment = (shader_dir / "runtime_hud.frag").read_text(encoding="utf8")
        self.assertIn("localPosition=sourcePosition", vertex)
        self.assertNotIn("determinant(upright)", vertex)
        self.assertIn("(2u-cell.x)", fragment)

        glyphs = (31599, 11415, 29671, 29647, 23497,
                  31183, 31215, 29257, 31727, 31695)
        def rows(digit):
            return tuple("".join("#" if glyphs[digit] >> ((4-y)*3+(2-x)) & 1 else "."
                                 for x in range(3)) for y in range(5))
        self.assertEqual(rows(2), ("###", "..#", "###", "#..", "###"))
        self.assertEqual(rows(7), ("###", "..#", "..#", "..#", "..#"))


if __name__ == "__main__":
    unittest.main()
