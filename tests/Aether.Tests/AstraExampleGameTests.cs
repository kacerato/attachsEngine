using Astra.Compilation;

namespace Aether.Tests;

public static class AstraExampleGameTests
{
    private static string ExamplesDirectory()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "android", "app", "src", "main", "assets",
                                         "astra", "example-projects");
            if (Directory.Exists(candidate)) return candidate;
            directory = directory.Parent;
        }
        throw new DirectoryNotFoundException("example-projects não encontrado a partir de " + AppContext.BaseDirectory);
    }

    [Test]
    public static void JogosDeExemploCompilamComNpcEInteracaoFisica()
    {
        var expected = new Dictionary<string, string[]>
        {
            ["quarentena"] = ["project.QuarantinePlayer", "project.MaintenanceDrone"],
            ["resgate"] = ["project.RescuePlayer", "project.SurvivorAgent"],
            ["perimetro"] = ["project.TacticalPlayer", "project.SentinelAgent", "project.ThrowableCrate"],
            ["mercado-nexus"] = ["project.MarketRunner", "project.VendorAgent", "project.CourierGuide"],
            ["farol-abissal"] = ["project.AbyssDiver", "project.AbyssStalker", "project.BeaconDrone"],
            ["expresso-tita"] = ["project.TitanEngineer", "project.RaiderAgent", "project.ConductorAgent",
                                  "project.ThrownCargo"],
        };
        var root = ExamplesDirectory();
        foreach (var (game, types) in expected)
        {
            var result = new ProjectCompiler().Build(Path.Combine(root, game));
            Assert.True(result.Success, game + ": " + string.Join("\n", result.Diagnostics
                .Where(d => d.Error).Select(d => $"{d.File}({d.Line},{d.Column}) {d.Code}: {d.Message}")));
            var ids = result.Project!.Types.Select(t => t.Id).ToHashSet(StringComparer.Ordinal);
            foreach (var type in types) Assert.True(ids.Contains(type), game + " não publicou " + type);
        }
    }
}
