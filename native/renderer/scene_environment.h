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

enum EnvironmentOverride : u32 {
  EnvironmentOverrideSky = 1u << 0,
  EnvironmentOverrideFog = 1u << 1,
  EnvironmentOverridePost = 1u << 2,
  // Luz indireta (o Indirect Lighting Controller do Volume da Unity): quanto do
  // céu e do reflexo do ambiente chega às superfícies dentro do volume.
  EnvironmentOverrideIndirect = 1u << 3,
  EnvironmentOverrideAll = EnvironmentOverrideSky | EnvironmentOverrideFog | EnvironmentOverridePost |
                           EnvironmentOverrideIndirect,
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
  float skyZenith[3]{0.14f, 0.38f, 0.72f};
  float skyHorizon[3]{0.46f, 0.66f, 0.82f};
  float ground[3]{0.18f, 0.32f, 0.50f};
  float atmosphere = 1.0f;
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

  bool valid() const noexcept {
    if (static_cast<u32>(sky) > 2 || static_cast<u32>(toneMapper) > 2) return false;
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
                          aerosolScaleHeightKm, atmosphereHeightKm, groundAlbedo};
    for (float value : scalars) if (!std::isfinite(value)) return false;
    for (const float *color : {skyZenith, skyHorizon, ground, fogColor})
      for (u32 channel = 0; channel < 3; ++channel)
        if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f)
          return false;
    return hdriRotationDegrees>=-360.0f && hdriRotationDegrees<=360.0f &&
           hdriExposureEv>=-16.0f && hdriExposureEv<=16.0f &&
           atmosphere >= 0.0f && atmosphere <= 1.0f &&
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
           indirectDiffuse >= 0.0f && indirectDiffuse <= 4.0f &&
           indirectSpecular >= 0.0f && indirectSpecular <= 4.0f &&
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

// Look inicial da Scene View quando o documento ainda não possui um Ambiente.
// É transitório: não cria componente, não altera a cena e não afeta a câmera
// de jogo. Assim o autor começa com céu, luz e pós coerentes e pode substituí-los
// ao adicionar um Environment/Profile.
SceneEnvironment defaultSceneViewEnvironment();

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
