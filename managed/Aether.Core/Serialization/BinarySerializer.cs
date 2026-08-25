using System.Text;

namespace Aether.Serialization;

/// <summary>
/// Formato de arquivo binário "rápido": sem reflexão em tempo de leitura/escrita — o único uso de
/// reflexão em todo este módulo acontece uma vez, em <see cref="ComponentRegistry.Register{T}"/>, para
/// calcular offsets. Escrever/ler um componente aqui é uma cópia de bytes crus
/// (<c>ReadOnlySpan&lt;byte&gt;</c>/<c>byte[]</c>) usando o offset de coluna do chunk — a mesma
/// aritmética que <see cref="Chunk.GetSpan{T}(int)"/> já usa para dar acesso tipado sem cópia.
///
/// <para><b>Layout do arquivo</b> (tudo little-endian via <see cref="BinaryWriter"/>/<see cref="BinaryReader"/>):</para>
/// <code>
/// magic            4 bytes  = "AEBF"
/// formatoContêiner int32    = versão deste formato de arquivo (não a versão de esquema de componente)
/// contagemEntidades int32
/// contagemTipos    int32
/// tabelaDeTipos    contagemTipos vezes { nome: string, versãoDeEsquemaNaGravação: int32 }
/// entidades        contagemEntidades vezes:
///   índiceDenso        int32   -- ver WorldSnapshot: 0..contagemEntidades-1, na ordem de gravação
///   contagemComponentes int32
///   componentes        contagemComponentes vezes { índiceNaTabelaDeTipos: int32, tamanho: int32, bytes crus }
/// </code>
///
/// <para><b>Por que índice denso e não <see cref="EntityId"/> bruto</b>: ver o comentário em
/// <see cref="WorldSnapshot"/>. Consequência direta: <see cref="Read"/> só aceita um
/// <see cref="World"/> vazio (<c>Capacity == 0</c>) — é isso que garante que recriar entidades na ordem
/// do arquivo reproduz exatamente os mesmos índices densos como <see cref="EntityId.Index"/>.</para>
///
/// <para><b>Entidades por <see cref="EntityId.Index"/> crescente, não pela ordem de iteração de chunks</b>
/// — ver o comentário de classe em <see cref="WorldSnapshot"/>: é o que garante que gravar, carregar e
/// gravar de novo produz os mesmos bytes, mesmo quando o World recarregado passou por uma história de
/// migração de arquétipo diferente da do original.</para>
///
/// <para><b>Componentes por entidade em ordem alfabética de nome</b> (não a ordem interna do
/// arquétipo): a ordem do arquétipo depende de <see cref="ComponentType.Id"/>, que é atribuído na
/// primeira vez que cada tipo é tocado NESTE PROCESSO — não é estável entre execuções. Ordenar pelo
/// nome estável do componente é o que torna duas gravações do MESMO <see cref="World"/>, no mesmo
/// processo, bit-a-bit idênticas (round-trip determinístico), sem depender dessa ordem incidental.</para>
///
/// <para><b>Componentes não registrados</b>: um tipo de componente presente numa entidade mas sem
/// <see cref="ComponentDescriptor"/> registrado é ignorado silenciosamente ao gravar (não sabemos o
/// nome estável dele, então não há como escrevê-lo). Um nome de componente no arquivo que este build
/// não reconhece (ex.: arquivo de uma versão futura, ou de um jogo com componentes de gameplay que este
/// processo de teste não registrou) também é ignorado ao ler. As duas escolhas seguem a mesma regra:
/// nunca travar a leitura/gravação por causa de um tipo desconhecido — ver CONVENCOES.md, barra de
/// qualidade #3 ("nunca crashar o editor por culpa do conteúdo do usuário").</para>
/// </summary>
public static class BinarySerializer
{
    private static readonly byte[] Magic = { (byte)'A', (byte)'E', (byte)'B', (byte)'F' };
    private const int ContainerFormatVersion = 1;

    public static void Write(World world, Stream stream)
    {
        var (entities, denseIndexOf) = WorldSnapshot.Capture(world);

        // Tabela de tipos: só os componentes registrados que realmente aparecem em algum lugar do
        // mundo, em ordem alfabética de nome (arquivo determinístico e sem entradas nunca usadas).
        var typeTable = new SortedDictionary<string, ComponentDescriptor>(StringComparer.Ordinal);
        foreach (var e in entities)
            foreach (var t in e.Types)
                if (ComponentRegistry.TryGetByType(t, out var d))
                    typeTable[d.Name] = d;

        var typeTableArray = typeTable.Values.ToArray();
        var typeIndexByName = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < typeTableArray.Length; i++) typeIndexByName[typeTableArray[i].Name] = i;

        using var w = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
        w.Write(Magic);
        w.Write(ContainerFormatVersion);
        w.Write(entities.Count);
        w.Write(typeTableArray.Length);
        foreach (var d in typeTableArray)
        {
            w.Write(d.Name);
            w.Write(d.SchemaVersion);
        }

        foreach (var e in entities)
        {
            var present = new List<(ComponentDescriptor Descriptor, int TypeIndexInArchetype)>();
            for (int ti = 0; ti < e.Types.Length; ti++)
                if (ComponentRegistry.TryGetByType(e.Types[ti], out var d))
                    present.Add((d, ti));
            present.Sort((a, b) => string.CompareOrdinal(a.Descriptor.Name, b.Descriptor.Name));

            w.Write(e.DenseIndex);
            w.Write(present.Count);
            foreach (var (descriptor, typeIndexInArchetype) in present)
            {
                var raw = WorldSnapshot.RawComponent(e.Chunk, e.Row, typeIndexInArchetype, descriptor.Type.Size);
                var payload = raw.ToArray();
                RemapEntityRefsForWrite(payload, descriptor, denseIndexOf);

                w.Write(typeIndexByName[descriptor.Name]);
                w.Write(payload.Length);
                w.Write(payload);
            }
        }
    }

    /// <summary>Popula <paramref name="world"/> a partir do stream. Exige um World vazio — ver o
    /// comentário de classe sobre por que o esquema de índice denso depende disso.</summary>
    public static void Read(World world, Stream stream)
    {
        ArgumentNullException.ThrowIfNull(world);
        if (world.Capacity != 0)
            throw new InvalidOperationException(
                "BinarySerializer.Read só popula um World vazio: os índices densos do arquivo viram os " +
                "EntityId.Index da reconstrução, e isso só é verdade se nenhuma entidade tiver sido criada " +
                "ou destruída neste World antes. Use um World novo.");

        using var r = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
        var magic = r.ReadBytes(4);
        if (!magic.AsSpan().SequenceEqual(Magic))
            throw new InvalidOperationException("O arquivo não é um mundo binário do Aether (assinatura mágica não bate).");

        int containerVersion = r.ReadInt32();
        if (containerVersion != ContainerFormatVersion)
            throw new InvalidOperationException(
                $"Versão de formato binário {containerVersion} não é suportada; esta versão do Aether entende a versão {ContainerFormatVersion}.");

        int entityCount = r.ReadInt32();
        int typeCount = r.ReadInt32();
        var typeTable = new (string Name, int SchemaVersionAtWrite)[typeCount];
        for (int i = 0; i < typeCount; i++)
            typeTable[i] = (r.ReadString(), r.ReadInt32());

        // Primeira passada: cria todas as entidades vazias na ordem do arquivo. Como o World está
        // garantidamente vazio (checado acima), a i-ésima chamada recebe Index == i e Version == 0 —
        // exatamente o esquema de índice denso descrito em WorldSnapshot.
        var byDenseIndex = new EntityId[entityCount];
        for (int i = 0; i < entityCount; i++)
        {
            var id = world.CreateEntity();
            if (id.Index != i)
                throw new InvalidOperationException(
                    $"Índice de entidade inesperado ao reconstruir ({id.Index}, esperado {i}) — o World não estava realmente vazio.");
            byDenseIndex[i] = id;
        }

        for (int i = 0; i < entityCount; i++)
        {
            int denseIndex = r.ReadInt32();
            if (denseIndex != i)
                throw new InvalidOperationException($"Entidade fora de ordem no arquivo: esperado índice denso {i}, veio {denseIndex}.");

            int componentCount = r.ReadInt32();
            for (int c = 0; c < componentCount; c++)
            {
                int typeIdx = r.ReadInt32();
                int length = r.ReadInt32();
                byte[] payload = r.ReadBytes(length);
                if (payload.Length != length)
                    throw new InvalidOperationException("Arquivo binário truncado no meio de um componente.");

                if (typeIdx < 0 || typeIdx >= typeTable.Length)
                    throw new InvalidOperationException($"Índice de tipo de componente inválido no arquivo: {typeIdx}.");
                var (name, schemaVersionAtWrite) = typeTable[typeIdx];

                if (!ComponentRegistry.TryGetByName(name, out var descriptor))
                    continue; // tipo que este build não conhece mais — ignorado, não é erro

                byte[] migrated = ComponentRegistry.Migrate(descriptor.Type, schemaVersionAtWrite, descriptor.SchemaVersion, payload);
                if (migrated.Length != descriptor.Type.Size)
                    throw new InvalidOperationException(
                        $"Após migração, o componente \"{name}\" tem {migrated.Length} bytes, mas o esquema atual " +
                        $"espera {descriptor.Type.Size} — migração incompleta ou incorreta.");

                RemapEntityRefsForRead(migrated, descriptor, byDenseIndex);
                world.AddComponentRaw(byDenseIndex[i], descriptor.Type, migrated);
            }
        }
    }

    private static void RemapEntityRefsForWrite(byte[] payload, ComponentDescriptor descriptor, Dictionary<EntityId, int> denseIndexOf)
    {
        foreach (var field in descriptor.Fields)
        {
            if (field.Kind != ComponentFieldKind.EntityId) continue;
            int index = BitConverter.ToInt32(payload, field.Offset);
            int version = BitConverter.ToInt32(payload, field.Offset + 4);
            var referenced = new EntityId(index, version);
            int dense = EntityRefCodec.ToDenseOrNull(referenced, denseIndexOf);
            BitConverter.TryWriteBytes(payload.AsSpan(field.Offset, 4), dense);
            BitConverter.TryWriteBytes(payload.AsSpan(field.Offset + 4, 4), 0);
        }
    }

    private static void RemapEntityRefsForRead(byte[] payload, ComponentDescriptor descriptor, EntityId[] byDenseIndex)
    {
        foreach (var field in descriptor.Fields)
        {
            if (field.Kind != ComponentFieldKind.EntityId) continue;
            int dense = BitConverter.ToInt32(payload, field.Offset);
            var resolved = EntityRefCodec.FromDenseOrNull(dense, byDenseIndex);
            BitConverter.TryWriteBytes(payload.AsSpan(field.Offset, 4), resolved.Index);
            BitConverter.TryWriteBytes(payload.AsSpan(field.Offset + 4, 4), resolved.Version);
        }
    }
}
