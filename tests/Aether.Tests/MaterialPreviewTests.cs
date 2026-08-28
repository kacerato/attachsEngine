using Aether.Rendering;
using Aether.Rendering.Diagnostics;
using Aether.Rendering.Interop;
using Aether.Serialization;
using System.Runtime.CompilerServices;
namespace Aether.Tests;
public static class MaterialPreviewTests
{
    [Test]
    public static void Material_StableIdsAndParametersRoundTrip()
    {
        var material=PbrMaterial.MetalPlate;
        var loaded=PbrMaterial.Deserialize(material.Serialize());
        Assert.Equal(material.Id,loaded.Id);Assert.Equal(material.Albedo,loaded.Albedo);
        Assert.Equal(material.Normal,loaded.Normal);
        Assert.Equal(material.OcclusionRoughnessMetallic,loaded.OcclusionRoughnessMetallic);
        Assert.Equal(material.Serialize(),loaded.Serialize());
        Assert.Equal(12,Unsafe.SizeOf<PbrMaterialParameters>());
    }
    [Test]
    public static void Material_RejectsInvalidFactorsAndFutureSchema()
    {
        var source=PbrMaterial.MetalPlate;
        Assert.Throws<FormatException>(()=>PbrMaterial.Deserialize(source.Serialize().Replace("\"Version\":1","\"Version\":2")));
        foreach(float value in new[]{float.NaN,float.PositiveInfinity,-1,2})
            Assert.Throws<ArgumentOutOfRangeException>(()=>new PbrMaterial(source.Id,source.Albedo,source.Normal,
                source.OcclusionRoughnessMetallic,new PbrMaterialParameters{RoughnessFactor=value,MetallicFactor=1,NormalScale=1}));
    }
    [Test]
    public static void Sphere_IsSceneDataAndSurvivesSerialization()
    {
        var scene=new ScenePreview(materialPreview:true);
        Assert.Equal(BuiltinRenderResources.Sphere,scene.World.Read<MeshRenderer>(scene.First).Mesh);
        string text=TextSerializer.Serialize(scene.World);
        var restored=new World();TextSerializer.Deserialize(restored,text);
        Assert.Equal(text,TextSerializer.Serialize(restored));
        var data=new RenderInstance[2];
        foreach(int step in new[]{0,1,2,3}) {
            if(step>0)scene.ApplyValidationStep(step);
            Assert.Equal(RenderExtractionStatus.Ok,scene.Extractor.Extract(data,out int count));
            Assert.Equal(step==2?0:1,count);
        }
    }
    [Test]
    public static void LitGeometry_RejectsSingularNormalMatrix()
    {
        var scene=new ScenePreview(materialPreview:true);
        scene.World.SetComponent(scene.First,new LocalTransform(new Transform(float3.Zero,quaternion.Identity,float3.Zero)));
        Assert.Equal(RenderExtractionStatus.InvalidTransform,scene.Extractor.Extract(new RenderInstance[2],out _));
    }
    [Test]
    public static unsafe void MaterialBoundary_IsIdempotentAndRejectsModeSwitch()
    {
        delegate* unmanaged<int> sphere=&SceneEntryPoints.InitializeMaterialPreview;
        delegate* unmanaged<int> cubes=&SceneEntryPoints.Initialize;
        delegate* unmanaged<void> shutdown=&SceneEntryPoints.Shutdown;
        delegate* unmanaged<PbrMaterialParameters*,int,int> read=&SceneEntryPoints.GetMaterialParameters;
        shutdown();
        try {
            var parameters=default(PbrMaterialParameters);
            Assert.Equal(-6,read(&parameters,12));Assert.Equal(0,sphere());Assert.Equal(0,sphere());
            Assert.Equal(-5,cubes());Assert.Equal(-5,read(&parameters,16));
            Assert.Equal(0,read(&parameters,12));Assert.True(parameters.IsValid);Assert.Equal(1f,parameters.NormalScale);
        } finally {shutdown();}
    }
}
