#include "resources/environment_looks.h"

#include <array>
#include <string>

namespace ae::resources {
namespace {

constexpr std::array<EnvironmentLookPreset, static_cast<usize>(EnvironmentLook::Count)> kPresets{{
  {EnvironmentLook::Neutral, "neutral", "Neutro"},
  {EnvironmentLook::Cinematic, "cinematic", "Cinemático"},
  {EnvironmentLook::WarmDay, "warm_day", "Dia quente"},
  {EnvironmentLook::Sunset, "sunset", "Entardecer"},
  {EnvironmentLook::BlueNight, "blue_night", "Noite azul"},
  {EnvironmentLook::Storm, "storm", "Tempestade"},
  {EnvironmentLook::Fantasy, "fantasy", "Fantasia"},
}};

void color(float target[3], float r, float g, float b) { target[0] = r;target[1] = g;target[2] = b; }

// Só o grupo Pós. Valores pensados para imagem já mapeada (depois do tonemap):
// deslocamentos pequenos, cores de trackball perto do branco.
renderer::SceneEnvironment lookValues(EnvironmentLook look) {
  renderer::SceneEnvironment v{};
  v.post = true;
  auto &g = v.grading;
  switch (look) {
  case EnvironmentLook::Neutral:
  case EnvironmentLook::Count: break;
  case EnvironmentLook::Cinematic:
    // Laranja e azul-petróleo: realces quentes, sombras frias, preto firme.
    v.contrast = 1.08f;v.saturation = 0.94f;
    v.bloomIntensity = 0.14f;
    v.vignette = true;v.vignetteIntensity = 0.28f;
    v.filmGrain = true;v.filmGrainIntensity = 0.05f;
    color(g.shadowsColor, 0.86f, 0.96f, 1.0f);
    color(g.highlightsColor, 1.0f, 0.94f, 0.84f);
    g.lift = -0.05f;g.vibrance = 0.10f;
    break;
  case EnvironmentLook::WarmDay:
    v.contrast = 1.04f;v.saturation = 1.06f;
    v.bloomIntensity = 0.12f;
    g.temperature = 22.0f;g.tint = 3.0f;
    color(g.highlightsColor, 1.0f, 0.97f, 0.90f);
    g.gain = 0.05f;g.vibrance = 0.20f;
    break;
  case EnvironmentLook::Sunset:
    v.contrast = 1.06f;v.saturation = 1.10f;
    v.bloomThreshold = 0.9f;v.bloomIntensity = 0.22f;
    v.vignette = true;v.vignetteIntensity = 0.22f;
    g.temperature = 45.0f;g.tint = 8.0f;
    color(g.shadowsColor, 0.90f, 0.86f, 1.0f);
    color(g.highlightsColor, 1.0f, 0.86f, 0.70f);
    g.highlights = 0.05f;g.vibrance = 0.15f;
    break;
  case EnvironmentLook::BlueNight:
    v.contrast = 1.05f;v.saturation = 0.78f;
    v.bloomThreshold = 0.8f;v.bloomIntensity = 0.20f;
    v.vignette = true;v.vignetteIntensity = 0.35f;
    v.filmGrain = true;v.filmGrainIntensity = 0.08f;
    g.temperature = -40.0f;g.tint = -5.0f;
    color(g.shadowsColor, 0.76f, 0.86f, 1.0f);
    color(g.midtonesColor, 0.86f, 0.93f, 1.0f);
    g.gain = -0.15f;g.gamma = -0.08f;
    break;
  case EnvironmentLook::Storm:
    v.contrast = 1.12f;v.saturation = 0.62f;
    v.bloomIntensity = 0.06f;
    v.vignette = true;v.vignetteIntensity = 0.30f;
    v.filmGrain = true;v.filmGrainIntensity = 0.10f;
    g.temperature = -12.0f;
    color(g.shadowsColor, 0.88f, 0.94f, 0.96f);
    color(g.highlightsColor, 0.95f, 0.97f, 1.0f);
    g.gain = -0.10f;g.lift = -0.03f;g.vibrance = -0.20f;
    break;
  case EnvironmentLook::Fantasy:
    v.contrast = 0.98f;v.saturation = 1.18f;
    v.bloomThreshold = 0.75f;v.bloomIntensity = 0.30f;
    v.vignette = true;v.vignetteIntensity = 0.18f;
    g.temperature = 8.0f;g.tint = 18.0f;
    color(g.shadowsColor, 0.90f, 0.82f, 1.0f);
    color(g.highlightsColor, 1.0f, 0.95f, 0.88f);
    g.gamma = 0.06f;g.vibrance = 0.35f;
    break;
  }
  return v;
}

void copyPost(renderer::SceneEnvironment &target, const renderer::SceneEnvironment &source) {
  target.post = source.post;
  target.toneMapper = source.toneMapper;
  target.bloomThreshold = source.bloomThreshold;target.bloomIntensity = source.bloomIntensity;
  target.vignetteIntensity = source.vignetteIntensity;
  target.filmGrainIntensity = source.filmGrainIntensity;
  target.contrast = source.contrast;target.saturation = source.saturation;
  target.grading = source.grading;
  target.syncEffectSwitches();
}

bool samePost(const renderer::SceneEnvironment &a, const renderer::SceneEnvironment &b) {
  // PK-5: o efeito liga pela intensidade (Unity); limiar só conta com bloom.
  return a.post == b.post && a.toneMapper == b.toneMapper && a.bloomIntensity == b.bloomIntensity &&
         (a.bloomIntensity <= 0.0f || a.bloomThreshold == b.bloomThreshold) &&
         a.vignetteIntensity == b.vignetteIntensity && a.filmGrainIntensity == b.filmGrainIntensity &&
         a.contrast == b.contrast && a.saturation == b.saturation && a.grading == b.grading;
}

} // namespace

std::span<const EnvironmentLookPreset> environmentLookPresets() noexcept { return kPresets; }

const EnvironmentLookPreset *findEnvironmentLook(std::string_view key) noexcept {
  for (const auto &preset : kPresets) if (preset.key == key) return &preset;
  return nullptr;
}

void applyEnvironmentLook(renderer::SceneEnvironment &target, EnvironmentLook look) noexcept {
  if (look >= EnvironmentLook::Count) return;
  copyPost(target, lookValues(look));
}

std::optional<EnvironmentLook> matchEnvironmentLook(const renderer::SceneEnvironment &environment) noexcept {
  for (const auto &preset : kPresets)
    if (samePost(environment, lookValues(preset.look))) return preset.look;
  return std::nullopt;
}

EnvironmentProfile environmentLookProfile(EnvironmentLook look) {
  EnvironmentProfile profile;
  const auto index = static_cast<usize>(look) < kPresets.size() ? static_cast<usize>(look) : 0u;
  profile.guid = assetGuidFromSeed(std::string("astra.environment.look.") + std::string(kPresets[index].key));
  profile.name = std::string(kPresets[index].name);
  profile.values = lookValues(kPresets[index].look);
  profile.values.active = true;
  return profile;
}

} // namespace ae::resources
