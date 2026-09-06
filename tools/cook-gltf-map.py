"""Cook a self-contained glTF ZIP into Aether's Android map package.

The runtime never parses JSON or decodes PNG/JPEG. Geometry is normalized into
a stable little-endian AEMAP v3 stream and textures into AETX v1 mip chains.
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
MATERIAL_WATER = 1 << 9

# LOD generation (item 2.5.4 / 7.1.6 of the plan): up to this many discrete
# levels per opaque or alpha-tested primitive, each a fully separate draw
# sharing one lodGroupId. Blended materials remain excluded because draw order
# participates in composition; cutout vegetation is specifically a major LOD
# target and uses the same complementary dither transition as opaque geometry.
MAX_LOD_LEVELS = 3
# A primitive below this triangle count is not worth simplifying: the saved
# triangles would not offset one more draw call/chunk, and small props are
# the ones most likely to disappear entirely under grid-snap clustering.
# Spatial cells can contain a small stand of cards/branches. 32 triangles is
# still enough to amortize one indirect command when the accepted level saves
# >=10%; the reduction gate below prevents metadata-only "LODs".
LOD_MINIMUM_TRIANGLES = 32
# Cell size as a fraction of the primitive's bounding-sphere radius, one
# entry per additional level beyond level 0 (which is always the untouched
# original). Strictly increasing so geometricError is strictly increasing by
# construction, matching MapDrawRecord's documented ordering contract.
LOD_CELL_SIZE_RATIOS = (0.01, 0.03)
# Vertices only ever merge within one grid cell and one discretized-normal
# bucket. UV0 contributes a guard only at an actual island seam (two source
# vertices at the same position with different UVs); using every UV bucket in
# every key prevents almost all simplification on ordinary continuous meshes.
# See simplify_by_clustering.
LOD_NORMAL_BUCKET_COUNT = 6
LOD_UV_BUCKET_SIZE = 1.0 / 64.0
# A LOD decision must describe a local part of a large map, not one giant
# imported primitive whose bounding sphere contains the camera everywhere.
# This matches SpatialRenderChunkSettings' production default: packages leave
# the cooker already partitioned, while the runtime remains a compatibility
# safety net for legacy packages.
LOD_GROUP_TRIANGLES = 2048
LOD_GROUP_WORLD_SIZE = 64.0
# Each emitted level adds draw metadata and can temporarily overlap its
# neighbor during dither. Tiny reductions lose on mobile; keep scanning coarser
# ratios, but only persist a level that saves at least this fraction versus the
# previously accepted level.
LOD_MINIMUM_TRIANGLE_REDUCTION = 0.10
# Alpha-tested vegetation is commonly authored as thousands of disconnected
# quads/cards. Vertex clustering cannot reduce a two-triangle card, so coarse
# levels reduce card density while retaining larger components first.
LOD_COVERAGE_COMPONENT_RATIOS = (0.65, 0.35)


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
                           cell_size, uv_bucket_size, normal_bucket_count,
                           return_source_indices=False):
    """Deterministic grid-snap vertex clustering (LOD simplification).

    Vertices merge only when they share the same position grid cell and the
    same discretized-normal bucket.  UV0 is added to the key only for source
    positions that actually occur with multiple UV buckets: those duplicate
    vertices mark an island seam and must remain separate.  Continuous UV
    gradients are averaged like the other attributes; treating every varying
    UV as a seam would make a textured grid impossible to simplify.  This
    prevents blending one island across another and keeps hard edges (normal
    bucket differs). This is a
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
    collapse to zero triangles. When ``return_source_indices`` is true, an
    eighth array maps every cluster to the closest source vertex. Legacy
    package migration uses that mapping to reuse LOD0 vertices instead of
    duplicating averaged attributes in memory; normal cooking keeps the mean.
    """
    if len(indices) == 0 or cell_size <= 0:
        return None
    position = np.asarray(position, dtype=np.float64)
    normal_bucket = np.rint(normal * normal_bucket_count).astype(np.int64)
    cell = np.floor(position / cell_size).astype(np.int64)
    uv_bucket = np.floor(np.asarray(uv0, dtype=np.float64) / uv_bucket_size).astype(np.int64)
    # A glTF UV seam is represented by duplicate positions with different UVs.
    # Detect that exact topology signal once; ordinary neighboring vertices on
    # a continuous chart deliberately receive the same neutral guard.
    uv_buckets_by_position = {}
    position_keys = [np.asarray(position[i], dtype=np.float32).tobytes()
                     for i in range(len(position))]
    for i, position_key in enumerate(position_keys):
        uv_buckets_by_position.setdefault(position_key, set()).add(tuple(uv_bucket[i]))
    seam_guards = [tuple(uv_bucket[i]) if len(uv_buckets_by_position[position_keys[i]]) > 1
                   else (0, 0) for i in range(len(position))]
    keys = [(*cell[i], *normal_bucket[i], *seam_guards[i]) for i in range(len(position))]

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

    representative_source_indices = np.zeros(cluster_count, dtype=np.uint32)
    representative_distances = np.full(cluster_count, np.inf, dtype=np.float64)
    for source_index, cluster_index in enumerate(vertex_cluster):
        delta = position[source_index] - new_position[cluster_index]
        distance_squared = float(np.dot(delta, delta))
        if distance_squared < representative_distances[cluster_index]:
            representative_distances[cluster_index] = distance_squared
            representative_source_indices[cluster_index] = source_index

    triangles = vertex_cluster[np.asarray(indices, dtype=np.int64)].reshape(-1, 3)
    degenerate = ((triangles[:, 0] == triangles[:, 1]) | (triangles[:, 1] == triangles[:, 2]) |
                 (triangles[:, 0] == triangles[:, 2]))
    kept = triangles[~degenerate]
    if len(kept) == 0:
        return None
    new_indices = kept.reshape(-1).astype(np.uint32)
    result = (new_position, new_normal, new_tangent, new_uv0, new_uv1, new_color, new_indices)
    if not return_source_indices:
        return result
    representative_positions = position[representative_source_indices]
    displacement = position - representative_positions[vertex_cluster]
    measured_geometric_error = float(np.linalg.norm(displacement, axis=1).max())
    return result + (representative_source_indices, measured_geometric_error)


def simplify_coverage_component_levels(position, normal, tangent, uv0, uv1, color, indices,
                                       reuse_source_vertices=False):
    """Build density LODs for disconnected alpha-tested cards/components.

    Connected components are derived from shared vertex indices. Larger
    components are retained first; equal-size components use their centroid
    and source root as a deterministic tiebreaker. The error stored for a
    level is the largest local-space bounding radius removed at that level,
    which lets the normal projected-pixel selector defer removal until those
    cards are small on screen.
    """
    indices = np.asarray(indices, dtype=np.uint32)
    triangle_count = len(indices) // 3
    if triangle_count < LOD_MINIMUM_TRIANGLES:
        return []
    parent = np.arange(len(position), dtype=np.int64)

    def find(vertex):
        vertex = int(vertex)
        while parent[vertex] != vertex:
            parent[vertex] = parent[parent[vertex]]
            vertex = int(parent[vertex])
        return vertex

    def union(left, right):
        left_root, right_root = find(left), find(right)
        if left_root != right_root:
            parent[right_root] = left_root

    triangles = indices.reshape((-1, 3))
    for triangle in triangles:
        union(triangle[0], triangle[1])
        union(triangle[1], triangle[2])
    component_triangles = {}
    for triangle_index, triangle in enumerate(triangles):
        component_triangles.setdefault(find(triangle[0]), []).append(triangle_index)
    if len(component_triangles) < 4:
        return []

    components = []
    for root, triangle_indices in component_triangles.items():
        used = np.unique(triangles[triangle_indices].reshape(-1))
        component_position = np.asarray(position, dtype=np.float64)[used]
        minimum = component_position.min(axis=0)
        maximum = component_position.max(axis=0)
        center = (minimum + maximum) * 0.5
        radius = float(np.linalg.norm(component_position - center, axis=1).max())
        components.append({"root": root, "triangles": triangle_indices,
                           "triangleCount": len(triangle_indices), "radius": radius,
                           "center": tuple(float(value) for value in center)})
    components.sort(key=lambda item: (-item["radius"], item["center"], item["root"]))

    original = {"level": 0, "geometricError": 0.0, "position": position, "normal": normal,
                "tangent": tangent, "uv0": uv0, "uv1": uv1, "color": color, "indices": indices}
    if reuse_source_vertices:
        original["sourceVertexIndices"] = np.arange(len(position), dtype=np.uint32)
    levels = [original]
    previous_count = triangle_count
    previous_error = 0.0
    for ratio in LOD_COVERAGE_COMPONENT_RATIOS:
        target_count = max(1, math.ceil(triangle_count * ratio))
        retained_roots = set()
        retained_count = 0
        for component in components:
            retained_roots.add(component["root"])
            retained_count += component["triangleCount"]
            if retained_count >= target_count:
                break
        kept_triangle_indices = [triangle_index for root, values in component_triangles.items()
                                 if root in retained_roots for triangle_index in values]
        kept_triangle_indices.sort()
        kept = triangles[kept_triangle_indices].reshape(-1).astype(np.uint32)
        kept_count = len(kept) // 3
        minimum_saved = max(1, math.ceil(previous_count * LOD_MINIMUM_TRIANGLE_REDUCTION))
        if previous_count - kept_count < minimum_saved:
            continue
        removed = [component["radius"] for component in components
                   if component["root"] not in retained_roots]
        if not removed:
            continue
        error = max(removed)
        if error <= previous_error:
            error = math.nextafter(previous_error, math.inf)
        level = {"level": len(levels), "geometricError": error, "position": position,
                 "normal": normal, "tangent": tangent, "uv0": uv0, "uv1": uv1,
                 "color": color, "indices": kept}
        if reuse_source_vertices:
            level["sourceVertexIndices"] = np.arange(len(position), dtype=np.uint32)
        levels.append(level)
        previous_count = kept_count
        previous_error = error
        if len(levels) >= MAX_LOD_LEVELS:
            break
    return levels if len(levels) > 1 else []


def simplify_primitive_levels(position, normal, tangent, uv0, uv1, color, indices,
                              material_flags, bounds_radius, reuse_source_vertices=False):
    """Builds the full LOD level chain for one primitive: level 0 (the
    untouched original) plus up to MAX_LOD_LEVELS - 1 progressively coarser
    levels from simplify_by_clustering.

    Blend materials are excluded from simplification entirely (levels stays a
    single, untouched entry) because primitive order participates in alpha
    compositing. Alpha-mask/cutout vegetation is allowed: zero-triangle
    candidates are rejected below, UV islands/hard normals remain guarded,
    and runtime transitions use complementary screen-space dither.
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
    if reuse_source_vertices:
        original["sourceVertexIndices"] = np.arange(len(position), dtype=np.uint32)
    levels = [original]
    triangle_count = len(indices) // 3
    if (material_flags & MATERIAL_BLEND) != 0:
        return levels
    if triangle_count < LOD_MINIMUM_TRIANGLES or bounds_radius <= 0.0:
        return levels
    if (material_flags & MATERIAL_ALPHA_MASK) != 0:
        coverage_levels = simplify_coverage_component_levels(
            position, normal, tangent, uv0, uv1, color, indices, reuse_source_vertices)
        if coverage_levels:
            return coverage_levels

    original_minimum = np.asarray(position, dtype=np.float64).min(axis=0)
    original_maximum = np.asarray(position, dtype=np.float64).max(axis=0)
    bounds_epsilon = max(bounds_radius * 1.0e-3, 1.0e-4)
    previous_triangle_count = triangle_count
    for ratio in LOD_CELL_SIZE_RATIOS:
        cell_size = bounds_radius * ratio
        simplified = simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                            cell_size, LOD_UV_BUCKET_SIZE, LOD_NORMAL_BUCKET_COUNT,
                                            return_source_indices=reuse_source_vertices)
        if simplified is None:
            break
        (s_position, s_normal, s_tangent, s_uv0, s_uv1, s_color, s_indices) = simplified[:7]
        new_triangle_count = len(s_indices) // 3
        minimum_saved = max(1, math.ceil(previous_triangle_count *
                                         LOD_MINIMUM_TRIANGLE_REDUCTION))
        if previous_triangle_count - new_triangle_count < minimum_saved:
            # A fine grid may preserve every triangle while a later, coarser
            # grid still produces a useful level.  Skipping this ratio (rather
            # than terminating the chain) is essential for large imported
            # primitives whose vertex spacing falls between our two cells.
            continue
        level_index = len(levels)
        simplified_minimum = s_position.astype(np.float64).min(axis=0)
        simplified_maximum = s_position.astype(np.float64).max(axis=0)
        if (np.any(simplified_minimum < original_minimum - bounds_epsilon) or
                np.any(simplified_maximum > original_maximum + bounds_epsilon)):
            raise AssertionError(
                f"LOD level {level_index} bounds expanded past the original by more than "
                f"{bounds_epsilon}; clustering invariant violated")
        # Packages that reuse source vertices can store the exact maximum
        # source-to-representative displacement measured by the simplifier.
        # The cell diagonal remains the conservative bound for averaged output
        # where no source representative mapping was requested.
        geometric_error = (simplified[8] if reuse_source_vertices else
                           cell_size * math.sqrt(3.0) / 2.0)
        if geometric_error <= levels[-1]["geometricError"]:
            raise AssertionError(
                f"LOD level {level_index} geometricError {geometric_error} did not increase past "
                f"level {level_index - 1}'s {levels[-1]['geometricError']}")
        level = {"level": level_index, "geometricError": geometric_error, "position": s_position,
                 "normal": s_normal, "tangent": s_tangent, "uv0": s_uv0, "uv1": s_uv1,
                 "color": s_color, "indices": s_indices}
        if reuse_source_vertices:
            level["sourceVertexIndices"] = simplified[7]
        levels.append(level)
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


def halve_alpha_weighted(value):
    """Reduz RGBA sem deixar o RGB invisivel contaminar a silhueta.

    Exportadores podem conservar qualquer cor em texels totalmente
    transparentes. Uma media comum mistura essa cor com folhas visiveis e cria
    halos claros; nos mips menores, o halo vira um card plano. O RGB e filtrado
    premultiplicado por alpha e o alpha conserva o box filter usado pela
    preservacao de cobertura.
    """
    alpha = value[..., 3:4]
    reduced_alpha = halve(alpha)
    reduced_premultiplied = halve(value[..., :3] * alpha)
    reduced_rgb = np.divide(
        reduced_premultiplied,
        reduced_alpha,
        out=np.zeros_like(reduced_premultiplied),
        where=reduced_alpha > 1e-8,
    )
    return np.concatenate((reduced_rgb, reduced_alpha), axis=-1)


def coverage_mip_is_representable(target, observed,
                                  relative_tolerance=.35,
                                  absolute_tolerance=.02):
    """Retorna se a cobertura discretizada ainda representa o original."""
    tolerance = max(absolute_tolerance, target * relative_tolerance)
    return abs(observed - target) <= tolerance


def write_aetx(path, width, height, encoding, levels):
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", 0x58544541, 1, width, height,
                                 encoding, len(levels), len(payload)) + payload)


def validate_aetx(path, width, height, encoding, mip_count):
    """Valida o envelope antes de reutilizar um payload ja comprimido."""
    if not path.is_file() or path.stat().st_size < 32:
        return False
    header = path.read_bytes()[:32]
    magic, version, stored_width, stored_height, stored_encoding, stored_mips, payload = (
        struct.unpack("<6IQ", header)
    )
    return (magic == 0x58544541 and version == 1 and
            stored_width == width and stored_height == height and
            stored_encoding == encoding and stored_mips == mip_count and
            path.stat().st_size == 32 + payload)


def astc_level(astcenc, cache, name, mip, rgba, srgb, quality, jobs):
    png = cache / f"{name}-{mip}.png"
    encoded = png.with_suffix(".astc")
    try:
        Image.fromarray(rgba, "RGBA").save(png, compress_level=1)
        subprocess.run([str(astcenc), "-cs" if srgb else "-cl", str(png), str(encoded),
                        "6x6", quality, "-j", str(jobs), "-silent"], check=True)
        blob = encoded.read_bytes()
        if len(blob) < 16 or blob[:4] != bytes.fromhex("13aba15c"):
            raise ValueError(f"invalid ASTC payload for {name} mip {mip}")
        return blob[16:]
    finally:
        # Estes arquivos sao workspace do encoder, nao cache: a chave nao
        # inclui fonte/qualidade e a funcao sempre reencoda. Conserva-los
        # enche o armazenamento durante um unico mapa e ainda permite que uma
        # escrita interrompida seja confundida com dado aproveitavel.
        png.unlink(missing_ok=True)
        encoded.unlink(missing_ok=True)


def cook_texture(astcenc, cache, output, name, source, srgb, normal_map, quality, jobs,
                 coverage_cutoff=None, reuse_existing=False):
    pixels = np.asarray(Image.open(io.BytesIO(source)).convert("RGBA"), dtype=np.float32) / 255.0
    width, height = pixels.shape[1], pixels.shape[0]
    if width == 0 or height == 0 or width & (width - 1) or height & (height - 1):
        raise ValueError(f"{name}: power-of-two texture required, got {width}x{height}")
    if srgb:
        pixels[..., :3] = srgb_decode(pixels[..., :3])
    elif normal_map:
        pixels[..., :3] = pixels[..., :3] * 2.0 - 1.0

    levels, fallback = [], []
    level_count = 0
    fallback_level_count = 0
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
            observed_coverage = alpha_coverage(rgba[..., 3] / 255.0, coverage_cutoff)
            # O tail e quantizado em passos grandes. Nao publicamos um nivel
            # que transforma uma copa esparsa em 2x2/1x1 opaco: o sampler
            # prende no ultimo mip real, cuja silhueta continua representavel.
            if mip > 0 and not coverage_mip_is_representable(
                    coverage_target, observed_coverage):
                break
            coverage_by_mip.append(round(observed_coverage, 6))
        current_height, current_width = rgba.shape[:2]
        if not reuse_existing:
            levels.append(astc_level(astcenc, cache, name, mip, rgba, srgb, quality, jobs))
        level_count += 1
        # Development fallback stays bounded: ASTC retains the full source;
        # devices without ASTC receive at most 512 px per axis instead of
        # multiplying APK and runtime memory by the complete RGBA8 corpus.
        if current_width <= 512 and current_height <= 512:
            if fallback_level_count == 0:
                fallback_width, fallback_height = current_width, current_height
            if not reuse_existing:
                fallback.append(rgba.tobytes())
            fallback_level_count += 1
        print(f"{name}: mip {mip} {current_width}x{current_height}", flush=True)
        if current_width == 1 and current_height == 1:
            break
        pixels = halve_alpha_weighted(pixels) if coverage_target is not None else halve(pixels)
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

    main_path = output / f"{name}.aetex"
    fallback_path = output / f"{name}-fallback.aetex"
    if reuse_existing:
        if not validate_aetx(main_path, width, height, 1 if srgb else 2, level_count):
            raise ValueError(f"{name}: AETX principal ausente ou incompativel para reuso")
        if not validate_aetx(fallback_path, fallback_width, fallback_height,
                             3 if srgb else 4, fallback_level_count):
            raise ValueError(f"{name}: fallback AETX ausente ou incompativel para reuso")
    else:
        write_aetx(main_path, width, height, 1 if srgb else 2, levels)
        write_aetx(fallback_path, fallback_width, fallback_height,
                   3 if srgb else 4, fallback)
    return width, height, level_count, coverage_by_mip



def read_glb(data):
    """Split a binary glTF container into its JSON and BIN chunks.

    A .glb carries the same glTF the ZIP export carries, but with the buffer
    and every image inlined instead of sitting next to a scene.gltf. Supporting
    it is not a convenience: the .glb is frequently the only form of an asset
    that is actually redistributable, and refusing it would mean the cooker can
    only ever improve assets whose loose export someone kept.
    """
    if len(data) < 12:
        raise ValueError("GLB shorter than its header")
    magic, version, total = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ValueError("not a GLB container")
    if version != 2:
        raise ValueError(f"unsupported GLB version {version}")
    if total > len(data):
        raise ValueError("GLB length field exceeds the file")
    offset = 12
    json_chunk = None
    binary_chunk = b""
    while offset + 8 <= total:
        length, kind = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8:offset + 8 + length]
        if len(payload) != length:
            raise ValueError("truncated GLB chunk")
        if kind == 0x4E4F534A and json_chunk is None:
            json_chunk = payload
        elif kind == 0x004E4942 and not binary_chunk:
            binary_chunk = payload
        # Chunk payloads are already padded to four bytes by the spec.
        offset += 8 + length
    if json_chunk is None:
        raise ValueError("GLB without a JSON chunk")
    return json.loads(json_chunk.decode("utf-8")), binary_chunk


def buffer_view_bytes(gltf, binary, view_index):
    view = gltf["bufferViews"][view_index]
    if view.get("buffer", 0) != 0:
        raise ValueError("only the first glTF buffer is supported")
    start = view.get("byteOffset", 0)
    end = start + view["byteLength"]
    if end > len(binary):
        raise ValueError("bufferView out of range")
    return binary[start:end]


def glb_as_source_zip(data, license_bytes):
    """Normaliza GLB para o mesmo contrato interno do export ZIP legado.

    O restante do cooker continua consumindo `scene.gltf`, `scene.bin` e URIs
    de imagem. Fazer a conversao somente em memoria evita dois importadores
    divergentes e preserva exatamente a mesma geracao de material, LOD e
    manifesto para as duas formas do glTF.
    """
    gltf, binary = read_glb(data)
    gltf.setdefault("buffers", [{}])
    if len(gltf["buffers"]) != 1:
        raise ValueError("GLB com mais de um buffer ainda nao e suportado")
    gltf["buffers"][0]["uri"] = "scene.bin"
    gltf["buffers"][0]["byteLength"] = len(binary)

    images = []
    extensions = {
        "image/png": ".png",
        "image/jpeg": ".jpg",
        "image/webp": ".webp",
        "image/ktx2": ".ktx2",
    }
    for index, image in enumerate(gltf.get("images", [])):
        if "bufferView" not in image:
            raise ValueError(f"GLB image {index} nao possui bufferView")
        mime = image.get("mimeType", "application/octet-stream")
        name = f"image_{index:03d}{extensions.get(mime, '.bin')}"
        images.append((name, buffer_view_bytes(gltf, binary, image["bufferView"])))
        image.pop("bufferView")
        image.pop("mimeType", None)
        image["uri"] = name

    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("scene.gltf", json.dumps(gltf, separators=(",", ":")))
        archive.writestr("scene.bin", binary)
        archive.writestr("license.txt", license_bytes)
        for name, payload in images:
            archive.writestr(name, payload)
    return output.getvalue()


def alpha_card_trim_box(alpha, cutoff, uv_minimum, uv_maximum):
    """Tightest UV box of the card that still contains every covered texel.

    Returns None when nothing can be trimmed -- either the card is already
    tight, or it is entirely transparent (which is a content bug this function
    refuses to "fix" by deleting geometry). Erring toward the original box is
    the safe direction: a card that stays too large only costs fragments.
    """
    height, width = alpha.shape
    if width == 0 or height == 0:
        return None
    # Half-open texel span of the card, clamped into the image. UV outside
    # [0,1] means the card tiles or mirrors; trimming would then move geometry
    # against a repeat that this function cannot see, so it declines.
    if uv_minimum[0] < -1e-4 or uv_minimum[1] < -1e-4:
        return None
    if uv_maximum[0] > 1.0 + 1e-4 or uv_maximum[1] > 1.0 + 1e-4:
        return None
    x0 = max(0, min(width - 1, int(math.floor(uv_minimum[0] * width))))
    x1 = max(x0 + 1, min(width, int(math.ceil(uv_maximum[0] * width))))
    y0 = max(0, min(height - 1, int(math.floor(uv_minimum[1] * height))))
    y1 = max(y0 + 1, min(height, int(math.ceil(uv_maximum[1] * height))))
    window = alpha[y0:y1, x0:x1]
    covered = window >= cutoff
    if not covered.any():
        return None
    rows = np.flatnonzero(covered.any(axis=1))
    columns = np.flatnonzero(covered.any(axis=0))
    # One texel of guard on each side so bilinear filtering at the new edge
    # still reads the original transparent neighbour instead of clamping a
    # covered texel outward, which would fatten the silhouette.
    left = max(x0 + int(columns[0]) - 1, 0)
    right = min(x0 + int(columns[-1]) + 2, width)
    top = max(y0 + int(rows[0]) - 1, 0)
    bottom = min(y0 + int(rows[-1]) + 2, height)
    return (left / width, top / height, right / width, bottom / height)


def trim_alpha_cards(position, uv0, indices, alpha, cutoff, minimum_gain=0.02):
    """Shrink alpha-tested quads to the silhouette of their own texture region.

    A leaf card is a rectangle whose texture is mostly transparent. The GPU
    rasterizes the whole rectangle and the shader discards most of it, so those
    fragments cost binning, interpolation, a texture fetch and a discard while
    contributing nothing. Pulling the four corners in to the covered box removes
    them before rasterization -- the cheapest fragment is the one never
    generated.

    The map from UV to position is recovered as an AFFINE map per triangle, not
    as a bilinear patch over the UV bounding box. That distinction is the whole
    correctness of this function: foliage atlases rotate their cards, so the
    four corners of a card are generally NOT at the corners of their own UV
    bounding box, and fitting a bilinear patch there is ill-conditioned -- it
    reproduces the input exactly (the system is square) while producing control
    points that explode the moment a corner is moved.

    Three guards keep this conservative, because a wrong trim deletes visible
    geometry while a refused trim only costs the fragments it already costs:

      * only exact quads (two triangles, four unique vertices);
      * the affine map has to reproduce the fourth corner, proving the card is
        planar and consistently parameterized;
      * a vertex is trimmed at most once, so cards welded to a shared vertex are
        left alone instead of being dragged by their neighbour.

    Returns (position, uv0, trimmed_quads, examined_quads); the arrays are
    copies only when something was actually trimmed.
    """
    triangles = np.asarray(indices, dtype=np.uint32).reshape(-1, 3)
    if len(triangles) < 2:
        return position, uv0, 0, 0
    examined = 0
    trimmed = 0
    out_position = None
    out_uv = None
    moved = np.zeros(len(position), dtype=bool)
    for pair in range(len(triangles) // 2):
        first = triangles[pair * 2]
        corners = np.unique(triangles[pair * 2:pair * 2 + 2].reshape(-1))
        if len(corners) != 4:
            continue
        examined += 1
        if moved[corners].any():
            continue
        card_uv = uv0[corners]
        uv_minimum = card_uv.min(axis=0)
        uv_maximum = card_uv.max(axis=0)
        extent = uv_maximum - uv_minimum
        if extent[0] <= 1e-6 or extent[1] <= 1e-6:
            continue

        # Affine UV -> position from the first triangle. Its UV basis must be
        # non-degenerate, otherwise the card has no usable parameterization.
        origin_uv = uv0[first[0]]
        basis_uv = np.stack([uv0[first[1]] - origin_uv, uv0[first[2]] - origin_uv]).astype(np.float64)
        determinant = basis_uv[0, 0] * basis_uv[1, 1] - basis_uv[0, 1] * basis_uv[1, 0]
        if abs(determinant) < 1e-12 * max(1.0, float(np.abs(basis_uv).max()) ** 2):
            continue
        basis_position = np.stack([position[first[1]] - position[first[0]],
                                   position[first[2]] - position[first[0]]]).astype(np.float64)
        try:
            jacobian = np.linalg.solve(basis_uv, basis_position)  # 2x3: d(position)/d(uv)
        except np.linalg.LinAlgError:
            continue

        def evaluate(uv):
            return position[first[0]].astype(np.float64) + (np.asarray(uv, dtype=np.float64) -
                                                            origin_uv.astype(np.float64)) @ jacobian

        # The map came from one triangle; the other corners prove the card is
        # planar and shares the parameterization.
        scale = max(1.0, float(np.abs(position[corners]).max()))
        if float(np.abs(evaluate(card_uv) - position[corners].astype(np.float64)).max()) > 1e-3 * scale:
            continue

        box = alpha_card_trim_box(alpha, cutoff, uv_minimum, uv_maximum)
        if box is None:
            continue
        new_minimum = np.array([max(box[0], float(uv_minimum[0])),
                                max(box[1], float(uv_minimum[1]))], dtype=np.float32)
        new_maximum = np.array([min(box[2], float(uv_maximum[0])),
                                min(box[3], float(uv_maximum[1]))], dtype=np.float32)
        new_extent = new_maximum - new_minimum
        if new_extent[0] <= 1e-6 or new_extent[1] <= 1e-6:
            continue
        gain = 1.0 - float(new_extent[0] * new_extent[1]) / float(extent[0] * extent[1])
        if gain < minimum_gain:
            continue

        # Each corner slides to the trimmed edge on the axes where it sits on
        # the original edge. A corner that is interior on an axis (a rotated
        # card touching the box only at one point) keeps its coordinate there,
        # so the quad never turns inside out.
        target = card_uv.astype(np.float32).copy()
        for axis in range(2):
            on_minimum = card_uv[:, axis] <= uv_minimum[axis] + extent[axis] * 1e-3
            on_maximum = card_uv[:, axis] >= uv_maximum[axis] - extent[axis] * 1e-3
            target[on_minimum, axis] = new_minimum[axis]
            target[on_maximum, axis] = new_maximum[axis]
        if np.allclose(target, card_uv, atol=1e-7):
            continue

        if out_position is None:
            out_position = position.copy()
            out_uv = uv0.copy()
        out_position[corners] = evaluate(target).astype(np.float32)
        out_uv[corners] = target
        moved[corners] = True
        trimmed += 1
    if out_position is None:
        return position, uv0, 0, examined
    return out_position, out_uv, trimmed, examined

def accessor(gltf, binary, index):
    item = gltf["accessors"][index]
    if "sparse" in item:
        raise ValueError("sparse accessors are not supported by AEMAP v1")
    view = gltf["bufferViews"][item["bufferView"]]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    dtype = {5120: np.dtype('i1'), 5121: np.dtype('u1'),
             5122: np.dtype('<i2'), 5123: np.dtype('<u2'),
             5125: np.dtype("<u4"), 5126: np.dtype("<f4")}.get(item["componentType"])
    if dtype is None:
        raise ValueError(f"unsupported accessor component type {item['componentType']}")
    offset = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", dtype.itemsize * components)
    array = np.ndarray((item["count"], components), dtype=dtype, buffer=binary,
                       offset=offset, strides=(stride, dtype.itemsize)).copy()
    if item.get('normalized', False):
        if dtype.kind not in 'iu' or item['componentType'] == 5125:
            raise ValueError('invalid normalized accessor type')
        array = np.maximum(array.astype(np.float32)/np.iinfo(dtype).max, -1.0)
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


def transform_positions(position, matrix):
    mat = np.asarray(matrix, dtype=np.float64).reshape((4, 4), order="F")
    points = np.concatenate((np.asarray(position, dtype=np.float64),
                             np.ones((len(position), 1), dtype=np.float64)), axis=1)
    transformed = (mat @ points.T).T[:, :3]
    if not np.all(np.isfinite(transformed)):
        raise ValueError("non-finite transformed map vertex")
    return transformed


def points_bounds(position, matrix):
    transformed = transform_positions(position, matrix)
    minimum = transformed.min(axis=0)
    maximum = transformed.max(axis=0)
    center = (minimum + maximum) * 0.5
    radius = float(np.linalg.norm(transformed - center, axis=1).max())
    return center.astype(np.float32), radius


def maximum_world_scale(matrix):
    linear = np.asarray(matrix, dtype=np.float64).reshape((4, 4), order="F")[:3, :3]
    if not np.all(np.isfinite(linear)):
        raise ValueError("non-finite map transform")
    return float(np.linalg.svd(linear, compute_uv=False)[0])


def morton_codes(points):
    minimum = points.min(axis=0)
    extent = points.max(axis=0) - minimum
    normalized_points = np.divide(points - minimum, extent, out=np.zeros_like(points),
                                  where=extent > 1.0e-9)
    quantized = np.rint(np.clip(normalized_points, 0.0, 1.0) * 1023.0).astype(np.uint32)
    codes = np.zeros(len(points), dtype=np.uint32)
    for bit in range(10):
        codes |= ((quantized[:, 0] >> bit) & 1) << (bit * 3)
        codes |= ((quantized[:, 1] >> bit) & 1) << (bit * 3 + 1)
        codes |= ((quantized[:, 2] >> bit) & 1) << (bit * 3 + 2)
    return codes


def spatial_primitive_chunks(position, indices, matrix, target_triangles=LOD_GROUP_TRIANGLES,
                             world_cell_size=LOD_GROUP_WORLD_SIZE):
    triangles = np.asarray(indices, dtype=np.uint32).reshape((-1, 3))
    centroids = np.asarray(position, dtype=np.float64)[triangles].mean(axis=1)
    world_centroids = transform_positions(centroids, matrix)
    cell_coordinates = np.floor(world_centroids / world_cell_size).astype(np.int64)
    cells = {}
    for triangle_index, coordinates in enumerate(cell_coordinates):
        cells.setdefault(tuple(coordinates), []).append(triangle_index)
    chunks = []
    for key in sorted(cells):
        cell_indices = np.asarray(cells[key], dtype=np.int64)
        if len(cell_indices) > target_triangles:
            local_order = np.argsort(morton_codes(world_centroids[cell_indices]), kind="stable")
            cell_indices = cell_indices[local_order]
        for first in range(0, len(cell_indices), target_triangles):
            chunks.append(triangles[cell_indices[first:first + target_triangles]].reshape(-1))
    return chunks


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
    # Aether-specific authoring metadata lives in glTF extras, never in a
    # material name heuristic. This keeps imports deterministic and lets DCC
    # exporters preserve a backend-neutral water classification.
    if material.get("extras", {}).get("aetherWater", False): flags |= MATERIAL_WATER
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
    parser.add_argument("--source-author", default="99.Miles")
    parser.add_argument("--source-license", default="CC-BY-4.0")
    parser.add_argument("--source-url", default="https://sketchfab.com/3d-models/update-dirt-road-through-forest-c4676cdf7715484382400ff63faffd45")
    parser.add_argument("--reuse-textures", action="store_true",
                        help="reutiliza AETX validados e recozinha apenas metadata/geometria")
    parser.add_argument("--lod-triangles-per-group", type=int, default=LOD_GROUP_TRIANGLES,
                        help="spatial LOD cell triangle budget (256..32768)")
    parser.add_argument("--lod-world-cell-size", type=float, default=LOD_GROUP_WORLD_SIZE,
                        help="spatial LOD cell size in world units (8..512)")
    args = parser.parse_args()
    encoder_version = subprocess.run([str(args.astcenc), '-version'], check=True,
                                     capture_output=True, text=True).stdout.splitlines()[0]
    if not 256 <= args.lod_triangles_per_group <= 32768:
        parser.error("--lod-triangles-per-group must be in [256, 32768]")
    if not 8.0 <= args.lod_world_cell_size <= 512.0:
        parser.error("--lod-world-cell-size must be in [8, 512]")
    manifest_path = args.out.parent / "manifest.json"
    previous_manifest = (json.loads(manifest_path.read_text(encoding="utf8"))
                         if manifest_path.is_file() else {})
    args.out.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)

    source_bytes = args.source.read_bytes()
    archive_bytes = source_bytes
    if source_bytes[:4] == b"glTF":
        license_path = args.out.parent / "LICENSE.txt"
        if not license_path.is_file():
            raise ValueError("importar GLB exige LICENSE.txt ao lado do pacote de destino")
        archive_bytes = glb_as_source_zip(source_bytes, license_path.read_bytes())
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
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
                normal_map, args.quality, args.jobs, coverage_cutoff, args.reuse_textures)
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
        source_draw_count = 0
        lod_level_counts = {}  # For the manifest: level index -> draws generated at that level.
        lod_triangle_counts = {}
        lod_group_count = 0
        next_lod_group_id = 0
        world_min = np.array([np.inf, np.inf, np.inf])
        world_max = -world_min
        for node_index, world in scene_nodes(gltf):
            node = gltf["nodes"][node_index]
            if "mesh" not in node:
                continue
            for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
                source_draw_count += 1
                attrs = primitive["attributes"]
                position = accessor(gltf, binary, attrs["POSITION"]).astype(np.float32)
                normal = accessor(gltf, binary, attrs["NORMAL"]).astype(np.float32)
                tangent = accessor(gltf, binary, attrs["TANGENT"]).astype(np.float32) if "TANGENT" in attrs else None
                uv0 = accessor(gltf, binary, attrs["TEXCOORD_0"]).astype(np.float32)
                uv1 = accessor(gltf, binary, attrs.get("TEXCOORD_1", attrs["TEXCOORD_0"])).astype(np.float32)
                color = (accessor(gltf, binary, attrs["COLOR_0"]).astype(np.float32)
                         if "COLOR_0" in attrs else np.ones((len(position),4), dtype=np.float32))
                local_indices = accessor(gltf, binary, primitive["indices"]).astype(np.uint32)
                if tangent is None:
                    tangent = np.zeros((len(position), 4), dtype=np.float32)
                    tangent[:, 0] = 1.0
                    tangent[:, 3] = 1.0
                accessor_bounds = gltf["accessors"][attrs["POSITION"]]
                minimum, maximum = transform_bounds(accessor_bounds["min"], accessor_bounds["max"], world)
                world_min = np.minimum(world_min, minimum)
                world_max = np.maximum(world_max, maximum)
                # MapMaterialRecord.flags sits at byte offset 68 (4 texture
                # indices + baseColorFactor[4] + emissiveFactorAndStrength[4]
                # + roughness/metallic/normalScale/specular + alphaCutoff =
                # 16+16+16+16+4 bytes), matching material_record's packing
                # order and native/renderer/map_package.h's struct layout.
                material_flags = struct.unpack_from("<I", materials[primitive.get("material", 0)],
                                                    offset=68)[0]
                base_vertex = len(vertices)
                for values in zip(position, normal, tangent, uv0, uv1, color):
                    vertices.append(vertex_record(*values))
                # True blend remains one ordered group; opaque and alpha-test
                # geometry are spatial cells so distance is local to the
                # camera and far parts of a large primitive can simplify.
                chunks = ([local_indices] if material_flags & MATERIAL_BLEND else
                          spatial_primitive_chunks(position, local_indices, world,
                                                   args.lod_triangles_per_group,
                                                   args.lod_world_cell_size))
                world_scale = maximum_world_scale(world)
                for chunk_indices in chunks:
                    source_vertices, compact_indices = np.unique(
                        np.asarray(chunk_indices, dtype=np.uint32), return_inverse=True)
                    compact_indices = compact_indices.astype(np.uint32)
                    chunk_position = position[source_vertices]
                    chunk_normal = normal[source_vertices]
                    chunk_tangent = tangent[source_vertices]
                    chunk_uv0 = uv0[source_vertices]
                    chunk_uv1 = uv1[source_vertices]
                    chunk_color = color[source_vertices]
                    center, radius = points_bounds(chunk_position, world)
                    local_extent = (chunk_position.astype(np.float64).max(axis=0) -
                                    chunk_position.astype(np.float64).min(axis=0))
                    local_radius = float(np.linalg.norm(local_extent) * .5)
                    lod_levels = simplify_primitive_levels(
                        chunk_position, chunk_normal, chunk_tangent, chunk_uv0, chunk_uv1,
                        chunk_color, compact_indices, material_flags, local_radius,
                        reuse_source_vertices=True)
                    lod_group_id = next_lod_group_id
                    next_lod_group_id += 1
                    if len(lod_levels) > 1:
                        lod_group_count += 1
                    for entry in lod_levels:
                        if entry["level"] == 0:
                            emitted_indices = np.asarray(chunk_indices, dtype=np.uint32) + base_vertex
                        else:
                            representatives = source_vertices[entry["sourceVertexIndices"]]
                            emitted_indices = representatives[entry["indices"]] + base_vertex
                        first_index = len(indices)
                        indices.extend(int(value) for value in emitted_indices)
                        geometric_error = float(entry["geometricError"] * world_scale)
                        draws.append(struct.pack(
                            "<4I16f4fIfI", first_index, len(emitted_indices), 0,
                            primitive.get("material", 0), *world, *center, radius,
                            entry["level"], geometric_error, lod_group_id))
                        lod_level_counts[entry["level"]] = lod_level_counts.get(entry["level"], 0) + 1
                        lod_triangle_counts[entry["level"]] = (
                            lod_triangle_counts.get(entry["level"], 0) + len(emitted_indices) // 3)

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
        # License text is an authored/legal asset, not normalized cooker output.
        # Writing through TextIO on Windows translates every existing LF and
        # compounds CRLF into CRCRLF on each recook. Preserve its exact bytes so
        # repeated GLB/ZIP imports are idempotent and attribution stays readable.
        (args.out.parent / "LICENSE.txt").write_bytes(archive.read("license.txt"))

    outputs = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(args.out.iterdir()) if path.is_file()}
    manifest = {"version": 3, "format": "AEMAP-3", "sourceZip": args.source.name,
                "sourceSha256": hashlib.sha256(source_bytes).hexdigest(),
                "license": args.source_license, "author": args.source_author,
                "source": args.source_url,
                "encoder": f"{encoder_version} {args.quality} 6x6",
                "statistics": {"textures": len(texture_records), "materials": len(materials),
                               "draws": len(draws), "vertices": len(vertices),
                               "triangles": len(indices) // 3,
                               "boundsMin": world_min.tolist(), "boundsMax": world_max.tolist(),
                               "defaultCamera": camera.tolist(),
                               "sourceDraws": source_draw_count,
                               "level0Draws": lod_level_counts.get(0, 0),
                               "level0Vertices": len(vertices),
                               "level0Triangles": lod_triangle_counts.get(0, 0),
                               "lodGroups": lod_group_count,
                               "lodTrianglesPerGroup": args.lod_triangles_per_group,
                               "lodWorldCellSize": args.lod_world_cell_size,
                               # Draws generated per LOD level index (0 = original/untouched);
                               # level 0's count also includes every primitive LOD was
                               # skipped for (blend materials and primitives below
                               # LOD_MINIMUM_TRIANGLES). See simplify_primitive_levels.
                               "lodDrawsByLevel": {str(level): count
                                                   for level, count in sorted(lod_level_counts.items())},
                               "lodTrianglesByLevel": {str(level): count
                                                       for level, count in sorted(lod_triangle_counts.items())}},
                "textures": texture_sources, "outputs": outputs,
                "geometryLod": {
                    "version": 1,
                    "sourceFormat": "glTF-2.0",
                    "preservesLevel0": True,
                    "level0Contract": "exact-vertices-and-triangles-spatially-reordered",
                    "strategy": "source-vertex-grid-clustering-v1",
                }}
    # O ambiente e um asset independente do modelo. Recozinhar geometria e
    # texturas nao pode apagar a autoria do ceu/IBL nem seu contrato binario.
    if "environment" in previous_manifest:
        manifest["environment"] = previous_manifest["environment"]
    (args.out.parent / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")


if __name__ == "__main__":
    main()
