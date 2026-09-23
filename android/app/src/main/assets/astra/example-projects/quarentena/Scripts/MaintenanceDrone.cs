using Astra;
using System;
using System.Numerics;

[ComponentId("project.MaintenanceDrone")]
public sealed class MaintenanceDrone : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 3.2f;
    private GameObject? _player;
    private readonly GameObject?[] _fuses = new GameObject?[3];
    private bool _power;

    public override void Start()
    {
        var world = Object.Parent;
        _player = world?.Find("Operador");
        for (var i = 0; i < _fuses.Length; i++) _fuses[i] = world?.Find("Fusível " + (i + 1));
        Scene.Log(ObjectId, "MIRA ONLINE · seguindo e indicando o próximo módulo");
    }

    public void PowerRestored() => _power = true;

    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        var current = Object.WorldTransform.Position;
        var player = _player.WorldTransform.Position;
        var destination = player + new Vector3(1.45f, 1.65f, -1.15f);
        if (!_power)
            foreach (var fuse in _fuses)
                if (fuse is { IsAlive: true } && Vector3.DistanceSquared(player, fuse.WorldTransform.Position) < 110)
                { destination = fuse.WorldTransform.Position + Vector3.UnitY * .85f; break; }
        if (_power && player.Z > 18) destination = new Vector3(0, 2.2f, 25.5f);
        var delta = destination - current;
        if (delta.LengthSquared() < .12f) return;
        var direction = Vector3.Normalize(delta);
        var step = direction * MathF.Min(Velocidade * dt, delta.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.34f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } hit && hit.Object.ObjectId != _player.ObjectId)
        {
            var side = Vector3.Normalize(Vector3.Cross(Vector3.UnitY, direction));
            step = (direction * .25f + side * (((ObjectId & 1) == 0) ? 1 : -1)) * Velocidade * dt;
        }
        var yaw = MathF.Atan2(direction.X, direction.Z);
        Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, yaw));
    }
}
