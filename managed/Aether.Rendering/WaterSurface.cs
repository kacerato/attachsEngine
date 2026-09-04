using System.Text.Json;
using Aether.Resources;
using Aether.Serialization;

namespace Aether.Rendering;

public enum WaterDomain : uint { InfiniteOcean, FiniteSurface, RiverSpline }
public enum WaterReflection : uint { Environment, ScreenSpace, Planar }

/// <summary>Persistent scene component. The profile owns reusable wave/optical
/// data; the component owns placement and per-instance visibility only.</summary>
public struct WaterSurface
{
    public ResourceId Profile;
    public float BaseHeight;
    public float2 Extent;
    public uint RenderLayer;
    public bool Enabled;

    public WaterSurface(ResourceId profile, float baseHeight, float2 extent, uint renderLayer = 1)
    {
        if (profile.IsEmpty) throw new ArgumentException("Water profile needs a stable resource ID.", nameof(profile));
        if (!float.IsFinite(baseHeight) || !float.IsFinite(extent.X) || !float.IsFinite(extent.Y) ||
            extent.X <= 0 || extent.Y <= 0) throw new ArgumentOutOfRangeException(nameof(extent));
        Profile=profile;BaseHeight=baseHeight;Extent=extent;RenderLayer=renderLayer;Enabled=true;
    }
}

public readonly record struct WaterWave(float DirectionX, float DirectionZ, float Amplitude,
    float Wavelength, float Speed, float Steepness, float Phase)
{
    public bool IsValid {
        get {
            float directionLength=DirectionX*DirectionX+DirectionZ*DirectionZ;
            return float.IsFinite(directionLength) && directionLength is >=.999f and <=1.001f &&
                float.IsFinite(Amplitude) && Amplitude is >=0 and <=20 &&
                float.IsFinite(Wavelength) && Wavelength is >=.05f and <=10000 &&
                float.IsFinite(Speed) && MathF.Abs(Speed)<=100 &&
                float.IsFinite(Steepness) && Steepness is >=0 and <=1 && float.IsFinite(Phase);
        }
    }
}

/// <summary>Versioned authoring resource shared by rendering, buoyancy and effects.</summary>
public sealed class WaterProfile
{
    public const int SchemaVersion=2;
    public ResourceId Id { get; }
    public WaterDomain Domain { get; }
    public WaterReflection Reflection { get; }
    public IReadOnlyList<WaterWave> Waves { get; }
    public float3 DeepColor { get; }
    public float3 ShallowColor { get; }
    public float3 Absorption { get; }
    public float RefractiveIndex { get; }
    public float Roughness { get; }
    public float Turbidity { get; }
    public float FoamThreshold { get; }
    public float FoamStrength { get; }
    public float SurfaceOpacity { get; }
    public float MicroWaveStrength { get; }
    public float MaximumDistance { get; }

    private sealed record ColorDocument(float R,float G,float B);
    private sealed record Document(int Version, Guid Id, WaterDomain Domain,
        WaterReflection Reflection, WaterWave[] Waves, ColorDocument DeepColor, ColorDocument ShallowColor,
        ColorDocument Absorption, float RefractiveIndex, float Roughness, float Turbidity,
        float MaximumDistance, float FoamThreshold=.82f, float FoamStrength=.65f,
        float SurfaceOpacity=.72f, float MicroWaveStrength=1f);

    public WaterProfile(ResourceId id, WaterDomain domain, WaterReflection reflection,
        IEnumerable<WaterWave> waves, float3 deepColor, float3 shallowColor, float3 absorption,
        float refractiveIndex=1.333f, float roughness=.075f, float turbidity=.18f,
        float maximumDistance=8000f, float foamThreshold=.82f, float foamStrength=.65f,
        float surfaceOpacity=.72f, float microWaveStrength=1f)
    {
        ArgumentNullException.ThrowIfNull(waves);
        var owned=waves.ToArray();
        if(id.IsEmpty || !Enum.IsDefined(domain) || !Enum.IsDefined(reflection) || owned.Length>8 ||
           owned.Any(w=>!w.IsValid) || !ColorValid(deepColor) || !ColorValid(shallowColor) ||
           !ColorValid(absorption) || !float.IsFinite(refractiveIndex) || refractiveIndex is <1 or >2 ||
           !float.IsFinite(roughness) || roughness is <0 or >1 || !float.IsFinite(turbidity) ||
           turbidity is <0 or >1 || !float.IsFinite(maximumDistance) || maximumDistance<=0)
            throw new ArgumentOutOfRangeException(nameof(waves),"Invalid water profile parameters.");
        if(!float.IsFinite(foamThreshold)||foamThreshold is <0 or >1 ||
           !float.IsFinite(foamStrength)||foamStrength is <0 or >2 ||
           !float.IsFinite(surfaceOpacity)||surfaceOpacity is <.05f or >1 ||
           !float.IsFinite(microWaveStrength)||microWaveStrength is <0 or >4)
            throw new ArgumentOutOfRangeException(nameof(waves),"Invalid water optical parameters.");
        Id=id;Domain=domain;Reflection=reflection;Waves=owned;DeepColor=deepColor;
        ShallowColor=shallowColor;Absorption=absorption;RefractiveIndex=refractiveIndex;
        Roughness=roughness;Turbidity=turbidity;MaximumDistance=maximumDistance;
        FoamThreshold=foamThreshold;FoamStrength=foamStrength;SurfaceOpacity=surfaceOpacity;
        MicroWaveStrength=microWaveStrength;
    }

    public string Serialize()=>JsonSerializer.Serialize(new Document(SchemaVersion,Id.Value,Domain,
        Reflection,Waves.ToArray(),Color(DeepColor),Color(ShallowColor),Color(Absorption),RefractiveIndex,Roughness,
        Turbidity,MaximumDistance,FoamThreshold,FoamStrength,SurfaceOpacity,MicroWaveStrength));
    public static WaterProfile Deserialize(string json)
    {
        var d=JsonSerializer.Deserialize<Document>(json)??throw new FormatException("Empty water profile.");
        if(d.Version is <1 or >SchemaVersion)throw new FormatException($"Unsupported water profile version {d.Version}.");
        return new(new(d.Id),d.Domain,d.Reflection,d.Waves,Color(d.DeepColor),Color(d.ShallowColor),Color(d.Absorption),
            d.RefractiveIndex,d.Roughness,d.Turbidity,d.MaximumDistance,d.FoamThreshold,
            d.FoamStrength,d.SurfaceOpacity,d.MicroWaveStrength);
    }
    private static bool ColorValid(float3 c)=>float.IsFinite(c.X)&&float.IsFinite(c.Y)&&
        float.IsFinite(c.Z)&&c.X>=0&&c.Y>=0&&c.Z>=0;
    private static ColorDocument Color(float3 c)=>new(c.X,c.Y,c.Z);
    private static float3 Color(ColorDocument c)=>new(c.R,c.G,c.B);
}
