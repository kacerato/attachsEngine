using Astra.Compilation;

namespace Aether.Tests;

public static class AstraLanguageTests
{
    private sealed class Project : IDisposable
    {
        public string Root { get; } = Path.Combine(Path.GetTempPath(), "astra-language-" + Guid.NewGuid().ToString("N"));
        public Project() { Directory.CreateDirectory(Root); }
        public void Dispose() { foreach (var file in Directory.GetFiles(Root)) File.Delete(file); Directory.Delete(Root); }
    }
    [Test]
    public static void CompletionUsesUnsavedTypesAndReceiverAccessibility()
    {
        using var project = new Project();
        File.WriteAllText(Path.Combine(project.Root, "Tool.cs"), "public class Tool { public void Old() {} }");
        const string active = "class User { void Run(Tool tool) { tool.Ne } }";
        var result = ProjectLanguage.Query(new(project.Root, "User.cs", active.IndexOf("tool.Ne", StringComparison.Ordinal) + 7, 0, "",
            [new("User.cs", active), new("Tool.cs", "public class Tool { public void NewAction() {} private void Never() {} public static void NewStatic() {} }")]));
        Assert.True(result.Items.Any(i => i.Label == "NewAction"), result.Message);
        Assert.False(result.Items.Any(i => i.Label is "Never" or "NewStatic" or "Old"));
        Assert.Equal(2, result.Items.First(i => i.Label == "NewAction").Length);
        Assert.False(Directory.Exists(Path.Combine(project.Root, ".astra")), "analysis must not publish");
    }
    [Test]
    public static void DefinitionResolvesOtherOpenFileWithUtf16Columns()
    {
        using var project = new Project();
        const string active = "class User { void Run() { var t = new Target(); } }";
        const string target = "// 😀\npublic class Target {}";
        var result = ProjectLanguage.Query(new(project.Root, "User.cs", active.IndexOf("Target", StringComparison.Ordinal) + 2, 1, "",
            [new("User.cs", active), new("Target.cs", target)]));
        Assert.True(result.Items.Any(i => i.File == "Target.cs" && i.Line == 2 && i.Column == 14), result.Message);
    }
    [Test]
    public static void SearchUsesDraftOverDiskAndCancellation()
    {
        using var project = new Project();
        File.WriteAllText(Path.Combine(project.Root, "A.cs"), "class DiskOnly {}");
        var request = new LanguageRequest(project.Root, "A.cs", 0, 2, "draft", [new("A.cs", "// 😀 Draft\nclass Draft {}")]);
        var result = ProjectLanguage.Query(request);
        Assert.Equal(2, result.Items.Length); Assert.Equal(7, result.Items[0].Column);
        using var cancellation = new CancellationTokenSource(); cancellation.Cancel();
        Assert.Throws<OperationCanceledException>(() => ProjectLanguage.Query(request, cancellation.Token));
    }
    [Test]
    public static void DiagnosticKeepsCompiledExcerptAfterFileChanges()
    {
        using var project = new Project();
        string path = Path.Combine(project.Root, "Broken.cs");
        File.WriteAllText(path, "class Broken {\n void Go() { int missing = ; }\n}");
        var result = new ProjectCompiler().Build(project.Root);
        var diagnostic = result.Diagnostics.First(d => d.Error);
        File.WriteAllText(path, "class Fixed {}");
        Assert.True(diagnostic.SourceExcerpt.Contains("int missing = ;", StringComparison.Ordinal));
        Assert.Equal(1, diagnostic.ExcerptLine);
    }
    [Test]
    public static void IncrementalAnalysisDropsRemovedFilesAndReplacesChangedTrees()
    {
        using var project = new Project();
        string path = Path.Combine(project.Root, "Tool.cs");
        const string active = "class User { void Run(Tool tool) { tool. } }";
        LanguageRequest Request() => new(project.Root, "User.cs", active.IndexOf("tool.", StringComparison.Ordinal) + 5, 0, "", [new("User.cs", active)]);
        File.WriteAllText(path, "public class Tool { public void First() {} }");
        Assert.True(ProjectLanguage.Query(Request()).Items.Any(i => i.Label == "First"));
        File.WriteAllText(path, "public class Tool { public void Second() {} }");
        var changed = ProjectLanguage.Query(Request());
        Assert.True(changed.Items.Any(i => i.Label == "Second"));
        Assert.False(changed.Items.Any(i => i.Label == "First"));
        File.Delete(path);
        Assert.False(ProjectLanguage.Query(Request()).Items.Any(i => i.Label == "Second"));
        using var other = new Project();
        File.WriteAllText(Path.Combine(other.Root, "Tool.cs"), "public class Tool { public void OtherProject() {} }");
        var switched = ProjectLanguage.Query(Request() with { Root = other.Root });
        Assert.True(switched.Items.Any(i => i.Label == "OtherProject"));
        Assert.False(ProjectLanguage.Query(Request()).Items.Any(i => i.Label == "OtherProject"));
    }

}
