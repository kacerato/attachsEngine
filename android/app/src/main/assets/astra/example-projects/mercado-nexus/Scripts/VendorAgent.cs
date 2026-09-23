using Astra;
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
