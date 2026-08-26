using System.Globalization;
using System.Text;
using Aether.Flow.Ast;
using Aether.Flow.Runtime;

namespace Aether.Flow.Serialization;

/// <summary>
/// Formato de arquivo <c>.aflow</c>: texto simples, uma entidade por linha,
/// pensado para dar diff e merge legíveis em controle de versão (o plano exige
/// isso explicitamente). Não é JSON genérico via reflexão — é um formato
/// próprio, plano, com uma gramática pequena o bastante para caber na cabeça.
///
/// A mesma AST sempre produz os mesmos bytes: nós, conexões e variáveis são
/// escritos na ordem em que aparecem nas listas do <see cref="FlowGraph"/>, e
/// os dicionários (Literais, Propriedades) são escritos em ordem alfabética de
/// chave — isso é o que torna o diff útil (duas rodadas de serialização do
/// mesmo grafo nunca divergem por causa da ordem de iteração de um Dictionary).
/// </summary>
public static class FlowSerializer
{
    public static string Serialize(FlowGraph graph)
    {
        var sb = new StringBuilder();
        sb.Append("grafo ").Append(Quote(graph.Name)).Append('\n');

        foreach (var v in graph.Variables)
        {
            sb.Append("variavel ").Append(v.Scope).Append(' ').Append(v.Type).Append(' ')
              .Append(Quote(v.Name)).Append(' ').Append(FormatValue(v.Initial)).Append('\n');
        }

        foreach (var node in graph.Nodes)
        {
            sb.Append("no ").Append(Quote(node.Id)).Append(' ').Append(Quote(node.NodeType)).Append(' ')
              .Append(F(node.CanvasX)).Append(' ').Append(F(node.CanvasY)).Append('\n');

            if (node.RequiredCapabilities != FlowCapability.None)
                sb.Append("  capacidades ")
                  .Append(node.RequiredCapabilities.ToString().Replace(", ", "|", StringComparison.Ordinal))
                  .Append('\n');

            foreach (var pin in node.Inputs)
                sb.Append("  entrada ").Append(Quote(pin.Name)).Append(' ').Append(pin.Type).Append('\n');
            foreach (var pin in node.Outputs)
                sb.Append("  saida ").Append(Quote(pin.Name)).Append(' ').Append(pin.Type).Append('\n');
            foreach (var key in node.Literals.Keys.OrderBy(k => k, StringComparer.Ordinal))
            {
                var lit = node.Literals[key];
                sb.Append("  literal ").Append(Quote(key)).Append(' ').Append(lit.Type).Append(' ').Append(FormatValue(lit)).Append('\n');
            }
            foreach (var key in node.Properties.Keys.OrderBy(k => k, StringComparer.Ordinal))
                sb.Append("  prop ").Append(Quote(key)).Append(' ').Append(Quote(node.Properties[key])).Append('\n');
        }

        foreach (var c in graph.Connections)
        {
            sb.Append("conexao ").Append(Quote(c.From.NodeId)).Append(' ').Append(Quote(c.From.PinName))
              .Append(" -> ").Append(Quote(c.To.NodeId)).Append(' ').Append(Quote(c.To.PinName)).Append('\n');
        }

        return sb.ToString();
    }

    public static FlowGraph Deserialize(string text)
    {
        var lines = text.Replace("\r\n", "\n").Split('\n');
        FlowGraph? graph = null;
        FlowNode? current = null;

        foreach (var rawLine in lines)
        {
            if (rawLine.Length == 0) continue;
            bool indented = rawLine[0] == ' ';
            var toks = Tokenize(rawLine);
            if (toks.Count == 0) continue;

            if (!indented)
            {
                current = null;
                switch (toks[0])
                {
                    case "grafo":
                        graph = new FlowGraph { Name = toks[1] };
                        break;
                    case "variavel":
                    {
                        var scope = Enum.Parse<VarScope>(toks[1]);
                        var type = Enum.Parse<FlowType>(toks[2]);
                        string name = toks[3];
                        var value = ParseValue(type, toks, 4);
                        graph!.Variables.Add(new FlowVariable { Name = name, Type = type, Initial = value, Scope = scope });
                        break;
                    }
                    case "no":
                    {
                        current = new FlowNode { Id = toks[1], NodeType = toks[2] };
                        current.CanvasX = float.Parse(toks[3], CultureInfo.InvariantCulture);
                        current.CanvasY = float.Parse(toks[4], CultureInfo.InvariantCulture);
                        graph!.Nodes.Add(current);
                        break;
                    }
                    case "conexao":
                    {
                        // toks: conexao fromNode fromPin -> toNode toPin
                        graph!.Connections.Add(new FlowConnection
                        {
                            From = new FlowEndpoint(toks[1], toks[2]),
                            To = new FlowEndpoint(toks[4], toks[5]),
                        });
                        break;
                    }
                    default:
                        throw new FormatException($"linha .aflow desconhecida: {rawLine}");
                }
            }
            else
            {
                if (current is null) throw new FormatException($"linha indentada fora de um nó: {rawLine}");
                switch (toks[0])
                {
                    case "capacidades":
                        current.RequiredCapabilities = Enum.Parse<FlowCapability>(
                            toks[1].Replace('|', ','));
                        break;
                    case "entrada":
                        current.Inputs.Add(new FlowPin { Name = toks[1], Type = Enum.Parse<FlowType>(toks[2]), Direction = PinDirection.Input });
                        break;
                    case "saida":
                        current.Outputs.Add(new FlowPin { Name = toks[1], Type = Enum.Parse<FlowType>(toks[2]), Direction = PinDirection.Output });
                        break;
                    case "literal":
                    {
                        string pin = toks[1];
                        var type = Enum.Parse<FlowType>(toks[2]);
                        current.Literals[pin] = ParseValue(type, toks, 3);
                        break;
                    }
                    case "prop":
                        current.Properties[toks[1]] = toks[2];
                        break;
                    default:
                        throw new FormatException($"linha .aflow desconhecida dentro de um nó: {rawLine}");
                }
            }
        }

        return graph ?? throw new FormatException("arquivo .aflow vazio ou sem cabeçalho 'grafo'.");
    }

    // ---- valores tipados ----

    private static string FormatValue(FlowValue v) => v.Type switch
    {
        FlowType.Bool => v.BoolValue ? "true" : "false",
        FlowType.Int => v.IntValue.ToString(CultureInfo.InvariantCulture),
        FlowType.Float => v.FloatValue.ToString("R", CultureInfo.InvariantCulture),
        FlowType.String => Quote(v.StringValue),
        FlowType.Float3 => $"{F(v.Float3Value.X)} {F(v.Float3Value.Y)} {F(v.Float3Value.Z)}",
        FlowType.Entity => v.IntValue.ToString(CultureInfo.InvariantCulture),
        FlowType.Asset => Quote(v.StringValue),
        _ => Quote(v.StringValue),
    };

    private static FlowValue ParseValue(FlowType declaredType, List<string> toks, int start) => declaredType switch
    {
        FlowType.Bool => FlowValue.OfBool(toks[start] == "true"),
        FlowType.Int => FlowValue.OfInt(long.Parse(toks[start], CultureInfo.InvariantCulture)),
        FlowType.Float => FlowValue.OfFloat(double.Parse(toks[start], CultureInfo.InvariantCulture)),
        FlowType.String => FlowValue.OfString(toks[start]),
        FlowType.Float3 => FlowValue.OfFloat3(new float3(
            float.Parse(toks[start], CultureInfo.InvariantCulture),
            float.Parse(toks[start + 1], CultureInfo.InvariantCulture),
            float.Parse(toks[start + 2], CultureInfo.InvariantCulture))),
        FlowType.Entity => FlowValue.OfEntity(long.Parse(toks[start], CultureInfo.InvariantCulture)),
        FlowType.Asset => FlowValue.OfAsset(toks[start]),
        _ => FlowValue.OfString(toks[start]),
    };

    private static string F(float v) => v.ToString("R", CultureInfo.InvariantCulture);

    // ---- quoting simples estilo shell ----

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
