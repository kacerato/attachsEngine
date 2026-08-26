using System.Collections.Concurrent;
using System.Diagnostics;

namespace Aether.Jobs;

/// <summary>
/// Classe de núcleo em que um job prefere rodar, pensando em ARM big.LITTLE:
/// latência crítica vai para núcleos grandes (Performance), import/compressão
/// vai para os pequenos (Efficiency) e o resto não importa (Any).
///
/// Isto é só a POLÍTICA: filas separadas por classe e contagem de workers
/// configurável por classe. Em .NET portátil não há como fixar de verdade uma
/// thread a um núcleo físico — o pinning real (sched_setaffinity / QoS class)
/// vem da camada nativa C++ quando a engine rodar no dispositivo. Não fingimos
/// pinning aqui.
/// </summary>
public enum CoreAffinity { Performance, Efficiency, Any }

/// <summary>Lançada por <see cref="JobSystem.Complete"/> quando o job (ou algum de seus pré-requisitos) lançou uma exceção.</summary>
public sealed class JobException : Exception
{
    public JobException(string message, Exception inner) : base(message, inner) { }
}

/// <summary>
/// Pool de workers com filas por classe de núcleo e work-stealing entre elas.
/// A thread chamadora de <see cref="Complete"/> ajuda a executar jobs pendentes
/// em vez de só bloquear, para não desperdiçar um núcleo esperando.
/// </summary>
public sealed class JobSystem : IDisposable
{
    // Watchdog externo: ciclos entre jobs são detectados pelo JobDependencyGraph antes
    // da espera. Este timeout permanece para trabalho externo que nunca retorna (I/O,
    // callback nativo travado etc.), que o grafo não tem como observar internamente.
    private static readonly TimeSpan DeadlockTimeout = TimeSpan.FromSeconds(3);

    private readonly ConcurrentQueue<JobEntry> _performanceQueue = new();
    private readonly ConcurrentQueue<JobEntry> _efficiencyQueue = new();
    private readonly ConcurrentQueue<JobEntry> _anyQueue = new();
    private readonly Thread[] _workers;
    private volatile bool _shuttingDown;

    [ThreadStatic] private static List<JobEntry>? _executionStack;

    public int WorkerCount => _workers.Length;

    /// <param name="performanceWorkers">Workers de núcleo "grande". -1 = automático a partir de <see cref="Environment.ProcessorCount"/>.</param>
    /// <param name="efficiencyWorkers">Workers de núcleo "pequeno" (import, compressão, etc).</param>
    public JobSystem(int performanceWorkers = -1, int efficiencyWorkers = 0)
    {
        if (efficiencyWorkers < 0) throw new ArgumentOutOfRangeException(nameof(efficiencyWorkers), "não pode ser negativo");
        if (performanceWorkers < 0) performanceWorkers = Math.Max(1, Environment.ProcessorCount - efficiencyWorkers);
        if (performanceWorkers + efficiencyWorkers <= 0)
            throw new ArgumentException("o sistema de jobs precisa de pelo menos 1 worker");

        int total = performanceWorkers + efficiencyWorkers;
        _workers = new Thread[total];
        for (int i = 0; i < total; i++)
        {
            var affinity = i < performanceWorkers ? CoreAffinity.Performance : CoreAffinity.Efficiency;
            var t = new Thread(() => WorkerLoop(affinity))
            {
                IsBackground = true,
                Name = $"Aether.Job.{affinity}.{i}",
            };
            _workers[i] = t;
            t.Start();
        }
    }

    private void WorkerLoop(CoreAffinity affinity)
    {
        var spin = new SpinWait();
        while (!_shuttingDown)
        {
            if (TryDequeueFor(affinity, out var entry))
            {
                RunEntry(entry);
                spin.Reset();
            }
            else
            {
                spin.SpinOnce();
            }
        }
    }

    private bool TryDequeueFor(CoreAffinity affinity, out JobEntry entry)
    {
        var primary = affinity == CoreAffinity.Performance ? _performanceQueue : _efficiencyQueue;
        if (primary.TryDequeue(out entry!)) return true;
        if (_anyQueue.TryDequeue(out entry!)) return true;

        // Work-stealing: a fila "certa" está vazia, mas a da outra classe pode ter
        // trabalho esperando — melhor roubar um job do que deixar o núcleo ocioso.
        var other = affinity == CoreAffinity.Performance ? _efficiencyQueue : _performanceQueue;
        if (other.TryDequeue(out entry!)) return true;

        entry = null!;
        return false;
    }

    private bool TryDequeueAny(out JobEntry entry)
    {
        if (_performanceQueue.TryDequeue(out entry!)) return true;
        if (_anyQueue.TryDequeue(out entry!)) return true;
        if (_efficiencyQueue.TryDequeue(out entry!)) return true;
        entry = null!;
        return false;
    }

    private void Enqueue(JobEntry entry)
    {
        switch (entry.Affinity)
        {
            case CoreAffinity.Performance: _performanceQueue.Enqueue(entry); break;
            case CoreAffinity.Efficiency: _efficiencyQueue.Enqueue(entry); break;
            default: _anyQueue.Enqueue(entry); break;
        }
    }

    private void RunEntry(JobEntry entry)
    {
        var stack = _executionStack ??= new List<JobEntry>();
        stack.Add(entry);
        try
        {
            // Uma exceção do job não pode derrubar o worker: capturamos aqui e
            // relançamos (empacotada, com contexto) em Complete().
            entry.Work?.Invoke();
        }
        catch (Exception ex)
        {
            entry.Fault = ex;
        }
        finally
        {
            stack.RemoveAt(stack.Count - 1);
            entry.FinishInline();
        }
    }

    public JobHandle Schedule(Action job, JobHandle dependsOn = default)
        => Schedule(job, CoreAffinity.Any, dependsOn);

    public JobHandle Schedule(Action job, CoreAffinity affinity, JobHandle dependsOn = default)
    {
        ArgumentNullException.ThrowIfNull(job);
        var entry = new JobEntry { Work = job, Affinity = affinity, EnqueueSelf = Enqueue };
        return ScheduleEntry(entry, dependsOn);
    }

    /// <summary>Divide [0, count) em lotes de até <paramref name="batchSize"/> e agenda um job por lote.</summary>
    public JobHandle ScheduleParallel(int count, int batchSize, Action<int, int> rangeJob, JobHandle dependsOn = default)
        => ScheduleParallel(count, batchSize, rangeJob, CoreAffinity.Any, dependsOn);

    public JobHandle ScheduleParallel(int count, int batchSize, Action<int, int> rangeJob, CoreAffinity affinity, JobHandle dependsOn = default)
    {
        ArgumentNullException.ThrowIfNull(rangeJob);
        if (count < 0) throw new ArgumentOutOfRangeException(nameof(count), "count não pode ser negativo");
        if (batchSize <= 0) throw new ArgumentOutOfRangeException(nameof(batchSize), "batchSize precisa ser positivo");
        if (count == 0) return default;

        int batchCount = (count + batchSize - 1) / batchSize;
        var handles = new JobHandle[batchCount];
        for (int b = 0; b < batchCount; b++)
        {
            int start = b * batchSize;
            int end = Math.Min(start + batchSize, count);
            handles[b] = Schedule(() => rangeJob(start, end), affinity, dependsOn);
        }
        return JobHandle.CombineDependencies(handles);
    }

    private JobHandle ScheduleEntry(JobEntry entry, JobHandle dependsOn)
    {
        var prereq = dependsOn.Entry;
        if (prereq is null)
        {
            Enqueue(entry);
            return new JobHandle(entry);
        }

        entry.RemainingDependencies = 1;
        JobDependencyGraph.SetPrerequisite(entry, prereq);
        bool prereqDone;
        lock (prereq.Lock)
        {
            prereqDone = prereq.Completed;
            if (!prereqDone) prereq.Dependents.Add(entry);
        }
        if (prereqDone) entry.NotifyPrerequisiteDone(prereq);
        return new JobHandle(entry);
    }

    /// <summary>
    /// Espera o job (e transitivamente suas dependências) terminar. A thread
    /// chamadora não fica só bloqueada: ela ajuda a esvaziar as filas enquanto
    /// espera, para não desperdiçar um núcleo.
    /// </summary>
    public void Complete(JobHandle handle)
    {
        if (!handle.IsValid) return;
        var entry = handle.Entry!;

        var stack = _executionStack ??= new List<JobEntry>();
        JobEntry? waiter = stack.Count == 0 ? null : stack[^1];
        bool waitRegistered = false;
        if (waiter is not null && !entry.Completed)
        {
            if (!JobDependencyGraph.TryBeginWait(waiter, entry, out string cyclePath))
                throw new InvalidOperationException(
                    $"ciclo de dependência detectado antes da espera: {cyclePath}");
            waitRegistered = true;
        }

        try
        {
            var sw = Stopwatch.StartNew();
            while (!entry.Completed)
            {
                if (TryDequeueAny(out var helped))
                {
                    RunEntry(helped);
                }
                else if (!entry.Done.Wait(2))
                {
                    if (sw.Elapsed > DeadlockTimeout)
                        throw new InvalidOperationException(
                            $"watchdog: job '{entry.DiagnosticName}' não terminou após {DeadlockTimeout.TotalSeconds:0}s sem nenhum trabalho disponível para ajudar");
                }
            }
        }
        finally
        {
            if (waitRegistered) JobDependencyGraph.EndWait(waiter!, entry);
        }

        if (entry.Fault is not null)
            throw new JobException($"job '{entry.DiagnosticName}' (ou uma de suas dependências) falhou durante a execução", entry.Fault);
    }

    public void Dispose()
    {
        if (_shuttingDown) return;
        _shuttingDown = true;
        foreach (var t in _workers) t.Join();
    }
}
