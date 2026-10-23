using System.Numerics;
using Astra.Rendering;

namespace Astra.Components;

// G7: nomes da Unity sobre as fachadas geradas. São membros finos: leem e
// escrevem as MESMAS propriedades por id que a fachada gerada, só com o nome e
// os enums da ScriptReference da Unity 6000.0. Onde a Astra já usa o nome da
// Unity (Intensity, Range, ShadowStrength...), não há alias.

/// <summary>UnityEngine.LightType.</summary>
public enum LightType : uint { Directional = 0, Point = 1, Spot = 2 }
/// <summary>UnityEngine.LightShadows.</summary>
public enum LightShadows : uint { None = 0, Hard = 1, Soft = 2 }
/// <summary>UnityEngine.Rendering.ShadowCastingMode.</summary>
public enum ShadowCastingMode : uint { Off = 0, On = 1, TwoSided = 2, ShadowsOnly = 3 }
/// <summary>UnityEngine.Rendering.LightProbeUsage (Off / Blend Probes).</summary>
public enum LightProbeUsage : uint { Off = 0, BlendProbes = 1 }
/// <summary>UnityEngine.Rendering.ReflectionProbeUsage.</summary>
public enum ReflectionProbeUsage : uint { Off = 0, BlendProbes = 1, BlendProbesAndSkybox = 2, Simple = 3 }
/// <summary>UnityEngine.CameraClearFlags (Skybox, SolidColor, Nothing).</summary>
public enum CameraClearFlags : uint { Skybox = 0, SolidColor = 1, Nothing = 2 }
/// <summary>UnityEngine.Rendering.ReflectionProbeMode (sem Custom).</summary>
public enum ReflectionProbeMode : uint { Baked = 0, Realtime = 1 }
/// <summary>UnityEngine.Rendering.ReflectionProbeRefreshMode.</summary>
public enum ReflectionProbeRefreshMode : uint { OnAwake = 0, EveryFrame = 1, ViaScripting = 2 }
/// <summary>UnityEngine.Rendering.ReflectionProbeTimeSlicingMode.</summary>
public enum ReflectionProbeTimeSlicingMode : uint { AllFacesAtOnce = 0, IndividualFaces = 1, NoTimeSlicing = 2 }

public readonly partial struct Light
{
    /// <summary>Light.type.</summary>
    public LightType Type { get => (LightType)Kind; set => Kind = (KindOption)value; }
    /// <summary>Light.shadows.</summary>
    public LightShadows Shadows { get => (LightShadows)ShadowMode; set => ShadowMode = (ShadowModeOption)value; }
    /// <summary>Light.renderingLayerMask (bits das Rendering Layers).</summary>
    public uint RenderingLayerMask { get => (uint)RenderingLayers; set => RenderingLayers = (RenderingLayersOption)value; }
    /// <summary>Light.cullingMask (bits das camadas de objeto).</summary>
    public uint CullingMaskBits { get => (uint)CullingMask; set => CullingMask = (CullingMaskOption)value; }
    /// <summary>Light.color em sRGB, como o seletor.</summary>
    public Astra.Color ColorRgb { get { var c = Color; return new(c.X, c.Y, c.Z); } set => Color = new(value.R, value.G, value.B); }
}

public readonly partial struct MeshRenderer
{
    public ShadowCastingMode ShadowCastingMode { get => (ShadowCastingMode)CastShadows; set => CastShadows = (CastShadowsOption)value; }
    public LightProbeUsage LightProbeUsage { get => (LightProbeUsage)LightProbes; set => LightProbes = (LightProbesOption)value; }
    public ReflectionProbeUsage ReflectionProbeUsage { get => (ReflectionProbeUsage)ReflectionProbes; set => ReflectionProbes = (ReflectionProbesOption)value; }
    public uint RenderingLayerMaskBits { get => (uint)RenderingLayerMask; set => RenderingLayerMask = (RenderingLayerMaskOption)value; }
    /// <summary>Renderer.sharedMaterial: o Material do primeiro slot.</summary>
    public AssetGuid SharedMaterial { get => GetMaterial(0); set => SetMaterial(value, 0); }
    /// <summary>Renderer.material: o override local do primeiro slot (não cria recurso).</summary>
    public MaterialSlot Material => new(Component, 0);
    public MaterialSlot MaterialAt(uint slot) => new(Component, slot);
    /// <summary>Renderer.lightmapScaleOffset do primeiro slot: (escala U, escala V, deslocamento U, deslocamento V).</summary>
    public Vector4 LightmapScaleOffset
    {
        get => new(GetLightmapScaleU(0), GetLightmapScaleV(0), GetLightmapOffsetU(0), GetLightmapOffsetV(0));
        set { SetLightmapScaleU(0, value.X); SetLightmapScaleV(0, value.Y); SetLightmapOffsetU(0, value.Z); SetLightmapOffsetV(0, value.W); }
    }
}

public readonly partial struct Camera
{
    public CameraClearFlags ClearFlags { get => (CameraClearFlags)BackgroundType; set => BackgroundType = (BackgroundTypeOption)value; }
    public float FieldOfView { get => VerticalFov; set => VerticalFov = value; }
    public float NearClipPlane { get => NearPlane; set => NearPlane = value; }
    public float FarClipPlane { get => FarPlane; set => FarPlane = value; }
    public bool Orthographic { get => Projection == ProjectionOption.Ortografica; set => Projection = value ? ProjectionOption.Ortografica : ProjectionOption.Perspectiva; }
    public float OrthographicSize { get => OrthographicHalfHeight; set => OrthographicHalfHeight = value; }
    /// <summary>Camera.depth: a câmera de maior prioridade desenha por último.</summary>
    public float Depth { get => Priority; set => Priority = value; }
    /// <summary>Camera.rect normalizado: (x, y, largura, altura).</summary>
    public Vector4 Rect
    {
        get => new(ViewportX, ViewportY, ViewportWidth, ViewportHeight);
        set { ViewportX = value.X; ViewportY = value.Y; ViewportWidth = value.Z; ViewportHeight = value.W; }
    }
    public bool UsePhysicalProperties { get => PhysicalCamera; set => PhysicalCamera = value; }
    public Vector2 SensorSize { get => new(SensorWidth, SensorHeight); set { SensorWidth = value.X; SensorHeight = value.Y; } }
    public bool AllowHDR { get => Hdr; set => Hdr = value; }
    /// <summary>UniversalAdditionalCameraData.renderPostProcessing.</summary>
    public bool RenderPostProcessing { get => PostProcessing; set => PostProcessing = value; }
    /// <summary>UniversalAdditionalCameraData.volumeLayerMask.</summary>
    public uint VolumeLayerMask { get => (uint)EnvironmentMask; set => EnvironmentMask = (EnvironmentMaskOption)value; }
    public uint CullingMaskBits { get => (uint)CullingMask; set => CullingMask = (CullingMaskOption)value; }
}

public readonly partial struct ReflectionProbe
{
    public ReflectionProbeMode Mode { get => (ReflectionProbeMode)Type; set => Type = (TypeOption)value; }
    public ReflectionProbeRefreshMode RefreshModeUnity { get => (ReflectionProbeRefreshMode)RefreshMode; set => RefreshMode = (RefreshModeOption)value; }
    public ReflectionProbeTimeSlicingMode TimeSlicingMode { get => (ReflectionProbeTimeSlicingMode)TimeSlicing; set => TimeSlicing = (TimeSlicingOption)value; }
    public float NearClipPlane { get => NearClip; set => NearClip = value; }
    public float FarClipPlane { get => FarClip; set => FarClip = value; }
    public Vector3 Size { get => BoxSize; set => BoxSize = value; }
    public Vector3 Center { get => BoxOffset; set => BoxOffset = value; }
    /// <summary>ReflectionProbe.bakedTexture: o recurso assado (.aeprobe).</summary>
    public AssetGuid BakedTexture { get => GetBakedTexture(0); set => SetBakedTexture(value, 0); }
}

public readonly partial struct LightProbeGroup
{
    /// <summary>LightProbeGroup.probePositions[index], no espaço do objeto.</summary>
    public Vector3 GetProbePosition(uint index) => new(GetProbePositionX(index), GetProbePositionY(index), GetProbePositionZ(index));
    public void SetProbePosition(uint index, Vector3 position)
    {
        SetProbePositionX(index, position.X); SetProbePositionY(index, position.Y); SetProbePositionZ(index, position.Z);
    }
}

public readonly partial struct TimeOfDay
{
    /// <summary>A primeira Hora do dia ativa do mundo (o "TimeOfDay.Current" do plano G4).</summary>
    public static TimeOfDay? Current => SceneQuery.FindFirstObjectByType<TimeOfDay>();
    /// <summary>Quantas horas de jogo passam por hora real: 60 faz 1 min real = 1 h.
    /// 0 congela; o mínimo além de 0 é 1 (tempo real), porque a Duração do dia vai até 1440 min.</summary>
    public float Speed
    {
        get => DayLengthMinutes <= 0 ? 0 : 1440f / DayLengthMinutes;
        set
        {
            if (!float.IsFinite(value) || value < 0 || (value > 0 && value < 1))
                throw new ArgumentOutOfRangeException(nameof(value), value, "Velocidade 0 (parada) ou de 1 a 1440.");
            DayLengthMinutes = value == 0 ? 0 : 1440f / value;
        }
    }
    /// <summary>Hora inteira nova (0..23), como o evento HourChanged do plano.</summary>
    public ComponentSubscription HourChanged(Behavior owner, Action<int> handler) =>
        OnHourChanged(owner, e => handler((int)e[0].AsInteger()));
    public ComponentSubscription Sunrise(Behavior owner, Action handler) => OnSunrise(owner, _ => handler());
    public ComponentSubscription Sunset(Behavior owner, Action handler) => OnSunset(owner, _ => handler());
    public ComponentSubscription DayChanged(Behavior owner, Action<int> handler) =>
        OnDayChanged(owner, e => handler((int)e[0].AsInteger()));
}

public readonly partial struct LightingSettings
{
    /// <summary>Lighting › Sun Source (RenderSettings.sun).</summary>
    public ObjectReference Sun { get => SunSource; set => SunSource = value; }
}
