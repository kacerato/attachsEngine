using Astra;
using System;
using System.Numerics;

[ComponentId("project.SentinelAgent")]
public sealed class SentinelAgent : Behavior
{
    [PropertyId("alcance")] public float Alcance = 17;
    [PropertyId("velocidade")] public float Velocidade = 1.8f;
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _shot;
    private int _health = 2;

    public override void Start()
    {
        _player = Object.Parent?.Parent?.Find("Operador");
        _home = Object.WorldTransform.Position;
        _phase = (ObjectId % 11) * .71f;
        _shot = 1 + (ObjectId % 5) * .35f;
    }

    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt * .72f;
        var current = Object.WorldTransform.Position;
        var target = _player.WorldTransform.Position + new Vector3(0, 1.1f, 0);
        var toPlayer = target - current;
        var distance = toPlayer.Length();
        var destination = _home + new Vector3(MathF.Sin(_phase) * 1.45f, 0, MathF.Cos(_phase * .7f) * .55f);
        if (distance < Alcance * 1.35f)
        {
            var forward = distance > .01f ? toPlayer / distance : Vector3.UnitZ;
            var strafe = Vector3.Cross(Vector3.UnitY, forward) * MathF.Sin(_phase * 1.7f);
            destination = distance > 9 ? current + forward * 2.2f : current + strafe * 1.7f;
        }
        var travel = destination - current;
        travel.Y = 0;
        var direction = travel.LengthSquared() > .01f ? Vector3.Normalize(travel) : Vector3.UnitZ;
        var step = direction * MathF.Min(Velocidade * dt, travel.Length());
        var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.4f), current, step,
                                         QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
            step = Vector3.Cross(Vector3.UnitY, direction) * Velocidade * dt * (((ObjectId & 1) == 0) ? 1 : -1);
        var next = current + step;
        var facing = MathF.Atan2(toPlayer.X, toPlayer.Z);
        Scene.MoveKinematic(ObjectId, next, Quaternion.CreateFromAxisAngle(Vector3.UnitY, facing));
        _shot -= dt;
        if (_shot > 0) return;
        _shot = 1.5f + (ObjectId % 4) * .22f;
        var from = next + new Vector3(0, .3f, 0);
        var shot = target - from;
        if (shot.LengthSquared() > Alcance * Alcance) return;
        var line = Physics.RayCast(from, shot, QueryFilter.Default.Ignoring(Object));
        if (line is { } hit && hit.Object.ObjectId != _player.ObjectId) return;
        FindBehavior<TacticalPlayer>(_player)?.TakeDamage(5);
    }

    public bool Damage(int amount)
    {
        _health -= amount;
        if (_health > 0) return false;
        Object.Destroy();
        return true;
    }
}
