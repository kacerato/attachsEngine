using Astra;
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
