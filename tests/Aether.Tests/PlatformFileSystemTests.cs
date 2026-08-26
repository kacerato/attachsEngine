using System.Text;
using Aether.Platform;

namespace Aether.Tests;

/// <summary>
/// Item 1.1.1 do plano: abstração de sistema de arquivos por escopo. Cobre a implementação
/// portátil <see cref="StandardFileSystem"/> — a mesma usada no editor em desktop e nos testes —
/// incluindo os casos degenerados de resolução de caminho (escape via "..", caminho absoluto,
/// escrita no escopo somente-leitura).
/// </summary>
public static class PlatformFileSystemTests
{
    /// <summary>Cria três diretórios temporários isolados (um por escopo) e devolve o
    /// <see cref="StandardFileSystem"/> apontando para eles, junto com a raiz para limpeza.</summary>
    private static (StandardFileSystem fs, string root) NovoFileSystemTemporario()
    {
        string root = Path.Combine(Path.GetTempPath(), $"aether-fs-{Guid.NewGuid():N}");
        string assets = Path.Combine(root, "assets");
        string persistent = Path.Combine(root, "persistent");
        string cache = Path.Combine(root, "cache");
        Directory.CreateDirectory(assets);
        // persistent/cache deliberadamente NÃO criados aqui: alguns testes verificam o comportamento
        // antes de EnsureScopeExists.
        return (new StandardFileSystem(assets, persistent, cache), root);
    }

    [Test] public static void ScopeExists_AssetsRecemCriado_EhVerdadeiro()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try { Assert.True(fs.ScopeExists(FileSystemScope.Assets), what: "diretório de assets foi criado no setup"); }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void ScopeExists_PersistentDataAntesDeEnsure_EhFalso()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try { Assert.False(fs.ScopeExists(FileSystemScope.PersistentData), what: "diretório ainda não foi criado nem garantido"); }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EnsureScopeExists_PersistentData_CriaDiretorio()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            Assert.True(fs.ScopeExists(FileSystemScope.PersistentData), what: "EnsureScopeExists deve criar o diretório raiz");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EnsureScopeExists_ChamadoParaAssets_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            Assert.Throws<InvalidOperationException>(() => fs.EnsureScopeExists(FileSystemScope.Assets),
                what: "Assets é somente leitura — garantir sua existência a partir do runtime não faz sentido");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void WriteAllBytes_DepoisReadAllBytes_RoundTripPreservaConteudo()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            byte[] original = Encoding.UTF8.GetBytes("conteúdo de teste com acentuação");
            fs.WriteAllBytes(FileSystemScope.PersistentData, "projeto/dados.bin", original);

            byte[] lido = fs.ReadAllBytes(FileSystemScope.PersistentData, "projeto/dados.bin");
            Assert.True(original.AsSpan().SequenceEqual(lido), what: "round-trip de escrita/leitura deve preservar bytes exatamente");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void WriteAllBytes_CriaDiretoriosIntermediariosAutomaticamente()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "a/b/c/arquivo.txt", "x"u8);
            Assert.True(fs.FileExists(FileSystemScope.PersistentData, "a/b/c/arquivo.txt"), what: "diretórios intermediários devem ser criados sob demanda");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void WriteAllBytes_NoEscopoAssets_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            Assert.Throws<InvalidOperationException>(() => fs.WriteAllBytes(FileSystemScope.Assets, "x.txt", "y"u8),
                what: "Assets é somente leitura por contrato — uma escrita silenciosamente ignorada seria pior que a exceção");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void DeleteFile_NoEscopoAssets_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            Assert.Throws<InvalidOperationException>(() => fs.DeleteFile(FileSystemScope.Assets, "x.txt"),
                what: "delete é uma escrita — mesma restrição de somente-leitura de Assets");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void ReadAllBytes_ArquivoInexistente_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            Assert.Throws<FileNotFoundException>(() => fs.ReadAllBytes(FileSystemScope.PersistentData, "nao-existe.bin"),
                what: "ausência deve ser um erro explícito, mesma disciplina de File.ReadAllBytes do BCL");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void DeleteFile_ArquivoInexistente_EhNoOp()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.DeleteFile(FileSystemScope.PersistentData, "nunca-existiu.bin"); // não deve lançar
            Assert.True(true, what: "delete de arquivo inexistente é idempotente, não é erro");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void ResolvePath_TentandoEscaparComPontoPonto_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            Assert.Throws<ArgumentException>(() => fs.ResolvePath(FileSystemScope.PersistentData, "../../etc/passwd"),
                what: "\"..\" não pode escapar da raiz do escopo — isso vazaria para fora do sandbox da plataforma real");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void ResolvePath_CaminhoAbsoluto_Lanca()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            string absoluto = OperatingSystem.IsWindows() ? "C:/outro/lugar" : "/etc/passwd";
            Assert.Throws<ArgumentException>(() => fs.ResolvePath(FileSystemScope.PersistentData, absoluto),
                what: "caminho relativo não pode ser absoluto — só existe um jeito de nomear um arquivo dentro do escopo");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void ResolvePath_CaminhoVazio_DevolveARaizDoEscopo()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            string resolvido = fs.ResolvePath(FileSystemScope.Assets, "");
            Assert.Equal(Path.GetFullPath(Path.Combine(root, "assets")), resolvido, what: "caminho relativo vazio deve resolver para a própria raiz do escopo");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EnumerateFiles_DiretorioInexistente_DevolveSequenciaVazia()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            var arquivos = fs.EnumerateFiles(FileSystemScope.PersistentData, "nao-existe").ToList();
            Assert.Equal(0, arquivos.Count, what: "diretório ausente não é erro, apenas nenhum arquivo");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EnumerateFiles_ComArquivos_DevolveCaminhosRelativosComBarra()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "cena/nivel1.ascene", "a"u8);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "cena/nivel2.ascene", "b"u8);

            var arquivos = fs.EnumerateFiles(FileSystemScope.PersistentData, "cena").OrderBy(x => x).ToList();
            Assert.Equal(2, arquivos.Count, what: "os dois arquivos gravados devem aparecer na enumeração");
            Assert.Equal("cena/nivel1.ascene", arquivos[0], what: "caminho relativo deve usar barra normal, independente de SO");
            Assert.Equal("cena/nivel2.ascene", arquivos[1], what: "caminho relativo deve usar barra normal, independente de SO");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EnumerateFiles_NaoEhRecursivo()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "raiz.txt", "a"u8);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "sub/aninhado.txt", "b"u8);

            var arquivos = fs.EnumerateFiles(FileSystemScope.PersistentData, "").ToList();
            Assert.Equal(1, arquivos.Count, what: "arquivo dentro de subdiretório não deve aparecer na listagem não-recursiva da raiz");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void OpenWrite_DepoisOpenRead_RoundTripPreservaConteudo()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.Cache);
            byte[] original = [1, 2, 3, 4, 5, 255, 0];
            using (var escrita = fs.OpenWrite(FileSystemScope.Cache, "blob.bin"))
                escrita.Write(original);

            using var leitura = fs.OpenRead(FileSystemScope.Cache, "blob.bin");
            using var buffer = new MemoryStream();
            leitura.CopyTo(buffer);
            Assert.True(original.AsSpan().SequenceEqual(buffer.ToArray()), what: "round-trip via streams deve preservar bytes exatamente");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void DirectoryExists_AposEscritaDeArquivoDentro_EhVerdadeiroParaOSubdiretorio()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "materiais/pedra.amat", "x"u8);
            Assert.True(fs.DirectoryExists(FileSystemScope.PersistentData, "materiais"), what: "diretório intermediário criado pela escrita deve ser visível");
        }
        finally { Directory.Delete(root, recursive: true); }
    }

    [Test] public static void EscoposDiferentes_SaoIsoladosEntreSi()
    {
        var (fs, root) = NovoFileSystemTemporario();
        try
        {
            fs.EnsureScopeExists(FileSystemScope.PersistentData);
            fs.EnsureScopeExists(FileSystemScope.Cache);
            fs.WriteAllBytes(FileSystemScope.PersistentData, "mesmo-nome.dat", "persistente"u8);
            fs.WriteAllBytes(FileSystemScope.Cache, "mesmo-nome.dat", "cache"u8);

            byte[] doPersistente = fs.ReadAllBytes(FileSystemScope.PersistentData, "mesmo-nome.dat");
            byte[] doCache = fs.ReadAllBytes(FileSystemScope.Cache, "mesmo-nome.dat");
            Assert.False(doPersistente.AsSpan().SequenceEqual(doCache), what: "mesmo caminho relativo em escopos diferentes deve resolver para arquivos físicos distintos");
        }
        finally { Directory.Delete(root, recursive: true); }
    }
}
