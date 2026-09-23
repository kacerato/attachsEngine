using Astra;
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
