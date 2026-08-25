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
/// <see cref="NativeMotionType.Static"/> não sincroniza depois da criação — nunca se move, então
/// recopiar a cada frame seria uma chamada nativa desperdiçada. <see cref="NativeMotionType.Kinematic"/>
/// só recebe a transform inicial na criação — ver o comentário de <see cref="SyncDynamicJoltToEcs"/>
/// para por que reposicioná-lo depois disso não está implementado nesta fatia.
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

        physicsWorld.Step(deltaTime, collisionSteps);

        SyncDynamicJoltToEcs(world, physicsWorld);
    }

    /// <summary>Cria o corpo nativo de toda entidade com <see cref="RigidBody"/>+<see cref="Collider"/>
    /// cujo <see cref="RigidBody.Handle"/> ainda está <see cref="PhysicsBodyHandle.Invalid"/> — é
    /// assim que o sistema descobre "entidade nova" sem precisar de evento de ECS (que não existe
    /// no repo ainda, ver <c>EntityCommandBuffer.cs</c>).</summary>
    private static void SyncNewBodies(World world, PhysicsWorld physicsWorld)
    {
        foreach (var chunk in world.Query().With<RigidBody>().With<Collider>().With<WorldTransform>())
        {
            var bodies = chunk.GetSpan<RigidBody>();
            var colliders = chunk.GetSpan<Collider>();
            var transforms = chunk.GetSpan<WorldTransform>();

            for (int i = 0; i < chunk.Count; i++)
            {
                if (bodies[i].Handle.IsValid) continue;

                var t = transforms[i].Value;
                var handle = physicsWorld.CreateBody(colliders[i].ToShape(), t.Position, t.Rotation,
                    bodies[i].MotionType, bodies[i].Friction, bodies[i].Restitution);
                bodies[i].Handle = handle;
            }
        }
    }

    /// <summary>Corpos dinâmicos são donos da posição — a simulação decide, o ECS só reflete.
    /// <para>
    /// Corpo <see cref="NativeMotionType.Kinematic"/> NÃO é resincronizado ECS→Jolt depois da
    /// criação nesta fatia — deliberadamente, não por descuido: mover um corpo cinemático exige
    /// <c>JPH::BodyInterface::MoveKinematic</c> (ou um setter de transform direto), e nenhum dos
    /// dois é exposto pela fronteira C ABI atual (<c>native/physics/jolt_bridge.h</c>, item
    /// 4.1.1) — só <c>AetherPhysics_SetLinearVelocity</c>/<c>AetherPhysics_GetTransform</c> existem.
    /// Adicionar esse setter é extensão do lado nativo, fora do escopo de "fachada C#" deste item.
    /// Um cinemático recebe a transform correta uma única vez, na criação (<see cref="SyncNewBodies"/>).
    /// </para>
    /// </summary>
    private static void SyncDynamicJoltToEcs(World world, PhysicsWorld physicsWorld)
    {
        foreach (var chunk in world.Query().With<RigidBody>().With<WorldTransform>())
        {
            var bodies = chunk.GetSpan<RigidBody>();
            var transforms = chunk.GetSpan<WorldTransform>();

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
        ref var body = ref world.GetComponent<RigidBody>(entity);
        if (!body.Handle.IsValid) return;
        physicsWorld.DestroyBody(body.Handle);
        body.Handle = PhysicsBodyHandle.Invalid;
    }
}
