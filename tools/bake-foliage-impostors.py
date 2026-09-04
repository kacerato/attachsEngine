"""Assa impostores de folhagem distante e emenda-os no pacote AEMAP v3 existente.

Por que este passo existe
-------------------------
A decomposicao medida do frame na pose do hotspot (docs/ORCAMENTO-120HZ.md)
mostra ~4,8 ms de custo fixo de geometria/binning e ~9,1 ms de custo por pixel,
e prova que nenhum ajuste de shading chega a 120 Hz: o passe opaco SO com base
color ja custa 8,811 ms contra 8,333 ms de frame inteiro. O que resta e reduzir
quantos fragmentos sao gerados e sombreados.

Um grupo de folhagem distante continua submetendo centenas de cards alfa que se
sobrepoem, cada um pagando binning, interpolacao, uma busca de textura e um
discard, para ocupar algumas dezenas de pixels. Trocar tudo isso por UM quad com
uma textura assada e a troca de memoria por computacao que a secao 4 do
orcamento mostra ter espaco de sobra: o app usa 173 MiB de varios GiB.

Por que emendar em vez de recozinhar
------------------------------------
O cooker completo exige `astcenc`, que nao esta disponivel nesta maquina. Mas o
AETX v1 aceita `encoding=3` (R8G8B8A8_SRGB) com cadeia de mip completa ate
16384x16384, e o runtime o decodifica sem caminho especial. Entao o atlas de
impostores sai como RGBA8 e as 70 texturas ASTC ja cozidas permanecem
intocadas: este programa acrescenta um registro de textura, um material, quatro
vertices e um draw por grupo, e reescreve so o `scene.aemap`.

O que NAO faz
-------------
Nao remove nada. O impostor entra como o ultimo nivel da cadeia de LOD do grupo,
com erro geometrico maior que o nivel anterior, e a selecao de LOD por erro
projetado do runtime decide quando usa-lo. Desligar LOD volta ao LOD0 exato.
"""
import argparse
import hashlib
import io
import json
import math
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

MAP_MAGIC = 0x504D4541
MAP_VERSION = 3
HEADER_SIZE = 144
VERTEX_STRIDE = 48
DRAW_STRIDE = 108
MATERIAL_STRIDE = 80
TEXTURE_STRIDE = 16
MATERIAL_ALPHA_MASK = 1 << 4
MATERIAL_DOUBLE_SIDED = 1 << 5
MATERIAL_IMPOSTOR = 1 << 8
MATERIAL_NORMAL_MAP = 1 << 1
AETX_MAGIC = 0x58544541
AETX_RGBA8_SRGB = 3
AETX_RGBA8_UNORM = 4


def normalized(value):
    return value / np.maximum(np.linalg.norm(value, axis=-1, keepdims=True), 1e-8)


def srgb_to_linear(value):
    return np.where(value <= 0.04045, value / 12.92, ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value):
    value = np.maximum(value, 0.0)
    return np.where(value <= 0.0031308, value * 12.92, 1.055 * value ** (1.0 / 2.4) - 0.055)


def load_cooker():
    """Reaproveita read_glb/vertex_record do cooker sem duplicar o formato."""
    text = pathlib.Path('tools/cook-gltf-map.py').read_text(encoding='utf-8')
    module = {}
    exec(compile(text.replace('def main():', 'def _cooker_main():'), 'cook-gltf-map', 'exec'),
         module)
    return module


class Package:
    """AEMAP v3 desmontado em secoes mutaveis."""

    def __init__(self, data):
        fields = struct.unpack_from('<10I5Q6f7f3I', data, 0)
        (self.magic, self.version, self.header_size, self.vertex_stride, self.texture_count,
         self.material_count, self.draw_count, self.vertex_count, self.index_count,
         self.reserved) = fields[:10]
        (self.texture_offset, self.material_offset, self.draw_offset, self.vertex_offset,
         self.index_offset) = fields[10:15]
        self.world_min = list(fields[15:18])
        self.world_max = list(fields[18:21])
        self.camera = list(fields[21:28])
        self.tail = list(fields[28:31])
        if self.magic != MAP_MAGIC or self.version != MAP_VERSION:
            raise ValueError('nao e AEMAP v3')
        if self.vertex_stride != VERTEX_STRIDE:
            raise ValueError(f'vertexStride {self.vertex_stride} inesperado')
        self.textures = [data[self.texture_offset + i * TEXTURE_STRIDE:
                              self.texture_offset + (i + 1) * TEXTURE_STRIDE]
                         for i in range(self.texture_count)]
        self.materials = [data[self.material_offset + i * MATERIAL_STRIDE:
                               self.material_offset + (i + 1) * MATERIAL_STRIDE]
                          for i in range(self.material_count)]
        self.draws = [data[self.draw_offset + i * DRAW_STRIDE:
                           self.draw_offset + (i + 1) * DRAW_STRIDE]
                      for i in range(self.draw_count)]
        self.vertices = bytearray(data[self.vertex_offset:
                                       self.vertex_offset + self.vertex_count * VERTEX_STRIDE])
        self.indices = list(struct.unpack_from(f'<{self.index_count}I', data, self.index_offset))

    def draw_fields(self, index):
        values = struct.unpack('<4I16f4fIfI', self.draws[index])
        return {'firstIndex': values[0], 'indexCount': values[1], 'vertexOffset': values[2],
                'materialIndex': values[3], 'world': list(values[4:20]),
                'center': list(values[20:23]), 'radius': values[23],
                'lodLevel': values[24], 'geometricError': values[25], 'lodGroupId': values[26]}

    def material_flags(self, index):
        return struct.unpack_from('<I', self.materials[index], 68)[0]

    def material_base_texture(self, index):
        return struct.unpack_from('<I', self.materials[index], 0)[0]

    def material_alpha_cutoff(self, index):
        return struct.unpack_from('<f', self.materials[index], 64)[0]

    def vertex_position(self, index):
        return struct.unpack_from('<3f', self.vertices, index * VERTEX_STRIDE)

    def vertex_uv0(self, index):
        return struct.unpack_from('<2f', self.vertices, index * VERTEX_STRIDE + 28)

    def vertex_uv(self, index, slot):
        return struct.unpack_from('<2f', self.vertices, index * VERTEX_STRIDE + 28 + slot * 8)

    def vertex_color(self, index):
        return np.asarray(struct.unpack_from('<4B', self.vertices, index * VERTEX_STRIDE + 44)) / 255.0

    def remove_baked_suffix(self):
        """Remove only the baker-owned appended suffix; reject mixed ownership."""
        material_indices = [i for i in range(self.material_count)
                            if self.material_flags(i) & MATERIAL_IMPOSTOR]
        if not material_indices:
            return
        if material_indices != [self.material_count - 1]:
            raise ValueError('impostor material is not a single appended suffix')
        first_material = material_indices[0]
        first_draw = next(i for i in range(self.draw_count)
                          if self.draw_fields(i)['materialIndex'] == first_material)
        if any(self.draw_fields(i)['materialIndex'] != first_material
               for i in range(first_draw, self.draw_count)):
            raise ValueError('impostor draws are not an appended suffix')
        first_index = self.draw_fields(first_draw)['firstIndex']
        first_vertex = min(self.indices[first_index:])
        first_texture = self.material_base_texture(first_material)
        for i in range(first_draw):
            draw = self.draw_fields(i)
            end = draw['firstIndex'] + draw['indexCount']
            if end > first_index or any(index + draw['vertexOffset'] >= first_vertex
                                        for index in self.indices[draw['firstIndex']:end]):
                raise ValueError('source geometry references baked suffix')
        for material in self.materials[:first_material]:
            if any(t != 0xffffffff and t >= first_texture
                   for t in struct.unpack_from('<4I', material)):
                raise ValueError('source material references baked texture suffix')
        self.draws = self.draws[:first_draw]
        self.indices = self.indices[:first_index]
        self.vertices = self.vertices[:first_vertex * VERTEX_STRIDE]
        self.materials = self.materials[:first_material]
        self.textures = self.textures[:first_texture]
        self.draw_count, self.index_count = len(self.draws), len(self.indices)
        self.vertex_count = first_vertex
        self.material_count, self.texture_count = len(self.materials), len(self.textures)

    def vertex_normal(self, index):
        packed = np.asarray(struct.unpack_from('<3h', self.vertices,
                                               index * VERTEX_STRIDE + 12),
                            dtype=np.float64)
        return tuple(np.clip(packed / 32767.0, -1.0, 1.0))

    def serialize(self):
        texture_offset = HEADER_SIZE
        material_offset = align(texture_offset + len(self.textures) * TEXTURE_STRIDE)
        draw_offset = align(material_offset + len(self.materials) * MATERIAL_STRIDE)
        vertex_offset = align(draw_offset + len(self.draws) * DRAW_STRIDE)
        vertex_count = len(self.vertices) // VERTEX_STRIDE
        index_offset = align(vertex_offset + len(self.vertices))
        total = index_offset + len(self.indices) * 4
        package = bytearray(total)
        header = struct.pack('<10I5Q6f7f3I', MAP_MAGIC, MAP_VERSION, HEADER_SIZE, VERTEX_STRIDE,
                             len(self.textures), len(self.materials), len(self.draws),
                             vertex_count, len(self.indices), self.reserved,
                             texture_offset, material_offset, draw_offset, vertex_offset,
                             index_offset, *self.world_min, *self.world_max, *self.camera,
                             len(self.indices) // 3, self.tail[1], self.tail[2])
        if len(header) != HEADER_SIZE:
            raise AssertionError('header fora do tamanho')
        package[:HEADER_SIZE] = header
        package[texture_offset:texture_offset + len(self.textures) * TEXTURE_STRIDE] = \
            b''.join(self.textures)
        package[material_offset:material_offset + len(self.materials) * MATERIAL_STRIDE] = \
            b''.join(self.materials)
        package[draw_offset:draw_offset + len(self.draws) * DRAW_STRIDE] = b''.join(self.draws)
        package[vertex_offset:vertex_offset + len(self.vertices)] = self.vertices
        package[index_offset:] = struct.pack(f'<{len(self.indices)}I', *self.indices)
        return bytes(package)


def align(value, alignment=16):
    return (value + alignment - 1) // alignment * alignment


def rasterize_impostor(positions, normals, uvs, triangles, albedo, cutoff, size,
                       right, up, forward, extent_x, extent_y, colors=None, factor=None):
    """Rasteriza cor e normal ortograficas de uma vista do grupo.

    Um z-buffer por tile resolve a ordem entre cards: o texel que fica e o do
    card mais proximo do observador que passou no teste de alfa, que e
    exatamente o que o alpha-test faz em tempo real. Sem isso o impostor
    misturaria folhas de tras com folhas da frente. Normais geometricas sao
    armazenadas em espaco de mundo: girar a camera nao deve girar a iluminacao.
    O normal map de detalhe da fonte nao e incorporado neste bake.
    """
    color_tile = np.zeros((size, size, 4), dtype=np.float32)
    normal_tile = np.zeros((size, size, 4), dtype=np.float32)
    depth = np.full((size, size), np.inf, dtype=np.float32)
    local = positions - positions.mean(axis=0)
    plane_x = local @ right
    plane_y = local @ up
    plane_z = local @ forward
    extent_x = max(float(extent_x), 1e-6)
    extent_y = max(float(extent_y), 1e-6)
    # Um texel de margem para que o card mais externo nao encoste na borda do
    # tile: o atlas e amostrado com filtro bilinear e uma folha colada na borda
    # sangraria para o tile vizinho.
    scale_x = (size * 0.5 - 1.0) / extent_x
    scale_y = (size * 0.5 - 1.0) / extent_y
    screen_x = plane_x * scale_x + size * 0.5
    screen_y = size * 0.5 - plane_y * scale_y
    height, width = albedo.shape[:2]
    colors = np.ones((len(positions), 4)) if colors is None else colors
    factor = np.ones(4) if factor is None else factor

    for triangle in triangles:
        xs = screen_x[triangle]
        ys = screen_y[triangle]
        minimum_x = max(int(math.floor(xs.min())), 0)
        maximum_x = min(int(math.ceil(xs.max())) + 1, size)
        minimum_y = max(int(math.floor(ys.min())), 0)
        maximum_y = min(int(math.ceil(ys.max())) + 1, size)
        if minimum_x >= maximum_x or minimum_y >= maximum_y:
            continue
        area = ((xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0]))
        if abs(area) < 1e-9:
            continue
        grid_x, grid_y = np.meshgrid(np.arange(minimum_x, maximum_x) + 0.5,
                                     np.arange(minimum_y, maximum_y) + 0.5)
        w0 = ((xs[1] - grid_x) * (ys[2] - grid_y) - (xs[2] - grid_x) * (ys[1] - grid_y)) / area
        w1 = ((xs[2] - grid_x) * (ys[0] - grid_y) - (xs[0] - grid_x) * (ys[2] - grid_y)) / area
        w2 = 1.0 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        z = w0 * plane_z[triangle[0]] + w1 * plane_z[triangle[1]] + w2 * plane_z[triangle[2]]
        u = w0 * uvs[triangle[0], 0] + w1 * uvs[triangle[1], 0] + w2 * uvs[triangle[2], 0]
        v = w0 * uvs[triangle[0], 1] + w1 * uvs[triangle[1], 1] + w2 * uvs[triangle[2], 1]
        texel_x = np.clip((u % 1.0) * width, 0, width - 1).astype(np.int32)
        texel_y = np.clip((v % 1.0) * height, 0, height - 1).astype(np.int32)
        sample = albedo[texel_y, texel_x].copy()
        vertex_color = (w0[..., None]*colors[triangle[0]] +
                        w1[..., None]*colors[triangle[1]] + w2[..., None]*colors[triangle[2]])
        sample[..., :3] = linear_to_srgb(srgb_to_linear(sample[..., :3]) *
                                         vertex_color[..., :3] * factor[:3])
        sample[..., 3] *= vertex_color[..., 3] * factor[3]
        covered = inside & (sample[..., 3] >= cutoff)
        if not covered.any():
            continue
        window = depth[minimum_y:maximum_y, minimum_x:maximum_x]
        nearer = covered & (z < window)
        if not nearer.any():
            continue
        window[nearer] = z[nearer]
        interpolated_normal = normalized(
            w0[..., None] * normals[triangle[0]] +
            w1[..., None] * normals[triangle[1]] +
            w2[..., None] * normals[triangle[2]])
        color_tile[minimum_y:maximum_y, minimum_x:maximum_x][nearer] = sample[nearer]
        normal_window = normal_tile[minimum_y:maximum_y, minimum_x:maximum_x]
        normal_window[nearer, :3] = interpolated_normal[nearer]
        normal_window[nearer, 3] = sample[..., 3][nearer]
    return color_tile, normal_tile


def corrected_coverage_alpha(alpha, target_coverage, cutoff):
    """Corrige cobertura sem contaminar a imagem usada pelo mip seguinte."""
    if target_coverage <= 0.0:
        return alpha
    low, high = 0.0, 8.0
    for _ in range(16):
        middle = (low + high) * 0.5
        coverage = float((np.clip(alpha * middle, 0, 1) >= cutoff).mean())
        if coverage < target_coverage:
            low = middle
        else:
            high = middle
    return np.clip(alpha * high, 0.0, 1.0)


def build_mip_chain(atlas, tile_size, minimum_tile=8, cutoff=0.5):
    """Reduz cada tile isoladamente e para antes do tail irrepresentavel.

    A imagem usada para gerar o nivel seguinte nunca recebe a correcao de
    cobertura do nivel publicado. Isso evita acumular alpha ate a arvore virar
    uma placa opaca. RGB e reduzido premultiplicado por alpha, portanto a cor
    escondida em texels transparentes tambem nao produz halo claro.
    """
    if tile_size <= 0 or atlas.shape[0] % tile_size or atlas.shape[1] % tile_size:
        raise ValueError('tile nao divide o atlas')
    if (minimum_tile < 1 or minimum_tile > tile_size or
            tile_size % minimum_tile or minimum_tile & (minimum_tile - 1)):
        raise ValueError('minimum_tile precisa ser potencia de dois e dividir tile_size')
    levels = []
    image = atlas.copy()
    image[..., :3] = srgb_to_linear(image[..., :3])
    rows = atlas.shape[0] // tile_size
    columns = atlas.shape[1] // tile_size
    base_coverage = np.zeros((rows, columns), dtype=np.float32)
    for row in range(rows):
        for column in range(columns):
            tile = atlas[row * tile_size:(row + 1) * tile_size,
                         column * tile_size:(column + 1) * tile_size]
            base_coverage[row, column] = float((tile[..., 3] >= cutoff).mean())
    current_tile = tile_size
    while True:
        display = image.copy()
        if current_tile != tile_size:
            for row in range(rows):
                for column in range(columns):
                    target = float(base_coverage[row, column])
                    if target <= 0.0:
                        continue
                    tile = display[row * current_tile:(row + 1) * current_tile,
                                   column * current_tile:(column + 1) * current_tile]
                    tile[..., 3] = corrected_coverage_alpha(tile[..., 3], target, cutoff)
        display[..., :3] = linear_to_srgb(display[..., :3])
        rgba = np.clip(display * 255.0 + 0.5, 0, 255).astype(np.uint8)
        levels.append((rgba.shape[1], rgba.shape[0], rgba.tobytes()))
        if current_tile == minimum_tile:
            break
        next_tile = current_tile // 2
        reduced = np.zeros((rows * next_tile, columns * next_tile, 4), dtype=np.float32)
        for row in range(rows):
            for column in range(columns):
                source = image[row * current_tile:(row + 1) * current_tile,
                               column * current_tile:(column + 1) * current_tile]
                blocks = source.reshape(next_tile, 2, next_tile, 2, 4)
                alpha = blocks[..., 3].mean(axis=(1, 3))
                alpha_sum = blocks[..., 3].sum(axis=(1, 3))
                rgb_sum = (blocks[..., :3] * blocks[..., 3:4]).sum(axis=(1, 3))
                target = reduced[row * next_tile:(row + 1) * next_tile,
                                 column * next_tile:(column + 1) * next_tile]
                target[..., :3] = np.where(
                    alpha_sum[..., None] > 1e-8,
                    rgb_sum / np.maximum(alpha_sum[..., None], 1e-8),
                    0.0,
                )
                target[..., 3] = alpha
        image = reduced
        current_tile = next_tile
    return levels


def build_normal_mip_chain(atlas, tile_size, minimum_tile=4, cutoff=0.5):
    """Reduz normais lineares por tile, ponderadas pela cobertura.

    `atlas.rgb` permanece em [-1,1] durante toda a reducao. Renormalizar antes
    de codificar RGBA8 UNORM evita que mips distantes puxem a copa para a normal
    neutra ou para o branco escondido em texels transparentes.
    """
    if tile_size <= 0 or atlas.shape[0] % tile_size or atlas.shape[1] % tile_size:
        raise ValueError('tile nao divide o atlas de normais')
    if (minimum_tile < 1 or minimum_tile > tile_size or
            tile_size % minimum_tile or minimum_tile & (minimum_tile - 1)):
        raise ValueError('minimum_tile precisa ser potencia de dois e dividir tile_size')
    levels = []
    image = atlas
    rows = atlas.shape[0] // tile_size
    columns = atlas.shape[1] // tile_size
    base_coverage = np.zeros((rows, columns), dtype=np.float32)
    for row in range(rows):
        for column in range(columns):
            tile = atlas[row * tile_size:(row + 1) * tile_size,
                         column * tile_size:(column + 1) * tile_size]
            base_coverage[row, column] = float((tile[..., 3] >= cutoff).mean())
    current_tile = tile_size
    while True:
        display = image.copy()
        if current_tile != tile_size:
            for row in range(rows):
                for column in range(columns):
                    target_coverage = float(base_coverage[row, column])
                    if target_coverage <= 0.0:
                        continue
                    tile = display[row * current_tile:(row + 1) * current_tile,
                                   column * current_tile:(column + 1) * current_tile]
                    tile[..., 3] = corrected_coverage_alpha(
                        tile[..., 3], target_coverage, cutoff)
        encoded = np.zeros_like(display)
        covered = display[..., 3] > 1.0e-8
        encoded[..., :3] = 0.5
        encoded[..., 2] = 1.0
        encoded[covered, :3] = normalized(display[covered, :3]) * 0.5 + 0.5
        encoded[..., 3] = display[..., 3]
        rgba = np.clip(encoded * 255.0 + 0.5, 0, 255).astype(np.uint8)
        levels.append((rgba.shape[1], rgba.shape[0], rgba.tobytes()))
        if current_tile == minimum_tile:
            break
        next_tile = current_tile // 2
        reduced = np.zeros((rows * next_tile, columns * next_tile, 4), dtype=np.float32)
        for row in range(rows):
            for column in range(columns):
                source = image[row * current_tile:(row + 1) * current_tile,
                               column * current_tile:(column + 1) * current_tile]
                blocks = source.reshape(next_tile, 2, next_tile, 2, 4)
                alpha = blocks[..., 3].mean(axis=(1, 3))
                normal_sum = (blocks[..., :3] * blocks[..., 3:4]).sum(axis=(1, 3))
                target = reduced[row * next_tile:(row + 1) * next_tile,
                                 column * next_tile:(column + 1) * next_tile]
                target[..., :3] = normalized(normal_sum)
                target[..., 3] = alpha
        image = reduced
        current_tile = next_tile
    return levels


def write_aetx(path, width, height, encoding, levels):
    """AETX v1: cabecalho explicito little-endian + cadeia de mip concatenada."""
    payload = b''.join(level[2] for level in levels)
    path.write_bytes(struct.pack('<6IQ', AETX_MAGIC, 1, width, height, encoding,
                                 len(levels), len(payload)) + payload)
    return len(payload)


def fallback_chain(levels, limit=512):
    """Cadeia do dispositivo sem ASTC: comeca no primeiro nivel <= 512 px.

    Mesmo contrato que o cooker aplica as 70 texturas do mapa -- o caminho ASTC
    guarda a resolucao inteira, o fallback de desenvolvimento fica limitado para
    nao multiplicar APK e memoria residente pelo corpus RGBA8 completo.
    """
    for index, (width, height, _) in enumerate(levels):
        if max(width, height) <= limit:
            return levels[index:]
    return levels[-1:]


def refresh_manifest(manifest_path, package_directory, package, baked, color_atlas_name,
                     normal_atlas_name, atlas_width, atlas_height, view_tile,
                     view_columns, view_rows, color_atlas_bytes, normal_atlas_bytes,
                     error_scale, mip_count, minimum_mip_tile):
    """Reescreve estatisticas e hashes do manifesto sobre o pacote ja gravado.

    O Gradle confere SHA-256 de cada entrada de `outputs` contra o arquivo
    empacotado (android/app/build.gradle.kts) e o teste de ativos exige que o
    conjunto de nomes seja exatamente o conteudo do diretorio. Recalcular tudo a
    partir do diretorio -- em vez de remendar campo a campo -- e o que impede o
    manifesto de descrever um pacote que nao existe mais.
    """
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    levels = {}
    groups = {}
    for index in range(package.draw_count):
        draw = package.draw_fields(index)
        levels[draw['lodLevel']] = levels.get(draw['lodLevel'], 0) + 1
        groups.setdefault(draw['lodGroupId'], 0)
        groups[draw['lodGroupId']] += 1
    statistics = manifest['statistics']
    statistics['textures'] = package.texture_count
    statistics['materials'] = package.material_count
    statistics['draws'] = package.draw_count
    statistics['vertices'] = len(package.vertices) // VERTEX_STRIDE
    statistics['triangles'] = package.index_count // 3
    statistics['lodDrawsByLevel'] = {str(level): count for level, count in sorted(levels.items())}
    statistics['lodGroups'] = sum(1 for count in groups.values() if count > 1)
    manifest['impostors'] = {
        'version': 2,
        'tool': 'tools/bake-foliage-impostors.py',
        'groups': baked,
        'atlas': f'{color_atlas_name}.aetex',
        'atlasFallback': f'{color_atlas_name}-fallback.aetex',
        'normalAtlas': f'{normal_atlas_name}.aetex',
        'normalAtlasFallback': f'{normal_atlas_name}-fallback.aetex',
        'atlasSize': [atlas_width, atlas_height],
        'viewTile': view_tile,
        'viewGrid': [view_columns, view_rows],
        'views': view_columns * view_rows,
        'errorScale': error_scale,
        'mipIsolation': 'per-view-alpha-weighted',
        'viewTransition': 'single-sample-bayer',
        'normalEncoding': 'world-space-rgba8-unorm',
        'maximumSafeLod': mip_count - 1,
        'minimumMipTile': minimum_mip_tile,
        # RGBA8 e nao ASTC porque astcenc nao esta disponivel nesta maquina; o
        # AETX ja aceita encoding 3 e o runtime o decodifica sem caminho
        # especial. Trocar por ASTC 6x6 dividiria estes bytes por ~4.
        'encoding': 'R8G8B8A8_SRGB',
        'normalEncodingFormat': 'R8G8B8A8_UNORM',
        'atlasBytes': color_atlas_bytes,
        'normalAtlasBytes': normal_atlas_bytes,
        'impostorVertices': baked * 4,
        'impostorTriangles': baked * 2,
    }
    manifest['outputs'] = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sorted(package_directory.iterdir()) if path.is_file()}
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=pathlib.Path, required=True, help='GLB de origem')
    parser.add_argument('--package', type=pathlib.Path, required=True, help='scene.aemap')
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True, help='diretorio de saida')
    parser.add_argument('--view-tile', type=int, default=32,
                        help='resolucao quadrada de cada vista (potencia de dois, 16..128)')
    parser.add_argument('--view-columns', type=int, default=4,
                        help='colunas de vistas azimutais no macro-tile')
    parser.add_argument('--view-rows', type=int, default=2,
                        help='linhas de vistas azimutais no macro-tile')
    parser.add_argument('--minimum-mip-tile', type=int, default=4,
                        help='menor vista publicada (potencia de dois, 4..view-tile)')
    parser.add_argument('--minimum-triangles', type=int, default=64,
                        help='grupos menores que isto nao valem um impostor')
    parser.add_argument('--error-scale', type=float, default=1.25,
                        help='erro geometrico do impostor como multiplo do nivel anterior')
    parser.add_argument('--update-manifest', action='store_true',
                        help='reescreve estatisticas e hashes do manifesto a partir de --out; '
                             'exige que --out seja o diretorio final do pacote')
    parser.add_argument('--replace-baked', action='store_true',
                        help='replace only a validated baker-owned appended suffix')
    args = parser.parse_args()
    if args.view_tile < 16 or args.view_tile > 128 or args.view_tile & (args.view_tile - 1):
        parser.error('--view-tile deve ser potencia de dois em [16, 128]')
    if (args.view_columns < 1 or args.view_rows < 1 or
            args.view_columns > 8 or args.view_rows > 8 or
            args.view_columns & (args.view_columns - 1) or
            args.view_rows & (args.view_rows - 1)):
        parser.error('--view-columns/--view-rows devem ser potencias de dois em [1, 8]')
    if args.view_columns * args.view_rows < 4:
        parser.error('o impostor multivista exige pelo menos quatro vistas')
    if (args.minimum_mip_tile < 4 or args.minimum_mip_tile > args.view_tile or
            args.minimum_mip_tile & (args.minimum_mip_tile - 1) or
            args.view_tile % args.minimum_mip_tile):
        parser.error('--minimum-mip-tile deve ser potencia de dois em [4, view-tile] e dividir view-tile')

    cooker = load_cooker()
    read_glb = cooker['read_glb']
    buffer_view_bytes = cooker['buffer_view_bytes']
    vertex_record = cooker['vertex_record']

    gltf, binary = read_glb(args.source.read_bytes())
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    package = Package(args.package.read_bytes())
    if args.replace_baked:
        package.remove_baked_suffix()
    print(f'pacote: {package.texture_count} texturas, {package.material_count} materiais, '
          f'{package.draw_count} draws, {package.vertex_count} vertices')
    # Assar duas vezes sobre o mesmo pacote empilharia um segundo conjunto de
    # quads sobre o primeiro, cada grupo passando a desenhar dois impostores no
    # mesmo lugar. O bit no material e a evidencia de que ja foi assado.
    if any((package.material_flags(index) & MATERIAL_IMPOSTOR) != 0
           for index in range(package.material_count)):
        raise SystemExit('o pacote ja contem impostores; restaure o scene.aemap original antes '
                         'de reassar (git checkout -- <scene.aemap>)')

    cooked_to_gltf = {entry['cookedIndex']: entry['gltfTexture'] for entry in manifest['textures']}
    albedo_cache = {}

    def albedo_for(material_index):
        texture = package.material_base_texture(material_index)
        if texture == 0xFFFFFFFF:
            return None
        if texture in albedo_cache:
            return albedo_cache[texture]
        gltf_texture = cooked_to_gltf.get(texture)
        if gltf_texture is None:
            albedo_cache[texture] = None
            return None
        image = gltf['images'][gltf['textures'][gltf_texture]['source']]
        raw = buffer_view_bytes(gltf, binary, image['bufferView'])
        decoded = np.asarray(Image.open(io.BytesIO(raw)).convert('RGBA'),
                             dtype=np.float32) / 255.0
        albedo_cache[texture] = decoded
        return decoded

    # Um impostor por grupo de LOD alpha-tested, construido a partir do nivel 0.
    groups = {}
    for index in range(package.draw_count):
        draw = package.draw_fields(index)
        if draw['lodLevel'] != 0:
            groups.setdefault(draw['lodGroupId'], {'maximumLevel': 0, 'maximumError': 0.0})
            entry = groups[draw['lodGroupId']]
            entry['maximumLevel'] = max(entry['maximumLevel'], draw['lodLevel'])
            entry['maximumError'] = max(entry['maximumError'], draw['geometricError'])
            continue
        if (package.material_flags(draw['materialIndex']) & MATERIAL_ALPHA_MASK) == 0:
            continue
        if draw['indexCount'] // 3 < args.minimum_triangles:
            continue
        entry = groups.setdefault(draw['lodGroupId'], {'maximumLevel': 0, 'maximumError': 0.0})
        entry['draw'] = draw

    candidates = [(gid, entry) for gid, entry in sorted(groups.items()) if 'draw' in entry]
    if not candidates:
        raise SystemExit('nenhum grupo alpha-tested elegivel')
    macro_width = args.view_columns * args.view_tile
    macro_height = args.view_rows * args.view_tile
    columns = int(math.ceil(math.sqrt(len(candidates) * macro_height / macro_width)))
    rows = int(math.ceil(len(candidates) / columns))
    atlas_width = columns * macro_width
    atlas_height = rows * macro_height
    # Potencia de dois nos dois eixos: a cadeia de mip do AETX reduz por metade
    # ate 1x1, e um atlas nao-potencia produziria niveis com tiles cortados ao
    # meio, sangrando um impostor no vizinho.
    atlas_width = 1 << (atlas_width - 1).bit_length()
    atlas_height = 1 << (atlas_height - 1).bit_length()
    columns = atlas_width // macro_width
    print(f'{len(candidates)} grupos elegiveis -> atlas {atlas_width}x{atlas_height} '
          f'com {args.view_columns * args.view_rows} vistas de {args.view_tile}px')

    color_atlas = np.zeros((atlas_height, atlas_width, 4), dtype=np.float32)
    normal_atlas = np.zeros((atlas_height, atlas_width, 4), dtype=np.float32)
    up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    view_count = args.view_columns * args.view_rows

    new_vertices = bytearray()
    new_indices = []
    new_draws = []
    baked = 0
    base_vertex = package.vertex_count
    for slot, (group_id, entry) in enumerate(candidates):
        draw = entry['draw']
        albedo = albedo_for(draw['materialIndex'])
        if albedo is None:
            continue
        cutoff = package.material_alpha_cutoff(draw['materialIndex'])
        first = draw['firstIndex']
        used = [index + draw['vertexOffset'] for index in
                package.indices[first:first + draw['indexCount']]]
        unique, compact = np.unique(np.asarray(used, dtype=np.uint32), return_inverse=True)
        positions = np.array([package.vertex_position(int(v)) for v in unique], dtype=np.float64)
        normals = np.array([package.vertex_normal(int(v)) for v in unique], dtype=np.float64)
        uv_slot = struct.unpack_from('<I', package.materials[draw['materialIndex']], 72)[0] & 3
        if uv_slot > 1:
            raise ValueError('unsupported base color UV channel')
        uvs = np.array([package.vertex_uv(int(v), uv_slot) for v in unique], dtype=np.float64)
        colors = np.array([package.vertex_color(int(v)) for v in unique])
        factor = np.asarray(struct.unpack_from('<4f', package.materials[draw['materialIndex']], 16))
        triangles = compact.reshape(-1, 3)

        # O AEMAP mantem vertices locais e matriz column-major. Converter a
        # geometria fonte para mundo antes do bake remove a suposicao fragil de
        # que toda arvore usa matriz identidade/sem escala. O impostor publicado
        # volta a ter matriz apenas de translacao, e seu yaw passa a ser mundial.
        model_columns = np.asarray(draw['world'], dtype=np.float64).reshape(4, 4)
        homogeneous = np.concatenate((positions, np.ones((len(positions), 1))), axis=1)
        world_positions = (homogeneous @ model_columns)[:, :3]
        linear = model_columns[:3, :3].T
        try:
            normal_matrix = np.linalg.inv(linear).T
        except np.linalg.LinAlgError:
            continue
        world_normals = normalized(normals @ normal_matrix.T)
        centre = world_positions.mean(axis=0)
        local = world_positions - centre
        half_width = max(float(np.linalg.norm(local[:, (0, 2)], axis=1).max()), 1e-3)
        half_height = max(float(np.abs(local[:, 1]).max()), 1e-3)

        macro_x = (slot % columns) * macro_width
        macro_y = (slot // columns) * macro_height
        visible_pixels = 0
        for view in range(view_count):
            angle = 2.0 * math.pi * view / view_count
            right = np.array([math.cos(angle), 0.0, -math.sin(angle)], dtype=np.float64)
            forward = np.array([math.sin(angle), 0.0, math.cos(angle)], dtype=np.float64)
            color_tile, normal_tile = rasterize_impostor(
                world_positions, world_normals, uvs, triangles, albedo, cutoff,
                args.view_tile, right, up, forward, half_width, half_height, colors, factor)
            view_x = macro_x + (view % args.view_columns) * args.view_tile
            view_y = macro_y + (view // args.view_columns) * args.view_tile
            color_atlas[view_y:view_y + args.view_tile,
                        view_x:view_x + args.view_tile] = color_tile
            normal_atlas[view_y:view_y + args.view_tile,
                         view_x:view_x + args.view_tile] = normal_tile
            visible_pixels += int((color_tile[..., 3] >= cutoff).sum())
        if visible_pixels == 0:
            continue

        world = [1.0, 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0, 0.0,
                 0.0, 0.0, 1.0, 0.0,
                 float(centre[0]), float(centre[1]), float(centre[2]), 1.0]
        # Inverse of raster scale for UV endpoints 0..1 (not texel centers).
        margin_x = 1.0 / (args.view_tile * 0.5 - 1.0) * half_width
        margin_y = 1.0 / (args.view_tile * 0.5 - 1.0) * half_height
        span_x = half_width + margin_x
        span_y = half_height + margin_y
        macro_origin = np.array([macro_x / atlas_width, macro_y / atlas_height],
                                dtype=np.float32)
        corners = [((-span_x, -span_y, 0.0), (0.0, 1.0)),
                   ((span_x, -span_y, 0.0), (1.0, 1.0)),
                   ((span_x, span_y, 0.0), (1.0, 0.0)),
                   ((-span_x, span_y, 0.0), (0.0, 0.0))]
        for local_position, local_uv in corners:
            new_vertices += vertex_record(np.array(local_position, dtype=np.float32),
                                          np.array([0.0, 0.0, 1.0], dtype=np.float32),
                                          np.array([1.0, 0.0, 0.0, 1.0], dtype=np.float32),
                                          np.array(local_uv, dtype=np.float32),
                                          macro_origin,
                                          np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32))
        quad = base_vertex + baked * 4
        first_index = package.index_count + len(new_indices)
        new_indices.extend([quad, quad + 1, quad + 2, quad, quad + 2, quad + 3])
        # Preserve strict LOD ordering while accounting for atlas resolution
        # and angular displacement between views. Never force early selection
        # by pretending a flattened crown has zero geometric error.
        texel = 2.0 * max(span_x, span_y) / args.view_tile
        # Angular/parallax displacement is real error, not just atlas texel size.
        error = max(entry['maximumError'] * args.error_scale, texel,
                    half_width * math.sin(math.pi / view_count))
        quad_radius = math.sqrt(span_x * span_x + span_y * span_y)
        centre_offset = float(np.linalg.norm(centre - np.asarray(draw['center'])))
        conservative_radius = max(float(draw['radius']), centre_offset + quad_radius)
        new_draws.append((first_index, 6, world, draw['center'], conservative_radius,
                          entry['maximumLevel'] + 1, error, group_id))
        baked += 1
        if baked % 25 == 0:
            print(f'  {baked}/{len(candidates)} assados')

    if baked == 0:
        raise SystemExit('nenhum impostor produzido')

    args.out.mkdir(parents=True, exist_ok=True)
    color_levels = build_mip_chain(color_atlas, args.view_tile, args.minimum_mip_tile)
    normal_levels = build_normal_mip_chain(normal_atlas, args.view_tile,
                                           args.minimum_mip_tile)
    color_atlas_name = f'texture_{package.texture_count:03d}'
    color_atlas_bytes = write_aetx(args.out / f'{color_atlas_name}.aetex',
                                   atlas_width, atlas_height, AETX_RGBA8_SRGB,
                                   color_levels)
    reduced = fallback_chain(color_levels)
    write_aetx(args.out / f'{color_atlas_name}-fallback.aetex', reduced[0][0], reduced[0][1],
               AETX_RGBA8_SRGB, reduced)

    # Registro de textura: filtro linear e mip linear (um impostor distante ocupa
    # dezenas de pixels -- amostrar por vizinho mais proximo o faria cintilar a
    # cada passo da camera), clamp nos dois eixos porque o atlas nao repete, e
    # sRGB porque o tile guarda cor de base.
    package.textures.append(struct.pack('<4I', 1 | 2 | 4 | 8 | 16, 0, 0, 0))
    impostor_texture = package.texture_count
    package.texture_count += 1
    normal_atlas_name = f'texture_{package.texture_count:03d}'
    normal_atlas_bytes = write_aetx(args.out / f'{normal_atlas_name}.aetex',
                                    atlas_width, atlas_height, AETX_RGBA8_UNORM,
                                    normal_levels)
    normal_reduced = fallback_chain(normal_levels)
    write_aetx(args.out / f'{normal_atlas_name}-fallback.aetex',
               normal_reduced[0][0], normal_reduced[0][1], AETX_RGBA8_UNORM,
               normal_reduced)
    package.textures.append(struct.pack('<4I', 1 | 2 | 4 | 8, 0, 0, 0))
    impostor_normal_texture = package.texture_count
    package.texture_count += 1

    # Distant diffuse foliage approximation: albedo remains unlit and receives
    # runtime lighting from the geometric normal atlas. Specular/MR/emissive
    # detail is not represented; geometric error keeps this LOD far away.
    flags = (MATERIAL_ALPHA_MASK | MATERIAL_DOUBLE_SIDED | MATERIAL_IMPOSTOR |
             MATERIAL_NORMAL_MAP)
    macro_columns = atlas_width // macro_width
    macro_rows = atlas_height // macro_height
    metadata = (int(math.log2(macro_columns)) |
                int(math.log2(macro_rows)) << 4 |
                args.view_columns << 8 |
                args.view_rows << 12)
    package.materials.append(struct.pack(
        '<4I4f4f4ff3I', impostor_texture, impostor_normal_texture,
        0xFFFFFFFF, 0xFFFFFFFF,
        1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0,
        1.0, 0.0, 1.0, 0.0, 0.5, flags, 0, metadata))
    impostor_material = package.material_count
    package.material_count += 1

    package.vertices += new_vertices
    package.indices.extend(new_indices)
    for first_index, index_count, world, centre, radius, level, error, group_id in new_draws:
        package.draws.append(struct.pack('<4I16f4fIfI', first_index, index_count, 0,
                                         impostor_material, *world, *centre, radius,
                                         level, error, group_id))
    package.draw_count = len(package.draws)
    package.index_count = len(package.indices)

    (args.out / 'scene.aemap').write_bytes(package.serialize())
    print(f'{baked} impostores multivista assados; atlas {color_atlas_name}.aetex '
          f'({color_atlas_bytes / 2**20:.1f} MiB, {len(color_levels)} mips) + fallback '
          f'{reduced[0][0]}x{reduced[0][1]}')
    print(f'normais: {normal_atlas_name}.aetex ({normal_atlas_bytes / 2**20:.1f} MiB)')
    print(f'pacote: +{baked} draws, +{baked * 4} vertices, +2 texturas, +1 material')
    if args.update_manifest:
        refresh_manifest(args.manifest, args.out, package, baked, color_atlas_name,
                         normal_atlas_name, atlas_width, atlas_height, args.view_tile,
                         args.view_columns, args.view_rows, color_atlas_bytes,
                         normal_atlas_bytes, args.error_scale, len(color_levels),
                         args.minimum_mip_tile)
        print(f'manifesto atualizado: {args.manifest}')


if __name__ == '__main__':
    main()
