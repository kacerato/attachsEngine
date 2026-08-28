using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Aether.Rendering;
using Aether.Rendering.Diagnostics;
using Aether.Rendering.Interop;
using Aether.Resources;
using Aether.Serialization;

namespace Aether.Tests;

public static class RenderSceneTests
{
    private static RenderInstance[] Extract(ScenePreview scene)
    {
        var data = new RenderInstance[4];
        Assert.Equal(RenderExtractionStatus.Ok, scene.Extractor.Extract(data, out int count));
        return data[..count];
    }

    [Test]
    public static void Abi_LayoutIsBlittableAndColumnMajor()
    {
        Assert.Equal(88, Unsafe.SizeOf<RenderInstance>());
        Assert.False(RuntimeHelpers.IsReferenceOrContainsReferences<RenderInstance>());
        Assert.Equal(64, (int)Marshal.OffsetOf<RenderInstance>(nameof(RenderInstance.Tint)));
        Assert.Equal(80, (int)Marshal.OffsetOf<RenderInstance>(nameof(RenderInstance.Entity)));
        var value = new RenderInstance { Model = float4x4.Translate(new float3(7, 8, 9)), Entity = new EntityId(13, 21) };
        var bytes = MemoryMarshal.AsBytes(MemoryMarshal.CreateSpan(ref value, 1));
        Assert.Equal(7f, BitConverter.ToSingle(bytes[48..]));
        Assert.Equal(13, BitConverter.ToInt32(bytes[80..]));
        Assert.Equal(21, BitConverter.ToInt32(bytes[84..]));
    }

    [Test]
    public static void Scene_HierarchyMutationDeletionAndGenerationReachTheBatch()
    {
        var scene = new ScenePreview();
        var before = Extract(scene);
        var oldSecond = scene.Second;
        Assert.Equal(2, before.Length);
        scene.ApplyValidationStep(1);
        var after = Extract(scene);
        Assert.Equal(before.Single(x => x.Entity == scene.First).Model, after.Single(x => x.Entity == scene.First).Model);
        Assert.NotEqual(before.Single(x => x.Entity == oldSecond).Model, after.Single(x => x.Entity == oldSecond).Model);
        scene.ApplyValidationStep(2);
        Assert.Equal(1, Extract(scene).Length);
        Assert.False(scene.World.Exists(oldSecond));
        scene.ApplyValidationStep(3);
        Assert.Equal(2, Extract(scene).Length);
        Assert.NotEqual(oldSecond, scene.Second);
        Assert.False(Extract(scene).Any(x => x.Entity == oldSecond));
    }

    [Test]
    public static void Matrix_PreservesAffineShearAndEveryAncestor()
    {
        var scene = new ScenePreview();
        var parent = new Transform(new float3(2, 3, 4), quaternion.AxisAngle(float3.Up, 0.7f), new float3(2, 3, 4));
        var child = new Transform(new float3(5, -2, 1), quaternion.AxisAngle(float3.Forward, 0.6f), new float3(1, 2, -1));
        scene.World.SetComponent(scene.Parent, new LocalTransform(parent));
        scene.World.SetComponent(scene.Second, new LocalTransform(child));
        Assert.True(TransformSystem.TryGetWorldMatrix(scene.World, scene.Second, out var actual));
        Assert.Equal(parent.ToMatrix() * child.ToMatrix(), actual);
        Assert.Equal(actual, Extract(scene).Single(x => x.Entity == scene.Second).Model);
    }

    [Test]
    public static void Matrix_RejectsMissingTransformDanglingParentAndCycle()
    {
        var world = new World();
        var a = world.CreateEntity(new LocalTransform(Transform.Identity));
        var b = world.CreateEntity(new LocalTransform(Transform.Identity));
        Assert.False(TransformSystem.TryGetWorldMatrix(world, EntityId.Null, out _));
        world.AddComponent(a, new Parent(b));
        world.AddComponent(b, new Parent(a)); // corrupted/raw data, bypasses hierarchy API
        Assert.False(TransformSystem.TryGetWorldMatrix(world, a, out _));
        world.DestroyEntity(b);
        Assert.False(TransformSystem.TryGetWorldMatrix(world, a, out _));
        world.RemoveComponent<LocalTransform>(a);
        Assert.False(TransformSystem.TryGetWorldMatrix(world, a, out _));
    }

    [Test]
    public static void Extraction_TooSmallDoesNotWriteAndReportsRequiredCount()
    {
        var scene = new ScenePreview();
        var data = new[] { new RenderInstance { Entity = new EntityId(123, 456) } };
        Assert.Equal(RenderExtractionStatus.BufferTooSmall, scene.Extractor.Extract(data, out int count));
        Assert.Equal(2, count);
        Assert.Equal(new EntityId(123, 456), data[0].Entity);
    }

    [Test]
    public static void Extraction_DisabledIsExcludedAndTailUntouched()
    {
        var scene = new ScenePreview();
        var renderer = scene.World.Read<MeshRenderer>(scene.First);
        renderer.Enabled = false;
        scene.World.SetComponent(scene.First, renderer);
        var data = new RenderInstance[3];
        data[1].Entity = new EntityId(123, 456);
        Assert.Equal(RenderExtractionStatus.Ok, scene.Extractor.Extract(data, out int count));
        Assert.Equal(1, count);
        Assert.Equal(scene.Second, data[0].Entity);
        Assert.Equal(new EntityId(123, 456), data[1].Entity);
    }

    [Test]
    public static void Extraction_UnsupportedResourceNeverSubstitutesBuiltin()
    {
        var scene = new ScenePreview();
        var renderer = scene.World.Read<MeshRenderer>(scene.Second);
        renderer.Mesh = new ResourceId(Guid.NewGuid());
        scene.World.SetComponent(scene.Second, renderer);
        Assert.Equal(RenderExtractionStatus.ResourceUnavailable, scene.Extractor.Extract(new RenderInstance[4], out _));
    }

    [Test]
    public static void Extraction_InvalidTransformLeavesDestinationUntouched()
    {
        var scene = new ScenePreview();
        scene.World.SetComponent(scene.Second, new LocalTransform(new Transform(
            new float3(float.NaN, 0, 0), quaternion.Identity, float3.One)));
        var data = new RenderInstance[4];
        data[0].Entity = new EntityId(123, 456);
        Assert.Equal(RenderExtractionStatus.InvalidTransform, scene.Extractor.Extract(data, out _));
        Assert.Equal(new EntityId(123, 456), data[0].Entity);
    }

    [Test]
    public static void Extraction_MissingTransformIsNotSilentlySkipped()
    {
        var scene = new ScenePreview();
        scene.World.RemoveComponent<LocalTransform>(scene.First);
        Assert.Equal(RenderExtractionStatus.InvalidTransform, scene.Extractor.Extract(new RenderInstance[4], out _));
    }

    [Test]
    public static void Extraction_OpaqueContractRejectsAlphaAndNonFiniteTint()
    {
        var scene = new ScenePreview();
        foreach (var tint in new[] { new float4(1, 1, 1, 0.5f), new float4(float.PositiveInfinity, 1, 1, 1) })
        {
            var renderer = scene.World.Read<MeshRenderer>(scene.First);
            renderer.Tint = tint;
            scene.World.SetComponent(scene.First, renderer);
            Assert.Equal(RenderExtractionStatus.InvalidTint, scene.Extractor.Extract(new RenderInstance[4], out _));
        }
    }

    [Test]
    public static void Extraction_AllocatesZeroAfterWarmupIncludingStructuralChanges()
    {
        var scene = new ScenePreview();
        scene.ApplyValidationStep(2);
        scene.ApplyValidationStep(3);
        var data = new RenderInstance[4];
        Assert.NoAlloc(() =>
        {
            for (int i = 0; i < 120; ++i)
                if (scene.Extractor.Extract(data, out int count) != RenderExtractionStatus.Ok || count != 2)
                    throw new InvalidOperationException("Invalid extraction.");
        });
    }

    [Test]
    public static void Metadata_ResourceIdsAreGuidLeaves()
    {
        RenderingComponents.Register();
        var descriptor = ComponentRegistry.GetByType(ComponentType.Of<MeshRenderer>());
        Assert.Equal(ComponentFieldKind.Guid, descriptor.Fields.Single(x => x.Path == "Mesh.Value").Kind);
        Assert.Equal(ComponentFieldKind.Guid, descriptor.Fields.Single(x => x.Path == "Material.Value").Kind);
        Assert.Equal(7, descriptor.Fields.Length);
    }

    [Test]
    public static void Serialization_TextAndBinaryPreserveResourcesTintEnabledAndHierarchy()
    {
        var scene = new ScenePreview();
        scene.ApplyValidationStep(1);
        var renderer = scene.World.Read<MeshRenderer>(scene.First);
        renderer.Enabled = false;
        scene.World.SetComponent(scene.First, renderer);
        string text = TextSerializer.Serialize(scene.World);
        Assert.True(text.Contains(BuiltinRenderResources.Cube.Value.ToString("D")));
        var restored = new World();
        TextSerializer.Deserialize(restored, text);
        Assert.Equal(text, TextSerializer.Serialize(restored));
        using var stream = new MemoryStream();
        BinarySerializer.Write(scene.World, stream);
        stream.Position = 0;
        var binary = new World();
        BinarySerializer.Read(binary, stream);
        Assert.Equal(text, TextSerializer.Serialize(binary));
        foreach (var world in new[] { restored, binary })
        {
            var extractor = new RenderSceneExtractor(world, BuiltinRenderResources.Cube, BuiltinRenderResources.CheckerMaterial);
            var data = new RenderInstance[4];
            Assert.Equal(RenderExtractionStatus.Ok, extractor.Extract(data, out int count));
            Assert.Equal(1, count);
            Assert.Equal(Extract(scene)[0].Model, data[0].Model);
        }
    }

    [Test]
    public static void Serialization_RejectsMalformedResourceGuid()
    {
        var scene = new ScenePreview();
        string text = TextSerializer.Serialize(scene.World).Replace(BuiltinRenderResources.Cube.Value.ToString("D"), "invalid-guid");
        Assert.Throws<FormatException>(() => TextSerializer.Deserialize(new World(), text));
    }

    [Test]
    public static unsafe void EntryPoints_ValidateAbiAndKeepWorldAcrossRendererRebuild()
    {
        delegate* unmanaged<int> initialize = &SceneEntryPoints.Initialize;
        delegate* unmanaged<RenderInstance*, int, int, int, int> extract = &SceneEntryPoints.Extract;
        delegate* unmanaged<int, int> step = &SceneEntryPoints.ApplyValidationStep;
        delegate* unmanaged<void> shutdown = &SceneEntryPoints.Shutdown;
        shutdown();
        RenderInstance* buffer = stackalloc RenderInstance[4];
        Assert.Equal(-6, extract(buffer, 4, 88, 1));
        Assert.Equal(0, initialize());
        try
        {
            Assert.Equal(-5, extract(buffer, 4, 20, 1));
            Assert.Equal(-5, extract(buffer, 4, 88, 2));
            Assert.Equal(-5, extract(null, 4, 88, 1));
            Assert.Equal(-5, extract(buffer, -1, 88, 1));
            Assert.Equal(-1, extract(buffer, 0, 88, 1));
            Assert.Equal(2, extract(buffer, 4, 88, 1));
            Assert.Equal(0, step(2));
            Assert.Equal(0, initialize()); // surface recreation must not recreate scene
            Assert.Equal(1, extract(buffer, 4, 88, 1));
        }
        finally { shutdown(); }
        Assert.Equal(-6, extract(buffer, 4, 88, 1));
    }
}
