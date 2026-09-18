using Astra;
using System;
using System.Numerics;

[ComponentId("project.DefensorArena")]
public sealed class DefensorArena : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 8;
    [PropertyId("alcance")] public float Alcance = 12;
    private GameObject? _drones;
    private int _abates;
    private float _vida = 100;
    private float _invulneravel;

    public override void Start()
    {
        _drones = Object.Parent?.Find("Drones");
        Scene.Log(ObjectId, "ARENA: mova e toque Ação para eliminar até quatro drones próximos por pulso.");
    }

    public override void Update(float dt)
    {
        if (_vida <= 0 || _abates == 48) return;
        var axis = Input.Move;
        var direction = new Vector3(axis.X, 0, axis.Y);
        if (direction.LengthSquared() > 1) direction = Vector3.Normalize(direction);
        var p = Object.Position + direction * Velocidade * dt;
        Object.Position = new Vector3(Math.Clamp(p.X, -22, 22), .7f, Math.Clamp(p.Z, -22, 22));
        _invulneravel = MathF.Max(0, _invulneravel - dt);
        if (_drones is not { IsAlive: true }) return;
        foreach (var drone in _drones.Children())
        {
            if (!drone.ActiveInHierarchy) continue;
            var d = Vector3.DistanceSquared(drone.Position, Object.Position);
            if (d < 1.6f && _invulneravel == 0)
            {
                _vida = MathF.Max(0, _vida - 5); _invulneravel = 1.5f;
                Scene.Log(ObjectId, "Vida " + _vida);
                if (_vida <= 0) { Scene.Log(ObjectId, "DERROTA: reinicie Play."); return; }
            }
        }
        if (!Input.JumpPressed) return;
        for (var hit = 0; hit < 4 && _abates < 48; hit++)
        {
            GameObject? closest = null;
            var distance = Alcance * Alcance;
            foreach (var drone in _drones.Children())
            {
                if (!drone.ActiveInHierarchy) continue;
                var d = Vector3.DistanceSquared(drone.Position, Object.Position);
                if (d < distance) { closest = drone; distance = d; }
            }
            if (closest is null) break;
            closest.SetActive(false);
            ++_abates;
        }
        Scene.Log(ObjectId, "Drones destruídos " + _abates + "/48");
        if (_abates == 48) Scene.Log(ObjectId, "VITÓRIA: arena limpa.");
    }
}
