namespace Aether.Flow.Ast;

/// <summary>
/// A árvore sintática completa de uma lógica do AetherFlow — a fonte única da
/// verdade da qual Blocos, Grafo e C# são apenas vistas (Parte 9.2 do plano).
/// </summary>
public sealed class FlowGraph
{
    public required string Name { get; set; }
    public List<FlowNode> Nodes { get; init; } = new();
    public List<FlowConnection> Connections { get; init; } = new();
    public List<FlowVariable> Variables { get; init; } = new();

    public FlowNode? FindNode(string id) => Nodes.FirstOrDefault(n => n.Id == id);
    public FlowVariable? FindVariable(string name) => Variables.FirstOrDefault(v => v.Name == name);

    /// <summary>A conexão (se houver) que alimenta um pino de ENTRADA específico.
    /// Um pino de entrada aceita no máximo uma conexão.</summary>
    public FlowConnection? IncomingTo(string nodeId, string pinName) =>
        Connections.FirstOrDefault(c => c.To.NodeId == nodeId && c.To.PinName == pinName);

    /// <summary>Todas as conexões que saem de um pino de SAÍDA. Um pino de saída
    /// de dado pode alimentar vários destinos; um pino de saída de execução
    /// normalmente alimenta só um (mas isso não é imposto aqui).</summary>
    public IEnumerable<FlowConnection> OutgoingFrom(string nodeId, string pinName) =>
        Connections.Where(c => c.From.NodeId == nodeId && c.From.PinName == pinName);

    public IEnumerable<FlowNode> EventNodes => Nodes.Where(n => n.NodeType.StartsWith("event.", StringComparison.Ordinal));

    public string NewNodeId(string hint)
    {
        int i = 0;
        string id;
        do { id = $"{hint}{i++}"; } while (FindNode(id) is not null);
        return id;
    }
}
