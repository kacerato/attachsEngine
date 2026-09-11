using Astra;
using System.Numerics;

// Quem interage: olha para a frente, encontra o objeto mirado e chama a
// capacidade dele.
//
// Não conhece porta, baú nem alavanca — procura por CONTRATO (`IInteragivel`).
// Acrescentar um tipo novo de coisa interagível não exige tocar neste arquivo.
[ComponentId("project.Interacao")]
public sealed class Interacao : Behavior
{
    [PropertyId("acao")] public string Acao = "Interagir";
    [PropertyId("alcance")] public float Alcance = 2.5f;
    [PropertyId("camadas")] public string Camadas = "";

    private uint _mascara = 0xffffffffu;
    private string _rotulo = "";

    /// <summary>O que está sob a mira agora, para a interface de jogo mostrar.</summary>
    public string RotuloAtual => _rotulo;

    public override void Start()
    {
        // A máscara é resolvida uma vez: nomes de camada viram bits aqui, e não
        // a cada quadro.
        if (!string.IsNullOrEmpty(Camadas)) _mascara = Physics.LayerMask(Camadas.Split(','));
    }

    public override void Update(float deltaTime)
    {
        var pose = Object.WorldTransform;
        var frente = Vector3.Transform(Vector3.UnitZ, pose.Rotation);

        var filtro = QueryFilter.Default;
        filtro.LayerMask = _mascara;
        filtro.Ignore = ObjectId;
        var alvo = Physics.RayCast(pose.Position, frente * Alcance, filtro);

        _rotulo = "";
        if (alvo is not { } acerto) return;
        var capacidade = FindBehavior<IInteragivel>(acerto.Object);
        if (capacidade is null) return;

        _rotulo = capacidade.Rotulo;
        // `JustPressed` e não `Pressed`: segurar o botão não pode reabrir a
        // porta sessenta vezes por segundo.
        if (Input.JustPressed(Acao)) capacidade.Interagir(Object);
    }
}
