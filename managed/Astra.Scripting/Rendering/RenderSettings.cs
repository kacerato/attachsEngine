using System.Numerics;
using Astra.Components;

namespace Astra.Rendering;

/// <summary>
/// RenderSettings da Unity (janela Lighting › Environment). Na Astra a janela
/// Iluminação edita o Ambiente global da cena: aqui é o Volume global ativo de
/// maior prioridade. Cada escrita marca a caixa do parâmetro e põe o override
/// no perfil, como mexer no campo da janela; com perfil compartilhado, a
/// primeira escrita cria a cópia própria (Volume.profile). Só no Play.
/// </summary>
/// <remarks>https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RenderSettings.html</remarks>
public static class RenderSettings
{
    /// <summary>O Volume que faz o papel da janela Lighting; null sem Volume global.</summary>
    public static Volume? SceneVolume
    {
        get
        {
            Volume? best = null;
            foreach (var volume in Volume.FindAll())
                if (volume.Enabled && volume.IsGlobal && (best is null || volume.Priority > best.Priority)) best = volume;
            return best;
        }
    }
    private static Volume Scene => SceneVolume ?? throw new WorldException(WorldStatus.ComponentMissing, "RenderSettings sem Volume global");

    private static Component Write(string propertyId)
    {
        var volume = Scene;
        volume.EnsureInstance();
        var index = VolumeParameterTable.IndexOf(propertyId);
        volume.Component.SetBool(VolumeParameterTable.GroupAddedProperties[VolumeParameterTable.Groups[index]], true);
        volume.SetParameterOverridden(index, true);
        return volume.Component;
    }
    private static Color ReadColor(string id)
    {
        var c = Scene.Component;
        return new(c.GetFloat(id + ".r"), c.GetFloat(id + ".g"), c.GetFloat(id + ".b"));
    }

    public static AmbientMode AmbientMode
    {
        get => (AmbientMode)Scene.Component.GetEnum("ambient_source");
        set => Write("ambient_source").SetEnum("ambient_source", (uint)value);
    }
    /// <summary>Cor do céu do modo Gradiente (sRGB, como o seletor).</summary>
    public static Color AmbientSkyColor { get => ReadColor("ambient_sky_color"); set => Write("ambient_sky_color").SetVector3("ambient_sky_color", new(value.R, value.G, value.B)); }
    public static Color AmbientEquatorColor { get => ReadColor("ambient_equator_color"); set => Write("ambient_equator_color").SetVector3("ambient_equator_color", new(value.R, value.G, value.B)); }
    public static Color AmbientGroundColor { get => ReadColor("ambient_ground_color"); set => Write("ambient_ground_color").SetVector3("ambient_ground_color", new(value.R, value.G, value.B)); }
    /// <summary>Cor do modo Cor (Flat).</summary>
    public static Color AmbientLight { get => ReadColor("ambient_color"); set => Write("ambient_color").SetVector3("ambient_color", new(value.R, value.G, value.B)); }
    /// <summary>Intensity Multiplier do ambiente (Indirect Lighting Controller › difuso).</summary>
    public static float AmbientIntensity { get => Scene.Component.GetFloat("indirect_diffuse"); set => Write("indirect_diffuse").SetFloat("indirect_diffuse", value); }
    public static float ReflectionIntensity { get => Scene.Component.GetFloat("indirect_specular"); set => Write("indirect_specular").SetFloat("indirect_specular", value); }

    public static bool Fog { get => Scene.Component.GetBool("fog"); set => Write("fog").SetBool("fog", value); }
    public static FogMode FogMode { get => (FogMode)Scene.Component.GetEnum("fog_mode"); set => Write("fog_mode").SetEnum("fog_mode", (uint)value); }
    public static Color FogColor { get => ReadColor("fog_color"); set => Write("fog_color").SetVector3("fog_color", new(value.R, value.G, value.B)); }
    public static float FogDensity { get => Scene.Component.GetFloat("fog_density"); set => Write("fog_density").SetFloat("fog_density", value); }
    public static float FogStartDistance { get => Scene.Component.GetFloat("fog_start"); set => Write("fog_start").SetFloat("fog_start", value); }
    public static float FogEndDistance { get => Scene.Component.GetFloat("fog_end"); set => Write("fog_end").SetFloat("fog_end", value); }

    /// <summary>Material do céu (Skybox Material); vazio usa o céu do próprio Ambiente.</summary>
    public static AssetGuid Skybox { get => Scene.Component.GetResource("skybox"); set => Write("skybox").SetResource("skybox", value); }

    /// <summary>Sun Source das Configurações de iluminação da cena.</summary>
    public static GameObject? Sun
    {
        get => SceneQuery.FindFirstObjectByType<LightingSettings>() is { } settings && settings.SunSource.ObjectId != 0
            ? GameObject.Resolve(settings.Component.Scene, settings.SunSource.ObjectId) : null;
        set
        {
            var settings = SceneQuery.FindFirstObjectByType<LightingSettings>()
                ?? throw new WorldException(WorldStatus.ComponentMissing, "RenderSettings.Sun sem Configurações de iluminação");
            settings.Component.SetReference("sun_source", value);
        }
    }
}
