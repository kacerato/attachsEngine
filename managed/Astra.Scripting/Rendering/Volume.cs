using System.Numerics;
using Astra.Components;

namespace Astra.Rendering;

// G7: o Volume da Unity (UnityEngine.Rendering.Volume, VolumeProfile,
// VolumeComponent, VolumeParameter<T>; SRP Core 17 / Unity 6000.0) sobre o
// componente Ambiente (astra.render.environment). Os valores e as caixas de
// override são os do componente; o índice de cada parâmetro vem da tabela
// gravada gerada (VolumeParameterTable). Nada aqui guarda estado próprio:
// ler e escrever passam pelo mundo de Play, como a fachada gerada.
// Referência: https://docs.unity3d.com/Packages/com.unity.render-pipelines.core@17.0/api/UnityEngine.Rendering.Volume.html

/// <summary>Volume da Unity: forma global ou local, peso, prioridade e perfil.</summary>
public sealed class Volume
{
    public Volume(EnvironmentVolume facade) => Facade = facade;
    /// <summary>A fachada gerada, com todos os campos pelo nome da Astra.</summary>
    public EnvironmentVolume Facade { get; }
    public Component Component => Facade.Component;
    public GameObject GameObject => Facade.Object;

    public static Volume? Get(GameObject owner) => owner.GetComponent<EnvironmentVolume>() is { } facade ? new(facade) : null;
    /// <summary>Todos os Volumes ativos do mundo, em pré-ordem.</summary>
    public static Volume[] FindAll() => SceneQuery.FindObjectsByType<EnvironmentVolume>().Select(f => new Volume(f)).ToArray();

    public bool Enabled { get => Facade.Enabled; set { var facade = Facade; facade.Enabled = value; } }
    /// <summary>Mode = Global (true) ou Local; local vira Caixa, preservando Esfera.</summary>
    public bool IsGlobal
    {
        get => Facade.VolumeShape == EnvironmentVolume.VolumeShapeOption.Global;
        set
        {
            var facade = Facade;
            if (value) facade.VolumeShape = EnvironmentVolume.VolumeShapeOption.Global;
            else if (facade.VolumeShape == EnvironmentVolume.VolumeShapeOption.Global) facade.VolumeShape = EnvironmentVolume.VolumeShapeOption.Caixa;
        }
    }
    public float Weight { get => Facade.Weight; set { var facade = Facade; facade.Weight = value; } }
    public float Priority { get => Facade.Priority; set { var facade = Facade; facade.Priority = value; } }
    public float BlendDistance { get => Facade.BlendDistance; set { var facade = Facade; facade.BlendDistance = value; } }

    /// <summary>Volume.sharedProfile: o Volume Profile do projeto. Trocar o recurso
    /// troca os valores no mesmo quadro; ler valores pelo perfil compartilhado
    /// não o copia.</summary>
    public AssetGuid SharedProfile
    {
        get => Facade.GetProfile();
        set => Facade.SetProfile(value);
    }
    /// <summary>Volume.profile: na primeira leitura copia o perfil compartilhado
    /// para este Volume; as mudanças seguintes valem só aqui e só no Play.</summary>
    public VolumeProfile Profile { get { Facade.InstantiateProfile(); return new(this); } }
    public bool HasInstantiatedProfile() => Facade.HasInstantiatedProfile();

    internal bool ParameterOverridden(int index) => Facade.ParameterOverridden(index);
    internal void SetParameterOverridden(int index, bool value) => Facade.SetParameterOverridden(index, value);
    internal bool ComponentActive(int group) => Facade.ComponentActive(group);
    internal void SetComponentActive(int group, bool value) => Facade.SetComponentActive(group, value);
    /// <summary>Antes de escrever um valor: com perfil compartilhado, a escrita
    /// iria para a cópia gravada e não teria efeito; aqui ela vira a cópia própria.</summary>
    internal void EnsureInstance() { if (!Facade.HasInstantiatedProfile()) Facade.InstantiateProfile(); }
}

/// <summary>VolumeProfile da Unity sobre um Volume: overrides presentes, Add, Remove e TryGet.</summary>
public sealed class VolumeProfile
{
    internal VolumeProfile(Volume volume) => Volume = volume;
    public Volume Volume { get; }

    private static T Create<T>(Volume volume) where T : VolumeComponent, new()
    {
        var component = new T();
        component.Attach(volume);
        return component;
    }
    public bool Has<T>() where T : VolumeComponent, new() =>
        Volume.Component.GetBool(VolumeParameterTable.GroupAddedProperties[new T().Group]);
    public bool TryGet<T>([System.Diagnostics.CodeAnalysis.MaybeNullWhen(false)] out T component) where T : VolumeComponent, new()
    {
        component = Create<T>(Volume);
        if (Volume.Component.GetBool(VolumeParameterTable.GroupAddedProperties[component.Group])) return true;
        component = null!;
        return false;
    }
    /// <summary>Add Override: com <paramref name="overrides"/> todas as caixas ficam marcadas (ALL).</summary>
    public T Add<T>(bool overrides = false) where T : VolumeComponent, new()
    {
        var component = Create<T>(Volume);
        Volume.EnsureInstance();
        Volume.Component.SetBool(VolumeParameterTable.GroupAddedProperties[component.Group], true);
        component.SetAllOverridesTo(overrides);
        return component;
    }
    public void Remove<T>() where T : VolumeComponent, new()
    {
        Volume.EnsureInstance();
        Volume.Component.SetBool(VolumeParameterTable.GroupAddedProperties[new T().Group], false);
    }
}

/// <summary>Um override do perfil (Bloom, Vignette...). <see cref="Active"/> é o
/// interruptor do cabeçalho; cada parâmetro tem a própria caixa.</summary>
public abstract class VolumeComponent
{
    private Volume? _volume;
    internal Volume Volume => _volume ?? throw new InvalidOperationException("Override sem Volume: use VolumeProfile.TryGet ou Add.");
    /// <summary>Índice do override na tabela gravada (renderer/volume_overrides.h).</summary>
    internal abstract int Group { get; }
    internal void Attach(Volume volume) => _volume = volume;

    public bool Active
    {
        get => Volume.ComponentActive(Group);
        set => Volume.SetComponentActive(Group, value);
    }
    /// <summary>ALL / NONE do cabeçalho.</summary>
    public void SetAllOverridesTo(bool state)
    {
        for (var index = 0; index < VolumeParameterTable.Groups.Length; ++index)
            if (VolumeParameterTable.Groups[index] == Group) Volume.SetParameterOverridden(index, state);
    }

    protected FloatParameter FloatParam(string id) => new(Volume, id);
    protected BoolParameter BoolParam(string id) => new(Volume, id);
    protected ColorParameter ColorParam(string id) => new(Volume, id);
    protected Vector2Parameter Vector2Param(string id) => new(Volume, id);
    protected TextureParameter TextureParam(string id) => new(Volume, id);
    protected EnumParameter<TEnum> EnumParam<TEnum>(string id) where TEnum : struct, System.Enum => new(Volume, id);
}

/// <summary>VolumeParameter da Unity: <see cref="Value"/> e a caixa <see cref="OverrideState"/>.</summary>
public abstract class VolumeParameter
{
    private protected VolumeParameter(Volume volume, string propertyId)
    {
        Volume = volume; PropertyId = propertyId;
        Index = VolumeParameterTable.IndexOf(propertyId);
        if (Index < 0) throw new ArgumentException("Não é parâmetro de Volume: " + propertyId, nameof(propertyId));
    }
    private protected Volume Volume { get; }
    private protected Component Component => Volume.Component;
    public string PropertyId { get; }
    public int Index { get; }
    public bool OverrideState
    {
        get => Volume.ParameterOverridden(Index);
        set => Volume.SetParameterOverridden(Index, value);
    }
}
public abstract class VolumeParameter<T> : VolumeParameter
{
    private protected VolumeParameter(Volume volume, string propertyId) : base(volume, propertyId) { }
    private protected abstract T Read();
    private protected abstract void Write(T value);
    public T Value
    {
        get => Read();
        set { Volume.EnsureInstance(); Write(value); }
    }
    /// <summary>Marca a caixa e escreve, como VolumeParameter.Override.</summary>
    public void Override(T value) { Value = value; OverrideState = true; }
    public static implicit operator T(VolumeParameter<T> parameter) => parameter.Value;
    public override string ToString() => $"{PropertyId} = {Value} ({(OverrideState ? "marcado" : "herda")})";
}
public sealed class FloatParameter : VolumeParameter<float>
{
    internal FloatParameter(Volume volume, string id) : base(volume, id) { }
    private protected override float Read() => Component.GetFloat(PropertyId);
    private protected override void Write(float value) => Component.SetFloat(PropertyId, value);
}
public sealed class BoolParameter : VolumeParameter<bool>
{
    internal BoolParameter(Volume volume, string id) : base(volume, id) { }
    private protected override bool Read() => Component.GetBool(PropertyId);
    private protected override void Write(bool value) => Component.SetBool(PropertyId, value);
}
/// <summary>Cor linear (sem alfa: o canal A é ignorado na escrita e 1 na leitura).</summary>
public sealed class ColorParameter : VolumeParameter<Color>
{
    internal ColorParameter(Volume volume, string id) : base(volume, id) { }
    private protected override Color Read() =>
        new(Component.GetFloat(PropertyId + ".r"), Component.GetFloat(PropertyId + ".g"), Component.GetFloat(PropertyId + ".b"));
    private protected override void Write(Color value) => Component.SetVector3(PropertyId, new Vector3(value.R, value.G, value.B));
}
public sealed class Vector2Parameter : VolumeParameter<Vector2>
{
    internal Vector2Parameter(Volume volume, string id) : base(volume, id) { }
    private protected override Vector2 Read() => new(Component.GetFloat(PropertyId + ".x"), Component.GetFloat(PropertyId + ".y"));
    private protected override void Write(Vector2 value) => Component.SetVector2(PropertyId, value);
}
public sealed class TextureParameter : VolumeParameter<AssetGuid>
{
    internal TextureParameter(Volume volume, string id) : base(volume, id) { }
    private protected override AssetGuid Read() => Component.GetResource(PropertyId);
    private protected override void Write(AssetGuid value) => Component.SetResource(PropertyId, value);
}
public sealed class EnumParameter<TEnum> : VolumeParameter<TEnum> where TEnum : struct, System.Enum
{
    internal EnumParameter(Volume volume, string id) : base(volume, id) { }
    private protected override TEnum Read() => (TEnum)System.Enum.ToObject(typeof(TEnum), Component.GetEnum(PropertyId));
    private protected override void Write(TEnum value) => Component.SetEnum(PropertyId, Convert.ToUInt32(value));
}

// --- Enums com os nomes da Unity; os valores são os gravados pela Astra. ---
public enum TonemappingMode : uint { None = 3, Neutral = 4, ACES = 1, AgX = 2 }
public enum DepthOfFieldMode : uint { Off = 0, Gaussian = 1, Bokeh = 2 }
public enum EffectQuality : uint { Low = 0, Medium = 1, High = 2 }
public enum ExposureMode : uint { Fixed = 0, Automatic = 1, AutomaticHistogram = 2 }
public enum MeteringMode : uint { Average = 0, Spot = 1, CenterWeighted = 2 }
public enum AdaptationMode : uint { Fixed = 0, Progressive = 1 }
public enum SkyType : uint { Panoramic = 0, Procedural = 1, PhysicallyBased = 2 }
public enum SunDiskMode : uint { None = 0, Simple = 1, HighQuality = 2 }
public enum FogColorMode : uint { ConstantColor = 0, SkyColor = 1 }
/// <summary>UnityEngine.FogMode (valores gravados pela Astra, a partir de 0).</summary>
public enum FogMode : uint { Linear = 0, Exponential = 1, ExponentialSquared = 2 }
/// <summary>UnityEngine.Rendering.AmbientMode: Skybox, Trilight (Gradiente) e Flat (Cor).</summary>
public enum AmbientMode : uint { Skybox = 0, Trilight = 1, Flat = 2 }

// --- Overrides de pós (URP) ---
// Referências: docs.unity3d.com/6000.0/Documentation/Manual/urp/<efeito>.html
public sealed class Tonemapping : VolumeComponent
{
    internal override int Group => 7;
    public EnumParameter<TonemappingMode> Mode => EnumParam<TonemappingMode>("tone_mapper");
    /// <summary>Liga o pós deste Volume (a chave "Pós" do Inspector).</summary>
    public BoolParameter PostProcessing => BoolParam("post");
}
public sealed class Bloom : VolumeComponent
{
    internal override int Group => 8;
    public FloatParameter Threshold => FloatParam("bloom_threshold");
    public FloatParameter Intensity => FloatParam("bloom_intensity");
    public FloatParameter Scatter => FloatParam("bloom_scatter");
    public FloatParameter Clamp => FloatParam("bloom_clamp");
    public ColorParameter Tint => ColorParam("bloom_tint");
    public BoolParameter HighQualityFiltering => BoolParam("bloom_high_quality");
    public TextureParameter DirtTexture => TextureParam("bloom_dirt");
    public FloatParameter DirtIntensity => FloatParam("bloom_dirt_intensity");
}
public sealed class Vignette : VolumeComponent
{
    internal override int Group => 9;
    public ColorParameter Color => ColorParam("vignette_color");
    public Vector2Parameter Center => Vector2Param("vignette_center");
    public FloatParameter Intensity => FloatParam("vignette_intensity");
    public FloatParameter Smoothness => FloatParam("vignette_smoothness");
    public BoolParameter Rounded => BoolParam("vignette_rounded");
}
public sealed class ColorAdjustments : VolumeComponent
{
    internal override int Group => 10;
    public FloatParameter PostExposure => FloatParam("post_exposure");
    public FloatParameter Contrast => FloatParam("contrast");
    public ColorParameter ColorFilter => ColorParam("color_filter");
    public FloatParameter HueShift => FloatParam("hue_shift");
    public FloatParameter Saturation => FloatParam("saturation");
}
/// <summary>White Balance da URP. Na Astra, White Balance, Lift Gamma Gain e
/// Shadows Midtones Highlights dividem o override "Cor": adicionar um adiciona os três.</summary>
public sealed class WhiteBalance : VolumeComponent
{
    internal override int Group => 11;
    public FloatParameter Temperature => FloatParam("grading_temperature");
    public FloatParameter Tint => FloatParam("grading_tint");
}
public sealed class LiftGammaGain : VolumeComponent
{
    internal override int Group => 11;
    public ColorParameter Lift => ColorParam("grading_lift_color");
    public FloatParameter LiftOffset => FloatParam("grading_lift");
    public ColorParameter Gamma => ColorParam("grading_gamma_color");
    public FloatParameter GammaOffset => FloatParam("grading_gamma");
    public ColorParameter Gain => ColorParam("grading_gain_color");
    public FloatParameter GainOffset => FloatParam("grading_gain");
}
public sealed class ShadowsMidtonesHighlights : VolumeComponent
{
    internal override int Group => 11;
    public ColorParameter Shadows => ColorParam("grading_shadows_color");
    public ColorParameter Midtones => ColorParam("grading_midtones_color");
    public ColorParameter Highlights => ColorParam("grading_highlights_color");
    public FloatParameter ShadowsStart => FloatParam("grading_shadows_start");
    public FloatParameter ShadowsEnd => FloatParam("grading_shadows_end");
    public FloatParameter HighlightsStart => FloatParam("grading_highlights_start");
    public FloatParameter HighlightsEnd => FloatParam("grading_highlights_end");
}
/// <summary>Color Curves: o override liga, desliga e marca as caixas; as chaves
/// das curvas são editadas no Inspector (sem API de chave por script ainda).</summary>
public sealed class ColorCurves : VolumeComponent
{
    internal override int Group => 12;
}
public sealed class FilmGrain : VolumeComponent
{
    internal override int Group => 13;
    public FloatParameter Intensity => FloatParam("film_grain_intensity");
}
/// <summary>Ambient Occlusion (HDRP; na URP é um Renderer Feature).</summary>
public sealed class AmbientOcclusion : VolumeComponent
{
    internal override int Group => 14;
    public FloatParameter Intensity => FloatParam("ambient_occlusion_intensity");
    public FloatParameter Radius => FloatParam("ambient_occlusion_radius");
    public FloatParameter Power => FloatParam("ambient_occlusion_power");
    public FloatParameter Bias => FloatParam("ambient_occlusion_bias");
}
public sealed class DepthOfField : VolumeComponent
{
    internal override int Group => 15;
    public EnumParameter<DepthOfFieldMode> Mode => EnumParam<DepthOfFieldMode>("dof_mode");
    public EnumParameter<EffectQuality> Quality => EnumParam<EffectQuality>("dof_quality");
    public FloatParameter GaussianStart => FloatParam("dof_gaussian_start");
    public FloatParameter GaussianEnd => FloatParam("dof_gaussian_end");
    public FloatParameter GaussianMaxRadius => FloatParam("dof_gaussian_max_radius");
    public BoolParameter HighQualitySampling => BoolParam("dof_gaussian_high_quality");
    public FloatParameter FocusDistance => FloatParam("dof_focus_distance");
    public FloatParameter FocalLength => FloatParam("dof_focal_length");
    public FloatParameter Aperture => FloatParam("dof_aperture");
    public FloatParameter BladeCount => FloatParam("dof_blade_count");
    public FloatParameter BladeCurvature => FloatParam("dof_blade_curvature");
    public FloatParameter BladeRotation => FloatParam("dof_blade_rotation");
}
public sealed class MotionBlur : VolumeComponent
{
    internal override int Group => 16;
    public EnumParameter<EffectQuality> Quality => EnumParam<EffectQuality>("motion_blur_quality");
    public FloatParameter Intensity => FloatParam("motion_blur_intensity");
    public FloatParameter Clamp => FloatParam("motion_blur_clamp");
}
public sealed class ChromaticAberration : VolumeComponent
{
    internal override int Group => 17;
    public FloatParameter Intensity => FloatParam("chromatic_aberration");
}
public sealed class LensDistortion : VolumeComponent
{
    internal override int Group => 18;
    public FloatParameter Intensity => FloatParam("lens_distortion");
    public FloatParameter XMultiplier => FloatParam("lens_distortion_x_multiplier");
    public FloatParameter YMultiplier => FloatParam("lens_distortion_y_multiplier");
    public Vector2Parameter Center => Vector2Param("lens_distortion_center");
    public FloatParameter Scale => FloatParam("lens_distortion_scale");
}

// --- Overrides de céu e luz (HDRP) ---
/// <summary>Exposure do HDRP (a URP só tem Post Exposure em Color Adjustments).</summary>
public sealed class Exposure : VolumeComponent
{
    internal override int Group => 6;
    public EnumParameter<ExposureMode> Mode => EnumParam<ExposureMode>("exposure_mode");
    public EnumParameter<Astra.Rendering.MeteringMode> MeteringMode => EnumParam<Astra.Rendering.MeteringMode>("exposure_metering");
    public EnumParameter<Astra.Rendering.AdaptationMode> AdaptationMode => EnumParam<Astra.Rendering.AdaptationMode>("exposure_adaptation");
    public FloatParameter FixedExposure => FloatParam("exposure_fixed");
    public FloatParameter Compensation => FloatParam("exposure_compensation");
    public FloatParameter LimitMin => FloatParam("exposure_limit_min");
    public FloatParameter LimitMax => FloatParam("exposure_limit_max");
    public FloatParameter HistogramLow => FloatParam("exposure_histogram_low");
    public FloatParameter HistogramHigh => FloatParam("exposure_histogram_high");
    public FloatParameter TargetMidGray => FloatParam("exposure_target_mid_grey");
    public FloatParameter AdaptationSpeedDarkToLight => FloatParam("exposure_speed_dark_to_light");
    public FloatParameter AdaptationSpeedLightToDark => FloatParam("exposure_speed_light_to_dark");
}
/// <summary>Visual Environment + Skybox: modelo do céu, disco do sol e Material do céu.</summary>
public sealed class Sky : VolumeComponent
{
    internal override int Group => 0;
    public EnumParameter<SkyType> Type => EnumParam<SkyType>("sky");
    public EnumParameter<SunDiskMode> SunDisk => EnumParam<SunDiskMode>("sun_disk");
    public FloatParameter SunSize => FloatParam("sun_size");
    public FloatParameter SunSizeConvergence => FloatParam("sun_size_convergence");
    public FloatParameter AtmosphereThickness => FloatParam("atmosphere");
    public ColorParameter SkyTint => ColorParam("sky_zenith");
    public ColorParameter HorizonTint => ColorParam("sky_horizon");
    public ColorParameter GroundColor => ColorParam("ground");
    public FloatParameter Exposure => FloatParam("sky_exposure");
    public TextureParameter Skybox => TextureParam("skybox");
    public TextureParameter HdriSky => TextureParam("environment_map");
    public FloatParameter Rotation => FloatParam("hdri_rotation_degrees");
}
/// <summary>Sol e lua do céu noturno (fase, tamanho, brilho e estrelas).</summary>
public sealed class Celestial : VolumeComponent
{
    internal override int Group => 1;
    public FloatParameter SunGlare => FloatParam("sun_glare");
    public FloatParameter MoonPhase => FloatParam("moon_phase");
    public FloatParameter MoonSize => FloatParam("moon_size");
    public FloatParameter MoonIntensity => FloatParam("moon_intensity");
    public FloatParameter StarIntensity => FloatParam("star_intensity");
}
/// <summary>Physically Based Sky do HDRP.</summary>
public sealed class PhysicallyBasedSky : VolumeComponent
{
    internal override int Group => 2;
    public FloatParameter Intensity => FloatParam("physical_sky_intensity");
    public FloatParameter AirDensity => FloatParam("air_density");
    public FloatParameter AerosolDensity => FloatParam("aerosol_density");
    public FloatParameter AerosolAnisotropy => FloatParam("aerosol_anisotropy");
    public FloatParameter PlanetaryRadius => FloatParam("planet_radius_km");
    public FloatParameter GroundAlbedo => FloatParam("ground_albedo");
}
/// <summary>Indirect Lighting Controller + Environment Lighting.</summary>
public sealed class IndirectLightingController : VolumeComponent
{
    internal override int Group => 3;
    public FloatParameter IndirectDiffuseIntensity => FloatParam("indirect_diffuse");
    public FloatParameter ReflectionLightingMultiplier => FloatParam("indirect_specular");
}
/// <summary>Fog do HDRP com os modos da neblina da janela Lighting.</summary>
public sealed class Fog : VolumeComponent
{
    internal override int Group => 4;
    public BoolParameter Enabled => BoolParam("fog");
    public EnumParameter<FogMode> Mode => EnumParam<FogMode>("fog_mode");
    public EnumParameter<Astra.Rendering.FogColorMode> ColorMode => EnumParam<Astra.Rendering.FogColorMode>("fog_color_mode");
    public ColorParameter Color => ColorParam("fog_color");
    public FloatParameter Density => FloatParam("fog_density");
    public FloatParameter StartDistance => FloatParam("fog_start");
    public FloatParameter EndDistance => FloatParam("fog_end");
    public FloatParameter BaseHeight => FloatParam("fog_base_height");
    public FloatParameter HeightFalloff => FloatParam("fog_height_falloff");
}
/// <summary>Cloud Layer do HDRP.</summary>
/// <remarks>https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/cloud-layer-volume-override-reference.html</remarks>
public sealed class CloudLayer : VolumeComponent
{
    internal override int Group => 5;
    public BoolParameter Enabled => BoolParam("clouds");
    public BoolParameter CastShadows => BoolParam("cloud_shadows");
    public FloatParameter Opacity => FloatParam("cloud_opacity");
    public FloatParameter Coverage => FloatParam("cloud_coverage");
    public FloatParameter Cirrus => FloatParam("cloud_cirrus");
    public FloatParameter Altitude => FloatParam("cloud_altitude");
    public FloatParameter Size => FloatParam("cloud_size");
    public ColorParameter Tint => ColorParam("cloud_tint");
    public FloatParameter Exposure => FloatParam("cloud_exposure");
    public FloatParameter WindOrientation => FloatParam("cloud_wind_orientation");
    public FloatParameter WindSpeed => FloatParam("cloud_wind_speed");
    public FloatParameter Density => FloatParam("cloud_density");
    public FloatParameter AmbientDimmer => FloatParam("cloud_ambient_dimmer");
    public FloatParameter ShadowMultiplier => FloatParam("cloud_shadow_multiplier");
}
