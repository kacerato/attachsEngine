using Aether.Configuration;

namespace Aether.Tests;

/// <summary>Testes do item 1.5.4: sistema de configuração/preferências.</summary>
public static class ConfigurationStoreTests
{
    [Test] public static void SetGet_IntFloatBoolString_RoundtripPorTipo()
    {
        var store = new ConfigurationStore();
        store.SetInt("graphics.msaa", 4);
        store.SetFloat("audio.masterVolume", 0.75f);
        store.SetBool("graphics.vsync", true);
        store.SetString("player.name", "Jorge");

        Assert.Equal(4, store.GetIntOrDefault("graphics.msaa", -1));
        Assert.Close(0.75f, store.GetFloatOrDefault("audio.masterVolume", -1f));
        Assert.True(store.GetBoolOrDefault("graphics.vsync", false));
        Assert.Equal("Jorge", store.GetStringOrDefault("player.name", ""));
    }

    [Test] public static void GetOrDefault_ChaveAusente_DevolveDefault()
    {
        var store = new ConfigurationStore();
        Assert.Equal(42, store.GetIntOrDefault("nao.existe", 42));
        Assert.Close(1.5f, store.GetFloatOrDefault("nao.existe", 1.5f));
        Assert.False(store.GetBoolOrDefault("nao.existe", false));
        Assert.Equal("padrao", store.GetStringOrDefault("nao.existe", "padrao"));
    }

    [Test] public static void GetOrDefault_TipoErrado_DevolveDefaultSemLancar()
    {
        // Caminho tolerante: ler um int como se fosse float não deveria lançar, só cair no
        // default — diferente de ConfigValue.AsFloat(), que é o caminho estrito.
        var store = new ConfigurationStore();
        store.SetInt("valor", 10);
        Assert.Close(-1f, store.GetFloatOrDefault("valor", -1f), what: "ler um Int como Float deveria cair no default, não lançar");
    }

    [Test] public static void ConfigValue_AsTipoErrado_Lanca()
    {
        var value = ConfigValue.Of(10);
        Assert.Throws<InvalidOperationException>(() => value.AsFloat(), "ler um Int como Float pelo caminho estrito deveria lançar");
        Assert.Throws<InvalidOperationException>(() => value.AsString());
        Assert.Throws<InvalidOperationException>(() => value.AsBool());
    }

    [Test] public static void TryGet_ChaveExistente_DevolveTrueEValor()
    {
        var store = new ConfigurationStore();
        store.SetInt("chave", 99);
        Assert.True(store.TryGet("chave", out var value));
        Assert.Equal(99, value.AsInt());
    }

    [Test] public static void TryGet_ChaveAusente_DevolveFalse()
    {
        var store = new ConfigurationStore();
        Assert.False(store.TryGet("nao.existe", out _));
    }

    [Test] public static void Set_SobrescreveValorExistente()
    {
        var store = new ConfigurationStore();
        store.SetInt("chave", 1);
        store.SetInt("chave", 2);
        Assert.Equal(2, store.GetIntOrDefault("chave", -1));
        Assert.Equal(1, store.Count, "sobrescrever não deveria criar uma segunda entrada");
    }

    [Test] public static void Set_TrocaDeTipoNaMesmaChave_SubstituiCompletamente()
    {
        var store = new ConfigurationStore();
        store.SetInt("chave", 5);
        store.SetString("chave", "agora string");
        Assert.False(store.TryGet("chave", out var value) && value.Kind == ConfigValueKind.Int);
        Assert.Equal("agora string", store.GetStringOrDefault("chave", ""));
    }

    [Test] public static void Remove_ChaveExistente_RemoveDeVerdade()
    {
        var store = new ConfigurationStore();
        store.SetInt("chave", 1);
        store.Remove("chave");
        Assert.False(store.Has("chave"));
        Assert.Equal(0, store.Count);
    }

    [Test] public static void Remove_ChaveAusente_EhNoOp()
    {
        var store = new ConfigurationStore();
        store.Remove("nao.existe"); // não deveria lançar
        Assert.Equal(0, store.Count);
    }

    [Test] public static void Set_ChaveVaziaOuComEspaco_Lanca()
    {
        var store = new ConfigurationStore();
        Assert.Throws<ArgumentException>(() => store.SetInt("", 1), "chave vazia não é válida");
        Assert.Throws<ArgumentException>(() => store.SetInt("   ", 1), "chave só com espaço não é válida");
        Assert.Throws<ArgumentException>(() => store.SetInt("tem espaco", 1), "chave com espaço quebraria o parser de linha \"chave tipo valor\"");
    }

    [Test] public static void Serialize_OrdenaChavesAlfabeticamente_Deterministico()
    {
        var store = new ConfigurationStore();
        store.SetInt("zebra", 1);
        store.SetInt("abacaxi", 2);
        store.SetInt("meio", 3);

        string serialized = store.Serialize();
        int abacaxiIndex = serialized.IndexOf("abacaxi", StringComparison.Ordinal);
        int meioIndex = serialized.IndexOf("meio", StringComparison.Ordinal);
        int zebraIndex = serialized.IndexOf("zebra", StringComparison.Ordinal);

        Assert.True(abacaxiIndex < meioIndex && meioIndex < zebraIndex,
            "chaves deveriam aparecer em ordem alfabética ordinal, não ordem de inserção");
    }

    [Test] public static void Serialize_MesmoStoreDuasVezes_ProduzTextoIdentico()
    {
        var store = new ConfigurationStore();
        store.SetInt("a", 1);
        store.SetFloat("b", 2.5f);
        store.SetBool("c", true);
        store.SetString("d", "texto");

        Assert.Equal(store.Serialize(), store.Serialize(), "serializar o mesmo store duas vezes deveria produzir bytes idênticos");
    }

    [Test] public static void Deserialize_RoundTrip_PreservaTodosOsValores()
    {
        var original = new ConfigurationStore();
        original.SetInt("graphics.msaa", 4);
        original.SetFloat("audio.masterVolume", 0.75f);
        original.SetBool("graphics.vsync", true);
        original.SetString("player.name", "Jorge Aragão");

        var roundtripped = ConfigurationStore.Deserialize(original.Serialize());

        Assert.Equal(4, roundtripped.GetIntOrDefault("graphics.msaa", -1));
        Assert.Close(0.75f, roundtripped.GetFloatOrDefault("audio.masterVolume", -1f));
        Assert.True(roundtripped.GetBoolOrDefault("graphics.vsync", false));
        Assert.Equal("Jorge Aragão", roundtripped.GetStringOrDefault("player.name", ""));
    }

    [Test] public static void Serialize_StringComAspasEBarra_EscapaERoundtrip()
    {
        var store = new ConfigurationStore();
        store.SetString("chave", "valor com \"aspas\" e \\barra\\");
        var roundtripped = ConfigurationStore.Deserialize(store.Serialize());
        Assert.Equal("valor com \"aspas\" e \\barra\\", roundtripped.GetStringOrDefault("chave", ""));
    }

    [Test] public static void Deserialize_LinhasVazias_SaoIgnoradas()
    {
        var store = ConfigurationStore.Deserialize("a int 1\n\nb int 2\n");
        Assert.Equal(2, store.Count);
    }

    [Test] public static void Deserialize_TextoVazio_ProduzStoreVazio()
    {
        var store = ConfigurationStore.Deserialize("");
        Assert.Equal(0, store.Count);
    }

    [Test] public static void Deserialize_LinhaMalformadaSemTipo_Lanca()
    {
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("chaveSemTipoOuValor"));
    }

    [Test] public static void Deserialize_TipoDesconhecido_Lanca()
    {
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("chave tipoinvalido 1"));
    }

    [Test] public static void Deserialize_ValorIncompativelComTipo_LancaComContexto()
    {
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("chave int naoehnumero"));
    }

    [Test] public static void Deserialize_BoolInvalido_Lanca()
    {
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("chave bool talvez"));
    }

    [Test] public static void Deserialize_StringSemAspas_Lanca()
    {
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("chave string semaspas"));
    }

    [Test] public static void Deserialize_ArquivoCorrompido_NaoDescartaSilenciosamente()
    {
        // Mesma disciplina documentada em TextSerializer: valor incompatível FALHA, não é
        // descartado silenciosamente como se a chave nunca tivesse existido.
        Assert.Throws<FormatException>(() => ConfigurationStore.Deserialize("bom int 1\nruim int naoehnumero\n"));
    }

    [Test] public static void ConfigValue_Of_StringNula_Lanca()
    {
        Assert.Throws<ArgumentNullException>(() => ConfigValue.Of((string)null!));
    }

    [Test] public static void Deserialize_TextoNulo_Lanca()
    {
        Assert.Throws<ArgumentNullException>(() => ConfigurationStore.Deserialize(null!));
    }
}
