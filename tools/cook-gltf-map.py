"""Cook a self-contained glTF ZIP into Aether's Android map package.

The runtime never parses JSON or decodes PNG/JPEG. Geometry is normalized into
a stable little-endian AEMAP v1 stream and textures into AETX v1 mip chains.
Pillow, numpy and astcenc are import-time tools only.
"""
import argparse
import hashlib
import io
import json
import math
import pathlib
import struct
import subprocess
import zipfile

import numpy as np
from PIL import Image

MAP_MAGIC = 0x504D4541  # AEMP
MAP_VERSION = 3
HEADER_SIZE = 144
VERTEX_STRIDE = 48
DRAW_STRIDE = 108  # v3: 96-byte v1/v2 record + lodLevel/geometricError/lodGroupId.
INVALID_TEXTURE = 0xFFFFFFFF
MATERIAL_BLEND = 1 << 0
MATERIAL_NORMAL_MAP = 1 << 1
MATERIAL_METALLIC_ROUGHNESS_MAP = 1 << 2
MATERIAL_EMISSIVE_MAP = 1 << 3
MATERIAL_ALPHA_MASK = 1 << 4
MATERIAL_DOUBLE_SIDED = 1 << 5

# LOD generation (item 2.5.4 / 7.1.6 of the plan): up to this many discrete
# levels per opaque primitive, each a fully separate draw sharing one
# lodGroupId. Blend and alpha-mask materials are deliberately excluded for
# this first slice -- see simplify_primitive_levels for why.
MAX_LOD_LEVELS = 3
# A primitive below this triangle count is not worth simplifying: the saved
# triangles would not offset one more draw call/chunk, and small props are
# the ones most likely to disappear entirely under grid-snap clustering.
LOD_MINIMUM_TRIANGLES = 256
# Cell size as a fraction of the primitive's bounding-sphere radius, one
# entry per additional level beyond level 0 (which is always the untouched
# original). Strictly increasing so geometricError is strictly increasing by
# construction, matching MapDrawRecord's documented ordering contract.
LOD_CELL_SIZE_RATIOS = (0.01, 0.03)
# Vertices only ever merge within one grid cell AND one discretized-normal
# bucket AND one discretized-UV0 bucket -- never across a hard edge or a UV
# island seam. See simplify_by_clustering.
LOD_NORMAL_BUCKET_COUNT = 6
LOD_UV_BUCKET_SIZE = 1.0 / 64.0


def align(value, alignment=16):
    return (value + alignment - 1) & ~(alignment - 1)


def srgb_decode(value):
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def srgb_encode(value):
    return np.where(value <= .0031308, value * 12.92,
                    1.055 * np.maximum(value, 0) ** (1 / 2.4) - .055)


def normalized(value):
    return value / np.maximum(np.linalg.norm(value, axis=-1, keepdims=True), 1e-8)


def snorm16(values):
    return np.rint(np.clip(values, -1.0, 1.0) * 32767.0).astype(np.int16)


def vertex_record(position, normal, tangent, uv0, uv1, color):
    """AEMAP v2: compact directions/color, preserve float position and UV.

    UV stays float32 because real imported maps can use large repeating ranges;
    blindly converting those coordinates to float16 visibly shifts texture phase.
    """
    normal4 = np.append(normal, 0.0)
    packed_normal = snorm16(normal4)
    packed_tangent = snorm16(tangent)
    packed_color = np.rint(np.clip(color, 0.0, 1.0) * 255.0).astype(np.uint8)
    return struct.pack("<3f4h4h2f2f4B", *position, *packed_normal, *packed_tangent,
                       *uv0, *uv1, *packed_color)


def simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                           cell_size, uv_bucket_size, normal_bucket_count):
    """Deterministic grid-snap vertex clustering (LOD simplification).

    Vertices merge only when they share the same position grid cell AND the
    same discretized-normal bucket AND the same discretized-UV0 bucket --
    never across a hard edge (normal bucket differs) or a UV island seam
    (UV0 bucket differs), which would otherwise blend one island's texture
    across another's geometry or flatten a sharp corner's shading. This is a
    deliberately weaker guarantee than quadric-error edge collapse (no
    boundary-preservation reasoning, no legality checks), traded for
    robustness on arbitrary/non-manifold imported geometry and for being
    simple enough to verify by hand -- see PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md.

    Representative attributes are the MEAN of every vertex in a cluster, not
    an arbitrary member: this guarantees the simplified level's bounds can
    never expand past the original's (a mean is always inside the convex
    hull of its inputs), which is exactly the property the caller's
    build-time bounds check exists to confirm holds in practice too.

    uv1 is preserved (averaged) but not used as a clustering guard: it is
    typically a lightmap/secondary channel, less seam-sensitive than uv0.

    Returns None (never a degenerate/empty result) if simplification would
    collapse to zero triangles.
    """
    if len(indices) == 0 or cell_size <= 0:
        return None
    position = np.asarray(position, dtype=np.float64)
    normal_bucket = np.rint(normal * normal_bucket_count).astype(np.int64)
    cell = np.floor(position / cell_size).astype(np.int64)
    uv_bucket = np.floor(np.asarray(uv0, dtype=np.float64) / uv_bucket_size).astype(np.int64)
    keys = [(*cell[i], *normal_bucket[i], *uv_bucket[i]) for i in range(len(position))]

    cluster_of_key = {}
    vertex_cluster = np.empty(len(position), dtype=np.int64)
    for i, key in enumerate(keys):
        cluster_id = cluster_of_key.setdefault(key, len(cluster_of_key))
        vertex_cluster[i] = cluster_id
    cluster_count = len(cluster_of_key)

    counts = np.zeros(cluster_count, dtype=np.int64)
    np.add.at(counts, vertex_cluster, 1)
    def average(attribute):
        attribute = np.asarray(attribute, dtype=np.float64)
        sums = np.zeros((cluster_count, attribute.shape[1]), dtype=np.float64)
        np.add.at(sums, vertex_cluster, attribute)
        return sums / counts[:, None]

    new_position = average(position).astype(np.float32)
    new_normal = normalized(average(normal)).astype(np.float32)
    new_tangent = average(tangent)
    new_tangent[:, :3] = normalized(new_tangent[:, :3])
    new_tangent = new_tangent.astype(np.float32)
    new_uv0 = average(uv0).astype(np.float32)
    new_uv1 = average(uv1).astype(np.float32)
    new_color = average(color).astype(np.float32)

    triangles = vertex_cluster[np.asarray(indices, dtype=np.int64)].reshape(-1, 3)
    degenerate = ((triangles[:, 0] == triangles[:, 1]) | (triangles[:, 1] == triangles[:, 2]) |
                 (triangles[:, 0] == triangles[:, 2]))
    kept = triangles[~degenerate]
    if len(kept) == 0:
        return None
    new_indices = kept.reshape(-1).astype(np.uint32)
    return new_position, new_normal, new_tangent, new_uv0, new_uv1, new_color, new_indices


def simplify_primitive_levels(position, normal, tangent, uv0, uv1, color, indices,
                              material_flags, bounds_radius):
    """Builds the full LOD level chain for one primitive: level 0 (the
    untouched original) plus up to MAX_LOD_LEVELS - 1 progressively coarser
    levels from simplify_by_clustering.

    Blend and alpha-mask materials are excluded from simplification entirely
    for this first slice (levels stays a single, untouched entry): blend's
    primitive order participates in alpha compositing (simplifying it risks
    a visibly wrong composite), and alpha-mask/cutout vegetation is exactly
    the content most likely to collapse into nothing under grid-snap
    clustering with no human able to check the result visually this cycle.
    Level generation also stops early (yielding fewer than MAX_LOD_LEVELS
    total) the moment a candidate level fails to reduce the triangle count
    versus the previous level -- an unproductive level is never emitted.

    Returns a list of dicts, each
    {"level": int, "geometricError": float,
     "position", "normal", "tangent", "uv0", "uv1", "color", "indices"},
    always at least the one original (untouched) level.
    """
    original = {"level": 0, "geometricError": 0.0, "position": position, "normal": normal,
                "tangent": tangent, "uv0": uv0, "uv1": uv1, "color": color, "indices": indices}
    levels = [original]
    triangle_count = len(indices) // 3
    if (material_flags & (MATERIAL_BLEND | MATERIAL_ALPHA_MASK)) != 0:
        return levels
    if triangle_count < LOD_MINIMUM_TRIANGLES or bounds_radius <= 0.0:
        return levels

    original_minimum = np.asarray(position, dtype=np.float64).min(axis=0)
    original_maximum = np.asarray(position, dtype=np.float64).max(axis=0)
    bounds_epsilon = max(bounds_radius * 1.0e-3, 1.0e-4)
    previous_triangle_count = triangle_count
    for level_index, ratio in enumerate(LOD_CELL_SIZE_RATIOS, start=1):
        cell_size = bounds_radius * ratio
        simplified = simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                            cell_size, LOD_UV_BUCKET_SIZE, LOD_NORMAL_BUCKET_COUNT)
        if simplified is None:
            break
        (s_position, s_normal, s_tangent, s_uv0, s_uv1, s_color, s_indices) = simplified
        new_triangle_count = len(s_indices) // 3
        if new_triangle_count >= previous_triangle_count:
            break  # No further win at this or any coarser ratio; stop here.
        simplified_minimum = s_position.astype(np.float64).min(axis=0)
        simplified_maximum = s_position.astype(np.float64).max(axis=0)
        if (np.any(simplified_minimum < original_minimum - bounds_epsilon) or
                np.any(simplified_maximum > original_maximum + bounds_epsilon)):
            raise AssertionError(
                f"LOD level {level_index} bounds expanded past the original by more than "
                f"{bounds_epsilon}; clustering invariant violated")
        geometric_error = cell_size * math.sqrt(3.0) / 2.0
        if geometric_error <= levels[-1]["geometricError"]:
            raise AssertionError(
                f"LOD level {level_index} geometricError {geometric_error} did not increase past "
                f"level {level_index - 1}'s {levels[-1]['geometricError']}")
        levels.append({"level": level_index, "geometricError": geometric_error, "position": s_position,
                       "normal": s_normal, "tangent": s_tangent, "uv0": s_uv0, "uv1": s_uv1,
                       "color": s_color, "indices": s_indices})
        previous_triangle_count = new_triangle_count
        if len(levels) >= MAX_LOD_LEVELS:
            break
    return levels


def alpha_coverage(alpha, cutoff):
    """Fracao de texels que passam no teste de alpha.

    E a unica grandeza que o runtime enxerga de um material alpha-tested: o
    fragmento existe ou nao existe. Media de alpha nao e cobertura.
    """
    if alpha.size == 0:
        return 0.0
    return float(np.count_nonzero(alpha >= cutoff)) / float(alpha.size)


def scale_alpha_to_coverage(alpha, cutoff, target_coverage, iterations=16):
    """Reescala o alpha de um mip para reproduzir a cobertura do nivel base.

    Um box filter preserva a MEDIA do alpha, nao a fracao acima do cutoff. Em
    folhagem, galho fino e cerca, as duas divergem rapido: medir a cena real
    mostrou a cobertura caindo a zero por volta do mip 5, ou seja, a vegetacao
    literalmente desaparece com a distancia, e sobrando ~30% nos mips 1-2.

    A correcao e a de Castano/NVIDIA, tambem usada pelo "Mip Maps Preserve
    Coverage" da Unity: buscar o multiplicador de alpha que faz o mesmo cutoff
    render a mesma cobertura. Cobertura e monotonica nao-decrescente no fator,
    entao busca binaria converge.

    Cobertura 0 ou 1 no nivel base nao tem o que preservar e sai intacta --
    evita amplificar ruido de uma textura totalmente transparente ou opaca.
    """
    if target_coverage <= 0.0 or target_coverage >= 1.0 or alpha.size == 0:
        return alpha
    # Invariante da busca: `high` e sempre uma escala conhecida por atingir a
    # cobertura alvo, `low` sempre uma que fica abaixo. Devolver `high` -- e nao
    # a ultima sonda -- e o que garante que o mip nunca sai com MENOS cobertura
    # que o nivel base.
    #
    # Isso importa porque cobertura e uma funcao ESCADA da escala: o alpha de um
    # atlas de recorte e quase binario, e o box filter de 2x2 produz poucos
    # valores distintos, entao o alvo costuma cair entre dois degraus e nao ha
    # escala que o atinja exatamente. Escolher o degrau de cima mantem folhagem
    # levemente mais densa; escolher o de baixo removeria vegetacao, que e o
    # defeito que esta funcao existe para corrigir.
    low, high = 0.0, 4.0
    for _ in range(iterations):
        middle = (low + high) * 0.5
        if alpha_coverage(np.clip(alpha * middle, 0.0, 1.0), cutoff) < target_coverage:
            low = middle
        else:
            high = middle
    return np.clip(alpha * high, 0.0, 1.0)


def coverage_alpha_cutoffs(gltf, inferred_cutouts=frozenset()):
    """Indice de textura base-color -> cutoff de alpha a preservar.

    A semantica de cobertura vem do MATERIAL, nunca do nome da textura nem da
    cena (ADR-014 e a secao 8 do plano de otimizacao proibem regra por cena).
    Uma textura carrega cobertura quando algum material que a amostra como base
    color e alpha-tested -- MASK explicito do glTF, ou BLEND que a heuristica
    de atlas reconheceu como recorte.

    Quando materiais que compartilham a mesma textura discordam do cutoff, o
    MENOR vence. Nenhuma cadeia de mip e correta para dois cutoffs ao mesmo
    tempo, e errar para o lado do menor preserva texels a mais: folhagem
    levemente mais densa ao longe custa alguns fragmentos, enquanto errar para
    o outro lado remove arvores -- exatamente o que a secao 8 proibe.
    """
    cutoffs = {}
    for material in gltf.get("materials", []):
        base = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
        if base is None:
            continue
        index = base["index"]
        alpha_mode = material.get("alphaMode", "OPAQUE")
        alpha_tested = alpha_mode == "MASK" or (alpha_mode == "BLEND" and index in inferred_cutouts)
        if not alpha_tested:
            continue
        cutoff = float(material.get("alphaCutoff", .5))
        cutoffs[index] = min(cutoffs[index], cutoff) if index in cutoffs else cutoff
    return cutoffs


def halve(value):
    height, width, channels = value.shape
    if height == 1 and width == 1:
        return value
    if height == 1:
        return value.reshape(1, width // 2, 2, channels).mean(axis=2)
    if width == 1:
        return value.reshape(height // 2, 2, 1, channels).mean(axis=1)
    return value.reshape(height // 2, 2, width // 2, 2, channels).mean(axis=(1, 3))


def write_aetx(path, width, height, encoding, levels):
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", 0x58544541, 1, width, height,
                                 encoding, len(levels), len(payload)) + payload)


def astc_level(astcenc, cache, name, mip, rgba, srgb, quality, jobs):
    png = cache / f"{name}-{mip}.png"
    encoded = png.with_suffix(".astc")
    Image.fromarray(rgba, "RGBA").save(png, compress_level=1)
    subprocess.run([str(astcenc), "-cs" if srgb else "-cl", str(png), str(encoded),
                    "6x6", quality, "-j", str(jobs), "-silent"], check=True)
    blob = encoded.read_bytes()
    if len(blob) < 16 or blob[:4] != bytes.fromhex("13aba15c"):
        raise ValueError(f"invalid ASTC payload for {name} mip {mip}")
    return blob[16:]


def cook_texture(astcenc, cache, output, name, source, srgb, normal_map, quality, jobs,
                 coverage_cutoff=None):
    pixels = np.asarray(Image.open(io.BytesIO(source)).convert("RGBA"), dtype=np.float32) / 255.0
    width, height = pixels.shape[1], pixels.shape[0]
    if width == 0 or height == 0 or width & (width - 1) or height & (height - 1):
        raise ValueError(f"{name}: power-of-two texture required, got {width}x{height}")
    if srgb:
        pixels[..., :3] = srgb_decode(pixels[..., :3])
    elif normal_map:
        pixels[..., :3] = pixels[..., :3] * 2.0 - 1.0

    levels, fallback = [], []
    fallback_width = fallback_height = 0
    coverage_target = (alpha_coverage(pixels[..., 3], coverage_cutoff)
                       if coverage_cutoff is not None else None)
    coverage_by_mip = []
    mip = 0
    while True:
        display = pixels.copy()
        if srgb:
            display[..., :3] = srgb_encode(display[..., :3])
        elif normal_map:
            display[..., :3] = normalized(display[..., :3]) * .5 + .5
        if coverage_target is not None and mip > 0:
            # A escala sai de `display`, nao de `pixels`: o proximo halve()
            # precisa continuar a partir da cadeia box-filtrada original.
            # Reescalar antes de reduzir empilharia o erro nivel a nivel.
            display[..., 3] = scale_alpha_to_coverage(display[..., 3], coverage_cutoff,
                                                      coverage_target)
        rgba = np.rint(np.clip(display, 0, 1) * 255).astype(np.uint8)
        if coverage_target is not None:
            coverage_by_mip.append(round(alpha_coverage(rgba[..., 3] / 255.0, coverage_cutoff), 6))
        current_height, current_width = rgba.shape[:2]
        levels.append(astc_level(astcenc, cache, name, mip, rgba, srgb, quality, jobs))
        # Development fallback stays bounded: ASTC retains the full source;
        # devices without ASTC receive at most 512 px per axis instead of
        # multiplying APK and runtime memory by the complete RGBA8 corpus.
        if current_width <= 512 and current_height <= 512:
            if not fallback:
                fallback_width, fallback_height = current_width, current_height
            fallback.append(rgba.tobytes())
        print(f"{name}: mip {mip} {current_width}x{current_height}", flush=True)
        if current_width == 1 and current_height == 1:
            break
        pixels = halve(pixels)
        if normal_map:
            pixels[..., :3] = normalized(pixels[..., :3])
        mip += 1

    # Gate de cozimento, não aviso: uma textura de cobertura cujo mip zera é
    # vegetação que desaparece com a distância. Falhar aqui é a única forma de
    # o defeito não chegar ao APK em silêncio — foi assim que ele passou
    # despercebido até ser medido.
    if coverage_target is not None:
        for mip_index, coverage in enumerate(coverage_by_mip):
            if coverage <= 0.0 < coverage_target:
                raise ValueError(
                    f"{name}: mip {mip_index} perdeu toda a cobertura alpha "
                    f"(alvo {coverage_target:.4f}, cutoff {coverage_cutoff:.3f})")

    write_aetx(output / f"{name}.aetex", width, height, 1 if srgb else 2, levels)
    write_aetx(output / f"{name}-fallback.aetex", fallback_width, fallback_height,
               3 if srgb else 4, fallback)
    return width, height, len(levels), coverage_by_mip


def accessor(gltf, binary, index):
    item = gltf["accessors"][index]
    if "sparse" in item:
        raise ValueError("sparse accessors are not supported by AEMAP v1")
    view = gltf["bufferViews"][item["bufferView"]]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    dtype = {5125: np.dtype("<u4"), 5126: np.dtype("<f4")}.get(item["componentType"])
    if dtype is None:
        raise ValueError(f"unsupported accessor component type {item['componentType']}")
    offset = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", dtype.itemsize * components)
    array = np.ndarray((item["count"], components), dtype=dtype, buffer=binary,
                       offset=offset, strides=(stride, dtype.itemsize)).copy()
    return array[:, 0] if components == 1 else array


def multiply_column_major(left, right):
    a = np.asarray(left, dtype=np.float64).reshape((4, 4), order="F")
    b = np.asarray(right, dtype=np.float64).reshape((4, 4), order="F")
    return (a @ b).reshape(16, order="F").astype(np.float32)


def scene_nodes(gltf):
    identity = np.identity(4, dtype=np.float32).reshape(16, order="F")
    roots = gltf["scenes"][gltf.get("scene", 0)]["nodes"]
    result = []

    def visit(index, parent):
        node = gltf["nodes"][index]
        local = np.asarray(node.get("matrix", identity), dtype=np.float32)
        world = multiply_column_major(parent, local)
        result.append((index, world))
        for child in node.get("children", []):
            visit(child, world)

    for root in roots:
        visit(root, identity)
    return result


def transform_bounds(minimum, maximum, matrix):
    corners = np.array([[x, y, z, 1.0] for x in (minimum[0], maximum[0])
                        for y in (minimum[1], maximum[1])
                        for z in (minimum[2], maximum[2])], dtype=np.float64)
    mat = np.asarray(matrix, dtype=np.float64).reshape((4, 4), order="F")
    transformed = (mat @ corners.T).T[:, :3]
    return transformed.min(axis=0), transformed.max(axis=0)


def texture_semantics(gltf):
    result = {}

    def use(texture_info, kind):
        if texture_info is None:
            return
        key = (texture_info["index"], kind == "srgb")
        result.setdefault(key, len(result))

    for material in gltf["materials"]:
        pbr = material.get("pbrMetallicRoughness", {})
        use(pbr.get("baseColorTexture"), "srgb")
        use(material.get("emissiveTexture"), "srgb")
        use(material.get("normalTexture"), "normal")
        use(pbr.get("metallicRoughnessTexture"), "linear")
    return result


def alpha_coverage_is_cutout(source):
    """Recognize hard coverage atlases incorrectly exported as BLEND.

    A large transparent region plus mostly binary coverage is characteristic
    of leaves, grass and fences. Soft road edges, puddles and painted decals
    intentionally stay blended. Explicit glTF MASK always wins independently
    of this compatibility heuristic.
    """
    alpha = np.asarray(Image.open(io.BytesIO(source)).convert("RGBA"), dtype=np.uint8)[..., 3]
    count = alpha.size
    transparent = np.count_nonzero(alpha == 0) / count
    binary = (np.count_nonzero(alpha == 0) + np.count_nonzero(alpha == 255)) / count
    return transparent >= .5 and binary >= .6


def material_record(material, texture_map, inferred_cutouts=frozenset()):
    pbr = material.get("pbrMetallicRoughness", {})
    infos = [pbr.get("baseColorTexture"), material.get("normalTexture"),
             pbr.get("metallicRoughnessTexture"), material.get("emissiveTexture")]
    kinds = [True, False, False, True]
    textures = [texture_map.get((info["index"], kind), INVALID_TEXTURE) if info else INVALID_TEXTURE
                for info, kind in zip(infos, kinds)]
    texcoords = sum((int(info.get("texCoord", 0)) & 3) << (slot * 2)
                    for slot, info in enumerate(infos) if info)
    flags = 0
    alpha_mode = material.get("alphaMode", "OPAQUE")
    base_texture = infos[0]["index"] if infos[0] else None
    if alpha_mode == "MASK" or (alpha_mode == "BLEND" and base_texture in inferred_cutouts):
        flags |= MATERIAL_ALPHA_MASK
    elif alpha_mode == "BLEND":
        flags |= MATERIAL_BLEND
    if infos[1]: flags |= MATERIAL_NORMAL_MAP
    if infos[2]: flags |= MATERIAL_METALLIC_ROUGHNESS_MAP
    if infos[3]: flags |= MATERIAL_EMISSIVE_MAP
    if material.get("doubleSided", False): flags |= MATERIAL_DOUBLE_SIDED
    base = pbr.get("baseColorFactor", [1, 1, 1, 1])
    emissive = material.get("emissiveFactor", [0, 0, 0])
    specular = material.get("extensions", {}).get("KHR_materials_specular", {}).get("specularFactor", 1.0)
    values = textures + list(base) + list(emissive) + [1.0,
              pbr.get("roughnessFactor", 1.0), pbr.get("metallicFactor", 1.0),
              material.get("normalTexture", {}).get("scale", 1.0), specular,
              material.get("alphaCutoff", .5), flags, texcoords, 0]
    return struct.pack("<4I4f4f4ff3I", *values)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--astcenc", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("samples/dirt-road/Imported"))
    parser.add_argument("--cache", type=pathlib.Path, default=pathlib.Path("build/dirt-road/cook"))
    parser.add_argument("--quality", choices=["-fast", "-medium", "-thorough"], default="-medium")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)

    source_bytes = args.source.read_bytes()
    with zipfile.ZipFile(io.BytesIO(source_bytes)) as archive:
        names = set(archive.namelist())
        if not {"scene.gltf", "scene.bin", "license.txt"}.issubset(names):
            raise ValueError("expected scene.gltf, scene.bin and license.txt at ZIP root")
        if any(pathlib.PurePosixPath(name).is_absolute() or ".." in pathlib.PurePosixPath(name).parts for name in names):
            raise ValueError("unsafe path in source ZIP")
        gltf = json.loads(archive.read("scene.gltf"))
        binary = archive.read(gltf["buffers"][0]["uri"])
        if len(binary) != gltf["buffers"][0]["byteLength"]:
            raise ValueError("glTF binary length mismatch")

        texture_map = texture_semantics(gltf)
        texture_records = []
        texture_sources = []
        # A heuristica de atlas roda antes do cozimento porque a semantica de
        # cobertura de uma textura depende dos MATERIAIS que a usam, e o cutoff
        # deles precisa estar resolvido antes de gerar o primeiro mip.
        sources = {}
        inferred_cutouts = set()
        for (texture_index, srgb), _ in sorted(texture_map.items(), key=lambda item: item[1]):
            texture = gltf["textures"][texture_index]
            source = archive.read(gltf["images"][texture["source"]]["uri"])
            sources[(texture_index, srgb)] = source
            if srgb and alpha_coverage_is_cutout(source):
                inferred_cutouts.add(texture_index)
        coverage_cutoffs = coverage_alpha_cutoffs(gltf, inferred_cutouts)
        for (texture_index, srgb), cooked_index in sorted(texture_map.items(), key=lambda item: item[1]):
            texture = gltf["textures"][texture_index]
            image = gltf["images"][texture["source"]]
            source = sources[(texture_index, srgb)]
            normal_map = any(material.get("normalTexture", {}).get("index") == texture_index
                             for material in gltf["materials"]) and not srgb
            # Só a base color carrega cobertura; normal/ARM/emissive de um
            # material recortado nao sao alpha-tested.
            coverage_cutoff = coverage_cutoffs.get(texture_index) if srgb else None
            *dimensions, coverage_by_mip = cook_texture(
                args.astcenc, args.cache, args.out, f"texture_{cooked_index:03d}", source, srgb,
                normal_map, args.quality, args.jobs, coverage_cutoff)
            sampler = gltf.get("samplers", [{}])[texture.get("sampler", 0)]
            flags = (1 if sampler.get("magFilter", 9729) == 9729 else 0)
            flags |= (2 if sampler.get("minFilter", 9987) in (9987, 9985) else 0)
            flags |= (4 if sampler.get("wrapS", 10497) == 33071 else 0)
            flags |= (8 if sampler.get("wrapT", 10497) == 33071 else 0)
            flags |= (16 if srgb else 0)
            texture_records.append(struct.pack("<4I", flags, 0, 0, 0))
            entry = {"cookedIndex": cooked_index, "gltfTexture": texture_index,
                     "image": image["uri"], "srgb": srgb,
                     "normalMap": normal_map, "dimensions": dimensions,
                     "sha256": hashlib.sha256(source).hexdigest()}
            # Versiona a decisao de semantica no manifesto: qual cutoff foi
            # preservado e a cobertura medida por nivel. Sem isso, "a folhagem
            # sumiu no mip 5" so aparece olhando a imagem no aparelho.
            if coverage_cutoff is not None:
                entry["alphaSemantics"] = "coverage"
                entry["coverageCutoff"] = coverage_cutoff
                entry["coverageByMip"] = coverage_by_mip
            texture_sources.append(entry)

        materials = [material_record(material, texture_map, inferred_cutouts)
                     for material in gltf["materials"]]
        vertices, indices, draws = [], [], []
        lod_level_counts = {}  # For the manifest: level index -> draws generated at that level.
        next_lod_group_id = 0
        world_min = np.array([np.inf, np.inf, np.inf])
        world_max = -world_min
        for node_index, world in scene_nodes(gltf):
            node = gltf["nodes"][node_index]
            if "mesh" not in node:
                continue
            for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
                attrs = primitive["attributes"]
                position = accessor(gltf, binary, attrs["POSITION"]).astype(np.float32)
                normal = accessor(gltf, binary, attrs["NORMAL"]).astype(np.float32)
                tangent = accessor(gltf, binary, attrs["TANGENT"]).astype(np.float32) if "TANGENT" in attrs else None
                uv0 = accessor(gltf, binary, attrs["TEXCOORD_0"]).astype(np.float32)
                uv1 = accessor(gltf, binary, attrs.get("TEXCOORD_1", attrs["TEXCOORD_0"])).astype(np.float32)
                color = accessor(gltf, binary, attrs["COLOR_0"]).astype(np.float32)
                local_indices = accessor(gltf, binary, primitive["indices"]).astype(np.uint32)
                if tangent is None:
                    tangent = np.zeros((len(position), 4), dtype=np.float32)
                    tangent[:, 0] = 1.0
                    tangent[:, 3] = 1.0
                accessor_bounds = gltf["accessors"][attrs["POSITION"]]
                minimum, maximum = transform_bounds(accessor_bounds["min"], accessor_bounds["max"], world)
                world_min = np.minimum(world_min, minimum)
                world_max = np.maximum(world_max, maximum)
                # World-space bounds are reused unchanged for every LOD level
                # of this primitive: simplify_by_clustering's mean-based
                # representative positions can only shrink the local-space
                # bounds (never expand them, enforced by its own assertion),
                # so the ORIGINAL (level 0) world bounds remain a valid,
                # conservative superset for culling every coarser level too.
                center = (minimum + maximum) * .5
                radius = float(np.linalg.norm(maximum - minimum) * .5)
                local_minimum = np.asarray(accessor_bounds["min"], dtype=np.float64)
                local_maximum = np.asarray(accessor_bounds["max"], dtype=np.float64)
                local_radius = float(np.linalg.norm(local_maximum - local_minimum) * .5)
                # MapMaterialRecord.flags sits at byte offset 68 (4 texture
                # indices + baseColorFactor[4] + emissiveFactorAndStrength[4]
                # + roughness/metallic/normalScale/specular + alphaCutoff =
                # 16+16+16+16+4 bytes), matching material_record's packing
                # order and native/renderer/map_package.h's struct layout.
                material_flags = struct.unpack_from("<I", materials[primitive.get("material", 0)],
                                                    offset=68)[0]
                lod_levels = simplify_primitive_levels(position, normal, tangent, uv0, uv1, color,
                                                       local_indices, material_flags, local_radius)
                lod_group_id = next_lod_group_id
                next_lod_group_id += 1
                for entry in lod_levels:
                    base_vertex = len(vertices)
                    for values in zip(entry["position"], entry["normal"], entry["tangent"], entry["uv0"],
                                      entry["uv1"], entry["color"]):
                        vertices.append(vertex_record(*values))
                    first_index = len(indices)
                    indices.extend(int(value) + base_vertex for value in entry["indices"])
                    draws.append(struct.pack(
                        "<4I16f4fIfI", first_index, len(entry["indices"]), 0,
                        primitive.get("material", 0), *world, *center, radius,
                        entry["level"], entry["geometricError"], lod_group_id))
                    lod_level_counts[entry["level"]] = lod_level_counts.get(entry["level"], 0) + 1

        center = (world_min + world_max) * .5
        extent = world_max - world_min
        camera = np.array([center[0], center[1] + max(6.0, extent[1] * .08),
                           world_min[2] - max(18.0, extent[2] * .22)], dtype=np.float32)
        texture_offset = HEADER_SIZE
        material_offset = align(texture_offset + len(texture_records) * 16)
        draw_offset = align(material_offset + len(materials) * 80)
        vertex_offset = align(draw_offset + len(draws) * DRAW_STRIDE)
        index_offset = align(vertex_offset + len(vertices) * VERTEX_STRIDE)
        total_size = index_offset + len(indices) * 4
        package = bytearray(total_size)
        header = struct.pack("<10I5Q6f7f3I", MAP_MAGIC, MAP_VERSION, HEADER_SIZE,
                             VERTEX_STRIDE, len(texture_records), len(materials), len(draws),
                             len(vertices), len(indices), 0, texture_offset, material_offset,
                             draw_offset, vertex_offset, index_offset, *world_min, *world_max,
                             *camera, 0.0, 0.0, .1, max(1000.0, float(np.linalg.norm(extent) * 4)),
                             len(indices) // 3, 0, 0)
        if len(header) != HEADER_SIZE:
            raise AssertionError(f"header size {len(header)} != {HEADER_SIZE}")
        package[:HEADER_SIZE] = header
        package[texture_offset:texture_offset + len(texture_records) * 16] = b"".join(texture_records)
        package[material_offset:material_offset + len(materials) * 80] = b"".join(materials)
        package[draw_offset:draw_offset + len(draws) * DRAW_STRIDE] = b"".join(draws)
        package[vertex_offset:vertex_offset + len(vertices) * VERTEX_STRIDE] = b"".join(vertices)
        package[index_offset:] = struct.pack(f"<{len(indices)}I", *indices)
        (args.out / "scene.aemap").write_bytes(package)
        license_text = archive.read("license.txt").decode("utf-8-sig")
        (args.out.parent / "LICENSE.txt").write_text(license_text, encoding="utf8")

    outputs = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(args.out.iterdir()) if path.is_file()}
    manifest = {"version": 3, "format": "AEMAP-3", "sourceZip": args.source.name,
                "sourceSha256": hashlib.sha256(source_bytes).hexdigest(),
                "license": "CC-BY-4.0", "author": "99.Miles",
                "source": "https://sketchfab.com/3d-models/update-dirt-road-through-forest-c4676cdf7715484382400ff63faffd45",
                "encoder": f"astcenc 5.7.0 {args.quality} 6x6",
                "statistics": {"textures": len(texture_records), "materials": len(materials),
                               "draws": len(draws), "vertices": len(vertices),
                               "triangles": len(indices) // 3,
                               "boundsMin": world_min.tolist(), "boundsMax": world_max.tolist(),
                               "defaultCamera": camera.tolist(),
                               # Draws generated per LOD level index (0 = original/untouched);
                               # level 0's count also includes every primitive LOD was
                               # skipped for (blend/alpha-mask materials, primitives below
                               # LOD_MINIMUM_TRIANGLES). See simplify_primitive_levels.
                               "lodDrawsByLevel": {str(level): count
                                                   for level, count in sorted(lod_level_counts.items())}},
                "textures": texture_sources, "outputs": outputs}
    (args.out.parent / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")


if __name__ == "__main__":
    main()
