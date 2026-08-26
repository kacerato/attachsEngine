namespace Aether.Jobs;

/// <summary>
/// Estado interno de um job agendado: seu trabalho, suas dependências pendentes
/// e a lista de quem depende dele. Não é API pública — <see cref="JobHandle"/> é
/// a fachada que o código de fora enxerga.
/// </summary>
internal sealed class JobEntry
{
    private static long s_nextId;

    /// <summary>Identidade somente de diagnóstico; nunca é serializada nem reutilizada.</summary>
    public readonly long Id = Interlocked.Increment(ref s_nextId);

    /// <summary>Nulo para nós de junção criados por <see cref="JobHandle.CombineDependencies"/>: não há trabalho a rodar, só propagação.</summary>
    public Action? Work;

    public CoreAffinity Affinity;

    /// <summary>Quantas dependências ainda faltam concluir. Decrementado com <see cref="Interlocked"/>.</summary>
    public int RemainingDependencies;

    /// <summary>Jobs que esperam este terminar. Protegida por <see cref="Lock"/>.</summary>
    public readonly List<JobEntry> Dependents = new();

    /// <summary>
    /// Arestas imutáveis deste job para os pré-requisitos que precisam terminar antes dele.
    /// Escritas somente durante o agendamento, sob <see cref="JobDependencyGraph.Lock"/>.
    /// </summary>
    public readonly List<JobEntry> Prerequisites = new();

    /// <summary>
    /// Aresta temporária criada quando este job chama <c>Complete(outro)</c>. Protegida pelo
    /// lock global do grafo para que duas threads não consigam publicar as duas metades de um
    /// ciclo simultaneamente sem que pelo menos uma o detecte.
    /// </summary>
    public JobEntry? WaitingOn;

    public readonly object Lock = new();

    public volatile bool Completed;

    public Exception? Fault;

    public readonly ManualResetEventSlim Done = new(false);

    /// <summary>
    /// Preenchido pelo <see cref="JobSystem"/> que agendou este job: como colocá-lo
    /// de volta numa fila de execução quando suas dependências terminarem. Nulo
    /// para nós de junção, que não têm trabalho a enfileirar.
    /// </summary>
    public Action<JobEntry>? EnqueueSelf;

    public string Label = "job";

    public string DiagnosticName => $"{Label}#{Id}";

    /// <summary>
    /// Chamado por um pré-requisito que acabou de terminar. Propaga a falha (se
    /// houver) e, ao chegar a zero dependências pendentes, enfileira este job
    /// (ou, se for um nó de junção sem trabalho, conclui e propaga na hora).
    /// </summary>
    public void NotifyPrerequisiteDone(JobEntry prerequisite)
    {
        if (prerequisite.Fault is not null)
            Interlocked.CompareExchange(ref Fault, prerequisite.Fault, null);

        if (Interlocked.Decrement(ref RemainingDependencies) == 0)
        {
            if (EnqueueSelf is not null) EnqueueSelf(this);
            else FinishInline();
        }
    }

    /// <summary>Marca como concluído, acorda quem estiver esperando e notifica os dependentes.</summary>
    public void FinishInline()
    {
        JobEntry[] deps;
        lock (Lock)
        {
            Completed = true;
            deps = Dependents.Count == 0 ? Array.Empty<JobEntry>() : Dependents.ToArray();
        }
        Done.Set();
        foreach (var d in deps) d.NotifyPrerequisiteDone(this);
    }
}

/// <summary>
/// Grafo explícito das relações que podem bloquear jobs. Pré-requisitos são permanentes;
/// esperas de <c>Complete()</c> existem somente enquanto a chamada está ativa. O lock global
/// aparece apenas em agendamento/espera, nunca na execução do trabalho do job.
/// </summary>
internal static class JobDependencyGraph
{
    internal static readonly object Lock = new();

    public static void SetPrerequisites(JobEntry entry, IReadOnlyList<JobEntry> prerequisites)
    {
        lock (Lock)
        {
            entry.Prerequisites.AddRange(prerequisites);
        }
    }

    public static void SetPrerequisite(JobEntry entry, JobEntry prerequisite)
    {
        lock (Lock)
        {
            entry.Prerequisites.Add(prerequisite);
        }
    }

    /// <summary>
    /// Publica <paramref name="waiter"/>→<paramref name="target"/> e procura, de forma atômica,
    /// um caminho target→waiter. Se existir, a nova aresta fecharia um ciclo e é removida antes
    /// de devolver a mensagem diagnóstica.
    /// </summary>
    public static bool TryBeginWait(JobEntry waiter, JobEntry target, out string cyclePath)
    {
        lock (Lock)
        {
            waiter.WaitingOn = target;
            if (TryFindPath(target, waiter, out var path))
            {
                waiter.WaitingOn = null;
                cyclePath = string.Join(" -> ",
                    new[] { waiter.DiagnosticName }.Concat(path.Select(e => e.DiagnosticName)));
                return false;
            }
        }

        cyclePath = string.Empty;
        return true;
    }

    public static void EndWait(JobEntry waiter, JobEntry target)
    {
        lock (Lock)
        {
            if (ReferenceEquals(waiter.WaitingOn, target)) waiter.WaitingOn = null;
        }
    }

    private static bool TryFindPath(JobEntry start, JobEntry goal, out List<JobEntry> path)
    {
        var visited = new HashSet<JobEntry>();
        path = new List<JobEntry>();
        return Visit(start, goal, visited, path);
    }

    private static bool Visit(JobEntry current, JobEntry goal, HashSet<JobEntry> visited,
        List<JobEntry> path)
    {
        if (!visited.Add(current)) return false;
        path.Add(current);
        if (ReferenceEquals(current, goal)) return true;

        if (current.WaitingOn is { } runtimeTarget &&
            Visit(runtimeTarget, goal, visited, path)) return true;

        foreach (var prerequisite in current.Prerequisites)
        {
            if (!prerequisite.Completed && Visit(prerequisite, goal, visited, path)) return true;
        }

        path.RemoveAt(path.Count - 1);
        return false;
    }
}
