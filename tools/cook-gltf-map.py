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
MAP_VERSION = 2
HEADER_SIZE = 144
VERTEX_STRIDE = 48
INVALID_TEXTURE = 0xFFFFFFFF
MATERIAL_BLEND = 1 << 0
MATERIAL_NORMAL_MAP = 1 << 1
MATERIAL_METALLIC_ROUGHNESS_MAP = 1 << 2
MATERIAL_EMISSIVE_MAP = 1 << 3
MATERIAL_ALPHA_MASK = 1 << 4
MATERIAL_DOUBLE_SIDED = 1 << 5


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


def cook_texture(astcenc, cache, output, name, source, srgb, normal_map, quality, jobs):
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
    mip = 0
    while True:
        display = pixels.copy()
        if srgb:
            display[..., :3] = srgb_encode(display[..., :3])
        elif normal_map:
            display[..., :3] = normalized(display[..., :3]) * .5 + .5
        rgba = np.rint(np.clip(display, 0, 1) * 255).astype(np.uint8)
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

    write_aetx(output / f"{name}.aetex", width, height, 1 if srgb else 2, levels)
    write_aetx(output / f"{name}-fallback.aetex", fallback_width, fallback_height,
               3 if srgb else 4, fallback)
    return width, height, len(levels)


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
        inferred_cutouts = set()
        for (texture_index, srgb), cooked_index in sorted(texture_map.items(), key=lambda item: item[1]):
            texture = gltf["textures"][texture_index]
            image = gltf["images"][texture["source"]]
            source = archive.read(image["uri"])
            if srgb and alpha_coverage_is_cutout(source):
                inferred_cutouts.add(texture_index)
            normal_map = any(material.get("normalTexture", {}).get("index") == texture_index
                             for material in gltf["materials"]) and not srgb
            dimensions = cook_texture(args.astcenc, args.cache, args.out,
                                      f"texture_{cooked_index:03d}", source, srgb,
                                      normal_map, args.quality, args.jobs)
            sampler = gltf.get("samplers", [{}])[texture.get("sampler", 0)]
            flags = (1 if sampler.get("magFilter", 9729) == 9729 else 0)
            flags |= (2 if sampler.get("minFilter", 9987) in (9987, 9985) else 0)
            flags |= (4 if sampler.get("wrapS", 10497) == 33071 else 0)
            flags |= (8 if sampler.get("wrapT", 10497) == 33071 else 0)
            flags |= (16 if srgb else 0)
            texture_records.append(struct.pack("<4I", flags, 0, 0, 0))
            texture_sources.append({"cookedIndex": cooked_index, "gltfTexture": texture_index,
                                    "image": image["uri"], "srgb": srgb,
                                    "normalMap": normal_map, "dimensions": dimensions,
                                    "sha256": hashlib.sha256(source).hexdigest()})

        materials = [material_record(material, texture_map, inferred_cutouts)
                     for material in gltf["materials"]]
        vertices, indices, draws = [], [], []
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
                base_vertex = len(vertices)
                for values in zip(position, normal, tangent, uv0, uv1, color):
                    vertices.append(vertex_record(*values))
                first_index = len(indices)
                indices.extend(int(value) + base_vertex for value in local_indices)
                accessor_bounds = gltf["accessors"][attrs["POSITION"]]
                minimum, maximum = transform_bounds(accessor_bounds["min"], accessor_bounds["max"], world)
                world_min = np.minimum(world_min, minimum)
                world_max = np.maximum(world_max, maximum)
                center = (minimum + maximum) * .5
                radius = float(np.linalg.norm(maximum - minimum) * .5)
                draws.append(struct.pack("<4I16f4f", first_index, len(local_indices), 0,
                                         primitive.get("material", 0), *world, *center, radius))

        center = (world_min + world_max) * .5
        extent = world_max - world_min
        camera = np.array([center[0], center[1] + max(6.0, extent[1] * .08),
                           world_min[2] - max(18.0, extent[2] * .22)], dtype=np.float32)
        texture_offset = HEADER_SIZE
        material_offset = align(texture_offset + len(texture_records) * 16)
        draw_offset = align(material_offset + len(materials) * 80)
        vertex_offset = align(draw_offset + len(draws) * 96)
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
        package[draw_offset:draw_offset + len(draws) * 96] = b"".join(draws)
        package[vertex_offset:vertex_offset + len(vertices) * VERTEX_STRIDE] = b"".join(vertices)
        package[index_offset:] = struct.pack(f"<{len(indices)}I", *indices)
        (args.out / "scene.aemap").write_bytes(package)
        license_text = archive.read("license.txt").decode("utf-8-sig")
        (args.out.parent / "LICENSE.txt").write_text(license_text, encoding="utf8")

    outputs = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(args.out.iterdir()) if path.is_file()}
    manifest = {"version": 2, "format": "AEMAP-2", "sourceZip": args.source.name,
                "sourceSha256": hashlib.sha256(source_bytes).hexdigest(),
                "license": "CC-BY-4.0", "author": "99.Miles",
                "source": "https://sketchfab.com/3d-models/update-dirt-road-through-forest-c4676cdf7715484382400ff63faffd45",
                "encoder": f"astcenc 5.7.0 {args.quality} 6x6",
                "statistics": {"textures": len(texture_records), "materials": len(materials),
                               "draws": len(draws), "vertices": len(vertices),
                               "triangles": len(indices) // 3,
                               "boundsMin": world_min.tolist(), "boundsMax": world_max.tolist(),
                               "defaultCamera": camera.tolist()},
                "textures": texture_sources, "outputs": outputs}
    (args.out.parent / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")


if __name__ == "__main__":
    main()
