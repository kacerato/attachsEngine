using Aether.Editing;

namespace Aether.Tests;

/// <summary>
/// Undo/redo e WAL de recuperação (Etapa 1.4.4 do plano). O cenário central de "crash" — gravar no
/// WAL, não fazer checkpoint, simular reabrir o processo e recuperar — é <see
/// cref="WriteAheadLog_Recover_ApósCrashSemCheckpoint_DevolveComandosNaOrdemCerta"/>.
/// </summary>
public static class EditingTests
{
    /// <summary>Comando fictício: registra "do:X"/"undo:X" num log compartilhado, para os testes
    /// poderem verificar ordem e conteúdo sem precisar de um documento de cena de verdade.</summary>
    private sealed class ComandoTeste : IEditCommand
    {
        public readonly string Alvo;
        public readonly float Delta;
        private readonly List<string>? _log;

        public ComandoTeste(string alvo, float delta, List<string>? log = null)
        {
            Alvo = alvo;
            Delta = delta;
            _log = log;
        }

        public string Description => $"Mover {Alvo} em {Delta}";
        public void Do() => _log?.Add($"do:{Alvo}:{Delta}");
        public void Undo() => _log?.Add($"undo:{Alvo}:{Delta}");
    }

    /// <summary>Comando fictício com um gancho arbitrário em Do() — usado só para observar, de
    /// dentro do próprio Do(), o estado do WAL no instante em que ele roda.</summary>
    private sealed class ComandoComGancho : IEditCommand
    {
        private readonly Action _aoExecutar;
        public ComandoComGancho(Action aoExecutar) => _aoExecutar = aoExecutar;
        public string Description => "Comando de teste com gancho";
        public void Do() => _aoExecutar();
        public void Undo() { }
    }

    private static WalCommandRegistry NovoRegistro()
    {
        var registry = new WalCommandRegistry();
        registry.Register<ComandoTeste>("ComandoTeste",
            (cmd, w) => { w.Write(cmd.Alvo); w.Write(cmd.Delta); },
            r => new ComandoTeste(r.ReadString(), r.ReadSingle()));
        registry.Register<ComandoComGancho>("ComandoComGancho",
            (_, _) => { },
            _ => new ComandoComGancho(() => { }));
        return registry;
    }

    private static string NovoCaminhoTemporario() => System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"aether-wal-{Guid.NewGuid():N}.log");

    // ---------- UndoStack ----------

    [Test] public static void Execute_RodaOComandoEEmpilhaParaUndo()
    {
        var log = new List<string>();
        var stack = new UndoStack();
        stack.Execute(new ComandoTeste("Cubo", 1f, log));

        Assert.Equal(1, stack.UndoCount);
        Assert.Equal(0, stack.RedoCount);
        Assert.Equal(1, log.Count);
        Assert.Equal("do:Cubo:1", log[0]);
    }

    [Test] public static void Undo_DesfazNaOrdemInversaDeExecucao()
    {
        var log = new List<string>();
        var stack = new UndoStack();
        stack.Execute(new ComandoTeste("A", 1f, log));
        stack.Execute(new ComandoTeste("B", 2f, log));

        bool desfez = stack.Undo();

        Assert.True(desfez);
        Assert.Equal(3, log.Count);
        Assert.Equal("undo:B:2", log[2], "o último executado é o primeiro desfeito");
        Assert.Equal(1, stack.UndoCount);
        Assert.Equal(1, stack.RedoCount);
    }

    [Test] public static void Redo_ReaplicaOQueFoiDesfeito()
    {
        var log = new List<string>();
        var stack = new UndoStack();
        stack.Execute(new ComandoTeste("A", 1f, log));
        stack.Undo();

        bool refez = stack.Redo();

        Assert.True(refez);
        Assert.Equal("do:A:1", log[^1], "redo chama Do() de novo, não algum 'reaplica' separado");
        Assert.Equal(1, stack.UndoCount);
        Assert.Equal(0, stack.RedoCount);
    }

    [Test] public static void Execute_DepoisDeUmUndo_DescartaOFuturoDeRedo()
    {
        var log = new List<string>();
        var stack = new UndoStack();
        stack.Execute(new ComandoTeste("A", 1f, log));
        stack.Execute(new ComandoTeste("B", 2f, log));
        stack.Undo(); // redo agora tem B

        stack.Execute(new ComandoTeste("C", 3f, log));

        Assert.Equal(0, stack.RedoCount, "um novo comando depois de Undo apaga o que tinha sido desfeito");
        Assert.False(stack.Redo(), "não há mais nada para refazer");
    }

    [Test] public static void Undo_PilhaVazia_DevolveFalseSemLancar()
    {
        var stack = new UndoStack();
        Assert.False(stack.Undo());
        Assert.False(stack.Redo());
    }

    [Test] public static void UndoStack_EstourarProfundidadeMaxima_DescartaOMaisAntigoENaoLanca()
    {
        var log = new List<string>();
        var stack = new UndoStack(maxDepth: 3);
        for (int i = 0; i < 5; i++) stack.Execute(new ComandoTeste($"Obj{i}", i, log));

        Assert.Equal(3, stack.UndoCount, "profundidade nunca passa do limite configurado");
        // Os dois mais antigos (Obj0, Obj1) foram descartados; o topo é o mais recente (Obj4).
        Assert.Equal("Mover Obj4 em 4", stack.PeekUndo!.Description);

        for (int i = 0; i < 3; i++) stack.Undo();
        Assert.False(stack.CanUndo, "só os 3 mais recentes podiam ser desfeitos — os mais antigos foram esquecidos, não travaram nem vazaram");
    }

    [Test] public static void UndoStack_ProfundidadeMaximaInvalida_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new UndoStack(maxDepth: 0));
    }

    // ---------- WalCommandRegistry ----------

    [Test] public static void WalCommandRegistry_RegistrarMesmaTagParaTipoDiferente_Lanca()
    {
        var registry = new WalCommandRegistry();
        registry.Register<ComandoTeste>("mesma-tag", (_, _) => { }, r => new ComandoTeste(r.ReadString(), 0f));
        Assert.Throws<InvalidOperationException>(() =>
            registry.Register<ComandoComGancho>("mesma-tag", (_, _) => { }, _ => new ComandoComGancho(() => { })));
    }

    [Test] public static void WalCommandRegistry_RegistrarDeNovoMesmoParTagTipo_EhIdempotente()
    {
        var registry = new WalCommandRegistry();
        registry.Register<ComandoTeste>("tag", (cmd, w) => w.Write(cmd.Alvo), r => new ComandoTeste(r.ReadString(), 0f));
        registry.Register<ComandoTeste>("tag", (cmd, w) => w.Write(cmd.Alvo), r => new ComandoTeste(r.ReadString(), 0f)); // não lança
    }

    // ---------- WriteAheadLog: ordem grava-antes-de-aplicar ----------

    [Test] public static void ExecuteWithWal_GravaNoDiscoAntesDeChamarDoDoComando()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            using var wal = new WriteAheadLog(caminho, registry);
            var stack = new UndoStack();

            long tamanhoDoArquivoDentroDeDo = -1;
            var comando = new ComandoComGancho(() => tamanhoDoArquivoDentroDeDo = new FileInfo(caminho).Length);

            stack.ExecuteWithWal(wal, comando);

            Assert.True(tamanhoDoArquivoDentroDeDo > 0,
                "quando Do() rodou, o registro já precisava estar gravado (e no disco) — essa é a ordem que garante a recuperação");
            Assert.Equal(1, stack.UndoCount, "ExecuteWithWal também empilha para undo, como Execute normal");
        }
        finally { File.Delete(caminho); }
    }

    // ---------- WriteAheadLog: recuperação após "crash" ----------

    [Test] public static void WriteAheadLog_Recover_ApósCrashSemCheckpoint_DevolveComandosNaOrdemCerta()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            using (var wal = new WriteAheadLog(caminho, registry))
            {
                wal.Append(new ComandoTeste("A", 1f));
                wal.Append(new ComandoTeste("B", 2f));
                wal.Append(new ComandoTeste("C", 3f));
                // Sem Checkpoint(): simula o processo morrendo aqui, com trabalho não salvo.
            }

            // "Reabre": um WriteAheadLog novo apontando para o mesmo arquivo é como o editor volta
            // a vida depois do crash. Recover lê o que sobrou no disco antes de qualquer nova escrita.
            var recuperados = WriteAheadLog.Recover(caminho, registry);

            Assert.Equal(3, recuperados.Count, "os três comandos gravados antes do 'crash' precisam voltar");
            var c0 = (ComandoTeste)recuperados[0];
            var c1 = (ComandoTeste)recuperados[1];
            var c2 = (ComandoTeste)recuperados[2];
            Assert.Equal("A", c0.Alvo); Assert.Close(1f, c0.Delta);
            Assert.Equal("B", c1.Alvo); Assert.Close(2f, c1.Delta);
            Assert.Equal("C", c2.Alvo); Assert.Close(3f, c2.Delta);
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void WriteAheadLog_Recover_ArquivoInexistente_DevolveListaVazia()
    {
        string caminho = NovoCaminhoTemporario(); // nunca criado
        var recuperados = WriteAheadLog.Recover(caminho, NovoRegistro());
        Assert.Equal(0, recuperados.Count);
    }

    [Test] public static void WriteAheadLog_Checkpoint_ZeraOLogERecoverNaoTrazNadaDepois()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            using (var wal = new WriteAheadLog(caminho, registry))
            {
                wal.Append(new ComandoTeste("A", 1f));
                wal.Checkpoint(); // equivalente a um save de projeto bem-sucedido
            }

            var recuperados = WriteAheadLog.Recover(caminho, registry);
            Assert.Equal(0, recuperados.Count, "checkpoint significa que o comando já está salvo de verdade — nada para recuperar");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void WriteAheadLog_AppendContinuaAposReabrirNoMesmoArquivo()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            using (var wal1 = new WriteAheadLog(caminho, registry))
                wal1.Append(new ComandoTeste("A", 1f));

            using (var wal2 = new WriteAheadLog(caminho, registry)) // "reabre" o mesmo arquivo
                wal2.Append(new ComandoTeste("B", 2f));

            var recuperados = WriteAheadLog.Recover(caminho, registry);
            Assert.Equal(2, recuperados.Count, "reabrir continua anexando ao final, não sobrescreve o que já existia");
        }
        finally { File.Delete(caminho); }
    }

    // ---------- WriteAheadLog: recuperação parcial ante escrita incompleta ----------

    [Test] public static void WriteAheadLog_Recover_ArquivoTruncadoNoMeioDoUltimoRegistro_DevolveOsAnterioresSemLancar()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            long tamanhoApósDoisRegistros;
            using (var wal = new WriteAheadLog(caminho, registry))
            {
                wal.Append(new ComandoTeste("A", 1f));
                wal.Append(new ComandoTeste("B", 2f));
                tamanhoApósDoisRegistros = new FileInfo(caminho).Length;
                wal.Append(new ComandoTeste("C-nunca-deveria-aparecer", 99f));
            }

            // Simula "a energia caiu no meio do append do terceiro registro": corta o arquivo de
            // volta para um ponto dentro do terceiro registro (depois do cabeçalho válido dos dois
            // primeiros, mas antes do terceiro terminar de ser escrito por completo).
            using (var raw = new FileStream(caminho, FileMode.Open, FileAccess.ReadWrite))
                raw.SetLength(tamanhoApósDoisRegistros + 3); // corta bem no meio do payload do 3º registro

            var recuperados = WriteAheadLog.Recover(caminho, registry); // não pode lançar

            Assert.Equal(2, recuperados.Count, "só os dois registros completos e íntegros voltam; o truncado é descartado, não os anteriores");
            Assert.Equal("A", ((ComandoTeste)recuperados[0]).Alvo);
            Assert.Equal("B", ((ComandoTeste)recuperados[1]).Alvo);
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void WriteAheadLog_Recover_ChecksumCorrompido_ParaAntesDoRegistroCorrompido()
    {
        string caminho = NovoCaminhoTemporario();
        try
        {
            var registry = NovoRegistro();
            using (var wal = new WriteAheadLog(caminho, registry))
            {
                wal.Append(new ComandoTeste("A", 1f));
                wal.Append(new ComandoTeste("B", 2f));
            }

            // Corrompe um byte no meio do payload do segundo registro (depois do cabeçalho de 8
            // bytes do primeiro registro + seu payload) — o tamanho declarado continua batendo, só
            // o conteúdo (e portanto o checksum) fica inconsistente.
            byte[] bytes = File.ReadAllBytes(caminho);
            bytes[bytes.Length - 2] ^= 0xFF;
            File.WriteAllBytes(caminho, bytes);

            var recuperados = WriteAheadLog.Recover(caminho, registry);

            Assert.Equal(1, recuperados.Count, "o primeiro registro (intacto) volta; o segundo (checksum não bate) é descartado sem lançar");
            Assert.Equal("A", ((ComandoTeste)recuperados[0]).Alvo);
        }
        finally { File.Delete(caminho); }
    }
}
