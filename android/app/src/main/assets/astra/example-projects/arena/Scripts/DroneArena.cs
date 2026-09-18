using Astra;
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
