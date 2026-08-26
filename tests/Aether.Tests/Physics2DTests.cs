using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Testes do item 4.1.6: física 2D — Jolt 3D restrito ao plano XY via
/// <see cref="AllowedDOFs.Plane2D"/> (não uma segunda biblioteca física; decisão documentada em
/// jolt_bridge.h e no benchmark <c>tests/native/benchmark_physics2d.cpp</c>). Mesma disciplina
/// de tolerância/pulo-se-lib-ausente dos demais arquivos de teste de física.
/// </summary>
public static class Physics2DTests
{
    private static bool NativeLibraryAvailable() => NativeInterop.PhysicsLibraryAvailable();

    private static PhysicsBodyHandle MakeSphere2D(PhysicsWorld physics, float3 position, NativeMotionType motion) =>
        physics.CreateBody(PhysicsShape.Sphere(0.5f), position, quaternion.Identity, motion,
            allowedDOFs: AllowedDOFs.Plane2D);

    [Test] public static void CorpoPlane2D_NaoSeMoveEmZ_MesmoComVelocidadeDiretaEmZ()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var body = MakeSphere2D(physics, new float3(0f, 10f, 0f), NativeMotionType.Dynamic);
        Assert.True(body.IsValid, "criação de corpo Plane2D deveria ter sucesso");

        physics.SetLinearVelocity(body, new float3(0f, 0f, 5f));
        for (int i = 0; i < 60; i++) physics.Step(1f / 60f);

        physics.GetTransform(body, out var position, out _);
        Assert.Close(0f, position.Z, eps: 0.001f, what: "corpo Plane2D não pode se mover em Z mesmo com velocidade em Z setada diretamente");
    }

    [Test] public static void CorpoPlane2D_CaiNormalmenteNoPlanoXY_SobGravidade()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var body = MakeSphere2D(physics, new float3(0f, 50f, 0f), NativeMotionType.Dynamic);

        const float dt = 1f / 60f;
        const int steps = 30;
        for (int i = 0; i < steps; i++) physics.Step(dt);
        float elapsed = dt * steps;

        physics.GetTransform(body, out var position, out _);
        float expectedY = 50f - 0.5f * 9.81f * elapsed * elapsed;
        Assert.Close(expectedY, position.Y, eps: 0.05f, what: "queda livre no plano 2D deveria seguir a mesma cinemática de um corpo 3D");
    }

    [Test] public static void CorpoPlane2D_AssentaSobrePiso2D_NaAlturaEsperada()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        physics.CreateBody(PhysicsShape.Box(new float3(10f, 0.5f, 10f)), float3.Zero, quaternion.Identity,
            NativeMotionType.Static, allowedDOFs: AllowedDOFs.Plane2D);
        var sphere = MakeSphere2D(physics, new float3(0f, 5f, 0f), NativeMotionType.Dynamic);

        for (int i = 0; i < 300; i++) physics.Step(1f / 60f); // 5s

        physics.GetTransform(sphere, out var position, out _);
        Assert.Close(1f, position.Y, eps: 0.02f, what: "esfera 2D deveria assentar sobre o piso 2D na soma das meias-alturas");
        Assert.Close(0f, position.Z, eps: 0.001f, what: "esfera 2D não pode ter se deslocado em Z");
    }

    [Test] public static void CorpoDynamic_ComTodosOsDofsDeTranslacaoTravados_EhRecusado()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var body = physics.CreateBody(PhysicsShape.Sphere(0.5f), new float3(0f, 5f, 0f), quaternion.Identity,
            NativeMotionType.Dynamic, allowedDOFs: AllowedDOFs.RotationZ); // só rotação, nenhuma translação livre

        Assert.False(body.IsValid, "corpo Dynamic sem nenhum eixo de translação livre deveria ser recusado, não crashar");
    }

    [Test] public static void CorpoStatic_ComDofsTravados_NaoEhAfetadoPelaValidacaoDeDynamic()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var body = physics.CreateBody(PhysicsShape.Box(new float3(10f, 0.5f, 10f)), float3.Zero, quaternion.Identity,
            NativeMotionType.Static, allowedDOFs: AllowedDOFs.RotationZ);

        Assert.True(body.IsValid, "corpo Static com DOFs de translação travados deveria ser criado normalmente");
    }

    [Test] public static void CorpoNormal_AllowedDOFsDefault_ContinuaLivreEmTodosOsEixos()
    {
        // Regressão: o parâmetro opcional allowedDOFs default (AllowedDOFs.All == 0) precisa
        // continuar produzindo um corpo 3D totalmente livre, exatamente como antes do item
        // 4.1.6 existir — não passar o argumento é o caminho mais comum de chamada.
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var body = physics.CreateBody(PhysicsShape.Box(new float3(0.5f)), float3.Zero, quaternion.Identity, NativeMotionType.Dynamic);
        Assert.True(body.IsValid, "corpo 3D com allowedDOFs default deveria ser criado normalmente");

        physics.SetLinearVelocity(body, new float3(0f, 0f, 5f));
        for (int i = 0; i < 60; i++) physics.Step(1f / 60f);

        physics.GetTransform(body, out var position, out _);
        Assert.True(position.Z > 1f, "corpo 3D normal (allowedDOFs default) deveria se mover livremente em Z");
    }
}
