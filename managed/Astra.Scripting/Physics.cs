using System.Numerics;
using System.Runtime.InteropServices;

namespace Astra;

/// <summary>
/// O resultado cru de uma consulta, exatamente como a fronteira nativa o entrega
/// (<c>ae::scene::ScriptQueryHit</c>). Blittable de propósito: é o que atravessa
/// a fronteira sem marshalling. Os comportamentos do projeto usam
/// <see cref="RayHit"/>, que já resolve a identidade do objeto.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct RawQueryHit
{
    public ulong Object;
    public ulong Collider;
    public float PointX, PointY, PointZ;
    public float NormalX, NormalY, NormalZ;
    public float Distance;
    public float Fraction;
    public uint Flags;
    public uint Reserved;

    public const uint HasNormalFlag = 1;
    public const uint SensorFlag = 2;
}

/// <summary>Como a consulta escolhe o que pode acertar.</summary>
public struct QueryFilter
{
    /// <summary>Bit por camada de gameplay (0..31).</summary>
    public uint LayerMask;
    public bool IncludeStatic;
    public bool IncludeDynamic;
    /// <summary>
    /// Falso por padrão: um raio de visada que parasse na zona de detecção
    /// invisível do próprio jogo seria um erro difícil de enxergar.
    /// </summary>
    public bool IncludeSensors;
    /// <summary>Objeto que não deve ser acertado — normalmente quem consulta.</summary>
    public ulong Ignore;

    public static QueryFilter Default => new()
    {
        LayerMask = 0xffffffffu,
        IncludeStatic = true,
        IncludeDynamic = true,
        IncludeSensors = false,
        Ignore = 0,
    };

    public QueryFilter Ignoring(GameObject self) { Ignore = self.ObjectId; return this; }
    public QueryFilter OnlyLayers(params int[] layers)
    {
        LayerMask = 0;
        foreach (var layer in layers) if (layer is >= 0 and < 32) LayerMask |= 1u << layer;
        return this;
    }
}

public enum QueryShapeKind : uint { Box = 0, Sphere = 1, Capsule = 2 }

/// <summary>A forma varrida ou sobreposta por uma consulta.</summary>
public struct ShapeQuery
{
    public QueryShapeKind Kind;
    public Vector3 HalfExtent;
    public float Radius;
    public float HalfHeight;
    public Quaternion Rotation;

    public static ShapeQuery Sphere(float radius) =>
        new() { Kind = QueryShapeKind.Sphere, Radius = radius, HalfExtent = new(radius), HalfHeight = radius, Rotation = Quaternion.Identity };
    public static ShapeQuery Box(Vector3 halfExtent, Quaternion? rotation = null) =>
        new() { Kind = QueryShapeKind.Box, HalfExtent = halfExtent, Radius = halfExtent.X, HalfHeight = halfExtent.Y, Rotation = rotation ?? Quaternion.Identity };
    public static ShapeQuery Capsule(float radius, float halfHeight, Quaternion? rotation = null) =>
        new() { Kind = QueryShapeKind.Capsule, Radius = radius, HalfHeight = halfHeight, HalfExtent = new(radius), Rotation = rotation ?? Quaternion.Identity };
}

/// <summary>
/// Um acerto já resolvido: objeto vivo, instância do colisor que respondeu e
/// contato. <see cref="Normal"/> é nulo quando o backend não tem uma normal
/// para oferecer — uma sobreposição parada não tem direção ao longo de quê, e
/// um vetor zero no lugar pareceria um contato de frente.
/// </summary>
public readonly record struct RayHit(
    GameObject Object,
    ulong ColliderInstance,
    Vector3 Point,
    Vector3? Normal,
    float Distance,
    float Fraction,
    bool IsSensor);

/// <summary>
/// Um contato sólido entregue a um comportamento. Chega aos DOIS objetos do par,
/// cada um recebendo o outro.
/// </summary>
public readonly record struct Collision(ObjectReference Other, Vector3? Normal);

/// <summary>
/// As consultas físicas vistas pelo projeto. Resolvidas contra o mundo de
/// execução: um objeto destruído nunca aparece em um resultado, e a identidade
/// devolvida já carrega geração.
/// </summary>
public readonly struct PhysicsAccess(ISceneAccess scene)
{
    private const int DefaultCapacity = 32;

    /// <summary>
    /// O acerto mais próximo ao longo de <paramref name="direction"/>, cujo
    /// comprimento é o alcance. Nulo quando nada foi acertado.
    /// </summary>
    public RayHit? RayCast(Vector3 origin, Vector3 direction, QueryFilter? filter = null)
    {
        Span<RawQueryHit> single = stackalloc RawQueryHit[1];
        return scene.RayCast(origin, direction, filter ?? QueryFilter.Default, single) > 0 ? Resolve(single[0]) : null;
    }

    /// <summary>
    /// Todos os acertos, do mais próximo ao mais distante. Devolve a lista
    /// efetivamente copiada; <paramref name="truncated"/> diz se havia mais
    /// acertos do que o buffer comportava.
    /// </summary>
    public IReadOnlyList<RayHit> RayCastAll(Vector3 origin, Vector3 direction, out bool truncated,
                                            QueryFilter? filter = null, int capacity = DefaultCapacity)
    {
        capacity = Math.Clamp(capacity, 1, 4096);
        var buffer = new RawQueryHit[capacity];
        var total = scene.RayCast(origin, direction, filter ?? QueryFilter.Default, buffer);
        truncated = total > capacity;
        return Collect(buffer, total, capacity);
    }

    public RayHit? ShapeCast(ShapeQuery shape, Vector3 origin, Vector3 direction, QueryFilter? filter = null) =>
        scene.ShapeCast(shape, origin, direction, filter ?? QueryFilter.Default, out var hit) > 0 ? Resolve(hit) : null;

    public IReadOnlyList<RayHit> Overlap(ShapeQuery shape, Vector3 origin, out bool truncated,
                                         QueryFilter? filter = null, int capacity = DefaultCapacity)
    {
        capacity = Math.Clamp(capacity, 1, 4096);
        var buffer = new RawQueryHit[capacity];
        var total = scene.Overlap(shape, origin, filter ?? QueryFilter.Default, buffer);
        truncated = total > capacity;
        return Collect(buffer, total, capacity);
    }

    /// <summary>Índice da camada de gameplay com esse nome, ou -1.</summary>
    public int LayerByName(string name) => scene.LayerByName(name);
    public string LayerName(int layer) => layer is >= 0 and < 32 ? scene.LayerName((uint)layer) : string.Empty;
    /// <summary>Máscara com apenas as camadas nomeadas informadas.</summary>
    public uint LayerMask(params string[] names)
    {
        uint mask = 0;
        foreach (var name in names)
        {
            var layer = LayerByName(name);
            if (layer >= 0) mask |= 1u << layer;
        }
        return mask;
    }

    private List<RayHit> Collect(RawQueryHit[] buffer, int total, int capacity)
    {
        var copied = Math.Min(total, capacity);
        var results = new List<RayHit>(Math.Max(copied, 0));
        for (var index = 0; index < copied; ++index) results.Add(Resolve(buffer[index]));
        return results;
    }

    private RayHit Resolve(in RawQueryHit hit) => new(
        GameObject.Resolve(scene, hit.Object),
        hit.Collider,
        new Vector3(hit.PointX, hit.PointY, hit.PointZ),
        (hit.Flags & RawQueryHit.HasNormalFlag) != 0 ? new Vector3(hit.NormalX, hit.NormalY, hit.NormalZ) : null,
        hit.Distance,
        hit.Fraction,
        (hit.Flags & RawQueryHit.SensorFlag) != 0);
}
