using Astra;
using System;
using System.Numerics;

[ComponentId("project.PilotoCircuito")]
public sealed class PilotoCircuito : Behavior
{
    [PropertyId("aceleracao")] public float Aceleracao = 12;
    [PropertyId("velocidade_maxima")] public float VelocidadeMaxima = 19;
    [PropertyId("direcao")] public float Direcao = 9;
    private GameObject? _portais;
    private int _proximo;
    private float _velocidade;

    public override void Start()
    {
        _portais = Object.Parent?.Find("Portais");
        Scene.Log(ObjectId, "CIRCUITO: eixo vertical acelera, horizontal desvia e Ação ativa turbo; atravesse cinco portais.");
    }

    public override void Update(float dt)
    {
        var axis = Input.Move;
        _velocidade = Math.Clamp(_velocidade + axis.Y * Aceleracao * dt, -7, VelocidadeMaxima);
        _velocidade *= MathF.Max(0, 1 - .35f * dt);
        if (Input.JumpPressed) { _velocidade = MathF.Min(VelocidadeMaxima, _velocidade + 8); Scene.Log(ObjectId, "Turbo"); }
        var p = Object.Position;
        p.X = Math.Clamp(p.X + axis.X * Direcao * dt, -11, 11);
        p.Z = Math.Clamp(p.Z + _velocidade * dt, -35, 35);
        Object.Position = p;
        if (_portais is not { IsAlive: true } || _proximo >= _portais.ChildCount) return;
        var portal = _portais.ChildAt(_proximo);
        if (Vector3.DistanceSquared(p, portal.Position) > 12) return;
        portal.SetActive(false);
        Scene.Log(ObjectId, "Checkpoint " + ++_proximo + "/5");
        if (_proximo == 5) Scene.Log(ObjectId, "VITÓRIA: circuito concluído.");
    }
}
