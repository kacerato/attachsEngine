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
};

constexpr PresetPoint presetForProfile(rhi::DeviceProfile profile) {
  switch (profile) {
    case rhi::DeviceProfile::S:
      return {ShadowQuality::UltraSoft, AmbientQuality::HemisphericSpecular, PostQuality::Bloom,
              TextureQuality::Full, 1.0f};
    case rhi::DeviceProfile::A:
      return {ShadowQuality::Soft, AmbientQuality::HemisphericSpecular, PostQuality::Bloom,
              TextureQuality::Full, 1.0f};
    case rhi::DeviceProfile::B:
      return {ShadowQuality::Soft, AmbientQuality::Hemispheric, PostQuality::Tonemap,
              TextureQuality::Full, 1.0f};
    case rhi::DeviceProfile::C:
    default:
      // O perfil base não perde direcionalidade do sol: mantém uma cascata dura,
      // que é o que separa "sombra barata" de "sem noção de oclusão".
      return {ShadowQuality::Hard, AmbientQuality::Hemispheric, PostQuality::None,
              TextureQuality::Half, 0.85f};
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
      settings = {true, 4, 2048, 25, 320.0f, 1.25f, 2.0f, 2.5f, true};
      break;
    case ShadowQuality::Soft:
      settings = {true, 3, 1536, 9, 220.0f, 1.5f, 2.25f, 2.0f, true};
      break;
    case ShadowQuality::Hard:
      // Uma cascata sem filtro ainda dá ao sol uma direção: objetos passam a
      // ocluir uns aos outros. O bias sobe porque, sem PCF, o acne aparece antes.
      settings = {true, 1, 1024, 1, 120.0f, 2.0f, 3.0f, 1.0f, true};
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
    case AmbientQuality::HemisphericSpecular: return {true, true};
    case AmbientQuality::Hemispheric: return {true, false};
    default: return {false, false};
  }
}

PostSettings derivePost(PostQuality quality) {
  switch (quality) {
    case PostQuality::Bloom: return {true, true, 1.15f, 0.35f};
    case PostQuality::Tonemap: return {true, false, 1.0f, 0.0f};
    default: return {};
  }
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
  policy.shadows = shadowSettings;
  policy.ambient = deriveAmbient(ambient);
  policy.post = derivePost(post);
  policy.textures = textureSettings;
  policy.resolutionScale = std::clamp(resolutionScale, 0.5f, 1.0f);
  return policy;
}

} // namespace ae::renderer
