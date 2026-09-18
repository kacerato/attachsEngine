using Astra;
using System;
using System.Numerics;

[ComponentId("project.ExploradorCristais")]
public sealed class ExploradorCristais : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 7;
    private GameObject? _cristais;
    private GameObject? _portal;
    private int _total;
    private Vector3 _ultimoMovimento = Vector3.UnitZ;

    public override void Start()
    {
        _cristais = Object.Parent?.Find("Cristais");
        _portal = Object.Parent?.Find("Portal");
        Scene.Log(ObjectId, "CRISTAIS: mova com o controle esquerdo; Ação dá um impulso. Colete seis cristais e chegue ao portal.");
    }

    public override void Update(float dt)
    {
        var move = Input.Move;
        var direction = new Vector3(move.X, 0, move.Y);
        if (direction.LengthSquared() > 1) direction = Vector3.Normalize(direction);
        if (direction.LengthSquared() > 0) _ultimoMovimento = direction;
        var p = Object.Position + direction * Velocidade * dt;
        if (Input.JumpPressed) { p += _ultimoMovimento * 3; Scene.Log(ObjectId, "Impulso"); }
        Object.Position = new Vector3(Math.Clamp(p.X, -14, 14), .6f, Math.Clamp(p.Z, -14, 14));
        if (_cristais is not { IsAlive: true }) return;
        foreach (var item in _cristais.Children())
        {
            if (!item.ActiveInHierarchy || Vector3.DistanceSquared(item.Position, Object.Position) > 2.2f) continue;
            item.SetActive(false);
            Scene.Log(ObjectId, "Cristal " + ++_total + "/6");
        }
        if (_total == 6 && _portal is { IsAlive: true } portal &&
            Vector3.DistanceSquared(portal.Position, Object.Position) < 5)
        {
            _total = 7;
            Scene.Log(ObjectId, "VITÓRIA: todos os cristais chegaram ao portal.");
        }
    }
}
