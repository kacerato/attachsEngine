namespace Aether.Physics;

/// <summary>
/// Sincroniza o ECS com um <see cref="PhysicsWorld"/> nativo: cria corpos Jolt para entidades
/// novas com <see cref="RigidBody"/>+<see cref="Collider"/>, avança a simulação, e copia a
/// transform resultante de volta para <see cref="WorldTransform"/>. Classe estática com um método
/// de entrada por frame — mesmo padrão de <see cref="TransformSystem"/> (não há scheduler de
/// sistemas no repo ainda; quem chama decide a ordem).
/// <para>
/// Ordem esperada no loop de frame: <see cref="TransformSystem.Propagate"/> primeiro (resolve
/// <see cref="WorldTransform"/> a partir de <see cref="LocalTransform"/> para entidades sem
/// física, e dá o estado inicial correto para corpos recém-criados), depois <see cref="Step"/>.
/// </para>
/// <para>
/// Direção da sincronização por tipo de corpo: <see cref="NativeMotionType.Dynamic"/> é dono da
/// transform depois de criado (Jolt→ECS todo frame, a simulação decide a posição);
/// <see cref="NativeMotionType.Static"/> mantém a transform de autoria e não sincroniza depois da
/// criação. <see cref="NativeMotionType.Kinematic"/> é dirigido pelo ECS/animação e envia alvos
/// ECS→Jolt antes do Step. O cache de alvo por body evita crossings estáveis e envia um comando
/// final para zerar a velocidade depois que o movimento termina.
/// </para>
/// </summary>
public static class PhysicsSyncSystem
{
    /// <summary>
    /// Cria corpos nativos para entidades novas, avança a simulação em <paramref name="deltaTime"/>,
    /// e copia a transform resultante de corpos dinâmicos de volta para o ECS. Não aloca no
    /// caminho quente (corpos já existentes) — a única alocação é para o corpo novo (evento raro,
    /// não every-frame), e mesmo essa não usa <see cref="EntityCommandBuffer"/> porque escrever
    /// <see cref="RigidBody.Handle"/> num componente já existente não é mudança estrutural.
    /// </summary>
    public static void Step(World world, PhysicsWorld physicsWorld, float deltaTime, int collisionSteps = 1)
    {
        SyncNewBodies(world, physicsWorld);

        SyncKinematicEcsToJolt(world, physicsWorld, deltaTime);

        physicsWorld.Step(deltaTime, collisionSteps);

        SyncDynamicJoltToEcs(world, physicsWorld);
    }

    private static void SyncKinematicEcsToJolt(World world, PhysicsWorld physicsWorld, float deltaTime)
    {
        foreach (var chunk in world.Query().With<RigidBody>().With<WorldTransform>())
        {
            var bodies = chunk.GetReadOnlySpan<RigidBody>();
            var transforms = chunk.GetReadOnlySpan<WorldTransform>();
            for (int i = 0; i < chunk.Count; i++)
            {
                if (bodies[i].MotionType != NativeMotionType.Kinematic || !bodies[i].Handle.IsValid)
                    continue;
                var target = transforms[i].Value;
                physicsWorld.MoveKinematic(bodies[i].Handle, target.Position, target.Rotation, deltaTime);
            }
        }
    }

    /// <summary>Cria o corpo nativo de toda entidade com <see cref="RigidBody"/>+<see cref="Collider"/>
    /// cujo <see cref="RigidBody.Handle"/> ainda está <see cref="PhysicsBodyHandle.Invalid"/> — é
    /// assim que o sistema descobre "entidade nova" sem precisar de evento de ECS (que não existe
    /// no repo ainda, ver <c>EntityCommandBuffer.cs</c>).</summary>
    private static void SyncNewBodies(World world, PhysicsWorld physicsWorld)
    {
        foreach (var chunk in world.Query().With<RigidBody>().With<Collider>().With<WorldTransform>())
        {
            var bodies = chunk.GetReadOnlySpan<RigidBody>();
            var colliders = chunk.GetReadOnlySpan<Collider>();
            var transforms = chunk.GetReadOnlySpan<WorldTransform>();

            bool needsCreation = false;
            for (int i = 0; i < chunk.Count; i++)
                if (!bodies[i].Handle.IsValid) { needsCreation = true; break; }
            if (!needsCreation) continue;

            var writableBodies = chunk.GetWritableSpan<RigidBody>();

            for (int i = 0; i < chunk.Count; i++)
            {
                if (writableBodies[i].Handle.IsValid) continue;

                var t = transforms[i].Value;
                var handle = physicsWorld.CreateBody(colliders[i].ToShape(), t.Position, t.Rotation,
                    writableBodies[i].MotionType, writableBodies[i].Friction, writableBodies[i].Restitution);
                writableBodies[i].Handle = handle;
            }
        }
    }

    /// <summary>Corpos dinâmicos são donos da posição — a simulação decide, o ECS só reflete.
    /// Cinemáticos nunca entram neste caminho, evitando feedback Jolt→ECS→Jolt.</summary>
    private static void SyncDynamicJoltToEcs(World world, PhysicsWorld physicsWorld)
    {
        foreach (var chunk in world.Query().With<RigidBody>().With<WorldTransform>())
        {
            var bodies = chunk.GetReadOnlySpan<RigidBody>();

            bool hasDynamicBody = false;
            for (int i = 0; i < chunk.Count; i++)
                if (bodies[i].MotionType == NativeMotionType.Dynamic && bodies[i].Handle.IsValid)
                { hasDynamicBody = true; break; }
            if (!hasDynamicBody) continue;

            var transforms = chunk.GetWritableSpan<WorldTransform>();

            for (int i = 0; i < chunk.Count; i++)
            {
                if (bodies[i].MotionType != NativeMotionType.Dynamic) continue;
                if (!bodies[i].Handle.IsValid) continue;

                physicsWorld.GetTransform(bodies[i].Handle, out var position, out var rotation);
                var current = transforms[i].Value;
                transforms[i].Value = new Transform(position, rotation, current.Scale);
            }
        }
    }

    /// <summary>Destrói o corpo nativo (se existir) e limpa o handle — chame antes de remover
    /// <see cref="RigidBody"/> ou destruir a entidade, tipicamente via <see cref="EntityCommandBuffer"/>
    /// se estiver dentro de uma iteração. Sem isso o corpo nativo vaza (fica vivo no
    /// <see cref="PhysicsWorld"/> sem nenhuma entidade ECS apontando para ele).</summary>
    public static void DestroyBody(World world, PhysicsWorld physicsWorld, EntityId entity)
    {
        if (!world.HasComponent<RigidBody>(entity)) return;
        ref var body = ref world.Write<RigidBody>(entity);
        if (!body.Handle.IsValid) return;
        physicsWorld.DestroyBody(body.Handle);
        body.Handle = PhysicsBodyHandle.Invalid;
    }
}
