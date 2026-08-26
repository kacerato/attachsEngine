namespace Aether;

/// <summary>
/// Fachada amigável e sem ownership sobre uma entidade do ECS. Um <see cref="Node"/> não guarda
/// componentes nem lifecycle próprios: conserva apenas o mundo e o <see cref="EntityId"/> com
/// geração, portanto todas as operações continuam passando pelas invariantes do <see cref="World"/>.
/// Cópias do struct apontam para a mesma entidade e se tornam inválidas juntas quando ela morre.
/// </summary>
public readonly struct Node : IEquatable<Node>
{
    private readonly World? _world;

    public EntityId Id { get; }
    public bool IsValid => _world is not null && _world.Exists(Id);

    internal Node(World world, EntityId id)
    {
        _world = world;
        Id = id;
    }

    public bool Has<T>() where T : unmanaged => RequireWorld().HasComponent<T>(Id);
    public ref readonly T Read<T>() where T : unmanaged => ref RequireWorld().Read<T>(Id);
    public ref T Write<T>() where T : unmanaged => ref RequireWorld().Write<T>(Id);
    public void Set<T>(in T value) where T : unmanaged => RequireWorld().SetComponent(Id, value);
    public void Add<T>(in T value) where T : unmanaged => RequireWorld().AddComponent(Id, value);
    public void Remove<T>() where T : unmanaged => RequireWorld().RemoveComponent<T>(Id);

    /// <summary>O pai atual, ou <c>default(Node)</c> quando este nó é raiz ou o pai morreu.</summary>
    public Node Parent
    {
        get
        {
            var world = RequireWorld();
            if (!world.HasComponent<Parent>(Id)) return default;
            EntityId parentId = world.Read<Parent>(Id).Value;
            return world.Exists(parentId) ? new Node(world, parentId) : default;
        }
    }

    /// <summary>Filhos diretos em ordem da lista de hierarquia, sem alocação.</summary>
    public NodeChildren Children => new(RequireWorld(), Id);

    /// <summary>Reparenta mantendo os três componentes de hierarquia consistentes. Um nó default
    /// remove o pai; um nó de outro mundo é recusado explicitamente.</summary>
    public void SetParent(Node parent)
    {
        var world = RequireWorld();
        if (parent._world is not null && !ReferenceEquals(parent._world, world))
            throw new ArgumentException("Pai e filho precisam pertencer ao mesmo World.", nameof(parent));
        Hierarchy.SetParent(world, Id, parent._world is null ? EntityId.Null : parent.RequireId());
    }

    public void Destroy() => RequireWorld().DestroyEntity(Id);

    private World RequireWorld()
    {
        if (_world is null)
            throw new InvalidOperationException("Este Node é vazio e não pertence a um World.");
        if (!_world.Exists(Id))
            throw new InvalidOperationException($"O Node {Id} não existe ou já foi destruído.");
        return _world;
    }

    private EntityId RequireId()
    {
        _ = RequireWorld();
        return Id;
    }

    public bool Equals(Node other) => ReferenceEquals(_world, other._world) && Id == other.Id;
    public override bool Equals(object? obj) => obj is Node other && Equals(other);
    public override int GetHashCode() => HashCode.Combine(_world, Id);
    public static bool operator ==(Node left, Node right) => left.Equals(right);
    public static bool operator !=(Node left, Node right) => !left.Equals(right);
    public override string ToString() => _world is null ? "Node(null)" : $"Node({Id})";
}

/// <summary>Enumerável struct dos filhos diretos de um <see cref="Node"/>.</summary>
public readonly struct NodeChildren
{
    private readonly World _world;
    private readonly EntityId _parent;

    internal NodeChildren(World world, EntityId parent)
    {
        _world = world;
        _parent = parent;
    }

    public NodeChildEnumerator GetEnumerator() => new(_world, _parent);
}

/// <summary>Adaptador sem alocação do enumerador de ids da hierarquia para nós.</summary>
public struct NodeChildEnumerator
{
    private readonly World _world;
    private ChildEnumerator _ids;

    internal NodeChildEnumerator(World world, EntityId parent)
    {
        _world = world;
        _ids = new ChildEnumerator(world, parent);
    }

    public readonly Node Current => new(_world, _ids.Current);
    public bool MoveNext() => _ids.MoveNext();
}
