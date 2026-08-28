namespace Aether.Rendering.Diagnostics;

/// <summary>Fixture de integração, separada do extrator reutilizável e da PoC-A de performance.
/// A criação ocorre uma vez por sessão, nunca durante uma recriação de surface.</summary>
public sealed class ScenePreview
{
    public bool IsMaterialPreview { get; }
    public World World { get; } = new();
    public RenderSceneExtractor Extractor { get; }
    public EntityId Parent { get; }
    public EntityId First { get; private set; }
    public EntityId Second { get; private set; }

    public ScenePreview(bool materialPreview = false)
    {
        IsMaterialPreview = materialPreview;
        RenderingComponents.Register();
        Parent = World.CreateEntity(new LocalTransform(Transform.Identity));
        if (materialPreview)
        {
            First = AddSphere();
            Second = EntityId.Null;
            Hierarchy.SetParent(World, First, Parent);
            Extractor = new RenderSceneExtractor(World, BuiltinRenderResources.Sphere, BuiltinRenderResources.MetalPlateMaterial,
                requireInvertibleTransform: true);
            return;
        }
        First = AddCube(new float3(-1.4f, 0, 0), new float3(0.7f, 1.0f, 0.7f), new float4(1, 0.65f, 0.45f, 1));
        Second = AddCube(new float3(1.3f, 0, 0), new float3(0.6f, 0.6f, 0.6f), new float4(0.5f, 0.75f, 1, 1));
        Hierarchy.SetParent(World, Second, Parent);
        Extractor = new RenderSceneExtractor(World, BuiltinRenderResources.Cube, BuiltinRenderResources.CheckerMaterial);
    }

    public EntityId AddCube(float3 position, float3 scale, float4 tint) => World.CreateEntity(
        new LocalTransform(new Transform(position, quaternion.Identity, scale)),
        new MeshRenderer(BuiltinRenderResources.Cube, BuiltinRenderResources.CheckerMaterial, tint));

    private EntityId AddSphere() => World.CreateEntity(
        new LocalTransform(new Transform(float3.Zero, quaternion.Identity, new float3(1.75f, 1.75f, 1.75f))),
        new MeshRenderer(BuiltinRenderResources.Sphere, BuiltinRenderResources.MetalPlateMaterial, new float4(1,1,1,1)));

    // Exercício determinístico pelo runner, não uma API de edição de produto.
    public void ApplyValidationStep(int step)
    {
        switch (step)
        {
            case 1:
                World.SetComponent(Parent, new LocalTransform(new Transform(new float3(0, 1, 0),
                    quaternion.AxisAngle(float3.Forward, 0.4f), new float3(1.2f, 0.7f, 1))));
                break;
            case 2:
                var target = IsMaterialPreview ? First : Second;
                Hierarchy.Detach(World, target);
                World.DestroyEntity(target);
                break;
            case 3:
                if (IsMaterialPreview) { First = AddSphere(); break; }
                Second = AddCube(new float3(0.7f, -0.4f, -0.5f), new float3(0.9f, 0.3f, 0.5f), new float4(0.6f, 1, 0.55f, 1));
                break;
            default: throw new ArgumentOutOfRangeException(nameof(step));
        }
    }
}
