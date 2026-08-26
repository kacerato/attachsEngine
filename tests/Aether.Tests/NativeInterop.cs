using Aether.Physics;
using Aether.Resources;

namespace Aether.Tests;

/// <summary>
/// Ponto único de verificação de "a biblioteca nativa X está disponível" — antes duplicado em cada
/// arquivo de teste de física (PhysicsTests, PhysicsJointTests, PhysicsQueryTests,
/// PhysicsCharacterTests, Physics2DTests), agora também usado por SqliteBridgeTests (item 1.4.5).
/// Além de checar, conta quantas vezes um teste retornou cedo por ausência de QUALQUER lib nativa
/// conhecida, para <see cref="TestRunner"/> poder aplicar a regra do plano de lacunas
/// (GAP-CORE-01/§4.2 de PLANO-FECHAMENTO-LACUNAS.md): skip por dependência nativa ausente é
/// aceitável no job unitário local, mas o job de integração (AETHER_REQUIRE_NATIVE=1) deve falhar
/// em vez de reportar verde vazio.
/// </summary>
public static class NativeInterop
{
    private static bool? _cachedPhysicsAvailable;
    private static bool? _cachedSqliteAvailable;

    public static int SkippedForMissingLibraryCount { get; private set; }

    /// <summary>
    /// True se aether_physics.dll/.so carregou com sucesso. O resultado é cacheado (criar um
    /// PhysicsWorld tem custo real) — a disponibilidade da lib não muda durante a execução da
    /// suíte, só entre execuções do processo.
    /// </summary>
    public static bool PhysicsLibraryAvailable()
    {
        _cachedPhysicsAvailable ??= TryCreateWorld();
        if (_cachedPhysicsAvailable == false) SkippedForMissingLibraryCount++;
        return _cachedPhysicsAvailable.Value;
    }

    /// <summary>True se aether_resources.dll/.so (item 1.4.5) carregou com sucesso. Mesma
    /// disciplina de cache/contagem de <see cref="PhysicsLibraryAvailable"/>.</summary>
    public static bool SqliteLibraryAvailable()
    {
        _cachedSqliteAvailable ??= TryOpenScratchDatabase();
        if (_cachedSqliteAvailable == false) SkippedForMissingLibraryCount++;
        return _cachedSqliteAvailable.Value;
    }

    private static bool TryCreateWorld()
    {
        try { using var w = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16); return true; }
        catch (DllNotFoundException) { return false; }
    }

    private static bool TryOpenScratchDatabase()
    {
        string path = Path.Combine(Path.GetTempPath(), $"aether-sqlite-probe-{Guid.NewGuid():N}.db");
        try
        {
            using var conn = SqliteConnection.Open(path);
            return true;
        }
        catch (DllNotFoundException) { return false; }
        finally { File.Delete(path); }
    }
}
