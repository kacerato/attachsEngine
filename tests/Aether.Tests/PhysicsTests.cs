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
/// Jolt em test_jolt_bridge.cpp). Retornam cedo se a biblioteca nativa "aether_physics" não
/// estiver ao lado do executável de teste (ver NativeInterop e Aether.Tests.csproj) — o
/// TestRunner decide se isso é aceitável (job unitário local) ou motivo de falha (job de
/// integração, AETHER_REQUIRE_NATIVE=1), não cada teste individualmente.
/// </para>
/// </summary>
public static class PhysicsTests
{
    private static bool NativeLibraryAvailable() => NativeInterop.PhysicsLibraryAvailable();

    [Test] public static void WorldConfiguration_DefaultsMantemCapacidadesIndependentes()
    {
        var config = PhysicsWorldConfiguration.ForBodyCapacity(float3.Zero, 500);
        Assert.Equal(500u, config.MaxBodies, "maxBodies deve representar somente corpos");
        Assert.Equal(2000u, config.MaxBodyPairs, "pares devem receber default conservador próprio");
        Assert.Equal(1024u, config.MaxContactConstraints, "contacts preserva seu piso independente");
        Assert.Equal(16384u, config.MaxBroadPhasePairs, "buffer da broad phase mantém orçamento próprio");
    }

    [Test] public static void WorldConfiguration_RejeitaLimitesQueOJoltNaoPodeUsar()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() =>
            new PhysicsWorldConfiguration(float3.Zero, 0, 4, 1, 1024));
        Assert.Throws<ArgumentOutOfRangeException>(() =>
            new PhysicsWorldConfiguration(float3.Zero, 8, 3, 1, 1024));
        Assert.Throws<ArgumentOutOfRangeException>(() =>
            new PhysicsWorldConfiguration(float3.Zero, 8, 32, 16, 1));
    }

    [Test] public static void StepV2_DevolveFlagsEContadoresDoMesmoMundo()
    {
        if (!NativeLibraryAvailable()) return;
        var config = new PhysicsWorldConfiguration(new float3(0f, -9.81f, 0f),
            maxBodies: 16, maxBodyPairs: 128, maxContactConstraints: 64,
            maxBroadPhasePairs: 4096);
        using var physics = new PhysicsWorld(in config);

        PhysicsUpdateError flags = physics.Step(1f / 60f);
        PhysicsStepStatistics stats = physics.GetStepStatistics();

        Assert.Equal(PhysicsUpdateError.None, flags, "mundo vazio não deve saturar capacidade");
        Assert.Equal(1ul, stats.TotalSteps, "estatística deve contar o StepV2 executado");
        Assert.Equal(0ul, stats.OverflowSteps, "mundo vazio não deve registrar overflow");
        Assert.Equal(PhysicsUpdateError.None, stats.LastErrorFlags,
            "última máscara deve corresponder ao retorno de StepV2");
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
        var handleApos1 = world.Read<RigidBody>(e).Handle;
        Assert.True(handleApos1.IsValid, "primeiro Step deveria ter criado o corpo nativo");

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        var handleApos2 = world.Read<RigidBody>(e).Handle;
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
        var posicaoFinal = world.Read<WorldTransform>(e).Value.Position;

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

        var final = world.Read<WorldTransform>(e).Value.Position;
        Assert.Close(moved.Position, final, what: "corpo estático não deveria ter sua transform sobrescrita pela sincronização");
    }

    [Test] public static void KinematicSync_EmiteMovimentoEParadaUmaVezSemCrossingEstavel()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var platform = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)), RigidBody.Kinematic());
        world.AddComponent(platform, Collider.Box(new float3(2f, 0.5f, 2f)));

        for (int i = 0; i < 5; i++) PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        Assert.Equal(0ul, physics.KinematicMoveCallCount,
            "alvo inicial estável não deve atravessar a ABI");

        world.SetComponent(platform, new WorldTransform(Transform.FromPosition(new float3(1f, 0f, 0f))));
        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        Assert.Equal(1ul, physics.KinematicMoveCallCount, "mudança real deve emitir MoveKinematic");

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        Assert.Equal(2ul, physics.KinematicMoveCallCount,
            "primeiro frame estável após mover deve zerar a velocidade calculada");
        for (int i = 0; i < 10; i++) PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        Assert.Equal(2ul, physics.KinematicMoveCallCount,
            "frames estáveis seguintes não devem repetir crossing");
    }

    [Test] public static void PlataformaCinematicaMovidaPeloEcs_TransportaCorpoDinamico()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var platform = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)), RigidBody.Kinematic(1f));
        world.AddComponent(platform, Collider.Box(new float3(3f, 0.5f, 3f)));
        var box = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(new float3(0f, 1.1f, 0f))), RigidBody.Dynamic(1f));
        world.AddComponent(box, Collider.Box(new float3(0.5f)));

        const float dt = 1f / 60f;
        for (int i = 0; i < 90; i++) PhysicsSyncSystem.Step(world, physics, dt);
        for (int i = 1; i <= 90; i++)
        {
            float progress = i / 90f;
            var target = new Transform(new float3(progress * 1.5f, 0f, 0f),
                quaternion.AxisAngle(float3.Up, progress * 0.3f), float3.One);
            world.SetComponent(platform, new WorldTransform(target));
            PhysicsSyncSystem.Step(world, physics, dt);
        }

        float3 final = world.Read<WorldTransform>(box).Value.Position;
        Assert.True(final.X > 0.4f,
            "corpo apoiado deveria acompanhar a plataforma dirigida pelo ECS");
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
        Assert.True(world.Read<RigidBody>(e).Handle.IsValid, "corpo deveria ter sido criado");

        PhysicsSyncSystem.DestroyBody(world, physics, e);
        Assert.False(world.Read<RigidBody>(e).Handle.IsValid, "handle deveria voltar a Invalid após destruir");

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
        Assert.Equal(world.Read<RigidBody>(chao).Handle, hitBody, "corpo acertado deveria ser o handle do piso");
        Assert.Close(0.475f, fraction, eps: 0.01f, what: "fração do impacto deveria corresponder ao topo do piso (y=0.5)");
    }

    [Test] public static void TriggerEcs_PublicaEnterStayExitEResolveEntidades()
    {
        if (!NativeLibraryAvailable()) return;
        var world = new World();
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var sensorEntity = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)), RigidBody.Kinematic());
        world.AddComponent(sensorEntity, Collider.Box(new float3(1f)));
        world.AddComponent(sensorEntity, new Trigger(QueryLayerMask.Static));
        var otherEntity = world.CreateEntity(
            new WorldTransform(Transform.FromPosition(float3.Zero)), RigidBody.Static());
        world.AddComponent(otherEntity, Collider.Sphere(0.25f));

        Span<PhysicsTriggerEvent> events = stackalloc PhysicsTriggerEvent[4];
        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        int count = physics.GetTriggerEvents(events);
        Assert.True(Contains(events, count, TriggerEventType.Enter, out var enter),
            "primeiro overlap ECS deveria produzir Enter");
        Assert.True(PhysicsSyncSystem.TryResolveTriggerEvent(world, in enter, out var resolvedSensor, out var resolvedOther),
            "handles nativos do evento deveriam resolver de volta para o ECS");
        Assert.Equal(sensorEntity, resolvedSensor);
        Assert.Equal(otherEntity, resolvedOther);

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);
        count = physics.GetTriggerEvents(events);
        Assert.True(Contains(events, count, TriggerEventType.Stay, out _),
            "overlap preservado deveria produzir Stay");

        world.SetComponent(sensorEntity,
            new WorldTransform(Transform.FromPosition(new float3(10f, 0f, 0f))));
        bool exited = false;
        for (int i = 0; i < 3 && !exited; i++)
        {
            PhysicsSyncSystem.Step(world, physics, 1f / 60f);
            count = physics.GetTriggerEvents(events);
            exited = Contains(events, count, TriggerEventType.Exit, out _);
        }
        Assert.True(exited, "separar o sensor pelo fluxo ECS deveria produzir Exit");
    }

    [Test] public static void TriggerFiltroEBuffer_ExcluemDynamicEInformamContagemReal()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);
        var sensor = physics.CreateBody(PhysicsShape.Box(new float3(2f)), float3.Zero,
            quaternion.Identity, NativeMotionType.Kinematic, isSensor: true,
            eventLayerMask: QueryLayerMask.Static);
        var accepted = physics.CreateBody(PhysicsShape.Sphere(0.25f), new float3(-0.5f, 0f, 0f),
            quaternion.Identity, NativeMotionType.Static);
        _ = physics.CreateBody(PhysicsShape.Sphere(0.25f), new float3(0.5f, 0f, 0f),
            quaternion.Identity, NativeMotionType.Dynamic);

        physics.Step(1f / 60f);
        Assert.Equal(1, physics.GetTriggerEvents(Span<PhysicsTriggerEvent>.Empty),
            "consulta sem buffer deve informar a capacidade necessária");
        Span<PhysicsTriggerEvent> one = stackalloc PhysicsTriggerEvent[1];
        Assert.Equal(1, physics.GetTriggerEvents(one));
        Assert.Equal(sensor, one[0].Sensor);
        Assert.Equal(accepted, one[0].Other, "filtro Static deve excluir corpo Dynamic");
        Assert.Equal(TriggerEventType.Enter, one[0].Type);
    }

    [Test] public static void BodyBatch_100_1000_10000_UsaUmCrossingESemGcNoCrossing()
    {
        if (!NativeLibraryAvailable()) return;
        foreach (int count in new[] { 100, 1_000, 10_000 })
        {
            var config = PhysicsWorldConfiguration.ForBodyCapacity(float3.Zero, (uint)(count + 1));
            using var physics = new PhysicsWorld(in config);
            var descriptions = new PhysicsBodyDescription[count];
            var handles = new PhysicsBodyHandle[count];
            var shape = PhysicsShape.Sphere(0.1f);
            for (int i = 0; i < count; i++)
                descriptions[i] = new PhysicsBodyDescription(shape,
                    new float3((i % 100) * 0.4f, i / 100 * 0.4f, 0f),
                    quaternion.Identity, NativeMotionType.Static);

            // Aquece o stub LibraryImport e o caminho nativo antes da região medida.
            var warmup = physics.CreateBody(shape, new float3(-10f, 0f, 0f),
                quaternion.Identity, NativeMotionType.Static);
            physics.DestroyBody(warmup);
            physics.ResetBodyInteropStatistics();
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();

            long before = GC.GetAllocatedBytesForCurrentThread();
            int created = physics.CreateBodies(descriptions, handles);
            long allocated = GC.GetAllocatedBytesForCurrentThread() - before;

            Assert.Equal(count, created, $"lote de {count} corpos deve ser integral");
            Assert.Equal(0L, allocated, $"crossing de {count} corpos não deve alocar heap gerenciado");
            var afterCreate = physics.GetBodyInteropStatistics();
            Assert.Equal(1ul, afterCreate.NativeCrossings, "N corpos precisam cruzar a ABI uma vez");
            Assert.Equal((ulong)count, afterCreate.BodiesCreated);
            Assert.Equal((ulong)(count * 4), afterCreate.BytesReceived,
                "retorno transfere exatamente um uint por handle");
            for (int i = 0; i < count; i++)
                Assert.True(handles[i].IsValid, "todo descritor deve produzir handle válido");

            physics.DestroyBodies(handles);
            var afterDestroy = physics.GetBodyInteropStatistics();
            Assert.Equal(2ul, afterDestroy.NativeCrossings,
                "create e destroy em massa devem somar somente dois crossings");
            Assert.Equal((ulong)count, afterDestroy.BodiesDestroyed);
        }
    }

    [Test] public static void PhysicsSyncSystem_SpawnDeMilEstaticosUsaUmUnicoCrossingDeCriacao()
    {
        if (!NativeLibraryAvailable()) return;
        const int count = 1_000;
        var world = new World();
        using var physics = new PhysicsWorld(float3.Zero, count);
        for (int i = 0; i < count; i++)
        {
            var entity = world.CreateEntity(
                new WorldTransform(Transform.FromPosition(new float3(i * 0.25f, 0f, 0f))),
                RigidBody.Static());
            world.AddComponent(entity, Collider.Sphere(0.1f));
        }
        physics.ResetBodyInteropStatistics();

        PhysicsSyncSystem.Step(world, physics, 1f / 60f);

        var stats = physics.GetBodyInteropStatistics();
        Assert.Equal(1ul, stats.NativeCrossings,
            "todas as entidades novas do Step devem compartilhar o mesmo CreateBodiesV2");
        Assert.Equal((ulong)count, stats.BodiesCreated);
        foreach (var chunk in world.Query().With<RigidBody>())
            foreach (var body in chunk.GetReadOnlySpan<RigidBody>())
                Assert.True(body.Handle.IsValid, "spawn em lote precisa escrever cada handle no ECS");
    }

    private static bool Contains(Span<PhysicsTriggerEvent> events, int realCount,
        TriggerEventType type, out PhysicsTriggerEvent found)
    {
        int copied = Math.Min(realCount, events.Length);
        for (int i = 0; i < copied; i++)
            if (events[i].Type == type) { found = events[i]; return true; }
        found = default;
        return false;
    }
}
