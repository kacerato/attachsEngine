using Astra;
using System;
using System.Numerics;

[ComponentId("project.SurvivorAgent")]
public sealed class SurvivorAgent : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 2.75f;
    private GameObject? _player;
    private Vector3 _rest;
    private float _phase;
    public bool IsHealed { get; private set; }

    public override void Start()
    {
        _player = Object.Parent?.Parent?.Find("Socorrista");
        _rest = Object.WorldTransform.Position;
        _phase = (ObjectId % 9) * .47f;
    }

    public bool Heal()
    {
        if (IsHealed) return false;
        IsHealed = true;
        return true;
    }

    public override void FixedUpdate(float dt)
    {
        _phase += dt;
        var current = Object.WorldTransform.Position;
        if (!IsHealed)
        {
            var idle = _rest + Vector3.UnitY * (MathF.Sin(_phase * 2) * .025f);
            Scene.MoveKinematic(ObjectId, idle, Quaternion.Identity);
            return;
        }
        if (_player is not { IsAlive: true }) return;
        var player = _player.WorldTransform.Position;
        var side = ((int)(ObjectId % 3) - 1) * .85f;
        var destination = player + new Vector3(side, 1.0f, -1.7f - (ObjectId % 2) * .65f);
        var delta = destination - current;
        delta.Y = 0;
        if (delta.LengthSquared() < .45f) return;
        var direction = Vector3.Normalize(delta);
        var step = direction * MathF.Min(Velocidade * dt, delta.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Capsule(.3f, .75f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } hit && hit.Object.ObjectId != _player.ObjectId)
        {
            var avoid = Vector3.Normalize(Vector3.Cross(Vector3.UnitY, direction));
            step = (direction * .25f + avoid * (((ObjectId & 1) == 0) ? 1 : -1)) * Velocidade * dt;
        }
        var yaw = MathF.Atan2(direction.X, direction.Z);
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, yaw));
    }
}
