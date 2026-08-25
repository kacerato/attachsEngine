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
/// de um mesmo arquétipo. O buffer é um <c>byte[]</c> simples: como o índice de um array gerenciado
/// é recalculado a cada acesso (não é um ponteiro fixo), não precisamos fixar (pin) nem nos
/// preocupar com o GC compactador mover o array entre uma chamada e outra.
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
        Buffer = new byte[bufferSize];
        _entities = new EntityId[archetype.ChunkCapacity];
        Versions = new int[archetype.Types.Length];
    }

    /// <summary>Ids das entidades ocupando as posições [0, Count) deste chunk, na mesma ordem
    /// usada pelos spans de componente devolvidos por <see cref="GetSpan{T}"/>.</summary>
    public ReadOnlySpan<EntityId> Entities => _entities.AsSpan(0, Count);

    internal ref EntityId EntityAt(int index) => ref _entities[index];

    /// <summary>Span, sem cópia, da coluna do componente <typeparamref name="T"/> para as
    /// entidades atualmente ocupadas neste chunk.</summary>
    public Span<T> GetSpan<T>() where T : unmanaged
    {
        int idx = Archetype.IndexOf(ComponentType.Of<T>());
        if (idx < 0)
            throw new InvalidOperationException($"O chunk não contém o componente {typeof(T).Name}.");
        return GetSpan<T>(idx);
    }

    internal Span<T> GetSpan<T>(int typeIndex) where T : unmanaged
    {
        int offset = Archetype.ColumnOffset(typeIndex);
        ref byte start = ref Buffer[offset];
        return MemoryMarshal.CreateSpan(ref Unsafe.As<byte, T>(ref start), Count);
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
