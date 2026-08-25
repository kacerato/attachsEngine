namespace Aether.Jobs;

/// <summary>
/// Estado interno de um job agendado: seu trabalho, suas dependências pendentes
/// e a lista de quem depende dele. Não é API pública — <see cref="JobHandle"/> é
/// a fachada que o código de fora enxerga.
/// </summary>
internal sealed class JobEntry
{
    /// <summary>Nulo para nós de junção criados por <see cref="JobHandle.CombineDependencies"/>: não há trabalho a rodar, só propagação.</summary>
    public Action? Work;

    public CoreAffinity Affinity;

    /// <summary>Quantas dependências ainda faltam concluir. Decrementado com <see cref="Interlocked"/>.</summary>
    public int RemainingDependencies;

    /// <summary>Jobs que esperam este terminar. Protegida por <see cref="Lock"/>.</summary>
    public readonly List<JobEntry> Dependents = new();

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
