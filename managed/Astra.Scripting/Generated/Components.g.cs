// GERADO por scene::componentCSharpApi() a partir de native/scene/schemas/*.h — não edite.
// Regenerar: aether_tests --write-component-api managed/Astra.Scripting/Generated/Components.g.cs
#nullable enable
using System.Numerics;

namespace Astra.Components;

/// <summary>Fachada tipada de um tipo de componente nativo.</summary>
public interface IComponentFacade<TSelf> where TSelf : struct, IComponentFacade<TSelf>
{
    static abstract string TypeId { get; }
    static abstract TSelf Wrap(Component component);
    Component Component { get; }
}

/// <summary>Acesso tipado a partir do objeto: <c>obj.GetComponent&lt;PhysicsBody&gt;()</c>.</summary>
public static class ComponentFacadeExtensions
{
    public static T? GetComponent<T>(this GameObject owner, int ordinal = 0) where T : struct, IComponentFacade<T> =>
        owner.GetComponent(T.TypeId, ordinal) is { } component ? T.Wrap(component) : null;
    public static T AddComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>
        T.Wrap(owner.AddComponent(T.TypeId));
    public static bool HasComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>
        owner.HasComponent(T.TypeId);
}

/// <summary>Timer: Dispara eventos temporizados para comportamentos. Família Lógica · Tempo.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_timer.html</remarks>
public readonly struct GameTimer : IComponentFacade<GameTimer>
{
    public static string TypeId => "astra.time.timer";
    public static GameTimer Wrap(Component component) => new(component);
    public Component Component { get; }
    public GameTimer(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Intervalo (s). Tempo entre disparos; uma vez quando Repetir está desligado</summary>
    /// <remarks>Faixa válida: 0.05 a 3600.</remarks>
    public float IntervalSeconds
    {
        get => Component.GetFloat("interval_seconds");
        set => Component.SetFloat("interval_seconds", value);
    }
    /// <summary>Repetir</summary>
    public bool Repeat
    {
        get => Component.GetBool("repeat");
        set => Component.SetBool("repeat", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Malha: Geometria e material. Família Renderização · Geometria.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html</remarks>
public readonly struct MeshRenderer : IComponentFacade<MeshRenderer>
{
    public static string TypeId => "astra.render.mesh";
    public static MeshRenderer Wrap(Component component) => new(component);
    public Component Component { get; }
    public MeshRenderer(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Cor R</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorR
    {
        get => Component.GetFloat("base_color.r");
        set => Component.SetFloat("base_color.r", value);
    }
    /// <summary>Cor G</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorG
    {
        get => Component.GetFloat("base_color.g");
        set => Component.SetFloat("base_color.g", value);
    }
    /// <summary>Cor B</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorB
    {
        get => Component.GetFloat("base_color.b");
        set => Component.SetFloat("base_color.b", value);
    }
    /// <summary>Rugosidade</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Roughness
    {
        get => Component.GetFloat("roughness");
        set => Component.SetFloat("roughness", value);
    }
    /// <summary>Metálico</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Metallic
    {
        get => Component.GetFloat("metallic");
        set => Component.SetFloat("metallic", value);
    }
    /// <summary>Intensidade da normal</summary>
    /// <remarks>Faixa válida: 0 a 16.</remarks>
    public float NormalScale
    {
        get => Component.GetFloat("normal_scale");
        set => Component.SetFloat("normal_scale", value);
    }
    /// <summary>Especular</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Specular
    {
        get => Component.GetFloat("specular");
        set => Component.SetFloat("specular", value);
    }
    /// <summary>Emissão R</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionR
    {
        get => Component.GetFloat("emission.r");
        set => Component.SetFloat("emission.r", value);
    }
    /// <summary>Emissão G</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionG
    {
        get => Component.GetFloat("emission.g");
        set => Component.SetFloat("emission.g", value);
    }
    /// <summary>Emissão B</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionB
    {
        get => Component.GetFloat("emission.b");
        set => Component.SetFloat("emission.b", value);
    }
    /// <summary>Potência de emissão</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float EmissionStrength
    {
        get => Component.GetFloat("emission_strength");
        set => Component.SetFloat("emission_strength", value);
    }
    /// <summary>Renderizar</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Malha. Recurso do projeto por slot</summary>
    public AssetGuid GetMesh(uint slot = 0) => Component.GetResource("mesh", slot);
    public void SetMesh(AssetGuid value, uint slot = 0) => Component.SetResource("mesh", value, slot);
    /// <summary>Material. Recurso do projeto por slot</summary>
    public AssetGuid GetMaterial(uint slot = 0) => Component.GetResource("material", slot);
    public void SetMaterial(AssetGuid value, uint slot = 0) => Component.SetResource("material", value, slot);
    /// <summary>Cor base. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureBaseColor(uint slot = 0) => Component.GetResource("texture.base_color", slot);
    public void SetTextureBaseColor(AssetGuid value, uint slot = 0) => Component.SetResource("texture.base_color", value, slot);
    /// <summary>Normal. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureNormal(uint slot = 0) => Component.GetResource("texture.normal", slot);
    public void SetTextureNormal(AssetGuid value, uint slot = 0) => Component.SetResource("texture.normal", value, slot);
    /// <summary>Metal / rugosidade. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureMetallicRoughness(uint slot = 0) => Component.GetResource("texture.metallic_roughness", slot);
    public void SetTextureMetallicRoughness(AssetGuid value, uint slot = 0) => Component.SetResource("texture.metallic_roughness", value, slot);
    /// <summary>Emissão. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureEmissive(uint slot = 0) => Component.GetResource("texture.emissive", slot);
    public void SetTextureEmissive(AssetGuid value, uint slot = 0) => Component.SetResource("texture.emissive", value, slot);
    /// <summary>Oclusão. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureOcclusion(uint slot = 0) => Component.GetResource("texture.occlusion", slot);
    public void SetTextureOcclusion(AssetGuid value, uint slot = 0) => Component.SetResource("texture.occlusion", value, slot);
    /// <summary>Corte do alfa. Valor por slot</summary>
    public float GetSurfaceAlphaCutoff(uint slot) => Component.GetSlotFloat("surface.alpha_cutoff", slot);
    public void SetSurfaceAlphaCutoff(uint slot, float value) => Component.SetSlotFloat("surface.alpha_cutoff", value, slot);
    /// <summary>Força da oclusão. Valor por slot</summary>
    public float GetChannelsOcclusionStrength(uint slot) => Component.GetSlotFloat("channels.occlusion_strength", slot);
    public void SetChannelsOcclusionStrength(uint slot, float value) => Component.SetSlotFloat("channels.occlusion_strength", value, slot);
    /// <summary>Deslocamento U. Valor por slot</summary>
    public float GetSamplingOffsetU(uint slot) => Component.GetSlotFloat("sampling.offset_u", slot);
    public void SetSamplingOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.offset_u", value, slot);
    /// <summary>Deslocamento V. Valor por slot</summary>
    public float GetSamplingOffsetV(uint slot) => Component.GetSlotFloat("sampling.offset_v", slot);
    public void SetSamplingOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.offset_v", value, slot);
    /// <summary>Escala U. Valor por slot</summary>
    public float GetSamplingScaleU(uint slot) => Component.GetSlotFloat("sampling.scale_u", slot);
    public void SetSamplingScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.scale_u", value, slot);
    /// <summary>Escala V. Valor por slot</summary>
    public float GetSamplingScaleV(uint slot) => Component.GetSlotFloat("sampling.scale_v", slot);
    public void SetSamplingScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.scale_v", value, slot);
    /// <summary>Rotação da UV. Valor por slot</summary>
    public float GetSamplingRotation(uint slot) => Component.GetSlotFloat("sampling.rotation", slot);
    public void SetSamplingRotation(uint slot, float value) => Component.SetSlotFloat("sampling.rotation", value, slot);
    /// <summary>Cor base / Deslocamento U. Valor por slot</summary>
    public float GetSamplingBaseColorOffsetU(uint slot) => Component.GetSlotFloat("sampling.base_color.offset_u", slot);
    public void SetSamplingBaseColorOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.offset_u", value, slot);
    /// <summary>Cor base / Deslocamento V. Valor por slot</summary>
    public float GetSamplingBaseColorOffsetV(uint slot) => Component.GetSlotFloat("sampling.base_color.offset_v", slot);
    public void SetSamplingBaseColorOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.offset_v", value, slot);
    /// <summary>Cor base / Escala U. Valor por slot</summary>
    public float GetSamplingBaseColorScaleU(uint slot) => Component.GetSlotFloat("sampling.base_color.scale_u", slot);
    public void SetSamplingBaseColorScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.scale_u", value, slot);
    /// <summary>Cor base / Escala V. Valor por slot</summary>
    public float GetSamplingBaseColorScaleV(uint slot) => Component.GetSlotFloat("sampling.base_color.scale_v", slot);
    public void SetSamplingBaseColorScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.scale_v", value, slot);
    /// <summary>Cor base / Rotação. Valor por slot</summary>
    public float GetSamplingBaseColorRotation(uint slot) => Component.GetSlotFloat("sampling.base_color.rotation", slot);
    public void SetSamplingBaseColorRotation(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.rotation", value, slot);
    /// <summary>Normal / Deslocamento U. Valor por slot</summary>
    public float GetSamplingNormalOffsetU(uint slot) => Component.GetSlotFloat("sampling.normal.offset_u", slot);
    public void SetSamplingNormalOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.normal.offset_u", value, slot);
    /// <summary>Normal / Deslocamento V. Valor por slot</summary>
    public float GetSamplingNormalOffsetV(uint slot) => Component.GetSlotFloat("sampling.normal.offset_v", slot);
    public void SetSamplingNormalOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.normal.offset_v", value, slot);
    /// <summary>Normal / Escala U. Valor por slot</summary>
    public float GetSamplingNormalScaleU(uint slot) => Component.GetSlotFloat("sampling.normal.scale_u", slot);
    public void SetSamplingNormalScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.normal.scale_u", value, slot);
    /// <summary>Normal / Escala V. Valor por slot</summary>
    public float GetSamplingNormalScaleV(uint slot) => Component.GetSlotFloat("sampling.normal.scale_v", slot);
    public void SetSamplingNormalScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.normal.scale_v", value, slot);
    /// <summary>Normal / Rotação. Valor por slot</summary>
    public float GetSamplingNormalRotation(uint slot) => Component.GetSlotFloat("sampling.normal.rotation", slot);
    public void SetSamplingNormalRotation(uint slot, float value) => Component.SetSlotFloat("sampling.normal.rotation", value, slot);
    /// <summary>Metal / rugosidade / Deslocamento U. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessOffsetU(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.offset_u", slot);
    public void SetSamplingMetallicRoughnessOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.offset_u", value, slot);
    /// <summary>Metal / rugosidade / Deslocamento V. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessOffsetV(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.offset_v", slot);
    public void SetSamplingMetallicRoughnessOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.offset_v", value, slot);
    /// <summary>Metal / rugosidade / Escala U. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessScaleU(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.scale_u", slot);
    public void SetSamplingMetallicRoughnessScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.scale_u", value, slot);
    /// <summary>Metal / rugosidade / Escala V. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessScaleV(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.scale_v", slot);
    public void SetSamplingMetallicRoughnessScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.scale_v", value, slot);
    /// <summary>Metal / rugosidade / Rotação. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessRotation(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.rotation", slot);
    public void SetSamplingMetallicRoughnessRotation(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.rotation", value, slot);
    /// <summary>Emissão / Deslocamento U. Valor por slot</summary>
    public float GetSamplingEmissiveOffsetU(uint slot) => Component.GetSlotFloat("sampling.emissive.offset_u", slot);
    public void SetSamplingEmissiveOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.offset_u", value, slot);
    /// <summary>Emissão / Deslocamento V. Valor por slot</summary>
    public float GetSamplingEmissiveOffsetV(uint slot) => Component.GetSlotFloat("sampling.emissive.offset_v", slot);
    public void SetSamplingEmissiveOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.offset_v", value, slot);
    /// <summary>Emissão / Escala U. Valor por slot</summary>
    public float GetSamplingEmissiveScaleU(uint slot) => Component.GetSlotFloat("sampling.emissive.scale_u", slot);
    public void SetSamplingEmissiveScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.scale_u", value, slot);
    /// <summary>Emissão / Escala V. Valor por slot</summary>
    public float GetSamplingEmissiveScaleV(uint slot) => Component.GetSlotFloat("sampling.emissive.scale_v", slot);
    public void SetSamplingEmissiveScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.scale_v", value, slot);
    /// <summary>Emissão / Rotação. Valor por slot</summary>
    public float GetSamplingEmissiveRotation(uint slot) => Component.GetSlotFloat("sampling.emissive.rotation", slot);
    public void SetSamplingEmissiveRotation(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.rotation", value, slot);
    /// <summary>Cor R. Valor por slot</summary>
    public float GetMaterialBaseColorR(uint slot) => Component.GetSlotFloat("material.base_color.r", slot);
    public void SetMaterialBaseColorR(uint slot, float value) => Component.SetSlotFloat("material.base_color.r", value, slot);
    /// <summary>Cor G. Valor por slot</summary>
    public float GetMaterialBaseColorG(uint slot) => Component.GetSlotFloat("material.base_color.g", slot);
    public void SetMaterialBaseColorG(uint slot, float value) => Component.SetSlotFloat("material.base_color.g", value, slot);
    /// <summary>Cor B. Valor por slot</summary>
    public float GetMaterialBaseColorB(uint slot) => Component.GetSlotFloat("material.base_color.b", slot);
    public void SetMaterialBaseColorB(uint slot, float value) => Component.SetSlotFloat("material.base_color.b", value, slot);
    /// <summary>Rugosidade. Valor por slot</summary>
    public float GetMaterialRoughness(uint slot) => Component.GetSlotFloat("material.roughness", slot);
    public void SetMaterialRoughness(uint slot, float value) => Component.SetSlotFloat("material.roughness", value, slot);
    /// <summary>Metálico. Valor por slot</summary>
    public float GetMaterialMetallic(uint slot) => Component.GetSlotFloat("material.metallic", slot);
    public void SetMaterialMetallic(uint slot, float value) => Component.SetSlotFloat("material.metallic", value, slot);
    /// <summary>Intensidade da normal. Valor por slot</summary>
    public float GetMaterialNormalScale(uint slot) => Component.GetSlotFloat("material.normal_scale", slot);
    public void SetMaterialNormalScale(uint slot, float value) => Component.SetSlotFloat("material.normal_scale", value, slot);
    /// <summary>Especular. Valor por slot</summary>
    public float GetMaterialSpecular(uint slot) => Component.GetSlotFloat("material.specular", slot);
    public void SetMaterialSpecular(uint slot, float value) => Component.SetSlotFloat("material.specular", value, slot);
    /// <summary>Emissão R. Valor por slot</summary>
    public float GetMaterialEmissionR(uint slot) => Component.GetSlotFloat("material.emission.r", slot);
    public void SetMaterialEmissionR(uint slot, float value) => Component.SetSlotFloat("material.emission.r", value, slot);
    /// <summary>Emissão G. Valor por slot</summary>
    public float GetMaterialEmissionG(uint slot) => Component.GetSlotFloat("material.emission.g", slot);
    public void SetMaterialEmissionG(uint slot, float value) => Component.SetSlotFloat("material.emission.g", value, slot);
    /// <summary>Emissão B. Valor por slot</summary>
    public float GetMaterialEmissionB(uint slot) => Component.GetSlotFloat("material.emission.b", slot);
    public void SetMaterialEmissionB(uint slot, float value) => Component.SetSlotFloat("material.emission.b", value, slot);
    /// <summary>Potência de emissão. Valor por slot</summary>
    public float GetMaterialEmissionStrength(uint slot) => Component.GetSlotFloat("material.emission_strength", slot);
    public void SetMaterialEmissionStrength(uint slot, float value) => Component.SetSlotFloat("material.emission_strength", value, slot);
    /// <summary>Material. Opção por slot</summary>
    public uint GetMaterialOverride(uint slot) => Component.GetSlotEnum("material.override", slot);
    public void SetMaterialOverride(uint slot, uint value) => Component.SetSlotEnum("material.override", value, slot);
    /// <summary>Tipo de superfície. Opção por slot</summary>
    public uint GetSurfaceAlphaMode(uint slot) => Component.GetSlotEnum("surface.alpha_mode", slot);
    public void SetSurfaceAlphaMode(uint slot, uint value) => Component.SetSlotEnum("surface.alpha_mode", value, slot);
    /// <summary>Faces. Opção por slot</summary>
    public uint GetSurfaceSides(uint slot) => Component.GetSlotEnum("surface.sides", slot);
    public void SetSurfaceSides(uint slot, uint value) => Component.SetSlotEnum("surface.sides", value, slot);
    /// <summary>Canal da rugosidade. Opção por slot</summary>
    public uint GetChannelsRoughness(uint slot) => Component.GetSlotEnum("channels.roughness", slot);
    public void SetChannelsRoughness(uint slot, uint value) => Component.SetSlotEnum("channels.roughness", value, slot);
    /// <summary>Canal do metálico. Opção por slot</summary>
    public uint GetChannelsMetallic(uint slot) => Component.GetSlotEnum("channels.metallic", slot);
    public void SetChannelsMetallic(uint slot, uint value) => Component.SetSlotEnum("channels.metallic", value, slot);
    /// <summary>Canal da oclusão. Opção por slot</summary>
    public uint GetChannelsOcclusion(uint slot) => Component.GetSlotEnum("channels.occlusion", slot);
    public void SetChannelsOcclusion(uint slot, uint value) => Component.SetSlotEnum("channels.occlusion", value, slot);
    /// <summary>Origem da oclusão. Opção por slot</summary>
    public uint GetChannelsOcclusionSource(uint slot) => Component.GetSlotEnum("channels.occlusion_source", slot);
    public void SetChannelsOcclusionSource(uint slot, uint value) => Component.SetSlotEnum("channels.occlusion_source", value, slot);
    /// <summary>Inverter Y da normal. Opção por slot</summary>
    public uint GetChannelsNormalFlipY(uint slot) => Component.GetSlotEnum("channels.normal_flip_y", slot);
    public void SetChannelsNormalFlipY(uint slot, uint value) => Component.SetSlotEnum("channels.normal_flip_y", value, slot);
    /// <summary>Origem do alfa. Opção por slot</summary>
    public uint GetChannelsAlphaSource(uint slot) => Component.GetSlotEnum("channels.alpha_source", slot);
    public void SetChannelsAlphaSource(uint slot, uint value) => Component.SetSlotEnum("channels.alpha_source", value, slot);
    /// <summary>Conjunto de UV. Opção por slot</summary>
    public uint GetSamplingUvSet(uint slot) => Component.GetSlotEnum("sampling.uv_set", slot);
    public void SetSamplingUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.uv_set", value, slot);
    /// <summary>Repetição. Opção por slot</summary>
    public uint GetSamplingWrap(uint slot) => Component.GetSlotEnum("sampling.wrap", slot);
    public void SetSamplingWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.wrap", value, slot);
    /// <summary>Filtro. Opção por slot</summary>
    public uint GetSamplingFilter(uint slot) => Component.GetSlotEnum("sampling.filter", slot);
    public void SetSamplingFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.filter", value, slot);
    /// <summary>Cor base / UV. Opção por slot</summary>
    public uint GetSamplingBaseColorUvSet(uint slot) => Component.GetSlotEnum("sampling.base_color.uv_set", slot);
    public void SetSamplingBaseColorUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.uv_set", value, slot);
    /// <summary>Cor base / Repetição. Opção por slot</summary>
    public uint GetSamplingBaseColorWrap(uint slot) => Component.GetSlotEnum("sampling.base_color.wrap", slot);
    public void SetSamplingBaseColorWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.wrap", value, slot);
    /// <summary>Cor base / Filtro. Opção por slot</summary>
    public uint GetSamplingBaseColorFilter(uint slot) => Component.GetSlotEnum("sampling.base_color.filter", slot);
    public void SetSamplingBaseColorFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.filter", value, slot);
    /// <summary>Normal / UV. Opção por slot</summary>
    public uint GetSamplingNormalUvSet(uint slot) => Component.GetSlotEnum("sampling.normal.uv_set", slot);
    public void SetSamplingNormalUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.uv_set", value, slot);
    /// <summary>Normal / Repetição. Opção por slot</summary>
    public uint GetSamplingNormalWrap(uint slot) => Component.GetSlotEnum("sampling.normal.wrap", slot);
    public void SetSamplingNormalWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.wrap", value, slot);
    /// <summary>Normal / Filtro. Opção por slot</summary>
    public uint GetSamplingNormalFilter(uint slot) => Component.GetSlotEnum("sampling.normal.filter", slot);
    public void SetSamplingNormalFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.filter", value, slot);
    /// <summary>Metal / rugosidade / UV. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessUvSet(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.uv_set", slot);
    public void SetSamplingMetallicRoughnessUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.uv_set", value, slot);
    /// <summary>Metal / rugosidade / Repetição. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessWrap(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.wrap", slot);
    public void SetSamplingMetallicRoughnessWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.wrap", value, slot);
    /// <summary>Metal / rugosidade / Filtro. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessFilter(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.filter", slot);
    public void SetSamplingMetallicRoughnessFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.filter", value, slot);
    /// <summary>Emissão / UV. Opção por slot</summary>
    public uint GetSamplingEmissiveUvSet(uint slot) => Component.GetSlotEnum("sampling.emissive.uv_set", slot);
    public void SetSamplingEmissiveUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.uv_set", value, slot);
    /// <summary>Emissão / Repetição. Opção por slot</summary>
    public uint GetSamplingEmissiveWrap(uint slot) => Component.GetSlotEnum("sampling.emissive.wrap", slot);
    public void SetSamplingEmissiveWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.wrap", value, slot);
    /// <summary>Emissão / Filtro. Opção por slot</summary>
    public uint GetSamplingEmissiveFilter(uint slot) => Component.GetSlotEnum("sampling.emissive.filter", slot);
    public void SetSamplingEmissiveFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.filter", value, slot);
}

/// <summary>Malha deformável: Esqueleto e blend shapes da Malha. Família Renderização · Geometria.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-SkinnedMeshRenderer.html</remarks>
public readonly struct SkinnedMesh : IComponentFacade<SkinnedMesh>
{
    public static string TypeId => "astra.render.skinned_mesh";
    public static SkinnedMesh Wrap(Component component) => new(component);
    public Component Component { get; }
    public SkinnedMesh(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Vetor de movimento da deformação. A reprojeção temporal usa a pose anterior dos ossos e dos blend shapes, não só a do objeto</summary>
    public bool SkinnedMotionVectors
    {
        get => Component.GetBool("skinned_motion_vectors");
        set => Component.SetBool("skinned_motion_vectors", value);
    }
    public enum QualityOption : uint
    {
        Automatica = 0,
        V1Osso = 1,
        V2Ossos = 2,
        V4Ossos = 4,
    }
    /// <summary>Qualidade. Influências por vértice usadas na deformação (Skin Weights)</summary>
    public QualityOption Quality
    {
        get => (QualityOption)Component.GetEnum("quality");
        set => Component.SetEnum("quality", (uint)value);
    }
    /// <summary>Peso do blend shape (%). Valor por slot</summary>
    public float GetBlendShapeWeight(uint slot) => Component.GetSlotFloat("blend_shape_weight", slot);
    public void SetBlendShapeWeight(uint slot, float value) => Component.SetSlotFloat("blend_shape_weight", value, slot);
}

/// <summary>LOD Group: Nível de detalhe pela altura na tela. Família Renderização · Desempenho.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html</remarks>
public readonly struct LodGroup : IComponentFacade<LodGroup>
{
    public static string TypeId => "astra.render.lod_group";
    public static LodGroup Wrap(Component component) => new(component);
    public Component Component { get; }
    public LodGroup(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Transição LOD 0 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition0
    {
        get => Component.GetFloat("transition_0");
        set => Component.SetFloat("transition_0", value);
    }
    /// <summary>Transição LOD 1 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition1
    {
        get => Component.GetFloat("transition_1");
        set => Component.SetFloat("transition_1", value);
    }
    /// <summary>Transição LOD 2 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition2
    {
        get => Component.GetFloat("transition_2");
        set => Component.SetFloat("transition_2", value);
    }
    /// <summary>Transição LOD 3 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition3
    {
        get => Component.GetFloat("transition_3");
        set => Component.SetFloat("transition_3", value);
    }
    /// <summary>Tamanho. Maior extensão do nível mais detalhado; Recalcular mede a malha</summary>
    /// <remarks>Faixa válida: 0.001 a 1000000.</remarks>
    public float Size
    {
        get => Component.GetFloat("size");
        set => Component.SetFloat("size", value);
    }
    /// <summary>Largura do fade LOD 0. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth0
    {
        get => Component.GetFloat("fade_width_0");
        set => Component.SetFloat("fade_width_0", value);
    }
    /// <summary>Largura do fade LOD 1. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth1
    {
        get => Component.GetFloat("fade_width_1");
        set => Component.SetFloat("fade_width_1", value);
    }
    /// <summary>Largura do fade LOD 2. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth2
    {
        get => Component.GetFloat("fade_width_2");
        set => Component.SetFloat("fade_width_2", value);
    }
    /// <summary>Largura do fade LOD 3. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth3
    {
        get => Component.GetFloat("fade_width_3");
        set => Component.SetFloat("fade_width_3", value);
    }
    /// <summary>Animate Cross-fading. Troca por tempo em vez da faixa de largura</summary>
    public bool AnimateCrossFading
    {
        get => Component.GetBool("animate_cross_fading");
        set => Component.SetBool("animate_cross_fading", value);
    }
    public enum LevelCountOption : uint
    {
        V1 = 1,
        V2 = 2,
        V3 = 3,
        V4 = 4,
    }
    /// <summary>Níveis</summary>
    public LevelCountOption LevelCount
    {
        get => (LevelCountOption)Component.GetEnum("level_count");
        set => Component.SetEnum("level_count", (uint)value);
    }
    public enum FadeModeOption : uint
    {
        Nenhum = 0,
        CrossFade = 1,
    }
    /// <summary>Fade Mode</summary>
    public FadeModeOption FadeMode
    {
        get => (FadeModeOption)Component.GetEnum("fade_mode");
        set => Component.SetEnum("fade_mode", (uint)value);
    }
    public enum ForceLevelOption : uint
    {
        Automatico = 0,
        LOD0 = 1,
        LOD1 = 2,
        LOD2 = 3,
        LOD3 = 4,
    }
    /// <summary>Forçar nível. Estado de execução: 0 automático, n força o LOD n-1</summary>
    public ForceLevelOption ForceLevel
    {
        get => (ForceLevelOption)Component.GetEnum("force_level");
        set => Component.SetEnum("force_level", (uint)value);
    }
    /// <summary>Objetos LOD 0</summary>
    public ObjectReference Level0
    {
        get => Component.GetReference("level_0");
        set => Component.SetReference("level_0", value);
    }
    /// <summary>Objetos LOD 1</summary>
    public ObjectReference Level1
    {
        get => Component.GetReference("level_1");
        set => Component.SetReference("level_1", value);
    }
    /// <summary>Objetos LOD 2</summary>
    public ObjectReference Level2
    {
        get => Component.GetReference("level_2");
        set => Component.SetReference("level_2", value);
    }
    /// <summary>Objetos LOD 3</summary>
    public ObjectReference Level3
    {
        get => Component.GetReference("level_3");
        set => Component.SetReference("level_3", value);
    }
}

/// <summary>Ambiente: Céu, atmosfera, neblina e pós globais ou por volume. Família Renderização · Ambiente.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/Volumes.html</remarks>
public readonly struct EnvironmentVolume : IComponentFacade<EnvironmentVolume>
{
    public static string TypeId => "astra.render.environment";
    public static EnvironmentVolume Wrap(Component component) => new(component);
    public Component Component { get; }
    public EnvironmentVolume(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Cor do zênite. Cor linear RGB</summary>
    public Vector3 SkyZenith
    {
        get => new(Component.GetFloat("sky_zenith.r"), Component.GetFloat("sky_zenith.g"), Component.GetFloat("sky_zenith.b"));
        set
        {
            Component.SetFloat("sky_zenith.r", value.X);
            Component.SetFloat("sky_zenith.g", value.Y);
            Component.SetFloat("sky_zenith.b", value.Z);
        }
    }
    /// <summary>Cor do horizonte. Cor linear RGB</summary>
    public Vector3 SkyHorizon
    {
        get => new(Component.GetFloat("sky_horizon.r"), Component.GetFloat("sky_horizon.g"), Component.GetFloat("sky_horizon.b"));
        set
        {
            Component.SetFloat("sky_horizon.r", value.X);
            Component.SetFloat("sky_horizon.g", value.Y);
            Component.SetFloat("sky_horizon.b", value.Z);
        }
    }
    /// <summary>Cor do chão. Cor linear RGB</summary>
    public Vector3 Ground
    {
        get => new(Component.GetFloat("ground.r"), Component.GetFloat("ground.g"), Component.GetFloat("ground.b"));
        set
        {
            Component.SetFloat("ground.r", value.X);
            Component.SetFloat("ground.g", value.Y);
            Component.SetFloat("ground.b", value.Z);
        }
    }
    /// <summary>Cor da neblina. Cor linear RGB</summary>
    public Vector3 FogColor
    {
        get => new(Component.GetFloat("fog_color.r"), Component.GetFloat("fog_color.g"), Component.GetFloat("fog_color.b"));
        set
        {
            Component.SetFloat("fog_color.r", value.X);
            Component.SetFloat("fog_color.g", value.Y);
            Component.SetFloat("fog_color.b", value.Z);
        }
    }
    /// <summary>Tamanho da caixa</summary>
    public Vector3 BoxSize
    {
        get => new(Component.GetFloat("box_size.x"), Component.GetFloat("box_size.y"), Component.GetFloat("box_size.z"));
        set
        {
            Component.SetFloat("box_size.x", value.X);
            Component.SetFloat("box_size.y", value.Y);
            Component.SetFloat("box_size.z", value.Z);
        }
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Força atmosférica</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Atmosphere
    {
        get => Component.GetFloat("atmosphere");
        set => Component.SetFloat("atmosphere", value);
    }
    /// <summary>Diâmetro do sol (°)</summary>
    /// <remarks>Faixa válida: 0.05 a 10.</remarks>
    public float SunDiskDegrees
    {
        get => Component.GetFloat("sun_disk_degrees");
        set => Component.SetFloat("sun_disk_degrees", value);
    }
    /// <summary>Brilho do disco solar</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float SunDiskIntensity
    {
        get => Component.GetFloat("sun_disk_intensity");
        set => Component.SetFloat("sun_disk_intensity", value);
    }
    /// <summary>Energia da neblina (×)</summary>
    /// <remarks>Faixa válida: 0 a 65504.</remarks>
    public float FogLightEnergy
    {
        get => Component.GetFloat("fog_light_energy");
        set => Component.SetFloat("fog_light_energy", value);
    }
    /// <summary>Densidade (1/m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FogDensity
    {
        get => Component.GetFloat("fog_density");
        set => Component.SetFloat("fog_density", value);
    }
    /// <summary>Início (m)</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float FogStart
    {
        get => Component.GetFloat("fog_start");
        set => Component.SetFloat("fog_start", value);
    }
    /// <summary>Altura base (m)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float FogBaseHeight
    {
        get => Component.GetFloat("fog_base_height");
        set => Component.SetFloat("fog_base_height", value);
    }
    /// <summary>Decaimento por altura (1/m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float FogHeightFalloff
    {
        get => Component.GetFloat("fog_height_falloff");
        set => Component.SetFloat("fog_height_falloff", value);
    }
    /// <summary>Compensação (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float ExposureEv
    {
        get => Component.GetFloat("exposure_ev");
        set => Component.SetFloat("exposure_ev", value);
    }
    /// <summary>Limiar do bloom</summary>
    /// <remarks>Faixa válida: 0 a 64.</remarks>
    public float BloomThreshold
    {
        get => Component.GetFloat("bloom_threshold");
        set => Component.SetFloat("bloom_threshold", value);
    }
    /// <summary>Intensidade do bloom</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float BloomIntensity
    {
        get => Component.GetFloat("bloom_intensity");
        set => Component.SetFloat("bloom_intensity", value);
    }
    /// <summary>Contraste</summary>
    /// <remarks>Faixa válida: 0.5 a 2.</remarks>
    public float Contrast
    {
        get => Component.GetFloat("contrast");
        set => Component.SetFloat("contrast", value);
    }
    /// <summary>Saturação</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float Saturation
    {
        get => Component.GetFloat("saturation");
        set => Component.SetFloat("saturation", value);
    }
    /// <summary>Intensidade da vinheta</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float VignetteIntensity
    {
        get => Component.GetFloat("vignette_intensity");
        set => Component.SetFloat("vignette_intensity", value);
    }
    /// <summary>Intensidade do grão</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FilmGrainIntensity
    {
        get => Component.GetFloat("film_grain_intensity");
        set => Component.SetFloat("film_grain_intensity", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.05 a 10.</remarks>
    public float AmbientOcclusionRadius
    {
        get => Component.GetFloat("ambient_occlusion_radius");
        set => Component.SetFloat("ambient_occlusion_radius", value);
    }
    /// <summary>Intensidade</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float AmbientOcclusionIntensity
    {
        get => Component.GetFloat("ambient_occlusion_intensity");
        set => Component.SetFloat("ambient_occlusion_intensity", value);
    }
    /// <summary>Potência</summary>
    /// <remarks>Faixa válida: 0.1 a 4.</remarks>
    public float AmbientOcclusionPower
    {
        get => Component.GetFloat("ambient_occlusion_power");
        set => Component.SetFloat("ambient_occlusion_power", value);
    }
    /// <summary>Viés (m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AmbientOcclusionBias
    {
        get => Component.GetFloat("ambient_occlusion_bias");
        set => Component.SetFloat("ambient_occlusion_bias", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Distância de mistura (m)</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float BlendDistance
    {
        get => Component.GetFloat("blend_distance");
        set => Component.SetFloat("blend_distance", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 100000.</remarks>
    public float SphereRadius
    {
        get => Component.GetFloat("sphere_radius");
        set => Component.SetFloat("sphere_radius", value);
    }
    /// <summary>Difuso indireto (×)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float IndirectDiffuse
    {
        get => Component.GetFloat("indirect_diffuse");
        set => Component.SetFloat("indirect_diffuse", value);
    }
    /// <summary>Reflexo indireto (×)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float IndirectSpecular
    {
        get => Component.GetFloat("indirect_specular");
        set => Component.SetFloat("indirect_specular", value);
    }
    /// <summary>Intensidade (×)</summary>
    /// <remarks>Faixa válida: 0 a 16.</remarks>
    public float PhysicalSkyIntensity
    {
        get => Component.GetFloat("physical_sky_intensity");
        set => Component.SetFloat("physical_sky_intensity", value);
    }
    /// <summary>Densidade do ar (×)</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float AirDensity
    {
        get => Component.GetFloat("air_density");
        set => Component.SetFloat("air_density", value);
    }
    /// <summary>Densidade de aerossóis (×)</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float AerosolDensity
    {
        get => Component.GetFloat("aerosol_density");
        set => Component.SetFloat("aerosol_density", value);
    }
    /// <summary>Anisotropia dos aerossóis (g)</summary>
    /// <remarks>Faixa válida: 0 a 0.95.</remarks>
    public float AerosolAnisotropy
    {
        get => Component.GetFloat("aerosol_anisotropy");
        set => Component.SetFloat("aerosol_anisotropy", value);
    }
    /// <summary>Raio do planeta (km)</summary>
    /// <remarks>Faixa válida: 1 a 100000.</remarks>
    public float PlanetRadiusKm
    {
        get => Component.GetFloat("planet_radius_km");
        set => Component.SetFloat("planet_radius_km", value);
    }
    /// <summary>Altura do observador (km)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ObserverHeightKm
    {
        get => Component.GetFloat("observer_height_km");
        set => Component.SetFloat("observer_height_km", value);
    }
    /// <summary>Escala Rayleigh (km)</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float RayleighScaleHeightKm
    {
        get => Component.GetFloat("rayleigh_scale_height_km");
        set => Component.SetFloat("rayleigh_scale_height_km", value);
    }
    /// <summary>Escala de aerossóis (km)</summary>
    /// <remarks>Faixa válida: 0.05 a 50.</remarks>
    public float AerosolScaleHeightKm
    {
        get => Component.GetFloat("aerosol_scale_height_km");
        set => Component.SetFloat("aerosol_scale_height_km", value);
    }
    /// <summary>Altura da atmosfera (km)</summary>
    /// <remarks>Faixa válida: 1 a 1000.</remarks>
    public float AtmosphereHeightKm
    {
        get => Component.GetFloat("atmosphere_height_km");
        set => Component.SetFloat("atmosphere_height_km", value);
    }
    /// <summary>Albedo médio do solo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GroundAlbedo
    {
        get => Component.GetFloat("ground_albedo");
        set => Component.SetFloat("ground_albedo", value);
    }
    /// <summary>EV mínimo (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float AutoExposureMinEv
    {
        get => Component.GetFloat("auto_exposure_min_ev");
        set => Component.SetFloat("auto_exposure_min_ev", value);
    }
    /// <summary>EV máximo (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float AutoExposureMaxEv
    {
        get => Component.GetFloat("auto_exposure_max_ev");
        set => Component.SetFloat("auto_exposure_max_ev", value);
    }
    /// <summary>Corte baixo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AutoExposureLowPercent
    {
        get => Component.GetFloat("auto_exposure_low_percent");
        set => Component.SetFloat("auto_exposure_low_percent", value);
    }
    /// <summary>Corte alto</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AutoExposureHighPercent
    {
        get => Component.GetFloat("auto_exposure_high_percent");
        set => Component.SetFloat("auto_exposure_high_percent", value);
    }
    /// <summary>Cinza alvo</summary>
    /// <remarks>Faixa válida: 0.01 a 1.</remarks>
    public float AutoExposureTargetGrey
    {
        get => Component.GetFloat("auto_exposure_target_grey");
        set => Component.SetFloat("auto_exposure_target_grey", value);
    }
    /// <summary>Velocidade ao escurecer (EV/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float AutoExposureSpeedUp
    {
        get => Component.GetFloat("auto_exposure_speed_up");
        set => Component.SetFloat("auto_exposure_speed_up", value);
    }
    /// <summary>Velocidade ao clarear (EV/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float AutoExposureSpeedDown
    {
        get => Component.GetFloat("auto_exposure_speed_down");
        set => Component.SetFloat("auto_exposure_speed_down", value);
    }
    /// <summary>Rotação HDRI (°)</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float HdriRotationDegrees
    {
        get => Component.GetFloat("hdri_rotation_degrees");
        set => Component.SetFloat("hdri_rotation_degrees", value);
    }
    /// <summary>Exposição HDRI (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float HdriExposureEv
    {
        get => Component.GetFloat("hdri_exposure_ev");
        set => Component.SetFloat("hdri_exposure_ev", value);
    }
    /// <summary>Ativo. Participa da seleção por prioridade</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Neblina. Aplica neblina exponencial uniforme ou com decaimento por altura</summary>
    public bool Fog
    {
        get => Component.GetBool("fog");
        set => Component.SetBool("fog", value);
    }
    /// <summary>Pós-processamento. Ativa os overrides autorais desta cena</summary>
    public bool Post
    {
        get => Component.GetBool("post");
        set => Component.SetBool("post", value);
    }
    /// <summary>Exposição automática. Mede luminância HDR e adapta a exposição por câmera</summary>
    public bool AutoExposure
    {
        get => Component.GetBool("auto_exposure");
        set => Component.SetBool("auto_exposure", value);
    }
    /// <summary>Peso central. Prioriza o centro da imagem na medição</summary>
    public bool AutoExposureCenterWeighted
    {
        get => Component.GetBool("auto_exposure_center_weighted");
        set => Component.SetBool("auto_exposure_center_weighted", value);
    }
    /// <summary>Bloom. Espalha altas luzes antes do tonemap</summary>
    public bool Bloom
    {
        get => Component.GetBool("bloom");
        set => Component.SetBool("bloom", value);
    }
    /// <summary>Vinheta. Escurece gradualmente as bordas</summary>
    public bool Vignette
    {
        get => Component.GetBool("vignette");
        set => Component.SetBool("vignette", value);
    }
    /// <summary>Grão de filme. Adiciona grão sensível à luminância depois da resolução temporal</summary>
    public bool FilmGrain
    {
        get => Component.GetBool("film_grain");
        set => Component.SetBool("film_grain", value);
    }
    /// <summary>Oclusão ambiente. Escurece contatos e concavidades a partir da profundidade da câmera</summary>
    public bool AmbientOcclusion
    {
        get => Component.GetBool("ambient_occlusion");
        set => Component.SetBool("ambient_occlusion", value);
    }
    /// <summary>Sobrescrever céu. Participa da mistura de céu e atmosfera</summary>
    public bool OverrideSky
    {
        get => Component.GetBool("override_sky");
        set => Component.SetBool("override_sky", value);
    }
    /// <summary>Sobrescrever neblina. Participa da mistura de neblina</summary>
    public bool OverrideFog
    {
        get => Component.GetBool("override_fog");
        set => Component.SetBool("override_fog", value);
    }
    /// <summary>Sobrescrever pós. Participa da mistura de exposição e pós</summary>
    public bool OverridePost
    {
        get => Component.GetBool("override_post");
        set => Component.SetBool("override_post", value);
    }
    /// <summary>Sobrescrever luz indireta. Participa da mistura de difuso e reflexo indiretos</summary>
    public bool OverrideIndirect
    {
        get => Component.GetBool("override_indirect");
        set => Component.SetBool("override_indirect", value);
    }
    /// <summary>Alta qualidade. Aumenta as amostras de vista e luz; custa mais GPU</summary>
    public bool PhysicalAtmosphereHighQuality
    {
        get => Component.GetBool("physical_atmosphere_high_quality");
        set => Component.SetBool("physical_atmosphere_high_quality", value);
    }
    public enum SkyOption : uint
    {
        HDRI = 0,
        Atmosfera = 1,
        AtmosferaFisica = 2,
    }
    /// <summary>Céu. Fonte visual do céu</summary>
    public SkyOption Sky
    {
        get => (SkyOption)Component.GetEnum("sky");
        set => Component.SetEnum("sky", (uint)value);
    }
    public enum ToneMapperOption : uint
    {
        Reinhard = 0,
        ACES = 1,
        AgX = 2,
    }
    /// <summary>Tonemapping. Curva aplicada ao HDR antes da saída</summary>
    public ToneMapperOption ToneMapper
    {
        get => (ToneMapperOption)Component.GetEnum("tone_mapper");
        set => Component.SetEnum("tone_mapper", (uint)value);
    }
    public enum VolumeShapeOption : uint
    {
        Global = 0,
        Caixa = 1,
        Esfera = 2,
    }
    /// <summary>Modo. Global afeta toda vista; formas usam o Transform do objeto</summary>
    public VolumeShapeOption VolumeShape
    {
        get => (VolumeShapeOption)Component.GetEnum("volume_shape");
        set => Component.SetEnum("volume_shape", (uint)value);
    }
    public enum VolumeLayerOption : uint
    {
        Ambiente0 = 0,
        Ambiente1 = 1,
        Ambiente2 = 2,
        Ambiente3 = 3,
        Ambiente4 = 4,
        Ambiente5 = 5,
        Ambiente6 = 6,
        Ambiente7 = 7,
    }
    /// <summary>Camada. Filtro consultado pela câmera</summary>
    public VolumeLayerOption VolumeLayer
    {
        get => (VolumeLayerOption)Component.GetEnum("volume_layer");
        set => Component.SetEnum("volume_layer", (uint)value);
    }
    /// <summary>Perfil. Recurso do projeto por slot</summary>
    public AssetGuid GetProfile(uint slot = 0) => Component.GetResource("profile", slot);
    public void SetProfile(AssetGuid value, uint slot = 0) => Component.SetResource("profile", value, slot);
    /// <summary>Mapa HDRI. Recurso do projeto por slot</summary>
    public AssetGuid GetEnvironmentMap(uint slot = 0) => Component.GetResource("environment_map", slot);
    public void SetEnvironmentMap(AssetGuid value, uint slot = 0) => Component.SetResource("environment_map", value, slot);
}

/// <summary>Luz: Direcional, pontual ou spot. Família Luz · Luzes.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html</remarks>
public readonly struct Light : IComponentFacade<Light>
{
    public static string TypeId => "astra.render.light";
    public static Light Wrap(Component component) => new(component);
    public Component Component { get; }
    public Light(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Cor linear. Cor linear RGB</summary>
    public Vector3 Color
    {
        get => new(Component.GetFloat("color.r"), Component.GetFloat("color.g"), Component.GetFloat("color.b"));
        set
        {
            Component.SetFloat("color.r", value.X);
            Component.SetFloat("color.g", value.Y);
            Component.SetFloat("color.b", value.Z);
        }
    }
    /// <summary>Temperatura (K). Multiplicada pela cor linear; D65 é 6500 K</summary>
    /// <remarks>Faixa válida: 1667 a 25000.</remarks>
    public float ColorTemperature
    {
        get => Component.GetFloat("color_temperature");
        set => Component.SetFloat("color_temperature", value);
    }
    /// <summary>Intensidade</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float Intensity
    {
        get => Component.GetFloat("intensity");
        set => Component.SetFloat("intensity", value);
    }
    /// <summary>Alcance (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000.</remarks>
    public float Range
    {
        get => Component.GetFloat("range");
        set => Component.SetFloat("range", value);
    }
    /// <summary>Meio-cone interno (°)</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float InnerAngle
    {
        get => Component.GetFloat("inner_angle");
        set => Component.SetFloat("inner_angle", value);
    }
    /// <summary>Meio-cone externo (°)</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float OuterAngle
    {
        get => Component.GetFloat("outer_angle");
        set => Component.SetFloat("outer_angle", value);
    }
    /// <summary>Força da sombra. 1 é sombra opaca; abaixo disso a luz vaza pelo oclusor</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShadowStrength
    {
        get => Component.GetFloat("shadow_strength");
        set => Component.SetFloat("shadow_strength", value);
    }
    /// <summary>Desvio (texel). Afasta a comparação de profundidade e some com a acne</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float ShadowBias
    {
        get => Component.GetFloat("shadow_bias");
        set => Component.SetFloat("shadow_bias", value);
    }
    /// <summary>Desvio na normal (texel). Desloca a amostra ao longo da normal; some com o serrilhado da borda</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float ShadowNormalBias
    {
        get => Component.GetFloat("shadow_normal_bias");
        set => Component.SetFloat("shadow_normal_bias", value);
    }
    /// <summary>Plano próximo da sombra (m). Perto demais perde precisão; longe demais corta o que está junto da lâmpada</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float ShadowNearPlane
    {
        get => Component.GetFloat("shadow_near_plane");
        set => Component.SetFloat("shadow_near_plane", value);
    }
    /// <summary>Acesa. Ativa a contribuição desta luz</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Filtro por temperatura. Multiplica a cor pela temperatura de corpo negro</summary>
    public bool UseColorTemperature
    {
        get => Component.GetBool("use_color_temperature");
        set => Component.SetBool("use_color_temperature", value);
    }
    public enum KindOption : uint
    {
        Direcional = 0,
        Pontual = 1,
        Spot = 2,
    }
    /// <summary>Modalidade. Tipo de emissão da luz</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum UnitOption : uint
    {
        InternaLegada = 0,
        LuxCandela = 1,
        LuxLumen = 2,
    }
    /// <summary>Unidade. Direcional usa lux; luz local usa candela ou lúmen</summary>
    public UnitOption Unit
    {
        get => (UnitOption)Component.GetEnum("unit");
        set => Component.SetEnum("unit", (uint)value);
    }
    public enum ShadowModeOption : uint
    {
        Nenhuma = 0,
        Dura = 1,
        Suave = 2,
    }
    /// <summary>Sombra. Projeção de sombra desta luz no atlas local</summary>
    public ShadowModeOption ShadowMode
    {
        get => (ShadowModeOption)Component.GetEnum("shadow_mode");
        set => Component.SetEnum("shadow_mode", (uint)value);
    }
    public enum ShadowResolutionOption : uint
    {
        Automatica = 0,
        Baixa = 1,
        Media = 2,
        Alta = 3,
        MuitoAlta = 4,
    }
    /// <summary>Resolução da sombra. Automática escolhe pelo tamanho da luz na tela</summary>
    public ShadowResolutionOption ShadowResolution
    {
        get => (ShadowResolutionOption)Component.GetEnum("shadow_resolution");
        set => Component.SetEnum("shadow_resolution", (uint)value);
    }
}

/// <summary>Câmera: Projeção e enquadramento. Família Câmera · Projeção.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html</remarks>
public readonly struct Camera : IComponentFacade<Camera>
{
    public static string TypeId => "astra.camera";
    public static Camera Wrap(Component component) => new(component);
    public Component Component { get; }
    public Camera(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Campo vertical (°)</summary>
    /// <remarks>Faixa válida: 1 a 170.</remarks>
    public float VerticalFov
    {
        get => Component.GetFloat("vertical_fov");
        set => Component.SetFloat("vertical_fov", value);
    }
    /// <summary>Próximo (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float NearPlane
    {
        get => Component.GetFloat("near_plane");
        set => Component.SetFloat("near_plane", value);
    }
    /// <summary>Distante (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000000.</remarks>
    public float FarPlane
    {
        get => Component.GetFloat("far_plane");
        set => Component.SetFloat("far_plane", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Meia altura (m). Metade da altura visível. Zoom altera esta extensão.</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float OrthographicHalfHeight
    {
        get => Component.GetFloat("orthographic_half_height");
        set => Component.SetFloat("orthographic_half_height", value);
    }
    /// <summary>Usar no Play</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum ProjectionOption : uint
    {
        Perspectiva = 0,
        Ortografica = 1,
    }
    /// <summary>Projeção</summary>
    public ProjectionOption Projection
    {
        get => (ProjectionOption)Component.GetEnum("projection");
        set => Component.SetEnum("projection", (uint)value);
    }
    public enum EnvironmentMaskOption : uint
    {
        TodosOsAmbientes = 4294967295,
        Ambiente0 = 1,
        Ambiente1 = 2,
        Ambiente2 = 4,
        Ambiente3 = 8,
        Ambiente4 = 16,
        Ambiente5 = 32,
        Ambiente6 = 64,
        Ambiente7 = 128,
    }
    /// <summary>Ambientes. Volumes que esta câmera consulta</summary>
    public EnvironmentMaskOption EnvironmentMask
    {
        get => (EnvironmentMaskOption)Component.GetEnum("environment_mask");
        set => Component.SetEnum("environment_mask", (uint)value);
    }
}

/// <summary>Olhar: Rotação local da câmera por entrada ou script. Família Câmera · Controle.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachinePanTilt.html</remarks>
public readonly struct CameraLook : IComponentFacade<CameraLook>
{
    public static string TypeId => "astra.camera.look";
    public static CameraLook Wrap(Component component) => new(component);
    public Component Component { get; }
    public CameraLook(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Sensibilidade horizontal graus/tela (°)</summary>
    /// <remarks>Faixa válida: 0 a 720.</remarks>
    public float YawSensitivity
    {
        get => Component.GetFloat("yaw_sensitivity");
        set => Component.SetFloat("yaw_sensitivity", value);
    }
    /// <summary>Sensibilidade vertical graus/tela (°)</summary>
    /// <remarks>Faixa válida: 0 a 720.</remarks>
    public float PitchSensitivity
    {
        get => Component.GetFloat("pitch_sensitivity");
        set => Component.SetFloat("pitch_sensitivity", value);
    }
    /// <summary>Limite vertical graus (°)</summary>
    /// <remarks>Faixa válida: 1 a 89.</remarks>
    public float PitchLimit
    {
        get => Component.GetFloat("pitch_limit");
        set => Component.SetFloat("pitch_limit", value);
    }
}

/// <summary>Acompanhar alvo: Posiciona a câmera após física e animação. Família Câmera · Controle.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineFollow.html</remarks>
public readonly struct CameraFollow : IComponentFacade<CameraFollow>
{
    public static string TypeId => "astra.camera.follow";
    public static CameraFollow Wrap(Component component) => new(component);
    public Component Component { get; }
    public CameraFollow(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set
        {
            Component.SetFloat("offset_x", value.X);
            Component.SetFloat("offset_y", value.Y);
            Component.SetFloat("offset_z", value.Z);
        }
    }
    /// <summary>Amortecimento (s)</summary>
    /// <remarks>Faixa válida: 0 a 30.</remarks>
    public float DampingSeconds
    {
        get => Component.GetFloat("damping_seconds");
        set => Component.SetFloat("damping_seconds", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Corpo físico: Massa e resposta física. Família Física 3D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html</remarks>
public readonly struct PhysicsBody : IComponentFacade<PhysicsBody>
{
    public static string TypeId => "astra.physics.body";
    public static PhysicsBody Wrap(Component component) => new(component);
    public Component Component { get; }
    public PhysicsBody(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Velocidade inicial</summary>
    public Vector3 Velocity
    {
        get => new(Component.GetFloat("velocity_x"), Component.GetFloat("velocity_y"), Component.GetFloat("velocity_z"));
        set
        {
            Component.SetFloat("velocity_x", value.X);
            Component.SetFloat("velocity_y", value.Y);
            Component.SetFloat("velocity_z", value.Z);
        }
    }
    /// <summary>Giro inicial</summary>
    public Vector3 AngularVelocity
    {
        get => new(Component.GetFloat("angular_x"), Component.GetFloat("angular_y"), Component.GetFloat("angular_z"));
        set
        {
            Component.SetFloat("angular_x", value.X);
            Component.SetFloat("angular_y", value.Y);
            Component.SetFloat("angular_z", value.Z);
        }
    }
    /// <summary>Massa kg (kg)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000000.</remarks>
    public float Mass
    {
        get => Component.GetFloat("mass");
        set => Component.SetFloat("mass", value);
    }
    /// <summary>Atrito</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Friction
    {
        get => Component.GetFloat("friction");
        set => Component.SetFloat("friction", value);
    }
    /// <summary>Restituição</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Restitution
    {
        get => Component.GetFloat("restitution");
        set => Component.SetFloat("restitution", value);
    }
    /// <summary>Amortecimento linear</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearDamping
    {
        get => Component.GetFloat("linear_damping");
        set => Component.SetFloat("linear_damping", value);
    }
    /// <summary>Amortecimento angular</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularDamping
    {
        get => Component.GetFloat("angular_damping");
        set => Component.SetFloat("angular_damping", value);
    }
    /// <summary>Multiplicador da gravidade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityFactor
    {
        get => Component.GetFloat("gravity_factor");
        set => Component.SetFloat("gravity_factor", value);
    }
    /// <summary>Sensor sem resposta</summary>
    public bool Sensor
    {
        get => Component.GetBool("sensor");
        set => Component.SetBool("sensor", value);
    }
    /// <summary>Permitir repouso</summary>
    public bool AllowSleep
    {
        get => Component.GetBool("allow_sleep");
        set => Component.SetBool("allow_sleep", value);
    }
    public enum MotionOption : uint
    {
        Estatico = 0,
        Cinematico = 1,
        Dinamico = 2,
    }
    /// <summary>Movimento</summary>
    public MotionOption Motion
    {
        get => (MotionOption)Component.GetEnum("motion");
        set => Component.SetEnum("motion", (uint)value);
    }
}

/// <summary>Personagem: Locomoção com cápsula. Família Física 3D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html</remarks>
public readonly struct Character : IComponentFacade<Character>
{
    public static string TypeId => "astra.physics.character";
    public static Character Wrap(Component component) => new(component);
    public Component Component { get; }
    public Character(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Raio m (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia altura do cilindro m (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float HalfHeight
    {
        get => Component.GetFloat("half_height");
        set => Component.SetFloat("half_height", value);
    }
    /// <summary>Altura dos olhos m (m)</summary>
    /// <remarks>Faixa válida: 0.02 a 20.</remarks>
    public float EyeHeight
    {
        get => Component.GetFloat("eye_height");
        set => Component.SetFloat("eye_height", value);
    }
    /// <summary>Velocidade m/s (m/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 100.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Inclinação máxima graus (°)</summary>
    /// <remarks>Faixa válida: 1 a 89.</remarks>
    public float SlopeDegrees
    {
        get => Component.GetFloat("slope_degrees");
        set => Component.SetFloat("slope_degrees", value);
    }
    /// <summary>Velocidade do salto m/s (m/s)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float JumpSpeed
    {
        get => Component.GetFloat("jump_speed");
        set => Component.SetFloat("jump_speed", value);
    }
}

/// <summary>Colisor 3D: Volume de contato. Família Física 3D · Formas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html</remarks>
public readonly struct Collider : IComponentFacade<Collider>
{
    public static string TypeId => "astra.physics.collider";
    public static Collider Wrap(Component component) => new(component);
    public Component Component { get; }
    public Collider(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Meia extensão</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set
        {
            Component.SetFloat("half_x", value.X);
            Component.SetFloat("half_y", value.Y);
            Component.SetFloat("half_z", value.Z);
        }
    }
    /// <summary>Centro</summary>
    public Vector3 Center
    {
        get => new(Component.GetFloat("center_x"), Component.GetFloat("center_y"), Component.GetFloat("center_z"));
        set
        {
            Component.SetFloat("center_x", value.X);
            Component.SetFloat("center_y", value.Y);
            Component.SetFloat("center_z", value.Z);
        }
    }
    /// <summary>Rotação</summary>
    public Vector3 Rotation
    {
        get => new(Component.GetFloat("rotation_x"), Component.GetFloat("rotation_y"), Component.GetFloat("rotation_z"));
        set
        {
            Component.SetFloat("rotation_x", value.X);
            Component.SetFloat("rotation_y", value.Y);
            Component.SetFloat("rotation_z", value.Z);
        }
    }
    /// <summary>Raio</summary>
    /// <remarks>Faixa válida: 0.01 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia altura cilíndrica</summary>
    /// <remarks>Faixa válida: 0.01 a 10000.</remarks>
    public float HalfHeight
    {
        get => Component.GetFloat("half_height");
        set => Component.SetFloat("half_height", value);
    }
    /// <summary>Tolerância do casco (u). Pontos até esta distância podem ficar fora do casco; valores maiores geram cascos mais simples.</summary>
    /// <remarks>Faixa válida: 0.00001 a 1.</remarks>
    public float HullTolerance
    {
        get => Component.GetFloat("hull_tolerance");
        set => Component.SetFloat("hull_tolerance", value);
    }
    /// <summary>Ângulo de aresta ativa (°). Separa arestas de contato em superfícies com mudança de normal acima deste ângulo.</summary>
    /// <remarks>Faixa válida: 0 a 90.</remarks>
    public float ActiveEdgeAngle
    {
        get => Component.GetFloat("active_edge_angle");
        set => Component.SetFloat("active_edge_angle", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Convexo. Casco convexo da malha: aceita corpo dinâmico. Desligado usa os triângulos exatos e só vale em corpo estático ou cinemático.</summary>
    public bool Convex
    {
        get => Component.GetBool("convex");
        set => Component.SetBool("convex", value);
    }
    /// <summary>Soldar vértices iguais. Compartilha vértices coincidentes antes de criar a malha física, reduzindo costuras internas.</summary>
    public bool WeldVertices
    {
        get => Component.GetBool("weld_vertices");
        set => Component.SetBool("weld_vertices", value);
    }
    /// <summary>Otimizar para o jogo. Constrói uma árvore de busca mais eficiente; desligue para cozinhar mais rápido durante iterações.</summary>
    public bool OptimizeCooking
    {
        get => Component.GetBool("optimize_cooking");
        set => Component.SetBool("optimize_cooking", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
        Capsula = 2,
        Malha = 3,
    }
    /// <summary>Forma</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    /// <summary>Corpo proprietário</summary>
    public ObjectReference Owner
    {
        get => Component.GetReference("owner");
        set => Component.SetReference("owner", value);
    }
    /// <summary>Malha de colisão. Recurso do projeto por slot</summary>
    public AssetGuid GetCollisionMesh(uint slot = 0) => Component.GetResource("collision_mesh", slot);
    public void SetCollisionMesh(AssetGuid value, uint slot = 0) => Component.SetResource("collision_mesh", value, slot);
}

/// <summary>Junta: Conexão, limites e motor entre corpos. Família Física 3D · Juntas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-HingeJoint.html</remarks>
public readonly struct Joint : IComponentFacade<Joint>
{
    public static string TypeId => "astra.physics.joint";
    public static Joint Wrap(Component component) => new(component);
    public Component Component { get; }
    public Joint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Âncora A</summary>
    public Vector3 AnchorA
    {
        get => new(Component.GetFloat("anchor_a_x"), Component.GetFloat("anchor_a_y"), Component.GetFloat("anchor_a_z"));
        set
        {
            Component.SetFloat("anchor_a_x", value.X);
            Component.SetFloat("anchor_a_y", value.Y);
            Component.SetFloat("anchor_a_z", value.Z);
        }
    }
    /// <summary>Âncora B</summary>
    public Vector3 AnchorB
    {
        get => new(Component.GetFloat("anchor_b_x"), Component.GetFloat("anchor_b_y"), Component.GetFloat("anchor_b_z"));
        set
        {
            Component.SetFloat("anchor_b_x", value.X);
            Component.SetFloat("anchor_b_y", value.Y);
            Component.SetFloat("anchor_b_z", value.Z);
        }
    }
    /// <summary>Eixo A</summary>
    public Vector3 AxisA
    {
        get => new(Component.GetFloat("axis_a_x"), Component.GetFloat("axis_a_y"), Component.GetFloat("axis_a_z"));
        set
        {
            Component.SetFloat("axis_a_x", value.X);
            Component.SetFloat("axis_a_y", value.Y);
            Component.SetFloat("axis_a_z", value.Z);
        }
    }
    /// <summary>Eixo B</summary>
    public Vector3 AxisB
    {
        get => new(Component.GetFloat("axis_b_x"), Component.GetFloat("axis_b_y"), Component.GetFloat("axis_b_z"));
        set
        {
            Component.SetFloat("axis_b_x", value.X);
            Component.SetFloat("axis_b_y", value.Y);
            Component.SetFloat("axis_b_z", value.Z);
        }
    }
    /// <summary>Limite mínimo</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LimitMin
    {
        get => Component.GetFloat("limit_min");
        set => Component.SetFloat("limit_min", value);
    }
    /// <summary>Limite máximo</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LimitMax
    {
        get => Component.GetFloat("limit_max");
        set => Component.SetFloat("limit_max", value);
    }
    /// <summary>Velocidade do motor</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float MotorVelocity
    {
        get => Component.GetFloat("motor_velocity");
        set => Component.SetFloat("motor_velocity", value);
    }
    /// <summary>Alvo do motor</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float MotorPosition
    {
        get => Component.GetFloat("motor_position");
        set => Component.SetFloat("motor_position", value);
    }
    /// <summary>Força / torque máximo</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float MotorForce
    {
        get => Component.GetFloat("motor_force");
        set => Component.SetFloat("motor_force", value);
    }
    /// <summary>Frequência · Hz (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float SpringFrequency
    {
        get => Component.GetFloat("spring_frequency");
        set => Component.SetFloat("spring_frequency", value);
    }
    /// <summary>Amortecimento da mola</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float SpringDamping
    {
        get => Component.GetFloat("spring_damping");
        set => Component.SetFloat("spring_damping", value);
    }
    /// <summary>Ativa</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum KindOption : uint
    {
        Ponto = 0,
        Dobradica = 1,
        Deslizante = 2,
        Distancia = 3,
    }
    /// <summary>Tipo</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum MotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public MotorOption Motor
    {
        get => (MotorOption)Component.GetEnum("motor");
        set => Component.SetEnum("motor", (uint)value);
    }
    /// <summary>Conectar corpo</summary>
    public ObjectReference ConnectedBody
    {
        get => Component.GetReference("connected_body");
        set => Component.SetReference("connected_body", value);
    }
}

/// <summary>Animação: Clipes tocados e misturados no Play. Família Animação · Clipes.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Animation.html</remarks>
public readonly struct Animation : IComponentFacade<Animation>
{
    public static string TypeId => "astra.animation";
    public static Animation Wrap(Component component) => new(component);
    public Component Component { get; }
    public Animation(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    /// <summary>Velocidade (x). Velocidade inicial de cada clipe (AnimationState.speed); negativo toca de trás para frente</summary>
    /// <remarks>Faixa válida: -10 a 10.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Tocar ao iniciar. Play Automatically: o clipe padrão começa a tocar quando o Play inicia</summary>
    public bool PlayAutomatically
    {
        get => Component.GetBool("play_automatically");
        set => Component.SetBool("play_automatically", value);
    }
    public enum WrapModeOption : uint
    {
        UmaVez = 0,
        Repetir = 1,
        VaiEVolta = 2,
        SegurarNoFim = 3,
    }
    /// <summary>Repetição. Wrap Mode padrão dos clipes: o que acontece depois do fim</summary>
    public WrapModeOption WrapMode
    {
        get => (WrapModeOption)Component.GetEnum("wrap_mode");
        set => Component.SetEnum("wrap_mode", (uint)value);
    }
    public enum ClipCountOption : uint
    {
        V0 = 0,
        V1 = 1,
        V2 = 2,
        V3 = 3,
        V4 = 4,
        V5 = 5,
        V6 = 6,
        V7 = 7,
        V8 = 8,
        V9 = 9,
        V10 = 10,
        V11 = 11,
        V12 = 12,
        V13 = 13,
        V14 = 14,
        V15 = 15,
        V16 = 16,
        V17 = 17,
        V18 = 18,
        V19 = 19,
        V20 = 20,
        V21 = 21,
        V22 = 22,
        V23 = 23,
        V24 = 24,
        V25 = 25,
        V26 = 26,
        V27 = 27,
        V28 = 28,
        V29 = 29,
        V30 = 30,
        V31 = 31,
        V32 = 32,
    }
    /// <summary>Quantidade de clipes</summary>
    public ClipCountOption ClipCount
    {
        get => (ClipCountOption)Component.GetEnum("clip_count");
        set => Component.SetEnum("clip_count", (uint)value);
    }
    /// <summary>Clipe padrão. Recurso do projeto por slot</summary>
    public AssetGuid GetClip(uint slot = 0) => Component.GetResource("clip", slot);
    public void SetClip(AssetGuid value, uint slot = 0) => Component.SetResource("clip", value, slot);
    /// <summary>Clipe. Recurso do projeto por slot</summary>
    public AssetGuid GetClips(uint slot = 0) => Component.GetResource("clips", slot);
    public void SetClips(AssetGuid value, uint slot = 0) => Component.SetResource("clips", value, slot);
}
