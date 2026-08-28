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
        if (parent != EntityId.Null)
            ValidateParentChain(world, child, parent);

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

    /// <summary>Valida ciclo e profundidade antes de alterar qualquer componente. O limite torna
    /// o custo de propagação previsível no mobile e impede que uma cadeia corrompida esconda um
    /// ciclo além do guard de <see cref="IsDescendantOf"/>.</summary>
    private static void ValidateParentChain(World world, EntityId child, EntityId parent)
    {
        EntityId cursor = parent;
        int resultingDepth = 1; // primeira aresta: child -> parent
        while (cursor != EntityId.Null)
        {
            if (cursor == child)
                throw new ArgumentException(
                    "Não dá para prender uma entidade a um descendente dela mesma — isso criaria um ciclo na hierarquia.",
                    nameof(parent));
            if (!world.HasComponent<Parent>(cursor)) return;
            if (resultingDepth >= MaxDepth)
                throw new ArgumentException(
                    $"A hierarquia aceita no máximo {MaxDepth} níveis; este reparent excederia o limite.",
                    nameof(parent));
            cursor = world.Read<Parent>(cursor).Value;
            resultingDepth++;
        }
    }

    /// <summary>Itera os filhos diretos sem alocar (enumerador struct).</summary>
    public static ChildEnumerable EnumerateChildren(World world, EntityId parent) => new(world, parent);

    private static void SetOrAdd<T>(World world, EntityId e, in T value) where T : unmanaged
    {
        if (world.HasComponent<T>(e)) world.SetComponent(e, value);
        else world.AddComponent(e, value);
    }

    /// <summary>Profundidade máxima suportada. É pública para importadores e ferramentas poderem
    /// validar conteúdo antes de tentar alterar a cena.</summary>
    public const int MaxDepth = 64;
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
/// de <see cref="Parent"/>. Um plano topológico é recompilado somente após mudanças estruturais ou
/// de parentesco. Em frames estáveis, locais são reunidos em um lote denso, compostos por um único
/// kernel nativo quando disponível e gravados de volta nas colunas ECS; o fallback gerenciado tem
/// exatamente a mesma ordem e semântica.
/// </summary>
public static class TransformSystem
{
    /// <summary>Matriz afim exata de uma entidade. Diferente da decomposição TRS de WorldTransform,
    /// preserva shear produzido por escala não uniforme de um pai e rotação do filho.
    /// Leitura sem mutação/alocação, O(profundidade), no máximo MaxDepth arestas.
    /// A árvore deve permanecer imutável durante a chamada.</summary>
    public static bool TryGetWorldMatrix(World world, EntityId entity, out float4x4 matrix)
    {
        ArgumentNullException.ThrowIfNull(world);
        matrix = float4x4.Identity;
        for (int depth = 0; depth <= Hierarchy.MaxDepth; ++depth)
        {
            if (!world.Exists(entity) || !world.HasComponent<LocalTransform>(entity)) return false;
            matrix = world.Read<LocalTransform>(entity).Value.ToMatrix() * matrix;
            if (!world.HasComponent<Parent>(entity)) return true;
            entity = world.Read<Parent>(entity).Value;
            if (entity == EntityId.Null) return true;
        }
        return false;
    }

    /// <summary>Backend usado na última propagação, disponível para profiler e diagnóstico.</summary>
    public static TransformPropagationBackend ActiveBackend => NativeTransformKernel.ActiveBackend;

    public static void Propagate(World world)
    {
        TransformPropagationPlanCache.Propagate(world);
    }
}
