namespace Aether.Serialization;

/// <summary>
/// Achatamento de um <see cref="World"/> compartilhado pelos dois formatos de serialização: entidades
/// vivas, cada uma com um índice denso 0..N-1.
/// <para>
/// O índice denso — não o <see cref="EntityId"/> original — é o que os dois formatos gravam, e é
/// também a decisão de design por trás de <see cref="BinarySerializer.Read"/> e
/// <see cref="TextSerializer.Deserialize"/> exigirem um <see cref="World"/> vazio: o índice denso não
/// carrega a história de reciclagem de slots do World de origem, então ao reconstruir num World vazio,
/// a N-ésima chamada a <see cref="World.CreateEntity()"/> sempre recebe <c>Index == N</c> e
/// <c>Version == 0</c> — ou seja, o índice denso do arquivo *é* o <see cref="EntityId.Index"/>
/// pós-carregamento, sem precisar carregar (nem persistir) uma tabela de remapeamento à parte.
/// </para>
/// <para>
/// <b>Ordem: por <see cref="EntityId.Index"/> crescente, não pela ordem de iteração de chunks.</b>
/// <c>world.Query()</c> (arquétipo → chunk → linha) parecia a ordem óbvia, mas depende de QUANDO cada
/// arquétipo foi criado no processo — e isso é um artefato da HISTÓRIA de chamadas
/// <c>AddComponent</c>/<c>RemoveComponent</c> que moveram cada entidade entre arquétipos, não do
/// conteúdo final. Duas entidades com os mesmos componentes hoje podem ter passado por sequências
/// diferentes de migração de arquétipo para chegar lá (ex.: <see cref="Hierarchy.SetParent"/> adiciona
/// <see cref="Parent"/>/<see cref="NextSibling"/>/<see cref="FirstChild"/> em momentos diferentes para
/// pai e filho) — e o <see cref="BinarySerializer.Read"/>/<see cref="TextSerializer.Deserialize"/>
/// reconstrói cada entidade adicionando seus componentes em ordem ALFABÉTICA de nome (não replicando a
/// história original), então a ordem de criação de arquétipo no World recarregado quase nunca bate com
/// a do World original — usar a ordem de <c>Query()</c> aqui faria o índice denso mudar entre a
/// gravação original e uma regravação após carregar, quebrando o requisito de round-trip bit-exato do
/// plano ("salvar e recarregar o estado é bit-exato"). Ordenar por <see cref="EntityId.Index"/> crescente
/// evita o problema porque é exatamente a ordem que <see cref="World.CreateEntity"/> reproduz sozinho ao
/// recriar as entidades em sequência num World vazio — nenhuma migração de arquétipo depois disso pode
/// mais alterar essa ordem.
/// </para>
/// </summary>
internal static class WorldSnapshot
{
    internal readonly struct EntityRecord
    {
        public readonly int DenseIndex;
        public readonly EntityId Id;
        public readonly ComponentType[] Types;
        public readonly Chunk Chunk;
        public readonly int Row;

        public EntityRecord(int denseIndex, EntityId id, ComponentType[] types, Chunk chunk, int row)
        { DenseIndex = denseIndex; Id = id; Types = types; Chunk = chunk; Row = row; }
    }

    internal static (List<EntityRecord> Entities, Dictionary<EntityId, int> DenseIndexOf) Capture(World world)
    {
        var unordered = new List<(EntityId Id, ComponentType[] Types, Chunk Chunk, int Row)>();
        foreach (var chunk in world.Query())
        {
            var ids = chunk.Entities;
            for (int row = 0; row < ids.Length; row++)
                unordered.Add((ids[row], chunk.Archetype.Types, chunk, row));
        }
        unordered.Sort((a, b) => a.Id.Index.CompareTo(b.Id.Index));

        var entities = new List<EntityRecord>(unordered.Count);
        var denseIndexOf = new Dictionary<EntityId, int>();
        for (int i = 0; i < unordered.Count; i++)
        {
            var (id, types, chunk, row) = unordered[i];
            denseIndexOf[id] = i;
            entities.Add(new EntityRecord(i, id, types, chunk, row));
        }
        return (entities, denseIndexOf);
    }

    /// <summary>Bytes crus da coluna de um componente para uma linha de um chunk — sem reflexão, é a
    /// mesma aritmética de offset que <see cref="Chunk.GetSpan{T}(int)"/> usa internamente.</summary>
    internal static ReadOnlySpan<byte> RawComponent(Chunk chunk, int row, int typeIndexInArchetype, int size)
    {
        int offset = chunk.Archetype.ColumnOffset(typeIndexInArchetype) + row * size;
        return chunk.Buffer.AsSpan(offset, size);
    }
}

/// <summary>
/// Codifica/decodifica referências a entidade (campos <see cref="EntityId"/> dentro de um componente,
/// como <see cref="Parent"/>) para o índice denso do arquivo, em vez do <see cref="EntityId.Index"/>
/// bruto — pela mesma razão descrita em <see cref="WorldSnapshot"/>.
/// <para>
/// Uma referência para uma entidade que não está mais viva no momento da gravação vira <c>null</c>. Isso
/// pode acontecer sem que nada esteja "errado": <see cref="TransformSystem.Propagate"/> já trata um
/// <see cref="Parent"/> apontando para uma entidade destruída como "sem pai" em vez de travar — a
/// engine já tolera essa referência pendurada como estado válido. Não há como preservar fielmente
/// "aponta para o slot morto nº 47" depois de reconstruir num World vazio, onde esse slot nunca
/// existiu — <c>null</c> é a tradução honesta.
/// </para>
/// </summary>
internal static class EntityRefCodec
{
    internal const int NullDenseIndex = int.MinValue;

    internal static int ToDenseOrNull(EntityId id, Dictionary<EntityId, int> denseIndexOf) =>
        !id.IsNull && denseIndexOf.TryGetValue(id, out var dense) ? dense : NullDenseIndex;

    internal static EntityId FromDenseOrNull(int denseOrNull, EntityId[] byDenseIndex) =>
        denseOrNull == NullDenseIndex ? EntityId.Null : byDenseIndex[denseOrNull];
}
