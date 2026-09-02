#include "renderer/rendering_policy.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::renderer {
namespace {

// Ponto de cada preset no espaço de eixos. É uma tabela de dados, não código de
// decisão: acrescentar um preset é acrescentar uma linha, e nenhum caminho de
// renderização passa a existir por causa dela.
struct PresetPoint {
  ShadowQuality shadows;
  AmbientQuality ambient;
  PostQuality post;
  TextureQuality textures;
  float resolutionScale;
  u32 shadowCascadeCount;
  u32 shadowCascadeResolution;
  u32 shadowFilterTaps;
  u32 shadowFarFilterTaps;
  float shadowMaximumDistance;
  float shadowCacheGuardBandRatio;
  float lodPixelErrorBudget;
  float coverageLodPixelErrorBudget;
  float lodHysteresisBandRatio;
  float normalMapMaximumDistance;
  float specularProbeMaximumDistance;
  float metallicRoughnessMaximumDistance;
  float emissiveMaximumDistance;
  bool dynamicResolution;
  float dynamicResolutionMinimumScale;
  bool materialShaderVariants;
};

constexpr PresetPoint presetForProfile(rhi::DeviceProfile profile) {
  switch (profile) {
    case rhi::DeviceProfile::S:
      return {ShadowQuality::UltraSoft, AmbientQuality::HemisphericSpecular, PostQuality::Bloom,
               TextureQuality::Full, 1.0f, 4, 2048, 25, 9, 320.0f, 1.05f,
               1.0f, 24.0f, 0.80f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0.85f, true};
    case rhi::DeviceProfile::A:
      return {ShadowQuality::Soft, AmbientQuality::HemisphericSpecular, PostQuality::Bloom,
               TextureQuality::Full, 1.0f, 3, 1536, 9, 1, 220.0f, 1.06f,
               1.5f, 32.0f, 0.78f, 220.0f, 400.0f, 300.0f, 400.0f, false, 0.78f, true};
    case rhi::DeviceProfile::B:
      return {ShadowQuality::Soft, AmbientQuality::Hemispheric, PostQuality::Tonemap,
               TextureQuality::Full, 1.0f, 2, 1024, 9, 1, 160.0f, 1.08f,
               2.5f, 48.0f, 0.75f, 60.0f, 120.0f, 180.0f, 240.0f, false, 0.58f, false};
    case rhi::DeviceProfile::C:
    default:
      // O perfil base não perde direcionalidade do sol: mantém uma cascata dura,
      // que é o que separa "sombra barata" de "sem noção de oclusão".
      return {ShadowQuality::Hard, AmbientQuality::Hemispheric, PostQuality::None,
               TextureQuality::Half, 0.85f, 1, 768, 1, 1, 90.0f, 1.10f,
               4.0f, 64.0f, 0.70f, 60.0f, 120.0f, 90.0f, 120.0f, true, 0.55f, false};
  }
}

constexpr rhi::DeviceProfile profileForPreset(QualityPreset preset,
                                              rhi::DeviceProfile detected) {
  switch (preset) {
    case QualityPreset::C: return rhi::DeviceProfile::C;
    case QualityPreset::B: return rhi::DeviceProfile::B;
    case QualityPreset::A: return rhi::DeviceProfile::A;
    case QualityPreset::S: return rhi::DeviceProfile::S;
    case QualityPreset::Auto:
    case QualityPreset::Custom:
    default:
      return detected;
  }
}

template <typename Axis>
constexpr Axis inherited(Axis requested, Axis fromPreset) {
  return requested == Axis::Inherit ? fromPreset : requested;
}

// Degrada um eixo em um degrau. Usada pela pressão térmica, que precisa piorar
// imediatamente sem inventar um caminho de renderização novo.
constexpr ShadowQuality degrade(ShadowQuality value) {
  switch (value) {
    case ShadowQuality::UltraSoft: return ShadowQuality::Soft;
    case ShadowQuality::Soft: return ShadowQuality::Hard;
    case ShadowQuality::Hard: return ShadowQuality::Off;
    default: return ShadowQuality::Off;
  }
}

constexpr PostQuality degrade(PostQuality value) {
  switch (value) {
    case PostQuality::Bloom: return PostQuality::Tonemap;
    case PostQuality::Tonemap: return PostQuality::None;
    default: return PostQuality::None;
  }
}

constexpr AmbientQuality degrade(AmbientQuality value) {
  switch (value) {
    case AmbientQuality::HemisphericSpecular: return AmbientQuality::Hemispheric;
    case AmbientQuality::Hemispheric: return AmbientQuality::Constant;
    default: return AmbientQuality::Constant;
  }
}

ShadowSettings deriveShadows(ShadowQuality quality, u32 maximumResolution) {
  ShadowSettings settings{};
  switch (quality) {
    case ShadowQuality::UltraSoft:
      settings = {true, 4, 2048, 25, 9, 320.0f, 1.25f, 2.0f, 2.5f, true, true, 1.05f};
      break;
    case ShadowQuality::Soft:
      settings = {true, 3, 1536, 9, 1, 220.0f, 1.5f, 2.25f, 2.0f, true, true, 1.08f};
      break;
    case ShadowQuality::Hard:
      // Uma cascata sem filtro ainda dá ao sol uma direção: objetos passam a
      // ocluir uns aos outros. O bias sobe porque, sem PCF, o acne aparece antes.
      settings = {true, 1, 1024, 1, 1, 120.0f, 2.0f, 3.0f, 1.0f, true, true, 1.10f};
      break;
    case ShadowQuality::Off:
    case ShadowQuality::Inherit:
    default:
      return {};
  }
  settings.cascadeResolution = std::min(settings.cascadeResolution, maximumResolution);
  return settings;
}

AmbientSettings deriveAmbient(AmbientQuality quality) {
  switch (quality) {
    case AmbientQuality::HemisphericSpecular: return {true, true, true};
    case AmbientQuality::Hemispheric: return {true, false, false};
    default: return {false, false, false};
  }
}

PostSettings derivePost(PostQuality quality) {
  switch (quality) {
    case PostQuality::Bloom:
      return {true, true, true, true, 1.15f, 0.28f, 1.04f, 1.03f, 0.12f, 0.16f};
    case PostQuality::Tonemap:
      return {true, false, true, false, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    default: return {};
  }
}

bool enabledOverride(FeatureOverride value, bool inheritedValue) {
  if (value == FeatureOverride::Enabled) return true;
  if (value == FeatureOverride::Disabled) return false;
  return inheritedValue;
}

u32 normalizedShadowFilterTaps(u32 taps) {
  return taps >= 25u ? 25u : taps >= 9u ? 9u : 1u;
}

TextureSettings deriveTextures(TextureQuality quality, float maximumAnisotropy) {
  TextureSettings settings{};
  settings.residencyMipBias = quality == TextureQuality::Half ? 1u : 0u;
  const float requested = quality == TextureQuality::Full ? 8.0f : 2.0f;
  settings.samplerAnisotropy = std::clamp(requested, 1.0f, std::max(1.0f, maximumAnisotropy));
  return settings;
}

} // namespace

namespace {
bool matches(const char *value, const char *expected) {
  return value != nullptr && std::strcmp(value, expected) == 0;
}
} // namespace

QualityPreset parseQualityPreset(const char *name) {
  if (matches(name, "c")) return QualityPreset::C;
  if (matches(name, "b")) return QualityPreset::B;
  if (matches(name, "a")) return QualityPreset::A;
  if (matches(name, "s")) return QualityPreset::S;
  if (matches(name, "custom")) return QualityPreset::Custom;
  return QualityPreset::Auto;
}

ShadowQuality parseShadowQuality(const char *name) {
  if (matches(name, "off")) return ShadowQuality::Off;
  if (matches(name, "hard")) return ShadowQuality::Hard;
  if (matches(name, "soft")) return ShadowQuality::Soft;
  if (matches(name, "ultra")) return ShadowQuality::UltraSoft;
  return ShadowQuality::Inherit;
}

AmbientQuality parseAmbientQuality(const char *name) {
  if (matches(name, "constant")) return AmbientQuality::Constant;
  if (matches(name, "hemispheric")) return AmbientQuality::Hemispheric;
  if (matches(name, "specular")) return AmbientQuality::HemisphericSpecular;
  return AmbientQuality::Inherit;
}

PostQuality parsePostQuality(const char *name) {
  if (matches(name, "none")) return PostQuality::None;
  if (matches(name, "tonemap")) return PostQuality::Tonemap;
  if (matches(name, "bloom")) return PostQuality::Bloom;
  return PostQuality::Inherit;
}

TextureQuality parseTextureQuality(const char *name) {
  if (matches(name, "half")) return TextureQuality::Half;
  if (matches(name, "full")) return TextureQuality::Full;
  return TextureQuality::Inherit;
}

const char *shadowQualityName(ShadowQuality quality) {
  switch (quality) {
    case ShadowQuality::Off: return "off";
    case ShadowQuality::Hard: return "hard";
    case ShadowQuality::Soft: return "soft";
    case ShadowQuality::UltraSoft: return "ultra";
    default: return "inherit";
  }
}

const char *ambientQualityName(AmbientQuality quality) {
  switch (quality) {
    case AmbientQuality::Constant: return "constant";
    case AmbientQuality::Hemispheric: return "hemispheric";
    case AmbientQuality::HemisphericSpecular: return "specular";
    default: return "inherit";
  }
}

const char *postQualityName(PostQuality quality) {
  switch (quality) {
    case PostQuality::None: return "none";
    case PostQuality::Tonemap: return "tonemap";
    case PostQuality::Bloom: return "bloom";
    default: return "inherit";
  }
}

const char *textureQualityName(TextureQuality quality) {
  switch (quality) {
    case TextureQuality::Half: return "half";
    case TextureQuality::Full: return "full";
    default: return "inherit";
  }
}

const char *qualityPresetName(QualityPreset preset) {
  switch (preset) {
    case QualityPreset::Auto: return "auto";
    case QualityPreset::C: return "c";
    case QualityPreset::B: return "b";
    case QualityPreset::A: return "a";
    case QualityPreset::S: return "s";
    case QualityPreset::Custom: return "custom";
    default: return "auto";
  }
}

const char *policyClampName(PolicyClamp clamp) {
  switch (clamp) {
    case PolicyClamp::Capability: return "capability";
    case PolicyClamp::Budget: return "budget";
    case PolicyClamp::Thermal: return "thermal";
    default: return "none";
  }
}

ResolvedRenderingPolicy resolveRenderingPolicy(const ProjectRenderingSettings &settings,
                                               const RenderingCapabilities &capabilities,
                                               ThermalPressure thermal) {
  ResolvedRenderingPolicy policy{};
  policy.effectiveProfile = profileForPreset(settings.preset, capabilities.profile);
  const PresetPoint point = presetForProfile(policy.effectiveProfile);

  const auto note = [&policy](const char *axis, PolicyClamp reason) {
    if (policy.clampCount >= MaximumPolicyClamps) return;
    policy.clamps[policy.clampCount++] = {axis, reason};
  };

  // --- eixos pedidos: preset, depois override explícito do autor --------------
  ShadowQuality shadows = inherited(settings.shadows, point.shadows);
  AmbientQuality ambient = inherited(settings.ambient, point.ambient);
  PostQuality post = inherited(settings.post, point.post);
  TextureQuality textures = inherited(settings.textures, point.textures);
  float resolutionScale = settings.resolutionScale > 0.0f ? settings.resolutionScale
                                                          : point.resolutionScale;

  // --- pressão térmica: piora imediata, um degrau por nível ------------------
  // Recuperação com histerese é responsabilidade de quem alimenta `thermal`, não
  // desta função: manter a resolução determinística é o que permite reproduzir
  // uma captura a partir das quatro entradas registradas.
  const u32 thermalSteps = thermal == ThermalPressure::Severe  ? 2u
                           : thermal == ThermalPressure::Light ? 1u
                                                               : 0u;
  for (u32 step = 0; step < thermalSteps; ++step) {
    if (shadows != ShadowQuality::Off) {
      shadows = degrade(shadows);
      note("shadows", PolicyClamp::Thermal);
    }
    if (post != PostQuality::None) {
      post = degrade(post);
      note("post", PolicyClamp::Thermal);
    }
    if (step > 0 && ambient != AmbientQuality::Constant) {
      // O ambiente só cede na pressão severa: perder o hemisfério achata a cena
      // inteira, e é uma degradação muito mais visível que perder bloom.
      ambient = degrade(ambient);
      note("ambient", PolicyClamp::Thermal);
    }
  }
  if (thermal == ThermalPressure::Severe) {
    const float reduced = std::min(resolutionScale, 0.75f);
    if (reduced < resolutionScale) {
      resolutionScale = reduced;
      note("resolutionScale", PolicyClamp::Thermal);
    }
  }

  // --- capability: pode reduzir, nunca elevar --------------------------------
  if (!capabilities.supportsDepthSampling && shadows != ShadowQuality::Off) {
    // Sem amostragem de profundidade não existe mapa de sombra legível. Desligar é
    // a única saída correta; produzir o passe assim mesmo daria sombra indefinida.
    shadows = ShadowQuality::Off;
    note("shadows", PolicyClamp::Capability);
  }
  const u32 maximumCascadeResolution =
      std::max(256u, std::min(capabilities.maximumImage2DSize, 4096u));
  ShadowSettings shadowSettings = deriveShadows(shadows, maximumCascadeResolution);
  if (shadowSettings.enabled) {
    // A linha do preset também pode escolher números dentro do mesmo algoritmo;
    // perfis não se resumem a três nomes de shader. Um override semântico
    // explícito (por exemplo, Ultra sobre perfil B) usa os defaults daquele eixo.
    if (settings.shadows == ShadowQuality::Inherit && thermalSteps == 0) {
      shadowSettings.cascadeCount = point.shadowCascadeCount;
      shadowSettings.cascadeResolution = point.shadowCascadeResolution;
      shadowSettings.filterTaps = point.shadowFilterTaps;
      shadowSettings.farFilterTaps = point.shadowFarFilterTaps;
      shadowSettings.maximumDistance = point.shadowMaximumDistance;
      shadowSettings.cacheGuardBandRatio = point.shadowCacheGuardBandRatio;
    }
    if (settings.shadowCascadeCount != 0)
      shadowSettings.cascadeCount = std::clamp(settings.shadowCascadeCount, 1u, 4u);
    if (settings.shadowCascadeResolution != 0)
      shadowSettings.cascadeResolution = std::clamp(settings.shadowCascadeResolution, 256u,
                                                     maximumCascadeResolution);
    if (settings.shadowFilterTaps != 0) {
      // O shader possui kernels quadrados 1x1, 3x3 e 5x5. Valores intermediários
      // são normalizados aqui, uma vez por época, nunca por fragmento.
      shadowSettings.filterTaps = normalizedShadowFilterTaps(settings.shadowFilterTaps);
    }
    if (settings.shadowFarFilterTaps != 0)
      shadowSettings.farFilterTaps = normalizedShadowFilterTaps(settings.shadowFarFilterTaps);
    shadowSettings.farFilterTaps = std::min(shadowSettings.farFilterTaps,
                                             shadowSettings.filterTaps);
    if (std::isfinite(settings.shadowMaximumDistance) && settings.shadowMaximumDistance > 0.0f)
      shadowSettings.maximumDistance = settings.shadowMaximumDistance;
    if (std::isfinite(settings.shadowDepthBiasConstant) && settings.shadowDepthBiasConstant >= 0.0f)
      shadowSettings.depthBiasConstant = settings.shadowDepthBiasConstant;
    if (std::isfinite(settings.shadowDepthBiasSlope) && settings.shadowDepthBiasSlope >= 0.0f)
      shadowSettings.depthBiasSlope = settings.shadowDepthBiasSlope;
    if (std::isfinite(settings.shadowNormalOffsetTexels) && settings.shadowNormalOffsetTexels >= 0.0f)
      shadowSettings.normalOffsetTexels = settings.shadowNormalOffsetTexels;
    shadowSettings.staticCasterCache =
        enabledOverride(settings.staticShadowCache, shadowSettings.staticCasterCache);
    if (std::isfinite(settings.shadowCacheGuardBandRatio) &&
        settings.shadowCacheGuardBandRatio >= 1.0f) {
      shadowSettings.cacheGuardBandRatio =
          std::clamp(settings.shadowCacheGuardBandRatio, 1.0f, 1.25f);
    }
    // O backend móvel atual empacota até quatro cascatas num atlas 2x2. Limitar
    // aqui preserva a mesma política auditável em vez de deixar vkCreateImage
    // falhar tarde ou criar uma tabela privada dentro do backend.
    const u32 atlasGrid = shadowSettings.cascadeCount > 1 ? 2u : 1u;
    const u32 atlasMaximumCascade = std::max(256u, capabilities.maximumImage2DSize / atlasGrid);
    if (shadowSettings.cascadeResolution > atlasMaximumCascade) {
      shadowSettings.cascadeResolution = atlasMaximumCascade;
      note("shadows.cascadeResolution", PolicyClamp::Capability);
    }
  }
  if (shadowSettings.enabled && shadowSettings.cascadeCount > capabilities.maximumImageArrayLayers) {
    shadowSettings.cascadeCount = std::max(1u, capabilities.maximumImageArrayLayers);
    note("shadows.cascadeCount", PolicyClamp::Capability);
  }
  if (shadowSettings.enabled &&
      deriveShadows(shadows, 4096u).cascadeResolution != shadowSettings.cascadeResolution) {
    note("shadows.cascadeResolution", PolicyClamp::Capability);
  }

  TextureSettings textureSettings = deriveTextures(textures, capabilities.maximumSamplerAnisotropy);
  if (textures == TextureQuality::Full && textureSettings.samplerAnisotropy < 8.0f) {
    note("textures.samplerAnisotropy", PolicyClamp::Capability);
  }

  // --- cadência: reaproveita a política já existente -------------------------
  const float requestedHz = settings.maximumRenderHz != 0
                                ? static_cast<float>(settings.maximumRenderHz)
                                : static_cast<float>(DefaultMaximumRenderHz);
  policy.frame = makeFrameBudget(capabilities.displayHz, requestedHz, 60);

  policy.visibility = {};
  policy.visibility.lodPixelErrorBudget =
      std::isfinite(settings.lodPixelErrorBudget) && settings.lodPixelErrorBudget > 0.0f
          ? std::clamp(settings.lodPixelErrorBudget, 0.25f, 16.0f)
          : point.lodPixelErrorBudget;
  policy.visibility.coverageLodPixelErrorBudget =
      std::isfinite(settings.coverageLodPixelErrorBudget) &&
              settings.coverageLodPixelErrorBudget > 0.0f
          ? std::clamp(settings.coverageLodPixelErrorBudget, 0.25f, 128.0f)
          : point.coverageLodPixelErrorBudget;
  policy.visibility.lodHysteresisBandRatio =
      std::isfinite(settings.lodHysteresisBandRatio) && settings.lodHysteresisBandRatio > 0.0f
          ? std::clamp(settings.lodHysteresisBandRatio, 0.1f, 0.99f)
          : point.lodHysteresisBandRatio;
  policy.shadows = shadowSettings;
  policy.ambient = deriveAmbient(ambient);
  policy.ambient.splitSumBrdf = policy.ambient.specularProbe &&
      enabledOverride(settings.environmentSplitSumBrdf, policy.ambient.splitSumBrdf);
  policy.post = derivePost(post);
  policy.post.fxaa = enabledOverride(settings.postFxaa, policy.post.fxaa);
  policy.post.vignette = enabledOverride(settings.postVignette, policy.post.vignette);
  if (std::isfinite(settings.bloomThreshold) && settings.bloomThreshold >= 0.0f)
    policy.post.bloomThreshold = std::clamp(settings.bloomThreshold, 0.0f, 8.0f);
  if (std::isfinite(settings.bloomIntensity) && settings.bloomIntensity >= 0.0f)
    policy.post.bloomIntensity = std::clamp(settings.bloomIntensity, 0.0f, 2.0f);
  if (std::isfinite(settings.postContrast) && settings.postContrast >= 0.0f)
    policy.post.contrast = std::clamp(settings.postContrast, 0.5f, 2.0f);
  if (std::isfinite(settings.postSaturation) && settings.postSaturation >= 0.0f)
    policy.post.saturation = std::clamp(settings.postSaturation, 0.0f, 2.0f);
  if (std::isfinite(settings.postSharpen) && settings.postSharpen >= 0.0f)
    policy.post.sharpen = std::clamp(settings.postSharpen, 0.0f, 1.0f);
  // Qualquer filtro solicitado exige o passe, mesmo quando o preset base usava
  // tonemap inline. Isto mantém cada eixo independente de nome de preset.
  if (policy.post.bloom || policy.post.fxaa || policy.post.vignette ||
      policy.post.sharpen > 0.0f || policy.post.contrast != 1.0f ||
      policy.post.saturation != 1.0f) {
    policy.post.dedicatedPass = true;
  }
  policy.textures = textureSettings;
  policy.materialDistance.normalMapMaximumDistance =
      std::isfinite(settings.normalMapMaximumDistance) && settings.normalMapMaximumDistance >= 0.0f
          ? settings.normalMapMaximumDistance : point.normalMapMaximumDistance;
  policy.materialDistance.specularProbeMaximumDistance =
      std::isfinite(settings.specularProbeMaximumDistance) && settings.specularProbeMaximumDistance >= 0.0f
          ? settings.specularProbeMaximumDistance : point.specularProbeMaximumDistance;
  policy.materialDistance.metallicRoughnessMaximumDistance =
      std::isfinite(settings.metallicRoughnessMaximumDistance) &&
              settings.metallicRoughnessMaximumDistance >= 0.0f
          ? settings.metallicRoughnessMaximumDistance : point.metallicRoughnessMaximumDistance;
  policy.materialDistance.emissiveMaximumDistance =
      std::isfinite(settings.emissiveMaximumDistance) && settings.emissiveMaximumDistance >= 0.0f
          ? settings.emissiveMaximumDistance : point.emissiveMaximumDistance;
  policy.materialDistance.fadeBandRatio =
      std::isfinite(settings.materialDetailFadeBandRatio) &&
              settings.materialDetailFadeBandRatio >= 0.0f
          ? std::clamp(settings.materialDetailFadeBandRatio, 0.0f, 0.5f)
          : 0.20f;
  policy.geometry.lodSelection = enabledOverride(settings.lodSelection, true);
  policy.geometry.materialShaderVariants =
      enabledOverride(settings.materialShaderVariants, point.materialShaderVariants);
  policy.resolutionScale = std::clamp(resolutionScale, 0.5f, 1.0f);
  // --- cadência alta: o preset foi autorado para 60 Hz ------------------------
  //
  // Os pontos de preset acima descrevem o que o perfil sustenta em 60 Hz. Pedir
  // 120 Hz corta o orçamento pela metade sem mudar uma linha do preset, e o
  // resultado medido no perfil B é o app perder a cadência em silêncio: a pose
  // do hotspot da floresta mede 84,7 FPS em escala nativa e 97,6 FPS com o piso
  // de 0,58 que o preset traz. Com o piso em 0,50 -- o limite do controlador --
  // a mesma pose mede 111,1 FPS. A folga não estava faltando, estava proibida
  // por um número autorado para outra cadência.
  //
  // Então acima de 60 Hz a resolução dinâmica deixa de ser opcional e o piso é
  // interpolado entre o valor do preset (em 60 Hz) e o limite do controlador
  // (em 120 Hz). Continua sendo política, não heurística de runtime: o autor que
  // define os campos explicitamente continua mandando, e o motivo entra em
  // `clamps` para aparecer no relatório de perfil em vez de virar mágica.
  const bool highCadence = policy.frame.renderHz > 60;
  const bool dynamicResolutionByCadence =
      highCadence && settings.dynamicResolution == FeatureOverride::Inherit;
  policy.dynamicResolution.enabled =
      enabledOverride(settings.dynamicResolution, point.dynamicResolution || highCadence);
  policy.dynamicResolution.maximumScale = policy.resolutionScale;
  float requestedMinimumScale =
      std::isfinite(settings.dynamicResolutionMinimumScale) &&
              settings.dynamicResolutionMinimumScale > 0.0f
          ? settings.dynamicResolutionMinimumScale : point.dynamicResolutionMinimumScale;
  bool minimumScaleByCadence = false;
  if (highCadence && !(std::isfinite(settings.dynamicResolutionMinimumScale) &&
                       settings.dynamicResolutionMinimumScale > 0.0f)) {
    // 60 Hz mantém o piso do preset; 120 Hz chega ao limite do controlador.
    // Entre os dois, interpolação linear na cadência -- 90 Hz fica no meio.
    const float cadenceRatio =
        std::clamp((static_cast<float>(policy.frame.renderHz) - 60.0f) / 60.0f, 0.0f, 1.0f);
    const float cadenceMinimumScale =
        requestedMinimumScale + (DynamicResolutionFloor - requestedMinimumScale) * cadenceRatio;
    minimumScaleByCadence = cadenceMinimumScale < requestedMinimumScale - 1.0e-4f;
    requestedMinimumScale = cadenceMinimumScale;
  }
  policy.dynamicResolution.minimumScale = std::clamp(
      requestedMinimumScale, DynamicResolutionFloor, policy.dynamicResolution.maximumScale);
  if (dynamicResolutionByCadence && policy.dynamicResolution.enabled && !point.dynamicResolution)
    note("dynamicResolution.enabled", PolicyClamp::Budget);
  if (minimumScaleByCadence) note("dynamicResolution.minimumScale", PolicyClamp::Budget);
  if (std::isfinite(settings.dynamicResolutionDecreaseStep) &&
      settings.dynamicResolutionDecreaseStep > 0.0f)
    policy.dynamicResolution.decreaseStep =
        std::clamp(settings.dynamicResolutionDecreaseStep, 0.01f, 0.25f);
  if (std::isfinite(settings.dynamicResolutionIncreaseStep) &&
      settings.dynamicResolutionIncreaseStep > 0.0f)
    policy.dynamicResolution.increaseStep =
        std::clamp(settings.dynamicResolutionIncreaseStep, 0.005f, 0.25f);
  if (std::isfinite(settings.dynamicResolutionRecoveryHeadroomRatio) &&
      settings.dynamicResolutionRecoveryHeadroomRatio > 0.0f)
    policy.dynamicResolution.recoveryHeadroomRatio =
        std::clamp(settings.dynamicResolutionRecoveryHeadroomRatio, 0.5f, 0.95f);
  if (settings.dynamicResolutionOverloadFrames != 0)
    policy.dynamicResolution.overloadFrames =
        std::clamp(settings.dynamicResolutionOverloadFrames, 1u, 240u);
  if (settings.dynamicResolutionRecoveryFrames != 0)
    policy.dynamicResolution.recoveryFrames =
        std::clamp(settings.dynamicResolutionRecoveryFrames, 1u, 1200u);
  // Escala menor que a swapchain precisa de um resolve/upscale; portanto o
  // passe deixa de ser opcional mesmo se todos os filtros estiverem desligados.
  if (policy.resolutionScale < 0.999f ||
      (policy.dynamicResolution.enabled &&
       policy.dynamicResolution.minimumScale < policy.dynamicResolution.maximumScale - 1.0e-4f))
    policy.post.dedicatedPass = true;
  return policy;
}

} // namespace ae::renderer
