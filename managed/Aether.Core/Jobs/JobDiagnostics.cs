namespace Aether.Jobs;

/// <summary>Instantâneo somente-leitura de um job já concluído — a metade pública que
/// <see cref="JobDiagnostics.Snapshot"/> devolve, sem vazar o tipo interno <c>JobEntry</c>.</summary>
public readonly struct JobRecord
{
    public readonly string Label;
    public readonly long Id;
    public readonly double ExecutionMilliseconds;
    public readonly long AllocatedBytes;
    public readonly bool Faulted;

    internal JobRecord(string label, long id, double executionMilliseconds, long allocatedBytes, bool faulted)
    {
        Label = label;
        Id = id;
        ExecutionMilliseconds = executionMilliseconds;
        AllocatedBytes = allocatedBytes;
        Faulted = faulted;
    }
}

/// <summary>Total agregado por rótulo — quantos jobs desse label já rodaram, tempo total,
/// alocação total. Útil para responder "que categoria de job está pesando no frame" sem
/// precisar somar registros individuais toda vez.</summary>
public struct JobLabelTotal
{
    public string Label;
    public int Count;
    public double TotalExecutionMilliseconds;
    public long TotalAllocatedBytes;

    public readonly double AverageExecutionMilliseconds => Count == 0 ? 0.0 : TotalExecutionMilliseconds / Count;
}

/// <summary>
/// Item 1.2.4 (visualizador de jobs): mantém os últimos <see cref="_capacity"/> jobs concluídos
/// num buffer circular (memória limitada, não cresce sem fim — um processo de longa duração não
/// pode vazar um registro por job para sempre) mais totais agregados por <c>Label</c> desde o
/// início do processo (esses sim acumulam indefinidamente, mas são poucos: um por categoria de
/// job usada, não um por job individual).
/// <para>
/// Thread-safe: <c>JobSystem.RunEntry</c> roda em qualquer worker, então <see cref="Record"/>
/// pode ser chamado concorrentemente por várias threads — protegido por um lock simples (não é
/// caminho quente do PRÓPRIO trabalho do job, só da contabilidade depois que ele termina, uma
/// vez por job — o custo de um lock aqui é desprezível comparado ao trabalho real do job).
/// </para>
/// </summary>
public sealed class JobDiagnostics
{
    private readonly object _lock = new();
    private readonly JobRecord[] _ring;
    private int _writeIndex;
    private int _count;
    private readonly Dictionary<string, JobLabelTotal> _totals = new();

    public JobDiagnostics(int capacity = 256)
    {
        if (capacity <= 0) throw new ArgumentOutOfRangeException(nameof(capacity), "capacidade precisa ser positiva");
        _ring = new JobRecord[capacity];
    }

    internal void Record(JobEntry entry)
    {
        double milliseconds = entry.ExecutionTicks * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        var record = new JobRecord(entry.Label, entry.Id, milliseconds, entry.AllocatedBytes, entry.Fault is not null);

        lock (_lock)
        {
            _ring[_writeIndex] = record;
            _writeIndex = (_writeIndex + 1) % _ring.Length;
            if (_count < _ring.Length) _count++;

            ref var total = ref System.Runtime.InteropServices.CollectionsMarshal.GetValueRefOrAddDefault(
                _totals, entry.Label, out bool exists);
            if (!exists) total.Label = entry.Label;
            total.Count++;
            total.TotalExecutionMilliseconds += milliseconds;
            total.TotalAllocatedBytes += entry.AllocatedBytes;
        }
    }

    /// <summary>Copia os últimos jobs concluídos (mais recente por último) para
    /// <paramref name="destination"/>. Devolve quantos registros foram escritos — pode ser menor
    /// que <paramref name="destination"/>.Length se ainda não há jobs suficientes.</summary>
    public int CopyRecentTo(Span<JobRecord> destination)
    {
        lock (_lock)
        {
            int toCopy = Math.Min(_count, destination.Length);
            // O buffer é circular: o mais antigo ainda presente está em (_writeIndex - _count),
            // o mais recente em (_writeIndex - 1) — reconstrói a ordem cronológica.
            int start = (_writeIndex - _count + _ring.Length) % _ring.Length;
            for (int i = 0; i < toCopy; i++)
            {
                int sourceIndex = (start + (_count - toCopy) + i) % _ring.Length;
                destination[i] = _ring[sourceIndex];
            }
            return toCopy;
        }
    }

    /// <summary>Totais agregados por label desde o início do processo (ou desde a última vez
    /// que <see cref="JobDiagnostics"/> foi criado). Ordem não é garantida.</summary>
    public JobLabelTotal[] SnapshotTotals()
    {
        lock (_lock)
        {
            var result = new JobLabelTotal[_totals.Count];
            _totals.Values.CopyTo(result, 0);
            return result;
        }
    }
}
