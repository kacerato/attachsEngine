using Astra;
using System;
using System.Numerics;

// Segue um alvo mantendo um deslocamento.
//
// Não conhece câmera: funciona igual em uma luz, um marcador ou um objeto
// qualquer, porque só lê a pose do alvo e escreve a própria. Trocar o modelo, o
// nome ou o alvo não muda nada aqui.
[ComponentId("project.SeguirAlvo")]
public sealed class SeguirAlvo : Behavior
{
    [PropertyId("alvo")] public ObjectReference Alvo;
    [PropertyId("deslocamento")] public Vector3 Deslocamento = new(0, 3, -6);
    // Zero segue instantaneamente; valores maiores alcançam mais rápido.
    [PropertyId("suavidade")] public float Suavidade = 8;

    private GameObject? _alvo;

    public override void Start()
    {
        // Resolvido UMA vez: procurar por nome todo quadro percorreria a cena
        // inteira, e um alvo removido deve parar de ser seguido, não ser
        // reencontrado por acaso em outro objeto com o mesmo nome.
        if (Resolve(Alvo) is { } encontrado) _alvo = encontrado;
    }

    public override void Update(float deltaTime)
    {
        if (_alvo is not { IsAlive: true }) return;
        var destino = _alvo.WorldTransform.Position + Deslocamento;
        var atual = Object.WorldTransform;
        var fator = Suavidade <= 0 ? 1 : Math.Min(1, Suavidade * deltaTime);
        Object.WorldTransform = atual with { Position = Vector3.Lerp(atual.Position, destino, fator) };
    }
}
