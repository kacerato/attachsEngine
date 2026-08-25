using System.Runtime.CompilerServices;

namespace Aether.Resources;

/// <summary>
/// Identidade estável de um recurso (textura, malha, cena, ...), independente de onde o arquivo
/// mora em disco ou de como o projeto foi reorganizado — mover/renomear um asset não pode quebrar
/// quem referencia o `ResourceId` dele. Usamos <see cref="Guid"/> em vez de um índice denso (como
/// <see cref="Aether.ComponentType"/>) porque este id precisa sobreviver a serialização em disco e
/// a merges entre sessões de edição diferentes; <see cref="Guid"/> é blittable, do BCL, e não traz
/// dependência externa nenhuma.
/// </summary>
public readonly struct ResourceId : IEquatable<ResourceId>
{
    public readonly Guid Value;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ResourceId(Guid value) => Value = value;

    /// <summary>Gera um novo id globalmente único para um recurso recém-criado.</summary>
    public static ResourceId New() => new(Guid.NewGuid());

    /// <summary>Id "vazio": nenhum recurso. É o valor de <c>default(ResourceId)</c>.</summary>
    public static readonly ResourceId Empty = new(Guid.Empty);

    /// <summary>Sinônimo de <see cref="Empty"/> — mesmo valor, nome que lê melhor em código de
    /// consumo ("nenhuma referência" em vez de "referência vazia").</summary>
    public static readonly ResourceId None = Empty;

    public bool IsEmpty => Value == Guid.Empty;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public bool Equals(ResourceId other) => Value.Equals(other.Value);
    public override bool Equals(object? obj) => obj is ResourceId id && Equals(id);
    public override int GetHashCode() => Value.GetHashCode();
    public static bool operator ==(ResourceId a, ResourceId b) => a.Equals(b);
    public static bool operator !=(ResourceId a, ResourceId b) => !a.Equals(b);
    public override string ToString() => IsEmpty ? "ResourceId(none)" : $"ResourceId({Value:D})";
}
