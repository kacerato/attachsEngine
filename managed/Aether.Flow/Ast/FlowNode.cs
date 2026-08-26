using Aether.Flow.Runtime;

namespace Aether.Flow.Ast;

/// <summary>
/// Um nó do grafo de execução. O tipo do nó (<see cref="NodeType"/>) é uma string
/// estável ("flow.if", "math.add", "event.start", ...) que identifica seu
/// comportamento — o comportamento em si mora no validador, no interpretador e
/// no gerador de código, não aqui. O nó só guarda FORMA: seus pinos, seus
/// literais e sua posição no canvas.
/// </summary>
public sealed class FlowNode
{
    /// <summary>Id estável — sobrevive a reordenação, movimento no canvas e
    /// round-trip por C#. Nunca reaproveitado dentro do mesmo grafo.</summary>
    public required string Id { get; init; }

    /// <summary>Tipo do nó, ex. "flow.if", "math.add", "transform.move".</summary>
    public required string NodeType { get; set; }

    /// <summary>Serviços externos exigidos por este nó. Faz parte da AST e do
    /// formato .aflow para que validação, interpretador e código gerado usem o
    /// mesmo contrato, inclusive para nós registrados futuramente por plugins.</summary>
    public FlowCapability RequiredCapabilities { get; set; }

    public List<FlowPin> Inputs { get; init; } = new();
    public List<FlowPin> Outputs { get; init; } = new();

    /// <summary>Posição no canvas — usada apenas pela vista Grafo. Perdida ao
    /// reconstruir a partir de C# (documentado no round-trip).</summary>
    public float CanvasX { get; set; }
    public float CanvasY { get; set; }

    /// <summary>Valores literais para pinos de ENTRADA de dado desconectados,
    /// chaveados pelo nome do pino.</summary>
    public Dictionary<string, FlowValue> Literals { get; init; } = new();

    /// <summary>Metadados específicos do tipo de nó que não são pinos nem
    /// literais — ex. o nome da variável referenciada por "flow.get_variable",
    /// o operador de "math.compare", ou o texto bruto de "code.raw".</summary>
    public Dictionary<string, string> Properties { get; init; } = new();

    public FlowPin? FindInput(string name) => Inputs.FirstOrDefault(p => p.Name == name);
    public FlowPin? FindOutput(string name) => Outputs.FirstOrDefault(p => p.Name == name);

    public override string ToString() => $"{NodeType}#{Id}";
}
