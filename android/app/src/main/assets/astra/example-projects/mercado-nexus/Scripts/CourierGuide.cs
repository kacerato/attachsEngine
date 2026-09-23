using Astra;
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
