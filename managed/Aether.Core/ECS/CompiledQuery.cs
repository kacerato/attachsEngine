namespace Aether;

/// <summary>
/// Consulta ECS reutilizável. Conserva somente as assinaturas de inclusão/exclusão e usa o cache
/// central do <see cref="World"/>; se um arquétipo compatível surgir depois da compilação, a
/// versão do cache do mundo o inclui automaticamente na próxima iteração.
/// </summary>
public readonly struct CompiledQuery
{
    private readonly World _world;
    private readonly ArchetypeSignature _with;
    private readonly ArchetypeSignature _without;

    internal CompiledQuery(World world, ArchetypeSignature with, ArchetypeSignature without)
    {
        _world = world;
        _with = with;
        _without = without;
    }

    public World.QueryEnumerable Chunks() => _world.QueryChunks(_with, _without);
    public World.QueryEnumerator GetEnumerator() => Chunks().GetEnumerator();

    /// <summary>Filtra chunks cuja coluna <typeparamref name="T"/> mudou desde a última vez em
    /// que este filtro os entregou. O tipo também passa a ser requisito da consulta.</summary>
    public ChangedQueryEnumerable<T> Changed<T>(ComponentChangeFilter<T> filter) where T : unmanaged
    {
        ArgumentNullException.ThrowIfNull(filter);
        var with = _with;
        with.Add(ComponentType.Of<T>());
        return new ChangedQueryEnumerable<T>(_world.QueryChunks(with, _without), filter);
    }
}

/// <summary>
/// Estado de change detection por chunk para um tipo de componente. A primeira observação de um
/// chunk o considera alterado; depois, somente uma versão diferente o libera novamente. Use uma
/// instância por consumidor/sistema, pois cada consumidor possui seu próprio cursor lógico.
/// </summary>
public sealed class ComponentChangeFilter<T> where T : unmanaged
{
    private readonly Dictionary<Chunk, int> _observedVersions = new();

    public int TrackedChunkCount => _observedVersions.Count;

    internal bool TryAccept(Chunk chunk)
    {
        int current = chunk.GetChangeVersion<T>();
        if (_observedVersions.TryGetValue(chunk, out int observed) && observed == current)
            return false;
        _observedVersions[chunk] = current;
        return true;
    }

    /// <summary>Esquece todos os cursores. Na próxima passagem, todos os chunks compatíveis serão
    /// tratados como novos. Deve ser chamado fora do caminho quente porque a primeira passagem
    /// pode precisar ampliar a tabela interna.</summary>
    public void Reset() => _observedVersions.Clear();
}

public readonly struct ChangedQueryEnumerable<T> where T : unmanaged
{
    private readonly World.QueryEnumerable _source;
    private readonly ComponentChangeFilter<T> _filter;

    internal ChangedQueryEnumerable(World.QueryEnumerable source, ComponentChangeFilter<T> filter)
    {
        _source = source;
        _filter = filter;
    }

    public ChangedQueryEnumerator<T> GetEnumerator() => new(_source.GetEnumerator(), _filter);
}

public struct ChangedQueryEnumerator<T> where T : unmanaged
{
    private World.QueryEnumerator _source;
    private readonly ComponentChangeFilter<T> _filter;

    internal ChangedQueryEnumerator(World.QueryEnumerator source, ComponentChangeFilter<T> filter)
    {
        _source = source;
        _filter = filter;
    }

    public readonly Chunk Current => _source.Current;

    public bool MoveNext()
    {
        while (_source.MoveNext())
            if (_filter.TryAccept(_source.Current)) return true;
        return false;
    }
}
