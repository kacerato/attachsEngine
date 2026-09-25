using System.Runtime.InteropServices;
using System.Globalization;

namespace Astra;

public enum GraphicsQuality : uint { Auto, Low, Medium, High, Ultra, Custom }
public enum GraphicsFeature : uint { Inherit, Disabled, Enabled }
public enum GraphicsAntiAliasing : uint { Inherit, Off, Fxaa, Temporal }
/// <summary>Filtro de reconstrução. <c>ArmAsr</c> e <c>Fsr2</c> são ampliadores
/// temporais: substituem FXAA/TAA enquanto ativos e exigem capacidade do aparelho.</summary>
public enum GraphicsUpscaling : uint { Inherit, Bilinear, CatmullRom, Fsr1, ArmAsr, Fsr2 }
/// <summary>Preset de shader do ampliador temporal (Arm ASR), independente da escala.</summary>
public enum GraphicsTemporalQuality : uint { Inherit, Quality, Balanced, Performance, UltraPerformance }
/// <summary>Motivo pelo qual um ampliador temporal não roda neste aparelho.</summary>
public enum GraphicsTemporalAvailability : uint
{
    Available, NotProbed, NotBuilt, MissingFloat16, MissingInt16, MissingQuadSubgroup,
    MissingStorageImageFormats, MissingStorageWriteWithoutFormat, MissingHdrSceneColor, ContextCreationFailed,
    MissingSampledDepth32
}
public enum GraphicsShadows : uint { Inherit, Off, Hard, Soft, UltraSoft }
public enum GraphicsAmbient : uint { Inherit, Constant, Hemispheric, HemisphericSpecular }
public enum GraphicsPost : uint { Inherit, None, Tonemap, Bloom }
public enum GraphicsTextures : uint { Inherit, Half, Full }
public enum GraphicsWaterMesh : uint { Inherit, Ultra, High, Medium, Low, VeryLow }

[StructLayout(LayoutKind.Sequential)]
public struct GraphicsSettings
{
    internal uint Size;
    public uint SchemaVersion;
    public GraphicsQuality Preset;
    public GraphicsShadows Shadows; public GraphicsAmbient Ambient; public GraphicsPost Post;
    public GraphicsTextures Textures; public GraphicsWaterMesh WaterMesh;
    public float ResolutionScale; public uint MaximumRenderHz;
    public uint ShadowCascadeCount, ShadowCascadeResolution, ShadowFilterTaps, ShadowFarFilterTaps;
    public float ShadowMaximumDistance, ShadowDepthBiasConstant, ShadowDepthBiasSlope, ShadowNormalOffsetTexels;
    public GraphicsFeature StaticShadowCache; public float ShadowCacheGuardBandRatio, ShadowCascadeBlendRatio, ShadowDistanceFadeRatio;
    public float LodPixelErrorBudget, CoverageLodPixelErrorBudget, LodHysteresisBandRatio;
    public GraphicsFeature LodSelection, MaterialShaderVariants, EnvironmentSplitSumBrdf;
    public float NormalMapMaximumDistance, SpecularProbeMaximumDistance, MetallicRoughnessMaximumDistance;
    public float EmissiveMaximumDistance, MaterialDetailFadeBandRatio; public GraphicsFeature ThermalDistanceScaling;
    public GraphicsAntiAliasing AntiAliasing; public GraphicsUpscaling UpscalingFilter;
    public GraphicsFeature LegacyPostFxaa, PostVignette;
    public float BloomThreshold, BloomIntensity, PostContrast, PostSaturation, PostSharpen, TemporalHistoryWeight;
    public GraphicsFeature DynamicResolution; public float DynamicResolutionMinimumScale, DynamicResolutionDecreaseStep;
    public float DynamicResolutionIncreaseStep, DynamicResolutionRecoveryHeadroomRatio;
    public uint DynamicResolutionOverloadFrames, DynamicResolutionRecoveryFrames;
    public GraphicsTemporalQuality TemporalUpscalerQuality;
    /// <summary>Mipmap Streaming (QualitySettings.streamingMipmapsActive da Unity).
    /// Zero nos números herda o nível; <c>Inherit</c> fica desligado.</summary>
    public GraphicsFeature TextureStreaming;
    public uint TextureStreamingBudgetMegabytes, TextureStreamingMaxLevelReduction, TextureStreamingUploadKilobytesPerFrame;
}

[StructLayout(LayoutKind.Sequential)]
public struct ResolvedGraphicsSettings
{
    internal uint Size;
    public uint RenderHz, SimulationHz; public float FrameIntervalMs, CpuLaneBudgetMs, GpuLaneBudgetMs, CompositorReserveMs;
    public uint HzbMinimumCandidateDraws, HzbHysteresisFrames; public float HzbNormalizedDepthBias, LodPixelErrorBudget;
    public float CoverageLodPixelErrorBudget, LodHysteresisBandRatio;
    public uint ShadowsEnabled, ShadowCascadeCount, ShadowCascadeResolution, ShadowFilterTaps, ShadowFarFilterTaps;
    public float ShadowMaximumDistance, ShadowDepthBiasConstant, ShadowDepthBiasSlope, ShadowNormalOffsetTexels;
    public uint ShadowStabilizeTexelSnap, StaticShadowCache; public float ShadowCacheGuardBandRatio, ShadowCascadeBlendRatio, ShadowDistanceFadeRatio;
    public uint AmbientHemispheric, AmbientSpecularProbe, AmbientSplitSumBrdf;
    public uint PostDedicatedPass, PostBloom; public GraphicsAntiAliasing AntiAliasing; public GraphicsUpscaling UpscalingFilter; public uint PostVignette;
    public float BloomThreshold, BloomIntensity, PostContrast, PostSaturation, PostSharpen, VignetteIntensity, TemporalHistoryWeight;
    public uint LodSelection, MaterialShaderVariants; public GraphicsWaterMesh WaterMesh; public uint TextureResidencyMipBias;
    public float SamplerAnisotropy, NormalMapMaximumDistance, SpecularProbeMaximumDistance;
    public float MetallicRoughnessMaximumDistance, EmissiveMaximumDistance, MaterialDetailFadeBandRatio;
    public uint DynamicResolutionEnabled; public float DynamicResolutionMinimumScale, DynamicResolutionMaximumScale;
    public float DynamicResolutionDecreaseStep, DynamicResolutionIncreaseStep, DynamicResolutionRecoveryHeadroomRatio;
    public uint DynamicResolutionOverloadFrames, DynamicResolutionRecoveryFrames;
    public float ResolutionScale; public uint EffectiveProfile, ClampCount;
    public GraphicsTemporalQuality TemporalUpscalerQuality;
    public uint TextureStreaming, TextureStreamingBudgetMegabytes, TextureStreamingMaxLevelReduction, TextureStreamingUploadKilobytesPerFrame;
}

/// <summary>Memória de textura do último quadro, nos termos da Unity:
/// <c>CurrentBytes</c> (currentTextureMemory), <c>DesiredBytes</c> (desiredTextureMemory),
/// <c>TargetBytes</c> (targetTextureMemory, depois do orçamento), <c>TotalBytes</c>
/// (totalTextureMemory) e <c>NonStreamingBytes</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct TextureStreamingStats
{
    public ulong BudgetBytes, TotalBytes, DesiredBytes, TargetBytes, CurrentBytes, NonStreamingBytes;
    public ulong UploadedBytesLastFrame;
    public uint Active, OverBudget, StreamingTextures, PendingLoads, BudgetReducedTextures;
    public uint UploadsLastFrame, FailedUploads;
    internal uint Reserved;
}

[StructLayout(LayoutKind.Sequential)]
public struct GraphicsCapabilities
{
    internal uint Size;
    public uint CapabilityProfile, RecommendedProfile, RecommendationSource;
    public uint MaximumImage2DSize, MaximumImageArrayLayers, SupportsDepthSampling;
    public float MaximumSamplerAnisotropy, DisplayHz;
    public GraphicsTemporalAvailability ArmAsr, Fsr2;
}

[StructLayout(LayoutKind.Sequential)]
public struct NativeGraphicsState
{
    public uint Size, World, Pending, LastRequestSucceeded, EffectiveAvailable;
    public ulong PendingRequestId;
    public GraphicsSettings Requested;
    public ResolvedGraphicsSettings Effective;
    public GraphicsCapabilities Capabilities;
    public GraphicsUpscaling ExecutedUpscaler;
    public GraphicsTemporalAvailability ExecutedStatus;
    public TextureStreamingStats TextureStreaming;
}

/// <summary>Estado gráfico da sessão. <c>ExecutedUpscaler</c> é o algoritmo que o
/// renderer realmente executou no último quadro; <c>ExecutedStatus</c> explica
/// por que um ampliador temporal pedido não rodou.</summary>
public readonly record struct GraphicsSnapshot(GraphicsSettings Requested,
    ResolvedGraphicsSettings Effective, GraphicsCapabilities Capabilities,
    bool Pending, ulong PendingRequestId, bool LastRequestSucceeded, bool EffectiveAvailable,
    string Diagnostics, GraphicsUpscaling ExecutedUpscaler = GraphicsUpscaling.Bilinear,
    GraphicsTemporalAvailability ExecutedStatus = GraphicsTemporalAvailability.Available,
    TextureStreamingStats TextureStreaming = default);

/// <summary>Política gráfica da sessão Play. Alterações não persistem no projeto.</summary>
public static class Graphics
{
    private static ISceneAccess? _scene;
    private static uint _world;
    internal static void Bind(ISceneAccess scene) { _scene = scene; _world = scene.WorldId; }
    internal static void Unbind() { _scene = null; _world = 0; }
    private static ISceneAccess Scene => _scene is not null && _scene.WorldId == _world
        ? _scene : throw new WorldException(WorldStatus.NotRunning, "acessar gráficos");
    public static GraphicsSnapshot State => Scene.GetGraphicsState(_world);
    public static ulong ApplyRuntime(GraphicsSettings settings)
    {
        if (!Scene.SetGraphicsSettings(_world, settings, out var request))
            throw new WorldException(Scene.LastStatus, "alterar gráficos");
        return request;
    }
}

public readonly record struct AssetGuid(ulong High, ulong Low)
{
    public bool IsValid => High != 0 || Low != 0;
    public static bool TryParse(string? text,out AssetGuid value)
    {
        value=default;
        if(text is null||text.Length!=32)return false;
        foreach(var c in text)if(!(c>='0'&&c<='9'||c>='a'&&c<='f'))return false;
        if(!ulong.TryParse(text[..16],NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out var high)||
           !ulong.TryParse(text[16..],NumberStyles.AllowHexSpecifier,CultureInfo.InvariantCulture,out var low)||
           (high==0&&low==0))return false;
        value=new(high,low);return true;
    }
    public static AssetGuid Parse(string text) => TryParse(text,out var value)?value:
        throw new FormatException("AssetGuid deve ter 32 dígitos hexadecimais minúsculos e não pode ser zero.");
}
