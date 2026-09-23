"""Generate three editable first-person Astra games and their shared authored asset kit."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import random
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "android/app/src/main/assets/astra/example-projects"
TEXTURES = ROOT / "tools/assets/highlevel-textures"
SOURCE = "Assets/world.glb"
MESHES = ("Ground", "Concrete", "Steel", "Timber", "Hazard", "Pipe", "Rock", "Valve", "Lamp")
REAL_MODELS = {
    "quarentena": ("Barrel_01", "portable_generator", "korean_fire_extinguisher_01"),
    "resgate": ("medical_box", "rock_07", "metal_toolbox"),
    "perimetro": ("old_military_crate", "old_gas_mask", "vintage_radio_transceiver"),
}

_base_spec = importlib.util.spec_from_file_location("astra_base_examples", ROOT / "tools/generate-example-games.py")
assert _base_spec and _base_spec.loader
base = importlib.util.module_from_spec(_base_spec)
_base_spec.loader.exec_module(base)


class Scene(base.Scene):
    def __init__(self):
        super().__init__(SOURCE)
        source_guid = base.guid("fonte:" + SOURCE)
        self.meshes = {name: base.guid("glb:" + source_guid + ":" + name + "/" + name + "#0")
                       for name in MESHES}


def shape(scene: Scene, name: str, mesh: str, parent: int, pos, scale, color=(1, 1, 1),
          rough=.65, metallic=0, emission=(0, 0, 0), strength=1, rot=(0, 0, 0),
          solid=False, dynamic=False, kinematic=False, extra=(), uv_world=True):
    component = base.mesh_component(scene.meshes[mesh], color, rough, metallic, emission, strength)
    if uv_world and mesh not in ("Lamp", "Valve") and not dynamic and not kinematic:
        # MeshRenderer v7: every texture binding projects in metres rather than
        # stretching a 0..1 cube UV across a scaled wall, pipe or rock.
        component = (component[0], 7, component[2] + " - 0 - - - - 0 0 0.5" +
                     " 300" * 4 + " 0 0 1 1 0" * 4)
    components = [component]
    if solid or dynamic or kinematic:
        components.extend((base.body_component(2 if dynamic else 1 if kinematic else 0,
                                               max(.5, scale[0] * scale[1] * scale[2])),
                           base.collider_component((1, 1, 1))))
    components.extend(extra)
    return scene.add(name, parent, 1, pos, rot, scale, components, static=solid)


def light(scene, name, parent, pos, color, intensity, radius=12, kind=1, rot=(0, 0, 0)):
    return scene.add(name, parent, 2, pos, rot, components=(base.light_component(kind, color, intensity, radius),))


def player(scene, name, parent, pos, script, speed=5.5, jump=5.5, fov=72):
    capsule = ("astra.physics.character", 2, " ".join(map(base.f, (.42, .55, 1.62, speed, 47, jump))))
    actor = scene.add(name, parent, 0, pos, components=(capsule, base.script_component(script)))
    camera = ("astra.camera", 2, " ".join(map(base.f, (1, fov, .08, 280, 100, 5, 0))))
    look = ("astra.camera.look", 1, " ".join(map(base.f, (245, 155, 80))))
    scene.add("Câmera dos olhos", actor, 3, (0, 1.62, 0), components=(camera, look))
    return actor


def input_archive(action: str) -> str:
    # Keep the configurable Move/Look/Jump roles. Button 1 is a distinct project action.
    return base.INPUT.replace('INPUT 3 ', 'INPUT 4 ', 1) + f' {base.quoted(action)} 0 0 1 "" 1 3 1 0 0 1 0'


def glb_document(path: Path) -> dict:
    data = path.read_bytes()
    if data[:4] != b"glTF" or struct.unpack_from("<I", data, 4)[0] != 2:
        raise ValueError(f"GLB inválido: {path}")
    length, chunk = struct.unpack_from("<I4s", data, 12)
    if chunk != b"JSON":
        raise ValueError(f"Bloco JSON ausente: {path}")
    return json.loads(data[20:20 + length])


def source_mesh_component(asset: str):
    # Keep the downloaded glTF's own PBR textures and scalar material values.
    return ("astra.render.mesh", 2, "1 1 0 1 1 1 0.5 0 1 1 0 0 0 1 " + asset)


def quaternion_euler(value):
    x, y, z, w = value
    xx = 1 - 2 * (y * y + z * z)
    xy = 2 * (x * y + z * w)
    xz = 2 * (x * z - y * w)
    yz = 2 * (y * z + x * w)
    zz = 1 - 2 * (x * x + y * y)
    yy = math.asin(max(-1, min(1, -xz)))
    xx_angle = math.atan2(yz, zz)
    zz_angle = math.atan2(xy, xx)
    return tuple(math.degrees(v) for v in (xx_angle, yy, zz_angle))


def real_prop(scene: Scene, game: str, model: str, label: str, parent: int, pos, scale=(1, 1, 1),
              components=()):
    path = OUT / game / "Assets" / f"{model}.glb"
    document = glb_document(path)
    asset_path = f"Assets/{model}.glb"
    source_guid = base.guid("fonte:" + asset_path)
    group = scene.add(label, parent, pos=pos, scale=scale, components=components)
    nodes = document["nodes"]
    parent_of = {}
    for index, node in enumerate(nodes):
        for child in node.get("children", []):
            parent_of[child] = index
    created = {}
    def emit(index):
        if index in created:
            return created[index]
        node = nodes[index]
        up = emit(parent_of[index]) if index in parent_of else group
        rotation = quaternion_euler(node.get("rotation", (0, 0, 0, 1)))
        number = scene.add(node.get("name", f"Nó {index}"), up, pos=node.get("translation", (0, 0, 0)),
                           rot=rotation, scale=node.get("scale", (1, 1, 1)))
        created[index] = number
        if "mesh" in node:
            mesh = document["meshes"][node["mesh"]]
            mesh_name = mesh.get("name", f"m{node['mesh']}")
            node_name = node.get("name", f"n{index}")
            for primitive in range(len(mesh["primitives"])):
                key = f"{node_name}/{mesh_name}#{primitive}"
                identity = base.guid("glb:" + source_guid + ":" + key)
                scene.add(f"{node_name} · material {primitive + 1}", number, 1,
                          components=(source_mesh_component(identity),))
        return number
    for index in range(len(nodes)):
        emit(index)
    return group


def collision_proxy(scene: Scene, parent: int, label: str, pos, half, body=True):
    components = ((base.body_component(0), base.collider_component((1, 1, 1))) if body
                  else (("astra.physics.collider", 3,
                         " ".join(map(base.f, (0, 1, 1, 1, .5, .5, 0, 0, 0, 0, 0, 0,
                                                     parent, 1)))),))
    return scene.add(label, parent, pos=pos, scale=half, static=body, components=components)


def npc(scene: Scene, name: str, parent: int, pos, script: str, suit, accent, drone=False):
    half = (.42, .42, .42) if drone else (.38, .82, .3)
    root = scene.add(name, parent, pos=pos, components=(
        base.body_component(1, 1), base.collider_component(half), base.script_component(script)))
    if drone:
        shape(scene, "Carcaça", "Rock", root, (0, 0, 0), (.48, .35, .48), suit, .36, .62, uv_world=False)
        shape(scene, "Anel de navegação", "Valve", root, (0, 0, 0), (.7, .7, .7), accent,
              .28, .72, accent, 1.4, rot=(90, 0, 0), uv_world=False)
        shape(scene, "Olho", "Lamp", root, (0, .03, .46), (.17, .12, .08), accent,
              .18, .05, accent, 5, uv_world=False)
        for side in (-1, 1):
            shape(scene, f"Estabilizador {side}", "Pipe", root, (side * .62, 0, 0),
                  (.08, .25, .08), suit, .34, .68, rot=(0, 0, 90), uv_world=False)
    else:
        shape(scene, "Tronco", "Concrete", root, (0, .18, 0), (.34, .55, .24), suit,
              .7, .05, uv_world=False)
        shape(scene, "Cabeça", "Lamp", root, (0, .92, 0), (.25, .28, .25), accent,
              .42, .05, accent, .45, uv_world=False)
        for side in (-1, 1):
            shape(scene, f"Braço {side}", "Pipe", root, (side * .43, .19, 0),
                  (.075, .46, .075), suit, .66, .08, rot=(0, 0, side * 8), uv_world=False)
            shape(scene, f"Perna {side}", "Pipe", root, (side * .17, -.65, 0),
                  (.09, .43, .09), suit, .72, .06, uv_world=False)
    return root


def png_rgb(width: int, height: int, pixel) -> bytes:
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(max(0, min(255, int(c))) for c in pixel(x, y))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def texture(name: str) -> bytes:
    def hash_noise(x, y):
        value = (x * 73856093) ^ (y * 19349663) ^ (len(name) * 83492791)
        value = (value ^ (value >> 13)) * 1274126177
        return ((value ^ (value >> 16)) & 255) / 255
    def pixel(x, y):
        n = hash_noise(x, y) - .5
        if name == "concrete":
            joint = 1 if (x % 64 < 2 or y % 64 < 2) else 0
            v = 156 + n * 28 - joint * 28
            return v, v - 2, v - 1
        if name == "steel":
            scratch = 1 if (y * 19 + x * 3) % 73 < 2 else 0
            rivet = 1 if (x % 32 - 16) ** 2 + (y % 32 - 16) ** 2 < 3 else 0
            v = 127 + n * 18 + scratch * 16 - rivet * 35
            return v - 8, v + 2, v + 9
        if name == "wood":
            grain = math.sin(y * .34 + math.sin(x * .09) * 2) * 20
            return 140 + grain + n * 11, 95 + grain * .7 + n * 8, 58 + grain * .4 + n * 6
        if name == "hazard":
            bright = ((x + y) // 24) % 2 == 0
            return ((228, 158, 37) if bright else (33, 36, 39))
        v = 116 + n * 45 + math.sin(x * .14) * 7
        return v + 8, v + 4, v
    return png_rgb(128, 128, pixel)


def cube(uv_repeat=1):
    faces = [((1, 0, 0), [(1, -1, -1), (1, -1, 1), (1, 1, 1), (1, 1, -1)]),
             ((-1, 0, 0), [(-1, -1, 1), (-1, -1, -1), (-1, 1, -1), (-1, 1, 1)]),
             ((0, 1, 0), [(-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)]),
             ((0, -1, 0), [(-1, -1, 1), (1, -1, 1), (1, -1, -1), (-1, -1, -1)]),
             ((0, 0, 1), [(1, -1, 1), (-1, -1, 1), (-1, 1, 1), (1, 1, 1)]),
             ((0, 0, -1), [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1)])]
    positions, normals, uvs, indices = [], [], [], []
    for normal, vertices in faces:
        first = len(positions)
        positions += vertices
        normals += [normal] * 4
        uvs += [(0, 0), (uv_repeat, 0), (uv_repeat, uv_repeat), (0, uv_repeat)]
        indices += [first, first + 1, first + 2, first, first + 2, first + 3]
    return positions, normals, uvs, indices


def sphere(radius_variance=0, segments=20, rings=12):
    positions, normals, uvs, indices = [], [], [], []
    for ring in range(rings + 1):
        theta = math.pi * ring / rings
        for segment in range(segments + 1):
            phi = 2 * math.pi * segment / segments
            unit = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            deformation = 1 + radius_variance * (math.sin(phi * 5 + theta * 3) + math.cos(phi * 7 - theta * 4)) * .5
            positions.append(tuple(v * deformation for v in unit))
            normals.append(unit)
            uvs.append((segment / segments, ring / rings))
    for ring in range(rings):
        for segment in range(segments):
            a = ring * (segments + 1) + segment
            b = a + segments + 1
            indices += [a, b, a + 1, a + 1, b, b + 1]
    return positions, normals, uvs, indices


def cylinder(segments=20):
    positions, normals, uvs, indices = [], [], [], []
    for side in range(segments + 1):
        angle = 2 * math.pi * side / segments
        nx, nz = math.cos(angle), math.sin(angle)
        for y in (-1, 1):
            positions.append((nx, y, nz)); normals.append((nx, 0, nz)); uvs.append((side / segments, (y + 1) / 2))
    for side in range(segments):
        a = side * 2
        indices += [a, a + 1, a + 2, a + 1, a + 3, a + 2]
    for y, normal in ((-1, (0, -1, 0)), (1, (0, 1, 0))):
        center = len(positions)
        positions.append((0, y, 0)); normals.append(normal); uvs.append((.5, .5))
        for side in range(segments + 1):
            angle = 2 * math.pi * side / segments
            x, z = math.cos(angle), math.sin(angle)
            positions.append((x, y, z)); normals.append(normal); uvs.append(((x + 1) / 2, (z + 1) / 2))
        for side in range(segments):
            if y > 0: indices += [center, center + side + 1, center + side + 2]
            else: indices += [center, center + side + 2, center + side + 1]
    return positions, normals, uvs, indices


def torus(major=.72, minor=.18, segments=20, sides=10):
    positions, normals, uvs, indices = [], [], [], []
    for segment in range(segments + 1):
        a = 2 * math.pi * segment / segments
        for side in range(sides + 1):
            b = 2 * math.pi * side / sides
            radial = major + minor * math.cos(b)
            positions.append((math.cos(a) * radial, math.sin(a) * radial, minor * math.sin(b)))
            normals.append((math.cos(a) * math.cos(b), math.sin(a) * math.cos(b), math.sin(b)))
            uvs.append((segment / segments, side / sides))
    for segment in range(segments):
        for side in range(sides):
            a = segment * (sides + 1) + side
            b = a + sides + 1
            indices += [a, b, a + 1, a + 1, b, b + 1]
    return positions, normals, uvs, indices


def world_glb(texture_surfaces: dict[str, str] | None = None,
              texture_root: Path = TEXTURES) -> bytes:
    texture_surfaces = texture_surfaces or {
        "concrete": "concrete", "steel": "steel", "wood": "wood", "rock": "rock"
    }
    definitions = [("Ground", cube(8), 0), ("Concrete", cube(1), 0),
                   ("Steel", cube(1), 1), ("Timber", cube(1), 2),
                   ("Hazard", cube(1), 3), ("Pipe", cylinder(), 1),
                   ("Rock", sphere(.12), 4), ("Valve", torus(), 1), ("Lamp", sphere(0, 14, 10), 5)]
    data = bytearray(); views, accessors, meshes, nodes = [], [], [], []
    def append(payload, target=None):
        data.extend(b"\0" * (-len(data) % 4))
        view = {"buffer": 0, "byteOffset": len(data), "byteLength": len(payload)}
        if target: view["target"] = target
        data.extend(payload); views.append(view)
        return len(views) - 1
    for name, geometry, material in definitions:
        positions, normals, uvs, indices = geometry
        pos_view = append(b"".join(struct.pack("<3f", *v) for v in positions), 34962)
        lo = [min(v[i] for v in positions) for i in range(3)]
        hi = [max(v[i] for v in positions) for i in range(3)]
        accessors.append({"bufferView": pos_view, "componentType": 5126, "count": len(positions),
                          "type": "VEC3", "min": lo, "max": hi})
        pos_accessor = len(accessors) - 1
        normal_view = append(b"".join(struct.pack("<3f", *v) for v in normals), 34962)
        accessors.append({"bufferView": normal_view, "componentType": 5126, "count": len(normals), "type": "VEC3"})
        normal_accessor = len(accessors) - 1
        uv_view = append(b"".join(struct.pack("<2f", *v) for v in uvs), 34962)
        accessors.append({"bufferView": uv_view, "componentType": 5126, "count": len(uvs), "type": "VEC2"})
        uv_accessor = len(accessors) - 1
        index_view = append(b"".join(struct.pack("<H", value) for value in indices), 34963)
        accessors.append({"bufferView": index_view, "componentType": 5123, "count": len(indices), "type": "SCALAR"})
        meshes.append({"name": name, "primitives": [{"attributes": {"POSITION": pos_accessor,
                        "NORMAL": normal_accessor, "TEXCOORD_0": uv_accessor},
                        "indices": len(accessors) - 1, "material": material}]})
        nodes.append({"name": name, "mesh": len(meshes) - 1})
    images, textures = [], []

    def image(payload: bytes, mime: str) -> int:
        view = append(payload)
        images.append({"bufferView": view, "mimeType": mime})
        textures.append({"source": len(images) - 1, "sampler": 0})
        return len(textures) - 1

    def surface(name: str, label: str) -> dict:
        authored_name = texture_surfaces[name]
        base_map = image((texture_root / f"{authored_name}-base.jpg").read_bytes(), "image/jpeg")
        normal_map = image((texture_root / f"{authored_name}-normal.png").read_bytes(), "image/png")
        arm_map = image((texture_root / f"{authored_name}-arm.png").read_bytes(), "image/png")
        return {"name": label, "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": base_map},
                    "metallicRoughnessTexture": {"index": arm_map},
                    "roughnessFactor": 1, "metallicFactor": 1},
                "normalTexture": {"index": normal_map, "scale": .82},
                "occlusionTexture": {"index": arm_map, "strength": .78}}

    materials = [surface("concrete", "Concrete"), surface("steel", "Steel"),
                 surface("wood", "Timber")]
    hazard_map = image(texture("hazard"), "image/png")
    materials.append({"name": "Hazard", "pbrMetallicRoughness": {
        "baseColorTexture": {"index": hazard_map}, "roughnessFactor": .65, "metallicFactor": .15}})
    materials.append(surface("rock", "Rock"))
    materials.append({"name": "Lamp", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1],
                                                                "roughnessFactor": .3, "metallicFactor": .05}})
    document = {"asset": {"version": "2.0", "generator": "Astra high level games"},
                "buffers": [{"byteLength": len(data)}], "bufferViews": views, "accessors": accessors,
                "images": images,
                "samplers": [{"wrapS": 10497, "wrapT": 10497, "magFilter": 9729, "minFilter": 9987}],
                "textures": textures,
                "materials": materials, "meshes": meshes, "nodes": nodes,
                "scenes": [{"nodes": list(range(len(nodes)))}], "scene": 0}
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    data.extend(b"\0" * (-len(data) % 4))
    total = 12 + 8 + len(encoded) + 8 + len(data)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(encoded), b"JSON") + encoded
            + struct.pack("<I4s", len(data), b"BIN\0") + data)


def quarantine() -> Scene:
    scene = Scene()
    world = scene.add("Mundo")
    architecture = scene.add("Arquitetura", world)
    props = scene.add("Equipamentos", world)
    pickups = scene.add("Fusíveis", world)
    emergency = scene.add("Luzes de emergência", world)
    work = scene.add("Iluminação restaurada", world)
    shape(scene, "Piso de concreto", "Ground", architecture, (0, -.22, 0), (8, .22, 30), solid=True)
    shape(scene, "Teto de serviço", "Concrete", architecture, (0, 4.45, 0), (8, .2, 30))
    for side in (-1, 1):
        shape(scene, f"Parede lateral {side}", "Concrete", architecture, (side * 8.25, 2.1, 0),
              (.25, 2.2, 30), solid=True)
        for z in range(-24, 29, 8):
            shape(scene, f"Coluna estrutural {side} {z}", "Steel", architecture,
                  (side * 7.65, 2.1, z), (.35, 2.2, .4), (.78, .84, .87), .42, .65)
            shape(scene, f"Duto longitudinal {side} {z}", "Pipe", props,
                  (side * 6.9, 3.45, z + 3.8), (.11, 3.9, .11), (.72, .78, .83), .34, .72,
                  rot=(90, 0, 0))
            shape(scene, f"Faixa de segurança {side} {z}", "Hazard", architecture,
                  (side * 7.88, .16, z), (.08, .12, 3.7), (1, 1, 1), .62, .1)
    shape(scene, "Parede do acesso", "Concrete", architecture, (0, 2.1, -29.7), (8, 2.2, .25), solid=True)
    for z in (-12, 4, 21):
        for side in (-1, 1):
            shape(scene, f"Parede corta-fogo {z} {side}", "Concrete", architecture,
                  (side * 5.15, 2.1, z), (2.85, 2.2, .32), solid=True)
        shape(scene, f"Travessa corta-fogo {z}", "Steel", architecture,
              (0, 4.0, z), (2.5, .3, .38), (.68, .75, .79), .4, .65)
        shape(scene, f"Faixa de passagem {z}", "Hazard", architecture,
              (0, .03, z - .6), (2.25, .03, .24), (1, 1, 1), .7, .1)
    shape(scene, "Porta blindada", "Steel", architecture, (0, 2, 21), (2.25, 2, .34),
          (.68, .76, .81), .36, .78, kinematic=True)
    for index, (x, z) in enumerate(((3.8, -17), (-3.9, -1), (3.8, 12)), 1):
        shape(scene, f"Fusível {index}", "Lamp", pickups, (x, 1.5, z), (.27, .38, .27),
              (.16, .73, 1), .2, .1, (.1, .55, 1), 4, dynamic=True)
        shape(scene, f"Suporte do fusível {index}", "Steel", props, (x, .75, z), (.55, .72, .55),
              (.45, .51, .55), .4, .7, solid=True)
        light(scene, f"Luz do fusível {index}", emergency, (x, 2.35, z), (.15, .54, 1), 7, 4)
    shape(scene, "Painel do gerador", "Steel", props, (0, 1.55, 18), (.78, .78, .2),
          (.62, .68, .71), .4, .7, solid=True)
    for index in range(3):
        shape(scene, f"Conector do painel {index + 1}", "Lamp", props,
              ((index - 1) * .43, 1.6, 17.76), (.12, .12, .12), (.19, .7, .9), .22, .05,
              (.08, .5, .8), 3)
    for z in (-23, -8, 7, 17, 25):
        for side in (-1, 1):
            shape(scene, f"Rack industrial {side} {z}", "Steel", props,
                  (side * 6.1, 1.2, z), (1.05, 1.2, 1.5), (.63, .67, .68), .48, .75)
            for shelf in (.55, 1.4, 2.25):
                shape(scene, f"Gaveta técnica {side} {z} {shelf}", "Steel", props,
                      (side * 5.96, shelf, z + .12), (.98, .07, 1.36), (.74, .76, .73), .46, .63)
            shape(scene, f"Monitor {side} {z}", "Lamp", props,
                  (side * 5.05, 2.12, z), (.22, .18, .3), (.16, .71, .88), .2, 0,
                  (.08, .43, .6), 2)
    for index in range(10):
        z = -25 + index * 4.4
        side = -1 if index % 2 else 1
        shape(scene, f"Caixa solta {index + 1}", "Steel", props,
              (side * (3.5 + index % 3), 2.9 + index % 2, z), (.43, .43, .43),
              (.7, .67, .61), .6, .42, dynamic=True)
    for index, (x, z) in enumerate(((-5.3, -19), (5.5, -3), (-5.5, 13)), 1):
        item=real_prop(scene, "quarentena", "Barrel_01", f"Barril industrial real {index}", props, (x, 0, z))
        collision_proxy(scene, item, "Colisão do barril", (0,.44,0), (.3,.44,.3))
    generator=real_prop(scene, "quarentena", "portable_generator", "Gerador portátil real", props,
                        (2.1, .7, 17.4), (1.6, 1.6, 1.6))
    collision_proxy(scene, generator, "Colisão do gerador", (0,.3,0), (.42,.3,.3))
    extinguisher=real_prop(scene, "quarentena", "korean_fire_extinguisher_01",
                           "Extintor de emergência real", props, (-5.8, .1, 15.4), (.8, .8, .8))
    collision_proxy(scene, extinguisher, "Colisão do extintor", (0,.42,0), (.18,.42,.18))
    for z in range(-25, 29, 6):
        light(scene, f"Balizador de emergência {z}", emergency,
              (0, 3.75, z), (1, .08, .04), 9, 6)
        light(scene, f"Lâmpada de trabalho {z}", work,
              (0, 3.65, z), (.87, .94, 1), 19, 8)
        shape(scene, f"Difusor de teto {z}", "Lamp", architecture,
              (0, 4.18, z), (.66, .06, .16), (.81, .91, 1), .22, 0, (.4, .54, .64), 2)
    shape(scene, "Baliza de extração", "Lamp", architecture, (0, 2.2, 27.5),
          (.55, .75, .2), (.15, 1, .56), .2, 0, (.08, .8, .24), 5)
    light(scene, "Luz da saída", work, (0, 3.5, 27), (.14, 1, .45), 25, 7)
    npc(scene, "MIRA · drone de manutenção", world, (1.4, 1.7, -22.5), "MaintenanceDrone",
        (.28, .42, .52), (.16, .72, 1), drone=True)
    actor = player(scene, "Operador", world, (0, 0, -25), "QuarantinePlayer", 5.1, 5.2)
    shape(scene, "Ferramenta de diagnóstico", "Steel", actor + 1,
          (.35, -.32, .72), (.11, .11, .31), (.55, .61, .65), .4, .72, uv_world=False)
    light(scene, "Luz ambiente", world, (0, 18, 0), (.68, .78, .93), .28, kind=0, rot=(60, 20, 0))
    scene.archive = lambda original=scene.archive: original().replace(base.INPUT, input_archive("Interagir"))
    return scene


def rescue() -> Scene:
    scene = Scene()
    world = scene.add("Mundo")
    tunnel = scene.add("Galeria da mina", world)
    debris = scene.add("Escombros móveis", world)
    pumps = scene.add("Bombas", world)
    survivors = scene.add("Sobreviventes", world)
    fixtures = scene.add("Iluminação da galeria", world)
    shape(scene, "Piso rochoso", "Ground", tunnel, (0, -.24, 0), (6, .24, 29),
          (.66, .58, .48), .94, 0, solid=True)
    for side in (-1, 1):
        shape(scene, f"Macico lateral {side}", "Rock", tunnel, (side * 6.1, 1.7, 0),
              (.8, 2.4, 29), (.67, .63, .58), .93, 0, solid=True)
        for z in range(-25, 29, 5):
            shape(scene, f"Escora vertical {side} {z}", "Timber", tunnel,
                  (side * 5.2, 2, z), (.27, 2, .33), (.92, .78, .59), .82)
            shape(scene, f"Cabo elétrico {side} {z}", "Pipe", tunnel,
                  (side * 5.48, 3.35, z + 2.2), (.07, 2.4, .07),
                  (.42, .48, .51), .5, .65, rot=(90, 0, 0))
    for z in range(-25, 29, 5):
        shape(scene, f"Viga do teto {z}", "Timber", tunnel, (0, 4, z),
              (5.6, .28, .36), (.87, .72, .51), .84)
        light(scene, f"Lâmpada de mina {z}", fixtures, (0, 3.45, z), (1, .63, .28), 11, 7)
        shape(scene, f"Abajur metálico {z}", "Steel", fixtures,
              (0, 3.65, z), (.52, .08, .42), (.62, .61, .54), .45, .6)
    for z in range(-26, 29, 2):
        shape(scene, f"Dormente {z}", "Timber", tunnel, (0, .05, z),
              (1.65, .08, .16), (.79, .64, .45), .83)
    for side in (-1, 1):
        shape(scene, f"Trilho {side}", "Steel", tunnel, (side * 1.1, .14, 0),
              (.09, .12, 28), (.71, .7, .66), .45, .82)
    for index, (x, z) in enumerate(((-3.8, -9), (3.8, 11)), 1):
        panel = shape(scene, f"Bomba {index}", "Steel", pumps, (x, 1.25, z),
                      (.9, 1.25, .8), (.55, .63, .69), .46, .76, solid=True)
        shape(scene, f"Volante da bomba {index}", "Valve", panel, (0, .3, -.83),
              (.48, .48, .48), (.95, .66, .26), .4, .7)
        shape(scene, "Indicador vermelho", "Lamp", panel, (-.4, .82, -.8),
              (.11, .11, .11), (1, .13, .05), .2, 0, (.8, .08, .02), 4)
        shape(scene, "Indicador verde", "Lamp", panel, (.4, .82, -.8),
              (.11, .11, .11), (.05, 1, .35), .2, 0, (.02, .9, .13), 4)
        light(scene, f"Luz da bomba {index}", fixtures, (x, 3.2, z), (.2, .67, 1), 13, 5)
    for index, (x, z) in enumerate(((-2.8, 1), (2.7, 18), (0, 25)), 1):
        npc(scene, f"Sobrevivente {index}", survivors, (x, 1.0, z), "SurvivorAgent",
            (.76, .55, .24), (.96, .8, .38))
        shape(scene, f"Maca {index}", "Timber", tunnel,
              (x, .3, z), (.75, .14, 1.2), (.66, .55, .4), .88)
    for index in range(15):
        x = (-1 if index % 2 else 1) * (2 + index % 4)
        z = -18 + index * 3.1
        shape(scene, f"Bloco móvel {index + 1}", "Rock", debris,
              (x, 2.8 + index % 3, z), (.42 + (index % 3) * .16,) * 3,
              (.79, .72, .61), .91, dynamic=True)
    for index, (x, z) in enumerate(((-3.2, -7.5), (3.1, 12.5), (.7, 24.5)), 1):
        kit=real_prop(scene, "resgate", "medical_box", f"Kit médico real {index}", debris,
                      (x, .65, z), (1.7, 1.7, 1.7), (base.body_component(2, 1.2),))
        collision_proxy(scene, kit, "Colisão do kit", (0,.12,0), (.25,.15,.34), body=False)
    for index, (x, z) in enumerate(((-4.2, -14), (4.3, 3), (-4.4, 20)), 1):
        rock=real_prop(scene, "resgate", "rock_07", f"Rocha escaneada real {index}", tunnel,
                       (x, 0, z), (5, 5, 5))
        collision_proxy(scene, rock, "Colisão da rocha", (0,.07,0), (.09,.08,.16))
    toolbox=real_prop(scene, "resgate", "metal_toolbox", "Caixa de ferramentas real", pumps,
                      (-4.1, .62, -7.7), (1.25, 1.25, 1.25))
    collision_proxy(scene, toolbox, "Colisão da caixa de ferramentas", (0,.14,0), (.28,.14,.2))
    for z in (-15, -2, 8, 22):
        shape(scene, f"Desabamento {z}", "Rock", tunnel,
              (4.9 if z % 2 else -4.9, 2.2, z), (1.35, 1.9, 1.9),
              (.69, .64, .57), .94)
    shape(scene, "Elevador de resgate", "Hazard", tunnel,
          (0, 2, 28), (2.4, 2, .3), (.9, .9, .9), .62, .2)
    light(scene, "Farol do elevador", fixtures, (0, 3.5, 27), (.16, 1, .42), 22, 8)
    actor = player(scene, "Socorrista", world, (0, 0, -26), "RescuePlayer", 5.25, 5.7)
    shape(scene, "Lanterna de mão", "Steel", actor + 1,
          (.33, -.28, .72), (.11, .1, .27), (.68, .7, .66), .38, .7, uv_world=False)
    light(scene, "Luz da lanterna", actor + 1, (.33, -.25, .9), (1, .89, .66), 13, 9, kind=2)
    light(scene, "Claridade da entrada", world, (0, 16, -35), (1, .85, .66), .22, kind=0, rot=(70, 25, 0))
    scene.archive = lambda original=scene.archive: original().replace(base.INPUT, input_archive("Interagir"))
    return scene


def perimeter() -> Scene:
    scene = Scene()
    world = scene.add("Mundo")
    field = scene.add("Complexo tático", world)
    covers = scene.add("Coberturas", world)
    enemies = scene.add("Sentinelas", world)
    debris = scene.add("Objetos físicos", world)
    illumination = scene.add("Iluminação", world)
    shape(scene, "Pátio de concreto", "Ground", field, (0, -.23, 0), (13, .23, 30),
          (.66, .69, .7), .84, 0, solid=True)
    for side in (-1, 1):
        shape(scene, f"Muralha lateral {side}", "Concrete", field,
              (side * 13.4, 2.5, 0), (.4, 2.5, 30), (.71, .72, .73), .84, 0, solid=True)
        for z in range(-26, 30, 7):
            shape(scene, f"Contraforte {side} {z}", "Concrete", field,
                  (side * 12.7, 2.2, z), (.75, 2.2, .52),
                  (.68, .7, .73), .86)
            shape(scene, f"Balizador {side} {z}", "Lamp", illumination,
                  (side * 12.45, 3.2, z), (.23, .18, .23),
                  (.2, .71, 1), .2, .05, (.08, .45, .75), 3)
            light(scene, f"Holofote {side} {z}", illumination,
                  (side * 11.9, 3.3, z), (.41, .68, 1), 17, 10)
    shape(scene, "Parede posterior", "Concrete", field, (0, 2.5, 30), (13, 2.5, .42), solid=True)
    shape(scene, "Parede da inserção", "Concrete", field, (0, 2.5, -30), (13, 2.5, .42), solid=True)
    for z in (-17, -6, 6, 18):
        for side in (-1, 1):
            x = side * 5.8
            shape(scene, f"Barreira de concreto {side} {z}", "Concrete", covers,
                  (x, .83, z), (2.2, .83, .48), (.73, .73, .72), .89, 0, solid=True)
            shape(scene, f"Faixa da barreira {side} {z}", "Hazard", covers,
                  (x, 1.68, z), (2.24, .1, .52), (1, 1, 1), .62, .1)
            shape(scene, f"Contêiner {side} {z}", "Steel", covers,
                  (side * 10, 1.35, z + 2.4), (1.7, 1.35, 2.2),
                  (.54, .65, .7), .56, .68, solid=True)
            for line in range(5):
                shape(scene, f"Friso de contêiner {side} {z} {line}", "Steel", covers,
                      (side * 8.25, .5 + line * .42, z + 2.4),
                      (.06, .05, 2.05), (.7, .75, .77), .43, .7)
    for index in range(12):
        side = -1 if index % 2 else 1
        shape(scene, f"Caixa solta {index + 1}", "Timber", debris,
              (side * (2.5 + index % 3), 2.9 + index % 2, -23 + index * 4.1),
              (.42, .42, .42), (.74, .64, .49), .82, 0, dynamic=True)
    for index, (x, z) in enumerate(((-2.9, -19), (3.1, -5), (-2.8, 10), (3.0, 23)), 1):
        crate=real_prop(scene, "perimetro", "old_military_crate", f"Caixote militar real {index}",
                        debris, (x, .58, z), (1.65, 1.65, 1.65),
                        (base.body_component(2, 4), base.script_component("ThrowableCrate")))
        collision_proxy(scene, crate, "Colisão física do caixote", (0,.0,0), (.62,.28,.36), body=False)
    real_prop(scene, "perimetro", "old_gas_mask", "Máscara de gás real", field,
              (-1.6, 1.05, 27), (.55, .55, .55))
    radio=real_prop(scene, "perimetro", "vintage_radio_transceiver", "Rádio de extração real", field,
                    (1.55, .86, 27.4), (.72, .72, .72))
    collision_proxy(scene, radio, "Colisão do rádio", (0,.18,0), (.3,.18,.22))
    for row, z in enumerate((-10, 0, 10, 19, 25)):
        for column, x in enumerate((-7.5, 0, 7.5)):
            index = row * 3 + column + 1
            npc(scene, f"Sentinela {index}", enemies, (x, 1.25, z), "SentinelAgent",
                (.42, .16, .13), (.93, .18, .08), drone=True)
            light(scene, f"Luz de sentinela {index}", illumination,
                  (x, 2.35, z), (1, .13, .07), 5, 3)
    shape(scene, "Terminal de extração", "Hazard", field,
          (0, 1.5, 28), (1.1, 1.5, .28), (1, 1, 1), .58, .2)
    light(scene, "Sinal de extração", illumination, (0, 3.7, 28), (.11, 1, .38), 25, 8)
    actor = player(scene, "Operador", world, (0, 0, -26), "TacticalPlayer", 6.3, 5.3, 76)
    shape(scene, "Arma de treinamento", "Steel", actor + 1,
          (.32, -.3, .78), (.13, .1, .42), (.58, .63, .67), .35, .8, uv_world=False)
    shape(scene, "Mira da arma", "Pipe", actor + 1,
          (.32, -.22, .92), (.035, .14, .035), (.24, .28, .31), .37, .84,
          rot=(90, 0, 0), uv_world=False)
    light(scene, "Céu nublado", world, (0, 24, 0), (.69, .79, .94), .55, kind=0, rot=(55, 20, 0))
    scene.archive = lambda original=scene.archive: original().replace(base.INPUT, input_archive("Ação"))
    return scene


SCRIPTS = {
    "quarentena": {"QuarantinePlayer.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.QuarantinePlayer")]
public sealed class QuarantinePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 180;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 3.8f;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.15f;
    private GameObject? _camera, _world, _emergency, _work, _door, _drone, _held;
    private float _oxygen, _doorHeight;
    private int _fuses;
    private bool _power, _finished;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _emergency = _world?.Find("Luzes de emergência");
        _work = _world?.Find("Iluminação restaurada");
        _door = _world?.Find("Porta blindada");
        _drone = _world?.Find("MIRA · drone de manutenção");
        _work?.SetActive(false);
        _oxygen = OxigenioInicial;
        _doorHeight = 2;
        Scene.Log(ObjectId, "QUARENTENA · 3 fusíveis · gerador · saída | O2 180 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _oxygen -= dt;
        if (_oxygen <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "SEM OXIGÊNIO · reinicie Play");
            return;
        }
        if (_power && _door is { IsAlive: true } && _doorHeight < 6.3f)
        {
            _doorHeight = MathF.Min(6.3f, _doorHeight + dt * 2.4f);
            Scene.MoveKinematic(_door.ObjectId, new Vector3(0, _doorHeight, 21), Quaternion.Identity);
        }
        if (_power && Object.Position.Z > 25)
        {
            _finished = true;
            Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · oxigênio restante " + MathF.Ceiling(_oxygen) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var target = AimTarget(_held);
        if (_held is { IsAlive: true } carried)
        {
            if (target?.Name == "Painel do gerador" && carried.Name.StartsWith("Fusível ", StringComparison.Ordinal))
            {
                carried.Destroy();
                _held = null;
                _fuses++;
                Scene.Log(ObjectId, "FUSÍVEL INSERIDO " + _fuses + "/3 · O2 " + MathF.Ceiling(_oxygen) + " s");
            }
            else Drop("OBJETO SOLTO");
            return;
        }
        if (target is null) { Scene.Log(ObjectId, "Mire em um fusível ou no painel"); return; }
        if (target.Name.StartsWith("Fusível ", StringComparison.Ordinal))
        {
            _held = target;
            Scene.Log(ObjectId, "SEGURANDO " + target.Name + " · leve ao painel e interaja");
        }
        else if (target.Name == "Painel do gerador")
        {
            if (_fuses < 3) Scene.Log(ObjectId, "Gerador exige 3 fusíveis; faltam " + (3 - _fuses));
            else if (!_power)
            {
                _power = true;
                _emergency?.SetActive(false);
                _work?.SetActive(true);
                FindBehavior<MaintenanceDrone>(_drone)?.PowerRestored();
                Scene.Log(ObjectId, "ENERGIA RESTAURADA · porta abrindo · alcance a saída");
            }
        }
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 42) { Drop("OBJETO PERDIDO"); return; }
        var velocity = delta * 13 - Scene.GetBodyVelocity(carried.ObjectId) * 2.7f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) Drop("NÃO FOI POSSÍVEL SEGURAR");
    }

    private GameObject? AimTarget(GameObject? ignored)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * AlcanceInteracao, out _,
                                      QueryFilter.Default.Ignoring(Object), 12);
        foreach (var hit in hits)
            if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }

    private void Drop(string message)
    {
        if (_held is { IsAlive: true }) Scene.SetBodyVelocity(_held.ObjectId, Vector3.Zero);
        _held = null;
        Scene.Log(ObjectId, message);
    }
}
''', "MaintenanceDrone.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.MaintenanceDrone")]
public sealed class MaintenanceDrone : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 3.2f;
    private GameObject? _player;
    private readonly GameObject?[] _fuses = new GameObject?[3];
    private bool _power;

    public override void Start()
    {
        var world = Object.Parent;
        _player = world?.Find("Operador");
        for (var i = 0; i < _fuses.Length; i++) _fuses[i] = world?.Find("Fusível " + (i + 1));
        Scene.Log(ObjectId, "MIRA ONLINE · seguindo e indicando o próximo módulo");
    }

    public void PowerRestored() => _power = true;

    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        var current = Object.WorldTransform.Position;
        var player = _player.WorldTransform.Position;
        var destination = player + new Vector3(1.45f, 1.65f, -1.15f);
        if (!_power)
            foreach (var fuse in _fuses)
                if (fuse is { IsAlive: true } && Vector3.DistanceSquared(player, fuse.WorldTransform.Position) < 110)
                { destination = fuse.WorldTransform.Position + Vector3.UnitY * .85f; break; }
        if (_power && player.Z > 18) destination = new Vector3(0, 2.2f, 25.5f);
        var delta = destination - current;
        if (delta.LengthSquared() < .12f) return;
        var direction = Vector3.Normalize(delta);
        var step = direction * MathF.Min(Velocidade * dt, delta.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.34f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } hit && hit.Object.ObjectId != _player.ObjectId)
        {
            var side = Vector3.Normalize(Vector3.Cross(Vector3.UnitY, direction));
            step = (direction * .25f + side * (((ObjectId & 1) == 0) ? 1 : -1)) * Velocidade * dt;
        }
        var yaw = MathF.Atan2(direction.X, direction.Z);
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, yaw));
    }
}
'''},
    "resgate": {"RescuePlayer.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.RescuePlayer")]
public sealed class RescuePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 240;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 4.1f;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.2f;
    private GameObject? _camera, _world, _held;
    private float _oxygen;
    private int _pumps, _survivors;
    private bool _finished;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _oxygen = OxigenioInicial;
        for (var index = 1; index <= 2; index++)
            _world?.Find("Bomba " + index)?.Find("Indicador verde")?.SetActive(false);
        Scene.Log(ObjectId, "RESGATE · 2 bombas · 3 sobreviventes · elevador | O2 240 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _oxygen -= dt * (_pumps == 2 ? .55f : 1);
        if (_oxygen <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "SEM OXIGÊNIO · reinicie Play");
            return;
        }
        if (_pumps == 2 && _survivors == 3 && Object.Position.Z > 26)
        {
            _finished = true;
            Scene.Log(ObjectId, "RESGATE CONCLUÍDO · equipe evacuada · O2 " + MathF.Ceiling(_oxygen) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var target = AimTarget(_held);
        if (_held is { IsAlive: true } carried)
        {
            var survivor = FindBehavior<SurvivorAgent>(target);
            if (carried.Name.StartsWith("Kit médico real ", StringComparison.Ordinal) && survivor?.Heal() == true)
            {
                carried.Destroy();
                _held = null;
                _survivors++;
                Scene.Log(ObjectId, "SOBREVIVENTE ESTABILIZADO " + _survivors + "/3 · ele seguirá a equipe");
            }
            else
            {
                var pose = _camera.WorldTransform;
                var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
                Scene.SetBodyVelocity(carried.ObjectId, forward * 6 + Vector3.UnitY * 1.4f);
                _held = null;
                Scene.Log(ObjectId, "OBJETO ARREMESSADO · use kits nos sobreviventes");
            }
            return;
        }
        if (target is null) { Scene.Log(ObjectId, "Mire em bomba, kit, escombro ou sobrevivente"); return; }
        if (target.Name.StartsWith("Bomba ", StringComparison.Ordinal))
        {
            var green = target.Find("Indicador verde");
            if (green is { ActiveInHierarchy: true }) return;
            target.Find("Indicador vermelho")?.SetActive(false);
            green?.SetActive(true);
            _pumps++;
            Scene.Log(ObjectId, "BOMBAS " + _pumps + "/2 · drenagem ativada");
        }
        else if (target.Name.StartsWith("Kit médico real ", StringComparison.Ordinal) ||
                 target.Name.StartsWith("Bloco móvel ", StringComparison.Ordinal))
        {
            _held = target;
            Scene.Log(ObjectId, "SEGURANDO " + target.Name + " · interaja para usar ou arremessar");
        }
        else if (FindBehavior<SurvivorAgent>(target) is { IsHealed: false })
        {
            Scene.Log(ObjectId, "O sobrevivente precisa de um kit médico");
        }
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 42) { _held = null; return; }
        var velocity = delta * 12.5f - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    private GameObject? AimTarget(GameObject? ignored)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * AlcanceInteracao, out _,
                                      QueryFilter.Default.Ignoring(Object), 12);
        foreach (var hit in hits)
            if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }
}
''', "SurvivorAgent.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.SurvivorAgent")]
public sealed class SurvivorAgent : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 2.75f;
    private GameObject? _player;
    private Vector3 _rest;
    private float _phase;
    public bool IsHealed { get; private set; }

    public override void Start()
    {
        _player = Object.Parent?.Parent?.Find("Socorrista");
        _rest = Object.WorldTransform.Position;
        _phase = (ObjectId % 9) * .47f;
    }

    public bool Heal()
    {
        if (IsHealed) return false;
        IsHealed = true;
        return true;
    }

    public override void FixedUpdate(float dt)
    {
        _phase += dt;
        var current = Object.WorldTransform.Position;
        if (!IsHealed)
        {
            var idle = _rest + Vector3.UnitY * (MathF.Sin(_phase * 2) * .025f);
            Scene.MoveKinematic(ObjectId, idle, Quaternion.Identity);
            return;
        }
        if (_player is not { IsAlive: true }) return;
        var player = _player.WorldTransform.Position;
        var side = ((int)(ObjectId % 3) - 1) * .85f;
        var destination = player + new Vector3(side, 1.0f, -1.7f - (ObjectId % 2) * .65f);
        var delta = destination - current;
        delta.Y = 0;
        if (delta.LengthSquared() < .45f) return;
        var direction = Vector3.Normalize(delta);
        var step = direction * MathF.Min(Velocidade * dt, delta.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Capsule(.3f, .75f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } hit && hit.Object.ObjectId != _player.ObjectId)
        {
            var avoid = Vector3.Normalize(Vector3.Cross(Vector3.UnitY, direction));
            step = (direction * .25f + avoid * (((ObjectId & 1) == 0) ? 1 : -1)) * Velocidade * dt;
        }
        var yaw = MathF.Atan2(direction.X, direction.Z);
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, yaw));
    }
}
'''},
    "perimetro": {"TacticalPlayer.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.TacticalPlayer")]
public sealed class TacticalPlayer : Behavior
{
    [PropertyId("municaoPente")] public int MunicaoPente = 12;
    [PropertyId("alcanceTiro")] public float AlcanceTiro = 48;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.25f;
    private GameObject? _camera, _held;
    private float _health = 100, _cooldown, _reload, _damageCooldown;
    private int _ammo, _kills;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos");
        _ammo = MunicaoPente;
        Scene.Log(ObjectId, "PERÍMETRO · AÇÃO pega/lança caixotes ou dispara · 15 sentinelas");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _cooldown = MathF.Max(0, _cooldown - dt);
        _damageCooldown = MathF.Max(0, _damageCooldown - dt);
        if (_reload > 0)
        {
            _reload -= dt;
            if (_reload <= 0)
            {
                _ammo = MunicaoPente;
                Scene.Log(ObjectId, "RECARREGADO · MUNIÇÃO " + _ammo + " · VIDA " + _health);
            }
        }
        if (!Input.JustPressed("Ação") || _camera is not { IsAlive: true }) return;
        if (_held is { IsAlive: true } carried)
        {
            var heldPose = _camera.WorldTransform;
            var heldForward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, heldPose.Rotation));
            FindBehavior<ThrowableCrate>(carried)?.Arm();
            Scene.SetBodyVelocity(carried.ObjectId, heldForward * 13 + Vector3.UnitY * 2.2f);
            _held = null;
            Scene.Log(ObjectId, "CAIXOTE LANÇADO · impactos fortes neutralizam sentinelas");
            return;
        }
        var close = AimTarget(4.2f);
        if (close?.Name.StartsWith("Caixote militar real ", StringComparison.Ordinal) == true)
        {
            _held = close;
            Scene.Log(ObjectId, "SEGURANDO " + close.Name + " · pressione AÇÃO para lançar");
            return;
        }
        if (close?.Name == "Terminal de extração")
        {
            if (_kills < 15) Scene.Log(ObjectId, "TERMINAL BLOQUEADO · restam " + (15 - _kills) + " sentinelas");
            else { _finished = true; Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · VIDA " + _health); }
            return;
        }
        if (_cooldown > 0 || _reload > 0) return;
        if (_ammo == 0)
        {
            _reload = 1.7f;
            Scene.Log(ObjectId, "RECARREGANDO · proteja-se atrás da cobertura");
            return;
        }
        _ammo--;
        _cooldown = .22f;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceTiro, QueryFilter.Default.Ignoring(Object));
        if (hit is { } contact)
        {
            var enemy = FindBehavior<SentinelAgent>(contact.Object);
            if (enemy != null && enemy.Damage(1))
            {
                RegisterKill();
            }
            else Scene.Log(ObjectId, "IMPACTO: " + contact.Object.Name + " · MUNIÇÃO " + _ammo);
        }
        else Scene.Log(ObjectId, "SEM ALVO · MUNIÇÃO " + _ammo);
        if (_ammo == 0 && _kills < 15) _reload = 1.7f;
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 45) { _held = null; return; }
        var velocity = delta * 12 - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    public void RegisterKill()
    {
        if (_finished) return;
        _kills = Math.Min(15, _kills + 1);
        Scene.Log(ObjectId, _kills == 15
            ? "ÁREA SEGURA · interaja com o terminal de extração"
            : "ALVO NEUTRALIZADO " + _kills + "/15 · MUNIÇÃO " + _ammo + " · VIDA " + _health);
    }

    private GameObject? AimTarget(float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        return Physics.RayCast(pose.Position, forward * range,
                               QueryFilter.Default.Ignoring(Object))?.Object;
    }

    public void TakeDamage(int amount)
    {
        if (_finished || _damageCooldown > 0) return;
        _health = MathF.Max(0, _health - amount);
        _damageCooldown = .38f;
        if (_health == 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "OPERADOR CAÍDO · reinicie Play");
        }
        else Scene.Log(ObjectId, "SOB FOGO · VIDA " + _health + " · ALVOS " + _kills + "/15");
    }
}
''', "SentinelAgent.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.SentinelAgent")]
public sealed class SentinelAgent : Behavior
{
    [PropertyId("alcance")] public float Alcance = 17;
    [PropertyId("velocidade")] public float Velocidade = 1.8f;
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _shot;
    private int _health = 2;

    public override void Start()
    {
        _player = Object.Parent?.Parent?.Find("Operador");
        _home = Object.WorldTransform.Position;
        _phase = (ObjectId % 11) * .71f;
        _shot = 1 + (ObjectId % 5) * .35f;
    }

    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt * .72f;
        var current = Object.WorldTransform.Position;
        var target = _player.WorldTransform.Position + new Vector3(0, 1.1f, 0);
        var toPlayer = target - current;
        var distance = toPlayer.Length();
        var destination = _home + new Vector3(MathF.Sin(_phase) * 1.45f, 0, MathF.Cos(_phase * .7f) * .55f);
        if (distance < Alcance * 1.35f)
        {
            var forward = distance > .01f ? toPlayer / distance : Vector3.UnitZ;
            var strafe = Vector3.Cross(Vector3.UnitY, forward) * MathF.Sin(_phase * 1.7f);
            destination = distance > 9 ? current + forward * 2.2f : current + strafe * 1.7f;
        }
        var travel = destination - current;
        travel.Y = 0;
        var direction = travel.LengthSquared() > .01f ? Vector3.Normalize(travel) : Vector3.UnitZ;
        var step = direction * MathF.Min(Velocidade * dt, travel.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.4f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
            step = Vector3.Cross(Vector3.UnitY, direction) * Velocidade * dt * (((ObjectId & 1) == 0) ? 1 : -1);
        var next = current + step;
        var facing = MathF.Atan2(toPlayer.X, toPlayer.Z);
        Scene.MoveKinematic(ObjectId, next, Quaternion.CreateFromAxisAngle(Vector3.UnitY, facing));
        _shot -= dt;
        if (_shot > 0) return;
        _shot = 1.5f + (ObjectId % 4) * .22f;
        var from = next + new Vector3(0, .3f, 0);
        var shot = target - from;
        if (shot.LengthSquared() > Alcance * Alcance) return;
        var line = Physics.RayCast(from, shot, QueryFilter.Default.Ignoring(Object));
        if (line is { } hit && hit.Object.ObjectId != _player.ObjectId) return;
        FindBehavior<TacticalPlayer>(_player)?.TakeDamage(5);
    }

    public bool Damage(int amount)
    {
        _health -= amount;
        if (_health > 0) return false;
        Object.Destroy();
        return true;
    }
}
''', "ThrowableCrate.cs": '''using Astra;
using System.Numerics;

[ComponentId("project.ThrowableCrate")]
public sealed class ThrowableCrate : Behavior
{
    private float _armed;
    private float _speed;

    public void Arm() => _armed = 3.5f;

    public override void FixedUpdate(float dt)
    {
        _armed = System.MathF.Max(0, _armed - dt);
        _speed = Scene.GetBodyVelocity(ObjectId).Length();
    }

    public override void CollisionEnter(Collision collision)
    {
        if (_armed <= 0 || _speed < 5.5f) return;
        var target = Resolve(collision.Other);
        var sentinel = FindBehavior<SentinelAgent>(target);
        if (sentinel is null) return;
        _armed = 0;
        if (!sentinel.Damage(2)) return;
        var player = Object.Parent?.Parent?.Find("Operador");
        FindBehavior<TacticalPlayer>(player)?.RegisterKill();
    }
}
'''},
}


def write_project(slug: str, title: str, scene: Scene, glb: bytes, objective: str):
    folder = OUT / slug
    for name in ("Assets", "Scripts", "scenes"):
        (folder / name).mkdir(parents=True, exist_ok=True)
    (folder / SOURCE).write_bytes(glb)
    (folder / "scenes/editor.aescene").write_text(scene.archive(), encoding="utf-8")
    (folder / "scenes/main.ascene").write_text(
        json.dumps({"format": "ASTRA-SCENE-1", "template": "empty", "nodes": []}) + "\n", encoding="utf-8")
    records = []
    for asset in (SOURCE, *(f"Assets/{name}.glb" for name in REAL_MODELS[slug])):
        source_guid = base.guid("fonte:" + asset)
        digest = hashlib.sha256((folder / asset).read_bytes()).hexdigest()
        records.append(f'{source_guid} mesh "{asset}" "{asset}" "{digest}" 1 "glb" 0 0')
    registry = f'AETHER_ASSETS 1 {len(records)}\n' + "\n".join(records) + "\n"
    (folder / "assets.astra").write_text(registry, encoding="utf-8")
    for filename, content in SCRIPTS[slug].items():
        (folder / "Scripts" / filename).write_text(content, encoding="utf-8")
    (folder / "LEIA-ME.md").write_text(
        f"# {title}\n\n{objective}\n\n"
        "Controles: arraste esquerdo para mover, arraste direito para olhar, "
        "Saltar para pular e o segundo botão para a ação do jogo. "
        "Mire pelo retículo. Na primeira abertura, abra Código e use Recompilar projeto; depois use Play. "
        "O texto no Play mostra a última mensagem do jogo. Stop restaura a cena para outra partida.\n\n"
        "Toda a geometria, luz, câmera, física, NPCs e scripts podem ser editados no projeto. "
        "Os objetos seguráveis continuam corpos físicos e colidem enquanto são conduzidos em FixedUpdate. "
        "A arquitetura usa texturas 1024 PBR projetadas por metro; os equipamentos reais são GLBs com materiais PBR "
        "e UVs da fonte. Veja Assets/FONTES-ARTE.md para origem e licença. "
        "Não há áudio nem animação esquelética.\n",
        encoding="utf-8")
    print(f"{title}: {len(scene.entities)} entidades, {len(SCRIPTS[slug])} scripts")


def write_thumbnail(slug: str):
    folder = ROOT / "android/app/src/main/assets/astra/thumbs"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"example-{slug}.png").write_bytes(
        (ROOT / "tools/assets/highlevel-thumbnails" / f"{slug}.png").read_bytes())


if __name__ == "__main__":
    glb = world_glb()
    projects = (
        ("quarentena", "Quarentena 04", quarantine(),
         "Siga o drone MIRA, segure e encaixe três fusíveis físicos, reative o gerador e escape antes do oxigênio acabar."),
        ("resgate", "Resgate na Mina", rescue(),
         "Ligue duas bombas, carregue kits médicos até três NPCs e conduza a equipe pelo túnel enquanto move escombros."),
        ("perimetro", "Perímetro Delta", perimeter(),
         "Enfrente quinze sentinelas com IA usando tiro, cobertura e caixotes físicos que podem ser segurados e lançados. "
         "Depois, interaja com o terminal de extração."),
    )
    for slug, title, scene, objective in projects:
        write_project(slug, title, scene, glb, objective)
        write_thumbnail(slug)
