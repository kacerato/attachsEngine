using System.Text;
using Aether.Flow.Ast;

namespace Aether.Flow.CodeGen;

/// <summary>
/// Gera C# legível a partir de um <see cref="FlowGraph"/> — a vista "Código"
/// da Parte 9.2 do plano. Cobre o subconjunto documentado em
/// <see cref="CSharpToFlow"/> (o mesmo subconjunto que o parser lê de volta);
/// qualquer coisa fora disso deveria ter chegado à AST como um nó
/// <c>code.raw</c>, que aqui é apenas devolvido como texto.
/// </summary>
public static class FlowToCSharp
{
    public static string Generate(FlowGraph graph)
    {
        var sb = new StringBuilder();
        string className = SanitizeIdentifier(graph.Name);
        sb.Append("public class ").Append(className).Append('\n');
        sb.Append("{\n");

        foreach (var variable in graph.Variables)
            sb.Append("    public ").Append(CSharpTypeName(variable.Type)).Append(' ').Append(variable.Name)
              .Append(" = ").Append(variable.Initial.ToCSharpLiteral()).Append(";\n");

        bool first = graph.Variables.Count == 0;
        foreach (var evt in graph.EventNodes)
        {
            if (!first) sb.Append('\n');
            first = false;
            EmitEvent(graph, evt, sb);
        }

        sb.Append("}\n");
        return sb.ToString();
    }

    private static void EmitEvent(FlowGraph graph, FlowNode evt, StringBuilder sb)
    {
        switch (evt.NodeType)
        {
            case NodeTypes.EventStart:
                sb.Append("    public void Start()\n    {\n");
                EmitExecChain(graph, evt.Id, "corpo", 2, sb);
                sb.Append("    }\n");
                break;
            case NodeTypes.EventUpdate:
                sb.Append("    public void Update(float dt)\n    {\n");
                EmitExecChain(graph, evt.Id, "corpo", 2, sb);
                sb.Append("    }\n");
                break;
            case NodeTypes.EventCollision:
                sb.Append("    public void OnCollision(Entity outro)\n    {\n");
                EmitExecChain(graph, evt.Id, "corpo", 2, sb);
                sb.Append("    }\n");
                break;
            default:
                sb.Append("    // evento desconhecido: ").Append(evt.NodeType).Append('\n');
                break;
        }
    }

    /// <summary>Emite a sequência de instruções alcançada a partir do pino de execução
    /// <paramref name="pinName"/> do nó <paramref name="nodeId"/>, seguindo conexões
    /// de execução até não haver mais nenhuma (fim do bloco).</summary>
    private static void EmitExecChain(FlowGraph graph, string nodeId, string pinName, int indent, StringBuilder sb)
    {
        var conn = graph.OutgoingFrom(nodeId, pinName).FirstOrDefault();
        if (conn is null) return;
        EmitStatement(graph, conn.To.NodeId, indent, sb);
    }

    private static void EmitStatement(FlowGraph graph, string nodeId, int indent, StringBuilder sb)
    {
        var node = graph.FindNode(nodeId)!;
        string pad = new(' ', indent * 4);

        switch (node.NodeType)
        {
            case NodeTypes.SetVariable:
            {
                string varName = node.Properties["VariableName"];
                string expr = EmitExpression(graph, node, "valor", 0);
                sb.Append(pad).Append(varName).Append(" = ").Append(expr).Append(";\n");
                EmitExecChain(graph, nodeId, "saida", indent, sb);
                break;
            }
            case NodeTypes.FlowIf:
            {
                string cond = EmitExpression(graph, node, "condicao", 0);
                sb.Append(pad).Append("if (").Append(cond).Append(")\n").Append(pad).Append("{\n");
                EmitExecChain(graph, nodeId, "entao", indent + 1, sb);
                sb.Append(pad).Append("}\n");
                if (graph.OutgoingFrom(nodeId, "senao").Any())
                {
                    sb.Append(pad).Append("else\n").Append(pad).Append("{\n");
                    EmitExecChain(graph, nodeId, "senao", indent + 1, sb);
                    sb.Append(pad).Append("}\n");
                }
                EmitExecChain(graph, nodeId, "depois", indent, sb);
                break;
            }
            case NodeTypes.FlowWhile:
            {
                string cond = EmitExpression(graph, node, "condicao", 0);
                sb.Append(pad).Append("while (").Append(cond).Append(")\n").Append(pad).Append("{\n");
                EmitExecChain(graph, nodeId, "corpo", indent + 1, sb);
                sb.Append(pad).Append("}\n");
                EmitExecChain(graph, nodeId, "fim", indent, sb);
                break;
            }
            case NodeTypes.FlowReturn:
                sb.Append(pad).Append("return;\n");
                break;
            case NodeTypes.FlowBreak:
                sb.Append(pad).Append("break;\n");
                break;
            case NodeTypes.FlowContinue:
                sb.Append(pad).Append("continue;\n");
                break;
            case NodeTypes.CodeRaw:
            {
                string raw = node.Properties.GetValueOrDefault("RawCode", "");
                foreach (var line in raw.Split('\n'))
                    sb.Append(pad).Append(line.TrimEnd('\r')).Append('\n');
                EmitExecChain(graph, nodeId, "saida", indent, sb);
                break;
            }
            default:
                sb.Append(pad).Append("// nó não suportado pelo gerador: ").Append(node.NodeType).Append('\n');
                break;
        }
    }

    /// <summary>Precedência do operador de um nó de dado — usada para só colocar
    /// parênteses quando a precedência realmente exige.</summary>
    private static int Precedence(FlowNode node) => node.NodeType switch
    {
        NodeTypes.MathCompare => 0,
        NodeTypes.MathAdd or NodeTypes.MathSubtract => 1,
        NodeTypes.MathMultiply or NodeTypes.MathDivide => 2,
        _ => 3, // literais, variáveis: precedência máxima, nunca precisam de parênteses
    };

    private static string EmitExpression(FlowGraph graph, FlowNode consumer, string pinName, int parentPrecedence)
    {
        var conn = graph.IncomingTo(consumer.Id, pinName);
        if (conn is null)
        {
            var literal = consumer.Literals[pinName];
            return literal.ToCSharpLiteral();
        }

        var source = graph.FindNode(conn.From.NodeId)!;
        string expr = source.NodeType switch
        {
            NodeTypes.GetVariable => source.Properties["VariableName"],
            NodeTypes.MathAdd => BinaryExpr(graph, source, "+"),
            NodeTypes.MathSubtract => BinaryExpr(graph, source, "-"),
            NodeTypes.MathMultiply => BinaryExpr(graph, source, "*"),
            NodeTypes.MathDivide => BinaryExpr(graph, source, "/"),
            NodeTypes.MathCompare => BinaryExpr(graph, source, source.Properties["Operator"]),
            NodeTypes.EventUpdate => "dt",
            NodeTypes.EventCollision => "outro",
            _ => $"/* nó de dado não suportado: {source.NodeType} */",
        };

        int myPrecedence = Precedence(source);
        bool needsParens = myPrecedence < parentPrecedence;
        return needsParens ? $"({expr})" : expr;
    }

    private static string BinaryExpr(FlowGraph graph, FlowNode node, string op)
    {
        int myPrecedence = Precedence(node);
        string a = EmitExpression(graph, node, "a", myPrecedence);
        string b = EmitExpression(graph, node, "b", myPrecedence + 1); // lado direito: exige precedência estrita para operadores não-associativos como -/÷
        return $"{a} {op} {b}";
    }

    public static string CSharpTypeName(FlowType type) => type switch
    {
        FlowType.Bool => "bool",
        FlowType.Int => "int",
        FlowType.Float => "float",
        FlowType.Float3 => "float3",
        FlowType.Quaternion => "quaternion",
        FlowType.String => "string",
        FlowType.Entity => "Entity",
        FlowType.Asset => "Asset",
        _ => "object",
    };

    private static string SanitizeIdentifier(string name)
    {
        var sb = new StringBuilder();
        foreach (var c in name)
            sb.Append(char.IsLetterOrDigit(c) ? c : '_');
        if (sb.Length == 0 || char.IsDigit(sb[0])) sb.Insert(0, '_');
        return sb.ToString();
    }
}
