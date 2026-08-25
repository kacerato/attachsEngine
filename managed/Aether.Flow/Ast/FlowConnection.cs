namespace Aether.Flow.Ast;

/// <summary>Uma aresta do grafo: (nó de origem, pino de saída) → (nó de destino, pino de entrada).</summary>
public sealed class FlowConnection
{
    public required FlowEndpoint From { get; init; }
    public required FlowEndpoint To { get; init; }

    public override string ToString() => $"{From} -> {To}";
}
