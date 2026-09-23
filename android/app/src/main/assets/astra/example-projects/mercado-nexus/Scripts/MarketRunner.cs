using Astra;
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
