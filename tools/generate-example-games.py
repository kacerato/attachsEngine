"""Author the three bundled Astra sample projects. Run to refresh packaged assets."""

from __future__ import annotations

import hashlib
import json
import math
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "android/app/src/main/assets/astra/example-projects"
INPUT = 'INPUT 3 "Mover" "Olhar" "Saltar" "Mover" 2 0.119999997 1 "" 2 1 0 0 0 1 0 1 0 0 1 1 0 "Olhar" 2 0 1 "" 2 2 0 0 0 1 0 2 0 0 1 1 0 "Saltar" 0 0 1 "" 1 3 0 0 0 1 0'


def guid(seed: str) -> str:
    return hashlib.sha256(seed.encode("utf-8")).hexdigest()[:32]


def quoted(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def f(value: float | int) -> str:
    return format(float(value), ".9g")


def mesh_component(asset: str, color: tuple[float, float, float], rough=.55, metallic=0, emission=(0, 0, 0), strength=1):
    numbers = [*color, rough, metallic, 1, 1, *emission, strength]
    return ("astra.render.mesh", 2, "1 1 1 " + " ".join(map(f, numbers)) + " " + asset)


def light_component(kind: int, color: tuple[float, float, float], intensity: float, radius=12):
    return ("astra.render.light", 1, " ".join(map(f, [kind, 1, *color, intensity, radius, 20, 35])))


def camera_component(orthographic=False, size=15, priority=10):
    return ("astra.camera", 2, " ".join(map(f, [1, 60, .1, 2000, priority, size, int(orthographic)])))


def script_component(name: str):
    return ("astra.script.behavior", 1, f'{quoted("project." + name)} {quoted("Scripts/" + name + ".cs")} 1 0')


def body_component(motion=0, mass=1):
    return ("astra.physics.body", 3, " ".join(map(f, [motion, mass, .65, .05, 0, 0, 0, 0, 0, 0, .05, .05, 1, 0, 1])))


def collider_component(size):
    return ("astra.physics.collider", 3, " ".join(map(f, [0, *size, .5, .5, 0, 0, 0, 0, 0, 0, 0, 1])))


class Scene:
    def __init__(self, source="Assets/kit.glb"):
        self.source = source
        source_guid = guid("fonte:" + source)
        self.meshes = {name: guid("glb:" + source_guid + ":" + name + "/" + name + "#0")
                       for name in ("Cube", "Slab", "Gem", "Pillar")}
        self.entities = []
        self.add("Cena", 0, 0)

    def add(self, name, parent=1, kind=0, pos=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1), components=(), static=False):
        entity_id = len(self.entities) + 1
        self.entities.append((entity_id, parent, kind, name, pos, rot, scale, tuple(components), static))
        return entity_id

    def shape(self, name, mesh, parent=1, pos=(0, 0, 0), scale=(1, 1, 1), color=(.8, .8, .8),
              rough=.55, metallic=0, emission=(0, 0, 0), strength=1, components=(), static=False):
        return self.add(name, parent, 1, pos, scale=scale, static=static, components=(
            mesh_component(self.meshes[mesh], color, rough, metallic, emission, strength), *components))

    def archive(self):
        lines = [f"AETHER_EDITOR 12 0 {len(self.entities)}"]
        for entity_id, parent, kind, name, pos, rot, scale, components, static in self.entities:
            values = [entity_id, parent, kind, quoted(name), *map(f, pos), *map(f, rot), *map(f, scale),
                      1, 1, 1, 1, int(static), 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, len(components), len(components) + 1]
            line = " ".join(map(str, values))
            for instance, (type_id, version, payload) in enumerate(components, 1):
                line += f" {instance} {quoted(type_id)} {version} {quoted(payload)}"
            lines.append(line)
        lines.append('LAYERS 1 "Padrão" 4294967295')
        lines.append(INPUT)
        return "\n".join(lines) + "\n"


def kit_glb() -> bytes:
    # Four tiny, reusable primitives. Geometry is authored once; scenes instance it.
    faces = [((1, 0, 0), [(1, -1, -1), (1, -1, 1), (1, 1, 1), (1, 1, -1)]),
             ((-1, 0, 0), [(-1, -1, 1), (-1, -1, -1), (-1, 1, -1), (-1, 1, 1)]),
             ((0, 1, 0), [(-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)]),
             ((0, -1, 0), [(-1, -1, 1), (1, -1, 1), (1, -1, -1), (-1, -1, -1)]),
             ((0, 0, 1), [(1, -1, 1), (-1, -1, 1), (-1, 1, 1), (1, 1, 1)]),
             ((0, 0, -1), [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1)])]
    cube_pos, cube_nrm, cube_idx = [], [], []
    for normal, quad in faces:
        start = len(cube_pos)
        cube_pos += quad
        cube_nrm += [normal] * 4
        cube_idx += [start, start + 1, start + 2, start, start + 2, start + 3]
    gem_pos = [(0, 1, 0), (1, 0, 0), (0, 0, 1), (-1, 0, 0), (0, 0, -1), (0, -1, 0)]
    gem_idx = [0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 1, 4, 5, 1, 2, 5, 2, 3, 5, 3, 4, 5, 4, 1]
    rings = 12
    pillar_pos = [(math.cos(2 * math.pi * j / rings), y, math.sin(2 * math.pi * j / rings))
                  for y in (-1, 1) for j in range(rings)]
    pillar_idx = []
    for j in range(rings):
        k = (j + 1) % rings
        pillar_idx += [j, k, rings + k, j, rings + k, rings + j]
        pillar_idx += [rings, rings + j, rings + k, 0, k, j]
    gem_normals = [(x / math.sqrt(x*x + y*y + z*z), y / math.sqrt(x*x + y*y + z*z),
                    z / math.sqrt(x*x + y*y + z*z)) for x, y, z in gem_pos]
    pillar_normals = [(x, 0, z) for x, _, z in pillar_pos]
    uv = [(0, 0), (1, 0), (1, 1), (0, 1)] * 6
    shapes = [("Cube", cube_pos, cube_nrm, cube_idx, uv),
              ("Slab", cube_pos, cube_nrm, cube_idx, [(u * 8, v * 8) for u, v in uv]),
              ("Gem", gem_pos, gem_normals, gem_idx, None),
              ("Pillar", pillar_pos, pillar_normals, pillar_idx, None)]
    data = bytearray()
    views, accessors, meshes, nodes = [], [], [], []

    def append(payload, target):
        while len(data) % 4:
            data.append(0)
        offset = len(data)
        data.extend(payload)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload), "target": target})
        return len(views) - 1

    for name, positions, normals, indices, coordinates in shapes:
        position_view = append(b"".join(struct.pack("<3f", *p) for p in positions), 34962)
        lo = [min(p[i] for p in positions) for i in range(3)]
        hi = [max(p[i] for p in positions) for i in range(3)]
        accessors.append({"bufferView": position_view, "componentType": 5126, "count": len(positions),
                          "type": "VEC3", "min": lo, "max": hi})
        position_accessor = len(accessors) - 1
        attributes = {"POSITION": position_accessor}
        if normals:
            normal_view = append(b"".join(struct.pack("<3f", *n) for n in normals), 34962)
            accessors.append({"bufferView": normal_view, "componentType": 5126, "count": len(normals), "type": "VEC3"})
            attributes["NORMAL"] = len(accessors) - 1
        if coordinates:
            uv_view = append(b"".join(struct.pack("<2f", *p) for p in coordinates), 34962)
            accessors.append({"bufferView": uv_view, "componentType": 5126, "count": len(coordinates), "type": "VEC2"})
            attributes["TEXCOORD_0"] = len(accessors) - 1
        index_view = append(b"".join(struct.pack("<H", i) for i in indices), 34963)
        accessors.append({"bufferView": index_view, "componentType": 5123, "count": len(indices), "type": "SCALAR"})
        meshes.append({"name": name, "primitives": [{"attributes": attributes, "indices": len(accessors) - 1,
                                                     "material": 1 if name == "Slab" else 0}]})
        nodes.append({"name": name, "mesh": len(meshes) - 1})
    texture_pixels = bytearray()
    for y in range(32):
        texture_pixels.append(0)
        for x in range(32):
            shade = 190 if (x // 8 + y // 8) % 2 else 245
            texture_pixels.extend((shade, shade, shade))

    def png_chunk(code, content):
        return struct.pack(">I", len(content)) + code + content + struct.pack(">I", zlib.crc32(code + content))

    png = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 32, 32, 8, 2, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(texture_pixels, 9)) + png_chunk(b"IEND", b""))
    image_view = append(png, None)
    views[image_view].pop("target")
    document = {"asset": {"version": "2.0", "generator": "Astra example games"}, "buffers": [{"byteLength": len(data)}],
                "bufferViews": views, "accessors": accessors,
                "images": [{"bufferView": image_view, "mimeType": "image/png"}],
                "textures": [{"source": 0, "sampler": 0}],
                "samplers": [{"wrapS": 10497, "wrapT": 10497, "magFilter": 9729, "minFilter": 9987}],
                "materials": [{"name": "Base"}, {"name": "Piso", "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0}, "roughnessFactor": .86, "metallicFactor": 0}}],
                "meshes": meshes, "nodes": nodes, "scenes": [{"nodes": list(range(len(nodes)))}], "scene": 0}
    json_bytes = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * (-len(json_bytes) % 4)
    data.extend(b"\0" * (-len(data) % 4))
    total = 12 + 8 + len(json_bytes) + 8 + len(data)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(json_bytes), b"JSON") + json_bytes
            + struct.pack("<I4s", len(data), b"BIN\0") + data)


def maze():
    s = Scene()
    world = s.add("Mundo")
    crystals = s.add("Cristais", world)
    s.shape("Ilha de pedra", "Slab", world, (0, -.35, 0), (16, .3, 16), (.16, .21, .28), static=True,
            components=(body_component(), collider_component((1, 1, 1))))
    for i in range(-3, 4):
        for j in range(-3, 4):
            if (i + j) % 3 == 0 and (i, j) != (0, 0):
                s.shape(f"Pilar {i} {j}", "Pillar", world, (i * 3.5, 1, j * 3.5), (.45, 1, .45), (.25, .34, .43), static=True)
    for i, (x, z) in enumerate([(-10, -9), (8, -10), (10, 8), (-8, 9), (0, 11), (11, 0)], 1):
        s.shape(f"Cristal {i}", "Gem", crystals, (x, 1, z), (.7, 1, .7), (.08, .78, 1), .2, .2,
                (.02, .55, 1), 5)
    s.shape("Portal", "Pillar", world, (0, 2, 13), (1.4, 2, 1.4), (.92, .28, .18), .25, .4, (.8, .12, .04), 3)
    s.shape("Explorador", "Cube", world, (0, .6, 0), (.55, .55, .55), (1, .72, .15), .35,
            components=(script_component("ExploradorCristais"),))
    s.add("Câmera tática", world, 3, (0, 30, -6), (75, 0, 0), components=(camera_component(True, 19),))
    s.add("Luz do sol", world, 2, (0, 18, 0), (50, 25, 0), components=(light_component(0, (1, .9, .72), 1.7),))
    for i, (x, z) in enumerate([(-10, -9), (8, -10), (10, 8), (-8, 9)]):
        s.add(f"Luz dos cristais {i+1}", world, 2, (x, 3, z), components=(light_component(1, (.1, .65, 1), 24, 7),))
    return s


def race():
    s = Scene()
    world = s.add("Mundo")
    gates = s.add("Portais", world)
    s.shape("Pista", "Slab", world, (0, -.4, 0), (14, .3, 38), (.13, .13, .17), .95, static=True,
            components=(body_component(), collider_component((1, 1, 1))))
    for side in (-1, 1):
        for i in range(26):
            s.shape(f"Barreira {side} {i}", "Cube", world, (side * 13, .6, -35 + i * 2.8), (.3, .6, 1.1),
                    (.1, .22, .32) if i % 2 else (1, .3, .08), static=True)
    for i, z in enumerate((-25, -10, 5, 20, 33), 1):
        s.shape(f"Portal {i}", "Gem", gates, ((-1) ** i * 3, 2, z), (1.3, 1.5, 1.3),
                (.1, 1, .42), .2, .2, (0, .7, .2), 4)
    for i in range(12):
        s.shape(f"Caixa física {i+1}", "Cube", world, ((i % 3 - 1) * 5, 3 + i % 2, -28 + i * 5),
                (.55, .55, .55), (.65, .68, .74), .32, .7,
                components=(body_component(2, 3), collider_component((1, 1, 1))))
    s.shape("Corredor", "Cube", world, (0, .7, -33), (.75, .45, 1.3), (.95, .18, .08), .28, .6,
            components=(script_component("PilotoCircuito"),))
    s.add("Câmera de corrida", world, 3, (0, 14, -40), (35, 0, 0), components=(camera_component(False, 15), script_component("CameraCircuito")))
    s.add("Sol", world, 2, (0, 20, 0), (55, -25, 0), components=(light_component(0, (1, .96, .83), 1.8),))
    for z in (-25, 5, 33):
        s.add(f"Farol {z}", world, 2, (0, 4, z), components=(light_component(1, (.25, 1, .42), 35, 12),))
    return s


def arena():
    s = Scene()
    world = s.add("Mundo")
    enemies = s.add("Drones", world)
    s.shape("Arena", "Slab", world, (0, -.4, 0), (24, .3, 24), (.18, .16, .28), .7, .2, static=True,
            components=(body_component(), collider_component((1, 1, 1))))
    # Hundreds of distinct scene objects exercise hierarchy, instancing,
    # picking, visibility, materials and mobile draw scheduling together.
    for ring in (7, 10, 13, 16, 19, 22):
        for n in range(48):
            a = n * 2 * math.pi / 48
            x, z = math.sin(a) * ring, math.cos(a) * ring
            s.shape(f"Torre {ring} {n}", "Pillar", world, (x, .75, z), (.35, .75, .35),
                    (.2, .27, .42) if n % 3 else (.7, .15, .56), .4, .4, static=True)
    for n in range(48):
        a = n * 2 * math.pi / 48
        radius = 11 + (n % 4) * 2.2
        s.shape(f"Drone {n+1}", "Gem", enemies, (math.sin(a) * radius, .9, math.cos(a) * radius),
                (.55, .55, .55), (.92, .12, .36), .25, .45, (.5, 0, .08), 2,
                components=(script_component("DroneArena"),))
    for n in range(16):
        a = n * 2 * math.pi / 16
        s.shape(f"Destroço físico {n+1}", "Cube", world,
                (math.sin(a) * 8, 3 + n % 3, math.cos(a) * 8), (.4, .4, .4),
                (.36, .42, .55), .6, .35,
                components=(body_component(2, 1.5), collider_component((1, 1, 1))))
    s.shape("Defensor", "Cube", world, (0, .7, 0), (.65, .65, .65), (.16, .65, 1), .3, .35,
            components=(script_component("DefensorArena"),))
    s.add("Câmera da arena", world, 3, (0, 38, -4), (82, 0, 0), components=(camera_component(True, 27),))
    s.add("Sol violeta", world, 2, (0, 20, 0), (55, 25, 0), components=(light_component(0, (.76, .7, 1), 1.5),))
    for n in range(8):
        a = n * math.pi / 4
        s.add(f"Luz {n}", world, 2, (math.sin(a) * 15, 3, math.cos(a) * 15),
              components=(light_component(1, (.8, .15, .65), 32, 12),))
    return s


SCRIPTS = {
    "cristais": {"ExploradorCristais.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.ExploradorCristais")]
public sealed class ExploradorCristais : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 7;
    private GameObject? _cristais;
    private GameObject? _portal;
    private int _total;
    private Vector3 _ultimoMovimento = Vector3.UnitZ;

    public override void Start()
    {
        _cristais = Object.Parent?.Find("Cristais");
        _portal = Object.Parent?.Find("Portal");
        Scene.Log(ObjectId, "CRISTAIS: mova com o controle esquerdo; Ação dá um impulso. Colete seis cristais e chegue ao portal.");
    }

    public override void Update(float dt)
    {
        var move = Input.Move;
        var direction = new Vector3(move.X, 0, move.Y);
        if (direction.LengthSquared() > 1) direction = Vector3.Normalize(direction);
        if (direction.LengthSquared() > 0) _ultimoMovimento = direction;
        var p = Object.Position + direction * Velocidade * dt;
        if (Input.JumpPressed) { p += _ultimoMovimento * 3; Scene.Log(ObjectId, "Impulso"); }
        Object.Position = new Vector3(Math.Clamp(p.X, -14, 14), .6f, Math.Clamp(p.Z, -14, 14));
        if (_cristais is not { IsAlive: true }) return;
        foreach (var item in _cristais.Children())
        {
            if (!item.ActiveInHierarchy || Vector3.DistanceSquared(item.Position, Object.Position) > 2.2f) continue;
            item.SetActive(false);
            Scene.Log(ObjectId, "Cristal " + ++_total + "/6");
        }
        if (_total == 6 && _portal is { IsAlive: true } portal &&
            Vector3.DistanceSquared(portal.Position, Object.Position) < 5)
        {
            _total = 7;
            Scene.Log(ObjectId, "VITÓRIA: todos os cristais chegaram ao portal.");
        }
    }
}
'''},
    "circuito": {"PilotoCircuito.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.PilotoCircuito")]
public sealed class PilotoCircuito : Behavior
{
    [PropertyId("aceleracao")] public float Aceleracao = 12;
    [PropertyId("velocidade_maxima")] public float VelocidadeMaxima = 19;
    [PropertyId("direcao")] public float Direcao = 9;
    private GameObject? _portais;
    private int _proximo;
    private float _velocidade;

    public override void Start()
    {
        _portais = Object.Parent?.Find("Portais");
        Scene.Log(ObjectId, "CIRCUITO: eixo vertical acelera, horizontal desvia e Ação ativa turbo; atravesse cinco portais.");
    }

    public override void Update(float dt)
    {
        var axis = Input.Move;
        _velocidade = Math.Clamp(_velocidade + axis.Y * Aceleracao * dt, -7, VelocidadeMaxima);
        _velocidade *= MathF.Max(0, 1 - .35f * dt);
        if (Input.JumpPressed) { _velocidade = MathF.Min(VelocidadeMaxima, _velocidade + 8); Scene.Log(ObjectId, "Turbo"); }
        var p = Object.Position;
        p.X = Math.Clamp(p.X + axis.X * Direcao * dt, -11, 11);
        p.Z = Math.Clamp(p.Z + _velocidade * dt, -35, 35);
        Object.Position = p;
        if (_portais is not { IsAlive: true } || _proximo >= _portais.ChildCount) return;
        var portal = _portais.ChildAt(_proximo);
        if (Vector3.DistanceSquared(p, portal.Position) > 12) return;
        portal.SetActive(false);
        Scene.Log(ObjectId, "Checkpoint " + ++_proximo + "/5");
        if (_proximo == 5) Scene.Log(ObjectId, "VITÓRIA: circuito concluído.");
    }
}
''', "CameraCircuito.cs": '''using Astra;
using System.Numerics;

[ComponentId("project.CameraCircuito")]
public sealed class CameraCircuito : Behavior
{
    private GameObject? _piloto;
    public override void Start() => _piloto = Object.Parent?.Find("Corredor");
    public override void Update(float dt)
    {
        if (_piloto is not { IsAlive: true }) return;
        var pos = _piloto.Position;
        Object.Position = new Vector3(pos.X, 13, pos.Z - 12);
    }
}
'''},
    "arena": {"DefensorArena.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.DefensorArena")]
public sealed class DefensorArena : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 8;
    [PropertyId("alcance")] public float Alcance = 12;
    private GameObject? _drones;
    private int _abates;
    private float _vida = 100;
    private float _invulneravel;

    public override void Start()
    {
        _drones = Object.Parent?.Find("Drones");
        Scene.Log(ObjectId, "ARENA: mova e toque Ação para eliminar até quatro drones próximos por pulso.");
    }

    public override void Update(float dt)
    {
        if (_vida <= 0 || _abates == 48) return;
        var axis = Input.Move;
        var direction = new Vector3(axis.X, 0, axis.Y);
        if (direction.LengthSquared() > 1) direction = Vector3.Normalize(direction);
        var p = Object.Position + direction * Velocidade * dt;
        Object.Position = new Vector3(Math.Clamp(p.X, -22, 22), .7f, Math.Clamp(p.Z, -22, 22));
        _invulneravel = MathF.Max(0, _invulneravel - dt);
        if (_drones is not { IsAlive: true }) return;
        foreach (var drone in _drones.Children())
        {
            if (!drone.ActiveInHierarchy) continue;
            var d = Vector3.DistanceSquared(drone.Position, Object.Position);
            if (d < 1.6f && _invulneravel == 0)
            {
                _vida = MathF.Max(0, _vida - 5); _invulneravel = 1.5f;
                Scene.Log(ObjectId, "Vida " + _vida);
                if (_vida <= 0) { Scene.Log(ObjectId, "DERROTA: reinicie Play."); return; }
            }
        }
        if (!Input.JumpPressed) return;
        for (var hit = 0; hit < 4 && _abates < 48; hit++)
        {
            GameObject? closest = null;
            var distance = Alcance * Alcance;
            foreach (var drone in _drones.Children())
            {
                if (!drone.ActiveInHierarchy) continue;
                var d = Vector3.DistanceSquared(drone.Position, Object.Position);
                if (d < distance) { closest = drone; distance = d; }
            }
            if (closest is null) break;
            closest.SetActive(false);
            ++_abates;
        }
        Scene.Log(ObjectId, "Drones destruídos " + _abates + "/48");
        if (_abates == 48) Scene.Log(ObjectId, "VITÓRIA: arena limpa.");
    }
}
''', "DroneArena.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.DroneArena")]
public sealed class DroneArena : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 1.6f;
    private GameObject? _alvo;
    public override void Start() => _alvo = Object.Parent?.Parent?.Find("Defensor");
    public override void Update(float dt)
    {
        if (_alvo is not { IsAlive: true }) return;
        var target = _alvo.Position - Object.Position;
        target.Y = 0;
        if (target.LengthSquared() < .01f) return;
        var p = Object.Position + Vector3.Normalize(target) * Velocidade * dt;
        Object.Position = new Vector3(p.X, .9f, p.Z);
    }
}
'''},
}


def write_project(slug: str, title: str, scene: Scene, glb: bytes):
    folder = OUT / slug
    (folder / "Assets").mkdir(parents=True, exist_ok=True)
    (folder / "Scripts").mkdir(exist_ok=True)
    (folder / "scenes").mkdir(exist_ok=True)
    (folder / "Assets/kit.glb").write_bytes(glb)
    (folder / "scenes/editor.aescene").write_text(scene.archive(), encoding="utf-8")
    (folder / "scenes/main.ascene").write_text(json.dumps({"format": "ASTRA-SCENE-1", "template": "empty", "nodes": []}) + "\n", encoding="utf-8")
    source = "Assets/kit.glb"
    source_guid = guid("fonte:" + source)
    asset = f'AETHER_ASSETS 1 1\n{source_guid} mesh "{source}" "{source}" "{hashlib.sha256(glb).hexdigest()}" 1 "glb" 0 0\n'
    # Android's asset packager may omit dot-directories. The installer moves
    # this file to .astra/assets.astra inside the editable project.
    (folder / "assets.astra").write_text(asset, encoding="utf-8")
    for filename, content in SCRIPTS[slug].items():
        (folder / "Scripts" / filename).write_text(content, encoding="utf-8")
    directions = {
        "cristais": "Colete os seis cristais e alcance o portal. Use o eixo esquerdo para mover; Ação dá um impulso.",
        "circuito": "Acelere, desvie e passe por cinco portais em ordem. Eixo vertical acelera; horizontal esterça; Ação ativa turbo.",
        "arena": "Elimine 48 drones antes de perder a vida. Eixo esquerdo move; Ação elimina até quatro drones próximos por pulso.",
    }
    (folder / "LEIA-ME.md").write_text(
        f"# {title}\n\n{directions[slug]}\n\nAbra Código e toque Aplicar na primeira abertura; depois toque Play. "
        "O console mostra o progresso. Stop restaura a cena para uma nova partida.\n",
        encoding="utf-8")
    print(f"{title}: {len(scene.entities)} entidades, {len(SCRIPTS[slug])} scripts")


def write_thumbnail(slug: str):
    width, height = 480, 320
    pixels = bytearray()
    for y in range(height):
        pixels.append(0)
        for x in range(width):
            dx, dy = x - width / 2, y - height / 2
            if slug == "cristais":
                glow = sum(max(0, 1 - math.hypot(x - cx, y - cy) / 55) for cx, cy in
                           ((95, 90), (360, 72), (310, 245), (120, 235), (240, 155)))
                grid = int(x % 44 < 2 or y % 44 < 2)
                rgb = (int(13 + 12 * grid), int(26 + 72 * glow + 15 * grid), int(43 + 100 * glow + 20 * grid))
            elif slug == "circuito":
                road = abs(dx) < 105 + .28 * dy
                edge = abs(abs(dx) - (105 + .28 * dy)) < 5
                dash = abs(dx) < 3 and y % 55 < 29
                gate = 52 < y < 64 or 192 < y < 205
                rgb = ((22, 25, 35) if road else (8, 20, 30))
                if edge or (gate and road): rgb = (15, 220, 165)
                if dash: rgb = (255, 130, 38)
            else:
                radius = math.hypot(dx, dy)
                ring = min(abs(radius - 55), abs(radius - 105), abs(radius - 151)) < 3
                drone = any(math.hypot(x - (240 + math.sin(i * math.pi / 6) * 105),
                                       y - (160 + math.cos(i * math.pi / 6) * 105)) < 8 for i in range(12))
                rgb = (20, 15, 39)
                if ring: rgb = (68, 54, 110)
                if drone: rgb = (235, 46, 134)
                if radius < 14: rgb = (43, 180, 245)
            pixels.extend(max(0, min(255, c)) for c in rgb)

    def chunk(code, data):
        return struct.pack(">I", len(data)) + code + data + struct.pack(">I", zlib.crc32(code + data))

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))
    folder = ROOT / "android/app/src/main/assets/astra/thumbs"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"example-{slug}.png").write_bytes(png)


if __name__ == "__main__":
    glb = kit_glb()
    write_project("cristais", "Cristais do Templo", maze(), glb)
    write_project("circuito", "Circuito Neon", race(), glb)
    write_project("arena", "Arena de Drones", arena(), glb)
    for game in ("cristais", "circuito", "arena"):
        write_thumbnail(game)
