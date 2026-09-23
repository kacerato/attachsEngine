using Astra;
using System;
using System.Numerics;

[ComponentId("project.RaiderAgent")]
public sealed class RaiderAgent : Behavior
{
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _attack;
    private int _health = 2;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Foguista"); _home = Object.WorldTransform.Position; _phase = (ObjectId % 13) * .41f; }
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return; _phase += dt; _attack = MathF.Max(0, _attack - dt);
        var current = Object.WorldTransform.Position; var target = _player.WorldTransform.Position; var toPlayer = target - current;
        var distance = toPlayer.Length(); var destination = distance < 20 ? target : _home + new Vector3(MathF.Sin(_phase) * 1.5f, 0, MathF.Cos(_phase) * 1.2f);
        var travel = destination - current; travel.Y = 0;
        if (travel.LengthSquared() > .04f)
        {
            var direction = Vector3.Normalize(travel); var step = direction * MathF.Min(2.45f * dt, travel.Length());
            var hit = Physics.ShapeCast(ShapeQuery.Capsule(.3f, .72f), current, step, QueryFilter.Default.Ignoring(Object));
            if (hit is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
                step = Vector3.Cross(Vector3.UnitY, direction) * 2.1f * dt * (((ObjectId & 1) == 0) ? 1 : -1);
            Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
        }
        if (distance < 1.8f && _attack <= 0) { _attack = 1.25f; FindBehavior<TitanEngineer>(_player)?.TakeDamage(7); }
    }
    public bool Damage(int amount) { _health -= amount; if (_health > 0) return false; Object.Destroy(); return true; }
}
