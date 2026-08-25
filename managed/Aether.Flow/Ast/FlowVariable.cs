namespace Aether.Flow.Ast;

/// <summary>Onde uma variável vive e por quanto tempo.</summary>
public enum VarScope
{
    /// <summary>Só existe durante a execução de um nó de laço/sub-grafo.</summary>
    Local,
    /// <summary>Uma por instância do objeto (ex. vida de um inimigo).</summary>
    Object,
    /// <summary>Compartilhada por todo o jogo.</summary>
    Global,
    /// <summary>Persiste entre sessões de jogo (disco/nuvem).</summary>
    Saved,
}

public sealed class FlowVariable
{
    public required string Name { get; init; }
    public required FlowType Type { get; init; }
    public required FlowValue Initial { get; init; }
    public VarScope Scope { get; init; } = VarScope.Object;

    public override string ToString() => $"{Scope} {Type} {Name} = {Initial}";
}
