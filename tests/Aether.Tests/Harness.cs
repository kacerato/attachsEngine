using System.Diagnostics;
using System.Reflection;

namespace Aether.Tests;

/// <summary>
/// Runner de testes mínimo e sem dependências. O núcleo do Aether é deliberadamente
/// livre de NuGet — inclusive nos testes — para que o build funcione offline e
/// para que a suíte possa rodar dentro do próprio dispositivo mais tarde.
/// </summary>
[AttributeUsage(AttributeTargets.Method)]
public sealed class TestAttribute : Attribute
{
    public string? Skip { get; init; }
}

public sealed class AssertException(string message) : Exception(message);

public static class Assert
{
    public static void True(bool condition, string? what = null)
    { if (!condition) throw new AssertException($"esperado verdadeiro{Suffix(what)}"); }

    public static void False(bool condition, string? what = null)
    { if (condition) throw new AssertException($"esperado falso{Suffix(what)}"); }

    public static void Equal<T>(T expected, T actual, string? what = null)
    {
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
            throw new AssertException($"esperado <{expected}>, obtido <{actual}>{Suffix(what)}");
    }

    public static void NotEqual<T>(T notExpected, T actual, string? what = null)
    {
        if (EqualityComparer<T>.Default.Equals(notExpected, actual))
            throw new AssertException($"não deveria ser <{actual}>{Suffix(what)}");
    }

    public static void Close(float expected, float actual, float eps = 1e-4f, string? what = null)
    {
        if (float.IsNaN(actual) || MathF.Abs(expected - actual) > eps)
            throw new AssertException($"esperado ~{expected}, obtido {actual} (eps {eps}){Suffix(what)}");
    }

    public static void Close(float3 expected, float3 actual, float eps = 1e-4f, string? what = null)
    {
        if (!math.Approximately(expected, actual, eps))
            throw new AssertException($"esperado ~{expected}, obtido {actual} (eps {eps}){Suffix(what)}");
    }

    public static void Close(quaternion expected, quaternion actual, float eps = 1e-4f, string? what = null)
    {
        float dot = MathF.Abs(expected.X * actual.X + expected.Y * actual.Y +
                              expected.Z * actual.Z + expected.W * actual.W);
        if (MathF.Abs(1f - dot) > eps)   // q e -q representam a mesma rotação
            throw new AssertException($"esperado ~{expected}, obtido {actual}{Suffix(what)}");
    }

    public static void Throws<TException>(Action action, string? what = null) where TException : Exception
    {
        try { action(); }
        catch (TException) { return; }
        catch (Exception e) { throw new AssertException($"esperado {typeof(TException).Name}, veio {e.GetType().Name}{Suffix(what)}"); }
        throw new AssertException($"esperado {typeof(TException).Name}, nada foi lançado{Suffix(what)}");
    }

    /// <summary>
    /// Falha se a ação alocar na heap gerenciada. É a materialização do orçamento
    /// "zero alocação de GC por frame" descrito no plano (RNF / KPI técnico).
    /// </summary>
    public static void NoAlloc(Action action, string? what = null)
    {
        action();                              // aquece: JIT, caches estáticos
        GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
        long before = GC.GetAllocatedBytesForCurrentThread();
        action();
        long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
        if (allocated > 0)
            throw new AssertException($"alocou {allocated} bytes; orçamento é 0{Suffix(what)}");
    }

    private static string Suffix(string? what) => what is null ? "" : $" — {what}";
}

public static class TestRunner
{
    private const string Green = "[32m";
    private const string Red = "[31m";
    private const string Dim = "[2m";
    private const string Reset = "[0m";

    public static int Run(string[] args)
    {
        string? filter = args.FirstOrDefault(a => !a.StartsWith('-'));
        var methods = Assembly.GetExecutingAssembly().GetTypes()
            .SelectMany(t => t.GetMethods(BindingFlags.Public | BindingFlags.Static))
            .Select(m => (m, a: m.GetCustomAttribute<TestAttribute>()))
            .Where(x => x.a is not null)
            .OrderBy(x => x.m.DeclaringType!.Name, StringComparer.Ordinal)
            .ThenBy(x => x.m.Name, StringComparer.Ordinal)
            .ToList();

        int passed = 0, failed = 0, skipped = 0;
        int missingNativeTests = 0;
        string? currentGroup = null;
        var sw = Stopwatch.StartNew();
        var failures = new List<string>();

        foreach (var (method, attr) in methods)
        {
            string group = method.DeclaringType!.Name;
            string name = $"{group}.{method.Name}";
            if (filter is not null && !name.Contains(filter, StringComparison.OrdinalIgnoreCase)) continue;

            if (currentGroup != group) { Console.WriteLine($"\n  {group}"); currentGroup = group; }

            if (attr!.Skip is not null)
            { skipped++; Console.WriteLine($"    {Dim}- {method.Name}  ({attr.Skip}){Reset}"); continue; }

            try
            {
                int missingNativeBefore = NativeInterop.SkippedForMissingLibraryCount;
                method.Invoke(null, null);
                if (NativeInterop.SkippedForMissingLibraryCount > missingNativeBefore)
                {
                    skipped++;
                    missingNativeTests++;
                    Console.WriteLine($"    {Dim}- {method.Name}  (biblioteca nativa ausente){Reset}");
                }
                else
                {
                    passed++;
                    Console.WriteLine($"    {Green}ok{Reset}   {method.Name}");
                }
            }
            catch (TargetInvocationException tie)
            {
                failed++;
                var inner = tie.InnerException!;
                string msg = inner is AssertException ? inner.Message : $"{inner.GetType().Name}: {inner.Message}";
                Console.WriteLine($"    {Red}FALHA{Reset} {method.Name}\n         {msg}");
                failures.Add($"{name}: {msg}");
                if (inner is not AssertException)
                    Console.WriteLine($"         {inner.StackTrace?.Split('\n').FirstOrDefault()?.Trim()}");
            }
        }

        // GAP-CORE-01 / §4.2 de PLANO-FECHAMENTO-LACUNAS.md: "teste que depende de aether_physics
        // deve falhar no job de integração se a biblioteca não estiver presente. Skip é permitido
        // somente no job unitário explicitamente sem nativo." AETHER_REQUIRE_NATIVE=1 é esse job
        // de integração — sem ela, ausência de lib nativa continua sendo "ok" silencioso (fluxo
        // de desenvolvimento local sem toolchain nativa instalada), exatamente como antes.
        bool requireNative = Environment.GetEnvironmentVariable("AETHER_REQUIRE_NATIVE") == "1";
        if (requireNative && missingNativeTests > 0)
        {
            failed++;
            string msg = $"AETHER_REQUIRE_NATIVE=1, mas a biblioteca nativa \"aether_physics\" não foi encontrada " +
                         $"({missingNativeTests} teste(s) foram marcados como pulados) — job de integração " +
                         "não pode reportar verde sem o nativo presente.";
            Console.WriteLine($"\n  {Red}FALHA{Reset} integração-nativa\n         {msg}");
            failures.Add($"integração-nativa: {msg}");
        }

        sw.Stop();
        string verdict = failed == 0 ? $"{Green}TUDO VERDE{Reset}" : $"{Red}{failed} FALHAS{Reset}";
        Console.WriteLine($"\n  {verdict} — {passed} passaram, {failed} falharam, {skipped} pulados em {sw.ElapsedMilliseconds} ms\n");
        if (failures.Count > 0)
        {
            Console.WriteLine("  Falhas:");
            foreach (var f in failures) Console.WriteLine($"    - {f}");
            Console.WriteLine();
        }
        return failed == 0 ? 0 : 1;
    }
}

public static class Program
{
    public static int Main(string[] args) => TestRunner.Run(args);
}
