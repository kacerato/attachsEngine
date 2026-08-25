namespace Aether;

/// <summary>Categorias de memória rastreadas pelo editor (parte 4.1 do plano).</summary>
public enum MemoryCategory
{
    Code,
    EditorUI,
    Scene,
    TexturesGpu,
    MeshesGpu,
    RenderTargets,
    AssetCache,
    UndoHistory,
}

/// <summary>Instantâneo de uma categoria que acabou de cruzar seu limiar de alerta.</summary>
public readonly struct MemoryThresholdEvent
{
    public required MemoryCategory Category { get; init; }
    public required long UsedBytes { get; init; }
    public required long LimitBytes { get; init; }
}

/// <summary>
/// Orçamento explícito de memória por categoria. Alimenta indicadores do editor
/// como "Memória de texturas: 420/500 MB" e dispara um callback quando uma
/// categoria cruza seu limiar — uma vez por cruzamento, não a cada byte alocado.
/// </summary>
public sealed class MemoryBudget
{
    private static readonly int CategoryCount = Enum.GetValues<MemoryCategory>().Length;

    private readonly long[] _limits = new long[CategoryCount];
    private readonly long[] _used = new long[CategoryCount];
    private readonly float[] _thresholdFraction = new float[CategoryCount];
    private readonly bool[] _overThreshold = new bool[CategoryCount];

    /// <summary>Disparado quando uma categoria cruza seu limiar de alerta (de baixo para cima).</summary>
    public event Action<MemoryThresholdEvent>? ThresholdCrossed;

    /// <summary>Define o limite (em bytes) de uma categoria e a fração dele que dispara o alerta.</summary>
    public void SetLimit(MemoryCategory category, long limitBytes, float thresholdFraction = 0.9f)
    {
        if (limitBytes < 0) throw new ArgumentOutOfRangeException(nameof(limitBytes), "limite não pode ser negativo");
        if (thresholdFraction is <= 0f or > 1f) throw new ArgumentOutOfRangeException(nameof(thresholdFraction), "fração de limiar precisa estar em (0, 1]");
        _limits[(int)category] = limitBytes;
        _thresholdFraction[(int)category] = thresholdFraction;
    }

    public void Allocate(MemoryCategory category, long bytes)
    {
        if (bytes < 0) throw new ArgumentOutOfRangeException(nameof(bytes), "quantidade não pode ser negativa");
        int i = (int)category;
        _used[i] += bytes;
        CheckThreshold(i);
    }

    public void Free(MemoryCategory category, long bytes)
    {
        if (bytes < 0) throw new ArgumentOutOfRangeException(nameof(bytes), "quantidade não pode ser negativa");
        int i = (int)category;
        _used[i] -= bytes;
        if (_used[i] < 0) _used[i] = 0; // defensivo: contabilidade externa nunca deve deixar o total negativo
        CheckThreshold(i);
    }

    public long Used(MemoryCategory category) => _used[(int)category];
    public long Limit(MemoryCategory category) => _limits[(int)category];

    /// <summary>Quanto ainda cabe na categoria antes de estourar o limite (nunca negativo).</summary>
    public long Remaining(MemoryCategory category)
    {
        int i = (int)category;
        long remaining = _limits[i] - _used[i];
        return remaining > 0 ? remaining : 0;
    }

    private void CheckThreshold(int i)
    {
        if (_limits[i] <= 0) return; // sem limite configurado: nada a vigiar

        bool over = _used[i] >= _limits[i] * _thresholdFraction[i];
        if (over && !_overThreshold[i])
        {
            _overThreshold[i] = true;
            ThresholdCrossed?.Invoke(new MemoryThresholdEvent
            {
                Category = (MemoryCategory)i,
                UsedBytes = _used[i],
                LimitBytes = _limits[i],
            });
        }
        else if (!over)
        {
            // Recuou pra baixo do limiar: um novo cruzamento no futuro deve disparar de novo.
            _overThreshold[i] = false;
        }
    }
}
