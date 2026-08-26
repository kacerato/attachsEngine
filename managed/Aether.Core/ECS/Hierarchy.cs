using System.Buffers;

namespace Aether;

/// <summary>Referência ao pai na hierarquia de cena. Ausência do componente = raiz.</summary>
public struct Parent
{
    public EntityId Value;
    public Parent(EntityId value) => Value = value;
}

/// <summary>
/// Primeiro filho direto. Junto com <see cref="NextSibling"/> forma uma lista encadeada de
/// irmãos — a estrutura padrão para hierarquia em ECS.
/// <para>
/// Por que encadeada e não uma lista inline de tamanho fixo: um chunk é um <c>byte[]</c> cru, então
/// nenhum componente pode carregar referência de GC (um <c>List&lt;EntityId&gt;</c> seria corrompido
/// pelo swap-back, que copia bytes). Um array inline resolveria isso mas imporia um teto de filhos —
/// e um teto é inaceitável aqui: o painel de Hierarquia do editor precisa aguentar um nó "Nível" com
/// centenas de objetos, ou uma malha glTF importada com dezenas de submalhas. A lista encadeada é
/// blittable, sem teto, e a inserção no início é O(1).
/// </para>
/// <para>
/// Custo aceito: percorrer os filhos é um salto de ponteiro por filho, e não uma varredura linear.
/// Isso não pesa porque a travessia de hierarquia é operação de editor e de setup, não caminho
/// quente de frame — a propagação de transform varre <see cref="Parent"/> por chunk e nem toca aqui.
/// </para>
/// </summary>
public struct FirstChild
{
    public EntityId Value;
    public FirstChild(EntityId value) => Value = value;
}

/// <summary>Próximo irmão na lista de filhos do mesmo pai. <see cref="EntityId.Null"/> encerra a lista.</summary>
public struct NextSibling
{
    public EntityId Value;
    public NextSibling(EntityId value) => Value = value;
}

/// <summary>Operações de hierarquia que mantêm <see cref="Parent"/>, <see cref="FirstChild"/> e
/// <see cref="NextSibling"/> consistentes entre si. Use estes métodos em vez de mexer nos
/// componentes na mão — a consistência dos três é o que o editor assume.</summary>
public static class Hierarchy
{
    /// <summary>Prende <paramref name="child"/> a <paramref name="parent"/>, destacando-o do pai
    /// anterior se houver. Passar <see cref="EntityId.Null"/> como pai torna a entidade raiz.</summary>
    public static void SetParent(World world, EntityId child, EntityId parent)
    {
        if (!world.Exists(child))
            throw new ArgumentException($"A entidade filha {child} não existe mais.", nameof(child));
        if (parent != EntityId.Null && !world.Exists(parent))
            throw new ArgumentException($"A entidade pai {parent} não existe mais.", nameof(parent));
        if (child == parent)
            throw new ArgumentException("Uma entidade não pode ser pai de si mesma.", nameof(parent));
        if (parent != EntityId.Null && IsDescendantOf(world, parent, child))
            throw new ArgumentException(
                "Não dá para prender uma entidade a um descendente dela mesma — isso criaria um ciclo na hierarquia.",
                nameof(parent));

        Detach(world, child);

        if (parent == EntityId.Null) return;

        EntityId head = world.HasComponent<FirstChild>(parent)
            ? world.Read<FirstChild>(parent).Value
            : EntityId.Null;

        SetOrAdd(world, child, new NextSibling(head));
        SetOrAdd(world, child, new Parent(parent));
        SetOrAdd(world, parent, new FirstChild(child));
    }

    /// <summary>Destaca a entidade do pai atual, costurando a lista de irmãos. Sem efeito se já for raiz.</summary>
    public static void Detach(World world, EntityId child)
    {
        if (!world.HasComponent<Parent>(child)) return;
        EntityId parent = world.Read<Parent>(child).Value;
        EntityId next = world.HasComponent<NextSibling>(child)
            ? world.Read<NextSibling>(child).Value
            : EntityId.Null;

        if (world.Exists(parent) && world.HasComponent<FirstChild>(parent))
        {
            EntityId head = world.Read<FirstChild>(parent).Value;
            if (head == child)
            {
                world.SetComponent(parent, new FirstChild(next));
            }
            else
            {
                // Encontra o irmão anterior e costura por cima do removido.
                EntityId cursor = head;
                while (cursor != EntityId.Null && world.Exists(cursor))
                {
                    EntityId sibling = world.HasComponent<NextSibling>(cursor)
                        ? world.Read<NextSibling>(cursor).Value
                        : EntityId.Null;
                    if (sibling == child) { world.SetComponent(cursor, new NextSibling(next)); break; }
                    cursor = sibling;
                }
            }
        }

        world.RemoveComponent<Parent>(child);
        if (world.HasComponent<NextSibling>(child)) world.RemoveComponent<NextSibling>(child);
    }

    /// <summary>Conta os filhos diretos. O(n) nos filhos.</summary>
    public static int ChildCount(World world, EntityId parent)
    {
        int n = 0;
        foreach (var _ in EnumerateChildren(world, parent)) n++;
        return n;
    }

    /// <summary>Verdadeiro se <paramref name="candidate"/> está em algum ponto abaixo de
    /// <paramref name="ancestor"/> na hierarquia.</summary>
    public static bool IsDescendantOf(World world, EntityId candidate, EntityId ancestor)
    {
        EntityId cursor = candidate;
        for (int guard = 0; guard < MaxDepth && cursor != EntityId.Null; guard++)
        {
            if (!world.HasComponent<Parent>(cursor)) return false;
            cursor = world.Read<Parent>(cursor).Value;
            if (cursor == ancestor) return true;
        }
        return false;
    }

    /// <summary>Itera os filhos diretos sem alocar (enumerador struct).</summary>
    public static ChildEnumerable EnumerateChildren(World world, EntityId parent) => new(world, parent);

    private static void SetOrAdd<T>(World world, EntityId e, in T value) where T : unmanaged
    {
        if (world.HasComponent<T>(e)) world.SetComponent(e, value);
        else world.AddComponent(e, value);
    }

    internal const int MaxDepth = 64;
}

/// <summary>Enumerável struct sobre os filhos diretos — não aloca.</summary>
public readonly struct ChildEnumerable(World world, EntityId parent)
{
    public ChildEnumerator GetEnumerator() => new(world, parent);
}

/// <summary>Enumerador struct sobre a lista encadeada de irmãos.</summary>
public struct ChildEnumerator
{
    private readonly World _world;
    private EntityId _current;
    private int _guard;

    internal ChildEnumerator(World world, EntityId parent)
    {
        _world = world;
        _current = EntityId.Null;
        _guard = 0;
        Next = world.Exists(parent) && world.HasComponent<FirstChild>(parent)
            ? world.Read<FirstChild>(parent).Value
            : EntityId.Null;
    }

    private EntityId Next;

    public readonly EntityId Current => _current;

    public bool MoveNext()
    {
        // O guard corta uma lista de irmãos corrompida em ciclo em vez de travar o editor.
        if (Next == EntityId.Null || !_world.Exists(Next) || ++_guard > 1_000_000) return false;
        _current = Next;
        Next = _world.HasComponent<NextSibling>(_current)
            ? _world.Read<NextSibling>(_current).Value
            : EntityId.Null;
        return true;
    }
}

/// <summary>Transform local, relativo ao pai (ou ao mundo, se raiz). É o que o usuário edita.</summary>
public struct LocalTransform
{
    public Transform Value;
    public LocalTransform(Transform value) => Value = value;
}

/// <summary>
/// Transform final no espaço do mundo, calculado por <see cref="TransformSystem"/>.
/// <para>
/// Guardamos um <see cref="Transform"/> (posição+quaternion+escala, 40 bytes) em vez de um
/// <c>float4x4</c> (64 bytes) por dois motivos: (1) a composição pai→filho via
/// <see cref="Transform.TransformChild"/> é exata para escala uniforme e barata (sem multiplicação
/// de matriz 4x4 completa), o que importa porque isso roda para toda entidade com hierarquia todo
/// frame; (2) é 40% menor, e largura de banda de memória é o recurso escasso citado nas convenções
/// — menos bytes por entidade nesta coluna quente. A matriz final (para upload à GPU) é derivada sob
/// demanda com <see cref="Transform.ToMatrix"/> só no sistema de render, não fica duplicada aqui.
/// </para>
/// </summary>
public struct WorldTransform
{
    public Transform Value;
    public WorldTransform(Transform value) => Value = value;
}

/// <summary>
/// Propaga <see cref="LocalTransform"/> para <see cref="WorldTransform"/> respeitando a hierarquia
/// de <see cref="Parent"/>. A propagação é por níveis (raízes primeiro, depois quem tem pai já
/// resolvido, e assim por diante) em vez de recursiva por entidade — cada nível processa todas as
/// entidades daquele nível varrendo chunks inteiros, o que pode ser paralelizado por chunk mais
/// tarde sem mudar o algoritmo.
/// </summary>
public static class TransformSystem
{
    // Limite de níveis de hierarquia processados por chamada: corta um ciclo pai/filho acidental
    // (que nunca deveria existir, mas não pode travar o editor num loop infinito).
    private const int MaxDepth = 64;

    public static void Propagate(World world)
    {
        int capacity = world.Capacity;
        if (capacity == 0) return;

        var ready = ArrayPool<bool>.Shared.Rent(capacity);
        Array.Clear(ready, 0, capacity);
        try
        {
            foreach (var chunk in world.Query().With<LocalTransform>().With<WorldTransform>().Without<Parent>())
            {
                var local = chunk.GetReadOnlySpan<LocalTransform>();
                var world_ = chunk.GetWritableSpan<WorldTransform>();
                var entities = chunk.Entities;
                for (int i = 0; i < chunk.Count; i++)
                {
                    world_[i].Value = local[i].Value;
                    ready[entities[i].Index] = true;
                }
            }

            bool progressed = true;
            for (int pass = 0; pass < MaxDepth && progressed; pass++)
            {
                progressed = false;
                foreach (var chunk in world.Query().With<Parent>().With<LocalTransform>().With<WorldTransform>())
                {
                    var parents = chunk.GetReadOnlySpan<Parent>();
                    var local = chunk.GetReadOnlySpan<LocalTransform>();
                    var world_ = chunk.GetWritableSpan<WorldTransform>();
                    var entities = chunk.Entities;

                    for (int i = 0; i < chunk.Count; i++)
                    {
                        int selfIndex = entities[i].Index;
                        if (ready[selfIndex]) continue;

                        var parentId = parents[i].Value;
                        if (!world.Exists(parentId))
                        {
                            // Pai destruído (ou nunca existiu): trata como raiz em vez de travar a propagação.
                            world_[i].Value = local[i].Value;
                            ready[selfIndex] = true;
                            progressed = true;
                            continue;
                        }

                        if (!ready[parentId.Index]) continue;   // pai ainda não resolvido nesta passada

                        var parentWorld = world.Read<WorldTransform>(parentId);
                        world_[i].Value = parentWorld.Value.TransformChild(local[i].Value);
                        ready[selfIndex] = true;
                        progressed = true;
                    }
                }
            }
        }
        finally
        {
            ArrayPool<bool>.Shared.Return(ready);
        }
    }
}
