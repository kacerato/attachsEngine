namespace Aether.Jobs;

/// <summary>
/// Alça leve para um job agendado no <see cref="JobSystem"/>. Usada para expressar
/// dependências (um job só começa depois que sua alça de dependência terminou) e
/// para aguardar conclusão via <see cref="JobSystem.Complete"/>.
/// </summary>
public readonly struct JobHandle : IEquatable<JobHandle>
{
    internal readonly JobEntry? Entry;

    internal JobHandle(JobEntry? entry) => Entry = entry;

    /// <summary>Alça "vazia": sem job associado. <see cref="JobSystem.Complete"/> nela é no-op.</summary>
    public static readonly JobHandle Default = default;

    public bool IsValid => Entry is not null;

    public bool Equals(JobHandle other) => ReferenceEquals(Entry, other.Entry);
    public override bool Equals(object? obj) => obj is JobHandle h && Equals(h);
    public override int GetHashCode() => Entry is null ? 0 : System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(Entry);

    /// <summary>
    /// Combina várias alças em uma só: a alça resultante conclui quando todas as
    /// alças de entrada concluírem. Não roda em nenhuma thread própria — é só um
    /// nó de junção que propaga a conclusão para quem depender dela.
    /// </summary>
    public static JobHandle CombineDependencies(params JobHandle[] handles)
    {
        ArgumentNullException.ThrowIfNull(handles);

        var prereqs = new List<JobEntry>(handles.Length);
        foreach (var h in handles)
            if (h.Entry is { } e) prereqs.Add(e);

        if (prereqs.Count == 0) return default;
        if (prereqs.Count == 1) return new JobHandle(prereqs[0]);

        var join = new JobEntry { Work = null, EnqueueSelf = null, RemainingDependencies = prereqs.Count };
        foreach (var p in prereqs)
        {
            bool pending;
            lock (p.Lock)
            {
                pending = !p.Completed;
                if (pending) p.Dependents.Add(join);
            }
            if (!pending) join.NotifyPrerequisiteDone(p);
        }
        return new JobHandle(join);
    }
}
