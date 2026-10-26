using System.Numerics;

namespace Astra;

public enum MaterialTextureBinding : uint { BaseColor, Normal, MetallicRoughness, Emissive, Occlusion }
public enum MaterialAlphaMode : uint { Inherit, Opaque, Mask, Blend }
/// <summary>Render Face da Unity: Single = Front, Double = Both, Back = só a face de trás.</summary>
public enum MaterialSides : uint { Inherit, Single, Double, Back }
public enum MaterialChannel : uint { Inherit, R, G, B, A }
public enum MaterialOcclusionSource : uint { Inherit, None, Packed, Texture }
public enum MaterialToggle : uint { Inherit, Off, On }
public enum MaterialAlphaSource : uint { Inherit, BaseAlpha, Opaque, BaseLuminance }
/// <summary>Rugosidade direta, ou suavidade do Standard da Unity (rugosidade = 1 - (1 - fator) * canal).</summary>
public enum MaterialRoughnessSource : uint { Inherit, Map, SmoothnessMap, SmoothnessBaseAlpha }
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
    /// <summary>Smoothness da URP: 1 - rugosidade. O arquivo guarda rugosidade; escrever ativa o override local.</summary>
    public float Smoothness { get=>1-F("material.roughness"); set {Range(value,0,1,nameof(Smoothness));F("material.roughness",1-value);} }
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
    public MaterialRoughnessSource RoughnessSource { get=>E<MaterialRoughnessSource>("channels.roughness_source"); set=>E("channels.roughness_source",value); }
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

    // G7: Material.SetFloat/SetColor/SetTexture com os nomes de propriedade dos
    // shaders Lit, Simple Lit e Unlit da URP (e os do Standard: _Color,
    // _MainTex, _Glossiness). Escrevem o override deste slot, como
    // Renderer.material na Unity; nome desconhecido lança, em vez de sumir.
    // Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html
    private static readonly string[] FloatNames=["_Smoothness","_Glossiness","_Metallic","_BumpScale","_Cutoff","_OcclusionStrength"];
    private static readonly string[] ColorNames=["_BaseColor","_Color","_EmissionColor"];
    private static readonly string[] TextureNames=["_BaseMap","_MainTex","_BumpMap","_MetallicGlossMap","_EmissionMap","_OcclusionMap"];
    private static ArgumentException Unknown(string name,string[] known)=>
        new($"Propriedade de material desconhecida: {name}. Aceitas: {string.Join(", ",known)}.",nameof(name));
    public bool HasProperty(string name)=>FloatNames.Contains(name)||ColorNames.Contains(name)||TextureNames.Contains(name);
    public float GetFloat(string name)=>name switch {
        "_Smoothness" or "_Glossiness"=>Smoothness,"_Metallic"=>Metallic,"_BumpScale"=>NormalScale,
        "_Cutoff"=>AlphaCutoff,"_OcclusionStrength"=>OcclusionStrength,_=>throw Unknown(name,FloatNames)};
    public void SetFloat(string name,float value)
    {
        switch(name) {
        case "_Smoothness": case "_Glossiness": Smoothness=value; break;
        case "_Metallic": Metallic=value; break;
        case "_BumpScale": NormalScale=value; break;
        case "_Cutoff": AlphaCutoff=value; break;
        case "_OcclusionStrength": OcclusionStrength=value; break;
        default: throw Unknown(name,FloatNames);
        }
    }
    /// <summary>_EmissionColor é HDR: lê a cor de emissão multiplicada pela força.</summary>
    public Color GetColor(string name)
    {
        switch(name) {
        case "_BaseColor": case "_Color": { var c=BaseColor; return new(c.X,c.Y,c.Z,1); }
        case "_EmissionColor": { var c=Emission*EmissionStrength; return new(c.X,c.Y,c.Z,1); }
        default: throw Unknown(name,ColorNames);
        }
    }
    /// <summary>Cor linear; o alfa não é usado. Na emissão, um canal acima de 1 vira força (a cor fica normalizada).</summary>
    public void SetColor(string name,Color value)
    {
        switch(name) {
        case "_BaseColor": case "_Color": BaseColor=new(value.R,value.G,value.B); break;
        case "_EmissionColor": {
            var peak=MathF.Max(value.R,MathF.Max(value.G,value.B));
            if(!float.IsFinite(peak)||peak<0) throw new ArgumentOutOfRangeException(nameof(value));
            var strength=MathF.Max(1,peak);
            Emission=new(value.R/strength,value.G/strength,value.B/strength);EmissionStrength=peak>0?strength:0;
            break;
        }
        default: throw Unknown(name,ColorNames);
        }
    }
    private static MaterialTextureBinding TextureBinding(string name)=>name switch {
        "_BaseMap" or "_MainTex"=>MaterialTextureBinding.BaseColor,"_BumpMap"=>MaterialTextureBinding.Normal,
        "_MetallicGlossMap"=>MaterialTextureBinding.MetallicRoughness,"_EmissionMap"=>MaterialTextureBinding.Emissive,
        "_OcclusionMap"=>MaterialTextureBinding.Occlusion,_=>throw Unknown(name,TextureNames)};
    public AssetGuid GetTexture(string name)=>GetTexture(TextureBinding(name));
    public void SetTexture(string name,AssetGuid texture)=>SetTexture(TextureBinding(name),texture);
    public void SetTexture(string name,AssetReference texture)=>SetTexture(TextureBinding(name),texture);
    public Vector2 GetTextureOffset(string name)=>TextureSampling(TextureBinding(name)).Offset;
    public void SetTextureOffset(string name,Vector2 offset){var s=TextureSampling(TextureBinding(name));s.Offset=offset;}
    public Vector2 GetTextureScale(string name)=>TextureSampling(TextureBinding(name)).Scale;
    public void SetTextureScale(string name,Vector2 scale){var s=TextureSampling(TextureBinding(name));s.Scale=scale;}
}
