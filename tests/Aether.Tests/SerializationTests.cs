using System.Runtime.CompilerServices;
using System.Text;
using Aether.Physics;
using Aether.Serialization;

namespace Aether.Tests;

/// <summary>
/// Registro de metadados de componente (Etapa 1.4.1) e os dois serializadores de mundo (1.4.2):
/// binário bit-exato e texto determinístico, com migração de esquema em cadeia.
/// </summary>
public static class SerializationTests
{
    // Os campos destas structs de teste nunca são atribuídos em C# — só existem para o registro de
    // metadados refletir sobre eles, e são preenchidos via cópia de bytes crus (AddComponentRaw) pelo
    // próprio serializador, nunca por atribuição direta. CS0649 é ruído aqui, não um bug real.
#pragma warning disable CS0649

    // Componente nunca registrado em ComponentRegistry — usado para testar que um tipo desconhecido
    // não trava o save/load, só é descartado.
    private struct ComponenteFantasma { public int X; }

    // Componentes usados só pelos testes de registro duplicado — cada um é tocado uma única vez em
    // todo o processo, então o estado global do ComponentRegistry entre testes nunca colide.
    private struct DuplicadoDeTipo { public int V; }
    private struct NomeDuplicadoA { public int V; }
    private struct NomeDuplicadoB { public int V; }
    private struct MigracaoAlvoInvalido { public int V; }

    // Componente "versão 2" de um esquema fictício cujos dois campos int foram reordenados em relação
    // à "versão 1" — o cenário de migração exigido pelo plano ("campo renomeado/reordenado entre
    // versão 1 e 2"). Só a forma ATUAL (v2) é registrada; a "v1" só existe como bytes crus montados à
    // mão no teste, simulando um arquivo salvo por uma versão antiga do jogo.
    private struct FakeComponentV2 { public int B; public int A; }

    // Regressão: bool seguido por um campo de 1 byte, sem nenhum campo de alinhamento maior no meio.
    // Layout gerenciado real: Ligado no offset 0, Extra no offset 1, struct de 2 bytes ao todo
    // (Unsafe.SizeOf). Se o offset de "Extra" fosse calculado como Marshal.OffsetOf calcularia (bool
    // marshalado como o BOOL de 4 bytes do Win32), daria offset 4 — fora dos limites de um buffer de
    // 2 bytes, e o acesso a esse offset lançaria IndexOutOfRangeException. Este teste existe porque
    // nenhum componente "de verdade" registrado hoje tem bool, então esse bug ficaria invisível até o
    // primeiro que tivesse.
    private struct SinalizadorSeguidoDeByte { public bool Ligado; public byte Extra; }

    // Schema textual atual (v3). V1 chamava o componente/field de OldTextSchema/OldValue;
    // v2 já persistia o id "value.primary". Added não existia e RemovedValue deixou de existir.
    private struct TextSchemaV3
    {
        public int CurrentValue;
        public int Added;
        public float Ratio;
    }

    private struct BinarySchemaV3
    {
        public int Value;
        public int Added;
    }

#pragma warning restore CS0649

    private static World NovoMundo() => new();

    private static World MundoComRegistro()
    {
        ComponentRegistryBootstrap.RegisterBuiltins();
        return NovoMundo();
    }

    private static void RegisterTextSchemaV3()
    {
        if (ComponentRegistry.TryGetByType(ComponentType.Of<TextSchemaV3>(), out _)) return;
        ComponentRegistry.Register<TextSchemaV3>("Aether.Tests.TextSchema", schemaVersion: 3, registration =>
        {
            registration.FormerlyNamed("Aether.Tests.OldTextSchema", "Aether.Tests.MiddleTextSchema");
            registration.Field(nameof(TextSchemaV3.CurrentValue), "value.primary", "OldValue", "MiddleValue");
            registration.Field(nameof(TextSchemaV3.Added), "value.added");
            registration.Field(nameof(TextSchemaV3.Ratio), "value.ratio", "OldRatio");
        });
    }

    private static void RegisterBinarySchemaV3()
    {
        if (ComponentRegistry.TryGetByType(ComponentType.Of<BinarySchemaV3>(), out _)) return;
        ComponentRegistry.Register<BinarySchemaV3>("Aether.Tests.BinarySchema", schemaVersion: 3,
            registration => registration.FormerlyNamed("Aether.Tests.OldBinarySchema"));
        ComponentRegistry.RegisterMigration<BinarySchemaV3>(1, v1 =>
        {
            var v2 = new byte[8];
            v1.AsSpan(0, 4).CopyTo(v2.AsSpan(0, 4));
            BitConverter.TryWriteBytes(v2.AsSpan(4, 4), 999); // campo transitório removido em v3
            return v2;
        });
        ComponentRegistry.RegisterMigration<BinarySchemaV3>(2, v2 =>
        {
            var v3 = new byte[8];
            v2.AsSpan(0, 4).CopyTo(v3.AsSpan(0, 4));
            // Added não existia: bytes zero/default no offset 4.
            return v3;
        });
    }

    // ------------------------------------------------------------ registro de metadados

    [Test] public static void Register_RecusaTipoDuplicado()
    {
        ComponentRegistry.Register<DuplicadoDeTipo>("Aether.Tests.DuplicadoDeTipo", schemaVersion: 1);
        Assert.Throws<InvalidOperationException>(
            () => ComponentRegistry.Register<DuplicadoDeTipo>("Aether.Tests.DuplicadoDeTipoOutroNome", schemaVersion: 1),
            "o mesmo tipo T não pode ser registrado duas vezes, nem com nome diferente");
    }

    [Test] public static void Register_RecusaNomeDuplicadoParaTipoDiferente()
    {
        ComponentRegistry.Register<NomeDuplicadoA>("Aether.Tests.NomeDuplicado", schemaVersion: 1);
        Assert.Throws<InvalidOperationException>(
            () => ComponentRegistry.Register<NomeDuplicadoB>("Aether.Tests.NomeDuplicado", schemaVersion: 1),
            "dois tipos diferentes não podem reivindicar o mesmo nome estável");
    }

    [Test] public static void RegisterBuiltins_EIdempotente()
    {
        ComponentRegistryBootstrap.RegisterBuiltins();
        ComponentRegistryBootstrap.RegisterBuiltins(); // não deve lançar na segunda chamada
        Assert.True(ComponentRegistry.TryGetByName("Aether.LocalTransform", out _));
        Assert.True(ComponentRegistry.TryGetByName("Aether.Physics.Trigger", out _));
    }

    [Test] public static void Trigger_SobreviveRoundTripBinarioETextoComFiltro()
    {
        var source = MundoComRegistro();
        source.CreateEntity(new Trigger(QueryLayerMask.Static, enabled: true));

        var binary = new MemoryStream();
        BinarySerializer.Write(source, binary);
        binary.Position = 0;
        var fromBinary = NovoMundo();
        BinarySerializer.Read(fromBinary, binary);
        var binaryTrigger = fromBinary.Read<Trigger>(new EntityId(0, 0));
        Assert.True(binaryTrigger.Enabled);
        Assert.Equal(QueryLayerMask.Static, binaryTrigger.EventLayerMask);

        string text = TextSerializer.Serialize(source);
        Assert.True(text.Contains("Aether.Physics.Trigger", StringComparison.Ordinal),
            "formato texto precisa persistir o nome estável do componente");
        var fromText = NovoMundo();
        TextSerializer.Deserialize(fromText, text);
        var textTrigger = fromText.Read<Trigger>(new EntityId(0, 0));
        Assert.True(textTrigger.Enabled);
        Assert.Equal(QueryLayerMask.Static, textTrigger.EventLayerMask);
    }

    [Test] public static void JointDeclarativa_SobreviveRoundTripBinarioETextoComReferencias()
    {
        var source = MundoComRegistro();
        var body1 = source.CreateEntity();
        var body2 = source.CreateEntity();
        source.CreateEntity(new Joint(body1, body2, JointKind.Hinge, JointSpace.LocalToBody1)
        {
            Point1 = new float3(1f, 2f, 3f),
            Point2 = new float3(4f, 5f, 6f),
            Axis1 = new float3(0f, 0f, 1f),
            Axis2 = new float3(0f, 0f, 1f),
            LimitsMin = -1.25f,
            LimitsMax = 1.5f,
            Motor = JointMotor.Position(0.5f, 20f, 3f, 0.75f),
        });

        var binary = new MemoryStream();
        BinarySerializer.Write(source, binary);
        binary.Position = 0;
        var fromBinary = NovoMundo();
        BinarySerializer.Read(fromBinary, binary);
        var binaryJoint = fromBinary.Read<Joint>(new EntityId(2, 0));
        Assert.Equal(new EntityId(0, 0), binaryJoint.Body1);
        Assert.Equal(new EntityId(1, 0), binaryJoint.Body2);
        Assert.Equal(JointKind.Hinge, binaryJoint.Kind);
        Assert.Equal(JointSpace.LocalToBody1, binaryJoint.Space);
        Assert.Close(0.5f, binaryJoint.Motor.TargetPosition);

        string text = TextSerializer.Serialize(source);
        Assert.True(text.Contains("Aether.Physics.Joint", StringComparison.Ordinal));
        var fromText = NovoMundo();
        TextSerializer.Deserialize(fromText, text);
        var textJoint = fromText.Read<Joint>(new EntityId(2, 0));
        Assert.Equal(new EntityId(0, 0), textJoint.Body1);
        Assert.Equal(new EntityId(1, 0), textJoint.Body2);
        Assert.Equal(JointSpace.LocalToBody1, textJoint.Space);
        Assert.Close(1.5f, textJoint.LimitsMax);
        Assert.Close(0.75f, textJoint.Motor.SpringDamping);
    }

    // ------------------------------------------------------------ binário: round-trip e hierarquia

    [Test] public static void RoundTripBinario_EBitExato()
    {
        var w1 = MundoComRegistro();
        var raiz = w1.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(1f, 2f, 3f))), new WorldTransform(Transform.Identity));
        var filho1 = w1.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0f, 1f, 0f))), new WorldTransform(Transform.Identity));
        var filho2 = w1.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0f, -1f, 0f))), new WorldTransform(Transform.Identity));
        Hierarchy.SetParent(w1, filho1, raiz);
        Hierarchy.SetParent(w1, filho2, raiz);
        w1.CreateEntity(); // entidade solta, sem nenhum componente

        var stream1 = new MemoryStream();
        BinarySerializer.Write(w1, stream1);

        var w2 = NovoMundo();
        stream1.Position = 0;
        BinarySerializer.Read(w2, stream1);

        var stream2 = new MemoryStream();
        BinarySerializer.Write(w2, stream2);

        Assert.Equal(w1.EntityCount, w2.EntityCount, "mesma contagem de entidades depois de recarregar");
        Assert.True(stream1.ToArray().AsSpan().SequenceEqual(stream2.ToArray()),
            "salvar -> carregar -> salvar de novo produz exatamente os mesmos bytes");
    }

    [Test] public static void Hierarquia_SobreviveAoRoundTripBinario()
    {
        var w1 = MundoComRegistro();
        var pai = w1.CreateEntity();
        var filho = w1.CreateEntity();
        Hierarchy.SetParent(w1, filho, pai);

        var stream = new MemoryStream();
        BinarySerializer.Write(w1, stream);
        stream.Position = 0;

        var w2 = NovoMundo();
        BinarySerializer.Read(w2, stream);

        // Índice denso 0 = pai (criado primeiro), 1 = filho — ver WorldSnapshot.
        var paiId = new EntityId(0, 0);
        var filhoId = new EntityId(1, 0);

        Assert.True(w2.HasComponent<FirstChild>(paiId), "o pai recupera o FirstChild apontando pro filho");
        Assert.Equal(filhoId, w2.Read<FirstChild>(paiId).Value);
        Assert.Equal(paiId, w2.Read<Parent>(filhoId).Value, "o filho recupera o Parent apontando pro pai");
        Assert.Equal(EntityId.Null, w2.Read<NextSibling>(filhoId).Value, "filho único: próximo irmão é nulo");
    }

    [Test] public static void ComponenteNaoRegistrado_EIgnoradoAoSalvarEAoCarregar()
    {
        var w1 = MundoComRegistro();
        var e = w1.CreateEntity(new LocalTransform(Transform.Identity), new WorldTransform(Transform.Identity));
        w1.AddComponent(e, new ComponenteFantasma { X = 42 });

        var stream = new MemoryStream();
        BinarySerializer.Write(w1, stream); // não lança mesmo com um componente sem descritor

        var w2 = NovoMundo();
        stream.Position = 0;
        BinarySerializer.Read(w2, stream); // idem na leitura

        var id = new EntityId(0, 0);
        Assert.True(w2.Exists(id), "a entidade em si sobrevive — só o componente desconhecido é descartado");
        Assert.True(w2.HasComponent<LocalTransform>(id), "componentes registrados continuam presentes");
        Assert.False(w2.HasComponent<ComponenteFantasma>(id), "um componente nunca registrado não é persistido");
    }

    [Test] public static void MundoVazio_SerializaEDesserializaSemExcecao()
    {
        var w1 = MundoComRegistro();
        var stream = new MemoryStream();
        BinarySerializer.Write(w1, stream);
        stream.Position = 0;

        var w2 = NovoMundo();
        BinarySerializer.Read(w2, stream);
        Assert.Equal(0, w2.EntityCount);
    }

    [Test] public static void Read_RecusaWorldNaoVazio()
    {
        var w1 = MundoComRegistro();
        w1.CreateEntity();
        var stream = new MemoryStream();
        BinarySerializer.Write(w1, stream);
        stream.Position = 0;

        var destino = NovoMundo();
        destino.CreateEntity(); // já não está mais vazio
        Assert.Throws<InvalidOperationException>(() => BinarySerializer.Read(destino, stream),
            "índices densos só valem se o World de destino nunca criou nem destruiu nada antes");
    }

    // ------------------------------------------------------------ texto: determinismo e round-trip

    [Test] public static void RoundTripTexto_EDeterministico()
    {
        var w = MundoComRegistro();
        var a = w.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(1f, 2f, 3f))), new WorldTransform(Transform.Identity));
        var b = w.CreateEntity();
        Hierarchy.SetParent(w, b, a);

        string text1 = TextSerializer.Serialize(w);
        string text2 = TextSerializer.Serialize(w);
        Assert.Equal(text1, text2, "duas serializações do mesmo World, sem mudar nada entre elas, produzem o mesmo texto");
    }

    [Test] public static void RoundTripTexto_PreservaValoresEHierarquia()
    {
        var w1 = MundoComRegistro();
        var pai = w1.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(5f, -2f, 9f))), new WorldTransform(Transform.Identity));
        var filho = w1.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0f, 0f, 1f))), new WorldTransform(Transform.Identity));
        Hierarchy.SetParent(w1, filho, pai);

        string text = TextSerializer.Serialize(w1);

        var w2 = NovoMundo();
        TextSerializer.Deserialize(w2, text);

        var paiId = new EntityId(0, 0);
        var filhoId = new EntityId(1, 0);
        Assert.Close(new float3(5f, -2f, 9f), w2.Read<LocalTransform>(paiId).Value.Position, 1e-6f, "posição local do pai sobrevive");
        Assert.Equal(paiId, w2.Read<Parent>(filhoId).Value, "hierarquia sobrevive ao round-trip de texto");

        string text2 = TextSerializer.Serialize(w2);
        Assert.Equal(text, text2, "resserializar o mundo recarregado bate byte a byte com o texto original");
    }

    [Test] public static void OffsetDeCampoAposBool_NaoUsaLayoutMarshaledDoInteropERespeitaOTamanhoRealDaStruct()
    {
        // Prova direta de que ComputeImmediateOffset (não Marshal.OffsetOf) é quem calcula o offset:
        // um componente de 2 bytes só round-tripa sem exceção se "Extra" for lido/escrito no offset 1
        // de verdade. Ver o comentário em SinalizadorSeguidoDeByte para por que este layout expõe a
        // divergência (bool seguido de um campo de 1 byte, sem padding de alinhamento entre eles).
        ComponentRegistry.Register<SinalizadorSeguidoDeByte>("Aether.Tests.SinalizadorSeguidoDeByte", schemaVersion: 1);
        Assert.Equal(2, Unsafe.SizeOf<SinalizadorSeguidoDeByte>(), "pré-condição do teste: o componente realmente tem 2 bytes, sem padding");

        var w1 = NovoMundo();
        var e = w1.CreateEntity(new SinalizadorSeguidoDeByte { Ligado = true, Extra = 200 });

        var stream = new MemoryStream();
        BinarySerializer.Write(w1, stream);
        var w2 = NovoMundo();
        stream.Position = 0;
        BinarySerializer.Read(w2, stream);
        var idBin = new EntityId(0, 0);
        Assert.True(w2.Read<SinalizadorSeguidoDeByte>(idBin).Ligado, "bool sobrevive ao round-trip binário");
        Assert.Equal((byte)200, w2.Read<SinalizadorSeguidoDeByte>(idBin).Extra, "byte após o bool sobrevive ao round-trip binário, sem estourar os limites do buffer");

        string text = TextSerializer.Serialize(w1);
        var w3 = NovoMundo();
        TextSerializer.Deserialize(w3, text);
        var idText = new EntityId(0, 0);
        Assert.True(w3.Read<SinalizadorSeguidoDeByte>(idText).Ligado, "bool sobrevive ao round-trip de texto");
        Assert.Equal((byte)200, w3.Read<SinalizadorSeguidoDeByte>(idText).Extra, "byte após o bool sobrevive ao round-trip de texto");
    }

    [Test] public static void MundoVazio_SerializaEDesserializaSemExcecaoTexto()
    {
        var w1 = MundoComRegistro();
        string text = TextSerializer.Serialize(w1);
        var w2 = NovoMundo();
        TextSerializer.Deserialize(w2, text);
        Assert.Equal(0, w2.EntityCount);
    }

    [Test] public static void Deserialize_RecusaWorldNaoVazio()
    {
        var w1 = MundoComRegistro();
        string text = TextSerializer.Serialize(w1);

        var destino = NovoMundo();
        destino.CreateEntity();
        Assert.Throws<InvalidOperationException>(() => TextSerializer.Deserialize(destino, text));
    }

    [Test] public static void TextoV1ParaV3_PreservaRenameAdicaoRemocaoETipoCompativel()
    {
        RegisterTextSchemaV3();
        const string fixtureV1 =
            "mundo 1\n" +
            "entidade 0\n" +
            "  componente \"Aether.Tests.OldTextSchema\" 1\n" +
            "    campo \"OldValue\" 123\n" +
            "    campo \"OldRatio\" 2\n" +
            "    campo \"RemovedValue\" 999\n";

        var world = NovoMundo();
        TextSerializer.Deserialize(world, fixtureV1);
        var value = world.Read<TextSchemaV3>(new EntityId(0, 0));
        Assert.Equal(123, value.CurrentValue, "alias v1 preserva o valor após rename");
        Assert.Equal(0, value.Added, "campo adicionado em v3 recebe default");
        Assert.Close(2f, value.Ratio, what: "número inteiro textual é compatível com float atual");

        string v3 = TextSerializer.Serialize(world);
        Assert.True(v3.StartsWith("mundo 2\n", StringComparison.Ordinal));
        Assert.True(v3.Contains("componente \"Aether.Tests.TextSchema\" 3", StringComparison.Ordinal));
        Assert.True(v3.Contains("campo \"value.primary\" 123", StringComparison.Ordinal));
        Assert.False(v3.Contains("OldValue", StringComparison.Ordinal));
        Assert.False(v3.Contains("RemovedValue", StringComparison.Ordinal));
    }

    [Test] public static void TextoV2ParaV3_IdEstavelSobreviveASegundoRename()
    {
        RegisterTextSchemaV3();
        const string fixtureV2 =
            "mundo 2\n" +
            "entidade 0\n" +
            "  componente \"Aether.Tests.MiddleTextSchema\" 2\n" +
            "    campo \"value.primary\" 456\n" +
            "    campo \"value.ratio\" 1.5\n";

        var world = NovoMundo();
        TextSerializer.Deserialize(world, fixtureV2);
        var value = world.Read<TextSchemaV3>(new EntityId(0, 0));
        Assert.Equal(456, value.CurrentValue);
        Assert.Close(1.5f, value.Ratio);
        Assert.Equal(0, value.Added);
    }

    [Test] public static void Texto_MudancaIncompativelEFuturoFalhamComContexto()
    {
        RegisterTextSchemaV3();
        const string incompatible =
            "mundo 2\nentidade 0\n  componente \"Aether.Tests.TextSchema\" 3\n" +
            "    campo \"value.primary\" nao-e-int\n";
        const string future =
            "mundo 2\nentidade 0\n  componente \"Aether.Tests.TextSchema\" 4\n";

        Assert.Throws<FormatException>(() => TextSerializer.Deserialize(NovoMundo(), incompatible),
            "mudança incompatível não pode virar zero silenciosamente");
        Assert.Throws<FormatException>(() => TextSerializer.Deserialize(NovoMundo(), future),
            "schema de versão futura não pode ser interpretado como o atual");
    }

    [Test] public static void Texto_CampoDuplicadoPorAliasEhRecusado()
    {
        RegisterTextSchemaV3();
        const string duplicate =
            "mundo 1\nentidade 0\n  componente \"Aether.Tests.OldTextSchema\" 1\n" +
            "    campo \"OldValue\" 1\n    campo \"value.primary\" 2\n";
        Assert.Throws<FormatException>(() => TextSerializer.Deserialize(NovoMundo(), duplicate),
            "alias e id apontando para o mesmo campo não podem sobrescrever um ao outro por ordem");
    }

    // ------------------------------------------------------------ migração de esquema (binário)

    [Test] public static void MigracaoDeEsquema_AplicaCadeiaAoLerUmBlobVersao1()
    {
        // "Versão 2" (a única forma que o código atual conhece) tem os campos B e A trocados de lugar
        // em relação à "versão 1" — só existe como bytes crus abaixo, nunca como um tipo CLR real.
        ComponentRegistry.Register<FakeComponentV2>("Aether.Tests.FakeComponent", schemaVersion: 2);
        ComponentRegistry.RegisterMigration<FakeComponentV2>(fromVersion: 1, v1Bytes =>
        {
            int a = BitConverter.ToInt32(v1Bytes, 0); // v1: (A, B) nessa ordem
            int b = BitConverter.ToInt32(v1Bytes, 4);
            var v2Bytes = new byte[8];
            BitConverter.TryWriteBytes(v2Bytes.AsSpan(0, 4), b); // v2: (B, A) — campos trocados
            BitConverter.TryWriteBytes(v2Bytes.AsSpan(4, 4), a);
            return v2Bytes;
        });

        var stream = new MemoryStream();
        using (var w = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true))
        {
            w.Write(new byte[] { (byte)'A', (byte)'E', (byte)'B', (byte)'F' }); // magic
            w.Write(1);                              // versão de formato de contêiner
            w.Write(1);                              // 1 entidade
            w.Write(1);                              // 1 tipo na tabela
            w.Write("Aether.Tests.FakeComponent");   // nome estável
            w.Write(1);                              // gravado com a versão de esquema 1
            w.Write(0);                              // índice denso da entidade 0
            w.Write(1);                              // 1 componente presente
            w.Write(0);                              // índice na tabela de tipos
            w.Write(8);                              // tamanho do payload (2 x int32)
            w.Write(BitConverter.GetBytes(111));      // A = 111
            w.Write(BitConverter.GetBytes(222));      // B = 222
        }
        stream.Position = 0;

        var world = NovoMundo();
        BinarySerializer.Read(world, stream);

        var id = new EntityId(0, 0);
        var migrado = world.Read<FakeComponentV2>(id);
        Assert.Equal(111, migrado.A, "campo A preserva o valor através da migração, mesmo mudando de offset");
        Assert.Equal(222, migrado.B, "campo B preserva o valor através da migração, mesmo mudando de offset");
    }

    [Test] public static void MigracaoBinariaV1ParaV3_AplicaDoisElosEAliasDeComponente()
    {
        RegisterBinarySchemaV3();
        var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(new byte[] { (byte)'A', (byte)'E', (byte)'B', (byte)'F' });
            writer.Write(1); // container
            writer.Write(1); // entidades
            writer.Write(1); // tipos
            writer.Write("Aether.Tests.OldBinarySchema");
            writer.Write(1); // schema v1
            writer.Write(0); // entidade 0
            writer.Write(1); // componentes
            writer.Write(0); // tipo 0
            writer.Write(4); // v1 tinha somente Value
            writer.Write(321);
        }
        stream.Position = 0;

        var world = NovoMundo();
        BinarySerializer.Read(world, stream);
        var migrated = world.Read<BinarySchemaV3>(new EntityId(0, 0));
        Assert.Equal(321, migrated.Value, "v1→v2→v3 preserva Value");
        Assert.Equal(0, migrated.Added, "campo adicionado em v3 recebe default");
    }

    [Test] public static void RegisterMigration_RecusaVersaoInvalida()
    {
        ComponentRegistry.Register<MigracaoAlvoInvalido>("Aether.Tests.MigracaoInvalidaAlvo", schemaVersion: 1);
        Assert.Throws<InvalidOperationException>(
            () => ComponentRegistry.RegisterMigration<MigracaoAlvoInvalido>(fromVersion: 1, b => b),
            "não existe versão 2 para migrar até — schemaVersion atual é 1, então fromVersion precisa ser < 1, e não há nenhum válido");
    }
}
