using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Testes do item 4.1.4: queries de física — RayCastAll (multi-hit), ShapeCastClosest
/// (varredura) e OverlapShape (overlap parado) sobre <see cref="PhysicsWorld"/>.
/// RayCastClosest (single-hit) já é coberto por PhysicsTests.cs. Mesma disciplina de
/// tolerância/pulo-se-lib-ausente dos demais arquivos de teste de física.
/// </summary>
public static class PhysicsQueryTests
{
    private static bool NativeLibraryAvailable()
    {
        try { using var w = new PhysicsWorld(float3.Zero, 16); return true; }
        catch (DllNotFoundException) { return false; }
    }

    private static PhysicsBodyHandle MakeSphere(PhysicsWorld physics, float3 position, NativeMotionType motion) =>
        physics.CreateBody(PhysicsShape.Sphere(0.5f), position, quaternion.Identity, motion);

    private static PhysicsBodyHandle MakeBox(PhysicsWorld physics, float3 halfExtent, float3 position, NativeMotionType motion) =>
        physics.CreateBody(PhysicsShape.Box(halfExtent), position, quaternion.Identity, motion);

    [Test] public static void RayCastAll_AcertaTodosOsCorposEmpilhadosOrdenadosPorDistancia()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var a = MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Static);
        var b = MakeSphere(physics, new float3(0f, 5f, 0f), NativeMotionType.Static);
        var c = MakeSphere(physics, new float3(0f, 2f, 0f), NativeMotionType.Static);

        Span<PhysicsBodyHandle> bodies = stackalloc PhysicsBodyHandle[8];
        Span<float> fractions = stackalloc float[8];
        int count = physics.RayCastAll(new float3(0f, 10f, 0f), new float3(0f, -20f, 0f), bodies, fractions);

        Assert.Equal(3, count, "raio vertical deveria acertar as 3 esferas empilhadas");
        Assert.Equal(a, bodies[0], "primeiro hit deveria ser o corpo mais próximo");
        Assert.Equal(b, bodies[1], "segundo hit deveria ser o corpo do meio");
        Assert.Equal(c, bodies[2], "terceiro hit deveria ser o corpo mais distante");
        Assert.True(fractions[0] < fractions[1] && fractions[1] < fractions[2], "frações deveriam crescer monotonicamente");
    }

    [Test] public static void RayCastAll_ComBufferMenorQueOsHits_DevolveContagemRealNaoTruncada()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Static);
        MakeSphere(physics, new float3(0f, 5f, 0f), NativeMotionType.Static);
        MakeSphere(physics, new float3(0f, 2f, 0f), NativeMotionType.Static);

        Span<PhysicsBodyHandle> onlyOne = stackalloc PhysicsBodyHandle[1];
        Span<float> onlyOneFraction = stackalloc float[1];
        int count = physics.RayCastAll(new float3(0f, 10f, 0f), new float3(0f, -20f, 0f), onlyOne, onlyOneFraction);

        Assert.Equal(3, count, "contagem devolvida deveria ser o total real, mesmo com buffer de tamanho 1");
    }

    [Test] public static void RayCastAll_ComLayerMaskStatic_IgnoraCorposDinamicos()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Static);
        MakeSphere(physics, new float3(0f, 5f, 0f), NativeMotionType.Dynamic);

        Span<PhysicsBodyHandle> bodies = stackalloc PhysicsBodyHandle[8];
        Span<float> fractions = stackalloc float[8];
        int count = physics.RayCastAll(new float3(0f, 10f, 0f), new float3(0f, -20f, 0f), bodies, fractions, QueryLayerMask.Static);

        Assert.Equal(1, count, "máscara Static deveria filtrar fora o corpo dinâmico");
    }

    [Test] public static void RayCastAll_ComIgnoreBody_ExcluiOCorpoEspecificado()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var a = MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Static);
        var b = MakeSphere(physics, new float3(0f, 5f, 0f), NativeMotionType.Static);

        Span<PhysicsBodyHandle> bodies = stackalloc PhysicsBodyHandle[8];
        Span<float> fractions = stackalloc float[8];
        int count = physics.RayCastAll(new float3(0f, 10f, 0f), new float3(0f, -20f, 0f), bodies, fractions, ignoreBody: a);

        Assert.Equal(1, count, "ignoreBody deveria excluir o corpo 'a' do resultado");
        Assert.Equal(b, bodies[0], "único hit restante deveria ser 'b'");
    }

    [Test] public static void ShapeCastClosest_EsferaVarridaAcertaOPisoNoPontoDeContato()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        const float floorHalfHeight = 0.5f;
        MakeBox(physics, new float3(10f, floorHalfHeight, 10f), float3.Zero, NativeMotionType.Static);

        bool found = physics.ShapeCastClosest(PhysicsShape.Sphere(0.5f), new float3(0f, 10f, 0f), quaternion.Identity,
            new float3(0f, -20f, 0f), out var hit);

        Assert.True(found, "esfera varrida de cima para baixo deveria acertar o piso");
        // Esfera de raio 0.5 toca o piso (topo em y=0.5) quando seu centro está em y=1.0 —
        // origem y=10, alcance 20: fração (10-1.0)/20 = 0.45.
        Assert.Close(0.45f, hit.Fraction, eps: 0.02f, what: "fração deveria corresponder ao ponto onde a esfera toca o piso");
    }

    [Test] public static void ShapeCastClosest_SemAlcanceSuficiente_NaoAcertaNada()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        MakeBox(physics, new float3(10f, 0.5f, 10f), float3.Zero, NativeMotionType.Static);

        bool found = physics.ShapeCastClosest(PhysicsShape.Sphere(0.5f), new float3(0f, 10f, 0f), quaternion.Identity,
            new float3(0f, -1f, 0f), out _);

        Assert.False(found, "alcance curto demais não pode acertar o piso distante");
    }

    [Test] public static void OverlapShape_EncontraCorposDentroDoVolumeEIgnoraOsDeFora()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var inside1 = MakeSphere(physics, new float3(0.2f, 0f, 0f), NativeMotionType.Static);
        var inside2 = MakeSphere(physics, new float3(-0.2f, 0f, 0f), NativeMotionType.Static);
        MakeSphere(physics, new float3(10f, 0f, 0f), NativeMotionType.Static); // longe, não deveria aparecer

        Span<ShapeQueryHit> hits = stackalloc ShapeQueryHit[8];
        int count = physics.OverlapShape(PhysicsShape.Box(new float3(1f)), float3.Zero, quaternion.Identity, hits);

        Assert.Equal(2, count, "volume deveria sobrepor as 2 esferas próximas, não a distante");
        bool foundInside1 = false, foundInside2 = false;
        for (int i = 0; i < count; i++)
        {
            if (hits[i].Body == inside1) foundInside1 = true;
            if (hits[i].Body == inside2) foundInside2 = true;
        }
        Assert.True(foundInside1 && foundInside2, "os dois corpos dentro do volume deveriam aparecer no resultado");
    }

    [Test] public static void OverlapShape_SemNenhumCorpoDentro_DevolveContagemZero()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        MakeSphere(physics, new float3(50f, 0f, 0f), NativeMotionType.Static);

        Span<ShapeQueryHit> hits = stackalloc ShapeQueryHit[8];
        int count = physics.OverlapShape(PhysicsShape.Box(new float3(1f)), float3.Zero, quaternion.Identity, hits);

        Assert.Equal(0, count, "volume vazio de corpos deveria devolver contagem zero");
    }

    [Test] public static void OverlapShape_ComIgnoreBody_ExcluiOProprioCorpo()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var self = MakeSphere(physics, float3.Zero, NativeMotionType.Static);
        var other = MakeSphere(physics, new float3(0.3f, 0f, 0f), NativeMotionType.Static);

        Span<ShapeQueryHit> hits = stackalloc ShapeQueryHit[8];
        int count = physics.OverlapShape(PhysicsShape.Sphere(1f), float3.Zero, quaternion.Identity, hits, ignoreBody: self);

        Assert.Equal(1, count, "ignoreBody deveria excluir 'self', deixando só 'other'");
        Assert.Equal(other, hits[0].Body, "único hit restante deveria ser 'other'");
    }
}
