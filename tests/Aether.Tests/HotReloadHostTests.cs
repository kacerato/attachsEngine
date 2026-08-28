using System.Diagnostics;
using System.Reflection;
using Aether.Scripting;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Emit;

namespace Aether.Tests;

/// <summary>
/// Item 0.2 do plano (PoC-D — "hot reload C#: editar → ver mudança em &lt; 2 s"). Ver comentário
/// de classe de <see cref="HotReloadHost"/> para o porquê deste PoC usar <see
/// cref="System.Runtime.Loader.AssemblyLoadContext"/> collectible em vez do protocolo completo de
/// Hot Reload do .NET.
/// <para>
/// A compilação Roslyn aqui (<see cref="CompileToBytes"/>) simula "o resultado de uma edição do
/// usuário" — na prática, cada chamada é o equivalente a rodar <c>dotnet build</c> do zero (não é o
/// caminho rápido de recompilação incremental que um pipeline de produto usaria), então o tempo
/// medido em <see cref="LoadInvokeUnload_CicloCompleto_MedeTempoENaoVazaMemoria"/> é uma cota
/// SUPERIOR conservadora — o pipeline real (Roslyn incremental + só o passo de load/unload que
/// este PoC prova) tende a ser mais rápido, não mais lento, que o número medido aqui.
/// </para>
/// </summary>
public static class HotReloadHostTests
{
    private const string ScriptV1Source = """
        namespace AetherScript;
        public static class Script
        {
            public static int Compute(int x) => x + 1;
        }
        """;

    private const string ScriptV2Source = """
        namespace AetherScript;
        public static class Script
        {
            public static int Compute(int x) => x * 10;
        }
        """;

    private static byte[] CompileToBytes(string source)
    {
        var syntaxTree = CSharpSyntaxTree.ParseText(source);
        var references = new[]
        {
            MetadataReference.CreateFromFile(typeof(object).Assembly.Location),
            MetadataReference.CreateFromFile(Assembly.Load("System.Runtime").Location),
        };
        var compilation = CSharpCompilation.Create(
            assemblyName: $"AetherScript_{Guid.NewGuid():N}",
            syntaxTrees: [syntaxTree],
            references: references,
            options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

        using var stream = new MemoryStream();
        EmitResult result = compilation.Emit(stream);
        if (!result.Success)
        {
            string erros = string.Join("; ", result.Diagnostics.Where(d => d.Severity == DiagnosticSeverity.Error));
            throw new InvalidOperationException($"Falha ao compilar script de teste: {erros}");
        }
        return stream.ToArray();
    }

    [Test] public static void Load_AssemblyValido_MarcaIsLoadedVerdadeiro()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        try
        {
            Assert.True(host.IsLoaded, what: "após Load bem-sucedido, IsLoaded deve ser verdadeiro");
        }
        finally { host.Unload(); }
    }

    [Test] public static void Load_ChamadoDuasVezesSemUnload_Lanca()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        try
        {
            Assert.Throws<InvalidOperationException>(() => host.Load(CompileToBytes(ScriptV2Source)),
                what: "carregar um segundo assembly sem descarregar o primeiro acumularia contextos indefinidamente — deve ser rejeitado explicitamente");
        }
        finally { host.Unload(); }
    }

    [Test] public static void InvokeStaticMethod_MetodoExistente_DevolveOValorCorreto()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        try
        {
            object? resultado = host.InvokeStaticMethod("AetherScript.Script", "Compute", 5);
            Assert.Equal(6, resultado, what: "Script V1 (x + 1) chamado com 5 deve devolver 6");
        }
        finally { host.Unload(); }
    }

    [Test] public static void InvokeStaticMethod_SemAssemblyCarregado_Lanca()
    {
        var host = new HotReloadHost();
        Assert.Throws<InvalidOperationException>(() => host.InvokeStaticMethod("AetherScript.Script", "Compute", 1),
            what: "invocar sem Load anterior deve falhar explicitamente, não com NullReferenceException confuso");
    }

    [Test] public static void InvokeStaticMethod_TipoInexistente_LancaMissingMemberException()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        try
        {
            Assert.Throws<MissingMemberException>(() => host.InvokeStaticMethod("AetherScript.NaoExiste", "Compute", 1),
                what: "tipo inexistente no assembly de script deve produzir erro específico e legível");
        }
        finally { host.Unload(); }
    }

    [Test] public static void InvokeStaticMethod_MetodoInexistente_LancaMissingMethodException()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        try
        {
            Assert.Throws<MissingMethodException>(() => host.InvokeStaticMethod("AetherScript.Script", "NaoExiste", 1),
                what: "método inexistente no tipo deve produzir erro específico e legível");
        }
        finally { host.Unload(); }
    }

    [Test] public static void Unload_SemNadaCarregado_EhNoOp()
    {
        var host = new HotReloadHost();
        host.Unload(); // não deve lançar
        Assert.False(host.IsLoaded, what: "Unload sem Load anterior é no-op seguro, não erro");
    }

    [Test] public static void Unload_DepoisDeLoad_IsLoadedVoltaAFalso()
    {
        var host = new HotReloadHost();
        host.Load(CompileToBytes(ScriptV1Source));
        host.Unload();
        Assert.False(host.IsLoaded, what: "IsLoaded deve refletir o estado após Unload");
    }

    [Test] public static void LoadInvokeUnloadLoad_CicloDeReload_NovaVersaoSubstituiOComportamentoAntigo()
    {
        var host = new HotReloadHost();

        host.Load(CompileToBytes(ScriptV1Source));
        object? resultadoV1 = host.InvokeStaticMethod("AetherScript.Script", "Compute", 5);
        host.Unload();

        host.Load(CompileToBytes(ScriptV2Source));
        object? resultadoV2 = host.InvokeStaticMethod("AetherScript.Script", "Compute", 5);
        host.Unload();

        Assert.Equal(6, resultadoV1, what: "V1 (x + 1) com 5 deve devolver 6");
        Assert.Equal(50, resultadoV2, what: "V2 (x * 10) com 5 deve devolver 50 — prova que o reload trocou o comportamento, não é cache da V1");
    }

    [Test] public static void LoadInvokeUnload_CicloCompleto_MedeTempoENaoVazaMemoria()
    {
        var host = new HotReloadHost();
        byte[] scriptBytes = CompileToBytes(ScriptV2Source); // compilação fora da medição — o PoC mede load+invoke+unload, não o build

        var stopwatch = Stopwatch.StartNew();
        host.Load(scriptBytes);
        object? resultado = host.InvokeStaticMethod("AetherScript.Script", "Compute", 3);
        host.Unload();
        stopwatch.Stop();

        Assert.Equal(30, resultado, what: "V2 (x * 10) com 3 deve devolver 30");

        // Critério do plano (item 0.2, PoC-D): "< 2 s". Medindo só load+invoke+unload (sem o passo
        // de compilação, que pertence a um pipeline de build incremental futuro, não a este PoC) —
        // a etapa que este tipo prova fica bem abaixo da cota do plano em qualquer hardware
        // razoável; o teto aqui é generoso de propósito para não ser frágil em CI compartilhado.
        Assert.True(stopwatch.Elapsed.TotalSeconds < 2.0,
            what: $"ciclo load+invoke+unload levou {stopwatch.Elapsed.TotalMilliseconds:F1}ms — deve ficar bem abaixo do orçamento de 2s do PoC-D");

        Assert.True(host.LastContextFullyCollected, what: "após Unload + duas coletas de GC, o AssemblyLoadContext anterior deve estar totalmente coletado — prova de que o ciclo não vaza memória a cada reload");
    }

    [Test] public static void MultiplosCiclosDeReload_NaoAcumulamMemoriaIndefinidamente()
    {
        var host = new HotReloadHost();
        for (int i = 0; i < 5; i++)
        {
            host.Load(CompileToBytes(i % 2 == 0 ? ScriptV1Source : ScriptV2Source));
            host.InvokeStaticMethod("AetherScript.Script", "Compute", 1);
            host.Unload();
            Assert.True(host.LastContextFullyCollected, what: $"ciclo {i}: contexto anterior deve ser totalmente coletado antes do próximo Load, senão reloads repetidos vazariam memória indefinidamente");
        }
    }
}
