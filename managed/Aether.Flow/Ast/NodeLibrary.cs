namespace Aether.Flow.Ast;

/// <summary>
/// Nomes canônicos de tipo de nó e fábricas para os nós do núcleo suportado
/// por esta fundação (validador + interpretador + gerador de C#). A biblioteca
/// completa do plano (Parte 9.5 — física, áudio, IA, rede, sensores, ...) é só
/// uma lista de tipos de nó adicionais que este mesmo modelo já comporta; não
/// implementamos todos aqui, só o núcleo que prova a tese do round-trip.
/// </summary>
public static class NodeTypes
{
    public const string EventStart = "event.start";
    public const string EventUpdate = "event.update";
    public const string EventCollision = "event.collision";

    public const string FlowIf = "flow.if";
    public const string FlowWhile = "flow.while";
    public const string FlowReturn = "flow.return";
    public const string FlowBreak = "flow.break";
    public const string FlowContinue = "flow.continue";

    public const string SetVariable = "flow.set_variable";
    public const string GetVariable = "flow.get_variable";

    public const string MathAdd = "math.add";
    public const string MathSubtract = "math.subtract";
    public const string MathMultiply = "math.multiply";
    public const string MathDivide = "math.divide";
    public const string MathCompare = "math.compare";

    /// <summary>Nó opaco "caixa de código" — a escotilha de escape para C# fora
    /// do subconjunto que o gerador/parser suportam (Parte 9.2 do plano).</summary>
    public const string CodeRaw = "code.raw";

    /// <summary>Tipos de nó que estruturam um laço. Um ciclo no grafo de EXECUÇÃO
    /// que passa por um destes é o "laço legítimo" citado no validador — diferente
    /// de um ciclo em fluxo de DADOS, que é sempre erro.</summary>
    public static readonly HashSet<string> LoopNodeTypes = new() { FlowWhile };
}

public static class NodeLibrary
{
    public static FlowNode EventStart(string id) => new()
    {
        Id = id,
        NodeType = NodeTypes.EventStart,
        Outputs = { Exec("corpo") },
    };

    public static FlowNode EventUpdate(string id) => new()
    {
        Id = id,
        NodeType = NodeTypes.EventUpdate,
        Outputs = { Exec("corpo"), Data("dt", FlowType.Float, PinDirection.Output) },
    };

    public static FlowNode EventCollision(string id) => new()
    {
        Id = id,
        NodeType = NodeTypes.EventCollision,
        Outputs = { Exec("corpo"), Data("outro", FlowType.Entity, PinDirection.Output) },
    };

    public static FlowNode FlowIf(string id) => new()
    {
        Id = id,
        NodeType = NodeTypes.FlowIf,
        Inputs = { Exec("entrada", PinDirection.Input), Data("condicao", FlowType.Bool, PinDirection.Input) },
        Outputs = { Exec("entao"), Exec("senao"), Exec("depois") },
    };

    public static FlowNode FlowWhile(string id) => new()
    {
        Id = id,
        NodeType = NodeTypes.FlowWhile,
        Inputs = { Exec("entrada", PinDirection.Input), Data("condicao", FlowType.Bool, PinDirection.Input) },
        Outputs = { Exec("corpo"), Exec("fim") },
    };

    /// <summary>Encerra o evento atual. Os eventos suportados hoje retornam
    /// <c>void</c>, portanto este nó não possui entrada de valor nem saída de
    /// execução.</summary>
    public static FlowNode FlowReturn(string id) => TerminalControl(id, NodeTypes.FlowReturn);

    /// <summary>Encerra apenas o laço mais interno que contém este nó.</summary>
    public static FlowNode FlowBreak(string id) => TerminalControl(id, NodeTypes.FlowBreak);

    /// <summary>Pula o restante da iteração do laço mais interno.</summary>
    public static FlowNode FlowContinue(string id) => TerminalControl(id, NodeTypes.FlowContinue);

    public static FlowNode SetVariable(string id, string variableName, FlowType type) => new()
    {
        Id = id,
        NodeType = NodeTypes.SetVariable,
        Inputs = { Exec("entrada", PinDirection.Input), Data("valor", type, PinDirection.Input) },
        Outputs = { Exec("saida") },
        Properties = { ["VariableName"] = variableName },
    };

    public static FlowNode GetVariable(string id, string variableName, FlowType type) => new()
    {
        Id = id,
        NodeType = NodeTypes.GetVariable,
        Outputs = { Data("valor", type, PinDirection.Output) },
        Properties = { ["VariableName"] = variableName },
    };

    public static FlowNode MathBinary(string id, string nodeType, FlowType operandType) => new()
    {
        Id = id,
        NodeType = nodeType,
        Inputs = { Data("a", operandType, PinDirection.Input), Data("b", operandType, PinDirection.Input) },
        Outputs = { Data("resultado", operandType, PinDirection.Output) },
    };

    /// <param name="op">Um de: &lt; &lt;= &gt; &gt;= == !=</param>
    public static FlowNode MathCompare(string id, string op, FlowType operandType) => new()
    {
        Id = id,
        NodeType = NodeTypes.MathCompare,
        Inputs = { Data("a", operandType, PinDirection.Input), Data("b", operandType, PinDirection.Input) },
        Outputs = { Data("resultado", FlowType.Bool, PinDirection.Output) },
        Properties = { ["Operator"] = op },
    };

    public static FlowNode CodeRaw(string id, string rawCode, IEnumerable<FlowPin>? pins = null)
    {
        var node = new FlowNode { Id = id, NodeType = NodeTypes.CodeRaw };
        node.Properties["RawCode"] = rawCode;
        if (pins is not null)
        {
            foreach (var p in pins)
                (p.Direction == PinDirection.Input ? node.Inputs : node.Outputs).Add(p);
        }
        return node;
    }

    private static FlowPin Exec(string name, PinDirection dir = PinDirection.Output) => new() { Name = name, Type = FlowType.Exec, Direction = dir };
    private static FlowPin Data(string name, FlowType type, PinDirection dir) => new() { Name = name, Type = type, Direction = dir };

    private static FlowNode TerminalControl(string id, string nodeType) => new()
    {
        Id = id,
        NodeType = nodeType,
        Inputs = { Exec("entrada", PinDirection.Input) },
    };
}
