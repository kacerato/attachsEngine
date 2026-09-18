"""Build the offline, editable Linha Fantasma first-person Astra example.

The CC0 station is downloaded as a real GLB. Its repeated instances are baked
into eight material meshes with metre-scale UVs so a mobile GPU can draw it.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import shutil
import struct
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "android/app/src/main/assets/astra/example-projects/linha-fantasma"
ASSETS = PROJECT / "Assets"
SOURCE_URL = "https://cdn.3dassets.dev/assets/13725/v1/model.glb"
SOURCE_SHA256 = "44ca7750f0a496b64e3736e679540172e45677571463b8d572634c2f55d17366"
SURFACES = {0: "floor_tiles_09", 4: "metal_plate_02"}
PROPS = ("power_box_01", "vintage_flashlight", "retro_multimeter", "portable_generator")


def module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / filename)
    assert spec and spec.loader
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


high = module("astra_highlevel", "generate-highlevel-games.py")
fetcher = module("astra_fetch", "fetch-highlevel-game-assets.py")
base = high.base


def sha(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_glb(value: bytes):
    if value[:4] != b"glTF" or struct.unpack_from("<I", value, 4)[0] != 2:
        raise ValueError("GLB v2 esperado")
    json_size, json_type = struct.unpack_from("<I4s", value, 12)
    if json_type != b"JSON":
        raise ValueError("JSON ausente")
    document = json.loads(value[20:20 + json_size])
    offset = 20 + json_size
    bin_size, bin_type = struct.unpack_from("<I4s", value, offset)
    if bin_type != b"BIN\0":
        raise ValueError("BIN ausente")
    return document, value[offset + 8:offset + 8 + bin_size]


def pack_glb(document: dict, binary: bytearray) -> bytes:
    while len(binary) % 4:
        binary.append(0)
    document["buffers"] = [{"byteLength": len(binary)}]
    encoded = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    return (struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(encoded) + 8 + len(binary))
            + struct.pack("<I4s", len(encoded), b"JSON") + encoded
            + struct.pack("<I4s", len(binary), b"BIN\0") + binary)


def fetch_assets():
    ASSETS.mkdir(parents=True, exist_ok=True)
    source = ASSETS / "station-source.glb"
    if not source.exists() or sha(source.read_bytes()) != SOURCE_SHA256:
        value = fetcher.get(SOURCE_URL)
        if sha(value) != SOURCE_SHA256:
            raise ValueError("A estação publicada mudou; revisar antes de empacotar")
        source.write_bytes(value)
    for model in PROPS:
        target = ASSETS / f"{model}.glb"
        if target.exists():
            continue
        if model == "portable_generator":
            existing = high.OUT / "quarentena/Assets/portable_generator.glb"
            if existing.exists():
                shutil.copy2(existing, target)
                continue
        fetcher.fetch(model, target)
    images = {}
    for surface in SURFACES.values():
        files = json.loads(fetcher.get(f"https://api.polyhaven.com/files/{surface}"))
        for channel in ("Diffuse", "nor_gl", "arm"):
            item = files[channel]["1k"]["jpg"]
            target = ASSETS / f"{surface}-{channel}.jpg"
            if not target.exists() or hashlib.md5(target.read_bytes()).hexdigest() != item["md5"]:
                target.write_bytes(fetcher.get(item["url"], item["md5"]))
            images[(surface, channel)] = target.read_bytes()
    (ASSETS / "world.glb").write_bytes(high.world_glb())
    return source.read_bytes(), images


def quaternion_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y*y + z*z), 2 * (x*y - z*w), 2 * (x*z + y*w)],
                     [2 * (x*y + z*w), 1 - 2 * (x*x + z*z), 2 * (y*z - x*w)],
                     [2 * (x*z - y*w), 2 * (y*z + x*w), 1 - 2 * (x*x + y*y)]], dtype=np.float64)


def optimize_station(source: bytes, images: dict) -> bytes:
    original, binary = read_glb(source)
    if set(original.get("extensionsRequired", [])) - {"KHR_mesh_quantization"}:
        raise ValueError("Extensão da estação não suportada")
    kinds = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16,
             5125: np.uint32, 5126: np.float32}
    widths = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def accessor(index):
        entry = original["accessors"][index]
        view = original["bufferViews"][entry["bufferView"]]
        dtype = np.dtype(kinds[entry["componentType"]]).newbyteorder("<")
        width = widths[entry["type"]]
        stride = view.get("byteStride", dtype.itemsize * width)
        offset = view.get("byteOffset", 0) + entry.get("byteOffset", 0)
        result = np.ndarray((entry["count"], width), dtype=dtype, buffer=binary,
                            offset=offset, strides=(stride, dtype.itemsize)).copy()
        if entry.get("normalized"):
            if np.issubdtype(dtype, np.signedinteger):
                result = np.maximum(result.astype(np.float64) / np.iinfo(dtype).max, -1)
            else:
                result = result.astype(np.float64) / np.iinfo(dtype).max
        return result

    nodes = original["nodes"]
    parents = {}
    for parent, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child] = parent
    matrices = {}

    def world_matrix(index):
        if index in matrices:
            return matrices[index]
        node = nodes[index]
        local = np.eye(4)
        local[:3, :3] = quaternion_matrix(node.get("rotation", (0, 0, 0, 1))) @ np.diag(node.get("scale", (1, 1, 1)))
        local[:3, 3] = node.get("translation", (0, 0, 0))
        matrices[index] = world_matrix(parents[index]) @ local if index in parents else local
        return matrices[index]

    grouped = {i: {"p": [], "n": [], "uv": [], "t": []} for i in range(len(original["materials"]))}
    for index, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        matrix = world_matrix(index)
        normal_matrix = np.linalg.inv(matrix[:3, :3]).T
        for primitive in original["meshes"][node["mesh"]]["primitives"]:
            if primitive.get("mode", 4) != 4:
                raise ValueError("A estação tem topologia não triangular")
            material = primitive.get("material", 0)
            attributes = primitive["attributes"]
            points = accessor(attributes["POSITION"]).astype(np.float64)
            normals = accessor(attributes["NORMAL"]).astype(np.float64)
            indices = accessor(primitive["indices"]).ravel().astype(np.int64)
            points = points @ matrix[:3, :3].T + matrix[:3, 3]
            normals = normals @ normal_matrix.T
            normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
            triangles = points[indices].reshape(-1, 3, 3)
            vertex_normals = normals[indices].reshape(-1, 3, 3)
            faces = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
            major = np.argmax(np.abs(faces), axis=1)
            uv = np.empty((len(triangles), 3, 2), dtype=np.float64)
            tangent = np.empty((len(triangles), 3, 4), dtype=np.float64)
            for axis, projection, direction in ((0, (2, 1), (0, 0, 1)),
                                                (1, (0, 2), (1, 0, 0)),
                                                (2, (0, 1), (1, 0, 0))):
                selected = major == axis
                if not selected.any():
                    continue
                uv[selected] = triangles[selected][:, :, projection] / 2.0
                n = vertex_normals[selected]
                along = np.broadcast_to(direction, n.shape)
                along = along - n * np.sum(along * n, axis=2, keepdims=True)
                along /= np.maximum(np.linalg.norm(along, axis=2, keepdims=True), 1e-12)
                bitangent = np.zeros_like(n)
                bitangent[:] = np.eye(3)[projection[1]]
                sign = np.sign(np.sum(np.cross(n, along) * bitangent, axis=2))
                tangent[selected] = np.concatenate((along, sign[:, :, None]), axis=2)
            group = grouped[material]
            group["p"].append(triangles.reshape(-1, 3).astype("<f4"))
            group["n"].append(vertex_normals.reshape(-1, 3).astype("<f4"))
            group["uv"].append(uv.reshape(-1, 2).astype("<f4"))
            group["t"].append(tangent.reshape(-1, 4).astype("<f4"))

    output = bytearray()
    views, accessors, meshes, mesh_nodes = [], [], [], []

    def add_view(value: bytes, target: int | None = None):
        output.extend(b"\0" * (-len(output) % 4))
        entry = {"buffer": 0, "byteOffset": len(output), "byteLength": len(value)}
        if target is not None:
            entry["target"] = target
        views.append(entry)
        output.extend(value)
        return len(views) - 1

    def add_accessor(value: np.ndarray, gltf_type: str, component: int, target: int):
        view = add_view(value.tobytes(), target)
        entry = {"bufferView": view, "componentType": component, "count": len(value), "type": gltf_type}
        if gltf_type == "VEC3" and component == 5126 and target == 34962 and value.shape[1] == 3:
            entry["min"] = value.min(axis=0).astype(float).tolist()
            entry["max"] = value.max(axis=0).astype(float).tolist()
        accessors.append(entry)
        return len(accessors) - 1

    for material, group in grouped.items():
        if not group["p"]:
            continue
        values = {key: np.concatenate(parts) for key, parts in group.items()}
        count = len(values["p"])
        attrs = {"POSITION": add_accessor(values["p"], "VEC3", 5126, 34962),
                 "NORMAL": add_accessor(values["n"], "VEC3", 5126, 34962),
                 "TEXCOORD_0": add_accessor(values["uv"], "VEC2", 5126, 34962),
                 "TANGENT": add_accessor(values["t"], "VEC4", 5126, 34962)}
        indices = add_accessor(np.arange(count, dtype="<u4")[:, None], "SCALAR", 5125, 34963)
        name = original["materials"][material].get("name", f"material-{material}")
        meshes.append({"name": name, "primitives": [{"attributes": attrs, "indices": indices, "material": material}]})
        mesh_nodes.append({"name": name, "mesh": len(meshes) - 1})

    result = {"asset": {"version": "2.0", "generator": "Astra Linha Fantasma station baker"},
              "bufferViews": views, "accessors": accessors, "meshes": meshes, "nodes": mesh_nodes,
              "scenes": [{"nodes": list(range(len(mesh_nodes)))}], "scene": 0,
              "materials": original["materials"], "images": [], "textures": [],
              "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]}
    for material, surface in SURFACES.items():
        pbr = result["materials"][material].setdefault("pbrMetallicRoughness", {})
        for channel in ("Diffuse", "nor_gl", "arm"):
            picture = add_view(images[(surface, channel)])
            result["images"].append({"bufferView": picture, "mimeType": "image/jpeg"})
            result["textures"].append({"source": len(result["images"]) - 1, "sampler": 0})
            texture = len(result["textures"]) - 1
            if channel == "Diffuse":
                pbr["baseColorTexture"] = {"index": texture}
            elif channel == "nor_gl":
                result["materials"][material]["normalTexture"] = {"index": texture, "scale": 0.65}
            else:
                pbr["metallicRoughnessTexture"] = {"index": texture}
    return pack_glb(result, output)


def world_mesh(scene, parent: int, material: str):
    asset = "Assets/station.glb"
    source_guid = base.guid("fonte:" + asset)
    identity = base.guid("glb:" + source_guid + ":" + material + "/" + material + "#0")
    return scene.add(f"Estação · {material}", parent, 1, components=(high.source_mesh_component(identity),), static=True)


def make_scene():
    scene = high.Scene()
    world = scene.add("Mundo")
    station = scene.add("Estação GLB otimizada", world)
    for name in ("concrete", "shell", "warn", "dark", "steel", "rubber", "accent", "glass"):
        world_mesh(scene, station, name)
    architecture = scene.add("Colisões arquitetônicas", world)
    controls = scene.add("Objetivos interativos", world)
    props = scene.add("Objetos reais e físicos", world)
    emergency = scene.add("Luzes de emergência", world)
    restored = scene.add("Luzes restauradas", world)
    high.collision_proxy(scene, architecture, "Piso da plataforma", (0, .79, 0), (3, .79, 18))
    for side in (-1, 1):
        high.collision_proxy(scene, architecture, f"Limite lateral {side}", (side * 3.15, 1.9, 0), (.14, 1.6, 18))
    high.collision_proxy(scene, architecture, "Túnel bloqueado", (0, 2, -18.1), (3, 2, .2))
    for index, (x, z) in enumerate(((-1.65, -11.8), (1.7, -1.5), (-1.55, 8.5)), 1):
        high.shape(scene, f"Fusível {index}", "Hazard", controls, (x, 2.25, z),
                   (.24, .28, .18), (.99, .68, .23), .33, .2, (.5, .23, .02), 2.4, solid=True)
        high.light(scene, f"Sinal do fusível {index}", emergency, (x, 2.6, z), (1, .49, .11), 5.5, 3)
    high.shape(scene, "Painel de energia", "Steel", controls, (1.8, 2.38, 13.2),
               (.58, .72, .24), (.58, .65, .68), .38, .58, solid=True)
    high.shape(scene, "Indicador vermelho", "Lamp", controls, (1.65, 2.66, 12.94),
               (.11, .11, .06), (1, .16, .08), .2, 0, (.85, .06, .02), 3)
    high.shape(scene, "Indicador verde", "Lamp", controls, (1.95, 2.66, 12.94),
               (.11, .11, .06), (.12, 1, .36), .2, 0, (.04, .7, .15), 3)
    high.shape(scene, "Barreira de segurança", "Hazard", controls, (0, 2.45, 16.2),
               (2.6, .85, .12), (.8, .82, .8), .53, .22, kinematic=True)
    high.shape(scene, "Zona de extração", "Lamp", controls, (0, 1.68, 17.6),
               (.85, .07, .32), (.11, 1, .43), .35, 0, (.04, .5, .13), 2)
    high.real_prop(scene, "linha-fantasma", "portable_generator", "Gerador real", props,
                   (-2.15, 1.57, -14.5), (1.3, 1.3, 1.3))
    high.real_prop(scene, "linha-fantasma", "power_box_01", "Quadro elétrico real", props,
                   (2.12, 2.05, 13.4), (.75, .75, .75))
    high.real_prop(scene, "linha-fantasma", "retro_multimeter", "Multímetro real", props,
                   (1.25, 1.63, 12.9), (.8, .8, .8))
    for index, (x, z) in enumerate(((-2.15, -7), (2.05, 3.5), (-2.1, 11.6)), 1):
        high.shape(scene, f"Caixa móvel {index}", "Timber", props, (x, 2.5, z),
                   (.4, .4, .4), (.62, .55, .44), .83, dynamic=True)
    for z in (-14, -8, -2, 4, 10, 15):
        for side in (-1, 1):
            high.light(scene, f"Emergência {side} {z}", emergency, (side * 2.6, 3.4, z),
                       (1, .18, .09), 7, 5)
            high.light(scene, f"Plafon {side} {z}", restored, (side * 1.65, 4.8, z),
                       (.78, .9, 1), 12, 7)
    high.light(scene, "Luz ambiente", world, (0, 12, 0), (.62, .7, .83), .28,
               kind=0, rot=(70, 30, 0))
    actor = high.player(scene, "Técnica de manutenção", world, (0, 1.61, -15.2),
                        "MetroPlayer", speed=5.2, jump=5.2, fov=75)
    camera = actor + 1
    flashlight = high.real_prop(scene, "linha-fantasma", "vintage_flashlight", "Lanterna real",
                                camera, (.3, -.31, .55), (.34, .34, .34))
    high.light(scene, "Facho da lanterna", flashlight, (0, 0, .34), (1, .87, .65), 17, 11, kind=2)
    scene.archive = lambda original=scene.archive: original().replace(base.INPUT, high.input_archive("Interagir"))
    return scene


SCRIPT = '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.MetroPlayer")]
public sealed class MetroPlayer : Behavior
{
    [PropertyId("tempoInicial")] public float TempoInicial = 300;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 3.7f;
    private GameObject? _camera, _world, _emergency, _restored, _gate, _flashlight;
    private float _time, _gateHeight, _flicker;
    private int _fuses;
    private bool _powered, _finished, _flashlightOn = true;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _emergency = _world?.Find("Luzes de emergência");
        _restored = _world?.Find("Luzes restauradas");
        _gate = _world?.Find("Barreira de segurança");
        _flashlight = _world?.Find("Lanterna real");
        _world?.Find("Indicador verde")?.SetActive(false);
        _restored?.SetActive(false);
        _time = TempoInicial;
        _gateHeight = 2.45f;
        Scene.Log(ObjectId, "LINHA FANTASMA · 3 fusíveis · energia · catraca · trem | 300 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _time -= dt;
        if (_time <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "O ÚLTIMO TREM PARTIU · reinicie Play");
            return;
        }
        if (!_powered && _emergency is { IsAlive: true })
        {
            _flicker += dt;
            if (_flicker > .18f)
            {
                _flicker = 0;
                _emergency.SetActive((int)(_time * 13) % 9 != 0);
            }
        }
        if (_powered && _gate is { IsAlive: true } && _gateHeight < 5.2f)
        {
            _gateHeight = MathF.Min(5.2f, _gateHeight + dt * 2.2f);
            Scene.MoveKinematic(_gate.ObjectId, new Vector3(0, _gateHeight, 16.2f), Quaternion.Identity);
        }
        if (_powered && Object.Position.Z > 17.1f)
        {
            _finished = true;
            Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · tempo restante " + MathF.Ceiling(_time) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceInteracao, QueryFilter.Default.Ignoring(Object));
        if (hit is null)
        {
            _flashlightOn = !_flashlightOn;
            _flashlight?.SetActive(_flashlightOn);
            Scene.Log(ObjectId, _flashlightOn ? "LANTERNA LIGADA" : "LANTERNA DESLIGADA");
            return;
        }
        var target = hit.Value.Object;
        if (target.Name.StartsWith("Fusível ", StringComparison.Ordinal))
        {
            target.Destroy();
            _fuses++;
            Scene.Log(ObjectId, "FUSÍVEIS " + _fuses + "/3 · tempo " + MathF.Ceiling(_time) + " s");
        }
        else if (target.Name == "Painel de energia")
        {
            if (_fuses < 3)
                Scene.Log(ObjectId, "PAINEL BLOQUEADO · faltam " + (3 - _fuses) + " fusíveis");
            else if (!_powered)
            {
                _powered = true;
                _emergency?.SetActive(false);
                _restored?.SetActive(true);
                _world?.Find("Indicador vermelho")?.SetActive(false);
                _world?.Find("Indicador verde")?.SetActive(true);
                Scene.Log(ObjectId, "ENERGIA RESTAURADA · catraca abrindo · siga até o trem");
            }
        }
        else if (target.Name.StartsWith("Caixa móvel ", StringComparison.Ordinal))
        {
            Scene.AddImpulse(target.ObjectId, forward * 12 + Vector3.UnitY * 1.5f);
            Scene.Log(ObjectId, "OBSTÁCULO DESLOCADO · tempo " + MathF.Ceiling(_time) + " s");
        }
    }
}
'''


def write_project(scene, source: bytes, optimized: bytes):
    (ASSETS / "station.glb").write_bytes(optimized)
    (PROJECT / "Scripts").mkdir(parents=True, exist_ok=True)
    (PROJECT / "scenes").mkdir(parents=True, exist_ok=True)
    (PROJECT / "Scripts/MetroPlayer.cs").write_text(SCRIPT, encoding="utf-8")
    (PROJECT / "scenes/editor.aescene").write_text(scene.archive(), encoding="utf-8")
    (PROJECT / "scenes/main.ascene").write_text(
        json.dumps({"format": "ASTRA-SCENE-1", "template": "empty", "nodes": []}) + "\n", encoding="utf-8")
    registered = ["Assets/world.glb", "Assets/station.glb", *(f"Assets/{name}.glb" for name in PROPS)]
    records = []
    for path in registered:
        digest = sha((PROJECT / path).read_bytes())
        records.append(f'{base.guid("fonte:" + path)} mesh "{path}" "{path}" "{digest}" 1 "glb" 0 0')
    (PROJECT / "assets.astra").write_text(
        f'AETHER_ASSETS 1 {len(records)}\n' + "\n".join(records) + "\n", encoding="utf-8")
    (ASSETS / "FONTES-ARTE.md").write_text(
        "# Arte do projeto\n\n"
        "- [Estação de metrô original](https://3dassets.dev/assets/modern-rail-and-metro-network-metro-stat-14053602-starter-scene), "
        "[GLB baixado](https://cdn.3dassets.dev/assets/13725/v1/model.glb), CC0 1.0. "
        f"SHA-256: `{sha(source)}`. A versão station.glb agrupa a mesma geometria em oito materiais, "
        "gera UV em metros e aplica texturas PBR; station-source.glb preserva a fonte.\n"
        + "\n".join(f"- [{name}](https://polyhaven.com/a/{name}), CC0 1.0."
                    for name in (*PROPS, *SURFACES.values())) + "\n",
        encoding="utf-8")
    (PROJECT / "LEIA-ME.md").write_text(
        "# Linha Fantasma\n\n"
        "Explore a estação em primeira pessoa, recolha três fusíveis, ligue o painel elétrico "
        "e atravesse a catraca antes da partida do último trem. A ação Interagir também "
        "empurra caixas físicas; mirando para um espaço vazio, ela liga ou desliga a lanterna.\n\n"
        "Arraste esquerdo para mover, arraste direito para olhar, Saltar para pular. "
        "Mire pelo retículo e toque Interagir. Na primeira abertura, use Código > Recompilar projeto, "
        "depois Play; Stop restaura a cena.\n\n"
        "O cenário é um GLB CC0 real, otimizado em oito peças editáveis por material para Android. "
        "As colisões, objetivos, luzes, física, câmera e script permanecem editáveis. "
        "O piso usa UV em metros e texturas PBR da Poly Haven, sem esticar um mapa por toda a estação. "
        "A fonte original está em Assets/station-source.glb. Não há áudio, animação esquelética "
        "ou promessa de fotorrealismo: a geometria original é simples e a qualidade final depende do aparelho.\n",
        encoding="utf-8")
    thumb = ROOT / "android/app/src/main/assets/astra/thumbs/example-linha-fantasma.png"
    thumb.parent.mkdir(parents=True, exist_ok=True)
    def pixel(x, y):
        depth = abs(x - 240) / (82 + y * .48)
        arch = depth > 1
        track = y > 205 and abs(x - 240) > 120 - (y - 205) * .2
        light = math.exp(-(((x - 240) / 115) ** 2 + ((y - 73) / 32) ** 2))
        return (33 + arch * 32 + track * 35 + light * 90,
                43 + arch * 39 + track * 12 + light * 69,
                55 + arch * 46 + track * 4 + light * 54)
    thumb.write_bytes(high.png_rgb(480, 320, pixel))
    print(f"Linha Fantasma: {len(scene.entities)} entidades, {len(optimized)} bytes no GLB otimizado")


if __name__ == "__main__":
    source, textures = fetch_assets()
    optimized = optimize_station(source, textures)
    write_project(make_scene(), source, optimized)
