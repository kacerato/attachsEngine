using System.Numerics;

namespace Astra;

public enum MaterialTextureBinding : uint { BaseColor, Normal, MetallicRoughness, Emissive, Occlusion }
public enum MaterialAlphaMode : uint { Inherit, Opaque, Mask, Blend }
public enum MaterialSides : uint { Inherit, Single, Double }
public enum MaterialChannel : uint { Inherit, R, G, B, A }
public enum MaterialOcclusionSource : uint { Inherit, None, Packed, Texture }
public enum MaterialToggle : uint { Inherit, Off, On }
public enum MaterialAlphaSource : uint { Inherit, BaseAlpha, Opaque, BaseLuminance }
public enum MaterialUvSet : uint { Inherit, Uv0, Uv1, World }
public enum MaterialWrap : uint { Inherit, Repeat, Clamp, Mirror }
public enum MaterialFilter : uint { Inherit, Linear, Nearest }

/// <summary>Amostragem de um binding de textura dentro de um slot de material.</summary>
public readonly struct MaterialTextureSampling(Component component,uint slot,string binding)
{
    private string Id(string field)=>$"sampling.{binding}.{field}";
    private float F(string field)=>component.GetSlotFloat(Id(field),slot);
    private void F(string field,float value)=>component.SetSlotFloat(Id(field),value,slot);
    private T E<T>(string field) where T:struct,Enum => (T)Enum.ToObject(typeof(T),component.GetSlotEnum(Id(field),slot));
    private void E<T>(string field,T value) where T:struct,Enum => component.SetSlotEnum(Id(field),Convert.ToUInt32(value),slot);
    private static void Range(float value,float minimum,float maximum,string name)
    {
        if(!float.IsFinite(value)||value<minimum||value>maximum)
            throw new ArgumentOutOfRangeException(name,value,$"O valor deve estar entre {minimum} e {maximum}.");
    }
    private static void Range(Vector2 value,float minimum,float maximum,string name)
    { Range(value.X,minimum,maximum,name+".X");Range(value.Y,minimum,maximum,name+".Y"); }
    public MaterialUvSet UvSet { get=>E<MaterialUvSet>("uv_set"); set=>E("uv_set",value); }
    public MaterialWrap Wrap { get=>E<MaterialWrap>("wrap"); set=>E("wrap",value); }
    public MaterialFilter Filter { get=>E<MaterialFilter>("filter"); set=>E("filter",value); }
    public Vector2 Offset { get=>new(F("offset_u"),F("offset_v")); set {Range(value,-100,100,nameof(Offset));F("offset_u",value.X);F("offset_v",value.Y);} }
    public Vector2 Scale { get=>new(F("scale_u"),F("scale_v")); set {Range(value,.01f,100,nameof(Scale));F("scale_u",value.X);F("scale_v",value.Y);} }
    public float RotationDegrees { get=>F("rotation"); set=>F("rotation",value); }
}

/// <summary>Vista de um slot existente de MeshRenderer no mundo Play. Propriedades de amostragem sem binding escrevem todos e leem a cor base; use TextureSampling para um binding.</summary>
public readonly struct MaterialSlot(Component component,uint slot)
{
    private static void Range(float value,float minimum,float maximum,string name)
    {
        if(!float.IsFinite(value)||value<minimum||value>maximum)
            throw new ArgumentOutOfRangeException(name,value,$"O valor deve estar entre {minimum} e {maximum}.");
    }
    private static void Range(Vector2 value,float minimum,float maximum,string name)
    { Range(value.X,minimum,maximum,name+".X");Range(value.Y,minimum,maximum,name+".Y"); }
    private static void Range(Vector3 value,float minimum,float maximum,string name)
    { Range(value.X,minimum,maximum,name+".X");Range(value.Y,minimum,maximum,name+".Y");Range(value.Z,minimum,maximum,name+".Z"); }
    private float F(string id)=>component.GetSlotFloat(id,slot);
    private void F(string id,float value)=>component.SetSlotFloat(id,value,slot);
    private T E<T>(string id) where T:struct,Enum => (T)Enum.ToObject(typeof(T),component.GetSlotEnum(id,slot));
    private void E<T>(string id,T value) where T:struct,Enum => component.SetSlotEnum(id,Convert.ToUInt32(value),slot);
    public AssetGuid SharedMaterial { get=>component.GetResource("material",slot); set=>component.SetResource("material",value,slot); }
    /// <summary>Controla o override local. Escrever qualquer fator PBR também o ativa, como no Inspector.</summary>
    public bool Override { get=>component.GetSlotEnum("material.override",slot)!=0; set=>component.SetSlotEnum("material.override",value?1u:0u,slot); }
    public Vector3 BaseColor { get=>new(F("material.base_color.r"),F("material.base_color.g"),F("material.base_color.b")); set {Range(value,0,1,nameof(BaseColor));F("material.base_color.r",value.X);F("material.base_color.g",value.Y);F("material.base_color.b",value.Z);} }
    public float Roughness { get=>F("material.roughness"); set=>F("material.roughness",value); }
    public float Metallic { get=>F("material.metallic"); set=>F("material.metallic",value); }
    public float NormalScale { get=>F("material.normal_scale"); set=>F("material.normal_scale",value); }
    public float Specular { get=>F("material.specular"); set=>F("material.specular",value); }
    public Vector3 Emission { get=>new(F("material.emission.r"),F("material.emission.g"),F("material.emission.b")); set {Range(value,0,1,nameof(Emission));F("material.emission.r",value.X);F("material.emission.g",value.Y);F("material.emission.b",value.Z);} }
    public float EmissionStrength { get=>F("material.emission_strength"); set=>F("material.emission_strength",value); }
    public MaterialAlphaMode AlphaMode { get=>E<MaterialAlphaMode>("surface.alpha_mode"); set=>E("surface.alpha_mode",value); }
    public MaterialSides Sides { get=>E<MaterialSides>("surface.sides"); set=>E("surface.sides",value); }
    public float AlphaCutoff { get=>F("surface.alpha_cutoff"); set=>F("surface.alpha_cutoff",value); }
    public MaterialChannel RoughnessChannel { get=>E<MaterialChannel>("channels.roughness"); set=>E("channels.roughness",value); }
    public MaterialChannel MetallicChannel { get=>E<MaterialChannel>("channels.metallic"); set=>E("channels.metallic",value); }
    public MaterialChannel OcclusionChannel { get=>E<MaterialChannel>("channels.occlusion"); set=>E("channels.occlusion",value); }
    public MaterialOcclusionSource OcclusionSource { get=>E<MaterialOcclusionSource>("channels.occlusion_source"); set=>E("channels.occlusion_source",value); }
    public float OcclusionStrength { get=>F("channels.occlusion_strength"); set=>F("channels.occlusion_strength",value); }
    public MaterialToggle NormalFlipY { get=>E<MaterialToggle>("channels.normal_flip_y"); set=>E("channels.normal_flip_y",value); }
    public MaterialAlphaSource AlphaSource { get=>E<MaterialAlphaSource>("channels.alpha_source"); set=>E("channels.alpha_source",value); }
    public MaterialUvSet UvSet { get=>E<MaterialUvSet>("sampling.uv_set"); set=>E("sampling.uv_set",value); }
    public MaterialWrap Wrap { get=>E<MaterialWrap>("sampling.wrap"); set=>E("sampling.wrap",value); }
    public MaterialFilter Filter { get=>E<MaterialFilter>("sampling.filter"); set=>E("sampling.filter",value); }
    public Vector2 Offset { get=>new(F("sampling.offset_u"),F("sampling.offset_v")); set {Range(value,-100,100,nameof(Offset));F("sampling.offset_u",value.X);F("sampling.offset_v",value.Y);} }
    public Vector2 Scale { get=>new(F("sampling.scale_u"),F("sampling.scale_v")); set {Range(value,.01f,100,nameof(Scale));F("sampling.scale_u",value.X);F("sampling.scale_v",value.Y);} }
    public float RotationDegrees { get=>F("sampling.rotation"); set=>F("sampling.rotation",value); }

    private static string SamplingId(MaterialTextureBinding binding)=>binding switch {
        MaterialTextureBinding.BaseColor=>"base_color",MaterialTextureBinding.Normal=>"normal",
        MaterialTextureBinding.MetallicRoughness=>"metallic_roughness",MaterialTextureBinding.Emissive=>"emissive",
        // A oclusão própria usa a mesma transformação UV/sampler do mapa
        // metálico/rugosidade no contrato nativo e no Inspector.
        MaterialTextureBinding.Occlusion=>"metallic_roughness",_=>throw new ArgumentOutOfRangeException(nameof(binding))};
    public MaterialTextureSampling TextureSampling(MaterialTextureBinding binding)=>new(component,slot,SamplingId(binding));

    private static string TextureId(MaterialTextureBinding binding)=>binding switch {
        MaterialTextureBinding.BaseColor=>"texture.base_color",MaterialTextureBinding.Normal=>"texture.normal",
        MaterialTextureBinding.MetallicRoughness=>"texture.metallic_roughness",MaterialTextureBinding.Emissive=>"texture.emissive",
        MaterialTextureBinding.Occlusion=>"texture.occlusion",_=>throw new ArgumentOutOfRangeException(nameof(binding))};
    public AssetGuid GetTexture(MaterialTextureBinding binding)=>component.GetResource(TextureId(binding),slot);
    public void SetTexture(MaterialTextureBinding binding,AssetGuid texture)=>component.SetResource(TextureId(binding),texture,slot);
    public void SetTexture(MaterialTextureBinding binding,AssetReference texture)=>SetTexture(binding,AssetGuid.Parse(texture.AssetId));
    /// <summary>Remove o override local; o material compartilhado ou a fonte volta a fornecer o binding.</summary>
    public void InheritTexture(MaterialTextureBinding binding)=>SetTexture(binding,default(AssetGuid));
    /// <summary>Declara o binding sem textura, mesmo quando a fonte ou o material compartilhado possui uma.</summary>
    public void RemoveTexture(MaterialTextureBinding binding)=>SetTexture(binding,new AssetGuid(ulong.MaxValue,ulong.MaxValue));
}
