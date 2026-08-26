namespace Aether.Platform;

/// <summary>
/// Implementação de <see cref="IFileSystem"/> sobre <see cref="System.IO"/> puro: um diretório raiz
/// real de disco por <see cref="FileSystemScope"/>, recebido no construtor. Serve dois papéis:
/// <list type="bullet">
/// <item>é a implementação de fato usada no editor rodando em desktop de desenvolvimento e nos
/// testes desta suíte, onde "escopo" e "pasta comum de disco" são a mesma coisa;</item>
/// <item>é a base que a implementação Android (item 0.1.4, <c>DotNetHost</c>) vai instanciar
/// passando os caminhos já resolvidos nativamente — <c>internalDataPath</c>/<c>externalDataPath</c>
/// de <c>ANativeActivity</c> (ver <c>native/platform/android/android_paths.h</c>) — como roots de
/// <see cref="FileSystemScope.PersistentData"/>/<see cref="FileSystemScope.Cache"/>. Nenhum código
/// específico de Android precisa existir dentro de <c>Aether.Core</c> por causa disso: a fronteira
/// nativa só transporta os caminhos já resolvidos (bytes UTF-8, blittable — CONVENCOES.md §2), não
/// lógica de scoped storage.
/// </item>
/// </list>
/// </summary>
public sealed class StandardFileSystem : IFileSystem
{
    private readonly Dictionary<FileSystemScope, string> _roots;

    /// <param name="assetsRoot">Raiz somente-leitura de <see cref="FileSystemScope.Assets"/>.</param>
    /// <param name="persistentDataRoot">Raiz de <see cref="FileSystemScope.PersistentData"/>.</param>
    /// <param name="cacheRoot">Raiz de <see cref="FileSystemScope.Cache"/>.</param>
    public StandardFileSystem(string assetsRoot, string persistentDataRoot, string cacheRoot)
    {
        ArgumentException.ThrowIfNullOrEmpty(assetsRoot);
        ArgumentException.ThrowIfNullOrEmpty(persistentDataRoot);
        ArgumentException.ThrowIfNullOrEmpty(cacheRoot);

        _roots = new Dictionary<FileSystemScope, string>
        {
            [FileSystemScope.Assets] = Path.GetFullPath(assetsRoot),
            [FileSystemScope.PersistentData] = Path.GetFullPath(persistentDataRoot),
            [FileSystemScope.Cache] = Path.GetFullPath(cacheRoot),
        };
    }

    public bool ScopeExists(FileSystemScope scope) => Directory.Exists(RootOf(scope));

    public void EnsureScopeExists(FileSystemScope scope)
    {
        if (scope == FileSystemScope.Assets)
            throw new InvalidOperationException("FileSystemScope.Assets é somente leitura — não faz sentido garantir sua existência a partir do runtime.");
        Directory.CreateDirectory(RootOf(scope));
    }

    public bool FileExists(FileSystemScope scope, string relativePath) => File.Exists(ResolvePath(scope, relativePath));

    public bool DirectoryExists(FileSystemScope scope, string relativePath) => Directory.Exists(ResolvePath(scope, relativePath));

    public byte[] ReadAllBytes(FileSystemScope scope, string relativePath) => File.ReadAllBytes(ResolvePath(scope, relativePath));

    public void WriteAllBytes(FileSystemScope scope, string relativePath, ReadOnlySpan<byte> data)
    {
        ThrowIfReadOnlyScope(scope);
        string fullPath = ResolvePath(scope, relativePath);
        string? directory = Path.GetDirectoryName(fullPath);
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
        File.WriteAllBytes(fullPath, data.ToArray());
    }

    public Stream OpenRead(FileSystemScope scope, string relativePath) => File.OpenRead(ResolvePath(scope, relativePath));

    public Stream OpenWrite(FileSystemScope scope, string relativePath)
    {
        ThrowIfReadOnlyScope(scope);
        string fullPath = ResolvePath(scope, relativePath);
        string? directory = Path.GetDirectoryName(fullPath);
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
        return File.Create(fullPath);
    }

    public void DeleteFile(FileSystemScope scope, string relativePath)
    {
        ThrowIfReadOnlyScope(scope);
        File.Delete(ResolvePath(scope, relativePath));
    }

    public IEnumerable<string> EnumerateFiles(FileSystemScope scope, string relativePath)
    {
        string fullPath = ResolvePath(scope, relativePath);
        if (!Directory.Exists(fullPath)) yield break;

        string root = RootOf(scope);
        foreach (string entry in Directory.EnumerateFiles(fullPath))
        {
            string relative = Path.GetRelativePath(root, entry).Replace(Path.DirectorySeparatorChar, '/');
            yield return relative;
        }
    }

    public string ResolvePath(FileSystemScope scope, string relativePath)
    {
        ArgumentNullException.ThrowIfNull(relativePath);
        string root = RootOf(scope);
        if (relativePath.Length == 0) return root;

        string normalized = relativePath.Replace('/', Path.DirectorySeparatorChar);
        if (Path.IsPathRooted(normalized))
            throw new ArgumentException($"Caminho relativo não pode ser absoluto: '{relativePath}'.", nameof(relativePath));

        string combined = Path.GetFullPath(Path.Combine(root, normalized));
        // Barreira contra ".." escapando do escopo: o caminho resolvido tem que continuar dentro da
        // raiz do escopo. Comparação por prefixo de diretório (não apenas StartsWith de string) para
        // não confundir "root-evil" com "root" quando os dois compartilham prefixo textual.
        if (!IsWithinRoot(combined, root))
            throw new ArgumentException($"Caminho relativo escapa do escopo '{scope}': '{relativePath}'.", nameof(relativePath));

        return combined;
    }

    private static bool IsWithinRoot(string fullPath, string root)
    {
        string rootWithSeparator = root.EndsWith(Path.DirectorySeparatorChar) ? root : root + Path.DirectorySeparatorChar;
        return fullPath.Equals(root, StringComparison.Ordinal) ||
               fullPath.StartsWith(rootWithSeparator, StringComparison.Ordinal);
    }

    private void ThrowIfReadOnlyScope(FileSystemScope scope)
    {
        if (scope == FileSystemScope.Assets)
            throw new InvalidOperationException("FileSystemScope.Assets é somente leitura em runtime — grave em PersistentData ou Cache.");
    }

    private string RootOf(FileSystemScope scope) => _roots[scope];
}
