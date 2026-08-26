namespace Aether.Platform;

/// <summary>
/// Item 1.1.1 do plano: abstração de sistema de arquivos por escopo nomeado (<see
/// cref="FileSystemScope"/>), em vez de caminho absoluto livre. É a fronteira que impede o resto da
/// engine (WAL de recuperação, <c>ConfigurationStore</c>, futuro formato de projeto do item 7.4) de
/// conhecer scoped storage do Android ou sandbox do iOS — cada implementação resolve um caminho
/// relativo dentro de um escopo para o caminho físico real daquela plataforma.
/// <para>
/// Todo caminho relativo usado nesta interface usa <c>/</c> como separador, independente de SO —
/// implementações traduzem para o separador nativo internamente. Caminhos absolutos, <c>..</c> e
/// drive letters não são aceitos (ver <see cref="StandardFileSystem"/>): um caminho relativo que
/// tenta escapar do escopo é um bug de chamador, não uma forma válida de alcançar outro escopo.
/// </para>
/// </summary>
public interface IFileSystem
{
    /// <summary>
    /// Verdadeiro se o escopo existe fisicamente nesta plataforma. <see cref="FileSystemScope.Assets"/>
    /// sempre existe (é onde o app foi instalado); os demais podem não existir ainda na primeira
    /// execução — chamadores devem usar <see cref="EnsureScopeExists"/> antes de escrever.
    /// </summary>
    bool ScopeExists(FileSystemScope scope);

    /// <summary>
    /// Cria o diretório raiz do escopo se ainda não existir. É um erro chamar isto para <see
    /// cref="FileSystemScope.Assets"/> (somente leitura por definição) — lança <see
    /// cref="InvalidOperationException"/>.
    /// </summary>
    void EnsureScopeExists(FileSystemScope scope);

    bool FileExists(FileSystemScope scope, string relativePath);

    bool DirectoryExists(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Lê o arquivo inteiro para um array novo. Lança <see cref="FileNotFoundException"/> se não
    /// existir — chamador que trata ausência como caso normal deve checar <see cref="FileExists"/>
    /// antes, mesma disciplina do resto do BCL.
    /// </summary>
    byte[] ReadAllBytes(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Grava o arquivo por inteiro, sobrescrevendo se já existir. Cria diretórios intermediários
    /// automaticamente (mas não o escopo raiz — ver <see cref="EnsureScopeExists"/>). Lança <see
    /// cref="InvalidOperationException"/> se <paramref name="scope"/> for <see
    /// cref="FileSystemScope.Assets"/>: esse escopo é somente leitura em runtime por contrato, uma
    /// escrita silenciosamente ignorada seria pior que uma exceção explícita.
    /// </summary>
    void WriteAllBytes(FileSystemScope scope, string relativePath, ReadOnlySpan<byte> data);

    /// <summary>
    /// Abre um stream de leitura. Quem chama é responsável por descartar o stream — mesmo contrato de
    /// <see cref="File.OpenRead(string)"/>. Preferir <see cref="ReadAllBytes"/> quando o arquivo é
    /// pequeno e cabe inteiro em memória (a maioria dos casos nesta engine); este método existe para
    /// os casos que precisam de leitura incremental (ex.: um formato binário grande demais para
    /// carregar de uma vez).
    /// </summary>
    Stream OpenRead(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Abre um stream de escrita, criando ou truncando o arquivo. Mesma restrição de <see
    /// cref="FileSystemScope.Assets"/> ser somente leitura que <see cref="WriteAllBytes"/>.
    /// </summary>
    Stream OpenWrite(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Remove o arquivo. Não-erro se já não existir (idempotente, mesmo padrão de <see
    /// cref="File.Delete(string)"/> do BCL).
    /// </summary>
    void DeleteFile(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Lista os caminhos relativos dos arquivos diretamente dentro de <paramref name="relativePath"/>
    /// (não recursivo — chamador que precisa de recursão itera os subdiretórios devolvidos junto,
    /// identificáveis via <see cref="DirectoryExists"/>). Devolve sequência vazia se o diretório não
    /// existir, nunca lança por ausência.
    /// </summary>
    IEnumerable<string> EnumerateFiles(FileSystemScope scope, string relativePath);

    /// <summary>
    /// Resolve o caminho relativo para o caminho físico absoluto real desta plataforma. Existe para
    /// interoperar com APIs de terceiros que só aceitam caminho de disco (ex.: abrir uma lib nativa
    /// vendorizada) — todo código novo dentro desta engine deve preferir os métodos acima, que não
    /// vazam a representação física do escopo.
    /// </summary>
    string ResolvePath(FileSystemScope scope, string relativePath);
}
