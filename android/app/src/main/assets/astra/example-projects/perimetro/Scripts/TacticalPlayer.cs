using Astra;
using System;
using System.Numerics;

[ComponentId("project.TacticalPlayer")]
public sealed class TacticalPlayer : Behavior
{
    [PropertyId("municaoPente")] public int MunicaoPente = 12;
    [PropertyId("alcanceTiro")] public float AlcanceTiro = 48;
    private GameObject? _camera;
    private float _health = 100, _cooldown, _reload, _damageCooldown;
    private int _ammo, _kills;
    private bool _finished;

    public override void Start()
    {
        _camera = Object.Find("Câmera dos olhos");
        _ammo = MunicaoPente;
        Scene.Log(ObjectId, "PERÍMETRO · 15 sentinelas · cobertura · extração | VIDA 100 · MUNIÇÃO 12");
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
        if (_kills == 15 && Object.Position.Z > 25)
        {
            _finished = true;
            Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · VIDA " + _health);
            return;
        }
        if (!Input.JustPressed("Atirar") || _cooldown > 0 || _reload > 0 || _camera is not { IsAlive: true }) return;
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
                _kills++;
                Scene.Log(ObjectId, "ALVO NEUTRALIZADO " + _kills + "/15 · MUNIÇÃO " + _ammo + " · VIDA " + _health);
            }
            else Scene.Log(ObjectId, "IMPACTO: " + contact.Object.Name + " · MUNIÇÃO " + _ammo);
        }
        else Scene.Log(ObjectId, "SEM ALVO · MUNIÇÃO " + _ammo);
        if (_ammo == 0 && _kills < 15) _reload = 1.7f;
        if (_kills == 15) Scene.Log(ObjectId, "ÁREA SEGURA · alcance o terminal de extração");
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
