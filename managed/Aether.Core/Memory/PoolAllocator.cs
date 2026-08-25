using System.Runtime.InteropServices;

namespace Aether;

/// <summary>
/// Pool de blocos de tamanho fixo com free-list, para objetos de vida longa.
/// Ao contrário do <see cref="FrameArena"/>, os ponteiros entregues por
/// <see cref="Rent"/> permanecem válidos para sempre (até o <see cref="Return"/>
/// correspondente): o pool nunca realoca o que já foi entregue, só encadeia
/// blocos novos quando a free-list esvazia.
/// </summary>
public sealed unsafe class PoolAllocator<T> : IDisposable where T : unmanaged
{
    private const nuint ChunkAlignment = 16;

    private struct Chunk { public byte* Ptr; }

    private readonly int _itemsPerChunk;
    private readonly List<Chunk> _chunks = new();
    private readonly List<int> _freeList = new();
    private readonly List<bool> _rented = new(); // usado só para detectar double-free; não é caminho quente
    private bool _disposed;

    /// <summary>Quantos itens estão emprestados agora.</summary>
    public int RentedCount { get; private set; }

    /// <summary>Capacidade total já alocada (emprestada ou não).</summary>
    public int Capacity => _chunks.Count * _itemsPerChunk;

    public PoolAllocator(int itemsPerChunk = 256)
    {
        if (itemsPerChunk <= 0) throw new ArgumentOutOfRangeException(nameof(itemsPerChunk), "itemsPerChunk precisa ser positivo");
        _itemsPerChunk = itemsPerChunk;
        GrowByOneChunk();
    }

    private void GrowByOneChunk()
    {
        byte* ptr = (byte*)NativeMemory.AlignedAlloc((nuint)((long)sizeof(T) * _itemsPerChunk), ChunkAlignment);
        int baseIndex = _chunks.Count * _itemsPerChunk;
        _chunks.Add(new Chunk { Ptr = ptr });
        // Devolve os índices em ordem decrescente para que Rent() consuma em ordem crescente
        // (só estética de depuração — não afeta corretude).
        for (int i = _itemsPerChunk - 1; i >= 0; i--)
        {
            _freeList.Add(baseIndex + i);
            _rented.Add(false);
        }
    }

    /// <summary>Empresta um slot do pool. Cresce em um novo bloco se a free-list estiver vazia.</summary>
    public int Rent(out T* pointer)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_freeList.Count == 0) GrowByOneChunk();

        int index = _freeList[^1];
        _freeList.RemoveAt(_freeList.Count - 1);
        _rented[index] = true;
        RentedCount++;
        pointer = GetPointer(index);
        return index;
    }

    /// <summary>Ponteiro estável para o índice dado. Continua válido mesmo depois de o pool crescer.</summary>
    public T* GetPointer(int index)
    {
        if ((uint)index >= (uint)_rented.Count)
            throw new IndexOutOfRangeException($"índice {index} fora do intervalo do pool (capacidade {_rented.Count})");
        int chunkIndex = index / _itemsPerChunk;
        int offset = index % _itemsPerChunk;
        return (T*)_chunks[chunkIndex].Ptr + offset;
    }

    /// <summary>Devolve um slot ao pool. Lança em double-free (build de debug e release).</summary>
    public void Return(int index)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if ((uint)index >= (uint)_rented.Count)
            throw new IndexOutOfRangeException($"índice {index} fora do intervalo do pool (capacidade {_rented.Count})");
        if (!_rented[index])
            throw new InvalidOperationException($"double-free detectado: índice {index} já foi devolvido ao pool");

        _rented[index] = false;
        RentedCount--;
        _freeList.Add(index);
    }

    public void Dispose()
    {
        if (_disposed) return;
        foreach (var c in _chunks) NativeMemory.AlignedFree(c.Ptr);
        _chunks.Clear();
        _disposed = true;
    }
}
