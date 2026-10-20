#include "runtime/scene_weather.h"
#include "runtime/transform_math.h"
#include "scene/environment.h"
#include "scene/particle_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::runtime {
namespace {
float lerp(float a, float b, float t) { return a + (b - a) * t; }
u32 hash(u32 x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
float unit(u32 x) { return static_cast<float>(hash(x) >> 8) * (1.0f / 16777216.0f); }

// Escreve só quando muda: a revisão do grafo é o sinal de "quadro novo" para
// partículas, ambiente e cache de cena.
template <class T> bool assign(T &target, T value, bool &changed) {
  if (target == value) return false;
  target = value;
  changed = true;
  return true;
}

bool applyEnvironment(SceneGraph &graph, ObjectId id, const scene::Weather &weather, const WeatherLook &look,
                      float flash) {
  const auto *object = graph.find(id);
  if (!object) return false;
  const auto *current = static_cast<const scene::Environment *>(object->components.find(scene::Environment::descriptor));
  if (!current) return false;
  scene::Environment next = *current;
  bool changed = false;
  auto &clouds = next.values.clouds;
  assign(next.overrideClouds, true, changed);
  assign(clouds.enabled, true, changed);
  assign(clouds.opacity, 1.0f, changed);
  assign(clouds.coverage, std::clamp(look.coverage, 0.0f, 1.0f), changed);
  assign(clouds.cirrus, std::clamp(look.cirrus, 0.0f, 1.0f), changed);
  assign(clouds.density, std::clamp(look.density, 0.0f, 1.0f), changed);
  for (u32 c = 0; c < 3; ++c) assign(clouds.tint[c], std::clamp(look.tint, 0.0f, 1.0f), changed);
  // O clarão acende as nuvens por dentro (Exposure Compensation) e a neblina.
  const float flashEv = flash * weather.lightningBrightness;
  assign(clouds.exposureEv, std::clamp(flashEv, -8.0f, 8.0f), changed);
  if (next.overrideFog) {
    assign(next.values.fog, true, changed);
    assign(next.values.fogDensity, weather.fogDensity * std::clamp(look.fog, 0.0f, 1.0f), changed);
    assign(next.values.fogLightEnergy, 1.0f + flash * (std::exp2(weather.lightningBrightness) - 1.0f) * 0.35f, changed);
  }
  if (!changed || !next.valid()) return false;
  auto *components = graph.editComponents(id);
  auto *target = components ? components->edit(scene::Environment::descriptor) : nullptr;
  if (!target) return false;
  static_cast<scene::Environment &>(*target) = next;
  return true;
}

// Taxa de emissão do sistema: a constante (modos Constante e Entre duas
// constantes) ou o multiplicador da curva -- o rateOverTimeMultiplier da Unity.
bool applyEmission(SceneGraph &graph, ObjectId id, float rate) {
  const auto *object = graph.find(id);
  if (!object) return false;
  const auto *found = object->components.find(scene::ParticleSystem::descriptor);
  if (!found) return false;
  const auto &current = scene::particleSystem(*found);
  scene::ParticleMinMaxCurve next = current.emissionRate;
  rate = std::clamp(rate, 0.0f, 100000.0f);
  bool changed = false;
  if (next.usesCurves()) assign(next.multiplier, rate, changed);
  else {
    assign(next.constantMax, rate, changed);
    if (next.mode == scene::ParticleCurveMode::TwoConstants) assign(next.constantMin, rate * 0.8f, changed);
  }
  if (!changed) return false;
  auto *components = graph.editComponents(id);
  auto *target = components ? components->edit(scene::ParticleSystem::descriptor) : nullptr;
  if (!target) return false;
  scene::particleSystem(*target).emissionRate = next;
  return true;
}

bool followCamera(const SceneGraph &graph, ObjectId id, const float camera[3], float height,
                  const std::function<bool(ObjectId, const Transform &)> &write) {
  const auto *object = graph.find(id);
  float world[16], parent[16];
  if (!object || !worldMatrix(graph, id, world) || !parentWorldMatrix(graph, id, parent)) return false;
  const float target[3]{camera[0], camera[1] + height, camera[2]};
  if (std::abs(world[12] - target[0]) < 1e-3f && std::abs(world[13] - target[1]) < 1e-3f &&
      std::abs(world[14] - target[2]) < 1e-3f)
    return false;
  for (u32 axis = 0; axis < 3; ++axis) world[12 + axis] = target[axis];
  Transform local{};
  if (!localTransformForWorld(world, parent, local)) return false;
  // Só a posição: rotação e escala autorais ficam como estão.
  Transform next = object->transform;
  for (u32 axis = 0; axis < 3; ++axis) next.position[axis] = local.position[axis];
  return write(id, next);
}
} // namespace

WeatherLook weatherTarget(scene::WeatherKind kind, float intensity) {
  const float i = std::clamp(intensity, 0.0f, 1.0f);
  WeatherLook look;
  switch (kind) {
  case scene::WeatherKind::Clear:
    look.coverage = lerp(0.2f, 0.05f, i);
    look.cirrus = 0.2f;
    look.density = 0.4f;
    look.tint = 1.0f;
    look.fog = 0.1f;
    break;
  case scene::WeatherKind::Cloudy:
    look.coverage = lerp(0.45f, 0.85f, i);
    look.cirrus = 0.4f;
    look.density = lerp(0.5f, 0.65f, i);
    look.tint = lerp(0.95f, 0.85f, i);
    look.fog = lerp(0.15f, 0.3f, i);
    break;
  case scene::WeatherKind::Rain:
    look.coverage = lerp(0.75f, 0.98f, i);
    look.cirrus = 0.5f;
    look.density = lerp(0.7f, 0.9f, i);
    look.tint = lerp(0.78f, 0.55f, i);
    look.rain = i;
    look.fog = lerp(0.3f, 0.7f, i);
    break;
  case scene::WeatherKind::Storm:
    look.coverage = lerp(0.92f, 1.0f, i);
    look.cirrus = 0.6f;
    look.density = lerp(0.9f, 1.0f, i);
    look.tint = lerp(0.5f, 0.35f, i);
    look.rain = lerp(0.5f, 1.0f, i);
    look.fog = lerp(0.5f, 0.9f, i);
    look.storm = 1.0f;
    break;
  case scene::WeatherKind::Snow:
    look.coverage = lerp(0.8f, 0.98f, i);
    look.cirrus = 0.5f;
    look.density = lerp(0.55f, 0.7f, i);
    look.tint = lerp(0.95f, 0.85f, i);
    look.snow = i;
    look.fog = lerp(0.4f, 1.0f, i);
    break;
  }
  return look;
}

float lightningFlash(u32 seed, double seconds, float interval) {
  if (!(interval > 0) || seconds < 0) return 0;
  const auto window = static_cast<u32>(seconds / interval);
  float flash = 0;
  // A janela atual e a anterior: um raio no fim da janela passada ainda brilha.
  for (u32 k = window > 0 ? window - 1 : 0; k <= window; ++k) {
    const u32 key = hash(seed ^ (k * 0x9e3779b9U));
    if (unit(key) > 0.85f) continue;  // algumas janelas sem raio
    const double strike = (static_cast<double>(k) + 0.15 + 0.7 * unit(key + 1)) * interval;
    const double t = seconds - strike;
    if (t < 0 || t > 1.0) continue;
    // Raio principal e um retorno 0,18 s depois, como o pisca-pisca real.
    float value = static_cast<float>(std::exp(-t / 0.07));
    if (t > 0.18) value = std::max(value, 0.6f * static_cast<float>(std::exp(-(t - 0.18) / 0.09)));
    flash = std::max(flash, value);
  }
  return flash;
}

SceneWeather::Report SceneWeather::advance(SceneGraph &graph, double seconds, float deltaSeconds, bool live,
                                           const float *camera,
                                           const std::function<bool(ObjectId, const Transform &)> &writeTransform) {
  Report report;
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (auto &[key, state] : states_) state.initialized = state.initialized && graph.exists(key.first);
  for (ObjectId id : ids) {
    const auto *object = graph.find(id);
    if (!object || !graph.activeInHierarchy(id)) continue;
    const auto *found = object->components.find(scene::Weather::descriptor);
    if (!found) continue;
    const auto weather = static_cast<const scene::Weather &>(*found);  // cópia: o grafo muda abaixo
    if (!weather.enabled || !weather.valid()) continue;
    ++report.weathers;
    auto &state = states_[{id, found->instanceId()}];
    const auto target = weatherTarget(weather.kind, weather.intensity);
    if (!live || !state.initialized || weather.transitionSeconds <= 0) state.look = target;
    else {
      // Aproximação exponencial: ~95 % do caminho em Transição segundos.
      const float k = 1.0f - std::exp(-3.0f * std::max(deltaSeconds, 0.0f) / weather.transitionSeconds);
      auto &l = state.look;
      l.coverage = lerp(l.coverage, target.coverage, k);
      l.cirrus = lerp(l.cirrus, target.cirrus, k);
      l.density = lerp(l.density, target.density, k);
      l.tint = lerp(l.tint, target.tint, k);
      l.rain = lerp(l.rain, target.rain, k);
      l.snow = lerp(l.snow, target.snow, k);
      l.fog = lerp(l.fog, target.fog, k);
      l.storm = lerp(l.storm, target.storm, k);
      // Sem resíduo eterno: a emissão chega a zero de verdade.
      if (std::abs(l.rain - target.rain) < 1e-3f) l.rain = target.rain;
      if (std::abs(l.snow - target.snow) < 1e-3f) l.snow = target.snow;
    }
    state.initialized = true;
    float flash = 0;
    if (live && weather.lightning && state.look.storm > 0.5f)
      flash = lightningFlash(static_cast<u32>(id) * 2654435761U, seconds, weather.lightningInterval) * state.look.storm;
    report.flash = std::max(report.flash, flash);
    report.writes += applyEnvironment(graph, id, weather, state.look, flash);
    const std::pair<u64, float> systems[]{{weather.rain, state.look.rain * weather.rainRate},
                                          {weather.snow, state.look.snow * weather.snowRate}};
    for (const auto &[system, rate] : systems) {
      const auto target = static_cast<ObjectId>(system);
      if (!system || !graph.exists(target)) continue;
      report.writes += applyEmission(graph, target, rate);
      if (weather.followCamera && camera)
        report.writes += followCamera(graph, target, camera, weather.heightAboveCamera, writeTransform);
    }
  }
  return report;
}

bool SceneWeather::current(ObjectId object, u64 instance, WeatherLook &out) const {
  const auto it = states_.find({object, instance});
  if (it == states_.end() || !it->second.initialized) return false;
  out = it->second.look;
  return true;
}

} // namespace ae::runtime
