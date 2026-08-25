using System.Globalization;
using System.Text;

namespace Aether.Serialization;

/// <summary>
/// Formato de arquivo texto, no mesmo "sabor" do <c>.aflow</c> de <c>Aether.Flow.Serialization.FlowSerializer</c>:
/// texto simples, plano, com uma gramática pequena o bastante para caber na cabeça — não é JSON
/// genérico via reflexão de propósito geral, é um formato próprio com nome de componente e campos
/// nomeados (a reflexão dos campos já foi feita uma vez, no registro — ver <see cref="ComponentRegistry"/>).
///
/// <para><b>Gramática</b> (indentação é só para leitura humana; o parser dispara pelo primeiro token de cada linha):</para>
/// <code>
/// mundo &lt;versãoDeFormatoDeContêiner&gt;
/// entidade &lt;índiceDenso&gt;
///   componente "&lt;NomeEstável&gt;" &lt;versãoDeEsquema&gt;
///     campo "&lt;caminho.pontilhado&gt;" &lt;valor&gt;
/// </code>
///
/// <para><b>Determinístico</b> (mesmo World sempre produz o mesmo texto, byte a byte): entidades na
/// ordem de <see cref="WorldSnapshot"/> (por <see cref="EntityId.Index"/> crescente — ver o comentário
/// de classe lá sobre por que não é a ordem de iteração de chunks), componentes dentro de uma entidade
/// em ordem alfabética do nome estável, campos dentro de um componente em ordem alfabética do caminho
/// (<see cref="ComponentDescriptor.Fields"/> já vem nessa ordem). Isso é o que faz o diff em controle de
/// versão ser útil, exatamente como o comentário de <c>FlowSerializer</c> explica para o <c>.aflow</c>.</para>
///
/// <para><b>Referências a entidade</b> (campos <see cref="ComponentFieldKind.EntityId"/>, ex.:
/// <c>Aether.Parent</c>) são escritas como o índice denso da entidade referenciada, ou a palavra
/// <c>null</c> — nunca o <see cref="EntityId.Index"/>/<see cref="EntityId.Version"/> brutos, pela mesma
/// razão documentada em <see cref="WorldSnapshot"/> e <see cref="EntityRefCodec"/>.</para>
///
/// <para><b>Limitação conhecida de migração de esquema no formato texto</b>: ao contrário do formato
/// binário (que migra BYTES crus de uma versão para a próxima via
/// <see cref="ComponentRegistry.RegisterMigration{T}"/>), este formato lê campos por NOME/caminho, já
/// no esquema atual — um campo removido entre versões é ignorado ao ler um arquivo antigo, e um campo
/// novo fica com o valor padrão (zero) se não estiver presente no arquivo. Um campo renomeado perde o
/// valor salvo. Uma migração textual fiel exigiria uma tabela de "caminhos de campo por versão" própria
/// deste formato — o plano só pede round-trip determinístico para o texto e migração de esquema para o
/// binário (que é onde o teste de migração deste módulo vive), então não implementamos isso aqui; fica
/// documentado como limitação, não escondido.</para>
/// </summary>
public static class TextSerializer
{
    private const int ContainerFormatVersion = 1;

    public static string Serialize(World world)
    {
        ArgumentNullException.ThrowIfNull(world);
        var (entities, denseIndexOf) = WorldSnapshot.Capture(world);
        var sb = new StringBuilder();
        sb.Append("mundo ").Append(ContainerFormatVersion).Append('\n');

        foreach (var e in entities)
        {
            sb.Append("entidade ").Append(e.DenseIndex).Append('\n');

            var present = new List<(ComponentDescriptor Descriptor, int TypeIndexInArchetype)>();
            for (int ti = 0; ti < e.Types.Length; ti++)
                if (ComponentRegistry.TryGetByType(e.Types[ti], out var d))
                    present.Add((d, ti));
            present.Sort((a, b) => string.CompareOrdinal(a.Descriptor.Name, b.Descriptor.Name));

            foreach (var (descriptor, typeIndexInArchetype) in present)
            {
                var raw = WorldSnapshot.RawComponent(e.Chunk, e.Row, typeIndexInArchetype, descriptor.Type.Size).ToArray();
                sb.Append("  componente ").Append(Quote(descriptor.Name)).Append(' ').Append(descriptor.SchemaVersion).Append('\n');
                foreach (var field in descriptor.Fields)
                    sb.Append("    campo ").Append(Quote(field.Path)).Append(' ').Append(FormatField(field, raw, denseIndexOf)).Append('\n');
            }
        }

        return sb.ToString();
    }

    /// <summary>Popula <paramref name="world"/> a partir do texto. Exige um World vazio — mesma razão
    /// documentada em <see cref="BinarySerializer.Read"/> e <see cref="WorldSnapshot"/>.</summary>
    public static void Deserialize(World world, string text)
    {
        ArgumentNullException.ThrowIfNull(world);
        ArgumentNullException.ThrowIfNull(text);
        if (world.Capacity != 0)
            throw new InvalidOperationException(
                "TextSerializer.Deserialize só popula um World vazio: os índices densos do texto viram os " +
                "EntityId.Index da reconstrução, e isso só é verdade se nenhuma entidade tiver sido criada " +
                "ou destruída neste World antes. Use um World novo.");

        var lines = text.Replace("\r\n", "\n").Split('\n');

        int entityCount = 0;
        foreach (var raw in lines)
        {
            var toks = Tokenize(raw);
            if (toks.Count > 0 && toks[0] == "entidade") entityCount++;
        }

        var byDenseIndex = new EntityId[entityCount];
        for (int i = 0; i < entityCount; i++)
        {
            var id = world.CreateEntity();
            if (id.Index != i)
                throw new InvalidOperationException($"Índice de entidade inesperado ao reconstruir ({id.Index}, esperado {i}).");
            byDenseIndex[i] = id;
        }

        EntityId current = EntityId.Null;
        bool haveCurrent = false;
        ComponentDescriptor currentDescriptor = default;
        byte[]? currentPayload = null;
        int nextExpectedDense = 0;
        bool sawHeader = false;

        void FlushComponent()
        {
            if (currentPayload is null) return;
            if (currentPayload.Length != currentDescriptor.Type.Size)
                throw new FormatException(
                    $"Componente \"{currentDescriptor.Name}\" ficou com {currentPayload.Length} bytes montados, esperado {currentDescriptor.Type.Size}.");
            world.AddComponentRaw(current, currentDescriptor.Type, currentPayload);
            currentPayload = null;
        }

        foreach (var rawLine in lines)
        {
            var toks = Tokenize(rawLine);
            if (toks.Count == 0) continue;

            switch (toks[0])
            {
                case "mundo":
                {
                    int containerVersion = int.Parse(toks[1], CultureInfo.InvariantCulture);
                    if (containerVersion != ContainerFormatVersion)
                        throw new FormatException($"Versão de formato texto {containerVersion} não suportada; esperado {ContainerFormatVersion}.");
                    sawHeader = true;
                    break;
                }

                case "entidade":
                {
                    FlushComponent();
                    int dense = int.Parse(toks[1], CultureInfo.InvariantCulture);
                    if (dense != nextExpectedDense)
                        throw new FormatException($"Entidade fora de ordem: esperado índice denso {nextExpectedDense}, veio {dense}.");
                    current = byDenseIndex[dense];
                    haveCurrent = true;
                    nextExpectedDense++;
                    break;
                }

                case "componente":
                {
                    FlushComponent();
                    if (!haveCurrent)
                        throw new FormatException("Linha \"componente\" fora de um bloco \"entidade\".");
                    string name = toks[1];
                    // A versão de esquema gravada no arquivo é só informativa aqui — ver a limitação de
                    // migração textual documentada no cabeçalho da classe.
                    _ = int.Parse(toks[2], CultureInfo.InvariantCulture);
                    if (!ComponentRegistry.TryGetByName(name, out currentDescriptor))
                    { currentPayload = null; break; } // componente que este build não conhece — campos seguintes são ignorados
                    currentPayload = new byte[currentDescriptor.Type.Size];
                    break;
                }

                case "campo":
                {
                    if (currentPayload is null) break; // dentro de um componente desconhecido — ignorado
                    var field = FindField(currentDescriptor, toks[1]);
                    ParseFieldInto(field, toks[2], currentPayload, byDenseIndex);
                    break;
                }

                default:
                    throw new FormatException($"Linha desconhecida no formato texto: {rawLine}");
            }
        }
        FlushComponent();

        if (!sawHeader)
            throw new FormatException("Texto vazio ou sem o cabeçalho \"mundo\".");
    }

    // ------------------------------------------------------------ campos: formatar / interpretar

    private static string FormatField(ComponentField field, byte[] raw, Dictionary<EntityId, int> denseIndexOf)
    {
        if (field.Kind == ComponentFieldKind.EntityId)
        {
            int index = BitConverter.ToInt32(raw, field.Offset);
            int version = BitConverter.ToInt32(raw, field.Offset + 4);
            int dense = EntityRefCodec.ToDenseOrNull(new EntityId(index, version), denseIndexOf);
            return dense == EntityRefCodec.NullDenseIndex ? "null" : dense.ToString(CultureInfo.InvariantCulture);
        }

        return field.Kind switch
        {
            ComponentFieldKind.Bool   => raw[field.Offset] != 0 ? "true" : "false",
            ComponentFieldKind.Byte   => raw[field.Offset].ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.SByte  => ((sbyte)raw[field.Offset]).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.Int16  => BitConverter.ToInt16(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.UInt16 => BitConverter.ToUInt16(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.Char   => ((int)BitConverter.ToChar(raw, field.Offset)).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.Int32  => BitConverter.ToInt32(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.UInt32 => BitConverter.ToUInt32(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.Int64  => BitConverter.ToInt64(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.UInt64 => BitConverter.ToUInt64(raw, field.Offset).ToString(CultureInfo.InvariantCulture),
            ComponentFieldKind.Single => BitConverter.ToSingle(raw, field.Offset).ToString("R", CultureInfo.InvariantCulture),
            ComponentFieldKind.Double => BitConverter.ToDouble(raw, field.Offset).ToString("R", CultureInfo.InvariantCulture),
            _ => throw new InvalidOperationException($"Kind de campo desconhecido: {field.Kind}."),
        };
    }

    private static void ParseFieldInto(ComponentField? fieldOrNull, string token, byte[] raw, EntityId[] byDenseIndex)
    {
        if (fieldOrNull is not { } field) return; // caminho de campo desconhecido no esquema atual — ignorado

        if (field.Kind == ComponentFieldKind.EntityId)
        {
            EntityId resolved = token == "null"
                ? EntityId.Null
                : EntityRefCodec.FromDenseOrNull(int.Parse(token, CultureInfo.InvariantCulture), byDenseIndex);
            BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 4), resolved.Index);
            BitConverter.TryWriteBytes(raw.AsSpan(field.Offset + 4, 4), resolved.Version);
            return;
        }

        switch (field.Kind)
        {
            case ComponentFieldKind.Bool: raw[field.Offset] = token == "true" ? (byte)1 : (byte)0; break;
            case ComponentFieldKind.Byte: raw[field.Offset] = byte.Parse(token, CultureInfo.InvariantCulture); break;
            case ComponentFieldKind.SByte: raw[field.Offset] = unchecked((byte)sbyte.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Int16: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 2), short.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.UInt16: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 2), ushort.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Char: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 2), (char)int.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Int32: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 4), int.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.UInt32: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 4), uint.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Int64: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 8), long.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.UInt64: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 8), ulong.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Single: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 4), float.Parse(token, CultureInfo.InvariantCulture)); break;
            case ComponentFieldKind.Double: BitConverter.TryWriteBytes(raw.AsSpan(field.Offset, 8), double.Parse(token, CultureInfo.InvariantCulture)); break;
            default: throw new InvalidOperationException($"Kind de campo desconhecido: {field.Kind}.");
        }
    }

    private static ComponentField? FindField(ComponentDescriptor descriptor, string path)
    {
        foreach (var f in descriptor.Fields)
            if (f.Path == path) return f;
        return null;
    }

    // ------------------------------------------------------------ quoting/tokenização estilo shell
    // (mesmo "sabor" do FlowSerializer — ver comentário de classe)

    private static string Quote(string s)
    {
        var sb = new StringBuilder();
        sb.Append('"');
        foreach (var c in s)
        {
            if (c == '"' || c == '\\') sb.Append('\\');
            sb.Append(c);
        }
        sb.Append('"');
        return sb.ToString();
    }

    private static List<string> Tokenize(string line)
    {
        var toks = new List<string>();
        int i = 0;
        while (i < line.Length)
        {
            while (i < line.Length && line[i] == ' ') i++;
            if (i >= line.Length) break;

            if (line[i] == '"')
            {
                var sb = new StringBuilder();
                i++;
                while (i < line.Length && line[i] != '"')
                {
                    if (line[i] == '\\' && i + 1 < line.Length) { sb.Append(line[i + 1]); i += 2; }
                    else { sb.Append(line[i]); i++; }
                }
                i++; // fecha aspas
                toks.Add(sb.ToString());
            }
            else
            {
                int start = i;
                while (i < line.Length && line[i] != ' ') i++;
                toks.Add(line[start..i]);
            }
        }
        return toks;
    }
}
