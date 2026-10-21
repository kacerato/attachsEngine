#pragma once

#include "core/base.h"
#include "resources/asset_registry.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace ae::renderer {

enum class SkyModel : u32 { Hdri = 0, Atmosphere = 1, PhysicalAtmosphere = 2 };
// Neutral was the historical name for Reinhard, not Unity's Neutral curve.
enum class ToneMapper : u32 { Reinhard = 0, Neutral = Reinhard, Aces = 1, AgX = 2 };
enum class EnvironmentVolumeShape : u32 { Global = 0, Box = 1, Sphere = 2 };
// PK-1/PK-2: os enums da janela Lighting da Unity 6 e do Skybox/Procedural.
// Sun do Skybox/Procedural: None, Simple, High Quality.
enum class SunDiskMode : u32 { None = 0, Simple = 1, HighQuality = 2 };
// Environment Lighting › Source.
enum class AmbientSource : u32 { Skybox = 0, Gradient = 1, Color = 2 };
// Other Settings › Fog › Mode (RenderSettings.fogMode).
enum class FogMode : u32 { Linear = 0, Exponential = 1, ExponentialSquared = 2 };
// Cor da neblina: a do autor (Unity URP) ou acompanhando a luz do céu (o
// comportamento anterior da Astra, como o "Sky Color" do HDRP).
enum class FogColorMode : u32 { Color = 0, SkyLight = 1 };

enum EnvironmentOverride : u32 {
  EnvironmentOverrideSky = 1u << 0,
  EnvironmentOverrideFog = 1u << 1,
  EnvironmentOverridePost = 1u << 2,
  // Luz indireta (o Indirect Lighting Controller do Volume da Unity): quanto do
  // céu e do reflexo do ambiente chega às superfícies dentro do volume.
  EnvironmentOverrideIndirect = 1u << 3,
  // Cloud Layer: override próprio, como na Unity, separado do modelo de céu.
  EnvironmentOverrideClouds = 1u << 4,
  EnvironmentOverrideAll = EnvironmentOverrideSky | EnvironmentOverrideFog | EnvironmentOverridePost |
                           EnvironmentOverrideIndirect | EnvironmentOverrideClouds,
};

// G2: gradação de cor depois do tonemap, aplicada por uma LUT 32³ montada na
// CPU (renderer/color_grading.h). O modelo segue o Volume da URP 17 (White
// Balance, Lift Gamma Gain e Shadows Midtones Highlights: cor do trackball +
// deslocamento) e a ordem do Environment da Godot 4.5, que gradua a imagem já
// mapeada para o monitor. Os valores padrão são neutros: sem LUT, sem custo, e
// a imagem continua a mesma de antes do G2.
struct ColorGrading final {
  // Balanço de branco: -100 (frio/verde) a 100 (quente/magenta).
  float temperature = 0.0f;
  float tint = 0.0f;
  // Trackballs: cor linear (branco é o centro) + deslocamento de -1 a 1.
  float liftColor[3]{1.0f, 1.0f, 1.0f};
  float lift = 0.0f;
  float gammaColor[3]{1.0f, 1.0f, 1.0f};
  float gamma = 0.0f;
  float gainColor[3]{1.0f, 1.0f, 1.0f};
  float gain = 0.0f;
  float shadowsColor[3]{1.0f, 1.0f, 1.0f};
  float shadows = 0.0f;
  float midtonesColor[3]{1.0f, 1.0f, 1.0f};
  float midtones = 0.0f;
  float highlightsColor[3]{1.0f, 1.0f, 1.0f};
  float highlights = 0.0f;
  // Faixas de luminância (já mapeada) das sombras e dos realces.
  float shadowsStart = 0.0f;
  float shadowsEnd = 0.3f;
  float highlightsStart = 0.55f;
  float highlightsEnd = 1.0f;
  // Satura mais as cores apagadas que as já saturadas (pele não estoura).
  float vibrance = 0.0f;

  bool operator==(const ColorGrading &) const = default;
  bool neutral() const noexcept {
    const auto white=[](const float color[3]) {
      return color[0]==1.0f && color[1]==1.0f && color[2]==1.0f;
    };
    return temperature==0.0f && tint==0.0f && vibrance==0.0f &&
           lift==0.0f && gamma==0.0f && gain==0.0f &&
           shadows==0.0f && midtones==0.0f && highlights==0.0f &&
           white(liftColor) && white(gammaColor) && white(gainColor) &&
           white(shadowsColor) && white(midtonesColor) && white(highlightsColor);
  }
  bool valid() const noexcept {
    const float offsets[]{lift, gamma, gain, shadows, midtones, highlights, vibrance};
    for (float value : offsets) if (!std::isfinite(value) || value < -1.0f || value > 1.0f) return false;
    for (const float *color : {liftColor, gammaColor, gainColor, shadowsColor, midtonesColor, highlightsColor})
      for (u32 channel = 0; channel < 3; ++channel)
        if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f) return false;
    const float ranges[]{shadowsStart, shadowsEnd, highlightsStart, highlightsEnd};
    for (float value : ranges) if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
    return std::isfinite(temperature) && std::isfinite(tint) &&
           temperature >= -100.0f && temperature <= 100.0f && tint >= -100.0f && tint <= 100.0f;
  }
};

// G4b: nuvens 2D, o Cloud Layer do HDRP 17 (Volume › Add Override › Sky ›
// Cloud Layer, e Cloud Type = Cloud Layer no Visual Environment). Uma camada
// sobre o mapa procedural de renderer/cloud_layer.h: cobertura e cirros fazem
// o papel de Opacity R (cúmulos) e Opacity B (cirros) do DefaultCloudMap. As
// sombras projetam o MESMO mapa no chão pela direção do sol a partir da
// Altitude, então a sombra cai embaixo da nuvem vista. Desligado é neutro.
struct CloudLayer final {
  bool enabled = false;          // Cloud Type: None / Cloud Layer
  bool castShadows = true;       // Cast Shadows (luz direcional principal)
  float opacity = 1.0f;          // Opacity
  float coverage = 0.45f;        // parte do céu com cúmulos (0 limpo, 1 encoberto)
  float cirrus = 0.3f;           // véu alto de cirros
  float altitudeMeters = 2000.0f;// Altitude
  float sizeMeters = 6000.0f;    // lado de uma repetição do mapa no mundo
  float tint[3]{1.0f, 1.0f, 1.0f};
  float exposureEv = 0.0f;       // Exposure Compensation, relativa ao sol
  float windOrientationDegrees = 0.0f; // Wind › Orientation (0 = +X do mundo)
  float windSpeedKmh = 20.0f;    // Wind › Speed
  float density = 0.5f;          // Raymarching › Density (mais escuro por dentro)
  float ambientDimmer = 1.0f;    // Ambient Probe Dimmer
  float shadowMultiplier = 0.8f; // Shadow Multiplier

  bool operator==(const CloudLayer &) const = default;
  bool valid() const noexcept {
    const float values[]{opacity, coverage, cirrus, altitudeMeters, sizeMeters, exposureEv,
                         windOrientationDegrees, windSpeedKmh, density, ambientDimmer, shadowMultiplier,
                         tint[0], tint[1], tint[2]};
    for (float value : values) if (!std::isfinite(value)) return false;
    for (float channel : tint) if (channel < 0.0f || channel > 1.0f) return false;
    return opacity >= 0.0f && opacity <= 1.0f && coverage >= 0.0f && coverage <= 1.0f &&
           cirrus >= 0.0f && cirrus <= 1.0f && altitudeMeters >= 100.0f && altitudeMeters <= 20000.0f &&
           sizeMeters >= 200.0f && sizeMeters <= 100000.0f && exposureEv >= -8.0f && exposureEv <= 8.0f &&
           windOrientationDegrees >= 0.0f && windOrientationDegrees <= 360.0f &&
           windSpeedKmh >= 0.0f && windSpeedKmh <= 400.0f && density >= 0.0f && density <= 1.0f &&
           ambientDimmer >= 0.0f && ambientDimmer <= 1.0f &&
           shadowMultiplier >= 0.0f && shadowMultiplier <= 1.0f;
  }
};

// Resultado autoral já resolvido para uma vista. O componente de cena e o
// renderer compartilham este contrato sem fazer o backend conhecer o editor.
struct SceneEnvironment final {
  bool active = false;
  bool fog = false;
  bool post = true;
  bool bloom = true;
  bool vignette = false;
  bool filmGrain = false;
  bool ambientOcclusion = false;
  bool physicalAtmosphereHighQuality = false;
  SkyModel sky = SkyModel::Atmosphere;
  resources::AssetGuid environmentMap{};
  float hdriRotationDegrees = 0.0f;
  float hdriExposureEv = 0.0f;
  ToneMapper toneMapper = ToneMapper::Aces;
  float priority = 0.0f;
  // Linear-light authoring values for a clear daytime procedural sky. Saved
  // environments keep their own colours; this does not retint physical skies.
  // Sky Tint do céu procedural (Skybox/Procedural da Unity): 0,5 é neutro.
  float skyZenith[3]{0.5f, 0.5f, 0.5f};
  float skyHorizon[3]{0.46f, 0.66f, 0.82f};
  float ground[3]{0.369f, 0.349f, 0.341f}; // Ground do Skybox/Procedural da Unity
  // Atmosphere Thickness do Skybox/Procedural (0..5). Até a v16 era uma
  // mistura 0..1 com o panorama; a migração devolve 1, a espessura de antes.
  float atmosphere = 1.0f;
  // PK-1: Exposure do Skybox (Unity 1,3). Só o céu procedural; o HDRI usa
  // hdriExposureEv e o físico physicalSkyIntensity. Nunca mais depende da
  // intensidade do sol (o antigo skyScale).
  float skyExposure = 1.3f;
  // PK-2: o sol do Skybox/Procedural (Sun, Sun Size, Sun Size Convergence).
  SunDiskMode sunDisk = SunDiskMode::HighQuality;
  float sunSize = 0.04f;
  float sunSizeConvergence = 5.0f;
  // Disco do céu físico (Physically Based Sky): diâmetro e brilho próprios.
  float sunDiskDegrees = 0.53f;
  float sunDiskIntensity = 8.0f;
  // Physical-atmosphere distances use kilometres. Keeping the planetary
  // calculation near 10^3 rather than 10^6 avoids precision loss in the
  // mobile fragment shader's ray/sphere intersections. This is real-time
  // single scattering: ozone, multiple scattering and volumetric aerial
  // perspective are outside this sky pass.
  float physicalSkyIntensity = 1.0f;
  float airDensity = 1.0f;
  float aerosolDensity = 1.0f;
  float aerosolAnisotropy = 0.76f;
  float planetRadiusKm = 6371.0f;
  float observerHeightKm = 0.002f;
  float rayleighScaleHeightKm = 8.0f;
  float aerosolScaleHeightKm = 1.2f;
  float atmosphereHeightKm = 100.0f;
  float groundAlbedo = 0.10f;
  // PK-1: Environment Lighting. Skybox tira harmônicos esféricos do próprio
  // céu; Gradient e Color usam as cores abaixo (sRGB, como o seletor da Unity:
  // ambientSkyColor/EquatorColor/GroundColor). A intensidade é indirectDiffuse.
  AmbientSource ambientSource = AmbientSource::Skybox;
  float ambientSkyColor[3]{0.212f, 0.227f, 0.259f};
  float ambientEquatorColor[3]{0.114f, 0.125f, 0.133f};
  float ambientGroundColor[3]{0.047f, 0.043f, 0.035f};
  float ambientColor[3]{0.212f, 0.227f, 0.259f}; // Source = Color (Ambient Color)
  // Fog da janela Lighting: Mode, Start/End (Linear), Density (exponenciais).
  FogMode fogMode = FogMode::ExponentialSquared;
  FogColorMode fogColorMode = FogColorMode::Color;
  float fogEnd = 300.0f;
  float fogColor[3]{0.58f, 0.67f, 0.76f};
  // Multiplies the linear fog tint into scene-referred HDR radiance.
  float fogLightEnergy = 1.0f;
  float fogDensity = 0.008f;
  float fogStart = 8.0f;
  // Density equals fogDensity at this world-space height and decays upward as
  // exp(-fogHeightFalloff * (height - fogBaseHeight)). Falloff zero preserves
  // the historical uniform exponential fog exactly.
  float fogBaseHeight = 0.0f;
  float fogHeightFalloff = 0.0f;
  float exposureEv = 0.0f;
  // Quando ativo, exposureEv compensa o EV medido do HDR da câmera.
  bool autoExposure = false;
  float autoExposureMinEv = -8.0f;
  float autoExposureMaxEv = 8.0f;
  float autoExposureLowPercent = 0.05f;
  float autoExposureHighPercent = 0.95f;
  float autoExposureTargetGrey = 0.18f;
  float autoExposureSpeedUp = 2.0f;
  float autoExposureSpeedDown = 3.0f;
  bool autoExposureCenterWeighted = false;
  float bloomThreshold = 1.0f;
  float bloomIntensity = 0.10f;
  float contrast = 1.0f;
  float saturation = 1.0f;
  float vignetteIntensity = 0.18f;
  float filmGrainIntensity = 0.05f;
  float ambientOcclusionRadius = 1.0f;
  float ambientOcclusionIntensity = 1.0f;
  float ambientOcclusionPower = 1.5f;
  float ambientOcclusionBias = 0.02f;
  // Multiplicadores de luz indireta, como Indirect Diffuse Lighting Multiplier e
  // Reflection Lighting Multiplier da Unity. Sem sondas de luz, é o que diz ao
  // renderer que dentro de uma sala o céu não chega inteiro: 1 é o legado.
  float indirectDiffuse = 1.0f;
  float indirectSpecular = 1.0f;
  ColorGrading grading{};
  // G4: corpos celestes do céu procedural e físico. Zero desliga cada um.
  // A direção da lua sai da do sol e da fase (oposta na cheia), então quem move
  // o sol -- luz direcional hoje, TimeOfDay depois -- move a lua junto.
  float sunGlare = 0.6f;          // auréola em volta do disco solar
  float moonPhase = 0.5f;         // 0 nova, 0.5 cheia, 1 nova de novo
  float moonSizeDegrees = 1.2f;   // diâmetro aparente (o real é ~0,52°)
  float moonIntensity = 1.0f;
  float starIntensity = 1.0f;
  CloudLayer clouds{};
  // PK-2: Material do céu (recurso Skybox). Vazio = céu desta instância.
  resources::AssetGuid skybox{};

  bool valid() const noexcept {
    if (static_cast<u32>(sky) > 2 || static_cast<u32>(toneMapper) > 2 || !grading.valid() || !clouds.valid() ||
        static_cast<u32>(sunDisk) > 2 || static_cast<u32>(ambientSource) > 2 || static_cast<u32>(fogMode) > 2 ||
        static_cast<u32>(fogColorMode) > 1) return false;
    const float scalars[]{priority, atmosphere, sunDiskDegrees, sunDiskIntensity, hdriRotationDegrees, hdriExposureEv,
                          fogLightEnergy, fogDensity, fogStart, fogBaseHeight, fogHeightFalloff,
                          exposureEv, autoExposureMinEv, autoExposureMaxEv,
                          autoExposureLowPercent, autoExposureHighPercent,
                          autoExposureTargetGrey, autoExposureSpeedUp, autoExposureSpeedDown,
                          bloomThreshold,
                          bloomIntensity, contrast, saturation, vignetteIntensity,
                          filmGrainIntensity,
                          ambientOcclusionRadius, ambientOcclusionIntensity,
                          ambientOcclusionPower, ambientOcclusionBias, indirectDiffuse, indirectSpecular,
                          physicalSkyIntensity, airDensity, aerosolDensity, aerosolAnisotropy,
                          planetRadiusKm, observerHeightKm, rayleighScaleHeightKm,
                          aerosolScaleHeightKm, atmosphereHeightKm, groundAlbedo,
                          sunGlare, moonPhase, moonSizeDegrees, moonIntensity, starIntensity,
                          skyExposure, sunSize, sunSizeConvergence, fogEnd};
    for (float value : scalars) if (!std::isfinite(value)) return false;
    for (const float *color : {skyZenith, skyHorizon, ground, fogColor, ambientSkyColor, ambientEquatorColor,
                               ambientGroundColor, ambientColor})
      for (u32 channel = 0; channel < 3; ++channel)
        if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f)
          return false;
    return hdriRotationDegrees>=-360.0f && hdriRotationDegrees<=360.0f &&
           hdriExposureEv>=-16.0f && hdriExposureEv<=16.0f &&
           atmosphere >= 0.0f && atmosphere <= 5.0f &&
           skyExposure >= 0.0f && skyExposure <= 1000.0f && sunSize >= 0.0f && sunSize <= 1.0f &&
           sunSizeConvergence >= 1.0f && sunSizeConvergence <= 10.0f &&
           fogEnd >= 0.0f && fogEnd <= 1000000.0f &&
           sunDiskDegrees >= 0.05f && sunDiskDegrees <= 10.0f && sunDiskIntensity >= 0.0f &&
           fogLightEnergy >= 0.0f && fogLightEnergy <= 65504.0f &&
           fogDensity >= 0.0f && fogDensity <= 1.0f && fogStart >= 0.0f &&
           fogBaseHeight >= -100000.0f && fogBaseHeight <= 100000.0f &&
           fogHeightFalloff >= 0.0f && fogHeightFalloff <= 10.0f &&
           exposureEv >= -16.0f && exposureEv <= 16.0f &&
           autoExposureMinEv >= -16.0f && autoExposureMaxEv <= 16.0f &&
           autoExposureMinEv <= autoExposureMaxEv &&
           autoExposureLowPercent >= 0.0f && autoExposureHighPercent <= 1.0f &&
           autoExposureLowPercent < autoExposureHighPercent &&
           autoExposureTargetGrey >= 0.01f && autoExposureTargetGrey <= 1.0f &&
           autoExposureSpeedUp >= 0.01f && autoExposureSpeedUp <= 20.0f &&
           autoExposureSpeedDown >= 0.01f && autoExposureSpeedDown <= 20.0f &&
           bloomThreshold >= 0.0f && bloomThreshold <= 64.0f &&
           bloomIntensity >= 0.0f && bloomIntensity <= 2.0f &&
           contrast >= 0.5f && contrast <= 2.0f && saturation >= 0.0f && saturation <= 2.0f &&
           vignetteIntensity >= 0.0f && vignetteIntensity <= 1.0f &&
           filmGrainIntensity >= 0.0f && filmGrainIntensity <= 1.0f &&
           ambientOcclusionRadius >= 0.05f && ambientOcclusionRadius <= 10.0f &&
           ambientOcclusionIntensity >= 0.0f && ambientOcclusionIntensity <= 4.0f &&
           ambientOcclusionPower >= 0.1f && ambientOcclusionPower <= 4.0f &&
           ambientOcclusionBias >= 0.0f && ambientOcclusionBias <= 1.0f &&
           indirectDiffuse >= 0.0f && indirectDiffuse <= 8.0f &&
           indirectSpecular >= 0.0f && indirectSpecular <= 8.0f &&
           sunGlare >= 0.0f && sunGlare <= 4.0f && moonPhase >= 0.0f && moonPhase <= 1.0f &&
           moonSizeDegrees >= 0.1f && moonSizeDegrees <= 10.0f &&
           moonIntensity >= 0.0f && moonIntensity <= 8.0f &&
           starIntensity >= 0.0f && starIntensity <= 8.0f &&
           physicalSkyIntensity >= 0.0f && physicalSkyIntensity <= 16.0f &&
           airDensity >= 0.0f && airDensity <= 8.0f &&
           aerosolDensity >= 0.0f && aerosolDensity <= 8.0f &&
           aerosolAnisotropy >= 0.0f && aerosolAnisotropy <= 0.95f &&
           planetRadiusKm >= 1.0f && planetRadiusKm <= 100000.0f &&
           observerHeightKm >= 0.0f && observerHeightKm <= atmosphereHeightKm &&
           rayleighScaleHeightKm >= 0.1f && rayleighScaleHeightKm <= 100.0f &&
           aerosolScaleHeightKm >= 0.05f && aerosolScaleHeightKm <= 50.0f &&
           atmosphereHeightKm >= 1.0f && atmosphereHeightKm <= 1000.0f &&
           groundAlbedo >= 0.0f && groundAlbedo <= 1.0f;
  }
};

// PK-2 (A-05): o ambiente de uma cena sem nenhum Ambiente, igual no editor e no
// Play -- o que a Unity dá a toda cena nova: Default-Skybox procedural, luz
// ambiente do Skybox e pós neutro. Também é a base sobre a qual volumes locais
// se misturam quando não há volume global.
SceneEnvironment defaultSceneEnvironment();

// Instância resolvida da cena. `worldToLocal` mantém o volume independente do
// editor e do grafo; o renderer só precisa da posição da câmera para atualizar
// a mistura a cada quadro. A forma local é centrada no Transform do objeto.
struct SceneEnvironmentVolume final {
  SceneEnvironment environment{};
  EnvironmentVolumeShape shape = EnvironmentVolumeShape::Global;
  u32 overrides = EnvironmentOverrideAll;
  u32 layer = 0;
  u64 stableId = 0;
  float weight = 1.0f;
  float blendDistance = 0.0f;
  float boxSize[3]{10.0f, 10.0f, 10.0f};
  float sphereRadius = 5.0f;
  float worldToLocal[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  float localToWorld[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

  bool valid() const noexcept {
    if (!environment.valid() || static_cast<u32>(shape) > 2 || overrides > EnvironmentOverrideAll ||
        layer > 31 || !std::isfinite(weight) || weight < 0 || weight > 1 ||
        !std::isfinite(blendDistance) || blendDistance < 0 ||
        !std::isfinite(sphereRadius) || sphereRadius <= 0) return false;
    for (float value : boxSize) if (!std::isfinite(value) || value <= 0) return false;
    for (float value : worldToLocal) if (!std::isfinite(value)) return false;
    for (float value : localToWorld) if (!std::isfinite(value)) return false;
    return true;
  }
};

struct SceneEnvironmentBlendReport final {
  u32 considered = 0;
  u32 contributing = 0;
  u64 dominant = 0;
  float dominantInfluence = 0;
};

float sceneEnvironmentVolumeInfluence(const SceneEnvironmentVolume &volume,
                                      const float cameraPosition[3]);

// Resolve na ordem de prioridade. Volumes de mesma prioridade usam stableId,
// portanto salvar/reabrir ou mudar a ordem interna não altera o resultado.
// `layerMask` segue a convenção de 32 layers da cena; ~0u aceita todas.
SceneEnvironment resolveSceneEnvironment(std::span<const SceneEnvironmentVolume> volumes,
                                         const float cameraPosition[3], u32 layerMask = ~0u,
                                         SceneEnvironmentBlendReport *report = nullptr);

} // namespace ae::renderer
