#pragma once

#include "core/base.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace ae::renderer {

enum class SkyModel : u32 { Hdri = 0, Atmosphere = 1 };
enum class ToneMapper : u32 { Neutral = 0, Aces = 1 };
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
  SkyModel sky = SkyModel::Atmosphere;
  ToneMapper toneMapper = ToneMapper::Aces;
  float priority = 0.0f;
  float skyZenith[3]{0.025f, 0.10f, 0.32f};
  float skyHorizon[3]{0.28f, 0.42f, 0.62f};
  float ground[3]{0.11f, 0.12f, 0.14f};
  float atmosphere = 1.0f;
  float sunDiskDegrees = 0.53f;
  float sunDiskIntensity = 8.0f;
  float fogColor[3]{0.58f, 0.67f, 0.76f};
  float fogDensity = 0.008f;
  float fogStart = 8.0f;
  float exposureEv = 0.0f;
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
    if (static_cast<u32>(sky) > 1 || static_cast<u32>(toneMapper) > 1) return false;
    const float scalars[]{priority, atmosphere, sunDiskDegrees, sunDiskIntensity,
                          fogDensity, fogStart, exposureEv, bloomThreshold,
                          bloomIntensity, contrast, saturation, vignetteIntensity,
                          filmGrainIntensity,
                          ambientOcclusionRadius, ambientOcclusionIntensity,
                          ambientOcclusionPower, ambientOcclusionBias, indirectDiffuse, indirectSpecular};
    for (float value : scalars) if (!std::isfinite(value)) return false;
    for (const float *color : {skyZenith, skyHorizon, ground, fogColor})
      for (u32 channel = 0; channel < 3; ++channel)
        if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f)
          return false;
    return atmosphere >= 0.0f && atmosphere <= 1.0f &&
           sunDiskDegrees >= 0.05f && sunDiskDegrees <= 10.0f && sunDiskIntensity >= 0.0f &&
           fogDensity >= 0.0f && fogDensity <= 1.0f && fogStart >= 0.0f &&
           exposureEv >= -16.0f && exposureEv <= 16.0f &&
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
           indirectSpecular >= 0.0f && indirectSpecular <= 4.0f;
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
