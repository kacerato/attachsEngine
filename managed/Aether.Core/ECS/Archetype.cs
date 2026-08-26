using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Aether;

/// <summary>
/// Um arquétipo: todas as entidades que têm exatamente o mesmo conjunto de tipos de componente,
/// armazenadas em <see cref="Chunk"/>s de layout SoA (structure of arrays) — os valores de um
/// mesmo componente ficam contíguos, para que sistemas iterem com acesso linear à memória.
/// </summary>
public sealed class Archetype
{
    /// <summary>Orçamento de bytes por chunk. 16 KB é um múltiplo pequeno da L1 dos núcleos
    /// "little" de um ARM big.LITTLE mobile — o chunk inteiro de uma iteração cabe perto do
    /// núcleo, e ainda sobra L1 para o resto do frame.</summary>
    public const int ChunkBytes = 16 * 1024;

    /// <summary>Alinhamento do início de cada chunk. Bate com o tamanho de linha de cache das
    /// CPUs ARM/x64 alvo, evitando que duas colunas de chunks vizinhos dividam a mesma linha.</summary>
    public const int ChunkAlignment = 64;

    public readonly ArchetypeSignature Signature;

    /// <summary>Tipos do arquétipo, em ordem canônica (por Id) — a mesma ordem usada para offsets
    /// de coluna, então dois arquétipos com os mesmos tipos sempre concordam em layout relativo.</summary>
    public readonly ComponentType[] Types;

    /// <summary>Quantas entidades cabem em um chunk deste arquétipo.</summary>
    public readonly int ChunkCapacity;

    private readonly int[] _columnOffsets;   // offset em bytes de cada coluna dentro do buffer do chunk
    private readonly int _bufferSize;        // tamanho total do buffer de um chunk, já alinhado

    public readonly List<Chunk> Chunks = new();

    public Archetype(ArchetypeSignature signature, ComponentType[] types)
    {
        Signature = signature;
        Types = (ComponentType[])types.Clone();
        Array.Sort(Types, (a, b) => a.Id.CompareTo(b.Id));

        int stride = 0;
        foreach (var t in Types) stride += t.Size;

        // Arquétipos "tag-only" (sem nenhum componente de dado) não têm coluna nenhuma; a
        // capacidade do chunk então só limita quantas entidades-tag cabem no array de EntityId.
        ChunkCapacity = stride > 0 ? Math.Max(1, ChunkBytes / stride) : 1024;

        _columnOffsets = new int[Types.Length];
        int cursor = 0;
        for (int i = 0; i < Types.Length; i++)
        {
            cursor = math.AlignUp(cursor, Types[i].Alignment);
            _columnOffsets[i] = cursor;
            cursor += Types[i].Size * ChunkCapacity;
        }
        _bufferSize = math.AlignUp(cursor, ChunkAlignment);
    }

    public int IndexOf(ComponentType t)
    {
        for (int i = 0; i < Types.Length; i++)
            if (Types[i].Id == t.Id) return i;
        return -1;
    }

    internal int ColumnOffset(int typeIndex) => _columnOffsets[typeIndex];

    internal Chunk CreateChunk() => new(this, _bufferSize);

    /// <summary>Devolve um chunk com espaço livre, criando um novo se todos estiverem cheios.</summary>
    internal Chunk GetChunkWithRoom()
    {
        if (Chunks.Count > 0)
        {
            var last = Chunks[^1];
            if (last.Count < ChunkCapacity) return last;
        }
        var chunk = CreateChunk();
        Chunks.Add(chunk);
        return chunk;
    }
}

/// <summary>
/// Um bloco contíguo de armazenamento SoA para até <see cref="Archetype.ChunkCapacity"/> entidades
/// de um mesmo arquétipo. O buffer é um <c>byte[]</c> alocado uma vez no Pinned Object Heap: spans
/// gerenciados continuam sendo a API normal, mas kernels síncronos em lote podem guardar endereços
/// de colunas no plano sem fixar centenas de chunks a cada frame. Chunks têm lifecycle longo e
/// tamanho limitado, o caso apropriado para memória pinada estável.
/// </summary>
public sealed class Chunk
{
    internal readonly Archetype Archetype;
    internal readonly byte[] Buffer;
    private readonly EntityId[] _entities;
    internal readonly int[] Versions;   // versão por coluna, incrementada a cada escrita — base de change detection

    public int Count { get; internal set; }
    public int Capacity => Archetype.ChunkCapacity;

    internal Chunk(Archetype archetype, int bufferSize)
    {
        Archetype = archetype;
        Buffer = GC.AllocateUninitializedArray<byte>(bufferSize, pinned: true);
        _entities = new EntityId[archetype.ChunkCapacity];
        Versions = new int[archetype.Types.Length];
    }

    /// <summary>Ids das entidades ocupando as posições [0, Count) deste chunk, na mesma ordem
    /// usada pelos spans de componente.</summary>
    public ReadOnlySpan<EntityId> Entities => _entities.AsSpan(0, Count);

    internal ref EntityId EntityAt(int index) => ref _entities[index];

    /// <summary>Visão somente leitura, sem cópia, da coluna do componente. Obter esta visão não
    /// altera a versão usada por change detection.</summary>
    public ReadOnlySpan<T> GetReadOnlySpan<T>() where T : unmanaged
    {
        int idx = RequireTypeIndex<T>();
        return GetReadOnlySpan<T>(idx);
    }

    /// <summary>Visão mutável, sem cópia, da coluna do componente. A versão da coluna avança
    /// exatamente uma vez ao obter o span, independentemente de quantos elementos forem escritos
    /// durante esse acesso lógico.</summary>
    public Span<T> GetWritableSpan<T>() where T : unmanaged
    {
        int idx = RequireTypeIndex<T>();
        MarkChanged(idx);
        return GetSpanUnchecked<T>(idx);
    }

    /// <summary>API de compatibilidade conservadora. Código novo deve declarar a intenção com
    /// <see cref="GetReadOnlySpan{T}"/> ou <see cref="GetWritableSpan{T}"/>.</summary>
    [Obsolete("GetSpan<T>() é conservador e marca escrita. Use GetReadOnlySpan<T>() ou GetWritableSpan<T>().")]
    public Span<T> GetSpan<T>() where T : unmanaged
    {
        return GetWritableSpan<T>();
    }

    /// <summary>Versão atual da coluna. Leitores podem guardar este valor e comparar por igualdade
    /// para decidir se precisam reprocessar o chunk.</summary>
    public int GetChangeVersion<T>() where T : unmanaged => Versions[RequireTypeIndex<T>()];

    private int RequireTypeIndex<T>() where T : unmanaged
    {
        int idx = Archetype.IndexOf(ComponentType.Of<T>());
        if (idx < 0)
            throw new InvalidOperationException($"O chunk não contém o componente {typeof(T).Name}.");
        return idx;
    }

    internal ReadOnlySpan<T> GetReadOnlySpan<T>(int typeIndex) where T : unmanaged =>
        GetSpanUnchecked<T>(typeIndex);

    internal Span<T> GetWritableSpan<T>(int typeIndex) where T : unmanaged
    {
        MarkChanged(typeIndex);
        return GetSpanUnchecked<T>(typeIndex);
    }

    /// <summary>Acesso direto usado por planos compilados que já validaram tipo, linha e versão
    /// estrutural. Evita reconstruir um Span e repetir lookup por entidade no caminho quente.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ref readonly T GetReadOnlyRefUnchecked<T>(int typeIndex, int row) where T : unmanaged =>
        ref GetRefUnchecked<T>(typeIndex, row);

    /// <summary>Versão mutável sem marcar dirty. O sistema chamador deve chamar
    /// <see cref="MarkChanged"/> uma vez por coluna/chunk antes de escrever o lote.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal ref T GetWritableRefUnchecked<T>(int typeIndex, int row) where T : unmanaged =>
        ref GetRefUnchecked<T>(typeIndex, row);

    /// <summary>Endereço estável de um componente dentro do buffer pinado. Somente planos nativos
    /// síncronos podem conservá-lo, e apenas enquanto mantiverem este Chunk vivo.</summary>
    internal unsafe nint GetAddressUnchecked<T>(int typeIndex, int row) where T : unmanaged
    {
        int offset = Archetype.ColumnOffset(typeIndex) + row * Unsafe.SizeOf<T>();
        fixed (byte* buffer = Buffer) return (nint)(buffer + offset);
    }

    private Span<T> GetSpanUnchecked<T>(int typeIndex) where T : unmanaged
    {
        int offset = Archetype.ColumnOffset(typeIndex);
        ref byte start = ref Buffer[offset];
        return MemoryMarshal.CreateSpan(ref Unsafe.As<byte, T>(ref start), Count);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private ref T GetRefUnchecked<T>(int typeIndex, int row) where T : unmanaged
    {
        int offset = Archetype.ColumnOffset(typeIndex) + row * Unsafe.SizeOf<T>();
        return ref Unsafe.As<byte, T>(ref Buffer[offset]);
    }

    internal void MarkChanged(int typeIndex) => Versions[typeIndex]++;

    /// <summary>Copia os bytes crus do componente de tipo <paramref name="type"/> de uma linha
    /// para outra, dentro do mesmo chunk ou entre chunks (usado por migração de arquétipo e por
    /// swap-back na remoção). Seguro porque todo componente é <c>unmanaged</c> — não há referência
    /// de GC para corromper copiando bytes diretamente.</summary>
    internal static void CopyRaw(Chunk src, int srcRow, Chunk dst, int dstRow, ComponentType type)
    {
        int srcIdx = src.Archetype.IndexOf(type);
        int dstIdx = dst.Archetype.IndexOf(type);
        if (srcIdx < 0 || dstIdx < 0) return;
        int size = type.Size;
        int srcOff = src.Archetype.ColumnOffset(srcIdx) + srcRow * size;
        int dstOff = dst.Archetype.ColumnOffset(dstIdx) + dstRow * size;
        System.Array.Copy(src.Buffer, srcOff, dst.Buffer, dstOff, size);
        dst.MarkChanged(dstIdx);
    }
}
