namespace Aether.Flow.Validation;

public enum Severity { Aviso, Erro }

/// <summary>Um problema encontrado no grafo. A mensagem é sempre em português e
/// escrita para alguém que nunca programou — sem jargão técnico.</summary>
public sealed record FlowDiagnostic(Severity Severity, string Message, string? NodeId)
{
    public override string ToString() => $"[{Severity}] {Message}" + (NodeId is null ? "" : $" (nó {NodeId})");
}
