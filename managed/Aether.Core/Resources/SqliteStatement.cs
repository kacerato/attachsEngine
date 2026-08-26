using System.Text;

namespace Aether.Resources;

/// <summary>Mesmo conjunto de <c>AetherSqliteColumnType</c> — tipo dinâmico de uma coluna lida,
/// que o SQLite decide por valor armazenado, não pelo tipo declarado da coluna (typing dinâmico é
/// uma característica central do SQLite, não uma limitação deste binding).</summary>
public enum SqliteColumnType
{
    Integer,
    Float,
    Text,
    Blob,
    Null,
}

/// <summary>
/// Um statement SQL preparado (item 1.4.5 do plano) — DDL, DML ou SELECT. Obtido via <see
/// cref="SqliteConnection.Prepare"/>, nunca construído diretamente. Bind é 1-based, leitura de
/// coluna é 0-based — mesma convenção do SQLite nativo, preservada aqui para não confundir quem já
/// conhece a API C do SQLite.
/// </summary>
public sealed unsafe class SqliteStatement : IDisposable
{
    private nint _handle;
    private bool _disposed;

    internal SqliteStatement(nint handle) => _handle = handle;

    private nint Handle => _disposed ? throw new ObjectDisposedException(nameof(SqliteStatement)) : _handle;

    public void BindInt64(int index, long value)
    {
        var result = NativeSqlite.AetherSqlite_BindInt64(Handle, index, value);
        ThrowIfNotOk(result, "BindInt64");
    }

    public void BindDouble(int index, double value)
    {
        var result = NativeSqlite.AetherSqlite_BindDouble(Handle, index, value);
        ThrowIfNotOk(result, "BindDouble");
    }

    public void BindText(int index, string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        byte[] utf8 = Encoding.UTF8.GetBytes(value);
        NativeSqliteResult result;
        fixed (byte* ptr = utf8)
        {
            result = NativeSqlite.AetherSqlite_BindText(Handle, index, ptr, utf8.Length);
        }
        ThrowIfNotOk(result, "BindText");
    }

    public void BindNull(int index)
    {
        var result = NativeSqlite.AetherSqlite_BindNull(Handle, index);
        ThrowIfNotOk(result, "BindNull");
    }

    /// <summary>Avança uma linha. <see cref="SqliteStepResult.Row"/> quando há dados prontos para
    /// ler via <c>Column*</c>; <see cref="SqliteStepResult.Done"/> quando a execução esgotou.
    /// Qualquer outro resultado nativo (erro, banco ocupado, uso indevido) vira <see
    /// cref="SqliteException"/>.</summary>
    public SqliteStepResult Step()
    {
        var result = NativeSqlite.AetherSqlite_Step(Handle);
        return result switch
        {
            NativeSqliteResult.Row => SqliteStepResult.Row,
            NativeSqliteResult.Done => SqliteStepResult.Done,
            NativeSqliteResult.Busy => throw new SqliteException("banco ocupado por outra conexão/transação (SQLITE_BUSY)"),
            NativeSqliteResult.Misuse => throw new SqliteException("Step chamado fora de ordem — statement já finalizado ou em estado inválido (SQLITE_MISUSE)"),
            _ => throw new SqliteException("Step falhou — SQL inválido em tempo de execução (tipo/restrição) ou outro erro do SQLite"),
        };
    }

    /// <summary>Reinicia o statement para nova execução, limpando bindings anteriores — evita
    /// re-preparar o mesmo SQL em loop (ex.: mesmo INSERT com valores diferentes).</summary>
    public void Reset()
    {
        var result = NativeSqlite.AetherSqlite_Reset(Handle);
        ThrowIfNotOk(result, "Reset");
    }

    public SqliteColumnType ColumnType(int columnIndex) => NativeSqlite.AetherSqlite_ColumnType(Handle, columnIndex) switch
    {
        NativeSqliteColumnType.Integer => SqliteColumnType.Integer,
        NativeSqliteColumnType.Float => SqliteColumnType.Float,
        NativeSqliteColumnType.Text => SqliteColumnType.Text,
        NativeSqliteColumnType.Blob => SqliteColumnType.Blob,
        _ => SqliteColumnType.Null,
    };

    public long ColumnInt64(int columnIndex) => NativeSqlite.AetherSqlite_ColumnInt64(Handle, columnIndex);

    public double ColumnDouble(int columnIndex) => NativeSqlite.AetherSqlite_ColumnDouble(Handle, columnIndex);

    public string ColumnText(int columnIndex)
    {
        byte* textPtr;
        int length;
        NativeSqlite.AetherSqlite_ColumnText(Handle, columnIndex, &textPtr, &length);
        if (length == 0 || textPtr == null) return string.Empty;
        return Encoding.UTF8.GetString(textPtr, length);
    }

    private static void ThrowIfNotOk(NativeSqliteResult result, string operationName)
    {
        if (result != NativeSqliteResult.Ok)
            throw new SqliteException($"{operationName} falhou ({result})");
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        if (_handle != nint.Zero)
        {
            NativeSqlite.AetherSqlite_Finalize(_handle);
            _handle = nint.Zero;
        }
    }
}
