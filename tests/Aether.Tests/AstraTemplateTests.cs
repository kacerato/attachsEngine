using Astra.Compilation;

namespace Aether.Tests;

/// <summary>
/// Os modelos de comportamento que o editor oferece são compilados AQUI, com o
/// mesmo compilador que o projeto do usuário usa. Um modelo que não compila
/// seria pior do que não existir: o usuário o criaria e receberia um erro que
/// não é dele.
/// </summary>
public static class AstraTemplateTests
{
    private static string TemplateDirectory()
    {
        // Sobe do binário de teste até a raiz do repositório procurando a pasta.
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "assets", "script-templates");
            if (Directory.Exists(candidate)) return candidate;
            directory = directory.Parent;
        }
        throw new DirectoryNotFoundException("assets/script-templates não encontrado a partir de " + AppContext.BaseDirectory);
    }

    [Test]
    public static void ModelosDeComportamentoCompilamComOSchemaEsperado()
    {
        var result = new ProjectCompiler().Build(TemplateDirectory());
        Assert.True(result.Success, string.Join("\n", result.Diagnostics
            .Where(d => d.Error).Select(d => $"{d.File}({d.Line},{d.Column}) {d.Code}: {d.Message}")));

        var ids = result.Project!.Types.Select(t => t.Id).ToHashSet();
        foreach (var expected in new[]
                 {
                     "project.SeguirAlvo", "project.CameraOrbital", "project.Interacao", "project.Porta",
                     "project.PlataformaMovel", "project.ItemColetavel", "project.Coletor", "project.TrocaMaterial",
                 })
            Assert.True(ids.Contains(expected), "modelo ausente: " + expected);

        // As propriedades autoradas precisam ter um tipo que o Inspector saiba
        // editar; "unsupported" seria um campo visível que ninguém consegue mudar.
        foreach (var type in result.Project.Types)
            foreach (var property in type.Properties)
                Assert.NotEqual("unsupported", property.ValueType, $"{type.Id}.{property.Id}");
    }

    [Test]
    public static void ModeloDePortaExpoeOsCamposQueOInspectorPrecisa()
    {
        var result = new ProjectCompiler().Build(TemplateDirectory());
        Assert.True(result.Success, "modelos compilam");
        var porta = result.Project!.Types.Single(t => t.Id == "project.Porta");
        var campos = porta.Properties.ToDictionary(p => p.Id, p => p.ValueType);
        Assert.Equal("float", campos["angulo_aberto"]);
        Assert.Equal("float", campos["graus_por_segundo"]);
        Assert.Equal("bool", campos["comeca_aberta"]);
        Assert.Equal("bool", campos["tranca"]);
        Assert.Equal("string", campos["rotulo"]);

        // O mesmo tipo em objetos diferentes com valores diferentes é o caso de
        // uso central: o schema é por TIPO, e os valores ficam na instância.
        var camera = result.Project.Types.Single(t => t.Id == "project.CameraOrbital");
        Assert.Equal("object", camera.Properties.Single(p => p.Id == "alvo").ValueType);
        Assert.Equal("float", camera.Properties.Single(p => p.Id == "distancia").ValueType);
    }
}
