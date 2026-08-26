using System.Runtime.InteropServices;

namespace Aether.Resources;

/// <summary>Mesmo layout de <c>AetherSqliteResult</c> em <c>native/resources/sqlite_bridge.h</c>.</summary>
internal enum NativeSqliteResult : int
{
    Ok = 0,
    Error = 1,
    CantOpen = 2,
    Busy = 3,
    Misuse = 4,
    Row = 5,
    Done = 6,
}

/// <summary>Mesmo layout de <c>AetherSqliteColumnType</c>.</summary>
internal enum NativeSqliteColumnType : int
{
    Integer = 1,
    Float = 2,
    Text = 3,
    Blob = 4,
    Null = 5,
}

/// <summary>
/// Bindings P/Invoke cruas sobre <c>native/resources/sqlite_bridge.h</c> (item 1.4.5). Espelha a
/// fronteira C ABI 1:1 — nenhuma lógica aqui, mesma disciplina de <c>NativePhysics</c>. Não use
/// esta classe diretamente fora de <see cref="Aether.Resources"/>; <see cref="SqliteConnection"/> e
/// <see cref="SqliteStatement"/> são a API que o resto da engine deve chamar.
/// <para>
/// Texto (caminho, SQL, bind de parâmetro texto, leitura de coluna texto) cruza a fronteira como
/// <c>byte*</c> UTF-8 + comprimento explícito, nunca <c>string</c> — CONVENCOES.md §2 ("nada de
/// string, nada de marshalling") e mesma convenção documentada em sqlite_bridge.h.
/// </para>
/// </summary>
internal static partial class NativeSqlite
{
    private const string LibraryName = "aether_resources";

    [LibraryImport(LibraryName)]
    internal static unsafe partial nint AetherSqlite_Open(byte* utf8Path, int pathLength);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_Close(nint connection);

    [LibraryImport(LibraryName)]
    internal static unsafe partial void AetherSqlite_GetLastError(nint connection, byte** outUtf8Message, int *outLength);

    [LibraryImport(LibraryName)]
    internal static unsafe partial nint AetherSqlite_Prepare(nint connection, byte* utf8Sql, int sqlLength);

    [LibraryImport(LibraryName)]
    internal static partial void AetherSqlite_Finalize(nint statement);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_Step(nint statement);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_Reset(nint statement);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_BindInt64(nint statement, int index, long value);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_BindDouble(nint statement, int index, double value);

    [LibraryImport(LibraryName)]
    internal static unsafe partial NativeSqliteResult AetherSqlite_BindText(nint statement, int index, byte* utf8Value, int length);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteResult AetherSqlite_BindNull(nint statement, int index);

    [LibraryImport(LibraryName)]
    internal static partial NativeSqliteColumnType AetherSqlite_ColumnType(nint statement, int columnIndex);

    [LibraryImport(LibraryName)]
    internal static partial long AetherSqlite_ColumnInt64(nint statement, int columnIndex);

    [LibraryImport(LibraryName)]
    internal static partial double AetherSqlite_ColumnDouble(nint statement, int columnIndex);

    [LibraryImport(LibraryName)]
    internal static unsafe partial void AetherSqlite_ColumnText(nint statement, int columnIndex, byte** outUtf8Text, int *outLength);
}
