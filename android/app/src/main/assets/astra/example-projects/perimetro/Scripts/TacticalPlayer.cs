using Astra;
using System;
using System.Numerics;

[ComponentId("project.TacticalPlayer")]
public sealed class TacticalPlayer : Behavior
{
    [PropertyId("municaoPente")] public int MunicaoPente = 12;
    [PropertyId("alcanceTiro")] public float AlcanceTiro = 48;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.25f;
    private GameObject? _camera, _held;
    private float _health = 100, _cooldown, _reload, _damageCooldown;
    private int _ammo, _kills;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos");
        _ammo = MunicaoPente;
        Scene.Log(ObjectId, "PERÍMETRO · AÇÃO pega/lança caixotes ou dispara · 15 sentinelas");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _cooldown = MathF.Max(0, _cooldown - dt);
        _damageCooldown = MathF.Max(0, _damageCooldown - dt);
        if (_reload > 0)
        {
            _reload -= dt;
            if (_reload <= 0)
            {
                _ammo = MunicaoPente;
                Scene.Log(ObjectId, "RECARREGADO · MUNIÇÃO " + _ammo + " · VIDA " + _health);
            }
        }
        if (!Input.JustPressed("Ação") || _camera is not { IsAlive: true }) return;
        if (_held is { IsAlive: true } carried)
        {
            var heldPose = _camera.WorldTransform;
            var heldForward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, heldPose.Rotation));
            FindBehavior<ThrowableCrate>(carried)?.Arm();
            Scene.SetBodyVelocity(carried.ObjectId, heldForward * 13 + Vector3.UnitY * 2.2f);
            _held = null;
            Scene.Log(ObjectId, "CAIXOTE LANÇADO · impactos fortes neutralizam sentinelas");
            return;
        }
        var close = AimTarget(4.2f);
        if (close?.Name.StartsWith("Caixote militar real ", StringComparison.Ordinal) == true)
        {
            _held = close;
            Scene.Log(ObjectId, "SEGURANDO " + close.Name + " · pressione AÇÃO para lançar");
            return;
        }
        if (close?.Name == "Terminal de extração")
        {
            if (_kills < 15) Scene.Log(ObjectId, "TERMINAL BLOQUEADO · restam " + (15 - _kills) + " sentinelas");
            else { _finished = true; Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · VIDA " + _health); }
            return;
        }
        if (_cooldown > 0 || _reload > 0) return;
        if (_ammo == 0)
        {
            _reload = 1.7f;
            Scene.Log(ObjectId, "RECARREGANDO · proteja-se atrás da cobertura");
            return;
        }
        _ammo--;
        _cooldown = .22f;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceTiro, QueryFilter.Default.Ignoring(Object));
        if (hit is { } contact)
        {
            var enemy = FindBehavior<SentinelAgent>(contact.Object);
            if (enemy != null && enemy.Damage(1))
            {
                RegisterKill();
            }
            else Scene.Log(ObjectId, "IMPACTO: " + contact.Object.Name + " · MUNIÇÃO " + _ammo);
        }
        else Scene.Log(ObjectId, "SEM ALVO · MUNIÇÃO " + _ammo);
        if (_ammo == 0 && _kills < 15) _reload = 1.7f;
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 45) { _held = null; return; }
        var velocity = delta * 12 - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
    }

    public void RegisterKill()
    {
        if (_finished) return;
        _kills = Math.Min(15, _kills + 1);
        Scene.Log(ObjectId, _kills == 15
            ? "ÁREA SEGURA · interaja com o terminal de extração"
            : "ALVO NEUTRALIZADO " + _kills + "/15 · MUNIÇÃO " + _ammo + " · VIDA " + _health);
    }

    private GameObject? AimTarget(float range)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        return Physics.RayCast(pose.Position, forward * range,
                               QueryFilter.Default.Ignoring(Object))?.Object;
    }

    public void TakeDamage(int amount)
    {
        if (_finished || _damageCooldown > 0) return;
        _health = MathF.Max(0, _health - amount);
        _damageCooldown = .38f;
        if (_health == 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "OPERADOR CAÍDO · reinicie Play");
        }
        else Scene.Log(ObjectId, "SOB FOGO · VIDA " + _health + " · ALVOS " + _kills + "/15");
    }
}
