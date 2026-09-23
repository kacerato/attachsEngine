using Astra;
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
