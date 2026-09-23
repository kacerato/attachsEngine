using Astra;
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
