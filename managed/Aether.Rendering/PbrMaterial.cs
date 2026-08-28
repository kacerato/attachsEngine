using System.Text.Json;
using System.Runtime.InteropServices;
using Aether.Resources;

namespace Aether.Rendering;

/// <summary>Immutable material resource; no GPU handles or editor dependencies.
/// Versioned asset data is separate from transient MeshRenderer instances.</summary>
public sealed class PbrMaterial
{
    public const int SchemaVersion = 1;
    public ResourceId Id { get; }
    public ResourceId Albedo { get; }
    public ResourceId Normal { get; }
    public ResourceId OcclusionRoughnessMetallic { get; }
    public PbrMaterialParameters Parameters { get; }

    public PbrMaterial(ResourceId id, ResourceId albedo, ResourceId normal, ResourceId orm,
        PbrMaterialParameters parameters)
    {
        if (id.IsEmpty || albedo.IsEmpty || normal.IsEmpty || orm.IsEmpty)
            throw new ArgumentException("Material and texture references must be stable resource IDs.");
        if (!parameters.IsValid) throw new ArgumentOutOfRangeException(nameof(parameters));
        Id=id; Albedo=albedo; Normal=normal; OcclusionRoughnessMetallic=orm; Parameters=parameters;
    }
    private sealed record Document(int Version, Guid Id, Guid Albedo, Guid Normal, Guid Orm,
        float Roughness, float Metallic, float NormalScale);
    public string Serialize() => JsonSerializer.Serialize(new Document(SchemaVersion,Id.Value,Albedo.Value,
        Normal.Value,OcclusionRoughnessMetallic.Value,Parameters.RoughnessFactor,Parameters.MetallicFactor,Parameters.NormalScale));
    public static PbrMaterial Deserialize(string json)
    {
        var data=JsonSerializer.Deserialize<Document>(json) ?? throw new FormatException("Empty material.");
        if (data.Version!=SchemaVersion) throw new FormatException($"Unsupported material version {data.Version}.");
        return new PbrMaterial(new(data.Id),new(data.Albedo),new(data.Normal),new(data.Orm),
            new PbrMaterialParameters { RoughnessFactor=data.Roughness, MetallicFactor=data.Metallic, NormalScale=data.NormalScale });
    }
    public static PbrMaterial MetalPlate { get; } = new(BuiltinRenderResources.MetalPlateMaterial,
        new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a05")),
        new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a06")),
        new(new Guid("05a9a93b-2906-4219-955b-c6cc9eb18a07")),
        new PbrMaterialParameters { RoughnessFactor=1, MetallicFactor=1, NormalScale=1 });
}

[StructLayout(LayoutKind.Sequential, Pack=4)]
public struct PbrMaterialParameters
{
    public float RoughnessFactor, MetallicFactor, NormalScale;
    public readonly bool IsValid => float.IsFinite(RoughnessFactor) && RoughnessFactor is >=0 and <=1 &&
        float.IsFinite(MetallicFactor) && MetallicFactor is >=0 and <=1 &&
        float.IsFinite(NormalScale) && NormalScale is >=0 and <=4;
}
