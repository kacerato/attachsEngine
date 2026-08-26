using Aether.Flow.Ast;
using Aether.Flow.Runtime;

namespace Aether.Flow.Validation;

/// <summary>
/// Confere um <see cref="FlowGraph"/> antes de gerar código ou interpretar.
/// Toda mensagem é em português, escrita para alguém que nunca programou —
/// nada de "pin", "null" ou "exception" nos textos.
/// </summary>
public static class FlowValidator
{
    public static List<FlowDiagnostic> Validate(FlowGraph graph, FlowExecutionContext? context = null)
    {
        var diagnostics = new List<FlowDiagnostic>();

        CheckPinosObrigatorios(graph, diagnostics);
        CheckTiposDasConexoes(graph, diagnostics);
        CheckCicloDeDados(graph, diagnostics);
        CheckCicloDeExecucaoIlegitimo(graph, diagnostics);
        CheckEscopoDoControle(graph, diagnostics);
        CheckVariaveisReferenciadas(graph, diagnostics);
        CheckEventosDuplicados(graph, diagnostics);
        CheckNosOrfaos(graph, diagnostics);
        if (context is not null)
            CheckCapabilities(graph, context, diagnostics);

        return diagnostics;
    }

    private static void CheckCapabilities(
        FlowGraph graph, FlowExecutionContext context, List<FlowDiagnostic> diagnostics)
    {
        foreach (var node in graph.Nodes)
        {
            FlowCapability missing = context.Missing(node.RequiredCapabilities);
            if (missing == FlowCapability.None) continue;

            diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                $"O bloco {FriendlyName(node)} precisa de serviços que este contexto não oferece: {missing}. " +
                "Conecte o Behavior ao runtime correto ou remova o bloco.",
                node.Id));
        }
    }

    private static void CheckPinosObrigatorios(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        foreach (var node in graph.Nodes)
        {
            if (node.NodeType == NodeTypes.CodeRaw) continue; // pinos declarados livremente, não exigimos literal

            foreach (var pin in node.Inputs)
            {
                if (pin.IsExec) continue; // pino de execução é ligado por quem chama; órfão é checado à parte
                bool conectado = graph.IncomingTo(node.Id, pin.Name) is not null;
                bool temLiteral = node.Literals.ContainsKey(pin.Name);
                if (!conectado && !temLiteral)
                {
                    diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                        $"O bloco {FriendlyName(node)} precisa de um valor na entrada \"{pin.Name}\" — " +
                        $"conecte alguma coisa ali ou preencha um valor.",
                        node.Id));
                }
            }
        }
    }

    private static void CheckTiposDasConexoes(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        foreach (var conn in graph.Connections)
        {
            var origem = graph.FindNode(conn.From.NodeId);
            var destino = graph.FindNode(conn.To.NodeId);
            var pinoOrigem = origem?.FindOutput(conn.From.PinName);
            var pinoDestino = destino?.FindInput(conn.To.PinName);
            if (pinoOrigem is null || pinoDestino is null) continue; // pino inexistente: fora do escopo deste validador

            if (pinoOrigem.IsExec || pinoDestino.IsExec) continue; // fluxo de execução não tem "tipo" de dado

            if (!TiposCompativeis(pinoOrigem.Type, pinoDestino.Type))
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    $"O fio que liga \"{FriendlyName(origem!)}\" a \"{FriendlyName(destino!)}\" não combina: " +
                    $"um lado dá {NomeDoTipo(pinoOrigem.Type)} e o outro espera {NomeDoTipo(pinoDestino.Type)}. " +
                    $"Troque um dos blocos ou ajuste o valor.",
                    destino!.Id));
            }
        }
    }

    private static bool TiposCompativeis(FlowType from, FlowType to)
    {
        if (from == to) return true;
        if (from == FlowType.Any || to == FlowType.Any) return true;
        if (from == FlowType.Int && to == FlowType.Float) return true; // conversão numérica implícita, como em C#
        return false;
    }

    private static void CheckCicloDeDados(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        // Aresta: nó de origem de um pino de SAÍDA de dado -> nó de destino do pino de ENTRADA que ele alimenta.
        var edges = graph.Connections
            .Where(c => IsDataPin(graph, c.From, PinDirection.Output) && IsDataPin(graph, c.To, PinDirection.Input))
            .Select(c => (from: c.From.NodeId, to: c.To.NodeId))
            .ToList();

        var cycleNode = FindCycle(graph.Nodes.Select(n => n.Id), edges);
        if (cycleNode is not null)
        {
            diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                $"Os dados que chegam em \"{FriendlyName(graph.FindNode(cycleNode)!)}\" dependem dele mesmo, " +
                "como uma cobra mordendo o próprio rabo — desfaça esse laço de valores.",
                cycleNode));
        }
    }

    private static void CheckCicloDeExecucaoIlegitimo(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        var edges = graph.Connections
            .Where(c => IsExecPin(graph, c.From) && IsExecPin(graph, c.To))
            .Select(c => (from: c.From.NodeId, to: c.To.NodeId))
            .ToList();

        // Pode haver mais de um ciclo; reportamos os que não passam por um nó de laço.
        var visitedGlobal = new HashSet<string>();
        foreach (var start in graph.Nodes.Select(n => n.Id))
        {
            if (visitedGlobal.Contains(start)) continue;
            var cycle = FindCycleWithPath(start, edges);
            if (cycle is null) continue;
            foreach (var n in cycle) visitedGlobal.Add(n);

            bool temNoDeLaco = cycle.Any(id => NodeLibrary_IsLoop(graph.FindNode(id)));
            if (!temNoDeLaco)
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    "A execução entra num círculo sem fim: um bloco leva a outro e volta para o primeiro, " +
                    "sem passar por um bloco de \"Enquanto\" ou \"Para cada\" que controle a repetição.",
                    cycle[0]));
            }
        }
    }

    private static bool NodeLibrary_IsLoop(FlowNode? n) => n is not null && NodeTypes.LoopNodeTypes.Contains(n.NodeType);

    private static void CheckEscopoDoControle(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        var loopBodyNodes = new HashSet<string>();
        foreach (var loop in graph.Nodes.Where(NodeLibrary_IsLoop))
        {
            var pending = new Stack<string>(graph.OutgoingFrom(loop.Id, "corpo").Select(c => c.To.NodeId));
            var visited = new HashSet<string>();
            while (pending.Count > 0)
            {
                string id = pending.Pop();
                // Uma aresta explícita de volta ao próprio laço fecha a região;
                // não seguimos seus pinos "corpo"/"fim", que pertencem a
                // regiões diferentes.
                if (id == loop.Id || !visited.Add(id)) continue;
                loopBodyNodes.Add(id);
                var node = graph.FindNode(id);
                if (node is null) continue;
                foreach (var pin in node.Outputs.Where(p => p.IsExec))
                    foreach (var connection in graph.OutgoingFrom(id, pin.Name))
                        pending.Push(connection.To.NodeId);
            }
        }

        foreach (var node in graph.Nodes)
        {
            bool isTerminal = node.NodeType is NodeTypes.FlowReturn or NodeTypes.FlowBreak or NodeTypes.FlowContinue;
            if (!isTerminal) continue;

            if (node.Outputs.Any(p => p.IsExec))
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    $"O bloco {FriendlyName(node)} encerra o caminho atual e não pode ter uma saída de execução.",
                    node.Id));
            }

            if (node.Inputs.Any(p => !p.IsExec))
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    $"O bloco {FriendlyName(node)} não aceita valor. Os eventos atuais não possuem retorno com resultado.",
                    node.Id));
            }

            if (node.NodeType is NodeTypes.FlowBreak or NodeTypes.FlowContinue && !loopBodyNodes.Contains(node.Id))
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    $"O bloco {FriendlyName(node)} só pode ser usado dentro do corpo de um bloco Enquanto.",
                    node.Id));
            }
        }
    }

    private static void CheckVariaveisReferenciadas(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        foreach (var node in graph.Nodes)
        {
            if (node.NodeType != NodeTypes.SetVariable && node.NodeType != NodeTypes.GetVariable) continue;
            if (!node.Properties.TryGetValue("VariableName", out var varName) || graph.FindVariable(varName) is null)
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Erro,
                    $"O bloco {FriendlyName(node)} usa uma variável chamada \"{node.Properties.GetValueOrDefault("VariableName", "?")}\" " +
                    "que não existe mais neste projeto — crie essa variável ou escolha outra.",
                    node.Id));
            }
        }
    }

    private static void CheckEventosDuplicados(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        foreach (var group in graph.EventNodes.GroupBy(n => n.NodeType))
        {
            var nodes = group.ToList();
            if (nodes.Count > 1)
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Aviso,
                    $"Existem {nodes.Count} blocos \"{FriendlyName(nodes[0])}\" no mesmo projeto — só o primeiro vai " +
                    "funcionar como você espera. Junte a lógica dos dois em um só.",
                    nodes[1].Id));
            }
        }
    }

    private static void CheckNosOrfaos(FlowGraph graph, List<FlowDiagnostic> diagnostics)
    {
        var reachable = new HashSet<string>(graph.EventNodes.Select(n => n.Id));

        // Alcançável por execução: segue pinos de execução a partir de eventos.
        var pending = new Queue<string>(reachable);
        while (pending.Count > 0)
        {
            var id = pending.Dequeue();
            var node = graph.FindNode(id)!;
            foreach (var pin in node.Outputs.Where(p => p.IsExec))
            {
                foreach (var conn in graph.OutgoingFrom(id, pin.Name))
                {
                    if (reachable.Add(conn.To.NodeId)) pending.Enqueue(conn.To.NodeId);
                }
            }
        }

        // Alcançável por dado: qualquer nó que alimenta uma entrada de dado de um nó já alcançável
        // também é considerado usado (nós puramente de dado, como "math.add", não têm pino de execução).
        bool changed = true;
        while (changed)
        {
            changed = false;
            foreach (var conn in graph.Connections)
            {
                if (reachable.Contains(conn.To.NodeId) && reachable.Add(conn.From.NodeId))
                    changed = true;
            }
        }

        foreach (var node in graph.Nodes)
        {
            if (!reachable.Contains(node.Id))
            {
                diagnostics.Add(new FlowDiagnostic(Severity.Aviso,
                    $"O bloco {FriendlyName(node)} não está ligado a nenhum \"Quando...\" — ele nunca vai rodar. " +
                    "Conecte-o a um evento ou apague-o.",
                    node.Id));
            }
        }
    }

    // ---- utilidades ----

    private static bool IsDataPin(FlowGraph graph, FlowEndpoint ep, PinDirection dir)
    {
        var node = graph.FindNode(ep.NodeId);
        var pin = dir == PinDirection.Output ? node?.FindOutput(ep.PinName) : node?.FindInput(ep.PinName);
        return pin is not null && !pin.IsExec;
    }

    private static bool IsExecPin(FlowGraph graph, FlowEndpoint ep)
    {
        var node = graph.FindNode(ep.NodeId);
        var pin = node?.FindOutput(ep.PinName) ?? node?.FindInput(ep.PinName);
        return pin?.IsExec == true;
    }

    /// <summary>Devolve o id de um nó dentro de um ciclo, se existir algum, usando DFS com pilha de cor.</summary>
    private static string? FindCycle(IEnumerable<string> nodeIds, List<(string from, string to)> edges)
    {
        var byFrom = edges.GroupBy(e => e.from).ToDictionary(g => g.Key, g => g.Select(e => e.to).ToList());
        var color = new Dictionary<string, int>(); // 0=branco,1=cinza,2=preto

        foreach (var id in nodeIds)
        {
            if (!color.ContainsKey(id) && Visit(id)) return _foundAt;
        }
        return null;

        bool Visit(string id)
        {
            color[id] = 1;
            if (byFrom.TryGetValue(id, out var next))
            {
                foreach (var to in next)
                {
                    if (color.GetValueOrDefault(to, 0) == 1) { _foundAt = to; return true; }
                    if (color.GetValueOrDefault(to, 0) == 0 && Visit(to)) return true;
                }
            }
            color[id] = 2;
            return false;
        }
    }

    [ThreadStatic] private static string? _foundAt;

    /// <summary>Como <see cref="FindCycle"/>, mas devolve a lista de nós que compõem o ciclo encontrado
    /// a partir de <paramref name="start"/> (ou null se não há ciclo alcançável a partir dele).</summary>
    private static List<string>? FindCycleWithPath(string start, List<(string from, string to)> edges)
    {
        var byFrom = edges.GroupBy(e => e.from).ToDictionary(g => g.Key, g => g.Select(e => e.to).ToList());
        var path = new List<string>();
        var onPath = new HashSet<string>();
        var visited = new HashSet<string>();

        List<string>? Visit(string id)
        {
            if (onPath.Contains(id))
            {
                int idx = path.IndexOf(id);
                return path.GetRange(idx, path.Count - idx);
            }
            if (visited.Contains(id)) return null;
            visited.Add(id);
            path.Add(id);
            onPath.Add(id);
            if (byFrom.TryGetValue(id, out var next))
            {
                foreach (var to in next)
                {
                    var found = Visit(to);
                    if (found is not null) return found;
                }
            }
            path.RemoveAt(path.Count - 1);
            onPath.Remove(id);
            return null;
        }

        return Visit(start);
    }

    private static string FriendlyName(FlowNode node) => node.NodeType switch
    {
        NodeTypes.EventStart => "Quando começar",
        NodeTypes.EventUpdate => "A cada quadro",
        NodeTypes.EventCollision => "Quando colidir",
        NodeTypes.FlowIf => "Se",
        NodeTypes.FlowWhile => "Enquanto",
        NodeTypes.FlowReturn => "Retornar",
        NodeTypes.FlowBreak => "Parar laço",
        NodeTypes.FlowContinue => "Continuar laço",
        NodeTypes.SetVariable => $"Definir {node.Properties.GetValueOrDefault("VariableName", "?")}",
        NodeTypes.GetVariable => node.Properties.GetValueOrDefault("VariableName", "?"),
        NodeTypes.MathAdd => "Somar",
        NodeTypes.MathSubtract => "Subtrair",
        NodeTypes.MathMultiply => "Multiplicar",
        NodeTypes.MathDivide => "Dividir",
        NodeTypes.MathCompare => "Comparar",
        NodeTypes.CodeRaw => "Código",
        NodeTypes.LogMessage => "Registrar mensagem",
        _ => node.NodeType,
    };

    private static string NomeDoTipo(FlowType type) => type switch
    {
        FlowType.Bool => "verdadeiro/falso",
        FlowType.Int => "número inteiro",
        FlowType.Float => "número",
        FlowType.Float3 => "posição/direção",
        FlowType.Quaternion => "rotação",
        FlowType.String => "texto",
        FlowType.Entity => "objeto da cena",
        FlowType.Asset => "arquivo do projeto",
        _ => type.ToString(),
    };
}
