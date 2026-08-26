using System.Runtime.CompilerServices;

namespace Aether.Physics;

/// <summary>Materializa componentes <see cref="Joint"/> como constraints nativas. O estado
/// aplicado é propriedade do par <see cref="PhysicsWorld"/>+entidade, não da cena: isso permite
/// serializar somente a intenção, resolver referências tardiamente e destruir constraints mesmo
/// quando a entidade proprietária já desapareceu do ECS.</summary>
public static class JointSyncSystem
{
    private struct AppliedJoint
    {
        public Joint Definition;
        public PhysicsBodyHandle Body1;
        public PhysicsBodyHandle Body2;
        public PhysicsJointHandle Handle;
        public int SeenEpoch;
    }

    private sealed class SyncState
    {
        public readonly Dictionary<EntityId, AppliedJoint> Applied = new();
        public readonly List<EntityId> RemovalScratch = new();
        public int Epoch;
    }

    private static readonly ConditionalWeakTable<PhysicsWorld, SyncState> s_states = new();

    /// <summary>Resolve entidades, cria constraints novas, preserva handles se o descritor não
    /// mudou e executa destroy→recreate somente quando corpos ou dados aplicados mudam. Uma
    /// referência ausente remove a constraint antiga (se houver) e volta a tentar em todo sync.</summary>
    public static void Sync(World world, PhysicsWorld physicsWorld)
    {
        ArgumentNullException.ThrowIfNull(world);
        ArgumentNullException.ThrowIfNull(physicsWorld);
        SyncState state = s_states.GetValue(physicsWorld, static _ => new SyncState());
        int epoch = NextEpoch(state);

        foreach (var chunk in world.Query().With<Joint>())
        {
            var entities = chunk.Entities;
            var joints = chunk.GetReadOnlySpan<Joint>();
            for (int i = 0; i < chunk.Count; i++)
            {
                EntityId owner = entities[i];
                Joint definition = joints[i];
                if (!TryResolve(world, in definition, out var body1, out var body2, out var desc))
                {
                    Remove(state, physicsWorld, owner);
                    continue;
                }

                if (state.Applied.TryGetValue(owner, out var applied) &&
                    applied.Definition == definition && applied.Body1 == body1 && applied.Body2 == body2)
                {
                    applied.SeenEpoch = epoch;
                    state.Applied[owner] = applied;
                    continue;
                }

                if (applied.Handle.IsValid) physicsWorld.DestroySynchronizedJoint(applied.Handle);
                PhysicsJointHandle handle = physicsWorld.CreateJoint(body1, body2, in desc);
                if (!handle.IsValid)
                {
                    state.Applied.Remove(owner);
                    continue;
                }

                state.Applied[owner] = new AppliedJoint
                {
                    Definition = definition,
                    Body1 = body1,
                    Body2 = body2,
                    Handle = handle,
                    SeenEpoch = epoch,
                };
            }
        }

        // Entidade destruída ou componente removido não aparece na consulta. O sweep por época
        // fecha esse lifecycle sem exigir que World conheça PhysicsWorld nem eventos estruturais.
        state.RemovalScratch.Clear();
        foreach (var pair in state.Applied)
            if (pair.Value.SeenEpoch != epoch) state.RemovalScratch.Add(pair.Key);
        foreach (EntityId owner in state.RemovalScratch) Remove(state, physicsWorld, owner);
    }

    /// <summary>Consulta o handle materializado sem expô-lo no componente persistente.</summary>
    public static bool TryGetHandle(PhysicsWorld physicsWorld, EntityId jointEntity,
        out PhysicsJointHandle handle)
    {
        ArgumentNullException.ThrowIfNull(physicsWorld);
        if (s_states.TryGetValue(physicsWorld, out var state) &&
            state.Applied.TryGetValue(jointEntity, out var applied))
        {
            handle = applied.Handle;
            return handle.IsValid;
        }
        handle = PhysicsJointHandle.Invalid;
        return false;
    }

    /// <summary>Destrói constraints sincronizadas que referenciam um corpo antes que o corpo
    /// nativo seja removido. Chamado pelo lifecycle ECS de <see cref="PhysicsSyncSystem"/>.</summary>
    internal static void DestroyReferencingBody(PhysicsWorld physicsWorld, PhysicsBodyHandle body)
    {
        if (!body.IsValid || !s_states.TryGetValue(physicsWorld, out var state)) return;
        state.RemovalScratch.Clear();
        foreach (var pair in state.Applied)
            if (pair.Value.Body1 == body || pair.Value.Body2 == body)
                state.RemovalScratch.Add(pair.Key);
        foreach (EntityId owner in state.RemovalScratch) Remove(state, physicsWorld, owner);
    }

    /// <summary>Descarta somente o espelho gerenciado quando o próprio mundo nativo será
    /// destruído (ele já possui e libera todas as constraints). Evita expor handles obsoletos
    /// depois de <see cref="PhysicsWorld.Dispose"/> sem emitir crossings redundantes.</summary>
    internal static void ForgetWorld(PhysicsWorld physicsWorld) => s_states.Remove(physicsWorld);

    /// <summary>Invalida o espelho quando alguém destrói diretamente um handle emprestado por
    /// <see cref="TryGetHandle"/>. Mantendo o componente, o próximo sync materializa outra
    /// constraint; para removê-la definitivamente, remova o componente declarativo.</summary>
    internal static void NotifyJointDestroyed(PhysicsWorld physicsWorld, PhysicsJointHandle handle)
    {
        if (!handle.IsValid || !s_states.TryGetValue(physicsWorld, out var state)) return;
        state.RemovalScratch.Clear();
        foreach (var pair in state.Applied)
            if (pair.Value.Handle == handle) state.RemovalScratch.Add(pair.Key);
        foreach (EntityId owner in state.RemovalScratch) state.Applied.Remove(owner);
    }

    private static bool TryResolve(World world, in Joint joint, out PhysicsBodyHandle body1,
        out PhysicsBodyHandle body2, out JointDesc desc)
    {
        body1 = PhysicsBodyHandle.Invalid;
        body2 = PhysicsBodyHandle.Invalid;
        desc = default;
        if (!joint.TryGetDescription(out desc) ||
            !world.TryGetComponent<RigidBody>(joint.Body1, out var first) ||
            !world.TryGetComponent<RigidBody>(joint.Body2, out var second) ||
            !first.Handle.IsValid || !second.Handle.IsValid)
            return false;
        body1 = first.Handle;
        body2 = second.Handle;
        return true;
    }

    private static void Remove(SyncState state, PhysicsWorld physicsWorld, EntityId owner)
    {
        if (!state.Applied.Remove(owner, out var applied)) return;
        if (applied.Handle.IsValid) physicsWorld.DestroySynchronizedJoint(applied.Handle);
    }

    private static int NextEpoch(SyncState state)
    {
        if (state.Epoch == int.MaxValue)
        {
            state.Epoch = 1;
            // Nenhuma entrada pode coincidir por acidente após wrap.
            var keys = state.Applied.Keys.ToArray();
            foreach (EntityId key in keys)
            {
                var value = state.Applied[key];
                value.SeenEpoch = 0;
                state.Applied[key] = value;
            }
            return state.Epoch;
        }
        return ++state.Epoch;
    }
}
