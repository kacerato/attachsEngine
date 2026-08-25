namespace Aether;

/// <summary>
/// Container de entidades e arquétipos: a API principal do ECS. Toda operação estrutural (criar,
/// destruir, adicionar/remover componente) move a entidade entre arquétipos preservando os
/// componentes que continuam existindo; toda leitura/escrita de componente devolve referência
/// direta ao armazenamento, sem cópia.
/// </summary>
public sealed class World
{
    private struct EntitySlot
    {
        public int Version;
        public Archetype? Archetype;
        public int ChunkIndex;
        public int Row;
        public readonly bool Alive => Archetype is not null;
    }

    private EntitySlot[] _slots = new EntitySlot[64];
    private int _slotCount;
    private readonly Stack<int> _freeIndices = new();

    private readonly Dictionary<ArchetypeSignature, Archetype> _archetypesBySignature = new();
    private readonly List<Archetype> _allArchetypes = new();

    // Cache de consultas: chave (with, without) -> lista de arquétipos que casam, mais a "versão"
    // do mundo (contagem de arquétipos) em que a lista foi calculada. Só recalcula quando nasce um
    // arquétipo novo — arquétipos nunca são removidos, então essa contagem só cresce.
    private readonly Dictionary<(ArchetypeSignature With, ArchetypeSignature Without), (int Version, List<Archetype> Archetypes)> _queryCache = new();

    public int EntityCount { get; private set; }

    public World()
    {
        // Aquece o dicionário genérico do cache de consultas. Na .NET, a primeira leva de chamadas
        // a um fechamento genérico novo de Dictionary<TKey,TValue> (aqui, com TKey/TValue montados
        // de ArchetypeSignature) paga um custo de inicialização de comparador/hash que não é mais
        // pago depois — sem isso, a primeira consulta real do usuário poderia estourar o orçamento
        // de zero alocação só por má sorte de ser a primeira a tocar esse tipo genérico no processo.
        var warm = default(ArchetypeSignature);
        QueryChunks(warm, warm);
        QueryChunks(warm, warm);
    }

    /// <summary>Um índice de slot maior que qualquer índice de entidade viva ou já usada — limite
    /// superior seguro para varreduras externas indexadas por <c>EntityId.Index</c> (ex.: <see cref="TransformSystem"/>).</summary>
    public int Capacity => _slotCount;

    // ---------------------------------------------------------------- criação / destruição

    public EntityId CreateEntity()
    {
        var archetype = GetOrCreateArchetype(default, Array.Empty<ComponentType>());
        return AllocateInArchetype(archetype);
    }

    public EntityId CreateEntity<T1>(in T1 c1) where T1 : unmanaged
    {
        var sig = default(ArchetypeSignature);
        sig.Add(ComponentType.Of<T1>());
        var archetype = GetOrCreateArchetype(sig, new[] { ComponentType.Of<T1>() });
        var id = AllocateInArchetype(archetype);
        GetComponent<T1>(id) = c1;
        return id;
    }

    public EntityId CreateEntity<T1, T2>(in T1 c1, in T2 c2) where T1 : unmanaged where T2 : unmanaged
    {
        var sig = default(ArchetypeSignature);
        sig.Add(ComponentType.Of<T1>()); sig.Add(ComponentType.Of<T2>());
        var archetype = GetOrCreateArchetype(sig, new[] { ComponentType.Of<T1>(), ComponentType.Of<T2>() });
        var id = AllocateInArchetype(archetype);
        GetComponent<T1>(id) = c1;
        GetComponent<T2>(id) = c2;
        return id;
    }

    public EntityId CreateEntity<T1, T2, T3>(in T1 c1, in T2 c2, in T3 c3)
        where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        var sig = default(ArchetypeSignature);
        sig.Add(ComponentType.Of<T1>()); sig.Add(ComponentType.Of<T2>()); sig.Add(ComponentType.Of<T3>());
        var archetype = GetOrCreateArchetype(sig, new[] { ComponentType.Of<T1>(), ComponentType.Of<T2>(), ComponentType.Of<T3>() });
        var id = AllocateInArchetype(archetype);
        GetComponent<T1>(id) = c1;
        GetComponent<T2>(id) = c2;
        GetComponent<T3>(id) = c3;
        return id;
    }

    private EntityId AllocateInArchetype(Archetype archetype)
    {
        int index;
        if (_freeIndices.Count > 0)
        {
            index = _freeIndices.Pop();
        }
        else
        {
            index = _slotCount++;
            if (index >= _slots.Length) Array.Resize(ref _slots, _slots.Length * 2);
        }

        var chunk = archetype.GetChunkWithRoom();
        int row = chunk.Count++;
        int version = _slots[index].Version;
        var id = new EntityId(index, version);
        chunk.EntityAt(row) = id;

        _slots[index] = new EntitySlot { Version = version, Archetype = archetype, ChunkIndex = archetype.Chunks.IndexOf(chunk), Row = row };
        EntityCount++;
        return id;
    }

    public bool Exists(EntityId id) =>
        id.Index >= 0 && id.Index < _slotCount && _slots[id.Index].Version == id.Version && _slots[id.Index].Alive;

    public void DestroyEntity(EntityId id)
    {
        ref var slot = ref RequireAlive(id);
        RemoveFromChunk(ref slot);
        slot.Archetype = null;
        slot.Version++;         // qualquer EntityId antigo com a versão anterior passa a falhar em Exists/RequireAlive
        _freeIndices.Push(id.Index);
        EntityCount--;
    }

    /// <summary>Remove a entidade do slot que ocupa em seu chunk atual, com swap-back: a última
    /// entidade do chunk toma o lugar da removida, para que o chunk nunca tenha buracos no meio —
    /// requisito da iteração linear por span.</summary>
    private void RemoveFromChunk(ref EntitySlot slot)
    {
        var archetype = slot.Archetype!;
        var chunk = archetype.Chunks[slot.ChunkIndex];
        int last = chunk.Count - 1;
        int row = slot.Row;
        if (row != last)
        {
            foreach (var t in archetype.Types)
                Chunk.CopyRaw(chunk, last, chunk, row, t);
            var movedId = chunk.EntityAt(last);
            chunk.EntityAt(row) = movedId;
            _slots[movedId.Index].Row = row;
        }
        chunk.EntityAt(last) = EntityId.Null;
        chunk.Count--;
    }

    // ---------------------------------------------------------------- acesso a componente

    private ref EntitySlot RequireAlive(EntityId id)
    {
        if (!Exists(id))
            throw new InvalidOperationException($"A entidade {id} não existe ou já foi destruída.");
        return ref _slots[id.Index];
    }

    /// <summary>Referência direta ao componente armazenado — sem cópia. Como não há como saber se
    /// o chamador vai só ler ou também escrever através da referência, tratamos todo acesso como
    /// uma possível escrita e avançamos a versão da coluna (conservador, mas correto).</summary>
    public ref T GetComponent<T>(EntityId id) where T : unmanaged
    {
        ref var slot = ref RequireAlive(id);
        var archetype = slot.Archetype!;
        int typeIndex = archetype.IndexOf(ComponentType.Of<T>());
        if (typeIndex < 0)
            throw new InvalidOperationException($"A entidade {id} não possui o componente {typeof(T).Name}.");
        var chunk = archetype.Chunks[slot.ChunkIndex];
        chunk.MarkChanged(typeIndex);
        return ref chunk.GetSpan<T>(typeIndex)[slot.Row];
    }

    public bool TryGetComponent<T>(EntityId id, out T value) where T : unmanaged
    {
        if (Exists(id))
        {
            var slot = _slots[id.Index];
            var archetype = slot.Archetype!;
            int typeIndex = archetype.IndexOf(ComponentType.Of<T>());
            if (typeIndex >= 0)
            {
                value = archetype.Chunks[slot.ChunkIndex].GetSpan<T>(typeIndex)[slot.Row];
                return true;
            }
        }
        value = default;
        return false;
    }

    public bool HasComponent<T>(EntityId id) where T : unmanaged
    {
        if (!Exists(id)) return false;
        var slot = _slots[id.Index];
        return slot.Archetype!.IndexOf(ComponentType.Of<T>()) >= 0;
    }

    public void SetComponent<T>(EntityId id, in T value) where T : unmanaged
    {
        GetComponent<T>(id) = value;
    }

    public void AddComponent<T>(EntityId id, in T value) where T : unmanaged
    {
        ref var slot = ref RequireAlive(id);
        var oldArchetype = slot.Archetype!;
        var newType = ComponentType.Of<T>();
        if (oldArchetype.IndexOf(newType) >= 0)
            throw new InvalidOperationException($"A entidade {id} já possui o componente {typeof(T).Name}; use SetComponent.");

        var newTypes = new ComponentType[oldArchetype.Types.Length + 1];
        Array.Copy(oldArchetype.Types, newTypes, oldArchetype.Types.Length);
        newTypes[^1] = newType;
        var newSignature = oldArchetype.Signature;
        newSignature.Add(newType);
        var newArchetype = GetOrCreateArchetype(newSignature, newTypes);

        MoveEntity(id, ref slot, oldArchetype, newArchetype);

        GetComponent<T>(id) = value;
    }

    public void RemoveComponent<T>(EntityId id) where T : unmanaged
    {
        ref var slot = ref RequireAlive(id);
        var oldArchetype = slot.Archetype!;
        var removedType = ComponentType.Of<T>();
        if (oldArchetype.IndexOf(removedType) < 0)
            throw new InvalidOperationException($"A entidade {id} não possui o componente {typeof(T).Name}.");

        var newTypes = new ComponentType[oldArchetype.Types.Length - 1];
        int w = 0;
        foreach (var t in oldArchetype.Types)
            if (t.Id != removedType.Id) newTypes[w++] = t;
        var newSignature = oldArchetype.Signature;
        newSignature.Remove(removedType);
        var newArchetype = GetOrCreateArchetype(newSignature, newTypes);

        MoveEntity(id, ref slot, oldArchetype, newArchetype);
    }

    /// <summary>
    /// Adiciona um componente a uma entidade a partir do <see cref="ComponentType"/> de tempo de
    /// execução e dos bytes crus do valor, sem T genérico no ponto de chamada. Único motivo de existir:
    /// <c>Aether.Serialization</c> reconstrói entidades a partir de metadados descobertos em tempo de
    /// execução (o nome do componente lido de um arquivo) — não há T disponível ali para chamar o
    /// <see cref="AddComponent{T}"/> público. Fica <c>internal</c> de propósito: não é parte da API segura em
    /// tipos que o resto da engine deve usar; espelha exatamente a lógica de <see cref="AddComponent{T}"/>,
    /// só trocando a escrita tipada final por uma cópia de bytes crus na coluna do novo chunk.
    /// </summary>
    internal void AddComponentRaw(EntityId id, ComponentType type, ReadOnlySpan<byte> rawValue)
    {
        ref var slot = ref RequireAlive(id);
        var oldArchetype = slot.Archetype!;
        if (oldArchetype.IndexOf(type) >= 0)
            throw new InvalidOperationException($"A entidade {id} já possui o componente #{type.Id}.");
        if (rawValue.Length != type.Size)
            throw new ArgumentException(
                $"Bytes crus de tamanho {rawValue.Length} não batem com o tamanho do componente #{type.Id} ({type.Size}).",
                nameof(rawValue));

        var newTypes = new ComponentType[oldArchetype.Types.Length + 1];
        Array.Copy(oldArchetype.Types, newTypes, oldArchetype.Types.Length);
        newTypes[^1] = type;
        var newSignature = oldArchetype.Signature;
        newSignature.Add(type);
        var newArchetype = GetOrCreateArchetype(newSignature, newTypes);

        MoveEntity(id, ref slot, oldArchetype, newArchetype);

        var chunk = newArchetype.Chunks[slot.ChunkIndex];
        int typeIndex = newArchetype.IndexOf(type);
        int offset = newArchetype.ColumnOffset(typeIndex) + slot.Row * type.Size;
        rawValue.CopyTo(chunk.Buffer.AsSpan(offset, type.Size));
        chunk.MarkChanged(typeIndex);
    }

    /// <summary>Move uma entidade de um arquétipo para outro, copiando (byte a byte) os
    /// componentes que existem em ambos e descartando o resto. Usado por Add/RemoveComponent.</summary>
    private void MoveEntity(EntityId id, ref EntitySlot slot, Archetype oldArchetype, Archetype newArchetype)
    {
        var oldChunk = oldArchetype.Chunks[slot.ChunkIndex];
        int oldRow = slot.Row;

        var newChunk = newArchetype.GetChunkWithRoom();
        int newRow = newChunk.Count++;
        newChunk.EntityAt(newRow) = id;

        foreach (var t in oldArchetype.Types)
            if (newArchetype.IndexOf(t) >= 0)
                Chunk.CopyRaw(oldChunk, oldRow, newChunk, newRow, t);

        RemoveFromChunk(ref slot);

        slot.Archetype = newArchetype;
        slot.ChunkIndex = newArchetype.Chunks.IndexOf(newChunk);
        slot.Row = newRow;
    }

    // ---------------------------------------------------------------- arquétipos

    private Archetype GetOrCreateArchetype(ArchetypeSignature signature, ComponentType[] types)
    {
        if (_archetypesBySignature.TryGetValue(signature, out var existing)) return existing;
        var archetype = new Archetype(signature, types);
        _archetypesBySignature.Add(signature, archetype);
        _allArchetypes.Add(archetype);
        return archetype;
    }

    // ---------------------------------------------------------------- consultas

    public QueryBuilder Query() => new(this);

    public QueryEnumerable Query<T1>() where T1 : unmanaged
    {
        var with = default(ArchetypeSignature);
        with.Add(ComponentType.Of<T1>());
        return QueryChunks(with, default);
    }

    public QueryEnumerable Query<T1, T2>() where T1 : unmanaged where T2 : unmanaged
    {
        var with = default(ArchetypeSignature);
        with.Add(ComponentType.Of<T1>()); with.Add(ComponentType.Of<T2>());
        return QueryChunks(with, default);
    }

    public QueryEnumerable Query<T1, T2, T3>() where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        var with = default(ArchetypeSignature);
        with.Add(ComponentType.Of<T1>()); with.Add(ComponentType.Of<T2>()); with.Add(ComponentType.Of<T3>());
        return QueryChunks(with, default);
    }

    internal QueryEnumerable QueryChunks(ArchetypeSignature with, ArchetypeSignature without)
    {
        var key = (with, without);
        if (!_queryCache.TryGetValue(key, out var cached) || cached.Version != _allArchetypes.Count)
        {
            var list = new List<Archetype>();
            foreach (var a in _allArchetypes)
                if (a.Signature.Matches(with, without)) list.Add(a);
            cached = (_allArchetypes.Count, list);
            _queryCache[key] = cached;
        }
        return new QueryEnumerable(cached.Archetypes);
    }

    public readonly struct QueryBuilder
    {
        private readonly World _world;
        private readonly ArchetypeSignature _with;
        private readonly ArchetypeSignature _without;

        internal QueryBuilder(World world) { _world = world; _with = default; _without = default; }
        private QueryBuilder(World world, ArchetypeSignature with, ArchetypeSignature without)
        { _world = world; _with = with; _without = without; }

        public QueryBuilder With<T>() where T : unmanaged
        {
            var w = _with; w.Add(ComponentType.Of<T>());
            return new QueryBuilder(_world, w, _without);
        }

        public QueryBuilder Without<T>() where T : unmanaged
        {
            var w = _without; w.Add(ComponentType.Of<T>());
            return new QueryBuilder(_world, _with, w);
        }

        public QueryEnumerable Chunks() => _world.QueryChunks(_with, _without);
        public QueryEnumerator GetEnumerator() => Chunks().GetEnumerator();
    }

    /// <summary>Enumerável de chunks casando uma consulta. É um struct fino sobre a lista de
    /// arquétipos já cacheada — obtê-lo e iterá-lo não aloca.</summary>
    public readonly struct QueryEnumerable
    {
        private readonly List<Archetype> _archetypes;
        internal QueryEnumerable(List<Archetype> archetypes) => _archetypes = archetypes;
        public QueryEnumerator GetEnumerator() => new(_archetypes);
    }

    public struct QueryEnumerator
    {
        private readonly List<Archetype> _archetypes;
        private int _archetypeIndex;
        private List<Chunk>? _chunks;
        private int _chunkIndex;

        internal QueryEnumerator(List<Archetype> archetypes)
        {
            _archetypes = archetypes;
            _archetypeIndex = -1;
            _chunks = null;
            _chunkIndex = -1;
            Current = null!;
        }

        public Chunk Current { get; private set; }

        public bool MoveNext()
        {
            while (true)
            {
                if (_chunks is not null)
                {
                    _chunkIndex++;
                    if (_chunkIndex < _chunks.Count)
                    {
                        var candidate = _chunks[_chunkIndex];
                        if (candidate.Count == 0) continue;   // chunk esvaziado por destruição, mas não removido da lista
                        Current = candidate;
                        return true;
                    }
                }

                _archetypeIndex++;
                if (_archetypeIndex >= _archetypes.Count) return false;
                _chunks = _archetypes[_archetypeIndex].Chunks;
                _chunkIndex = -1;
            }
        }
    }
}
