using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Testes do item 4.1.2: fachada C# de <see cref="RigidBody"/>/<see cref="Collider"/> como
/// componentes ECS, sincronizados bidirecionalmente com um <see cref="PhysicsWorld"/> nativo
/// (Jolt, via P/Invoke sobre native/physics/jolt_bridge.h — item 4.1.1).
/// <para>
/// Diferente do resto da suíte, estes testes tocam um recurso nativo real (o mesmo Jolt já
/// validado por tests/native/test_jolt_bridge.cpp) — os números esperados vêm de cinemática
/// básica, com a mesma tolerância usada lá (ver o comentário sobre mLinearDamping padrão do
/// Jolt em test_jolt_bridge.cpp). Pulados (não falham) se a biblioteca nativa "aether_physics"
/// não estiver ao lado do executável de teste — ver a condição de cópia em Aether.Tests.csproj.
/// </para>
/// </summary>
public static class PhysicsTests
{
    // O harness (Harness.cs) só suporta Skip estático via atributo, não uma condição avaliada em
    // runtime — então cada teste checa a biblioteca nativa e retorna cedo (silenciosamente "ok",
    // não "pulado") quando ela está ausente. Aceitável aqui: a ausência é sempre um problema de
    // ambiente de build local (ver Aether.Tests.csproj), nunca um bug de produto a esconder.
    private static bool NativeLibraryAvailable()
    {
        try { using var w = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16); return true; }
        catch (DllNotFoundException) { return false; }
    }

    [Test] public static void CriarCorpoEstatico_NaoLancaEDevolveHandleValido()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var handle = physics.CreateBody(PhysicsShape.Box(new float3(1f)), float3.Zero, quaternion.Identity, NativeMotionType.Static);
        Assert.True(handle.IsValid, "criação de corpo estático deveria devolver handle válido");
    }

    [Test] public static void SyncNewBodies_CriaCorpoNativoUmaUnicaVez()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, 0f, 0f), 16);

        var e = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(new float3(0f, 10f, 0f))),
            RigidBody.Dynamic());
        world.AddComponent(e, Collider.Sphere(0.5f));

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        var handleApos1 = world.GetComponent<RigidBody>(e).Handle;
        Assert.True(handleApos1.IsValid, "primeiro Step deveria ter criado o corpo nativo");

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        var handleApos2 = world.GetComponent<RigidBody>(e).Handle;
        Assert.Equal(handleApos1, handleApos2, "segundo Step não deveria recriar o corpo (mesmo handle)");
    }

    [Test] public static void CorpoDinamico_CaiSobGravidadeESincronizaWorldTransform()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        float3 posicaoInicial = new(0f, 50f, 0f);
        var e = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(posicaoInicial)),
            RigidBody.Dynamic());
        world.AddComponent(e, Collider.Sphere(0.5f));

        const float dt = 1f / 60f;
        const int steps = 30; // meio segundo, longe do chão — só cinemática de queda livre
        for (int i = 0; i < steps; i++) PhysicsSyncSystem.Step(world, physics, dt);

        float elapsed = dt * steps;
        float expectedY = 50f - 0.5f * 9.81f * elapsed * elapsed;
        var posicaoFinal = world.GetComponent<WorldTransform>(e).Value.Position;

        Assert.Close(expectedY, posicaoFinal.Y, eps: 0.05f, what: "WorldTransform deveria refletir a queda livre simulada pelo Jolt");
        Assert.Close(0f, posicaoFinal.X, eps: 1e-4f, what: "gravidade vertical não pode produzir deriva em x");
    }

    [Test] public static void CorpoEstatico_NaoRecebeTransformDoJoltAposCriacao()
    {
        // Corpo estático nunca sincroniza Jolt->ECS depois da criação (ver comentário em
        // PhysicsSyncSystem.Step) — este teste prova que mover o WorldTransform de um corpo
        // estático no ECS não é sobrescrito de volta pela simulação (porque SyncDynamicJoltToEcs
        // simplesmente não olha para MotionType != Dynamic).
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var e = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)),
            RigidBody.Static());
        world.AddComponent(e, Collider.Box(new float3(1f)));

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);

        var moved = new Transform(new float3(3f, 3f, 3f), quaternion.Identity, float3.One);
        world.SetComponent(e, new WorldTransform(moved));

        for (int i = 0; i < 60; i++) PhysicsSyncSystem.Step(world, physics, 1f / 60f);

        var final = world.GetComponent<WorldTransform>(e).Value.Position;
        Assert.Close(moved.Position, final, what: "corpo estático não deveria ter sua transform sobrescrita pela sincronização");
    }

    [Test] public static void DestroyBody_LimpaHandleENaoCrashaChamadoDuasVezes()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var e = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(new float3(0f, 5f, 0f))),
            RigidBody.Dynamic());
        world.AddComponent(e, Collider.Sphere(0.5f));
        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        Assert.True(world.GetComponent<RigidBody>(e).Handle.IsValid, "corpo deveria ter sido criado");

        PhysicsSyncSystem.DestroyBody(world, physics, e);
        Assert.False(world.GetComponent<RigidBody>(e).Handle.IsValid, "handle deveria voltar a Invalid após destruir");

        PhysicsSyncSystem.DestroyBody(world, physics, e); // chamar de novo não pode crashar
    }

    [Test] public static void RayCastClosest_AcertaCorpoCriadoPelaFachada()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var chao = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)),
            RigidBody.Static());
        world.AddComponent(chao, Collider.Box(new float3(10f, 0.5f, 10f)));
        PhysicsSyncSystem.Step(world, physics, 1f / 60f);

        bool hit = physics.RayCastClosest(new float3(0f, 10f, 0f), new float3(0f, -20f, 0f), out var hitBody, out var fraction);
        Assert.True(hit, "raio de cima para baixo deveria acertar o piso criado pela fachada");
        Assert.Equal(world.GetComponent<RigidBody>(chao).Handle, hitBody, "corpo acertado deveria ser o handle do piso");
        Assert.Close(0.475f, fraction, eps: 0.01f, what: "fração do impacto deveria corresponder ao topo do piso (y=0.5)");
    }
}
