#pragma once

#include "core/base.h"
#include "resources/asset_registry.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace ae::renderer {

enum class SkyModel : u32 { Hdri = 0, Atmosphere = 1, PhysicalAtmosphere = 2 };
// Tonemapping da Unity (URP/HDRP 17): None, Neutral e ACES. AgX fica como
// extra. Reinhard (0) é só o legado: cenas antigas migram para Neutral.
enum class ToneMapper : u32 { Reinhard = 0, Aces = 1, AgX = 2, None = 3, Neutral = 4 };
enum class EnvironmentVolumeShape : u32 { Global = 0, Box = 1, Sphere = 2 };
// Override Exposure do Volume do HDRP 17 (Exposure.cs). Só os modos que a Astra
// executa: Curve Mapping e Physical Camera não têm consumidor e ficam de fora.
enum class ExposureMode : u32 { Fixed = 0, Automatic = 1, AutomaticHistogram = 2 };
// Metering Mode do HDRP (ExposureCommon.hlsl, WeightSample): média de toda a
// imagem, círculo de 7,5% no centro, ou peso que cai linearmente até a borda.
enum class ExposureMetering : u32 { Average = 0, Spot = 1, CenterWeighted = 2 };
// Adaptation Mode: Fixed salta para o alvo; Progressive aproxima exponencialmente.
enum class ExposureAdaptation : u32 { Fixed = 0, Progressive = 1 };

enum EnvironmentOverride : u32 {
  EnvironmentOverrideSky = 1u << 0,
  EnvironmentOverrideFog = 1u << 1,
  EnvironmentOverridePost = 1u << 2,
  // Luz indireta (o Indirect Lighting Controller do Volume da Unity): quanto do
  // céu e do reflexo do ambiente chega às superfícies dentro do volume.
  EnvironmentOverrideIndirect = 1u << 3,
  // Overrides de pós no modelo do Volume da Unity: cada um é uma caixa própria
  // no Inspector. O que fica desmarcado herda do volume de prioridade menor, e
  // por fim dos padrões do mundo (o Default Volume Profile do HDRP). O bit Post
  // passa a ser o Tonemapping (com a chave geral do pós).
  EnvironmentOverrideExposure = 1u << 4,
  EnvironmentOverrideBloom = 1u << 5,
  EnvironmentOverrideVignette = 1u << 6,
  EnvironmentOverrideColorAdjustments = 1u << 7,
  EnvironmentOverrideColorGrading = 1u << 8,
  EnvironmentOverrideFilmGrain = 1u << 9,
  EnvironmentOverrideAmbientOcclusion = 1u << 10,
  EnvironmentOverridePostEffects = EnvironmentOverridePost | EnvironmentOverrideExposure | EnvironmentOverrideBloom |
                                   EnvironmentOverrideVignette | EnvironmentOverrideColorAdjustments |
                                   EnvironmentOverrideColorGrading | EnvironmentOverrideFilmGrain |
                                   EnvironmentOverrideAmbientOcclusion,
  // Cloud Layer: override próprio, como na Unity, separado do modelo de céu.
  EnvironmentOverrideClouds = 1u << 11,
  EnvironmentOverrideAll = EnvironmentOverrideSky | EnvironmentOverrideFog | EnvironmentOverrideIndirect |
                           EnvironmentOverridePostEffects | EnvironmentOverrideClouds,
};

// Constante de calibração do fotômetro (K) e conversões de EV100 do HDRP
// (PhysicalCamera.hlsl). Na Astra as luzes não estão em lux; a fórmula é a mesma
// sobre a radiância da cena já multiplicada pela exposição base da vista.
inline float exposureEv100FromLuminance(float luminance, float targetMidGrey) {
  return std::log2(std::max(luminance, 1.0e-4f) / targetMidGrey); // log2(L·100/K), K = cinza·100
}
inline float exposureMultiplierFromEv100(float ev100) { return 1.0f / (1.2f * std::exp2(ev100)); }
// EV de clareamento que o pós aplica (exp2) para chegar ao mesmo multiplicador.
inline float exposureBrighteningEvFromEv100(float ev100) { return -ev100 - std::log2(1.2f); }
// ComputeLuminanceAdaptation do SRP Core: decaimento exponencial em EV100.
inline float adaptExposureEv100(float previous, float target, float deltaSeconds,
                                float speedDarkToLight, float speedLightToDark) {
  const float delta = target - previous;
  const float speed = delta > 0.0f ? speedDarkToLight : speedLightToDark;
  return previous + delta * (1.0f - std::exp2(-std::max(deltaSeconds, 0.0f) * speed));
}

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
  // Exposure (HDRP 17). Fixed usa fixedExposure; os automáticos medem a
  // luminância da câmera em EV100, somam a compensação, adaptam e limitam.
  // O padrão é o do Default Volume Profile do HDRP: Automatic, Center Weighted.
  ExposureMode exposureMode = ExposureMode::Automatic;
  ExposureMetering exposureMetering = ExposureMetering::CenterWeighted;
  ExposureAdaptation exposureAdaptation = ExposureAdaptation::Progressive;
  float fixedExposure = 0.0f;          // EV100
  float exposureCompensation = 0.0f;   // EV; positivo clareia
  // Limit Min/Max em EV100 da cena. Os -1/14 do HDRP supõem sol de 100000 lux
  // (meio-dia ~EV100 14). Na Astra o meio-dia mede ~0 e o poente da Vila ~-2,7
  // (POCO, 20/10/2026). V-2: com o céu noturno escuro e a Lua de 0,5 lux, a
  // noite mede ~-12 (luar) a ~-10 (praça com postes). O limite fica ~2 EV acima
  // disso, como o Volume noturno da Unity (exposição fixa EV 1 sobre ~-1,4
  // medidos no tutorial HDRP Night): a noite fica escura, a praça dos postes
  // legível, e dia e poente, que medem acima de -8, não mudam.
  float exposureLimitMin = -8.0f;
  float exposureLimitMax = 10.0f;
  float exposureHistogramLow = 40.0f;  // Histogram Percentages, 0 a 100
  float exposureHistogramHigh = 90.0f;
  float exposureTargetMidGrey = 0.125f; // Grey 12.5%, 14% ou 18%
  float exposureSpeedDarkToLight = 3.0f;
  float exposureSpeedLightToDark = 1.0f;
  bool automaticExposure() const noexcept { return exposureMode != ExposureMode::Fixed; }
  // EV de clareamento da exposição fixa (e o reserva dos automáticos sem compute).
  float fixedBrighteningEv() const noexcept {
    return exposureBrighteningEvFromEv100(fixedExposure - exposureCompensation);
  }
  float bloomThreshold = 1.0f;
  float bloomIntensity = 0.10f;
  float contrast = 1.0f;
  float saturation = 1.0f;
  // Color Adjustments da Unity: Post Exposure (EV, depois da exposição e antes
  // do tonemap), Color Filter (multiplica a cor HDR) e Hue Shift (graus).
  float postExposure = 0.0f;
  float colorFilter[3]{1.0f, 1.0f, 1.0f};
  float hueShift = 0.0f;
  // Bloom da Unity: Scatter (0 a 1, quanto o brilho se espalha), Tint e Clamp
  // (brilho máximo que entra no bloom; o padrão da Unity é 65472).
  float bloomScatter = 0.7f;
  float bloomTint[3]{1.0f, 1.0f, 1.0f};
  float bloomClamp = 65472.0f;
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

  bool valid() const noexcept {
    if (static_cast<u32>(sky) > 2 || static_cast<u32>(toneMapper) > 4 || !grading.valid() || !clouds.valid() ||
        static_cast<u32>(exposureMode) > 2 || static_cast<u32>(exposureMetering) > 2 ||
        static_cast<u32>(exposureAdaptation) > 1) return false;
    const float scalars[]{priority, atmosphere, sunDiskDegrees, sunDiskIntensity, hdriRotationDegrees, hdriExposureEv,
                          fogLightEnergy, fogDensity, fogStart, fogBaseHeight, fogHeightFalloff,
                          fixedExposure, exposureCompensation, exposureLimitMin, exposureLimitMax,
                          exposureHistogramLow, exposureHistogramHigh,
                          exposureTargetMidGrey, exposureSpeedDarkToLight, exposureSpeedLightToDark,
                          bloomThreshold,
                          bloomIntensity, contrast, saturation, vignetteIntensity,
                          filmGrainIntensity,
                          ambientOcclusionRadius, ambientOcclusionIntensity,
                          ambientOcclusionPower, ambientOcclusionBias, indirectDiffuse, indirectSpecular,
                          physicalSkyIntensity, airDensity, aerosolDensity, aerosolAnisotropy,
                          planetRadiusKm, observerHeightKm, rayleighScaleHeightKm,
                          aerosolScaleHeightKm, atmosphereHeightKm, groundAlbedo,
                          sunGlare, moonPhase, moonSizeDegrees, moonIntensity, starIntensity,
                          postExposure, hueShift, bloomScatter, bloomClamp};
    for (float value : scalars) if (!std::isfinite(value)) return false;
    for (const float *color : {colorFilter, bloomTint})
      for (u32 channel = 0; channel < 3; ++channel)
        if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f) return false;
    if (postExposure < -16.0f || postExposure > 16.0f || hueShift < -180.0f || hueShift > 180.0f ||
        bloomScatter < 0.0f || bloomScatter > 1.0f || bloomClamp < 0.0f || bloomClamp > 65472.0f) return false;
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
           fixedExposure >= -16.0f && fixedExposure <= 24.0f &&
           exposureCompensation >= -16.0f && exposureCompensation <= 16.0f &&
           exposureLimitMin >= -16.0f && exposureLimitMax <= 24.0f &&
           exposureLimitMin <= exposureLimitMax &&
           exposureHistogramLow >= 0.0f && exposureHistogramHigh <= 100.0f &&
           exposureHistogramLow < exposureHistogramHigh &&
           exposureTargetMidGrey >= 0.05f && exposureTargetMidGrey <= 0.5f &&
           exposureSpeedDarkToLight >= 0.001f && exposureSpeedDarkToLight <= 100.0f &&
           exposureSpeedLightToDark >= 0.001f && exposureSpeedLightToDark <= 100.0f &&
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

// Cenas e perfis gravados antes da Exposure do HDRP guardavam EV de
// clareamento (exp2(ev) multiplicava a imagem), percentis em fração e
// velocidades lineares. Os campos novos foram lidos com esse sentido antigo;
// isto os converte para o mesmo multiplicador em EV100 (-ev - log2(1,2)).
// `legacyNumbers` diz se o arquivo tinha os números da exposição automática.
inline void migrateLegacyExposure(SceneEnvironment &values,bool legacyNumbers,bool automatic,bool centerWeighted) {
  const float toEv100=std::log2(1.2f);
  if(legacyNumbers) {
    // Limites antigos eram de clareamento; a ordem se inverte em EV100.
    const float minimum=values.exposureLimitMin,maximum=values.exposureLimitMax;
    values.exposureLimitMin=std::clamp(-maximum-toEv100,-16.0f,24.0f);
    values.exposureLimitMax=std::clamp(-minimum-toEv100,values.exposureLimitMin,24.0f);
    values.exposureHistogramLow=std::clamp(values.exposureHistogramLow*100.0f,0.0f,99.0f);
    values.exposureHistogramHigh=std::clamp(values.exposureHistogramHigh*100.0f,values.exposureHistogramLow+1.0f,100.0f);
    const float grey=values.exposureTargetMidGrey;
    values.exposureTargetMidGrey=grey<.1325f?.125f:grey<.16f?.14f:.18f;
    // "Subir" (clarear) era a resposta a uma cena que escureceu: Light to Dark.
    std::swap(values.exposureSpeedDarkToLight,values.exposureSpeedLightToDark);
  }
  if(!automatic) {
    values.exposureMode=ExposureMode::Fixed;
    values.fixedExposure=std::clamp(-values.exposureCompensation-toEv100,-16.0f,24.0f);
    values.exposureCompensation=0;
    return;
  }
  values.exposureMode=ExposureMode::AutomaticHistogram;
  values.exposureMetering=centerWeighted?ExposureMetering::CenterWeighted:ExposureMetering::Average;
}

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
