using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>
/// Identidade de tipo de componente em runtime: um id inteiro denso, o tamanho em bytes e o
/// alinhamento necessário para posicionar a coluna dentro do chunk. Componentes precisam ser
/// structs sem campos gerenciados (blittable) — o chunk é um <c>byte[]</c> cru e não há barreira
/// de GC para proteger uma referência guardada ali dentro.
/// </summary>
public readonly struct ComponentType : IEquatable<ComponentType>
{
    public readonly int Id;
    public readonly int Size;
    public readonly int Alignment;

    private ComponentType(int id, int size, int alignment) { Id = id; Size = size; Alignment = alignment; }

    /// <summary>
    /// Id denso + tamanho + alinhamento de <typeparamref name="T"/>, calculado uma única vez e
    /// cacheado num campo estático genérico. É a razão de <see cref="Of{T}"/> ser um acesso a
    /// campo (uma leitura, sem dicionário, sem lock) em vez de um lookup por <see cref="Type"/> —
    /// isso está no caminho quente de toda operação de ECS.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ComponentType Of<T>() where T : unmanaged => TypeIndex<T>.Type;

    private static int s_nextId;

    private static class TypeIndex<T> where T : unmanaged
    {
        public static readonly ComponentType Type = Create();

        private static ComponentType Create()
        {
            int id = Interlocked.Increment(ref s_nextId) - 1;
            int size = Unsafe.SizeOf<T>();
            // Heurística de alinhamento: sem refletir sobre os campos, assumimos que o maior
            // requisito de alinhamento de uma struct de dados de jogo (posições, ids, flags) é o do
            // seu maior campo primitivo comum, que nunca passa de 8 bytes (double/long). É segura
            // para todos os componentes usados nesta engine (float, int, EntityId, Transform, ...).
            int alignment = size >= 8 ? 8 : size >= 4 ? 4 : size >= 2 ? 2 : 1;
            return new ComponentType(id, size, alignment);
        }
    }

    public bool Equals(ComponentType o) => Id == o.Id;
    public override bool Equals(object? o) => o is ComponentType c && Equals(c);
    public override int GetHashCode() => Id;
    public static bool operator ==(ComponentType a, ComponentType b) => a.Equals(b);
    public static bool operator !=(ComponentType a, ComponentType b) => !a.Equals(b);
    public override string ToString() => $"ComponentType(#{Id}, {Size}b)";
}
