using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Ponto único de verificação de "a biblioteca nativa aether_physics está disponível" — antes
/// duplicado em cada arquivo de teste de física (PhysicsTests, PhysicsJointTests,
/// PhysicsQueryTests, PhysicsCharacterTests, Physics2DTests). Além de checar, conta quantas vezes
/// um teste retornou cedo por ausência da lib, para <see cref="TestRunner"/> poder aplicar a
/// regra do plano de lacunas (GAP-CORE-01/§4.2 de PLANO-FECHAMENTO-LACUNAS.md): skip por
/// dependência nativa ausente é aceitável no job unitário local, mas o job de integração
/// (AETHER_REQUIRE_NATIVE=1) deve falhar em vez de reportar verde vazio.
/// </summary>
public static class NativeInterop
{
    private static bool? _cachedAvailable;

    public static int SkippedForMissingLibraryCount { get; private set; }

    /// <summary>
    /// True se aether_physics.dll/.so carregou com sucesso. O resultado é cacheado (criar um
    /// PhysicsWorld tem custo real) — a disponibilidade da lib não muda durante a execução da
    /// suíte, só entre execuções do processo.
    /// </summary>
    public static bool PhysicsLibraryAvailable()
    {
        _cachedAvailable ??= TryCreateWorld();
        if (_cachedAvailable == false) SkippedForMissingLibraryCount++;
        return _cachedAvailable.Value;
    }

    private static bool TryCreateWorld()
    {
        try { using var w = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16); return true; }
        catch (DllNotFoundException) { return false; }
    }
}
