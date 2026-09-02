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
AETX_MAGIC = 0x58544541
AETX_RGBA8_SRGB = 3


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


def rasterize_impostor(positions, uvs, triangles, albedo, cutoff, size, right, up, forward):
    """Rasteriza ortograficamente um grupo num tile RGBA com teste de alfa.

    Um z-buffer por tile resolve a ordem entre cards: o texel que fica e o do
    card mais proximo do observador que passou no teste de alfa, que e
    exatamente o que o alpha-test faz em tempo real. Sem isso o impostor
    misturaria folhas de tras com folhas da frente.
    """
    tile = np.zeros((size, size, 4), dtype=np.float32)
    depth = np.full((size, size), np.inf, dtype=np.float32)
    local = positions - positions.mean(axis=0)
    plane_x = local @ right
    plane_y = local @ up
    plane_z = local @ forward
    extent = max(float(np.abs(plane_x).max()), float(np.abs(plane_y).max()), 1e-6)
    # Meio texel de margem para que o card mais externo nao encoste na borda do
    # tile: o atlas e amostrado com filtro bilinear e uma folha colada na borda
    # sangraria para o tile vizinho.
    scale = (size * 0.5 - 1.0) / extent
    screen_x = plane_x * scale + size * 0.5
    screen_y = size * 0.5 - plane_y * scale
    height, width = albedo.shape[:2]

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
        sample = albedo[texel_y, texel_x]
        covered = inside & (sample[..., 3] >= cutoff)
        if not covered.any():
            continue
        window = depth[minimum_y:maximum_y, minimum_x:maximum_x]
        nearer = covered & (z < window)
        if not nearer.any():
            continue
        window[nearer] = z[nearer]
        tile[minimum_y:maximum_y, minimum_x:maximum_x][nearer] = sample[nearer]
    return tile


def build_mip_chain(atlas):
    """Cadeia completa de mips preservando cobertura de alfa.

    Um box filter comum borra o alfa e o impostor engorda com a distancia --
    exatamente o defeito que o cooker ja corrige nas texturas de cobertura.
    Aqui o alfa e reescalado para manter a fracao de texels acima do cutoff.
    """
    levels = []
    image = atlas
    base_coverage = float((image[..., 3] >= 0.5).mean())
    while True:
        rgba = np.clip(image * 255.0 + 0.5, 0, 255).astype(np.uint8)
        levels.append((rgba.shape[1], rgba.shape[0], rgba.tobytes()))
        if image.shape[0] == 1 and image.shape[1] == 1:
            break
        height = max(1, image.shape[0] // 2)
        width = max(1, image.shape[1] // 2)
        reduced = image.reshape(height, 2, width, 2, 4).mean(axis=(1, 3))
        if base_coverage > 0.0:
            low, high = 0.0, 8.0
            for _ in range(16):
                middle = (low + high) * 0.5
                coverage = float((np.clip(reduced[..., 3] * middle, 0, 1) >= 0.5).mean())
                if coverage < base_coverage:
                    low = middle
                else:
                    high = middle
            reduced[..., 3] = np.clip(reduced[..., 3] * high, 0.0, 1.0)
        image = reduced
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


def refresh_manifest(manifest_path, package_directory, package, baked, atlas_name,
                     atlas_width, atlas_height, tile, atlas_bytes, error_scale):
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
        'version': 1,
        'tool': 'tools/bake-foliage-impostors.py',
        'groups': baked,
        'atlas': f'{atlas_name}.aetex',
        'atlasFallback': f'{atlas_name}-fallback.aetex',
        'atlasSize': [atlas_width, atlas_height],
        'tile': tile,
        'errorScale': error_scale,
        # RGBA8 e nao ASTC porque astcenc nao esta disponivel nesta maquina; o
        # AETX ja aceita encoding 3 e o runtime o decodifica sem caminho
        # especial. Trocar por ASTC 6x6 dividiria estes bytes por ~4.
        'encoding': 'R8G8B8A8_SRGB',
        'atlasBytes': atlas_bytes,
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
    parser.add_argument('--tile', type=int, default=128)
    parser.add_argument('--minimum-triangles', type=int, default=64,
                        help='grupos menores que isto nao valem um impostor')
    parser.add_argument('--error-scale', type=float, default=1.25,
                        help='erro geometrico do impostor como multiplo do nivel anterior')
    parser.add_argument('--update-manifest', action='store_true',
                        help='reescreve estatisticas e hashes do manifesto a partir de --out; '
                             'exige que --out seja o diretorio final do pacote')
    args = parser.parse_args()

    cooker = load_cooker()
    read_glb = cooker['read_glb']
    buffer_view_bytes = cooker['buffer_view_bytes']
    vertex_record = cooker['vertex_record']

    gltf, binary = read_glb(args.source.read_bytes())
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    package = Package(args.package.read_bytes())
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
    columns = int(math.ceil(math.sqrt(len(candidates))))
    rows = int(math.ceil(len(candidates) / columns))
    atlas_width = columns * args.tile
    atlas_height = rows * args.tile
    # Potencia de dois nos dois eixos: a cadeia de mip do AETX reduz por metade
    # ate 1x1, e um atlas nao-potencia produziria niveis com tiles cortados ao
    # meio, sangrando um impostor no vizinho.
    atlas_width = 1 << (atlas_width - 1).bit_length()
    atlas_height = 1 << (atlas_height - 1).bit_length()
    columns = atlas_width // args.tile
    print(f'{len(candidates)} grupos elegiveis -> atlas {atlas_width}x{atlas_height} '
          f'com tiles de {args.tile}px')

    atlas = np.zeros((atlas_height, atlas_width, 4), dtype=np.float32)
    right = np.array([1.0, 0.0, 0.0], dtype=np.float64)
    up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    forward = np.array([0.0, 0.0, 1.0], dtype=np.float64)

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
        used = package.indices[first:first + draw['indexCount']]
        unique, compact = np.unique(np.asarray(used, dtype=np.uint32), return_inverse=True)
        positions = np.array([package.vertex_position(int(v)) for v in unique], dtype=np.float64)
        uvs = np.array([package.vertex_uv0(int(v)) for v in unique], dtype=np.float64)
        triangles = compact.reshape(-1, 3)
        tile = rasterize_impostor(positions, uvs, triangles, albedo, cutoff, args.tile,
                                  right, up, forward)
        if float((tile[..., 3] >= cutoff).mean()) <= 0.0:
            continue
        tile_x = (slot % columns) * args.tile
        tile_y = (slot // columns) * args.tile
        atlas[tile_y:tile_y + args.tile, tile_x:tile_x + args.tile] = tile

        centre = positions.mean(axis=0)
        local = positions - centre
        half_width = max(float(np.abs(local @ right).max()), float(np.abs(local @ up).max()), 1e-3)
        # O quad e local e centrado na origem; a matriz de mundo do draw o
        # coloca no lugar. Assim o vertex shader pode gira-lo para encarar a
        # camera sem precisar do centro do grupo como uniform.
        world = list(draw['world'])
        offset = centre @ np.array(world, dtype=np.float64).reshape(4, 4)[:3, :3]
        world[12] += float(offset[0])
        world[13] += float(offset[1])
        world[14] += float(offset[2])
        margin = 0.5 / (args.tile * 0.5 - 1.0) * half_width
        span = half_width + margin
        u0 = (tile_x + 0.5) / atlas_width
        v0 = (tile_y + 0.5) / atlas_height
        u1 = (tile_x + args.tile - 0.5) / atlas_width
        v1 = (tile_y + args.tile - 0.5) / atlas_height
        corners = [((-span, -span, 0.0), (u0, v1)), ((span, -span, 0.0), (u1, v1)),
                   ((span, span, 0.0), (u1, v0)), ((-span, span, 0.0), (u0, v0))]
        for local_position, uv in corners:
            new_vertices += vertex_record(np.array(local_position, dtype=np.float32),
                                          np.array([0.0, 0.0, 1.0], dtype=np.float32),
                                          np.array([1.0, 0.0, 0.0, 1.0], dtype=np.float32),
                                          np.array(uv, dtype=np.float32),
                                          np.array(uv, dtype=np.float32),
                                          np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32))
        quad = base_vertex + baked * 4
        first_index = package.index_count + len(new_indices)
        new_indices.extend([quad, quad + 1, quad + 2, quad, quad + 2, quad + 3])
        # Erro geometrico do impostor.
        #
        # A tentacao e cobrar a extensao em profundidade do grupo (o raio):
        # o quad achata tudo num plano, entao "erra" por isso. Medido, esse
        # numero torna o impostor INALCANCAVEL -- com raio mediano de 32,9 num
        # mapa de 551 unidades, a distancia de troca cai em 2.470 unidades, e
        # zero impostores sao selecionados em qualquer pose deste mapa.
        #
        # E cobrar errado, nao so cobrar caro. O erro de tela que a selecao de
        # LOD modela e desvio de SILHUETA, e um quad que gira para encarar a
        # camera nao tem desvio de silhueta nenhum: ele reproduz exatamente a
        # silhueta assada. O que o achatamento produz e parallax errada quando
        # a camera anda de lado -- um erro que nao escala com 1/distancia e que
        # esta metrica nao representa. O que ela representa e a resolucao do
        # impostor: um texel do tile vale `texel` unidades de mundo, e e esse o
        # detalhe que ele deixa de resolver.
        #
        # O multiplo do nivel anterior existe porque a cadeia exige erro
        # estritamente crescente e porque `texel` sozinho e menor que o erro do
        # nivel anterior (0,4 contra 3,1 medianos) -- o impostor entraria antes
        # da malha simplificada, o que inverteria a ordem da cadeia.
        texel = 2.0 * span / args.tile
        error = max(entry['maximumError'] * args.error_scale, texel)
        new_draws.append((first_index, 6, world, draw['center'], draw['radius'],
                          entry['maximumLevel'] + 1, error, group_id))
        baked += 1
        if baked % 25 == 0:
            print(f'  {baked}/{len(candidates)} assados')

    if baked == 0:
        raise SystemExit('nenhum impostor produzido')

    args.out.mkdir(parents=True, exist_ok=True)
    levels = build_mip_chain(atlas)
    atlas_name = f'texture_{package.texture_count:03d}'
    atlas_bytes = write_aetx(args.out / f'{atlas_name}.aetex', atlas_width, atlas_height,
                             AETX_RGBA8_SRGB, levels)
    reduced = fallback_chain(levels)
    write_aetx(args.out / f'{atlas_name}-fallback.aetex', reduced[0][0], reduced[0][1],
               AETX_RGBA8_SRGB, reduced)

    # Registro de textura: filtro linear e mip linear (um impostor distante ocupa
    # dezenas de pixels -- amostrar por vizinho mais proximo o faria cintilar a
    # cada passo da camera), clamp nos dois eixos porque o atlas nao repete, e
    # sRGB porque o tile guarda cor de base.
    package.textures.append(struct.pack('<4I', 1 | 2 | 4 | 8 | 16, 0, 0, 0))
    impostor_texture = package.texture_count
    package.texture_count += 1

    # roughness=1, metallic=0: folhagem e dieletrica. O tile ja carrega a cor
    # sombreada do LOD0, entao o especular so acrescentaria brilho que a
    # geometria substituida nao tinha.
    flags = MATERIAL_ALPHA_MASK | MATERIAL_DOUBLE_SIDED | MATERIAL_IMPOSTOR
    package.materials.append(struct.pack(
        '<4I4f4f4ff3I', impostor_texture, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.5, flags, 0, 0))
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
    print(f'{baked} impostores assados; atlas {atlas_name}.aetex '
          f'({atlas_bytes / 2**20:.1f} MiB, {len(levels)} mips) + fallback '
          f'{reduced[0][0]}x{reduced[0][1]}')
    print(f'pacote: +{baked} draws, +{baked * 4} vertices, +1 textura, +1 material')
    if args.update_manifest:
        refresh_manifest(args.manifest, args.out, package, baked, atlas_name,
                         atlas_width, atlas_height, args.tile, atlas_bytes, args.error_scale)
        print(f'manifesto atualizado: {args.manifest}')


if __name__ == '__main__':
    main()
