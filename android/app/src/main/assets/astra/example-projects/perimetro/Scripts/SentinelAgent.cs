using Astra;
using System;
using System.Numerics;

[ComponentId("project.SentinelAgent")]
public sealed class SentinelAgent : Behavior
{
    [PropertyId("alcance")] public float Alcance = 13;
    private GameObject? _player;
    private Vector3 _home;
    private float _phase, _shot;
    private int _health = 2;

    public override void Start()
    {
        _player = Object.Parent?.Parent?.Find("Operador");
        _home = Object.Position;
        _phase = (ObjectId % 11) * .71f;
        _shot = 1 + (ObjectId % 5) * .35f;
    }

    public override void Update(float dt)
    {
        if (_player is not { IsAlive: true }) return;
        _phase += dt * .6f;
        var next = _home + new Vector3(MathF.Sin(_phase) * .8f, 0, 0);
        Scene.MoveKinematic(ObjectId, next, Quaternion.Identity);
        _shot -= dt;
        if (_shot > 0) return;
        _shot = 1.5f + (ObjectId % 4) * .22f;
        var from = next + new Vector3(0, .3f, 0);
        var target = _player.WorldTransform.Position + new Vector3(0, 1.2f, 0);
        var direction = target - from;
        if (direction.LengthSquared() > Alcance * Alcance) return;
        var obstacle = Physics.RayCast(from, direction, QueryFilter.Default.Ignoring(Object));
        if (obstacle is { } hit && hit.Object.ObjectId != _player.ObjectId) return;
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
