using Aether.Rendering;
using Aether.Resources;
using Aether.Serialization;

namespace Aether.Tests;

public static class WaterTests
{
    [Test]
    public static void ProfileRoundTripPreservesAuthoredAxes()
    {
        var id=new ResourceId(Guid.Parse("8b4c7981-1022-4b2f-a0b2-0712b89a6201"));
        var profile=new WaterProfile(id,WaterDomain.InfiniteOcean,WaterReflection.ScreenSpace,
            [new WaterWave(1,0,.5f,8,1.2f,.35f,.1f)],new(.01f,.1f,.2f),new(.1f,.5f,.6f),
            new(.2f,.06f,.03f));
        var loaded=WaterProfile.Deserialize(profile.Serialize());
        Assert.Equal(profile.Id,loaded.Id);Assert.Equal(profile.Reflection,loaded.Reflection);
        Assert.Equal(1,loaded.Waves.Count);Assert.Equal(profile.Waves[0],loaded.Waves[0]);
    }

    [Test]
    public static void ComponentIsReflectedAndUsesStableResourceIdentity()
    {
        RenderingComponents.Register();
        var descriptor=ComponentRegistry.GetByType(ComponentType.Of<WaterSurface>());
        Assert.Equal("Aether.Rendering.WaterSurface",descriptor.Name);
        Assert.True(descriptor.Fields.Any(f=>f.Path=="Profile.Value"));
        Assert.True(descriptor.Fields.Any(f=>f.Path=="BaseHeight"));
        Assert.Throws<ArgumentException>(()=>new WaterSurface(default,0,new(10,10)));
    }

    [Test]
    public static void ProfileRejectsNonNormalizedDirections()
    {
        var id=new ResourceId(Guid.NewGuid());
        Assert.Throws<ArgumentOutOfRangeException>(()=>new WaterProfile(id,WaterDomain.InfiniteOcean,
            WaterReflection.Environment,[new WaterWave(2,0,.5f,8,1,.3f,0)],new(0,0,0),
            new(0,0,0),new(0,0,0)));
    }
}
