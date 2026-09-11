using System.Text.RegularExpressions;

namespace Aether.Tests;

/// <summary>
/// A versão do arquivo de cena é escrita pelo C++ e conferida pelo Java do shell.
/// São duas linguagens que nenhum compilador liga, e a divergência não aparece em
/// teste nenhum de host: ela aparece NO APARELHO, como "Formato de cena não
/// reconhecido" ao tentar abrir qualquer projeto salvo pela build nova.
///
/// Foi exatamente o que aconteceu ao subir o arquivo para v12. Este teste lê os
/// dois arquivos e compara, para que a próxima vez falhe aqui.
/// </summary>
public static class ArquivoDeCenaTests
{
    private static DirectoryInfo RepositoryRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "native", "editor", "editor_archive.cpp")))
            directory = directory.Parent;
        Assert.True(directory is not null, "raiz do repositório localizada");
        return directory!;
    }

    [Test]
    public static void VersaoDoEscritorNativoEDoShellJavaNaoPodemDivergir()
    {
        var root = RepositoryRoot();
        var archive = File.ReadAllText(Path.Combine(root.FullName, "native", "editor", "editor_archive.cpp"));
        var shell = File.ReadAllText(Path.Combine(root.FullName, "android", "app", "src", "main", "java",
            "dev", "aether", "editor", "shell", "ProjectSceneSource.java"));

        var written = Regex.Match(archive, @"""AETHER_EDITOR (\d+) """);
        Assert.True(written.Success, "versão emitida pelo escritor nativo encontrada");
        var accepted = Regex.Match(shell, @"LAST_SUPPORTED_ARCHIVE_VERSION\s*=\s*(\d+)");
        Assert.True(accepted.Success, "constante do shell encontrada");

        Assert.Equal(written.Groups[1].Value, accepted.Groups[1].Value,
            "o shell Java precisa aceitar a versão que o escritor nativo emite");

        // O leitor nativo precisa aceitar pelo menos o que ele mesmo escreve.
        var reader = Regex.Match(archive, @"version>(\d+)\)");
        Assert.True(reader.Success, "limite do leitor nativo encontrado");
        Assert.Equal(written.Groups[1].Value, reader.Groups[1].Value,
            "o leitor nativo precisa aceitar a versão que o escritor emite");
    }
}
