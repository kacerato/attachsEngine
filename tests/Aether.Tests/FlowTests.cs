using Aether.Flow.Ast;
using Aether.Flow.CodeGen;
using Aether.Flow.Interpreter;
using Aether.Flow.Serialization;
using Aether.Flow.Validation;

namespace Aether.Tests;

public static class FlowTests
{
    // ---------------- construção de grafos de exemplo ----------------

    /// <summary>"Quando começar, definir vida = 100".</summary>
    private static FlowGraph GrafoDefinirVida(int valorInicial = 100)
    {
        var graph = new FlowGraph { Name = "Jogador" };
        graph.Variables.Add(new FlowVariable { Name = "vida", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var set = NodeLibrary.SetVariable("set", "vida", FlowType.Int);
        set.Literals["valor"] = FlowValue.OfInt(valorInicial);
        graph.Nodes.Add(start);
        graph.Nodes.Add(set);
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("set", "entrada") });
        return graph;
    }

    /// <summary>"Quando começar, se estaVivo então vida = 1 senão vida = 0".</summary>
    private static FlowGraph GrafoIfElse()
    {
        var graph = new FlowGraph { Name = "Estado" };
        graph.Variables.Add(new FlowVariable { Name = "estaVivo", Type = FlowType.Bool, Initial = FlowValue.OfBool(true), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "resultado", Type = FlowType.Int, Initial = FlowValue.OfInt(-1), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var iff = NodeLibrary.FlowIf("if");
        var getVivo = NodeLibrary.GetVariable("getVivo", "estaVivo", FlowType.Bool);
        var setEntao = NodeLibrary.SetVariable("setEntao", "resultado", FlowType.Int);
        setEntao.Literals["valor"] = FlowValue.OfInt(1);
        var setSenao = NodeLibrary.SetVariable("setSenao", "resultado", FlowType.Int);
        setSenao.Literals["valor"] = FlowValue.OfInt(0);

        graph.Nodes.Add(start);
        graph.Nodes.Add(iff);
        graph.Nodes.Add(getVivo);
        graph.Nodes.Add(setEntao);
        graph.Nodes.Add(setSenao);

        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("if", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("getVivo", "valor"), To = new("if", "condicao") });
        graph.Connections.Add(new FlowConnection { From = new("if", "entao"), To = new("setEntao", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("if", "senao"), To = new("setSenao", "entrada") });
        return graph;
    }

    /// <summary>"Quando começar, contador = 0; enquanto contador &lt; alvo: contador = contador + 1".</summary>
    private static FlowGraph GrafoWhileConta(int alvo)
    {
        var graph = new FlowGraph { Name = "Contador" };
        graph.Variables.Add(new FlowVariable { Name = "contador", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "alvo", Type = FlowType.Int, Initial = FlowValue.OfInt(alvo), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var whileNode = NodeLibrary.FlowWhile("while");
        var cmp = NodeLibrary.MathCompare("cmp", "<", FlowType.Int);
        var getContador = NodeLibrary.GetVariable("getContador", "contador", FlowType.Int);
        var getAlvo = NodeLibrary.GetVariable("getAlvo", "alvo", FlowType.Int);
        var add = NodeLibrary.MathBinary("add", NodeTypes.MathAdd, FlowType.Int);
        var setContador = NodeLibrary.SetVariable("setContador", "contador", FlowType.Int);

        graph.Nodes.AddRange(new[] { start, whileNode, cmp, getContador, getAlvo, add, setContador });

        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("while", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("cmp", "resultado"), To = new("while", "condicao") });
        graph.Connections.Add(new FlowConnection { From = new("getContador", "valor"), To = new("cmp", "a") });
        graph.Connections.Add(new FlowConnection { From = new("getAlvo", "valor"), To = new("cmp", "b") });
        graph.Connections.Add(new FlowConnection { From = new("while", "corpo"), To = new("setContador", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("getContador", "valor"), To = new("add", "a") });
        add.Literals["b"] = FlowValue.OfInt(1);
        graph.Connections.Add(new FlowConnection { From = new("add", "resultado"), To = new("setContador", "valor") });
        return graph;
    }

    // ---------------- interpretador ----------------

    [Test] public static void Interpretador_DefinirVariavelNoInicio()
    {
        var graph = GrafoDefinirVida(100);
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(100L, interp.Variables["vida"].IntValue, "vida deveria ter sido definida para 100");
    }

    [Test] public static void Interpretador_SeExecutaRamoVerdadeiro()
    {
        var graph = GrafoIfElse();
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(1L, interp.Variables["resultado"].IntValue, "condição verdadeira deveria seguir o ramo 'então'");
    }

    [Test] public static void Interpretador_SeExecutaRamoFalso()
    {
        var graph = GrafoIfElse();
        var estaVivo = graph.Variables.First(v => v.Name == "estaVivo");
        graph.Variables.Remove(estaVivo);
        graph.Variables.Add(new FlowVariable { Name = "estaVivo", Type = FlowType.Bool, Initial = FlowValue.OfBool(false), Scope = VarScope.Object });

        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, interp.Variables["resultado"].IntValue, "condição falsa deveria seguir o ramo 'senão'");
    }

    [Test] public static void Interpretador_EnquantoRepeteONumeroCertoDeVezes()
    {
        var graph = GrafoWhileConta(5);
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(5L, interp.Variables["contador"].IntValue, "o laço deveria parar exatamente quando contador == alvo");
    }

    [Test] public static void Interpretador_EnquantoComAlvoZeroNaoExecutaOCorpo()
    {
        var graph = GrafoWhileConta(0);
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, interp.Variables["contador"].IntValue, "condição já falsa de início: o corpo nunca roda");
    }

    [Test] public static void Interpretador_LacoInfinitoEhInterrompidoComMensagemClara()
    {
        var graph = new FlowGraph { Name = "Trava" };
        graph.Variables.Add(new FlowVariable { Name = "x", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        var start = NodeLibrary.EventStart("evt");
        var whileNode = NodeLibrary.FlowWhile("while");
        whileNode.Literals["condicao"] = FlowValue.OfBool(true); // nunca fica falsa: laço infinito de propósito
        graph.Nodes.Add(start);
        graph.Nodes.Add(whileNode);
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("while", "entrada") });

        var interp = new FlowInterpreter(graph, maxSteps: 1000);
        FlowExecutionLimitException? caught = null;
        try { interp.RunEvent(NodeTypes.EventStart); }
        catch (FlowExecutionLimitException ex) { caught = ex; }
        Assert.True(caught is not null, "laço sem condição de parada deve estourar o limite de passos, não travar o processo");
        Assert.Equal("while", caught!.NodeId, "a mensagem deve apontar o bloco culpado pelo laço");

        // o interpretador continua utilizável depois de um estouro de limite
        var graph2 = GrafoDefinirVida(42);
        var interp2 = new FlowInterpreter(graph2);
        interp2.RunEvent(NodeTypes.EventStart);
        Assert.Equal(42L, interp2.Variables["vida"].IntValue, "uma nova execução depois de um estouro continua funcionando normalmente");
    }

    [Test] public static void Interpretador_GrafoVazioNaoLancaExcecao()
    {
        var graph = new FlowGraph { Name = "Vazio" };
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart); // não deve lançar nada
        Assert.True(true, "rodar um grafo vazio não deveria lançar exceção nenhuma");
    }

    [Test] public static void Interpretador_EventoSemCorpoNaoLancaExcecao()
    {
        var graph = new FlowGraph { Name = "SoEvento" };
        graph.Nodes.Add(NodeLibrary.EventStart("evt"));
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.True(true, "um evento sem nada conectado ao corpo não deveria lançar exceção");
    }

    [Test] public static void Interpretador_NoSemNenhumaConexaoNaoAfetaExecucao()
    {
        var graph = GrafoDefinirVida(7);
        graph.Nodes.Add(NodeLibrary.GetVariable("solto", "vida", FlowType.Int)); // órfão, ninguém usa
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(7L, interp.Variables["vida"].IntValue, "um nó desconectado não deve afetar o restante do grafo");
    }

    // ---------------- validador ----------------

    [Test] public static void Validador_PinoObrigatorioVazioGeraErro()
    {
        var graph = new FlowGraph { Name = "Incompleto" };
        graph.Variables.Add(new FlowVariable { Name = "vida", Type = FlowType.Int, Initial = FlowValue.OfInt(0) });
        var start = NodeLibrary.EventStart("evt");
        var set = NodeLibrary.SetVariable("set", "vida", FlowType.Int); // "valor" não tem literal nem conexão
        graph.Nodes.Add(start);
        graph.Nodes.Add(set);
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("set", "entrada") });

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Erro && d.NodeId == "set"),
            "entrada de dado desconectada e sem literal deveria ser erro");
    }

    [Test] public static void Validador_TipoIncompativelNaConexaoGeraErro()
    {
        var graph = new FlowGraph { Name = "TiposErrados" };
        graph.Variables.Add(new FlowVariable { Name = "nome", Type = FlowType.String, Initial = FlowValue.OfString("") });
        var getNumero = NodeLibrary.GetVariable("getNum", "naoImporta", FlowType.Int); // saída Int
        var setTexto = NodeLibrary.SetVariable("setTexto", "nome", FlowType.String);   // entrada String
        graph.Nodes.Add(getNumero);
        graph.Nodes.Add(setTexto);
        graph.Connections.Add(new FlowConnection { From = new("getNum", "valor"), To = new("setTexto", "valor") });

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Erro && d.Message.Contains("não combina")),
            "ligar uma saída de número numa entrada de texto deveria ser erro de tipo");
    }

    [Test] public static void Validador_CicloDeDadosGeraErro()
    {
        var graph = new FlowGraph { Name = "CicloDeDados" };
        var a = NodeLibrary.MathBinary("a", NodeTypes.MathAdd, FlowType.Int);
        var b = NodeLibrary.MathBinary("b", NodeTypes.MathAdd, FlowType.Int);
        a.Literals["b"] = FlowValue.OfInt(1);
        b.Literals["b"] = FlowValue.OfInt(1);
        graph.Nodes.Add(a);
        graph.Nodes.Add(b);
        // a.a <- b.resultado ; b.a <- a.resultado : dependência circular de dados
        graph.Connections.Add(new FlowConnection { From = new("b", "resultado"), To = new("a", "a") });
        graph.Connections.Add(new FlowConnection { From = new("a", "resultado"), To = new("b", "a") });

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Erro && d.Message.Contains("mordendo")),
            "um ciclo em fluxo de dados deveria sempre ser erro");
    }

    [Test] public static void Validador_CicloDeExecucaoAtravesDeLacoEhLegitimo()
    {
        // Um "while" cujo corpo aponta de volta para ele mesmo é, estruturalmente, um ciclo de
        // execução — mas é exatamente como um laço deveria funcionar, então não pode virar erro.
        var graph = new FlowGraph { Name = "LacoLegitimo" };
        var whileNode = NodeLibrary.FlowWhile("while");
        whileNode.Literals["condicao"] = FlowValue.OfBool(false);
        var passo = NodeLibrary.CodeRaw("passo", "// corpo do laço", ExecPins());
        graph.Nodes.Add(whileNode);
        graph.Nodes.Add(passo);
        graph.Connections.Add(new FlowConnection { From = new("while", "corpo"), To = new("passo", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("passo", "saida"), To = new("while", "entrada") });

        var diags = FlowValidator.Validate(graph);
        Assert.False(diags.Any(d => d.Severity == Severity.Erro && d.Message.Contains("círculo sem fim")),
            "um ciclo de execução que passa por um bloco 'Enquanto' é legítimo, não deveria gerar erro");
    }

    [Test] public static void Validador_CicloDeExecucaoSemLacoGeraErro()
    {
        var graph = new FlowGraph { Name = "CicloIlegitimo" };
        var a = NodeLibrary.CodeRaw("a", "// a", ExecPins());
        var b = NodeLibrary.CodeRaw("b", "// b", ExecPins());
        graph.Nodes.Add(a);
        graph.Nodes.Add(b);
        graph.Connections.Add(new FlowConnection { From = new("a", "saida"), To = new("b", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("b", "saida"), To = new("a", "entrada") });

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Erro && d.Message.Contains("círculo sem fim")),
            "um ciclo de execução sem nenhum bloco de laço deveria ser erro");
    }

    [Test] public static void Validador_VariavelInexistenteGeraErro()
    {
        var graph = new FlowGraph { Name = "SemVariavel" };
        var get = NodeLibrary.GetVariable("get", "naoExiste", FlowType.Int);
        graph.Nodes.Add(get);

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Erro && d.NodeId == "get"),
            "referenciar uma variável que não existe deveria ser erro");
    }

    [Test] public static void Validador_NoOrfaoGeraAviso()
    {
        var graph = GrafoDefinirVida(1);
        graph.Nodes.Add(NodeLibrary.SetVariable("solto", "vida", FlowType.Int)); // ninguém chama esse
        graph.FindNode("solto")!.Literals["valor"] = FlowValue.OfInt(9);

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Aviso && d.NodeId == "solto"),
            "um bloco não alcançável por nenhum evento deveria ser um aviso, não um erro");
    }

    [Test] public static void Validador_EventosDuplicadosGeraAviso()
    {
        var graph = new FlowGraph { Name = "DoisInicios" };
        graph.Nodes.Add(NodeLibrary.EventStart("evt0"));
        graph.Nodes.Add(NodeLibrary.EventStart("evt1"));

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Any(d => d.Severity == Severity.Aviso && d.NodeId == "evt1"),
            "dois eventos 'Quando começar' no mesmo grafo deveria ser um aviso");
    }

    [Test] public static void Validador_MensagensSaoEmPortuguesSemJargao()
    {
        var graph = new FlowGraph { Name = "Incompleto" };
        var set = NodeLibrary.SetVariable("set", "vida", FlowType.Int);
        graph.Nodes.Add(set);

        var diags = FlowValidator.Validate(graph);
        Assert.True(diags.Count > 0, "deveria ter gerado ao menos um diagnóstico para este grafo incompleto");
        foreach (var d in diags)
        {
            string msg = d.Message.ToLowerInvariant();
            Assert.False(msg.Contains("pin"), "mensagem não deveria usar o termo técnico 'pin'");
            Assert.False(msg.Contains("null"), "mensagem não deveria usar o termo técnico 'null'");
            Assert.False(msg.Contains("exception"), "mensagem não deveria usar o termo técnico 'exception'");
        }
    }

    // ---------------- gerador de C# ----------------

    [Test] public static void CodeGen_GrafoConhecidoProduzCSharpEsperado()
    {
        var graph = GrafoDefinirVida(100);
        string code = FlowToCSharp.Generate(graph);

        string esperado =
            "public class Jogador\n" +
            "{\n" +
            "    public int vida = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        vida = 100;\n" +
            "    }\n" +
            "}\n";

        Assert.Equal(NormalizarFimDeLinha(esperado), NormalizarFimDeLinha(code));
    }

    [Test] public static void CodeGen_PrecedenciaSoUsaParentesesQuandoNecessario()
    {
        // resultado = (contador + 1) * alvo  -- os parênteses em volta da soma SÃO necessários
        var graph = new FlowGraph { Name = "Precedencia" };
        graph.Variables.Add(new FlowVariable { Name = "contador", Type = FlowType.Int, Initial = FlowValue.OfInt(0) });
        graph.Variables.Add(new FlowVariable { Name = "alvo", Type = FlowType.Int, Initial = FlowValue.OfInt(0) });
        graph.Variables.Add(new FlowVariable { Name = "resultado", Type = FlowType.Int, Initial = FlowValue.OfInt(0) });

        var start = NodeLibrary.EventStart("evt");
        var set = NodeLibrary.SetVariable("set", "resultado", FlowType.Int);
        var add = NodeLibrary.MathBinary("add", NodeTypes.MathAdd, FlowType.Int);
        var getContador = NodeLibrary.GetVariable("getContador", "contador", FlowType.Int);
        var getAlvo = NodeLibrary.GetVariable("getAlvo", "alvo", FlowType.Int);
        var mul = NodeLibrary.MathBinary("mul", NodeTypes.MathMultiply, FlowType.Int);
        add.Literals["b"] = FlowValue.OfInt(1);

        graph.Nodes.AddRange(new[] { start, set, add, getContador, getAlvo, mul });
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("set", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("mul", "resultado"), To = new("set", "valor") });
        graph.Connections.Add(new FlowConnection { From = new("add", "resultado"), To = new("mul", "a") });
        graph.Connections.Add(new FlowConnection { From = new("getContador", "valor"), To = new("add", "a") });
        graph.Connections.Add(new FlowConnection { From = new("getAlvo", "valor"), To = new("mul", "b") });

        string code = FlowToCSharp.Generate(graph);
        Assert.True(code.Contains("resultado = (contador + 1) * alvo;"),
            $"esperava parênteses em volta da soma por causa da precedência de '*'; obtido:\n{code}");
    }

    // ---------------- round-trip C# ----------------

    [Test] public static void RoundTrip_AstParaCSharpParaAstEhEquivalente()
    {
        var original = GrafoDefinirVida(100);
        string code = FlowToCSharp.Generate(original);
        var reconstruido = CSharpToFlow.Parse(code);

        Assert.Equal(original.Name, reconstruido.Name);
        Assert.Equal(original.Variables.Count, reconstruido.Variables.Count);
        Assert.Equal(original.Variables[0].Name, reconstruido.Variables[0].Name);
        Assert.Equal(original.Variables[0].Type, reconstruido.Variables[0].Type);

        // A posição no canvas é perdida no round-trip por C# — é uma limitação documentada:
        // o C# gerado não guarda coordenadas de canvas, e reconstruir a partir dele não pode
        // inventar uma posição que nunca existiu no texto.
        var interp = new FlowInterpreter(reconstruido);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(100L, interp.Variables["vida"].IntValue,
            "o grafo reconstruído deveria se comportar de forma idêntica ao original");
    }

    [Test] public static void RoundTrip_IfEleGeraOMesmoComportamentoDepoisDeReconstruido()
    {
        var original = GrafoIfElse();
        string code = FlowToCSharp.Generate(original);
        var reconstruido = CSharpToFlow.Parse(code);

        var interp = new FlowInterpreter(reconstruido);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(1L, interp.Variables["resultado"].IntValue, "if/else reconstruído deveria se comportar igual ao original");
    }

    [Test] public static void RoundTrip_CSharpForaDoSubconjuntoViraCodeRaw()
    {
        string code =
            "public class Estranho\n" +
            "{\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        Console.WriteLine(\"oi\");\n" +
            "    }\n" +
            "}\n";

        var graph = CSharpToFlow.Parse(code);
        var rawNode = graph.Nodes.FirstOrDefault(n => n.NodeType == NodeTypes.CodeRaw);
        Assert.True(rawNode is not null, "uma chamada de método fora do subconjunto deveria virar um nó code.raw");
        Assert.True(rawNode!.Properties["RawCode"].Contains("Console.WriteLine(\"oi\")"),
            "o nó code.raw deveria preservar o texto original da instrução não suportada");
    }

    [Test] public static void RoundTrip_SerializacaoIdaEVoltaPreservaOGrafo()
    {
        var original = GrafoWhileConta(3);
        string texto = FlowSerializer.Serialize(original);
        var reconstruido = FlowSerializer.Deserialize(texto);

        Assert.Equal(original.Nodes.Count, reconstruido.Nodes.Count, "mesmo número de nós depois do round-trip");
        Assert.Equal(original.Connections.Count, reconstruido.Connections.Count, "mesmo número de conexões depois do round-trip");
        Assert.Equal(original.Variables.Count, reconstruido.Variables.Count, "mesmo número de variáveis depois do round-trip");

        var interp = new FlowInterpreter(reconstruido);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(3L, interp.Variables["contador"].IntValue, "o grafo reconstruído do texto deveria se comportar igual ao original");
    }

    [Test] public static void RoundTrip_SerializacaoEhDeterministica()
    {
        var graph = GrafoIfElse();
        string texto1 = FlowSerializer.Serialize(graph);
        string texto2 = FlowSerializer.Serialize(graph);
        Assert.Equal(texto1, texto2, "serializar o mesmo grafo duas vezes deveria produzir bytes idênticos (diff útil em VCS)");

        var reconstruido = FlowSerializer.Deserialize(texto1);
        string texto3 = FlowSerializer.Serialize(reconstruido);
        Assert.Equal(texto1, texto3, "serializar depois de um round-trip completo ainda produz os mesmos bytes");
    }

    [Test] public static void RoundTrip_GrafoVazioSerializaEDeserializaSemErro()
    {
        var graph = new FlowGraph { Name = "Vazio" };
        string texto = FlowSerializer.Serialize(graph);
        var reconstruido = FlowSerializer.Deserialize(texto);
        Assert.Equal("Vazio", reconstruido.Name);
        Assert.Equal(0, reconstruido.Nodes.Count);
    }

    private static List<FlowPin> ExecPins() => new()
    {
        new FlowPin { Name = "entrada", Type = FlowType.Exec, Direction = PinDirection.Input },
        new FlowPin { Name = "saida", Type = FlowType.Exec, Direction = PinDirection.Output },
    };

    private static string NormalizarFimDeLinha(string s) =>
        string.Join('\n', s.Replace("\r\n", "\n").Split('\n').Select(l => l.TrimEnd()));
}
