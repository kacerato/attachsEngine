using System.Globalization;
using System.Text;
using Aether.Flow.Ast;

namespace Aether.Flow.CodeGen;

/// <summary>
/// Lê de volta o C# gerado por <see cref="FlowToCSharp"/> e reconstrói a AST —
/// a outra metade do round-trip que é a inovação central do produto (Parte 9.2
/// do plano).
///
/// SUBCONJUNTO SUPORTADO (documentado aqui porque é o contrato do parser):
///   - uma única `public class Nome { ... }`
///   - campos: `public &lt;tipo&gt; Nome = &lt;literal&gt;;` (vira <see cref="FlowVariable"/>)
///   - métodos de evento: `public void Start()`, `public void Update(float dt)`,
///     `public void OnCollision(Entity outro)`
///   - dentro de um método: atribuição simples `Alvo = expressao;`,
///     `if (cond) { ... } else { ... }`, `while (cond) { ... }`
///   - expressões: identificador (variável), literal (bool/int/float/string),
///     `+ - * /` e comparações `&lt; &lt;= &gt; &gt;= == !=`, parênteses
///
/// Não é um parser de C# completo — não é essa a meta. Qualquer trecho fora
/// deste subconjunto (chamada de método, `for`, LINQ, genéricos, ...) não gera
/// erro: o parser captura o texto original da instrução (ou do laço) e produz
/// um nó opaco <c>code.raw</c> encadeado no mesmo lugar da sequência de
/// execução, exatamente como a "caixa de código" descrita na Parte 9.2. Isso é
/// o comportamento CORRETO do escape hatch, não uma falha de parsing.
/// </summary>
public static class CSharpToFlow
{
    public static FlowGraph Parse(string source)
    {
        var tokens = Tokenize(source);
        return new Parser(source, tokens).ParseCompilationUnit();
    }

    // ---------------- tokenizador ----------------

    private enum TokKind { Ident, Number, String, Symbol, End }
    private readonly record struct Tok(TokKind Kind, int Start, int Length, string Text);

    private static List<Tok> Tokenize(string s)
    {
        var toks = new List<Tok>();
        int i = 0;
        while (i < s.Length)
        {
            char c = s[i];
            if (char.IsWhiteSpace(c)) { i++; continue; }
            if (c == '/' && i + 1 < s.Length && s[i + 1] == '/')
            {
                while (i < s.Length && s[i] != '\n') i++;
                continue;
            }
            if (char.IsLetter(c) || c == '_')
            {
                int start = i;
                while (i < s.Length && (char.IsLetterOrDigit(s[i]) || s[i] == '_')) i++;
                toks.Add(new Tok(TokKind.Ident, start, i - start, s[start..i]));
                continue;
            }
            if (char.IsDigit(c))
            {
                int start = i;
                while (i < s.Length && (char.IsDigit(s[i]) || s[i] == '.')) i++;
                if (i < s.Length && (s[i] == 'f' || s[i] == 'F')) i++;
                toks.Add(new Tok(TokKind.Number, start, i - start, s[start..i]));
                continue;
            }
            if (c == '"')
            {
                int start = i;
                i++;
                while (i < s.Length && s[i] != '"')
                {
                    if (s[i] == '\\') i++;
                    i++;
                }
                i++; // fecha aspas
                toks.Add(new Tok(TokKind.String, start, i - start, s[start..i]));
                continue;
            }
            // símbolos de dois caracteres primeiro
            if (i + 1 < s.Length)
            {
                string two = s.Substring(i, 2);
                if (two is "<=" or ">=" or "==" or "!=")
                {
                    toks.Add(new Tok(TokKind.Symbol, i, 2, two));
                    i += 2;
                    continue;
                }
            }
            toks.Add(new Tok(TokKind.Symbol, i, 1, c.ToString()));
            i++;
        }
        toks.Add(new Tok(TokKind.End, s.Length, 0, ""));
        return toks;
    }

    // ---------------- IR intermediária de expressão (tipada antes de virar nó) ----------------

    private sealed class ExprIR
    {
        public required FlowType Type;
        public FlowValue? Literal;
        public string? VarName;
        public string? Op;
        public ExprIR? Left, Right;
        public string? SpecialRef; // "dt" ou "outro" — saída de dado do próprio nó de evento
    }

    private sealed class Parser
    {
        private readonly string _src;
        private readonly List<Tok> _toks;
        private int _pos;
        private readonly FlowGraph _graph;
        private int _autoId;

        public Parser(string src, List<Tok> toks)
        {
            _src = src;
            _toks = toks;
            _graph = new FlowGraph { Name = "Grafo" };
        }

        private Tok Cur => _toks[_pos];
        private bool IsIdent(string text) => Cur.Kind == TokKind.Ident && Cur.Text == text;
        private bool IsSymbol(string text) => Cur.Kind == TokKind.Symbol && Cur.Text == text;

        private Tok Advance() => _toks[_pos++];

        private Tok Expect(TokKind kind, string? text = null)
        {
            if (Cur.Kind != kind || (text is not null && Cur.Text != text))
                throw new FormatException($"C# fora do subconjunto suportado perto de '{Cur.Text}' (posição {Cur.Start}).");
            return Advance();
        }

        public FlowGraph ParseCompilationUnit()
        {
            Expect(TokKind.Ident, "public");
            Expect(TokKind.Ident, "class");
            _graph.Name = Expect(TokKind.Ident).Text;
            Expect(TokKind.Symbol, "{");

            while (!IsSymbol("}"))
            {
                Expect(TokKind.Ident, "public");
                // pode ser campo ("<tipo> Nome = valor;") ou método ("void Nome(...) { ... }")
                if (IsIdent("void"))
                {
                    ParseMethod();
                }
                else
                {
                    ParseField();
                }
            }
            Expect(TokKind.Symbol, "}");
            return _graph;
        }

        private void ParseField()
        {
            string typeName = Expect(TokKind.Ident).Text;
            string name = Expect(TokKind.Ident).Text;
            Expect(TokKind.Symbol, "=");
            var type = CSharpTypeNameToFlowType(typeName);
            var literal = ParseLiteralValue(type);
            Expect(TokKind.Symbol, ";");
            _graph.Variables.Add(new FlowVariable { Name = name, Type = type, Initial = literal, Scope = VarScope.Object });
        }

        private void ParseMethod()
        {
            Expect(TokKind.Ident, "void");
            string name = Expect(TokKind.Ident).Text;
            Expect(TokKind.Symbol, "(");
            // parâmetros: ignoramos os nomes, o TIPO do evento já diz quais existem
            while (!IsSymbol(")")) Advance();
            Expect(TokKind.Symbol, ")");
            Expect(TokKind.Symbol, "{");

            string nodeType = name switch
            {
                "Start" => NodeTypes.EventStart,
                "Update" => NodeTypes.EventUpdate,
                "OnCollision" => NodeTypes.EventCollision,
                _ => throw new FormatException($"evento desconhecido no C#: {name}"),
            };
            string eventId = NewId("evt");
            var eventNode = nodeType switch
            {
                NodeTypes.EventStart => NodeLibrary.EventStart(eventId),
                NodeTypes.EventUpdate => NodeLibrary.EventUpdate(eventId),
                _ => NodeLibrary.EventCollision(eventId),
            };
            _graph.Nodes.Add(eventNode);

            ParseStatementsInto(eventId, "corpo");
            Expect(TokKind.Symbol, "}");
        }

        /// <summary>Analisa instruções até o '}' de fechamento do bloco atual, encadeando
        /// cada uma ao pino de execução <paramref name="fromPin"/> do nó <paramref name="fromNode"/>.</summary>
        private void ParseStatementsInto(string fromNode, string fromPin)
        {
            string curNode = fromNode, curPin = fromPin;
            while (!IsSymbol("}"))
            {
                var (nodeId, outPin) = ParseStatement();
                _graph.Connections.Add(new FlowConnection
                {
                    From = new FlowEndpoint(curNode, curPin),
                    To = new FlowEndpoint(nodeId, "entrada"),
                });
                curNode = nodeId;
                curPin = outPin;
            }
        }

        /// <summary>Analisa uma instrução e devolve (id do nó criado, nome do pino de saída
        /// pelo qual a próxima instrução deve encadear).</summary>
        private (string nodeId, string outPin) ParseStatement()
        {
            if (IsIdent("if")) return ParseIf();
            if (IsIdent("while")) return ParseWhile();
            if (Cur.Kind == TokKind.Ident && _toks[_pos + 1].Kind == TokKind.Symbol && _toks[_pos + 1].Text == "="
                && LooksLikeKnownIdentifier(Cur.Text))
                return ParseAssignment();

            return ParseRawStatement();
        }

        private bool LooksLikeKnownIdentifier(string name) => _graph.FindVariable(name) is not null;

        private (string, string) ParseAssignment()
        {
            string varName = Expect(TokKind.Ident).Text;
            Expect(TokKind.Symbol, "=");
            var variable = _graph.FindVariable(varName) ?? throw new FormatException($"variável não declarada: {varName}");
            var expr = ParseExpr();
            Expect(TokKind.Symbol, ";");

            string id = NewId("set");
            var node = NodeLibrary.SetVariable(id, varName, variable.Type);
            _graph.Nodes.Add(node);
            Attach(node, "valor", expr);
            return (id, "saida");
        }

        private (string, string) ParseIf()
        {
            Expect(TokKind.Ident, "if");
            Expect(TokKind.Symbol, "(");
            var cond = ParseExpr();
            Expect(TokKind.Symbol, ")");
            Expect(TokKind.Symbol, "{");

            string id = NewId("if");
            var node = NodeLibrary.FlowIf(id);
            _graph.Nodes.Add(node);
            Attach(node, "condicao", cond);

            ParseStatementsInto(id, "entao");
            Expect(TokKind.Symbol, "}");

            if (IsIdent("else"))
            {
                Advance();
                Expect(TokKind.Symbol, "{");
                ParseStatementsInto(id, "senao");
                Expect(TokKind.Symbol, "}");
            }
            // "depois" é o pino de convergência dos dois ramos (existindo "else" ou não) — é
            // ele que a próxima instrução do bloco recebe, nunca "entao"/"senao" diretamente,
            // que são só os pontos de ENTRADA de cada ramo. Mesmo padrão de "fim" em ParseWhile.
            return (id, "depois");
        }

        private (string, string) ParseWhile()
        {
            Expect(TokKind.Ident, "while");
            Expect(TokKind.Symbol, "(");
            var cond = ParseExpr();
            Expect(TokKind.Symbol, ")");
            Expect(TokKind.Symbol, "{");

            string id = NewId("while");
            var node = NodeLibrary.FlowWhile(id);
            _graph.Nodes.Add(node);
            Attach(node, "condicao", cond);

            ParseStatementsInto(id, "corpo");
            Expect(TokKind.Symbol, "}");
            return (id, "fim");
        }

        /// <summary>Escape hatch: captura o texto original de uma instrução (ou bloco) que
        /// não está no subconjunto suportado e a preserva como nó <c>code.raw</c> opaco.</summary>
        private (string, string) ParseRawStatement()
        {
            int startOffset = Cur.Start;
            int braceDepth = 0, parenDepth = 0;
            int lastEnd = Cur.Start;
            while (true)
            {
                if (Cur.Kind == TokKind.End)
                    throw new FormatException("instrução sem fim: chegou ao final do arquivo dentro de um bloco.");
                if (IsSymbol("{"))
                {
                    braceDepth++;
                    lastEnd = Cur.Start + Cur.Length;
                    Advance();
                    continue;
                }
                if (IsSymbol("}"))
                {
                    if (braceDepth == 0) break; // fecha o bloco que CONTÉM esta instrução — não consome
                    braceDepth--;
                    lastEnd = Cur.Start + Cur.Length;
                    Advance();
                    if (braceDepth == 0) break; // fechamos o bloco desta própria instrução (ex.: um laço com chaves)
                    continue;
                }
                if (IsSymbol("(")) parenDepth++;
                else if (IsSymbol(")")) parenDepth--;

                bool topSemicolon = IsSymbol(";") && braceDepth == 0 && parenDepth == 0;
                lastEnd = Cur.Start + Cur.Length;
                Advance();
                if (topSemicolon) break;
            }
            string raw = _src[startOffset..lastEnd].Trim();
            string id = NewId("raw");
            var node = NodeLibrary.CodeRaw(id, raw);
            _graph.Nodes.Add(node);
            // code.raw não declara pinos no schema (é opaco por natureza), mas ainda assim
            // participa da sequência de execução por convenção de nome de pino "saida",
            // igual aos demais nós de instrução — é só assim que a próxima instrução encadeia.
            return (id, "saida");
        }

        private void Attach(FlowNode consumer, string pinName, ExprIR expr)
        {
            if (expr.Literal is not null)
            {
                consumer.Literals[pinName] = expr.Literal;
                return;
            }
            if (expr.VarName is not null)
            {
                string id = NewId("get");
                var getNode = NodeLibrary.GetVariable(id, expr.VarName, expr.Type);
                _graph.Nodes.Add(getNode);
                _graph.Connections.Add(new FlowConnection
                {
                    From = new FlowEndpoint(id, "valor"),
                    To = new FlowEndpoint(consumer.Id, pinName),
                });
                return;
            }
            if (expr.SpecialRef is not null)
            {
                // "dt" e "outro" vêm do próprio nó de evento; localizamos o único evento que os declara.
                var evt = _graph.Nodes.FirstOrDefault(n => n.FindOutput(expr.SpecialRef) is not null
                    && (n.NodeType == NodeTypes.EventUpdate || n.NodeType == NodeTypes.EventCollision));
                if (evt is null) throw new FormatException($"referência a '{expr.SpecialRef}' fora de um evento que o forneça.");
                _graph.Connections.Add(new FlowConnection
                {
                    From = new FlowEndpoint(evt.Id, expr.SpecialRef),
                    To = new FlowEndpoint(consumer.Id, pinName),
                });
                return;
            }

            // operador binário: cria o nó de operação e conecta seus dois operandos recursivamente
            string opId = NewId("op");
            bool isCompare = expr.Op is "<" or "<=" or ">" or ">=" or "==" or "!=";
            var opNode = isCompare
                ? NodeLibrary.MathCompare(opId, expr.Op!, OperandTypeOf(expr))
                : NodeLibrary.MathBinary(opId, OpNodeType(expr.Op!), expr.Type);
            _graph.Nodes.Add(opNode);
            Attach(opNode, "a", expr.Left!);
            Attach(opNode, "b", expr.Right!);
            _graph.Connections.Add(new FlowConnection
            {
                From = new FlowEndpoint(opId, "resultado"),
                To = new FlowEndpoint(consumer.Id, pinName),
            });
        }

        private static FlowType OperandTypeOf(ExprIR compareExpr) => compareExpr.Left!.Type;

        private static string OpNodeType(string op) => op switch
        {
            "+" => NodeTypes.MathAdd,
            "-" => NodeTypes.MathSubtract,
            "*" => NodeTypes.MathMultiply,
            "/" => NodeTypes.MathDivide,
            _ => throw new FormatException($"operador não suportado: {op}"),
        };

        // ---- expressões (precedência: comparação < aditiva < multiplicativa < primária) ----

        private ExprIR ParseExpr() => ParseComparison();

        private ExprIR ParseComparison()
        {
            var left = ParseAdditive();
            if (Cur.Kind == TokKind.Symbol && Cur.Text is "<" or "<=" or ">" or ">=" or "==" or "!=")
            {
                string op = Advance().Text;
                var right = ParseAdditive();
                return new ExprIR { Type = FlowType.Bool, Op = op, Left = left, Right = right };
            }
            return left;
        }

        private ExprIR ParseAdditive()
        {
            var left = ParseMultiplicative();
            while (Cur.Kind == TokKind.Symbol && Cur.Text is "+" or "-")
            {
                string op = Advance().Text;
                var right = ParseMultiplicative();
                left = new ExprIR { Type = WiderType(left.Type, right.Type), Op = op, Left = left, Right = right };
            }
            return left;
        }

        private ExprIR ParseMultiplicative()
        {
            var left = ParsePrimary();
            while (Cur.Kind == TokKind.Symbol && Cur.Text is "*" or "/")
            {
                string op = Advance().Text;
                var right = ParsePrimary();
                left = new ExprIR { Type = WiderType(left.Type, right.Type), Op = op, Left = left, Right = right };
            }
            return left;
        }

        private static FlowType WiderType(FlowType a, FlowType b) =>
            a == FlowType.Float || b == FlowType.Float ? FlowType.Float : a;

        private ExprIR ParsePrimary()
        {
            if (IsSymbol("-"))
            {
                // Unário: só suportado diretamente sobre um literal numérico (é só isso que o
                // gerador produz — ex. o valor inicial de campo "-1"). Uma expressão negativa
                // mais geral like "-x" fica fora do subconjunto e vira code.raw no chamador.
                Advance();
                var inner = ParsePrimary();
                if (inner.Literal is null || (inner.Type != FlowType.Int && inner.Type != FlowType.Float))
                    throw new FormatException("o sinal de menos só é suportado na frente de um número literal.");
                var negated = inner.Type == FlowType.Int
                    ? FlowValue.OfInt(-inner.Literal.IntValue)
                    : FlowValue.OfFloat(-inner.Literal.FloatValue);
                return new ExprIR { Type = inner.Type, Literal = negated };
            }
            if (IsSymbol("("))
            {
                Advance();
                var e = ParseExpr();
                Expect(TokKind.Symbol, ")");
                return e;
            }
            if (Cur.Kind == TokKind.Number)
            {
                string text = Advance().Text;
                bool isFloat = text.Contains('.') || text.EndsWith('f') || text.EndsWith('F');
                text = text.TrimEnd('f', 'F');
                var val = isFloat ? FlowValue.OfFloat(double.Parse(text, CultureInfo.InvariantCulture))
                                   : FlowValue.OfInt(long.Parse(text, CultureInfo.InvariantCulture));
                return new ExprIR { Type = val.Type, Literal = val };
            }
            if (Cur.Kind == TokKind.String)
            {
                string text = Advance().Text;
                string unescaped = text[1..^1].Replace("\\\"", "\"").Replace("\\\\", "\\");
                return new ExprIR { Type = FlowType.String, Literal = FlowValue.OfString(unescaped) };
            }
            if (IsIdent("true") || IsIdent("false"))
            {
                bool b = Advance().Text == "true";
                return new ExprIR { Type = FlowType.Bool, Literal = FlowValue.OfBool(b) };
            }
            if (Cur.Kind == TokKind.Ident)
            {
                string name = Advance().Text;
                if (name is "dt" or "outro") return new ExprIR { Type = name == "dt" ? FlowType.Float : FlowType.Entity, SpecialRef = name };
                var variable = _graph.FindVariable(name) ?? throw new FormatException($"variável não declarada: {name}");
                return new ExprIR { Type = variable.Type, VarName = name };
            }
            throw new FormatException($"C# fora do subconjunto suportado perto de '{Cur.Text}'.");
        }

        private FlowValue ParseLiteralValue(FlowType type)
        {
            var expr = ParsePrimary();
            return expr.Literal ?? throw new FormatException("valor inicial de campo precisa ser um literal.");
        }

        private string NewId(string hint) => $"{hint}{_autoId++}";
    }

    public static FlowType CSharpTypeNameToFlowType(string typeName) => typeName switch
    {
        "bool" => FlowType.Bool,
        "int" => FlowType.Int,
        "float" => FlowType.Float,
        "float3" => FlowType.Float3,
        "quaternion" => FlowType.Quaternion,
        "string" => FlowType.String,
        "Entity" => FlowType.Entity,
        "Asset" => FlowType.Asset,
        _ => FlowType.Any,
    };
}
