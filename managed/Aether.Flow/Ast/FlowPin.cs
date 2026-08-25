namespace Aether.Flow.Ast;

public enum PinDirection { Input, Output }

/// <summary>Um pino de um nó do grafo — de execução ("bata aqui para rodar isto")
/// ou de dado ("aqui entra/sai um valor").</summary>
public sealed class FlowPin
{
    public required string Name { get; init; }
    public required FlowType Type { get; init; }
    public required PinDirection Direction { get; init; }

    /// <summary>Pino de execução carrega ordem, não valor.</summary>
    public bool IsExec => Type == FlowType.Exec;

    public override string ToString() => $"{Name}:{Type}({Direction})";
}

/// <summary>Referência a um pino específico de um nó específico — a ponta de uma conexão.</summary>
public readonly record struct FlowEndpoint(string NodeId, string PinName)
{
    public override string ToString() => $"{NodeId}.{PinName}";
}
