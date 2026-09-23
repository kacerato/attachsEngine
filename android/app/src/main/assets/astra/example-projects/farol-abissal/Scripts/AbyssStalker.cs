using Astra;
using System;
using System.Numerics;

[ComponentId("project.AbyssStalker")]
public sealed class AbyssStalker : Behavior
{
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _attack, _stun;
    public override void Start() { _player = Object.Parent?.Parent?.Find("Mergulhador"); _home = Object.WorldTransform.Position; _phase = (ObjectId % 11) * .43f; }
    public void Stun() => _stun = 4.5f;
    public override void FixedUpdate(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt; _attack = MathF.Max(0, _attack - dt); _stun = MathF.Max(0, _stun - dt);
        if (_stun > 0) return;
        var current = Object.WorldTransform.Position; var player = _player.WorldTransform.Position;
        var delta = player - current; var distance = delta.Length();
        var destination = distance < 13 ? player + new Vector3(MathF.Sin(_phase) * 1.8f, 1.2f, 0)
                                        : _home + new Vector3(MathF.Sin(_phase) * 2.4f, MathF.Cos(_phase * .8f) * .6f, MathF.Cos(_phase) * 2.1f);
        var travel = destination - current;
        if (travel.LengthSquared() > .05f)
        {
            var direction = Vector3.Normalize(travel); var step = direction * MathF.Min(2.65f * dt, travel.Length());
            var obstacle = Physics.ShapeCast(ShapeQuery.Sphere(.36f), current, step, QueryFilter.Default.Ignoring(Object));
            if (obstacle is { } blocked && blocked.Object.ObjectId != _player.ObjectId)
                step = Vector3.Cross(Vector3.UnitY, direction) * 2.2f * dt;
            Scene.MoveKinematic(ObjectId, current + step, Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.Atan2(direction.X, direction.Z)));
        }
        if (distance < 2.2f && _attack <= 0) { _attack = 1.4f; FindBehavior<AbyssDiver>(_player)?.TakePressure(9); }
    }
}
