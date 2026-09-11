using Astra;
using System;
using System.Numerics;

// Plataforma cinemática que vai e volta entre dois pontos.
//
// Ela usa MOVIMENTO FÍSICO (`MoveKinematic`), não escrita de transform: é o
// movimento cinemático que faz o solver transportar quem está em cima. Escrever
// a pose direto teleportaria a plataforma e deixaria o personagem para trás —
// e o mundo de execução recusa essa escrita justamente por isso.
[ComponentId("project.PlataformaMovel")]
public sealed class PlataformaMovel : Behavior
{
    [PropertyId("deslocamento")] public Vector3 Deslocamento = new(0, 0, 6);
    [PropertyId("velocidade")] public float Velocidade = 2;
    [PropertyId("espera")] public float Espera = 1;

    private Vector3 _inicio;
    private float _progresso;
    private float _sentido = 1;
    private float _parada;

    public override void Start() => _inicio = Object.WorldTransform.Position;

    public override void FixedUpdate(float deltaTime)
    {
        var comprimento = Deslocamento.Length();
        if (comprimento <= 0.0001f || Velocidade <= 0) return;

        if (_parada > 0)
        {
            _parada -= deltaTime;
            // Continua publicando a pose parada: o contrato cinemático é uma
            // pose por passo, e pular passos faria o solver interpolar saltos.
            Publicar();
            return;
        }

        _progresso += _sentido * Velocidade * deltaTime / comprimento;
        if (_progresso >= 1) { _progresso = 1; _sentido = -1; _parada = Espera; }
        else if (_progresso <= 0) { _progresso = 0; _sentido = 1; _parada = Espera; }
        Publicar();
    }

    private void Publicar()
    {
        var destino = _inicio + Deslocamento * Math.Clamp(_progresso, 0, 1);
        Scene.MoveKinematic(ObjectId, destino, Object.WorldTransform.Rotation);
    }
}
