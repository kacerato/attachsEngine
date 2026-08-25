namespace Aether;

/// <summary>
/// Grava operações estruturais (criar, destruir, adicionar/remover componente) para aplicar depois,
/// num ponto de sincronização — necessário porque estruturar o mundo (mover entidades entre
/// arquétipos) durante a iteração de uma <see cref="World.QueryEnumerable"/> invalidaria os spans
/// em uso. Uma entidade criada no próprio buffer recebe um id temporário de índice negativo,
/// resolvido para o id real no <see cref="Playback"/>.
/// <para>
/// Nota de desempenho: gravar um comando aqui aloca (o delegate fecha sobre o valor do
/// componente). Isso é aceitável porque o ECB é o mecanismo *explícito* de mudança estrutural —
/// ele já não roda no caminho quente de iteração por span, que é o único trecho coberto pela
/// garantia de zero alocação da Regra 3 das convenções.
/// </para>
/// </summary>
public sealed class EntityCommandBuffer
{
    private readonly List<Action<World, EntityId[]>> _ops = new();
    private int _tempCount;

    /// <summary>Registra a criação de uma entidade. Devolve um id temporário (índice negativo,
    /// nunca colide com um id real) que pode ser passado para outras chamadas neste mesmo buffer
    /// antes do playback — por exemplo para montar uma hierarquia pai/filho ainda não existente.</summary>
    public EntityId CreateEntity()
    {
        int slot = _tempCount++;
        var temp = new EntityId(-(slot + 1), 0);
        _ops.Add((world, temps) => temps[slot] = world.CreateEntity());
        return temp;
    }

    public void DestroyEntity(EntityId id)
    {
        _ops.Add((world, temps) =>
        {
            var real = Resolve(id, temps);
            if (world.Exists(real)) world.DestroyEntity(real);
        });
    }

    public void AddComponent<T>(EntityId id, in T value) where T : unmanaged
    {
        T captured = value;
        _ops.Add((world, temps) =>
        {
            var real = Resolve(id, temps);
            world.AddComponent(real, captured);
        });
    }

    /// <summary>
    /// Variante de <see cref="AddComponent{T}(EntityId,in T)"/> para quando o próprio valor do
    /// componente referencia uma entidade temporária deste buffer (ex.: <c>new Parent(idTemp)</c>).
    /// <paramref name="factory"/> roda no playback e recebe um resolvedor que converte qualquer id
    /// temporário (índice negativo) — inclusive o do próprio buffer — no id real correspondente.
    /// </summary>
    public void AddComponent<T>(EntityId id, Func<Func<EntityId, EntityId>, T> factory) where T : unmanaged
    {
        _ops.Add((world, temps) =>
        {
            var real = Resolve(id, temps);
            EntityId ResolveRef(EntityId maybeTemp) => Resolve(maybeTemp, temps);
            world.AddComponent(real, factory(ResolveRef));
        });
    }

    public void SetComponent<T>(EntityId id, in T value) where T : unmanaged
    {
        T captured = value;
        _ops.Add((world, temps) =>
        {
            var real = Resolve(id, temps);
            world.SetComponent(real, captured);
        });
    }

    public void RemoveComponent<T>(EntityId id) where T : unmanaged
    {
        _ops.Add((world, temps) =>
        {
            var real = Resolve(id, temps);
            world.RemoveComponent<T>(real);
        });
    }

    /// <summary>Aplica todos os comandos gravados, na ordem de gravação, e limpa o buffer.</summary>
    public void Playback(World world)
    {
        var temps = _tempCount > 0 ? new EntityId[_tempCount] : Array.Empty<EntityId>();
        foreach (var op in _ops) op(world, temps);
        _ops.Clear();
        _tempCount = 0;
    }

    private static EntityId Resolve(EntityId id, EntityId[] temps) =>
        id.Index < 0 ? temps[-id.Index - 1] : id;
}
