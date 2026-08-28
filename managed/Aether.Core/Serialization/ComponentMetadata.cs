using System.Reflection;
using System.Runtime.InteropServices;

namespace Aether.Serialization;

/// <summary>
/// Tipos de campo folha que o registro de metadados sabe formatar/interpretar (texto) ou localizar em
/// bytes crus (binário). "Folha" porque a árvore de campos de um componente é achatada recursivamente
/// até chegar num destes — ver <see cref="ComponentRegistry"/>.
/// <para>
/// <see cref="EntityId"/> é tratado como folha (não decomposto em Index/Version soltos) de propósito:
/// o serializador binário precisa remapear o par inteiro inteiro para o índice denso do arquivo como
/// uma unidade só — ver <c>EntityRefCodec</c> em <c>WorldSnapshot.cs</c>.
/// </para>
/// </summary>
public enum ComponentFieldKind
{
    Bool, Byte, SByte, Int16, UInt16, Char, Int32, UInt32, Int64, UInt64, Single, Double,
    EntityId, Guid,
}

/// <summary>
/// Um campo folha achatado a partir da árvore de campos públicos de um componente — ex.: o campo
/// <c>Value</c> de <c>WorldTransform</c> (que é um <c>Transform</c>) vira as folhas
/// <c>Value.Position.X</c>, <c>Value.Position.Y</c>, ..., <c>Value.Rotation.W</c>. Só usado pelo
/// formato texto e pelo remapeamento de referências de entidade no formato binário — o formato binário
/// nunca decompõe um componente em campos para copiar dados, ele copia os bytes crus inteiros.
/// </summary>
public readonly struct ComponentField
{
    /// <summary>Identidade persistente do campo. Diferente de <see cref="Path"/>, não muda quando
    /// o nome CLR muda; pode ser configurada explicitamente no registro.</summary>
    public readonly string Id;

    /// <summary>Caminho pontilhado da raiz do componente até esta folha, ex.: <c>"Value.Position.X"</c>.</summary>
    public readonly string Path;
    public readonly ComponentFieldKind Kind;

    /// <summary>Nomes/caminhos usados por versões antigas do formato texto.</summary>
    public readonly string[] Aliases;

    /// <summary>Offset em bytes desta folha dentro do layout cru do componente (não do tipo aninhado
    /// que a contém diretamente) — soma dos offsets de cada nível do caminho.</summary>
    internal readonly int Offset;

    /// <summary>Tamanho em bytes desta folha: 1, 2, 4, 8 ou 16 conforme <see cref="Kind"/> (16 para Guid; 8 para
    /// <see cref="ComponentFieldKind.EntityId"/>, que é <c>{int Index; int Version;}</c>).</summary>
    internal readonly int Size;

    internal ComponentField(string id, string path, ComponentFieldKind kind, int offset, int size, string[] aliases)
    { Id = id; Path = path; Kind = kind; Offset = offset; Size = size; Aliases = aliases; }
}

/// <summary>Configuração explícita de identidades persistentes e aliases usada durante o registro
/// de um componente. É consumida imediatamente; não permanece mutável no descriptor.</summary>
public sealed class ComponentRegistration
{
    internal readonly List<string> ComponentAliases = new();
    internal readonly Dictionary<string, FieldRule> FieldRules = new(StringComparer.Ordinal);

    internal readonly record struct FieldRule(string Id, string[] Aliases);

    public void FormerlyNamed(params string[] aliases)
    {
        ArgumentNullException.ThrowIfNull(aliases);
        foreach (string alias in aliases)
        {
            if (string.IsNullOrWhiteSpace(alias))
                throw new ArgumentException("Alias de componente não pode ser vazio.", nameof(aliases));
            if (ComponentAliases.Contains(alias, StringComparer.Ordinal))
                throw new InvalidOperationException($"Alias de componente duplicado: \"{alias}\".");
            ComponentAliases.Add(alias);
        }
    }

    /// <summary>Associa o caminho CLR atual a um id persistente e, opcionalmente, aos caminhos
    /// usados antes do rename.</summary>
    public void Field(string currentPath, string stableId, params string[] formerlySerializedAs)
    {
        if (string.IsNullOrWhiteSpace(currentPath)) throw new ArgumentException("Caminho atual não pode ser vazio.", nameof(currentPath));
        if (string.IsNullOrWhiteSpace(stableId)) throw new ArgumentException("Id estável não pode ser vazio.", nameof(stableId));
        ArgumentNullException.ThrowIfNull(formerlySerializedAs);
        if (!FieldRules.TryAdd(currentPath,
                new FieldRule(stableId, formerlySerializedAs.ToArray())))
            throw new InvalidOperationException($"O campo \"{currentPath}\" já foi configurado.");
    }
}

/// <summary>
/// Descreve um tipo de componente para fins de serialização e edição.
/// <para>
/// <b>Por que isto existe em vez de um source generator Roslyn (item 1.4.1 do plano):</b> este
/// repositório tem política arquitetural de zero dependências externas — <c>NuGet.config</c> limpa
/// todas as fontes de pacote de propósito (ver <c>docs/CONVENCOES.md</c> §7). Um source generator de
/// verdade exigiria referenciar <c>Microsoft.CodeAnalysis.CSharp</c> via NuGet, o que violaria essa
/// política. Em vez disso, isto é um <b>registro de metadados em tempo de execução</b>, populado por
/// chamadas explícitas de registro (nunca reflexão automática de assembly — isso seria mágico demais e
/// ruim para trimming/AOT no futuro, quando o editor precisar rodar no próprio celular). A API foi
/// desenhada para que trocar isto por um source generator no futuro seja só uma troca de
/// <i>implementação</i> — o <see cref="ComponentDescriptor"/> continua tendo nome estável, tipo,
/// versão de esquema e campos nomeados; só a forma como ele é POPULADO mudaria, de chamadas explícitas
/// para código gerado que faz as mesmas chamadas.
/// </para>
/// </summary>
public readonly struct ComponentDescriptor
{
    /// <summary>Nome estável do componente, persistido em arquivo e mostrado no editor — independente
    /// do nome do tipo CLR (que pode mudar num refactor sem quebrar arquivos salvos, desde que o nome
    /// estável seja mantido).</summary>
    public readonly string Name;

    public readonly ComponentType Type;

    /// <summary>Versão de esquema ATUAL deste componente no código rodando agora. Um arquivo salvo com
    /// uma versão menor precisa passar pela cadeia de migração (<see cref="ComponentRegistry.RegisterMigration{T}"/>)
    /// antes de ser interpretado como este tipo.</summary>
    public readonly int SchemaVersion;

    public readonly Type ClrType;

    /// <summary>Nomes estáveis anteriores aceitos na leitura. A gravação sempre usa <see cref="Name"/>.</summary>
    public readonly string[] Aliases;

    /// <summary>Campos folha, achatados e em ordem alfabética de <see cref="ComponentField.Path"/> —
    /// a ordem alfabética (não a ordem de declaração) é o que torna o formato texto determinístico
    /// independente da ordem em que <see cref="System.Type.GetFields()"/> devolve os campos (a
    /// especificação do CLR não garante essa ordem).</summary>
    public readonly ComponentField[] Fields;

    internal ComponentDescriptor(string name, ComponentType type, int schemaVersion, Type clrType,
        string[] aliases, ComponentField[] fields)
    { Name = name; Type = type; SchemaVersion = schemaVersion; ClrType = clrType; Aliases = aliases; Fields = fields; }

    internal ComponentField? FindField(string persistedIdOrAlias)
    {
        foreach (var field in Fields)
        {
            if (field.Id == persistedIdOrAlias || field.Path == persistedIdOrAlias) return field;
            foreach (string alias in field.Aliases)
                if (alias == persistedIdOrAlias) return field;
        }
        return null;
    }
}

/// <summary>
/// Registro global de metadados de componente. Precisa ser populado explicitamente antes de qualquer
/// serialização — ver <see cref="ComponentRegistryBootstrap.RegisterBuiltins"/>. Registro acontece na
/// inicialização do processo, de forma single-threaded, como o resto do bootstrap da engine; não há
/// necessidade de sincronização fina aqui.
/// </summary>
public static class ComponentRegistry
{
    private static readonly Dictionary<int, ComponentDescriptor> s_byTypeId = new();
    private static readonly Dictionary<string, ComponentDescriptor> s_byName = new(StringComparer.Ordinal);
    private static readonly List<ComponentDescriptor> s_all = new();

    // Cadeia de migração por tipo: ComponentType.Id -> (versão de origem -> transforma bytes N em N+1).
    private static readonly Dictionary<int, Dictionary<int, Func<byte[], byte[]>>> s_migrations = new();

    private const int MaxFieldDepth = 8;

    /// <summary>
    /// Registra <typeparamref name="T"/> com um nome estável e a versão de esquema atual. Reflete
    /// sobre os campos públicos de <typeparamref name="T"/> uma única vez, aqui — depois disso, ler e
    /// escrever esse componente nunca mais precisa de reflexão (só dos offsets já calculados).
    /// </summary>
    public static void Register<T>(string name, int schemaVersion) where T : unmanaged
        => Register<T>(name, schemaVersion, configure: null);

    /// <summary>Registra um componente com ids/aliases persistentes explícitos. Paths sem regra
    /// usam o próprio caminho como id por compatibilidade.</summary>
    public static void Register<T>(string name, int schemaVersion, Action<ComponentRegistration>? configure)
        where T : unmanaged
    {
        if (string.IsNullOrWhiteSpace(name)) throw new ArgumentException("Nome estável não pode ser vazio.", nameof(name));
        if (schemaVersion < 1) throw new ArgumentOutOfRangeException(nameof(schemaVersion), "Schema version precisa ser >= 1.");
        var type = ComponentType.Of<T>();
        if (s_byTypeId.ContainsKey(type.Id))
            throw new InvalidOperationException(
                $"O componente {typeof(T).FullName} já está registrado (como \"{s_byTypeId[type.Id].Name}\").");
        if (s_byName.TryGetValue(name, out var existing))
            throw new InvalidOperationException(
                $"O nome de componente \"{name}\" já está registrado para outro tipo ({existing.ClrType.FullName}).");

        var registration = new ComponentRegistration();
        configure?.Invoke(registration);
        var fields = BuildFields(typeof(T), registration);
        var aliases = registration.ComponentAliases.ToArray();

        var claimedNames = new HashSet<string>(StringComparer.Ordinal) { name };
        foreach (string alias in aliases)
        {
            if (!claimedNames.Add(alias))
                throw new InvalidOperationException($"Nome/alias de componente duplicado: \"{alias}\".");
            if (s_byName.TryGetValue(alias, out existing))
                throw new InvalidOperationException(
                    $"O alias de componente \"{alias}\" já pertence a {existing.ClrType.FullName}.");
        }

        var descriptor = new ComponentDescriptor(name, type, schemaVersion, typeof(T), aliases, fields);
        s_byTypeId[type.Id] = descriptor;
        s_byName[name] = descriptor;
        foreach (string alias in aliases) s_byName[alias] = descriptor;
        s_all.Add(descriptor);
    }

    public static bool TryGetByType(ComponentType type, out ComponentDescriptor descriptor) =>
        s_byTypeId.TryGetValue(type.Id, out descriptor);

    public static bool TryGetByName(string name, out ComponentDescriptor descriptor) =>
        s_byName.TryGetValue(name, out descriptor);

    public static ComponentDescriptor GetByType(ComponentType type) =>
        TryGetByType(type, out var d) ? d : throw new InvalidOperationException($"Nenhum componente registrado para o ComponentType #{type.Id}.");

    public static ComponentDescriptor GetByName(string name) =>
        TryGetByName(name, out var d) ? d : throw new InvalidOperationException($"Nenhum componente registrado com o nome \"{name}\".");

    /// <summary>Todos os descritores registrados, na ordem de registro — usado pelo serializador para
    /// iterar "todo componente que a engine conhece", ex.: ao montar a tabela de tipos de um arquivo.</summary>
    public static IReadOnlyList<ComponentDescriptor> All => s_all;

    /// <summary>
    /// Registra o elo de migração da versão <paramref name="fromVersion"/> para
    /// <paramref name="fromVersion"/> + 1 de <typeparamref name="T"/>. A cadeia completa até a versão
    /// atual é aplicada elo por elo por <see cref="Migrate"/> — cada <paramref name="migrate"/> só
    /// precisa saber transformar UMA versão na seguinte, nunca pular versões.
    /// </summary>
    public static void RegisterMigration<T>(int fromVersion, Func<byte[], byte[]> migrate) where T : unmanaged
    {
        var type = ComponentType.Of<T>();
        if (!s_byTypeId.TryGetValue(type.Id, out var descriptor))
            throw new InvalidOperationException($"Registre o componente {typeof(T).FullName} antes de registrar uma migração para ele.");
        if (fromVersion < 1 || fromVersion >= descriptor.SchemaVersion)
            throw new InvalidOperationException(
                $"Versão de origem de migração inválida para {typeof(T).FullName}: {fromVersion} " +
                $"(precisa estar entre 1 e {descriptor.SchemaVersion - 1}, a versão atual registrada é {descriptor.SchemaVersion}).");

        if (!s_migrations.TryGetValue(type.Id, out var chain))
        { chain = new Dictionary<int, Func<byte[], byte[]>>(); s_migrations[type.Id] = chain; }

        if (!chain.TryAdd(fromVersion, migrate))
            throw new InvalidOperationException($"Já existe uma migração da versão {fromVersion} para o componente {typeof(T).FullName}.");
    }

    /// <summary>Aplica a cadeia de migração de <paramref name="fromVersion"/> até <paramref name="toVersion"/>,
    /// elo por elo. Devolve <paramref name="bytes"/> sem cópia se as versões já baterem.</summary>
    internal static byte[] Migrate(ComponentType type, int fromVersion, int toVersion, byte[] bytes)
    {
        if (fromVersion == toVersion) return bytes;
        if (fromVersion > toVersion)
            throw new InvalidOperationException(
                $"O componente #{type.Id} foi salvo com a versão de esquema {fromVersion}, mais nova que a versão " +
                $"{toVersion} deste build — o arquivo foi salvo por uma versão futura da engine; downgrade não é suportado.");

        if (!s_migrations.TryGetValue(type.Id, out var chain))
            throw new InvalidOperationException(
                $"O componente #{type.Id} foi salvo com a versão de esquema {fromVersion}, mas não há nenhuma migração " +
                $"registrada para ele — registre uma cadeia com ComponentRegistry.RegisterMigration antes de ler este arquivo.");

        var current = bytes;
        for (int v = fromVersion; v < toVersion; v++)
        {
            if (!chain.TryGetValue(v, out var step))
                throw new InvalidOperationException(
                    $"Falta o elo de migração da versão {v} para {v + 1} do componente #{type.Id} " +
                    $"(a cadeia registrada não é contínua até a versão atual {toVersion}).");
            current = step(current);
        }
        return current;
    }

    // ------------------------------------------------------------ reflexão (só em tempo de registro)

    /// <summary>
    /// Achata recursivamente os campos públicos de instância de <paramref name="root"/> em folhas
    /// primitivas, calculando o offset absoluto de cada uma dentro do layout cru de <paramref name="root"/>
    /// via <see cref="ComputeImmediateOffset"/> encadeado nível a nível. Roda uma única vez, no
    /// registro — depois disso o formato texto e o remapeamento de <see cref="EntityId"/> no formato
    /// binário só leem os offsets já calculados, sem tocar em reflexão de novo.
    ///
    /// <para><b>Por que não <see cref="Marshal.OffsetOf(Type, string)"/></b> (a escolha óbvia, e a que
    /// uma primeira versão deste arquivo usava): <c>Marshal.OffsetOf</c> calcula o offset no layout
    /// MARSHALED/interop, que não é garantido bater com o layout gerenciado real que <see
    /// cref="MemoryMarshal.AsBytes{T}(Span{T})"/> e o resto da engine (chunks do ECS, o serializador
    /// binário) enxergam. O caso concreto que expõe a diferença: <c>bool</c> marshala por padrão como
    /// o <c>BOOL</c> de 4 bytes do Win32, mas ocupa 1 byte de verdade na memória gerenciada. Um
    /// componente hipotético <c>struct X { bool Flag; byte Depois; }</c> tem <c>Depois</c> no offset 1
    /// de verdade — comprovado experimentalmente nesta máquina (<c>Unsafe.SizeOf&lt;X&gt;() == 2</c>) —
    /// mas <c>Marshal.OffsetOf</c> devolve 4. Nenhum componente registrado hoje tem <c>bool</c>, então
    /// esse bug ficaria invisível até o primeiro que tivesse, corrompendo silenciosamente todo campo
    /// depois dele. <see cref="ComputeImmediateOffset"/> descobre o offset observando o layout
    /// gerenciado de verdade (preenche o campo com um valor "todo-bits-1" e observa qual byte mudou em
    /// relação a uma instância zerada), nunca a visão de interop.</para>
    /// </summary>
    private static ComponentField[] BuildFields(Type root, ComponentRegistration registration)
    {
        var discovered = new List<(string Path, ComponentFieldKind Kind, int Offset, int Size)>();
        Walk(root, "", 0, 0);
        discovered.Sort((a, b) => string.CompareOrdinal(a.Path, b.Path));

        var knownPaths = new HashSet<string>(discovered.Select(f => f.Path), StringComparer.Ordinal);
        foreach (string configuredPath in registration.FieldRules.Keys)
            if (!knownPaths.Contains(configuredPath))
                throw new InvalidOperationException(
                    $"Componente {root.FullName}: configuração referencia campo inexistente \"{configuredPath}\".");

        var claimedKeys = new Dictionary<string, string>(StringComparer.Ordinal);
        var fields = new ComponentField[discovered.Count];
        for (int i = 0; i < discovered.Count; i++)
        {
            var f = discovered[i];
            bool configured = registration.FieldRules.TryGetValue(f.Path, out var rule);
            string id = configured ? rule.Id : f.Path;
            string[] aliases = configured ? rule.Aliases : Array.Empty<string>();
            Claim(id, f.Path);
            Claim(f.Path, f.Path);
            foreach (string alias in aliases)
            {
                if (string.IsNullOrWhiteSpace(alias))
                    throw new InvalidOperationException($"Componente {root.FullName}: alias vazio no campo \"{f.Path}\".");
                Claim(alias, f.Path);
            }
            fields[i] = new ComponentField(id, f.Path, f.Kind, f.Offset, f.Size, aliases.ToArray());
        }
        return fields;

        void Claim(string key, string path)
        {
            if (claimedKeys.TryGetValue(key, out string? owner) && owner != path)
                throw new InvalidOperationException(
                    $"Componente {root.FullName}: id/path/alias \"{key}\" é ambíguo entre \"{owner}\" e \"{path}\".");
            claimedKeys[key] = path;
        }

        void Walk(Type t, string prefix, int baseOffset, int depth)
        {
            if (depth > MaxFieldDepth)
                throw new InvalidOperationException(
                    $"Componente {root.FullName}: campo aninhado demais em \"{prefix}\" (profundidade > {MaxFieldDepth}) — " +
                    "provavelmente um tipo aninhado em si mesmo, o que não é um componente blittable válido.");

            foreach (var f in t.GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                string path = prefix.Length == 0 ? f.Name : $"{prefix}.{f.Name}";
                int offset = baseOffset + ComputeImmediateOffset(t, f);

                if (f.FieldType == typeof(EntityId))
                {
                    discovered.Add((path, ComponentFieldKind.EntityId, offset, 8));
                    continue;
                }

                if (TryLeafKind(f.FieldType, out var kind, out int size))
                {
                    discovered.Add((path, kind, offset, size));
                    continue;
                }

                if (f.FieldType.IsEnum)
                    throw new InvalidOperationException(
                        $"Componente {root.FullName}: campo \"{path}\" é um enum ({f.FieldType.Name}) — " +
                        "enums ainda não são suportados pelo registro de metadados.");

                if (!f.FieldType.IsValueType)
                    throw new InvalidOperationException(
                        $"Componente {root.FullName}: campo \"{path}\" não é um tipo de valor ({f.FieldType.FullName}) — " +
                        "componentes precisam ser inteiramente blittable.");

                Walk(f.FieldType, path, offset, depth + 1);
            }
        }
    }

    /// <summary>
    /// Offset em bytes do campo <paramref name="field"/> dentro do layout gerenciado REAL de
    /// <paramref name="parentType"/> (não o layout de interop — ver a nota em <see cref="BuildFields"/>).
    /// Técnica: duas instâncias de <paramref name="parentType"/> zeradas, preenche o campo-alvo de uma
    /// delas com um valor "todo-bits-1" (<see cref="AllOnes"/>) via reflexão direta sobre o campo
    /// (nunca passa por marshaling), reinterpreta as duas como bytes crus com <see
    /// cref="MemoryMarshal.AsBytes{T}(Span{T})"/> (a mesma primitiva que o resto da engine usa para ver
    /// structs como bytes) e devolve a posição do primeiro byte que difere.
    /// </summary>
    private static int ComputeImmediateOffset(Type parentType, FieldInfo field)
    {
        object baseline = Activator.CreateInstance(parentType)!; // todos os campos no valor default (zero)
        object probe = Activator.CreateInstance(parentType)!;
        field.SetValue(probe, AllOnes(field.FieldType)); // grava direto no campo, nunca via construtor/marshaling

        byte[] baselineBytes = StructToBytes(parentType, baseline);
        byte[] probeBytes = StructToBytes(parentType, probe);

        for (int i = 0; i < baselineBytes.Length; i++)
            if (baselineBytes[i] != probeBytes[i]) return i;

        throw new InvalidOperationException(
            $"Não foi possível localizar o offset do campo \"{field.Name}\" em {parentType.FullName} " +
            "(nenhum byte mudou ao preenchê-lo — o tipo do campo é vazio?).");
    }

    /// <summary>
    /// Um valor de <paramref name="type"/> com todo bit gerenciado ligado — recursivo para tipos
    /// aninhados, preenchendo campo a campo via reflexão (nunca via construtor, para não disparar
    /// nenhuma normalização/validação que o tipo tenha). Usado só para observar layout em
    /// <see cref="ComputeImmediateOffset"/>, nunca guardado de verdade em nenhum componente.
    /// </summary>
    private static object AllOnes(Type type)
    {
        if (type == typeof(bool)) return true;
        if (type == typeof(byte)) return (byte)0xFF;
        if (type == typeof(sbyte)) return (sbyte)-1;
        if (type == typeof(short)) return (short)-1;
        if (type == typeof(ushort)) return (ushort)0xFFFF;
        if (type == typeof(char)) return (char)0xFFFF;
        if (type == typeof(int)) return -1;
        if (type == typeof(uint)) return uint.MaxValue;
        if (type == typeof(long)) return -1L;
        if (type == typeof(ulong)) return ulong.MaxValue;
        if (type == typeof(float)) return BitConverter.Int32BitsToSingle(-1);
        if (type == typeof(double)) return BitConverter.Int64BitsToDouble(-1L);
        if (type == typeof(EntityId)) return new EntityId(-1, -1);
        if (type == typeof(Guid)) return new Guid("ffffffff-ffff-ffff-ffff-ffffffffffff");

        // Tipo aninhado (ex.: float3 dentro de Transform): preenche cada campo público recursivamente.
        object nested = Activator.CreateInstance(type)!;
        foreach (var f in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
            f.SetValue(nested, AllOnes(f.FieldType));
        return nested;
    }

    /// <summary>Bytes crus do layout gerenciado real de <paramref name="value"/> (tipo
    /// <paramref name="type"/>), via <see cref="MemoryMarshal.AsBytes{T}(Span{T})"/> — invocado por
    /// reflexão porque <paramref name="type"/> só é conhecido em tempo de execução aqui.</summary>
    private static byte[] StructToBytes(Type type, object value)
    {
        var method = typeof(ComponentRegistry).GetMethod(nameof(StructToBytesGeneric), BindingFlags.NonPublic | BindingFlags.Static)!;
        return (byte[])method.MakeGenericMethod(type).Invoke(null, new[] { value })!;
    }

    private static byte[] StructToBytesGeneric<T>(object boxedValue) where T : unmanaged
    {
        T value = (T)boxedValue;
        return MemoryMarshal.AsBytes(MemoryMarshal.CreateSpan(ref value, 1)).ToArray();
    }

    private static bool TryLeafKind(Type t, out ComponentFieldKind kind, out int size)
    {
        if (t == typeof(Guid)) { kind = ComponentFieldKind.Guid; size = 16; return true; }
        if (t == typeof(bool))   { kind = ComponentFieldKind.Bool;   size = 1; return true; }
        if (t == typeof(byte))   { kind = ComponentFieldKind.Byte;   size = 1; return true; }
        if (t == typeof(sbyte))  { kind = ComponentFieldKind.SByte;  size = 1; return true; }
        if (t == typeof(short))  { kind = ComponentFieldKind.Int16;  size = 2; return true; }
        if (t == typeof(ushort)) { kind = ComponentFieldKind.UInt16; size = 2; return true; }
        if (t == typeof(char))   { kind = ComponentFieldKind.Char;   size = 2; return true; }
        if (t == typeof(int))    { kind = ComponentFieldKind.Int32;  size = 4; return true; }
        if (t == typeof(uint))   { kind = ComponentFieldKind.UInt32; size = 4; return true; }
        if (t == typeof(float))  { kind = ComponentFieldKind.Single; size = 4; return true; }
        if (t == typeof(long))   { kind = ComponentFieldKind.Int64;  size = 8; return true; }
        if (t == typeof(ulong))  { kind = ComponentFieldKind.UInt64; size = 8; return true; }
        if (t == typeof(double)) { kind = ComponentFieldKind.Double; size = 8; return true; }
        kind = default; size = 0; return false;
    }
}
