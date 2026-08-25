using System.Runtime.InteropServices;

namespace Aether;

/// <summary>
/// Alocador linear sobre memória não-gerenciada. É o destino de todo dado
/// temporário de frame (listas de cull, comandos de draw): <see cref="Alloc{T}"/>
/// é só um avanço de ponteiro e <see cref="Reset"/> é O(1) — não existe free
/// individual, então não há fragmentação nem pressão sobre o GC no caminho quente.
/// Cresce por blocos encadeados quando o bloco atual estoura, em vez de falhar.
/// </summary>
public sealed unsafe class FrameArena : IDisposable
{
    private struct Block
    {
        public byte* Ptr;
        public nuint Size;
    }

    private const byte DebugFillPattern = 0xDD; // padrão reconhecível pra pegar uso-após-reset em debug

    private Block[] _blocks;
    private int _blockCount;
    private int _currentBlock;
    private nuint _cursor;
    private nuint _bytesUsedThisCycle;
    private nuint _peakBytes;
    private readonly nuint _defaultBlockSize;
    private readonly nuint _alignment;
    private bool _disposed;

    /// <summary>Marca d'água: maior quantidade de bytes em uso simultaneamente, através de todos os ciclos.</summary>
    public nuint PeakBytes => _peakBytes;

    /// <summary>Quantos blocos nativos foram criados até agora (1 = nunca precisou crescer).</summary>
    public int BlockCount => _blockCount;

#if DEBUG
    /// <summary>Em debug, Reset() preenche a memória devolvida com um padrão para pegar uso-após-reset.</summary>
    public bool DebugFillOnReset { get; set; } = true;
#endif

    public FrameArena(nuint blockSize = 1 << 20, nuint alignment = 16)
    {
        if (blockSize == 0) throw new ArgumentOutOfRangeException(nameof(blockSize), "tamanho de bloco precisa ser maior que zero");
        if (!IsPowerOfTwo(alignment)) throw new ArgumentException("alinhamento precisa ser uma potência de dois", nameof(alignment));

        _defaultBlockSize = blockSize;
        _alignment = alignment;
        _blocks = new Block[4];
        AddBlock(blockSize);
    }

    private static bool IsPowerOfTwo(nuint v) => v != 0 && (v & (v - 1)) == 0;

    // Mesma lógica de math.AlignUp, só que em nuint: o alocador nativo precisa
    // trabalhar com tamanhos que podem passar do intervalo de int.
    private static nuint AlignUpNuint(nuint value, nuint alignment) => (value + alignment - 1) & ~(alignment - 1);

    private void AddBlock(nuint size)
    {
        if (_blockCount == _blocks.Length) Array.Resize(ref _blocks, _blocks.Length * 2);
        byte* ptr = (byte*)NativeMemory.AlignedAlloc(size, _alignment);
        _blocks[_blockCount++] = new Block { Ptr = ptr, Size = size };
    }

    /// <summary>Aloca <paramref name="count"/> elementos de <typeparamref name="T"/>, alinhados, sem inicialização.</summary>
    public Span<T> Alloc<T>(int count) where T : unmanaged
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (count < 0) throw new ArgumentOutOfRangeException(nameof(count), "contagem não pode ser negativa");
        if (count == 0) return Span<T>.Empty;

        nuint size = (nuint)((long)sizeof(T) * count);
        ref Block block = ref _blocks[_currentBlock];
        nuint alignedCursor = AlignUpNuint(_cursor, _alignment);

        if (alignedCursor + size > block.Size)
        {
            // O bloco atual não comporta: avança para o próximo bloco já existente
            // ou encadeia um novo (do tamanho padrão, ou maior se a alocação pedir mais).
            _currentBlock++;
            if (_currentBlock == _blockCount)
            {
                nuint newSize = size > _defaultBlockSize ? size : _defaultBlockSize;
                AddBlock(newSize);
            }
            _cursor = 0;
            return Alloc<T>(count);
        }

        byte* result = block.Ptr + alignedCursor;
        nuint padding = alignedCursor - _cursor;
        _cursor = alignedCursor + size;

        _bytesUsedThisCycle += padding + size;
        if (_bytesUsedThisCycle > _peakBytes) _peakBytes = _bytesUsedThisCycle;

        return new Span<T>(result, count);
    }

    /// <summary>Devolve o cursor ao início do primeiro bloco. Custo O(1): não há free individual.</summary>
    public void Reset()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
#if DEBUG
        if (DebugFillOnReset)
        {
            for (int i = 0; i <= _currentBlock; i++)
            {
                nuint fillLen = i == _currentBlock ? _cursor : _blocks[i].Size;
                if (fillLen > 0) NativeMemory.Fill(_blocks[i].Ptr, fillLen, DebugFillPattern);
            }
        }
#endif
        _currentBlock = 0;
        _cursor = 0;
        _bytesUsedThisCycle = 0;
    }

    public void Dispose()
    {
        if (_disposed) return;
        for (int i = 0; i < _blockCount; i++) NativeMemory.AlignedFree(_blocks[i].Ptr);
        _blockCount = 0;
        _disposed = true;
    }
}
