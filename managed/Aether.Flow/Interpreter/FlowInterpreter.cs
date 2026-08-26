using Aether.Flow.Ast;

namespace Aether.Flow.Interpreter;

/// <summary>Estourou o orçamento de passos de execução — em geral, um laço que
/// o usuário montou sem perceber que nunca termina. Nunca deixamos isso travar
/// o editor (barra de qualidade nº 3); em vez disso, interrompemos e apontamos
/// exatamente qual bloco estava repetindo.</summary>
public sealed class FlowExecutionLimitException : Exception
{
    public string NodeId { get; }
    public FlowExecutionLimitException(string nodeId, int limit)
        : base($"Este projeto parece estar num laço sem fim — o bloco \"{nodeId}\" repetiu mais de {limit} vezes " +
               "sem parar. Confira a condição do \"Enquanto\": ela precisa ficar falsa em algum momento.")
    {
        NodeId = nodeId;
    }
}

/// <summary>
/// Executa a AST diretamente, nó a nó — o modo "editor" descrito na Parte 9.3
/// do plano: sem passar por C#/IL, para reagir em menos de 100 ms enquanto o
/// usuário ainda está montando o grafo. É deliberadamente simples; a
/// performance de produção vem da compilação, não deste caminho.
/// </summary>
public sealed class FlowInterpreter
{
    private enum ControlSignal
    {
        None,
        Return,
        Break,
        Continue,
    }

    private readonly record struct ExecutionStep(string? NextNodeId, ControlSignal Signal)
    {
        public static ExecutionStep Next(string? nodeId) => new(nodeId, ControlSignal.None);
        public static ExecutionStep Stop(ControlSignal signal) => new(null, signal);
    }

    private readonly FlowGraph _graph;
    private readonly Dictionary<string, FlowValue> _variables = new();
    private readonly int _maxSteps;
    private readonly Action<string>? _onNodeExecuted;
    private int _steps;

    public IReadOnlyDictionary<string, FlowValue> Variables => _variables;

    public FlowInterpreter(FlowGraph graph, int maxSteps = 100_000, Action<string>? onNodeExecuted = null)
    {
        _graph = graph;
        _maxSteps = maxSteps;
        _onNodeExecuted = onNodeExecuted;
        foreach (var v in graph.Variables)
            _variables[v.Name] = v.Initial;
    }

    /// <summary>Roda o corpo do primeiro nó de evento do tipo pedido (ex.
    /// "event.start"). Se não existir tal evento, não faz nada — grafo vazio ou
    /// sem esse evento não é erro, é só um projeto incompleto.</summary>
    public void RunEvent(string eventNodeType, FlowValue? dt = null, FlowValue? other = null)
    {
        _steps = 0; // cada chamada tem seu próprio orçamento — uma explosão de limite não deixa o interpretador inutilizável
        var evt = _graph.Nodes.FirstOrDefault(n => n.NodeType == eventNodeType);
        if (evt is null) return;

        if (dt is not null) _variables["__dt"] = dt;
        if (other is not null) _variables["__outro"] = other;

        var startConn = _graph.OutgoingFrom(evt.Id, "corpo").FirstOrDefault();
        if (startConn is null) return; // evento sem corpo: nada para fazer, não é erro

        var signal = ExecuteChain(startConn.To.NodeId);
        if (signal is ControlSignal.Break or ControlSignal.Continue)
            throw new InvalidOperationException("O grafo contém 'Parar laço' ou 'Continuar' fora de um laço válido. Execute o validador antes de rodar o evento.");
    }

    private void Step(string nodeId)
    {
        _steps++;
        if (_steps > _maxSteps) throw new FlowExecutionLimitException(nodeId, _maxSteps);
        _onNodeExecuted?.Invoke(nodeId);
    }

    /// <summary>Executa a sequência de nós de execução a partir de <paramref name="nodeId"/>
    /// até não haver mais próximo (fim do bloco).</summary>
    private ControlSignal ExecuteChain(string? nodeId)
    {
        while (nodeId is not null)
        {
            Step(nodeId);
            var node = _graph.FindNode(nodeId)!;
            var result = ExecuteOne(node);
            if (result.Signal != ControlSignal.None) return result.Signal;
            nodeId = result.NextNodeId;
        }
        return ControlSignal.None;
    }

    /// <summary>Executa um único nó e devolve o id do PRÓXIMO nó da cadeia
    /// (ou null se o bloco termina aqui). Nós de laço/ramo tratam sua própria
    /// recursão internamente e devolvem apenas o que vem DEPOIS deles.</summary>
    private ExecutionStep ExecuteOne(FlowNode node)
    {
        switch (node.NodeType)
        {
            case NodeTypes.SetVariable:
            {
                var value = Evaluate(node, "valor");
                _variables[node.Properties["VariableName"]] = value;
                return ExecutionStep.Next(NextOf(node.Id, "saida"));
            }
            case NodeTypes.FlowIf:
            {
                bool cond = Evaluate(node, "condicao").BoolValue;
                var branchSignal = ExecuteChain(NextOf(node.Id, cond ? "entao" : "senao"));
                if (branchSignal != ControlSignal.None) return ExecutionStep.Stop(branchSignal);
                return ExecutionStep.Next(NextOf(node.Id, "depois")); // os dois ramos convergem aqui, exista "senão" ou não
            }
            case NodeTypes.FlowWhile:
            {
                while (Evaluate(node, "condicao").BoolValue)
                {
                    Step(node.Id); // cada iteração conta para o limite de passos
                    var bodySignal = ExecuteChain(NextOf(node.Id, "corpo"));
                    if (bodySignal == ControlSignal.Return) return ExecutionStep.Stop(ControlSignal.Return);
                    if (bodySignal == ControlSignal.Break) break;
                    if (bodySignal == ControlSignal.Continue) continue;
                }
                return ExecutionStep.Next(NextOf(node.Id, "fim"));
            }
            case NodeTypes.FlowReturn:
                return ExecutionStep.Stop(ControlSignal.Return);
            case NodeTypes.FlowBreak:
                return ExecutionStep.Stop(ControlSignal.Break);
            case NodeTypes.FlowContinue:
                return ExecutionStep.Stop(ControlSignal.Continue);
            case NodeTypes.CodeRaw:
                // Nó opaco: o interpretador de edição não tenta rodar C# arbitrário.
                // No modo build ele já foi compilado para IL como qualquer outro código.
                return ExecutionStep.Next(NextOf(node.Id, "saida"));
            default:
                return ExecutionStep.Next(NextOf(node.Id, "saida"));
        }
    }

    private string? NextOf(string nodeId, string pin) => _graph.OutgoingFrom(nodeId, pin).FirstOrDefault()?.To.NodeId;

    /// <summary>Avalia o valor que chega num pino de ENTRADA de dado: segue a conexão
    /// se houver uma, senão usa o literal do próprio nó.</summary>
    private FlowValue Evaluate(FlowNode consumer, string pinName)
    {
        var conn = _graph.IncomingTo(consumer.Id, pinName);
        if (conn is null) return consumer.Literals[pinName];

        var source = _graph.FindNode(conn.From.NodeId)!;
        return source.NodeType switch
        {
            NodeTypes.GetVariable => _variables[source.Properties["VariableName"]],
            NodeTypes.EventUpdate => _variables.GetValueOrDefault("__dt", FlowValue.OfFloat(0)),
            NodeTypes.EventCollision => _variables.GetValueOrDefault("__outro", FlowValue.OfEntity(-1)),
            NodeTypes.MathAdd => Numeric(source, (a, b) => a + b),
            NodeTypes.MathSubtract => Numeric(source, (a, b) => a - b),
            NodeTypes.MathMultiply => Numeric(source, (a, b) => a * b),
            NodeTypes.MathDivide => Numeric(source, (a, b) => b == 0 ? 0 : a / b),
            NodeTypes.MathCompare => Compare(source),
            _ => throw new InvalidOperationException($"nó de dado não suportado pelo interpretador: {source.NodeType}"),
        };
    }

    private FlowValue Numeric(FlowNode node, Func<double, double, double> op)
    {
        var a = Evaluate(node, "a");
        var b = Evaluate(node, "b");
        double result = op(a.AsNumber(), b.AsNumber());
        bool bothInt = a.Type == FlowType.Int && b.Type == FlowType.Int;
        return bothInt ? FlowValue.OfInt((long)result) : FlowValue.OfFloat(result);
    }

    private FlowValue Compare(FlowNode node)
    {
        var a = Evaluate(node, "a");
        var b = Evaluate(node, "b");
        double x = a.AsNumber(), y = b.AsNumber();
        bool result = node.Properties["Operator"] switch
        {
            "<" => x < y,
            "<=" => x <= y,
            ">" => x > y,
            ">=" => x >= y,
            "==" => x == y,
            "!=" => x != y,
            var op => throw new InvalidOperationException($"operador de comparação desconhecido: {op}"),
        };
        return FlowValue.OfBool(result);
    }
}
