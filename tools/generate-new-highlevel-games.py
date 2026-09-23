"""Generate three genuinely new Astra games without replacing earlier examples."""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("astra_highlevel_kit", ROOT / "tools/generate-highlevel-games.py")
assert _spec and _spec.loader
kit = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(kit)
base = kit.base
Scene = kit.Scene
shape = kit.shape
light = kit.light
player = kit.player
npc = kit.npc
real_prop = kit.real_prop
collision_proxy = kit.collision_proxy
TEXTURES = ROOT / "tools/assets/new-game-materials"


NEW_MODELS = {
    "mercado-nexus": ("plastic_crate_01", "wicker_basket_01", "CoffeeCart_01"),
    "farol-abissal": ("propane_tank", "modular_industrial_pipes_01", "Lantern_01"),
    "expresso-tita": ("vintage_suitcase", "industrial_storage_cart", "metal_tool_chest"),
}
kit.REAL_MODELS.update(NEW_MODELS)


def action(scene: Scene, name: str) -> None:
    original = scene.archive
    scene.archive = lambda: original().replace(base.INPUT, kit.input_archive(name))


def mercado_nexus() -> Scene:
    scene = Scene()
    world = scene.add("Mercado Nexus")
    architecture = scene.add("Arquitetura do bazar", world)
    stalls = scene.add("Bancas autônomas", world)
    cargo = scene.add("Mercadorias físicas", world)
    agents = scene.add("Agentes do mercado", world)
    lighting = scene.add("Iluminação neon", world)
    shape(scene, "Praça hexagonal", "Ground", architecture, (0, -.22, 0), (15, .22, 28), solid=True)
    for side in (-1, 1):
        shape(scene, f"Muralha comercial {side}", "Steel", architecture,
              (side * 15.3, 2.2, 0), (.3, 2.2, 28), solid=True)
        for z in range(-24, 27, 6):
            shape(scene, f"Coluna luminosa {side} {z}", "Steel", architecture,
                  (side * 14.2, 2.35, z), (.22, 2.35, .22), (.2, .24, .29), .42, .72)
            shape(scene, f"Faixa neon {side} {z}", "Lamp", lighting,
                  (side * 13.95, 2.7, z), (.05, .09, 1.8),
                  (.12, .9 if side > 0 else .24, 1 if side > 0 else .86), .18, .05,
                  (.04, .7 if side > 0 else .13, 1 if side > 0 else .72), 5)
            light(scene, f"Luz de corredor {side} {z}", lighting,
                  (side * 12.6, 3.5, z), (.18, .72, 1) if side > 0 else (1, .12, .55), 12, 8)
    vendor_specs = (
        ("Âmbar", -8.2, (1, .58, .12), (1, .24, .04)),
        ("Ciano", 0, (.1, .88, 1), (.02, .55, 1)),
        ("Magenta", 8.2, (1, .18, .7), (.82, .04, .44)),
    )
    for label, x, color, glow in vendor_specs:
        booth = scene.add(f"Banca {label}", stalls)
        shape(scene, f"Balcão {label}", "Steel", booth, (x, .85, 13), (2.5, .85, 1.0),
              (.22, .25, .3), .48, .68, solid=True)
        shape(scene, f"Cobertura {label}", "Timber", booth, (x, 3.2, 13.3), (2.8, .1, 1.65),
              color, .88, 0)
        for post in (-2.25, 2.25):
            shape(scene, f"Poste {label} {post}", "Pipe", booth, (x + post, 2, 13.8),
                  (.08, 1.4, .08), (.28, .31, .36), .4, .75)
        shape(scene, f"Totem {label}", "Lamp", booth, (x, 2.0, 11.92), (.55, .62, .08),
              color, .2, .1, glow, 4)
        light(scene, f"Luz da banca {label}", lighting, (x, 3.0, 12), color, 15, 7)
        npc(scene, f"Mercador {label}", agents, (x, 1.0, 10.6), "VendorAgent",
            (.2, .24, .29), color)
    for row, z in enumerate((-16, -10)):
        for column, (label, x, color, _) in enumerate(vendor_specs):
            crate_x = x + (-1.4 if row == 0 else 1.4)
            shape(scene, f"Carga {label} {row + 1}", "Steel", cargo,
                  (crate_x, 1.0, z + column * 1.25), (.48, .48, .48), color,
                  .58, .32, color, .35, dynamic=True, uv_world=False)
    for index in range(24):
        x = (-12.2 if index % 2 else 12.2) + ((index % 3) - 1) * .45
        z = -22 + (index // 2) * 4
        shape(scene, f"Expositor {index + 1}", "Timber", stalls, (x, .65, z),
              (.8, .65, 1.2), (.16, .5, .52) if index % 2 else (.6, .16, .46), .82)
        shape(scene, f"Produto luminoso {index + 1}", "Lamp", stalls, (x, 1.5, z),
              (.28, .28, .28), (.15, .8, 1) if index % 2 else (1, .2, .68),
              .22, .05, (.08, .5, .9) if index % 2 else (.8, .05, .4), 2.5)
    for index, pos in enumerate(((-10.5, .25, -7), (10.8, .25, 2)), 1):
        prop = real_prop(scene, "mercado-nexus", "plastic_crate_01",
                         f"Caixote comercial real {index}", cargo, pos, (1.35, 1.35, 1.35))
        collision_proxy(scene, prop, "Colisão do caixote comercial", (0, .25, 0), (.42, .25, .42))
    basket = real_prop(scene, "mercado-nexus", "wicker_basket_01", "Cesto artesanal real",
                       stalls, (-7.8, 1.75, 12.1), (1.15, 1.15, 1.15))
    collision_proxy(scene, basket, "Colisão do cesto", (0, .2, 0), (.35, .2, .35))
    cart = real_prop(scene, "mercado-nexus", "CoffeeCart_01", "Quiosque móvel real",
                     stalls, (11.3, .2, -12.5), (1.4, 1.4, 1.4))
    collision_proxy(scene, cart, "Colisão do quiosque", (0, .8, 0), (1.2, .8, .75))
    npc(scene, "NIX · drone de logística", agents, (0, 2.4, -18), "CourierGuide",
        (.16, .28, .34), (.08, .92, 1), drone=True)
    actor = player(scene, "Entregador", world, (0, 0, -23), "MarketRunner", 6.0, 5.5, 76)
    shape(scene, "Leitor de carga", "Steel", actor + 1, (.36, -.3, .72), (.11, .08, .26),
          (.2, .25, .3), .4, .75, (.02, .5, .8), 1.5, uv_world=False)
    light(scene, "Lua artificial", world, (0, 24, -10), (.46, .6, 1), .45, kind=0, rot=(55, 22, 0))
    action(scene, "Entregar")
    return scene


def farol_abissal() -> Scene:
    scene = Scene()
    world = scene.add("Farol Abissal")
    station = scene.add("Estação pressurizada", world)
    machinery = scene.add("Máquinas e tubulações", world)
    cells = scene.add("Células de pressão", world)
    agents = scene.add("Agentes abissais", world)
    emergency = scene.add("Iluminação de emergência", world)
    restored = scene.add("Iluminação restaurada", world)
    shape(scene, "Passarela central", "Ground", station, (0, -.22, 0), (7, .22, 28), solid=True)
    for side in (-1, 1):
        shape(scene, f"Casco de pressão {side}", "Steel", station,
              (side * 7.35, 2.3, 0), (.35, 2.3, 28), solid=True)
        for z in range(-24, 27, 6):
            shape(scene, f"Nervura {side} {z}", "Pipe", station,
                  (side * 6.75, 2.25, z), (.18, 2.25, .18), (.16, .4, .42), .55, .58)
            shape(scene, f"Visor abissal {side} {z}", "Lamp", station,
                  (side * 6.92, 2.25, z + 2.4), (.06, 1.35, 1.65),
                  (.02, .18, .27), .18, .05, (.01, .16, .28), 1.2)
            light(scene, f"Bioluminescência {side} {z}", restored,
                  (side * 5.8, 2.6, z), (.05, .65, 1), 10, 7)
    for z in (-18, -6, 6, 18):
        shape(scene, f"Anel estrutural {z}", "Pipe", station, (0, 4.2, z), (6.7, .18, .18),
              (.22, .47, .48), .55, .65, rot=(0, 0, 90))
        shape(scene, f"Faixa de pressão {z}", "Hazard", station, (0, .04, z), (2.3, .03, .18))
    module_positions = ((-4.4, -16), (4.4, -5), (-4.4, 7), (4.4, 18))
    for index, (x, z) in enumerate(module_positions, 1):
        module = shape(scene, f"Módulo de pressão {index}", "Steel", machinery,
                       (x, 1.35, z), (.9, 1.35, .72), (.13, .35, .38), .55, .6, solid=True)
        shape(scene, "Indicador vermelho", "Lamp", module, (-.35, .72, -.74), (.12, .12, .08),
              (1, .12, .04), .18, 0, (.9, .04, .01), 5)
        shape(scene, "Indicador azul", "Lamp", module, (.35, .72, -.74), (.12, .12, .08),
              (.05, .75, 1), .18, 0, (.02, .55, 1), 5)
        shape(scene, f"Célula de pressão {index}", "Pipe", cells,
              (-x * .62, 1.2, z - 5.0), (.3, .62, .3), (.92, .58, .12),
              .38, .72, (.9, .38, .04), 1.2, rot=(0, 0, 90), dynamic=True, uv_world=False)
        light(scene, f"Alarme {index}", emergency, (x, 3.4, z), (1, .13, .05), 12, 6)
    for index, (x, z) in enumerate(((-3.8, -9), (3.9, 2), (-3.4, 14)), 1):
        npc(scene, f"Predador abissal {index}", agents, (x, 1.8, z), "AbyssStalker",
            (.05, .18, .24), (.04, .86, 1), drone=True)
    npc(scene, "LUME · drone faroleiro", agents, (1.2, 2.2, -21), "BeaconDrone",
        (.72, .62, .28), (1, .68, .15), drone=True)
    tank = real_prop(scene, "farol-abissal", "propane_tank", "Reservatório auxiliar real",
                     machinery, (-5.4, .4, -21), (1.1, 1.1, 1.1))
    collision_proxy(scene, tank, "Colisão do reservatório", (0, .55, 0), (.32, .55, .32))
    pipes = real_prop(scene, "farol-abissal", "modular_industrial_pipes_01",
                      "Rede de tubulação real", machinery, (5.5, .6, 10), (.9, .9, .9))
    collision_proxy(scene, pipes, "Colisão da tubulação", (0, .55, 0), (1.25, .55, .55))
    lantern = real_prop(scene, "farol-abissal", "Lantern_01", "Lanterna de latão real",
                        machinery, (0, .85, 23), (.65, .65, .65))
    collision_proxy(scene, lantern, "Colisão da lanterna", (0, .3, 0), (.22, .3, .22))
    shape(scene, "Núcleo do farol", "Lamp", station, (0, 2.2, 25), (.8, 1.65, .8),
          (.8, .9, 1), .2, .05, (.12, .68, 1), 7)
    light(scene, "Feixe do farol", restored, (0, 4.0, 24), (.25, .76, 1), 30, 13)
    actor = player(scene, "Mergulhador", world, (0, 0, -24), "AbyssDiver", 5.0, 4.8, 73)
    shape(scene, "Ferramenta de pulso", "Steel", actor + 1, (.34, -.28, .72), (.12, .12, .3),
          (.12, .35, .4), .42, .7, (.02, .5, .7), 1.6, uv_world=False)
    light(scene, "Luz de profundidade", world, (0, 20, -8), (.05, .18, .34), .3, kind=0, rot=(60, 0, 0))
    action(scene, "Pulso")
    return scene


def expresso_tita() -> Scene:
    scene = Scene()
    world = scene.add("Expresso Titã")
    train = scene.add("Composição blindada", world)
    cargo = scene.add("Carga móvel", world)
    crew = scene.add("Tripulação", world)
    raiders = scene.add("Invasores", world)
    lighting = scene.add("Luzes do trem", world)
    shape(scene, "Piso longitudinal", "Ground", train, (0, -.22, 0), (6.2, .22, 32), solid=True)
    for side in (-1, 1):
        shape(scene, f"Blindagem lateral {side}", "Steel", train,
              (side * 6.45, 1.35, 0), (.25, 1.35, 32), (.18, .19, .21), .48, .76, solid=True)
        for z in range(-28, 31, 6):
            shape(scene, f"Rebite estrutural {side} {z}", "Pipe", train,
                  (side * 6.12, 1.6, z), (.12, 1.5, .12), (.42, .31, .18), .42, .7)
            shape(scene, f"Janela congelada {side} {z}", "Lamp", train,
                  (side * 6.2, 2.65, z + 2.4), (.04, .55, 1.45), (.12, .22, .34),
                  .22, .05, (.05, .15, .26), .7)
    for z in range(-27, 31, 6):
        shape(scene, f"Junção de vagão {z}", "Hazard", train, (0, .04, z), (5.7, .03, .14))
        light(scene, f"Lâmpada do vagão {z}", lighting, (0, 3.4, z), (1, .53, .16), 12, 7)
        shape(scene, f"Luminária {z}", "Lamp", lighting, (0, 3.6, z), (.48, .08, .18),
              (1, .67, .26), .2, 0, (1, .35, .06), 3)
    shape(scene, "Fornalha central", "Steel", train, (0, 1.45, 23), (1.65, 1.45, 1.0),
          (.22, .18, .12), .5, .72, solid=True)
    shape(scene, "Boca da fornalha", "Lamp", train, (0, 1.35, 21.94), (.8, .7, .08),
          (1, .25, .04), .2, 0, (1, .12, .01), 7)
    for index, z in enumerate((-8, 10), 1):
        lever = shape(scene, f"Alavanca de trilho {index}", "Steel", train,
                      (-4.8 if index == 1 else 4.8, 1.05, z), (.48, 1.05, .42),
                      (.28, .22, .16), .48, .7, solid=True)
        shape(scene, "Indicador vermelho", "Lamp", lever, (-.18, .55, -.44), (.1, .1, .06),
              (1, .1, .03), .18, 0, (.9, .03, .01), 4)
        shape(scene, "Indicador verde", "Lamp", lever, (.18, .55, -.44), (.1, .1, .06),
              (.08, 1, .35), .18, 0, (.02, .8, .12), 4)
    for index, (x, z) in enumerate(((-3.8, -24), (0, -20), (3.8, -16)), 1):
        shape(scene, f"Cápsula de carvão {index}", "Pipe", cargo, (x, 1.15, z), (.38, .65, .38),
              (.35, .23, .12), .48, .62, (1, .22, .03), .8, rot=(0, 0, 90),
              dynamic=True, extra=(base.script_component("ThrownCargo"),), uv_world=False)
    for index in range(10):
        x = (-1 if index % 2 else 1) * (2.1 + index % 3)
        z = -25 + index * 4.6
        shape(scene, f"Carga solta {index + 1}", "Rock", cargo, (x, 1.2, z),
              (.38 + (index % 2) * .12,) * 3, (.12, .11, .1), .92, .08,
              dynamic=True, extra=(base.script_component("ThrownCargo"),), uv_world=False)
    for index, (x, z) in enumerate(((-4.5, -4), (4.4, 5), (-3.8, 16), (3.9, 27), (-4.0, 25), (4.1, -12)), 1):
        npc(scene, f"Saqueador {index}", raiders, (x, 1.0, z), "RaiderAgent",
            (.18, .2, .23), (1, .23, .06))
    npc(scene, "CONDUTOR 7", crew, (0, 1.0, 18), "ConductorAgent",
        (.16, .22, .3), (1, .62, .14))
    suitcase = real_prop(scene, "expresso-tita", "vintage_suitcase", "Mala de passageiro real",
                         cargo, (-4.7, .4, -18), (.9, .9, .9))
    collision_proxy(scene, suitcase, "Colisão da mala", (0, .24, 0), (.42, .24, .26))
    cart = real_prop(scene, "expresso-tita", "industrial_storage_cart", "Carrinho industrial real",
                     cargo, (4.4, .25, 13), (1.15, 1.15, 1.15))
    collision_proxy(scene, cart, "Colisão do carrinho", (0, .55, 0), (.7, .55, .45))
    chest = real_prop(scene, "expresso-tita", "metal_tool_chest", "Baú de ferramentas real",
                      cargo, (-4.1, .45, 19), (.85, .85, .85))
    collision_proxy(scene, chest, "Colisão do baú", (0, .35, 0), (.55, .35, .32))
    actor = player(scene, "Foguista", world, (0, 0, -28), "TitanEngineer", 6.1, 5.4, 76)
    shape(scene, "Manopla magnética", "Steel", actor + 1, (.34, -.3, .72), (.13, .1, .31),
          (.25, .19, .12), .42, .72, (1, .2, .02), 1.2, uv_world=False)
    light(scene, "Lua sobre os trilhos", world, (0, 24, -15), (.42, .55, .82), .5,
          kind=0, rot=(58, -18, 0))
    action(scene, "Ação")
    return scene


SCRIPTS = {
    "mercado-nexus": {
        "MarketRunner.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.MarketRunner")]
public sealed class MarketRunner : Behavior
{
    [PropertyId("tempoLimite")] public float TempoLimite = 210;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.2f;
    private GameObject? _camera, _held;
    private float _time;
    private int _delivered;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos");
        _time = TempoLimite;
        Scene.Log(ObjectId, "MERCADO NEXUS · entregue 2 cargas para cada um dos 3 mercadores");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _time -= dt;
        if (_time <= 0) { _finished = true; Scene.Log(ObjectId, "MERCADO FECHADO · reinicie Play"); return; }
        if (!Input.JustPressed("Entregar") || _camera is not { IsAlive: true }) return;
        var target = Aim(_held, 5.0f);
        if (_held is { IsAlive: true } carried)
        {
            var vendor = FindBehavior<VendorAgent>(target);
            if (vendor?.Accept(carried.Name) == true)
            {
                carried.Destroy(); _held = null; _delivered++;
                if (_delivered == 6) { _finished = true; Scene.Log(ObjectId, "ROTA PERFEITA · 6/6 entregas · " + MathF.Ceiling(_time) + " s restantes"); }
                else Scene.Log(ObjectId, "ENTREGA ACEITA " + _delivered + "/6 · " + MathF.Ceiling(_time) + " s");
            }
            else
            {
                var pose = _camera.WorldTransform;
                var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
                Scene.SetBodyVelocity(carried.ObjectId, forward * 6 + Vector3.UnitY);
                _held = null; Scene.Log(ObjectId, "CARGA RECUSADA OU SOLTA · confira a cor do mercador");
            }
            return;
        }
        if (target?.Name.StartsWith("Carga ", StringComparison.Ordinal) == true)
        { _held = target; Scene.Log(ObjectId, "SEGURANDO " + target.Name + " · leve ao mercador da mesma cor"); }
        else Scene.Log(ObjectId, "Mire em uma carga física");
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 49) { _held = null; return; }
        var velocity = delta * 13 - Scene.GetBodyVelocity(carried.ObjectId) * 2.7f;
        if (velocity.LengthSquared() > 169) velocity = Vector3.Normalize(velocity) * 13;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    private GameObject? Aim(GameObject? ignored, float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * range, out _, QueryFilter.Default.Ignoring(Object), 16);
        foreach (var hit in hits) if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }
}
''',
        "VendorAgent.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.VendorAgent")]
public sealed class VendorAgent : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 1.35f;
    private Vector3 _home;
    private float _phase;
    private int _received;

    public override void Start() { _home = Object.WorldTransform.Position; _phase = (ObjectId % 9) * .6f; }

    public bool Accept(string cargo)
    {
        var tag = Object.Name.Replace("Mercador ", "", StringComparison.Ordinal);
        if (_received >= 2 || !cargo.Contains(tag, StringComparison.Ordinal)) return false;
        _received++; return true;
    }

    public override void FixedUpdate(float dt)
    {
        _phase += dt * .65f;
        var current = Object.WorldTransform.Position;
        var destination = _home + new Vector3(MathF.Sin(_phase) * 1.1f, 0, MathF.Cos(_phase * .73f) * .55f);
        var travel = destination - current; travel.Y = 0;
        if (travel.LengthSquared() < .02f) return;
        var direction = Vector3.Normalize(travel);
        var step = direction * MathF.Min(Velocidade * dt, travel.Length());
        var hit = Physics.ShapeCast(ShapeQuery.Capsule(.3f, .7f), current, step, QueryFilter.Default.Ignoring(Object));
        if (hit is not null) step = Vector3.Cross(Vector3.UnitY, direction) * Velocidade * dt;
        var yaw = MathF.Atan2(direction.X, direction.Z);
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, yaw));
    }
}
''',
        "CourierGuide.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.CourierGuide")]
public sealed class CourierGuide : Behavior
{
    private GameObject? _player;
    private float _phase;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Entregador"); }
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt;
        var current = Object.WorldTransform.Position;
        var destination = _player.WorldTransform.Position + new Vector3(MathF.Sin(_phase) * 1.5f, 2.1f, 1.4f);
        var delta = destination - current;
        if (delta.LengthSquared() < .08f) return;
        var direction = Vector3.Normalize(delta);
        var step = direction * MathF.Min(3.8f * dt, delta.Length());
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
    }
}
'''
    },
    "farol-abissal": {
        "AbyssDiver.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.AbyssDiver")]
public sealed class AbyssDiver : Behavior
{
    [PropertyId("pressaoInicial")] public float PressaoInicial = 100;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.1f;
    private GameObject? _camera, _held, _emergency, _restored;
    private float _pressure, _pulse;
    private int _modules;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos"); _pressure = PressaoInicial;
        var world = Object.Parent;
        _emergency = world?.Find("Iluminação de emergência");
        _restored = world?.Find("Iluminação restaurada"); _restored?.SetActive(false);
        for (var index = 1; index <= 4; index++)
            world?.Find("Módulo de pressão " + index)?.Find("Indicador azul")?.SetActive(false);
        Scene.Log(ObjectId, "FAROL ABISSAL · instale 4 células · PULSO atordoa predadores");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _pulse = MathF.Max(0, _pulse - dt);
        if (_pressure <= 0) { _finished = true; Scene.Log(ObjectId, "PRESSÃO CRÍTICA · reinicie Play"); return; }
        if (_modules == 4 && Object.Position.Z > 22) { _finished = true; Scene.Log(ObjectId, "FAROL REATIVADO · pressão " + MathF.Ceiling(_pressure)); return; }
        if (!Input.JustPressed("Pulso") || _camera is not { IsAlive: true }) return;
        var target = Aim(_held, _held is null ? 11 : 4.5f);
        if (_held is { IsAlive: true } carried)
        {
            if (target?.Name.StartsWith("Módulo de pressão ", StringComparison.Ordinal) == true &&
                target.Find("Indicador azul")?.ActiveInHierarchy == false)
            {
                target.Find("Indicador vermelho")?.SetActive(false); target.Find("Indicador azul")?.SetActive(true);
                carried.Destroy(); _held = null; _modules++;
                if (_modules == 4) { _emergency?.SetActive(false); _restored?.SetActive(true); Scene.Log(ObjectId, "PRESSÃO ESTÁVEL · alcance o núcleo do farol"); }
                else Scene.Log(ObjectId, "MÓDULOS " + _modules + "/4 · pressão " + MathF.Ceiling(_pressure));
            }
            else { Scene.SetBodyVelocity(carried.ObjectId, Vector3.Zero); _held = null; Scene.Log(ObjectId, "CÉLULA SOLTA"); }
            return;
        }
        if (target?.Name.StartsWith("Célula de pressão ", StringComparison.Ordinal) == true)
        { _held = target; Scene.Log(ObjectId, "CÉLULA ACOPLADA À MANOPLA · leve a um módulo vermelho"); return; }
        var stalker = FindBehavior<AbyssStalker>(target);
        if (stalker != null && _pulse <= 0) { stalker.Stun(); _pulse = 2.2f; Scene.Log(ObjectId, "PULSO ELETROMAGNÉTICO · predador atordoado"); }
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

    public void TakePressure(float amount)
    {
        if (_finished) return; _pressure = MathF.Max(0, _pressure - amount);
        Scene.Log(ObjectId, "IMPACTO ABISSAL · pressão " + MathF.Ceiling(_pressure) + " · módulos " + _modules + "/4");
    }

    private GameObject? Aim(GameObject? ignored, float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform; var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * range, out _, QueryFilter.Default.Ignoring(Object), 16);
        foreach (var hit in hits) if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }
}
''',
        "AbyssStalker.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.AbyssStalker")]
public sealed class AbyssStalker : Behavior
{
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _attack, _stun;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Mergulhador"); _home = Object.WorldTransform.Position; _phase = (ObjectId % 11) * .43f; }
    public void Stun() => _stun = 4.5f;
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt; _attack = MathF.Max(0, _attack - dt); _stun = MathF.Max(0, _stun - dt);
        if (_stun > 0) return;
        var current = Object.WorldTransform.Position; var player = _player.WorldTransform.Position;
        var delta = player - current; var distance = delta.Length();
        var destination = distance < 13 ? player + new Vector3(MathF.Sin(_phase) * 1.8f, 1.2f, 0)
                                        : _home + new Vector3(MathF.Sin(_phase) * 2.4f, MathF.Cos(_phase * .8f) * .6f, MathF.Cos(_phase) * 2.1f);
        var travel = destination - current;
        if (travel.LengthSquared() > .05f)
        {
            var direction = Vector3.Normalize(travel); var step = direction * MathF.Min(2.65f * dt, travel.Length());
            var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.36f), current, step, QueryFilter.Default.Ignoring(Object));
            if (obstacle is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
                step = Vector3.Cross(Vector3.UnitY, direction) * 2.2f * dt;
            Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
        }
        if (distance < 2.2f && _attack <= 0) { _attack = 1.4f; FindBehavior<AbyssDiver>(_player)?.TakePressure(9); }
    }
}
''',
        "BeaconDrone.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.BeaconDrone")]
public sealed class BeaconDrone : Behavior
{
    private GameObject? _player;
    private float _phase;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Mergulhador"); }
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return; _phase += dt;
        var current = Object.WorldTransform.Position;
        var destination = _player.WorldTransform.Position + new Vector3(MathF.Sin(_phase * .8f) * 1.2f, 2.0f, 1.0f);
        var delta = destination - current; if (delta.LengthSquared() < .05f) return;
        var direction = Vector3.Normalize(delta); var step = direction * MathF.Min(3.2f * dt, delta.Length());
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
    }
}
'''
    },
    "expresso-tita": {
        "TitanEngineer.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.TitanEngineer")]
public sealed class TitanEngineer : Behavior
{
    [PropertyId("integridadeInicial")] public float IntegridadeInicial = 100;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.2f;
    private GameObject? _camera, _held;
    private float _integrity;
    private int _fuel, _switches, _raiders;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos"); _integrity = IntegridadeInicial;
        var world = Object.Parent;
        for (var index = 1; index <= 2; index++)
            world?.Find("Alavanca de trilho " + index)?.Find("Indicador verde")?.SetActive(false);
        Scene.Log(ObjectId, "EXPRESSO TITÃ · 3 cápsulas · 2 alavancas · 6 saqueadores");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        if (_integrity <= 0) { _finished = true; Scene.Log(ObjectId, "TREM TOMADO · reinicie Play"); return; }
        if (_fuel == 3 && _switches == 2 && _raiders == 6)
        { _finished = true; Scene.Log(ObjectId, "EXPRESSO SALVO · integridade " + MathF.Ceiling(_integrity)); return; }
        if (!Input.JustPressed("Ação") || _camera is not { IsAlive: true }) return;
        var target = Aim(_held, 5.2f);
        if (_held is { IsAlive: true } carried)
        {
            if (carried.Name.StartsWith("Cápsula de carvão ", StringComparison.Ordinal) && target?.Name == "Fornalha central")
            { carried.Destroy(); _held = null; _fuel++; Scene.Log(ObjectId, "FORNALHA " + _fuel + "/3 · alavancas " + _switches + "/2 · invasores " + _raiders + "/6"); }
            else
            {
                var pose = _camera.WorldTransform; var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
                FindBehavior<ThrownCargo>(carried)?.Arm(); Scene.SetBodyVelocity(carried.ObjectId, forward * 13 + Vector3.UnitY * 2.1f);
                _held = null; Scene.Log(ObjectId, "CARGA LANÇADA");
            }
            return;
        }
        if (target?.Name.StartsWith("Cápsula de carvão ", StringComparison.Ordinal) == true ||
            target?.Name.StartsWith("Carga solta ", StringComparison.Ordinal) == true)
        { _held = target; Scene.Log(ObjectId, "SEGURANDO " + target.Name); return; }
        if (target?.Name.StartsWith("Alavanca de trilho ", StringComparison.Ordinal) == true)
        {
            var green = target.Find("Indicador verde");
            if (green is { ActiveInHierarchy: true }) return;
            target.Find("Indicador vermelho")?.SetActive(false); green?.SetActive(true); _switches++;
            Scene.Log(ObjectId, "ROTA DEFINIDA " + _switches + "/2"); return;
        }
        var raider = FindBehavior<RaiderAgent>(target);
        if (raider != null && raider.Damage(1)) RegisterRaider();
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform; var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 49) { _held = null; return; }
        var velocity = delta * 12.5f - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 169) velocity = Vector3.Normalize(velocity) * 13;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    public void TakeDamage(float amount)
    { if (!_finished) { _integrity = MathF.Max(0, _integrity - amount); Scene.Log(ObjectId, "ATAQUE AO TREM · integridade " + MathF.Ceiling(_integrity)); } }
    public void RegisterRaider() { _raiders = Math.Min(6, _raiders + 1); Scene.Log(ObjectId, "SAQUEADORES " + _raiders + "/6"); }

    private GameObject? Aim(GameObject? ignored, float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform; var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * range, out _, QueryFilter.Default.Ignoring(Object), 16);
        foreach (var hit in hits) if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }
}
''',
        "RaiderAgent.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.RaiderAgent")]
public sealed class RaiderAgent : Behavior
{
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _attack;
    private int _health = 2;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Foguista"); _home = Object.WorldTransform.Position; _phase = (ObjectId % 13) * .41f; }
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return; _phase += dt; _attack = MathF.Max(0, _attack - dt);
        var current = Object.WorldTransform.Position; var target = _player.WorldTransform.Position; var toPlayer = target - current;
        var distance = toPlayer.Length(); var destination = distance < 20 ? target : _home + new Vector3(MathF.Sin(_phase) * 1.5f, 0, MathF.Cos(_phase) * 1.2f);
        var travel = destination - current; travel.Y = 0;
        if (travel.LengthSquared() > .04f)
        {
            var direction = Vector3.Normalize(travel); var step = direction * MathF.Min(2.45f * dt, travel.Length());
            var hit = Physics.ShapeCast(ShapeQuery.Capsule(.3f, .72f), current, step, QueryFilter.Default.Ignoring(Object));
            if (hit is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
                step = Vector3.Cross(Vector3.UnitY, direction) * 2.1f * dt * (((ObjectId & 1) == 0) ? 1 : -1);
            Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
        }
        if (distance < 1.8f && _attack <= 0) { _attack = 1.25f; FindBehavior<TitanEngineer>(_player)?.TakeDamage(7); }
    }
    public bool Damage(int amount) { _health -= amount; if (_health > 0) return false; Object.Destroy(); return true; }
}
''',
        "ConductorAgent.cs": '''using Astra;
using System;
using System.Numerics;

[ComponentId("project.ConductorAgent")]
public sealed class ConductorAgent : Behavior
{
    private float _phase;
    public override void FixedUpdate(float dt)
    {
        _phase += dt * .45f; var current = Object.WorldTransform.Position;
        var destination = new Vector3(MathF.Sin(_phase) * 2.4f, current.Y, 17 + MathF.Cos(_phase) * 4.5f);
        var delta = destination - current; if (delta.LengthSquared() < .03f) return;
        var direction = Vector3.Normalize(delta); var step = direction * MathF.Min(1.8f * dt, delta.Length());
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
    }
}
''',
        "ThrownCargo.cs": '''using Astra;
using System.Numerics;

[ComponentId("project.ThrownCargo")]
public sealed class ThrownCargo : Behavior
{
    private float _armed, _speed;
    public void Arm() => _armed = 3.5f;
    public override void FixedUpdate(float dt) { _armed = System.MathF.Max(0, _armed - dt); _speed = Scene.GetBodyVelocity(ObjectId).Length(); }
    public override void CollisionEnter(Collision collision)
    {
        if (_armed <= 0 || _speed < 5.2f) return;
        var raider = FindBehavior<RaiderAgent>(Resolve(collision.Other)); if (raider is null) return;
        _armed = 0; if (!raider.Damage(2)) return;
        var player = Object.Parent?.Parent?.Find("Foguista"); FindBehavior<TitanEngineer>(player)?.RegisterRaider();
    }
}
'''
    }
}
kit.SCRIPTS.update(SCRIPTS)


if __name__ == "__main__":
    projects = (
        ("mercado-nexus", "MERCADO NEXUS", mercado_nexus(),
         "Distribua seis cargas físicas entre três mercadores autônomos enquanto NPCs cumprem rotinas no bazar.",
         {"concrete": "nexus-pavement", "steel": "nexus-titanium", "wood": "nexus-fabric", "rock": "nexus-rubber"}),
        ("farol-abissal", "FAROL ABISSAL", farol_abissal(),
         "Transporte quatro células de pressão, reative o farol e use pulsos contra predadores abissais com IA.",
         {"concrete": "abyss-ceramic", "steel": "abyss-steel", "wood": "abyss-teak", "rock": "abyss-basalt"}),
        ("expresso-tita", "EXPRESSO TITÃ", expresso_tita(),
         "Alimente a fornalha, opere duas alavancas e defenda o trem lançando carga física contra saqueadores.",
         {"concrete": "titan-granite", "steel": "titan-armor", "wood": "titan-walnut", "rock": "titan-coal"}),
    )
    for slug, title, scene, objective, surfaces in projects:
        glb = kit.world_glb(surfaces, TEXTURES)
        kit.write_project(slug, title, scene, glb, objective)
        kit.write_thumbnail(slug)
