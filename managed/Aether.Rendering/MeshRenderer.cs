using Aether.Resources;
using Aether.Serialization;

namespace Aether.Rendering;

/// <summary>Dados persistentes, sem handles Vulkan nem referências gerenciadas dentro do ECS.</summary>
public struct MeshRenderer
{
    public ResourceId Mesh;
    public ResourceId Material;
    public float4 Tint;
    public bool Enabled;

    public MeshRenderer(ResourceId mesh, ResourceId material, float4 tint)
    { Mesh = mesh; Material = material; Tint = tint; Enabled = true; }
}

/// <summary>Identidades estáveis do primeiro par de recursos embutidos. Não são handles GPU.</summary>
public static class BuiltinRenderResources
{
    public static readonly ResourceId Cube = new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a01"));
    public static readonly ResourceId CheckerMaterial = new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a02"));
    public static readonly ResourceId Sphere = new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a03"));
    public static readonly ResourceId MetalPlateMaterial = new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a04"));
}

public static class RenderingComponents
{
    private static readonly object Gate = new();
    private static bool _registered;
    public static void Register()
    {
        lock (Gate)
        {
            if (_registered) return;
            ComponentRegistryBootstrap.RegisterBuiltins();
            ComponentRegistry.Register<MeshRenderer>("Aether.Rendering.MeshRenderer", schemaVersion: 1);
            _registered = true;
        }
    }
}
