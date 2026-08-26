using Aether.Resources;

namespace Aether.Tests;

/// <summary>
/// Item 1.4.5 do plano: binding C# via P/Invoke sobre o SQLite vendorizado (native/resources/
/// sqlite_bridge.h — já validado por tests/native/test_sqlite_bridge.cpp). Estes testes cobrem a
/// tradução gerenciada: conversão string↔UTF-8 na fronteira, mapeamento de erro nativo para <see
/// cref="SqliteException"/>, e round-trip através de <see cref="SqliteConnection"/>/<see
/// cref="SqliteStatement"/>.
/// <para>
/// Como os testes de física (ver <c>PhysicsTests</c>), retornam cedo se a biblioteca nativa
/// "aether_resources" não estiver ao lado do executável de teste — <see cref="NativeInterop"/>
/// decide se isso é aceitável (job unitário local) ou motivo de falha (job de integração,
/// AETHER_REQUIRE_NATIVE=1).
/// </para>
/// </summary>
public static class SqliteBridgeTests
{
    private static bool NativeLibraryAvailable() => NativeInterop.SqliteLibraryAvailable();

    private static string NovoCaminhoTemporario() => Path.Combine(Path.GetTempPath(), $"aether-sqlite-{Guid.NewGuid():N}.db");

    [Test] public static void Open_ArquivoNovo_SucedeEDeixaOArquivoNoDisco()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            Assert.True(File.Exists(caminho), what: "SQLite deve criar o arquivo físico ao abrir um caminho inexistente");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void Open_DiretorioInexistente_LancaSqliteException()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = Path.Combine(Path.GetTempPath(), $"aether-sqlite-diretorio-inexistente-{Guid.NewGuid():N}", "nested", "banco.db");
        Assert.Throws<SqliteException>(() => SqliteConnection.Open(caminho),
            what: "diretório pai inexistente deve falhar explicitamente, não criar silenciosamente nem devolver conexão inválida");
    }

    [Test] public static void Prepare_SqlInvalido_LancaComMensagemDoSqlite()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            var ex = Assert2ThrowsAndReturns<SqliteException>(() => conn.Prepare("ISTO NAO E SQL VALIDO ;;;"));
            Assert.True(ex.Message.Length > 0, what: "mensagem de erro deve vir preenchida com o texto real do SQLite, não vazia");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void Execute_CreateTable_NaoLanca()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE item (id INTEGER PRIMARY KEY, nome TEXT, peso REAL)"); // não deve lançar
            Assert.True(true, what: "CREATE TABLE válido não deve lançar");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void InsertSelect_RoundTripComOsTresTipos_PreservaValoresExatamente()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE item (id INTEGER PRIMARY KEY, nome TEXT, peso REAL)");

            using (var insert = conn.Prepare("INSERT INTO item (id, nome, peso) VALUES (?, ?, ?)"))
            {
                insert.BindInt64(1, 42);
                insert.BindText(2, "espada-de-ferro");
                insert.BindDouble(3, 3.5);
                Assert.Equal(SqliteStepResult.Done, insert.Step(), what: "INSERT bem-sucedido deve produzir Done, não Row");
            }

            using var select = conn.Prepare("SELECT id, nome, peso FROM item WHERE id = ?");
            select.BindInt64(1, 42);
            Assert.Equal(SqliteStepResult.Row, select.Step(), what: "linha inserida deve ser encontrada");

            Assert.Equal(SqliteColumnType.Integer, select.ColumnType(0), what: "coluna id deve reportar tipo Integer");
            Assert.Equal(42L, select.ColumnInt64(0), what: "id lido deve bater com o inserido");
            Assert.Equal("espada-de-ferro", select.ColumnText(1), what: "texto lido deve bater exatamente, incluindo hífens");
            Assert.Close(3.5f, (float)select.ColumnDouble(2), what: "peso lido deve bater com o inserido");

            Assert.Equal(SqliteStepResult.Done, select.Step(), what: "só uma linha foi inserida — segundo Step deve esgotar o resultado");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void InsertSelect_ComAcentuacaoEEmoji_PreservaUtf8Exatamente()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE t (v TEXT)");

            const string textoOriginal = "espadão flamejante 🔥 não-ASCII";
            using (var insert = conn.Prepare("INSERT INTO t (v) VALUES (?)"))
            {
                insert.BindText(1, textoOriginal);
                insert.Step();
            }

            using var select = conn.Prepare("SELECT v FROM t");
            select.Step();
            Assert.Equal(textoOriginal, select.ColumnText(0), what: "round-trip UTF-8 deve preservar acentuação e caracteres fora do plano ASCII");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void BindNull_LidoDeVoltaComoColumnTypeNull()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE t (v TEXT)");

            using (var insert = conn.Prepare("INSERT INTO t (v) VALUES (?)"))
            {
                insert.BindNull(1);
                insert.Step();
            }

            using var select = conn.Prepare("SELECT v FROM t");
            select.Step();
            Assert.Equal(SqliteColumnType.Null, select.ColumnType(0), what: "valor bindado como NULL deve ser lido de volta como tipo Null");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void SelectSemLinhas_DevolveDoneNaPrimeiraChamada()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE vazio (id INTEGER)");

            using var select = conn.Prepare("SELECT id FROM vazio");
            Assert.Equal(SqliteStepResult.Done, select.Step(), what: "tabela vazia deve produzir Done imediatamente, nunca Row");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void Reset_PermiteReexecutarOMesmoStatementComBindingsDiferentes()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE t (v INTEGER)");

            using (var insert = conn.Prepare("INSERT INTO t (v) VALUES (?)"))
            {
                insert.BindInt64(1, 1);
                insert.Step();
                insert.Reset();
                insert.BindInt64(1, 2);
                insert.Step();
            }

            using var count = conn.Prepare("SELECT COUNT(*) FROM t");
            count.Step();
            Assert.Equal(2L, count.ColumnInt64(0), what: "duas execuções via Reset devem inserir duas linhas, não sobrescrever uma com a outra");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void MultiplasLinhas_StepIteraNaOrdemDeInsercao()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using var conn = SqliteConnection.Open(caminho);
            conn.Execute("CREATE TABLE t (v INTEGER)");
            conn.Execute("INSERT INTO t (v) VALUES (10), (20), (30)");

            using var select = conn.Prepare("SELECT v FROM t ORDER BY v");
            var valores = new List<long>();
            while (select.Step() == SqliteStepResult.Row)
                valores.Add(select.ColumnInt64(0));

            Assert.Equal(3, valores.Count, what: "as três linhas inseridas devem ser todas enumeradas");
            Assert.Equal(10L, valores[0], what: "ordem deve respeitar o ORDER BY");
            Assert.Equal(30L, valores[2], what: "ordem deve respeitar o ORDER BY");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void Dispose_DaConexao_PermiteAbrirNovamenteOMesmoArquivo()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            using (var conn1 = SqliteConnection.Open(caminho))
                conn1.Execute("CREATE TABLE t (v INTEGER)");

            using var conn2 = SqliteConnection.Open(caminho);
            using var select = conn2.Prepare("SELECT COUNT(*) FROM t");
            select.Step();
            Assert.Equal(0L, select.ColumnInt64(0), what: "reabrir o mesmo arquivo depois do Dispose deve enxergar o schema persistido");
        }
        finally { File.Delete(caminho); }
    }

    [Test] public static void UsarConexaoAposDispose_LancaObjectDisposedException()
    {
        if (!NativeLibraryAvailable()) return;
        string caminho = NovoCaminhoTemporario();
        try
        {
            var conn = SqliteConnection.Open(caminho);
            conn.Dispose();
            Assert.Throws<ObjectDisposedException>(() => conn.Execute("CREATE TABLE t (v INTEGER)"),
                what: "usar uma conexão após Dispose deve falhar de forma explícita, não crashar ou silenciosamente reabrir");
        }
        finally { File.Delete(caminho); }
    }

    // Assert.Throws não devolve a exceção capturada (só verifica o tipo) — helper local para os
    // poucos testes que também precisam inspecionar a mensagem.
    private static TException Assert2ThrowsAndReturns<TException>(Action action) where TException : Exception
    {
        try { action(); }
        catch (TException ex) { return ex; }
        throw new AssertException($"esperado {typeof(TException).Name}, nada foi lançado");
    }
}
