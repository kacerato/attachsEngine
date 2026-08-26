using System.Globalization;
using System.Text;

namespace Aether.Configuration;

/// <summary>Tipo de valor armazenado numa chave. Union manual (não <c>object</c>) para evitar
/// boxing de <c>int</c>/<c>float</c>/<c>bool</c> — mesma disciplina de zero-alocação do resto do
/// projeto (docs/CONVENCOES.md §3), mesmo que configuração não seja caminho de frame; não custa
/// nada fazer certo aqui também.</summary>
public enum ConfigValueKind : byte
{
    Int = 0,
    Float = 1,
    Bool = 2,
    String = 3,
}

/// <summary>Um valor de configuração com seu tipo — só um dos campos é significativo, conforme
/// <see cref="Kind"/>. <see cref="StringValue"/> é a única referência gerenciada do struct.</summary>
public readonly struct ConfigValue
{
    public readonly ConfigValueKind Kind;
    private readonly int _intOrBoolValue;
    private readonly float _floatValue;
    public readonly string? StringValue;

    private ConfigValue(ConfigValueKind kind, int intOrBoolValue, float floatValue, string? stringValue)
    {
        Kind = kind;
        _intOrBoolValue = intOrBoolValue;
        _floatValue = floatValue;
        StringValue = stringValue;
    }

    public static ConfigValue Of(int value) => new(ConfigValueKind.Int, value, 0f, null);
    public static ConfigValue Of(float value) => new(ConfigValueKind.Float, 0, value, null);
    public static ConfigValue Of(bool value) => new(ConfigValueKind.Bool, value ? 1 : 0, 0f, null);
    public static ConfigValue Of(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        return new ConfigValue(ConfigValueKind.String, 0, 0f, value);
    }

    /// <summary>Lê como <c>int</c>. Lança se <see cref="Kind"/> não for <see cref="ConfigValueKind.Int"/> —
    /// ler com o tipo errado é erro de configuração/código, não um caso a mascarar com conversão
    /// implícita silenciosa (mesma disciplina de <c>ComponentField</c> recusar tipo incompatível).</summary>
    public int AsInt() => Kind == ConfigValueKind.Int
        ? _intOrBoolValue
        : throw new InvalidOperationException($"Valor é {Kind}, não Int.");

    public float AsFloat() => Kind == ConfigValueKind.Float
        ? _floatValue
        : throw new InvalidOperationException($"Valor é {Kind}, não Float.");

    public bool AsBool() => Kind == ConfigValueKind.Bool
        ? _intOrBoolValue != 0
        : throw new InvalidOperationException($"Valor é {Kind}, não Bool.");

    public string AsString() => Kind == ConfigValueKind.String
        ? StringValue!
        : throw new InvalidOperationException($"Valor é {Kind}, não String.");

    internal string SerializeValue() => Kind switch
    {
        ConfigValueKind.Int => _intOrBoolValue.ToString(CultureInfo.InvariantCulture),
        ConfigValueKind.Float => _floatValue.ToString("R", CultureInfo.InvariantCulture),
        ConfigValueKind.Bool => _intOrBoolValue != 0 ? "true" : "false",
        ConfigValueKind.String => EscapeString(StringValue!),
        _ => throw new InvalidOperationException($"Kind desconhecido: {Kind}"),
    };

    private static string EscapeString(string value) =>
        "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
}

/// <summary>
/// Item 1.5.4 do plano: "sistema de configuração/preferências". Um dicionário chave→valor
/// tipado, com defaults e leitura em camadas: <see cref="ConfigurationStore"/> por si só não
/// sabe de "categoria de projeto vs. de dispositivo" — o chamador compõe isso empilhando stores
/// (ex.: <c>projectDefaults</c> por baixo, <c>deviceOverride</c> por cima, via
/// <see cref="TryGet"/> em cascata) exatamente como <c>PhysicsWorldConfiguration</c> já separa
/// "capacidade default" de "override explícito" sem essa lógica morar dentro do tipo de dado.
/// <para>
/// Persistência é texto determinístico (chaves em ordem alfabética, mesmo <c>TextSerializer</c>):
/// <see cref="Serialize"/> devolve <c>string</c>, <see cref="Deserialize"/> recebe <c>string</c> —
/// sem acoplar a um path de arquivo ou stream, porque onde o texto mora (arquivo local, asset
/// Android, preferência do sistema) é decisão de plataforma que este tipo não deveria conhecer
/// (mesma separação que 1.1.1, abstração de filesystem, ainda não resolveu — não é escopo deste
/// item antecipar essa resposta).
/// </para>
/// </summary>
public sealed class ConfigurationStore
{
    private readonly Dictionary<string, ConfigValue> _values = new();

    public int Count => _values.Count;

    public void Set(string key, ConfigValue value)
    {
        ValidateKey(key);
        _values[key] = value;
    }

    public void SetInt(string key, int value) => Set(key, ConfigValue.Of(value));
    public void SetFloat(string key, float value) => Set(key, ConfigValue.Of(value));
    public void SetBool(string key, bool value) => Set(key, ConfigValue.Of(value));
    public void SetString(string key, string value) => Set(key, ConfigValue.Of(value));

    public bool TryGet(string key, out ConfigValue value) => _values.TryGetValue(key, out value);

    public bool Has(string key) => _values.ContainsKey(key);

    /// <summary>Remove a chave. No-op silencioso se não existir — mesma disciplina de
    /// idempotência de remoção do resto do projeto (<c>PhysicsWorld.DestroyBody</c>,
    /// <c>Signal&lt;T&gt;.Unsubscribe</c>).</summary>
    public void Remove(string key) => _values.Remove(key);

    /// <summary>Lê com valor default quando a chave não existir ou for do tipo errado. Ao
    /// contrário de <see cref="ConfigValue.AsInt"/>, aqui o tipo errado NÃO lança — este é o
    /// caminho de leitura "tolerante", pensado para configuração de usuário que pode ter sido
    /// editada à mão de forma incompatível; o caminho estrito é <see cref="TryGet"/> +
    /// <see cref="ConfigValue.AsInt"/> quando o chamador quer saber da divergência.</summary>
    public int GetIntOrDefault(string key, int defaultValue) =>
        TryGet(key, out var v) && v.Kind == ConfigValueKind.Int ? v.AsInt() : defaultValue;

    public float GetFloatOrDefault(string key, float defaultValue) =>
        TryGet(key, out var v) && v.Kind == ConfigValueKind.Float ? v.AsFloat() : defaultValue;

    public bool GetBoolOrDefault(string key, bool defaultValue) =>
        TryGet(key, out var v) && v.Kind == ConfigValueKind.Bool ? v.AsBool() : defaultValue;

    public string GetStringOrDefault(string key, string defaultValue) =>
        TryGet(key, out var v) && v.Kind == ConfigValueKind.String ? v.AsString() : defaultValue;

    private static void ValidateKey(string key)
    {
        if (string.IsNullOrWhiteSpace(key))
            throw new ArgumentException("Chave de configuração não pode ser vazia ou só espaço.", nameof(key));
        if (key.Contains(' ') || key.Contains('\n') || key.Contains('\t'))
            throw new ArgumentException($"Chave de configuração não pode conter espaço em branco: \"{key}\".", nameof(key));
    }

    /// <summary>
    /// Serializa em ordem alfabética de chave (ordinal), determinístico — mesmo motivo de
    /// <c>TextSerializer</c>: diff em controle de versão precisa de ordem estável, não a ordem de
    /// inserção do <see cref="Dictionary{TKey,TValue}"/> (que não é garantida entre execuções).
    /// Gramática por linha: <c>&lt;chave&gt; &lt;tipo&gt; &lt;valor&gt;</c>.
    /// </summary>
    public string Serialize()
    {
        var keys = new List<string>(_values.Keys);
        keys.Sort(StringComparer.Ordinal);

        var sb = new StringBuilder();
        foreach (var key in keys)
        {
            var value = _values[key];
            sb.Append(key).Append(' ').Append(value.Kind switch
            {
                ConfigValueKind.Int => "int",
                ConfigValueKind.Float => "float",
                ConfigValueKind.Bool => "bool",
                ConfigValueKind.String => "string",
                _ => throw new InvalidOperationException($"Kind desconhecido: {value.Kind}"),
            }).Append(' ').Append(value.SerializeValue()).Append('\n');
        }
        return sb.ToString();
    }

    /// <summary>
    /// Lê o formato de <see cref="Serialize"/>. Linha malformada, tipo desconhecido ou valor
    /// incompatível com o tipo declarado lança com contexto (linha + conteúdo) — mesma
    /// disciplina de "valor incompatível falha em vez de ser descartado" que
    /// <c>TextSerializer</c> já documenta, não um parser tolerante que mascara arquivo corrompido.
    /// Linhas vazias são ignoradas (permite espaçamento para legibilidade humana).
    /// </summary>
    public static ConfigurationStore Deserialize(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        var store = new ConfigurationStore();
        var lines = text.Split('\n');
        for (int lineIndex = 0; lineIndex < lines.Length; lineIndex++)
        {
            string line = lines[lineIndex].TrimEnd('\r');
            if (line.Length == 0) continue;

            (string key, string typeToken, string valueToken) = SplitLine(line, lineIndex);
            store.Set(key, ParseValue(typeToken, valueToken, lineIndex, line));
        }
        return store;
    }

    private static (string key, string typeToken, string valueToken) SplitLine(string line, int lineIndex)
    {
        int firstSpace = line.IndexOf(' ');
        if (firstSpace < 0)
            throw new FormatException($"Linha {lineIndex + 1} malformada (esperado \"chave tipo valor\"): \"{line}\".");
        int secondSpace = line.IndexOf(' ', firstSpace + 1);
        if (secondSpace < 0)
            throw new FormatException($"Linha {lineIndex + 1} malformada (esperado \"chave tipo valor\"): \"{line}\".");

        string key = line[..firstSpace];
        string typeToken = line[(firstSpace + 1)..secondSpace];
        string valueToken = line[(secondSpace + 1)..];
        return (key, typeToken, valueToken);
    }

    private static ConfigValue ParseValue(string typeToken, string valueToken, int lineIndex, string line)
    {
        try
        {
            return typeToken switch
            {
                "int" => ConfigValue.Of(int.Parse(valueToken, CultureInfo.InvariantCulture)),
                "float" => ConfigValue.Of(float.Parse(valueToken, CultureInfo.InvariantCulture)),
                "bool" => ConfigValue.Of(ParseBool(valueToken, lineIndex, line)),
                "string" => ConfigValue.Of(UnescapeString(valueToken, lineIndex, line)),
                _ => throw new FormatException($"Linha {lineIndex + 1}: tipo desconhecido \"{typeToken}\": \"{line}\"."),
            };
        }
        catch (Exception ex) when (ex is FormatException or OverflowException)
        {
            throw new FormatException($"Linha {lineIndex + 1}: valor \"{valueToken}\" incompatível com tipo \"{typeToken}\": \"{line}\".", ex);
        }
    }

    private static bool ParseBool(string token, int lineIndex, string line) => token switch
    {
        "true" => true,
        "false" => false,
        _ => throw new FormatException($"Linha {lineIndex + 1}: valor bool precisa ser \"true\"/\"false\", veio \"{token}\": \"{line}\"."),
    };

    private static string UnescapeString(string token, int lineIndex, string line)
    {
        if (token.Length < 2 || token[0] != '"' || token[^1] != '"')
            throw new FormatException($"Linha {lineIndex + 1}: valor string precisa estar entre aspas: \"{line}\".");

        var sb = new StringBuilder(token.Length - 2);
        for (int i = 1; i < token.Length - 1; i++)
        {
            if (token[i] == '\\' && i + 1 < token.Length - 1)
            {
                i++;
                sb.Append(token[i] switch
                {
                    '\\' => '\\',
                    '"' => '"',
                    _ => throw new FormatException($"Linha {lineIndex + 1}: escape inválido \"\\{token[i]}\": \"{line}\"."),
                });
            }
            else
            {
                sb.Append(token[i]);
            }
        }
        return sb.ToString();
    }
}
