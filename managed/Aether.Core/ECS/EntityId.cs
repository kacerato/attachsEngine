using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>
/// Identidade de uma entidade: índice na tabela de slots do <see cref="World"/> mais uma versão.
/// A versão é incrementada toda vez que o slot é destruído, então um <see cref="EntityId"/> antigo
/// cujo índice foi reciclado por uma entidade nova nunca é confundido com ela — a comparação de
/// versão falha. 8 bytes, blittable, cabe em registrador.
/// </summary>
public readonly struct EntityId : IEquatable<EntityId>
{
    public readonly int Index;
    public readonly int Version;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public EntityId(int index, int version) { Index = index; Version = version; }

    /// <summary>Entidade inválida. Usa um índice fora do espaço de índices reais (que começam em 0),
    /// distinto também dos índices negativos temporários do <see cref="EntityCommandBuffer"/>.</summary>
    public static readonly EntityId Null = new(int.MinValue, 0);

    public bool IsNull => Index == int.MinValue;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public bool Equals(EntityId o) => Index == o.Index && Version == o.Version;
    public override bool Equals(object? o) => o is EntityId e && Equals(e);
    public override int GetHashCode() => HashCode.Combine(Index, Version);
    public static bool operator ==(EntityId a, EntityId b) => a.Equals(b);
    public static bool operator !=(EntityId a, EntityId b) => !a.Equals(b);
    public override string ToString() => IsNull ? "Entity(null)" : $"Entity({Index}#{Version})";
}
