using Astra;
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
