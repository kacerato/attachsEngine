using System.Diagnostics;
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

    /// <summary>"Quando começar, se (condicao) então a = 1; depois b = b + 1" — sem "senão".
    /// Prova que o pino "depois" recebe a continuação mesmo quando não há ramo "senão".</summary>
    private static FlowGraph GrafoIfSemElseSeguidoDeInstrucao(bool condicaoInicial)
    {
        var graph = new FlowGraph { Name = "SemElse" };
        graph.Variables.Add(new FlowVariable { Name = "cond", Type = FlowType.Bool, Initial = FlowValue.OfBool(condicaoInicial), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "a", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "marcador", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var iff = NodeLibrary.FlowIf("if");
        var getCond = NodeLibrary.GetVariable("getCond", "cond", FlowType.Bool);
        var setA = NodeLibrary.SetVariable("setA", "a", FlowType.Int);
        setA.Literals["valor"] = FlowValue.OfInt(1);
        var setMarcador = NodeLibrary.SetVariable("setMarcador", "marcador", FlowType.Int);
        setMarcador.Literals["valor"] = FlowValue.OfInt(9);

        graph.Nodes.AddRange(new[] { start, iff, getCond, setA, setMarcador });
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("if", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("getCond", "valor"), To = new("if", "condicao") });
        graph.Connections.Add(new FlowConnection { From = new("if", "entao"), To = new("setA", "entrada") });
        // sem ramo "senao" — o pino "senao" nunca aparece numa conexão de saída.
        graph.Connections.Add(new FlowConnection { From = new("if", "depois"), To = new("setMarcador", "entrada") });
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

    [Test] public static void Interpretador_SeSemSenaoAindaAssimEncadeiaDepois()
    {
        var graphVerdadeiro = GrafoIfSemElseSeguidoDeInstrucao(condicaoInicial: true);
        var interpV = new FlowInterpreter(graphVerdadeiro);
        interpV.RunEvent(NodeTypes.EventStart);
        Assert.Equal(1L, interpV.Variables["a"].IntValue, "ramo 'então' deveria ter rodado");
        Assert.Equal(9L, interpV.Variables["marcador"].IntValue, "instrução depois do 'se' sem 'senão' deveria rodar mesmo assim");

        var graphFalso = GrafoIfSemElseSeguidoDeInstrucao(condicaoInicial: false);
        var interpF = new FlowInterpreter(graphFalso);
        interpF.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, interpF.Variables["a"].IntValue, "ramo 'então' não deveria ter rodado com condição falsa");
        Assert.Equal(9L, interpF.Variables["marcador"].IntValue, "instrução depois do 'se' sem 'senão' deveria rodar mesmo quando o 'se' não entrou em nenhum ramo 'então'");
    }

    [Test] public static void Interpretador_SeComSenaoOsDoisRamosConvergemParaDepois()
    {
        // Reaproveita GrafoIfElse (se estaVivo então resultado=1 senão resultado=0) e encadeia
        // mais uma instrução no pino "depois", que precisa rodar independente do ramo escolhido.
        void RodarComCondicao(bool valor, long resultadoEsperado)
        {
            var g = GrafoIfElse();
            g.Variables.Remove(g.Variables.First(v => v.Name == "estaVivo"));
            g.Variables.Add(new FlowVariable { Name = "estaVivo", Type = FlowType.Bool, Initial = FlowValue.OfBool(valor), Scope = VarScope.Object });
            g.Variables.Add(new FlowVariable { Name = "marcador", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
            var set = NodeLibrary.SetVariable("setMarcador", "marcador", FlowType.Int);
            set.Literals["valor"] = FlowValue.OfInt(5);
            g.Nodes.Add(set);
            g.Connections.Add(new FlowConnection { From = new("if", "depois"), To = new("setMarcador", "entrada") });

            var interp = new FlowInterpreter(g);
            interp.RunEvent(NodeTypes.EventStart);
            Assert.Equal(resultadoEsperado, interp.Variables["resultado"].IntValue, "ramo escolhido deveria ter rodado normalmente");
            Assert.Equal(5L, interp.Variables["marcador"].IntValue, "instrução depois do 'se/senão' deveria rodar não importa qual ramo foi tomado");
        }

        RodarComCondicao(true, 1L);
        RodarComCondicao(false, 0L);
    }

    [Test] public static void Interpretador_SeDentroDeEnquantoContinuaOCorpoDoLaco()
    {
        // "enquanto contador < alvo: se verdadeiro então vezes+=1; depois contador+=1" — o "se"
        // fica NO MEIO do corpo do laço, não no fim; prova que "depois" encadeia mesmo aninhado
        // dentro de um "Enquanto" (e não só no nível do evento).
        int alvo = 5;
        var graph = new FlowGraph { Name = "SeDentroDeLaco" };
        graph.Variables.Add(new FlowVariable { Name = "contador", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "alvo", Type = FlowType.Int, Initial = FlowValue.OfInt(alvo), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "vezes", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var whileNode = NodeLibrary.FlowWhile("while");
        var cmp = NodeLibrary.MathCompare("cmp", "<", FlowType.Int);
        var getContador = NodeLibrary.GetVariable("getContador", "contador", FlowType.Int);
        var getAlvo = NodeLibrary.GetVariable("getAlvo", "alvo", FlowType.Int);

        var iff = NodeLibrary.FlowIf("if");
        // condição sempre verdadeira: o que importa aqui é que TANTO o ramo "então" quanto o
        // que vem "depois" dele rodem em toda iteração.
        iff.Literals["condicao"] = FlowValue.OfBool(true);

        var addVezes = NodeLibrary.MathBinary("addVezes", NodeTypes.MathAdd, FlowType.Int);
        var getVezes = NodeLibrary.GetVariable("getVezes", "vezes", FlowType.Int);
        addVezes.Literals["b"] = FlowValue.OfInt(1);
        var setVezes = NodeLibrary.SetVariable("setVezes", "vezes", FlowType.Int);

        var addContador = NodeLibrary.MathBinary("addContador", NodeTypes.MathAdd, FlowType.Int);
        addContador.Literals["b"] = FlowValue.OfInt(1);
        var setContador = NodeLibrary.SetVariable("setContador", "contador", FlowType.Int);

        graph.Nodes.AddRange(new[] { start, whileNode, cmp, getContador, getAlvo, iff, addVezes, getVezes, setVezes, addContador, setContador });

        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("while", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("cmp", "resultado"), To = new("while", "condicao") });
        graph.Connections.Add(new FlowConnection { From = new("getContador", "valor"), To = new("cmp", "a") });
        graph.Connections.Add(new FlowConnection { From = new("getAlvo", "valor"), To = new("cmp", "b") });

        graph.Connections.Add(new FlowConnection { From = new("while", "corpo"), To = new("if", "entrada") });

        graph.Connections.Add(new FlowConnection { From = new("if", "entao"), To = new("setVezes", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("getVezes", "valor"), To = new("addVezes", "a") });
        graph.Connections.Add(new FlowConnection { From = new("addVezes", "resultado"), To = new("setVezes", "valor") });

        // depois do "se" (independente do ramo), continua o corpo do laço incrementando o contador.
        graph.Connections.Add(new FlowConnection { From = new("if", "depois"), To = new("setContador", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("getContador", "valor"), To = new("addContador", "a") });
        graph.Connections.Add(new FlowConnection { From = new("addContador", "resultado"), To = new("setContador", "valor") });

        var interp = new FlowInterpreter(graph, maxSteps: 1000);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal((long)alvo, interp.Variables["contador"].IntValue, "o incremento depois do 'se' precisa continuar o corpo do laço em toda iteração, senão o laço nunca converge");
        Assert.Equal((long)alvo, interp.Variables["vezes"].IntValue, "o ramo 'então' do 'se' também deveria ter rodado em toda iteração");
    }

    [Test] public static void Interpretador_SeAninhadoDentroDoRamoEntaoDeOutroSeComInstrucoesDepoisDosDois()
    {
        // se (verdadeiro) então { se (verdadeiro) então { a = 1 } depois { b = 1 } } depois { c = 1 }
        // Prova que o pino "depois" funciona em qualquer profundidade de aninhamento de "se".
        var graph = new FlowGraph { Name = "SeAninhado" };
        graph.Variables.Add(new FlowVariable { Name = "a", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "b", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });
        graph.Variables.Add(new FlowVariable { Name = "c", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object });

        var start = NodeLibrary.EventStart("evt");
        var outerIf = NodeLibrary.FlowIf("outer");
        outerIf.Literals["condicao"] = FlowValue.OfBool(true);
        var innerIf = NodeLibrary.FlowIf("inner");
        innerIf.Literals["condicao"] = FlowValue.OfBool(true);

        var setA = NodeLibrary.SetVariable("setA", "a", FlowType.Int);
        setA.Literals["valor"] = FlowValue.OfInt(1);
        var setB = NodeLibrary.SetVariable("setB", "b", FlowType.Int);
        setB.Literals["valor"] = FlowValue.OfInt(1);
        var setC = NodeLibrary.SetVariable("setC", "c", FlowType.Int);
        setC.Literals["valor"] = FlowValue.OfInt(1);

        graph.Nodes.AddRange(new[] { start, outerIf, innerIf, setA, setB, setC });
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("outer", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("outer", "entao"), To = new("inner", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("inner", "entao"), To = new("setA", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("inner", "depois"), To = new("setB", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("outer", "depois"), To = new("setC", "entrada") });

        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(1L, interp.Variables["a"].IntValue, "ramo 'então' do 'se' interno deveria ter rodado");
        Assert.Equal(1L, interp.Variables["b"].IntValue, "instrução depois do 'se' interno deveria ter rodado");
        Assert.Equal(1L, interp.Variables["c"].IntValue, "instrução depois do 'se' externo deveria ter rodado");
    }

    [Test] public static void Interpretador_ReturnDentroDeIfImpedeAContinuacaoDoEvento()
    {
        const string code =
            "public class RetornoAntecipado\n" +
            "{\n" +
            "    public bool sair = true;\n" +
            "    public int marcador = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        if (sair)\n" +
            "        {\n" +
            "            return;\n" +
            "        }\n" +
            "        marcador = 9;\n" +
            "    }\n" +
            "}\n";

        var graphTrue = CSharpToFlow.Parse(code);
        Assert.True(graphTrue.Nodes.Any(n => n.NodeType == NodeTypes.FlowReturn),
            "return precisa virar nó explícito, nunca code.raw");
        Assert.False(graphTrue.Nodes.Any(n => n.NodeType == NodeTypes.CodeRaw),
            "todo o exemplo pertence ao subconjunto suportado");
        var interpTrue = new FlowInterpreter(graphTrue);
        interpTrue.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, interpTrue.Variables["marcador"].IntValue,
            "return no ramo verdadeiro precisa impedir a instrução depois do if");

        var graphFalse = CSharpToFlow.Parse(code);
        DefinirValorInicial(graphFalse, "sair", FlowValue.OfBool(false));
        var interpFalse = new FlowInterpreter(graphFalse);
        interpFalse.RunEvent(NodeTypes.EventStart);
        Assert.Equal(9L, interpFalse.Variables["marcador"].IntValue,
            "quando o ramo com return não é tomado, a continuação deve executar");

        string regenerated = FlowToCSharp.Generate(graphTrue);
        Assert.Equal(NormalizarFimDeLinha(code), NormalizarFimDeLinha(regenerated),
            "parse/generate de return antecipado deve preservar a forma canônica");
        var roundTripped = CSharpToFlow.Parse(regenerated);
        var roundTripInterpreter = new FlowInterpreter(roundTripped);
        roundTripInterpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, roundTripInterpreter.Variables["marcador"].IntValue,
            "o segundo parse precisa preservar a semântica do retorno");
    }

    [Test] public static void Interpretador_BreakDentroDeIfEncerraSomenteOLaco()
    {
        const string code =
            "public class PararLaco\n" +
            "{\n" +
            "    public int contador = 0;\n" +
            "    public int soma = 0;\n" +
            "    public int marcador = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        while (contador < 10)\n" +
            "        {\n" +
            "            contador = contador + 1;\n" +
            "            if (contador == 3)\n" +
            "            {\n" +
            "                break;\n" +
            "            }\n" +
            "            soma = soma + 1;\n" +
            "        }\n" +
            "        marcador = 9;\n" +
            "    }\n" +
            "}\n";

        var graph = CSharpToFlow.Parse(code);
        var diagnostics = FlowValidator.Validate(graph);
        Assert.False(diagnostics.Any(d => d.Severity == Severity.Erro),
            "break dentro do corpo de while precisa ser válido");

        var interpreter = new FlowInterpreter(graph);
        interpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(3L, interpreter.Variables["contador"].IntValue);
        Assert.Equal(2L, interpreter.Variables["soma"].IntValue,
            "a iteração que executa break não pode rodar o restante do corpo");
        Assert.Equal(9L, interpreter.Variables["marcador"].IntValue,
            "break encerra o laço, não o evento");

        string regenerated = FlowToCSharp.Generate(graph);
        Assert.Equal(NormalizarFimDeLinha(code), NormalizarFimDeLinha(regenerated));
    }

    [Test] public static void Interpretador_ContinueDentroDeIfPulaORestanteDaIteracao()
    {
        const string code =
            "public class ContinuarLaco\n" +
            "{\n" +
            "    public int contador = 0;\n" +
            "    public int soma = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        while (contador < 5)\n" +
            "        {\n" +
            "            contador = contador + 1;\n" +
            "            if (contador == 3)\n" +
            "            {\n" +
            "                continue;\n" +
            "            }\n" +
            "            soma = soma + 1;\n" +
            "        }\n" +
            "    }\n" +
            "}\n";

        var graph = CSharpToFlow.Parse(code);
        var interpreter = new FlowInterpreter(graph);
        interpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(5L, interpreter.Variables["contador"].IntValue);
        Assert.Equal(4L, interpreter.Variables["soma"].IntValue,
            "somente a iteração contador==3 deve pular a soma");

        string regenerated = FlowToCSharp.Generate(graph);
        var reconstructed = CSharpToFlow.Parse(regenerated);
        var reconstructedInterpreter = new FlowInterpreter(reconstructed);
        reconstructedInterpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(4L, reconstructedInterpreter.Variables["soma"].IntValue,
            "continue precisa sobreviver ao round-trip C#");
    }

    [Test] public static void Interpretador_BreakAninhadoEncerraApenasOLacoMaisInterno()
    {
        const string code =
            "public class LacosAninhados\n" +
            "{\n" +
            "    public int externo = 0;\n" +
            "    public int interno = 0;\n" +
            "    public int total = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        while (externo < 2)\n" +
            "        {\n" +
            "            externo = externo + 1;\n" +
            "            interno = 0;\n" +
            "            while (interno < 5)\n" +
            "            {\n" +
            "                interno = interno + 1;\n" +
            "                if (interno == 2)\n" +
            "                {\n" +
            "                    break;\n" +
            "                }\n" +
            "                total = total + 1;\n" +
            "            }\n" +
            "            total = total + 10;\n" +
            "        }\n" +
            "    }\n" +
            "}\n";

        var graph = CSharpToFlow.Parse(code);
        var interpreter = new FlowInterpreter(graph);
        interpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(2L, interpreter.Variables["externo"].IntValue,
            "break interno não pode encerrar o laço externo");
        Assert.Equal(2L, interpreter.Variables["interno"].IntValue);
        Assert.Equal(22L, interpreter.Variables["total"].IntValue);
    }

    [Test] public static void Validador_BreakEContinueForaDeLacoGeramErro()
    {
        var graph = new FlowGraph { Name = "ControleInvalido" };
        var start = NodeLibrary.EventStart("evt");
        var iff = NodeLibrary.FlowIf("if");
        iff.Literals["condicao"] = FlowValue.OfBool(true);
        var stop = NodeLibrary.FlowBreak("break");
        var next = NodeLibrary.FlowContinue("continue");
        graph.Nodes.AddRange(new[] { start, iff, stop, next });
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("if", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("if", "entao"), To = new("break", "entrada") });
        graph.Connections.Add(new FlowConnection { From = new("if", "senao"), To = new("continue", "entrada") });

        var diagnostics = FlowValidator.Validate(graph);
        Assert.True(diagnostics.Any(d => d.Severity == Severity.Erro && d.NodeId == "break"));
        Assert.True(diagnostics.Any(d => d.Severity == Severity.Erro && d.NodeId == "continue"));
    }

    [Test] public static void Validador_ReturnComValorManualGeraErro()
    {
        var graph = new FlowGraph { Name = "RetornoComValor" };
        var start = NodeLibrary.EventStart("evt");
        var terminal = NodeLibrary.FlowReturn("return");
        terminal.Inputs.Add(new FlowPin { Name = "valor", Type = FlowType.Int, Direction = PinDirection.Input });
        terminal.Literals["valor"] = FlowValue.OfInt(1);
        graph.Nodes.AddRange(new[] { start, terminal });
        graph.Connections.Add(new FlowConnection { From = new("evt", "corpo"), To = new("return", "entrada") });

        var diagnostics = FlowValidator.Validate(graph);
        Assert.True(diagnostics.Any(d => d.Severity == Severity.Erro && d.NodeId == "return" && d.Message.Contains("não aceita valor")),
            "um .aflow editado manualmente não pode introduzir return com valor em evento void");
    }

    [Test] public static void Parser_RejeitaReturnComValorEInstrucaoInalcancavel()
    {
        const string withValue =
            "public class Invalido { public void Start() { return 1; } }";
        Assert.Throws<FormatException>(() => CSharpToFlow.Parse(withValue),
            "eventos void não podem aceitar return com valor");

        const string unreachable =
            "public class Inalcancavel { public int x = 0; public void Start() { return; x = 1; } }";
        Assert.Throws<FormatException>(() => CSharpToFlow.Parse(unreachable),
            "o parser deve rejeitar código inalcançável em vez de reinterpretá-lo silenciosamente");
    }

    [Test] public static void RoundTrip_SerializacaoPreservaNosDeControleTerminal()
    {
        var graph = new FlowGraph { Name = "Terminais" };
        graph.Nodes.Add(NodeLibrary.FlowReturn("return"));
        graph.Nodes.Add(NodeLibrary.FlowBreak("break"));
        graph.Nodes.Add(NodeLibrary.FlowContinue("continue"));

        string text = FlowSerializer.Serialize(graph);
        var reconstructed = FlowSerializer.Deserialize(text);
        Assert.True(reconstructed.Nodes.Any(n => n.NodeType == NodeTypes.FlowReturn));
        Assert.True(reconstructed.Nodes.Any(n => n.NodeType == NodeTypes.FlowBreak));
        Assert.True(reconstructed.Nodes.Any(n => n.NodeType == NodeTypes.FlowContinue));
        Assert.True(reconstructed.Nodes.All(n => n.Inputs.Count == 1 && n.Inputs[0].Name == "entrada"));
        Assert.True(reconstructed.Nodes.All(n => n.Outputs.Count == 0),
            "nós terminais não podem ganhar saída durante serialização");
    }

    [Test] public static void Diferencial_CSharpOriginalInterpretadorECSharpRegeneradoConcordam()
    {
        const string returnCode =
            "public class RetornoReal\n" +
            "{\n" +
            "    public bool sair = true;\n" +
            "    public int marcador = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        if (sair)\n" +
            "        {\n" +
            "            return;\n" +
            "        }\n" +
            "        marcador = 9;\n" +
            "    }\n" +
            "}\n";

        const string loopCode =
            "public class ControleReal\n" +
            "{\n" +
            "    public int contador = 0;\n" +
            "    public int soma = 0;\n" +
            "    public int marcador = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        while (contador < 5)\n" +
            "        {\n" +
            "            contador = contador + 1;\n" +
            "            if (contador == 2)\n" +
            "            {\n" +
            "                continue;\n" +
            "            }\n" +
            "            if (contador == 4)\n" +
            "            {\n" +
            "                break;\n" +
            "            }\n" +
            "            soma = soma + 1;\n" +
            "        }\n" +
            "        marcador = 9;\n" +
            "    }\n" +
            "}\n";

        var returnGraph = CSharpToFlow.Parse(returnCode);
        var returnInterpreter = new FlowInterpreter(returnGraph);
        returnInterpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(0L, returnInterpreter.Variables["marcador"].IntValue);

        var loopGraph = CSharpToFlow.Parse(loopCode);
        var loopInterpreter = new FlowInterpreter(loopGraph);
        loopInterpreter.RunEvent(NodeTypes.EventStart);
        Assert.Equal(4L, loopInterpreter.Variables["contador"].IntValue);
        Assert.Equal(2L, loopInterpreter.Variables["soma"].IntValue);
        Assert.Equal(9L, loopInterpreter.Variables["marcador"].IntValue);

        string source =
            "namespace OriginalReturn\n{\n" + returnCode + "}\n" +
            "namespace RegeneratedReturn\n{\n" + FlowToCSharp.Generate(returnGraph) + "}\n" +
            "namespace OriginalLoop\n{\n" + loopCode + "}\n" +
            "namespace RegeneratedLoop\n{\n" + FlowToCSharp.Generate(loopGraph) + "}\n" +
            "public static class Program\n" +
            "{\n" +
            "    public static void Main()\n" +
            "    {\n" +
            "        var or = new OriginalReturn.RetornoReal(); or.Start();\n" +
            "        var rr = new RegeneratedReturn.RetornoReal(); rr.Start();\n" +
            "        var ol = new OriginalLoop.ControleReal(); ol.Start();\n" +
            "        var rl = new RegeneratedLoop.ControleReal(); rl.Start();\n" +
            "        Console.WriteLine($\"return:{or.marcador}:{rr.marcador}\");\n" +
            "        Console.WriteLine($\"loop:{ol.contador},{ol.soma},{ol.marcador}:{rl.contador},{rl.soma},{rl.marcador}\");\n" +
            "    }\n" +
            "}\n";

        string output = CompileAndRunCSharp(source);
        string normalized = NormalizarFimDeLinha(output).Trim();
        Assert.Equal("return:0:0\nloop:4,2,9:4,2,9", normalized,
            "C# original, interpretador e C# regenerado precisam produzir o mesmo estado determinístico");
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

    [Test] public static void CodeGen_SeSemSenaoSeguidoDeInstrucaoApareceDepoisDoBlocoFechado()
    {
        var graph = GrafoIfSemElseSeguidoDeInstrucao(condicaoInicial: true);
        string code = FlowToCSharp.Generate(graph);

        string esperado =
            "public class SemElse\n" +
            "{\n" +
            "    public bool cond = true;\n" +
            "    public int a = 0;\n" +
            "    public int marcador = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        if (cond)\n" +
            "        {\n" +
            "            a = 1;\n" +
            "        }\n" +
            "        marcador = 9;\n" +
            "    }\n" +
            "}\n";

        Assert.Equal(NormalizarFimDeLinha(esperado), NormalizarFimDeLinha(code));
    }

    [Test] public static void CodeGen_SeComSenaoSeguidoDeInstrucaoNaoDuplicaNemPerdeAInstrucaoSeguinte()
    {
        var graph = GrafoIfElse();
        var marcador = new FlowVariable { Name = "marcador", Type = FlowType.Int, Initial = FlowValue.OfInt(0), Scope = VarScope.Object };
        graph.Variables.Add(marcador);
        var setMarcador = NodeLibrary.SetVariable("setMarcador", "marcador", FlowType.Int);
        setMarcador.Literals["valor"] = FlowValue.OfInt(5);
        graph.Nodes.Add(setMarcador);
        graph.Connections.Add(new FlowConnection { From = new("if", "depois"), To = new("setMarcador", "entrada") });

        string code = FlowToCSharp.Generate(graph);

        int posFechaBloco = code.IndexOf("        }\n", StringComparison.Ordinal); // fecha o "else"
        int posMarcador = code.IndexOf("marcador = 5;", StringComparison.Ordinal);
        Assert.True(posFechaBloco >= 0, "esperava encontrar o fechamento do bloco 'if/else' no código gerado");
        Assert.True(posMarcador >= 0, "a instrução 'depois' do if/else deveria aparecer no código gerado");
        Assert.True(posMarcador > posFechaBloco, "a instrução depois do if/else deveria vir DEPOIS do bloco fechado, não dentro dele");

        int quantasVezesAparece = System.Text.RegularExpressions.Regex.Matches(code, "marcador = 5;").Count;
        Assert.Equal(1, quantasVezesAparece, "a instrução depois do if/else não deveria ser duplicada");
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

    [Test] public static void RoundTrip_ParserLeIfNaoUltimoEReconstroiOGrafoCorretamente()
    {
        // C# escrito exatamente na convenção de formatação de FlowToCSharp.Generate: um "if/else"
        // que NÃO é a última instrução do bloco, seguido de mais uma atribuição. Isso já lançava
        // FormatException antes desta mudança; agora precisa ser lido normalmente.
        string original =
            "public class Cond\n" +
            "{\n" +
            "    public int a = 0;\n" +
            "    public int b = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        if (a > 0)\n" +
            "        {\n" +
            "            b = 1;\n" +
            "        }\n" +
            "        else\n" +
            "        {\n" +
            "            b = 2;\n" +
            "        }\n" +
            "        b = b + 10;\n" +
            "    }\n" +
            "}\n";

        var graph1 = CSharpToFlow.Parse(original);

        // Critério de equivalência nº 1: gerar C# de volta a partir do grafo reconstruído produz
        // TEXTO IDÊNTICO ao original — o gerador é a forma canônica deste subconjunto, então
        // parse -> generate é um ponto fixo quando a entrada já estava nessa forma canônica.
        string code1 = FlowToCSharp.Generate(graph1);
        Assert.Equal(NormalizarFimDeLinha(original), NormalizarFimDeLinha(code1),
            "gerar C# a partir do grafo reconstruído deveria reproduzir o texto original byte a byte");

        // Critério de equivalência nº 2: parse -> generate -> parse -> generate converge (é
        // realmente um round-trip estável, não um acidente da primeira rodada).
        var graph2 = CSharpToFlow.Parse(code1);
        string code2 = FlowToCSharp.Generate(graph2);
        Assert.Equal(code1, code2, "uma segunda rodada de parse/generate deveria produzir exatamente o mesmo texto");

        // Critério de equivalência nº 3 (comportamental): os dois ramos do "if" convergem para a
        // mesma instrução seguinte, e ela roda não importa qual ramo foi tomado.
        var g1 = CSharpToFlow.Parse(original);
        DefinirValorInicial(g1, "a", FlowValue.OfInt(1)); // a > 0: ramo "então"
        var interp1 = new FlowInterpreter(g1);
        interp1.RunEvent(NodeTypes.EventStart);
        Assert.Equal(11L, interp1.Variables["b"].IntValue, "ramo 'então' (b=1) mais a instrução seguinte (b+=10) deveria dar 11");

        var g2 = CSharpToFlow.Parse(original);
        DefinirValorInicial(g2, "a", FlowValue.OfInt(-1)); // a <= 0: ramo "senão"
        var interp2 = new FlowInterpreter(g2);
        interp2.RunEvent(NodeTypes.EventStart);
        Assert.Equal(12L, interp2.Variables["b"].IntValue, "ramo 'senão' (b=2) mais a instrução seguinte (b+=10) deveria dar 12");
    }

    /// <summary>"Initial" de <see cref="FlowVariable"/> é <c>init</c>-only; para reutilizar um
    /// grafo já montado com outro valor inicial, troca a variável inteira preservando tipo/escopo.</summary>
    private static void DefinirValorInicial(FlowGraph graph, string nome, FlowValue novoValor)
    {
        var antiga = graph.Variables.First(v => v.Name == nome);
        graph.Variables.Remove(antiga);
        graph.Variables.Add(new FlowVariable { Name = antiga.Name, Type = antiga.Type, Initial = novoValor, Scope = antiga.Scope });
    }

    [Test] public static void RoundTrip_ParserLeIfSemSenaoSeguidoDeInstrucao()
    {
        string original =
            "public class SoEntao\n" +
            "{\n" +
            "    public int a = 0;\n" +
            "    public int b = 0;\n" +
            "\n" +
            "    public void Start()\n" +
            "    {\n" +
            "        if (a > 0)\n" +
            "        {\n" +
            "            b = 1;\n" +
            "        }\n" +
            "        b = b + 10;\n" +
            "    }\n" +
            "}\n";

        var graph = CSharpToFlow.Parse(original);
        string regenerado = FlowToCSharp.Generate(graph);
        Assert.Equal(NormalizarFimDeLinha(original), NormalizarFimDeLinha(regenerado),
            "'if' sem 'else' seguido de instrução deveria reconstruir e regerar o mesmo texto");

        // condição falsa: o "então" não roda, mas a instrução seguinte (b += 10) precisa rodar de qualquer jeito.
        DefinirValorInicial(graph, "a", FlowValue.OfInt(-1));
        var interp = new FlowInterpreter(graph);
        interp.RunEvent(NodeTypes.EventStart);
        Assert.Equal(10L, interp.Variables["b"].IntValue, "sem 'else', a instrução depois do 'if' ainda precisa rodar quando a condição é falsa");
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

    private static string CompileAndRunCSharp(string source)
    {
        string directory = Path.Combine(Path.GetTempPath(), $"aether-flow-differential-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string projectPath = Path.Combine(directory, "Differential.csproj");
            File.WriteAllText(projectPath,
                "<Project Sdk=\"Microsoft.NET.Sdk\">" +
                "<PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework>" +
                "<ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable>" +
                "<RestoreIgnoreFailedSources>true</RestoreIgnoreFailedSources></PropertyGroup></Project>");
            File.WriteAllText(Path.Combine(directory, "NuGet.config"),
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><configuration><packageSources><clear /></packageSources></configuration>");
            File.WriteAllText(Path.Combine(directory, "Program.cs"), source);

            string dotnetHost = Environment.GetEnvironmentVariable("DOTNET_HOST_PATH") ?? "dotnet";
            var startInfo = new ProcessStartInfo
            {
                FileName = dotnetHost,
                WorkingDirectory = directory,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
            };
            startInfo.ArgumentList.Add("run");
            startInfo.ArgumentList.Add("--project");
            startInfo.ArgumentList.Add(projectPath);
            startInfo.ArgumentList.Add("--configuration");
            startInfo.ArgumentList.Add("Release");
            startInfo.ArgumentList.Add("--nologo");
            startInfo.ArgumentList.Add("--verbosity");
            startInfo.ArgumentList.Add("quiet");

            using var process = Process.Start(startInfo) ?? throw new InvalidOperationException("não foi possível iniciar o compilador C# do SDK.");
            Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
            Task<string> stderrTask = process.StandardError.ReadToEndAsync();
            if (!process.WaitForExit(60_000))
            {
                process.Kill(entireProcessTree: true);
                throw new TimeoutException("a compilação diferencial de C# excedeu 60 segundos.");
            }
            Task.WaitAll(stdoutTask, stderrTask);
            if (process.ExitCode != 0)
                throw new InvalidOperationException($"C# diferencial não compilou/executou (código {process.ExitCode}):\n{stderrTask.Result}\n{stdoutTask.Result}");
            return stdoutTask.Result;
        }
        finally
        {
            if (Directory.Exists(directory)) Directory.Delete(directory, recursive: true);
        }
    }
}
