using System.Text;

namespace Aether.Resources;

/// <summary>
/// Erro devolvido pelo SQLite nativo que não é fluxo normal (<see cref="SqliteStepResult"/> cobre
/// os dois casos de sucesso de <c>Step</c> — linha disponível ou execução esgotada — separadamente).
/// </summary>
public sealed class SqliteException : Exception
{
    public SqliteException(string message) : base(message) { }
}

/// <summary>Resultado de <see cref="SqliteStatement.Step"/> — mesma forma de <c>AetherSqliteResult</c>
/// restrita aos dois valores que representam sucesso, não erro (qualquer outro valor vira
/// <see cref="SqliteException"/> antes de chegar ao chamador).</summary>
public enum SqliteStepResult
{
    /// <summary>Há uma linha disponível — ler colunas via <see cref="SqliteStatement"/> é seguro agora.</summary>
    Row,
    /// <summary>Execução terminou sem (mais) linhas — para um SELECT, resultado esgotado; para
    /// DDL/DML, execução completa.</summary>
    Done,
}

/// <summary>
/// Conexão a um banco SQLite (item 1.4.5 do plano). Wrapper fino sobre <see cref="NativeSqlite"/> —
/// converte <c>string</c> gerenciada para UTF-8 na fronteira (a ABI nativa só aceita bytes
/// explícitos, CONVENCOES.md §2) e traduz <see cref="NativeSqliteResult"/> de erro em <see
/// cref="SqliteException"/> com a mensagem real do SQLite, em vez do chamador precisar checar um
/// código de retorno manualmente toda vez.
/// <para>
/// Deliberadamente NÃO inclui nenhum schema de domínio (índice de dependências, tabelas de asset) —
/// isso é trabalho da Fase 7 quando existir consumidor real. Esta classe é infraestrutura genérica
/// de acesso a um banco SQLite, reutilizável por qualquer schema futuro.
/// </para>
/// </summary>
public sealed unsafe class SqliteConnection : IDisposable
{
    private nint _handle;
    private bool _disposed;

    private SqliteConnection(nint handle) => _handle = handle;

    /// <summary>Abre (ou cria, se não existir) o banco em <paramref name="path"/> — caminho físico
    /// já resolvido pelo chamador (ex.: via <c>Aether.Platform.IFileSystem.ResolvePath</c>, item
    /// 1.1.1; este tipo não sabe nada de <c>FileSystemScope</c>). Lança <see cref="SqliteException"/>
    /// se o arquivo/diretório não for acessível.</summary>
    public static SqliteConnection Open(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        byte[] utf8Path = Encoding.UTF8.GetBytes(path);
        nint handle;
        fixed (byte* ptr = utf8Path)
        {
            handle = NativeSqlite.AetherSqlite_Open(ptr, utf8Path.Length);
        }
        if (handle == nint.Zero)
            throw new SqliteException($"não foi possível abrir o banco em '{path}' — diretório inacessível, permissão negada, ou disco cheio");
        return new SqliteConnection(handle);
    }

    internal nint Handle => _disposed ? throw new ObjectDisposedException(nameof(SqliteConnection)) : _handle;

    /// <summary>Prepara <paramref name="sql"/> para execução. Lança <see cref="SqliteException"/>
    /// com a mensagem de erro real do SQLite se a sintaxe for inválida.</summary>
    public SqliteStatement Prepare(string sql)
    {
        ArgumentException.ThrowIfNullOrEmpty(sql);
        byte[] utf8Sql = Encoding.UTF8.GetBytes(sql);
        nint stmtHandle;
        fixed (byte* ptr = utf8Sql)
        {
            stmtHandle = NativeSqlite.AetherSqlite_Prepare(Handle, ptr, utf8Sql.Length);
        }
        if (stmtHandle == nint.Zero) throw new SqliteException(GetLastError());
        return new SqliteStatement(stmtHandle);
    }

    /// <summary>Conveniência para SQL sem parâmetro e sem resultado esperado (DDL típico como
    /// <c>CREATE TABLE</c>) — prepara, executa um <see cref="SqliteStatement.Step"/> e finaliza.
    /// Para SQL com parâmetros ou que produz linhas, usar <see cref="Prepare"/> diretamente.</summary>
    public void Execute(string sql)
    {
        using var statement = Prepare(sql);
        statement.Step();
    }

    private string GetLastError()
    {
        byte* messagePtr;
        int length;
        NativeSqlite.AetherSqlite_GetLastError(Handle, &messagePtr, &length);
        if (length == 0 || messagePtr == null) return "erro SQLite sem mensagem disponível";
        return Encoding.UTF8.GetString(messagePtr, length);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        if (_handle != nint.Zero)
        {
            // Close pode falhar com Busy se um SqliteStatement desta conexão não foi finalizado —
            // propositalmente NÃO lançamos dentro de Dispose (CONVENCOES.md item 8, regra 2: nunca
            // travar/quebrar por descuido do chamador em cascata); o statement vazado já é um bug do
            // chamador (deveria ter Disposed o SqliteStatement primeiro), reportar aqui só
            // mascararia esse bug atrás de uma exceção de Dispose confusa.
            NativeSqlite.AetherSqlite_Close(_handle);
            _handle = nint.Zero;
        }
    }
}
