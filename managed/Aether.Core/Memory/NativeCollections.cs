using System.Runtime.InteropServices;

namespace Aether;

/// <summary>
/// Array de tamanho fixo sobre memória não-gerenciada. Sem realocação, sem GC:
/// serve de base para buffers de vida longa cujo tamanho é conhecido de antemão.
/// </summary>
public unsafe struct NativeArray<T> : IDisposable where T : unmanaged
{
    private T* _ptr;
    private int _length;

    public readonly int Length => _length;

    public NativeArray(int length)
    {
        if (length < 0) throw new ArgumentOutOfRangeException(nameof(length), "tamanho não pode ser negativo");
        _length = length;
        _ptr = length == 0 ? null : (T*)NativeMemory.AlignedAlloc((nuint)((long)sizeof(T) * length), 16);
    }

    public readonly ref T this[int index]
    {
        get
        {
            if ((uint)index >= (uint)_length)
                throw new IndexOutOfRangeException($"índice {index} fora do intervalo [0, {_length})");
            return ref _ptr[index];
        }
    }

    public readonly Span<T> AsSpan() => _length == 0 ? Span<T>.Empty : new Span<T>(_ptr, _length);

    public void Dispose()
    {
        if (_ptr == null) return;
        NativeMemory.AlignedFree(_ptr);
        _ptr = null;
        _length = 0;
    }
}

/// <summary>
/// Lista dinâmica sobre memória não-gerenciada, com crescimento por dobra.
/// Equivalente não-gerenciado de <see cref="List{T}"/> para dados que precisam
/// atravessar a fronteira nativa ou viver fora do heap do GC.
/// </summary>
public unsafe struct NativeList<T> : IDisposable where T : unmanaged
{
    private T* _ptr;
    private int _capacity;
    private int _count;

    public readonly int Count => _count;
    public readonly int Capacity => _capacity;

    public NativeList(int initialCapacity = 4)
    {
        if (initialCapacity < 0) throw new ArgumentOutOfRangeException(nameof(initialCapacity), "capacidade não pode ser negativa");
        _capacity = initialCapacity;
        _count = 0;
        _ptr = _capacity == 0 ? null : (T*)NativeMemory.AlignedAlloc((nuint)((long)sizeof(T) * _capacity), 16);
    }

    public void Add(T value)
    {
        if (_count == _capacity) Grow();
        _ptr[_count++] = value;
    }

    private void Grow()
    {
        int newCapacity = _capacity == 0 ? 4 : _capacity * 2;
        var newPtr = (T*)NativeMemory.AlignedAlloc((nuint)((long)sizeof(T) * newCapacity), 16);
        if (_ptr != null)
        {
            new Span<T>(_ptr, _count).CopyTo(new Span<T>(newPtr, newCapacity));
            NativeMemory.AlignedFree(_ptr);
        }
        _ptr = newPtr;
        _capacity = newCapacity;
    }

    public readonly ref T this[int index]
    {
        get
        {
            if ((uint)index >= (uint)_count)
                throw new IndexOutOfRangeException($"índice {index} fora do intervalo [0, {_count})");
            return ref _ptr[index];
        }
    }

    public void Clear() => _count = 0;

    public readonly Span<T> AsSpan() => _count == 0 ? Span<T>.Empty : new Span<T>(_ptr, _count);

    public void Dispose()
    {
        if (_ptr == null) return;
        NativeMemory.AlignedFree(_ptr);
        _ptr = null;
        _capacity = 0;
        _count = 0;
    }
}
