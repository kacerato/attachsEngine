using Astra.Components;

namespace Astra;

/// <summary>Tipo de superfície do material físico (mesmos valores de
/// <c>scene::physicsSurfaceOptions</c>). Gravado na cena: só cresce no fim.</summary>
public enum PhysicsSurface : uint
{
    Default, Concrete, Wood, Metal, Grass, Dirt, Sand, Water, Ice, Rubber, Glass, Fabric, Stone
}

/// <summary>
/// Superfície do que foi tocado: a forma com material próprio vence; senão,
/// vale a do Corpo físico. Usado para escolher passos, faíscas e sons.
/// </summary>
public static class PhysicsSurfaces
{
    public static PhysicsSurface Surface(this RayHit hit)
    {
        if (hit.Collider is { } collider && collider.OwnMaterial) return (PhysicsSurface)(uint)collider.Surface;
        return Of(hit.BodyObject);
    }

    /// <summary>Superfície do Corpo físico do objeto; Default sem corpo.</summary>
    public static PhysicsSurface Of(GameObject body)
    {
        ArgumentNullException.ThrowIfNull(body);
        return body.TryGetComponent<PhysicsBody>(out var value) ? (PhysicsSurface)(uint)value.Surface : PhysicsSurface.Default;
    }
}
