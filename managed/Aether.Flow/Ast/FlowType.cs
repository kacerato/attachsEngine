// NOTA DE DESEMPENHO: ao contrário do resto do repositório, o código sob
// Aether.Flow.Ast NÃO segue a regra de "zero alocação no caminho de frame".
// A AST é dado de EDITOR (montada por um humano tocando na tela, salva em
// disco, reconstruída quando um arquivo .aflow é aberto) — nunca é percorrida
// dentro do laço de jogo em tempo real. Aqui priorizamos clareza e facilidade
// de manutenção sobre alocação zero: `sealed class`, `List<T>`, `Dictionary`,
// LINQ onde ajudar a legibilidade. O código GERADO a partir da AST (C# ou o
// interpretador em modo build/IL) é que precisa obedecer às regras normais.

namespace Aether.Flow.Ast;

/// <summary>
/// Sistema de tipos do AetherFlow. Cobre os tipos que os nós de dado podem
/// carregar nos pinos, mais <see cref="Exec"/> (fluxo de execução, não dado)
/// e <see cref="Any"/> (genérico limitado, usado por nós como "Log" que aceitam
/// qualquer coisa formatável).
/// </summary>
public enum FlowType
{
    Bool,
    Int,
    Float,
    Float3,
    Quaternion,
    String,
    Entity,
    Asset,
    Exec,
    Any,
}

/// <summary>
/// Valor literal de um pino desconectado, ou valor em trânsito durante a
/// interpretação. É uma união marcada simples — um valor por vez, indicado
/// por <see cref="Type"/>.
/// </summary>
public sealed class FlowValue
{
    public FlowType Type { get; }
    public bool BoolValue { get; }
    public long IntValue { get; }
    public double FloatValue { get; }
    public string StringValue { get; }
    public float3 Float3Value { get; }
    public quaternion QuaternionValue { get; }

    private FlowValue(FlowType type, bool b = false, long i = 0, double f = 0,
        string s = "", float3 v3 = default, quaternion q = default)
    {
        Type = type;
        BoolValue = b;
        IntValue = i;
        FloatValue = f;
        StringValue = s;
        Float3Value = v3;
        QuaternionValue = q;
    }

    public static FlowValue OfBool(bool b) => new(FlowType.Bool, b: b);
    public static FlowValue OfInt(long i) => new(FlowType.Int, i: i);
    public static FlowValue OfFloat(double f) => new(FlowType.Float, f: f);
    public static FlowValue OfString(string s) => new(FlowType.String, s: s);
    public static FlowValue OfFloat3(float3 v) => new(FlowType.Float3, v3: v);
    public static FlowValue OfQuaternion(quaternion q) => new(FlowType.Quaternion, q: q);
    public static FlowValue OfEntity(long id) => new(FlowType.Entity, i: id);
    public static FlowValue OfAsset(string path) => new(FlowType.Asset, s: path);

    /// <summary>Valor "vazio" razoável para o tipo — usado quando um pino de dado
    /// não tem nem conexão nem literal (o validador ainda assim reporta isso como
    /// erro; este valor só existe para o interpretador não explodir em cascata).</summary>
    public static FlowValue Default(FlowType type) => type switch
    {
        FlowType.Bool => OfBool(false),
        FlowType.Int => OfInt(0),
        FlowType.Float => OfFloat(0),
        FlowType.Float3 => OfFloat3(Aether.float3.Zero),
        FlowType.Quaternion => OfQuaternion(Aether.quaternion.Identity),
        FlowType.String => OfString(""),
        FlowType.Entity => OfEntity(-1),
        FlowType.Asset => OfAsset(""),
        _ => OfInt(0),
    };

    public double AsNumber() => Type switch
    {
        FlowType.Int => IntValue,
        FlowType.Float => FloatValue,
        _ => throw new InvalidOperationException($"valor do tipo {Type} não é numérico"),
    };

    public override string ToString() => Type switch
    {
        FlowType.Bool => BoolValue ? "true" : "false",
        FlowType.Int => IntValue.ToString(System.Globalization.CultureInfo.InvariantCulture),
        FlowType.Float => FloatValue.ToString("R", System.Globalization.CultureInfo.InvariantCulture),
        FlowType.String => StringValue,
        FlowType.Float3 => Float3Value.ToString(),
        FlowType.Quaternion => QuaternionValue.ToString(),
        FlowType.Entity => $"Entity#{IntValue}",
        FlowType.Asset => $"Asset({StringValue})",
        _ => Type.ToString(),
    };

    /// <summary>Literal C# equivalente, usado pelo gerador de código.</summary>
    public string ToCSharpLiteral() => Type switch
    {
        FlowType.Bool => BoolValue ? "true" : "false",
        FlowType.Int => IntValue.ToString(System.Globalization.CultureInfo.InvariantCulture),
        FlowType.Float => FloatValue.ToString("R", System.Globalization.CultureInfo.InvariantCulture) + "f",
        FlowType.String => "\"" + StringValue.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"",
        FlowType.Float3 => $"new float3({F(Float3Value.X)}f, {F(Float3Value.Y)}f, {F(Float3Value.Z)}f)",
        _ => ToString(),
    };

    private static string F(float v) => v.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
}
