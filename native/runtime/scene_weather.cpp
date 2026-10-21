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
// Ruído suave em 0..1 ao longo do tempo (duas oitavas de ruído de valor com
// interpolação suave): pancadas e calmarias sem repetição perceptível.
float gusts(u32 seed, double t) {
  float sum = 0, weight = 0, amplitude = 1;
  for (u32 octave = 0; octave < 2; ++octave) {
    const double x = t * static_cast<double>(1u << octave);
    const auto cell = static_cast<i64>(std::floor(x));
    const float f = static_cast<float>(x - std::floor(x)), s = f * f * (3 - 2 * f);
    const u32 key = seed ^ (octave * 0x85ebca6bU);
    const float a = unit(key ^ static_cast<u32>(cell * 0x27d4eb2d)), b = unit(key ^ static_cast<u32>((cell + 1) * 0x27d4eb2d));
    sum += amplitude * (a + (b - a) * s);
    weight += amplitude;
    amplitude *= 0.5f;
  }
  return sum / weight;
}

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
  // Tempo fechado: a sombra das nuvens chega ao máximo (o sol quase não passa).
  const float overcast = std::clamp((look.coverage - 0.5f) / 0.4f, 0.0f, 1.0f);
  assign(clouds.shadowMultiplier, lerp(0.8f, 1.0f, overcast), changed);
  // O clarão acende as nuvens por dentro (Exposure Compensation) e a neblina.
  const float flashEv = flash * weather.lightningBrightness;
  assign(clouds.exposureEv, std::clamp(flashEv, -8.0f, 8.0f), changed);
  if (next.overrideFog) {
    assign(next.values.fog, true, changed);
    assign(next.values.fogDensity, weather.fogDensity * std::clamp(look.fog, 0.0f, 1.0f), changed);
    // A névoa escurece com o céu (chuva e tempestade mais escuras, como os
    // frames da Pixagon) e acende no clarão do relâmpago.
    const float shade = lerp(1.0f, std::clamp(look.tint, 0.0f, 1.0f), 0.8f);
    assign(next.values.fogLightEnergy, shade + flash * (std::exp2(weather.lightningBrightness) - 1.0f) * 0.35f, changed);
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
// O vento entra pela Velocidade no tempo em espaço Mundo: a gota cai inclinada
// e o traço esticado mostra a inclinação, como o vento de um script da Unity.
bool applyPrecipitation(SceneGraph &graph, ObjectId id, float rate, const float (&wind)[2]) {
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
  const bool windy = std::abs(wind[0]) > 1e-4f || std::abs(wind[1]) > 1e-4f;
  bool velocityEnabled = current.velocityOverLifetimeEnabled;
  auto space = current.velocitySpace;
  auto vx = current.velocityX, vz = current.velocityZ;
  if (windy || current.velocityOverLifetimeEnabled) {
    assign(velocityEnabled, true, changed);
    assign(space, scene::ParticleSimulationSpace::World, changed);
    assign(vx.mode, scene::ParticleCurveMode::Constant, changed);
    assign(vz.mode, scene::ParticleCurveMode::Constant, changed);
    assign(vx.constantMax, wind[0], changed);
    assign(vz.constantMax, wind[1], changed);
  }
  if (!changed) return false;
  auto *components = graph.editComponents(id);
  auto *target = components ? components->edit(scene::ParticleSystem::descriptor) : nullptr;
  if (!target) return false;
  auto &system = scene::particleSystem(*target);
  system.emissionRate = next;
  system.velocityOverLifetimeEnabled = velocityEnabled;
  system.velocitySpace = space;
  system.velocityX = vx;
  system.velocityZ = vz;
  return true;
}

// O emissor fica acima da câmera, um pouco À FRENTE dela (a caixa não gasta
// partículas atrás de quem olha) e CONTRA o vento por meia vida da partícula:
// assim a gota ou o floco arrastado pelo vento chega na frente da câmera, e não
// só de um lado da tela.
bool followCamera(const SceneGraph &graph, ObjectId id, const WeatherCamera &camera, float height, const float (&wind)[2],
                  const std::function<bool(ObjectId, const Transform &)> &write) {
  const auto *object = graph.find(id);
  float world[16], parent[16];
  if (!object || !worldMatrix(graph, id, world) || !parentWorldMatrix(graph, id, parent)) return false;
  float halfLife = 0;
  if (const auto *found = object->components.find(scene::ParticleSystem::descriptor))
    halfLife = 0.5f * std::max(0.0f, scene::particleSystem(*found).startLifetime.constantMax);
  const float ahead = height * 0.6f;
  const float target[3]{camera.position[0] + std::sin(camera.yaw) * ahead - wind[0] * halfLife, camera.position[1] + height,
                        camera.position[2] + std::cos(camera.yaw) * ahead - wind[1] * halfLife};
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
    look.coverage = lerp(0.45f, 0.72f, i);
    look.cirrus = 0.4f;
    look.density = lerp(0.6f, 0.75f, i);
    look.tint = lerp(0.95f, 0.85f, i);
    look.fog = lerp(0.15f, 0.3f, i);
    break;
  case scene::WeatherKind::Rain:
    look.coverage = lerp(0.7f, 0.88f, i);
    look.cirrus = 0.45f;
    look.density = lerp(0.85f, 0.95f, i);
    look.tint = lerp(0.8f, 0.6f, i);
    look.rain = i;
    look.fog = lerp(0.3f, 0.7f, i);
    break;
  case scene::WeatherKind::Storm:
    look.coverage = lerp(0.85f, 0.95f, i);
    look.cirrus = 0.55f;
    look.density = lerp(0.95f, 1.0f, i);
    look.tint = lerp(0.55f, 0.4f, i);
    look.rain = lerp(0.5f, 1.0f, i);
    look.fog = lerp(0.5f, 0.9f, i);
    look.storm = 1.0f;
    break;
  case scene::WeatherKind::Snow:
    look.coverage = lerp(0.68f, 0.85f, i);
    look.cirrus = 0.5f;
    look.density = lerp(0.65f, 0.75f, i);
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
                                           const WeatherCamera *camera,
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
    // Variação natural (só em Play): a intensidade oscila devagar abaixo da
    // autoral, e a transição suaviza a passagem como nas trocas de tipo.
    float intensity = weather.intensity;
    if (live && weather.variation > 0)
      intensity *= 1.0f - weather.variation * gusts(static_cast<u32>(id) * 0x9e3779b1U, seconds / std::max(5.0f, weather.variationPeriod));
    const auto target = weatherTarget(weather.kind, intensity);
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
    // Vento do Cloud Layer deste Ambiente (0° sopra para +X, como as nuvens).
    float wind[2]{0, 0};
    if (const auto *env = static_cast<const scene::Environment *>(object->components.find(scene::Environment::descriptor))) {
      const float radians = env->values.clouds.windOrientationDegrees * 0.017453292f;
      const float speed = env->values.clouds.windSpeedKmh / 3.6f * weather.windInfluence;
      wind[0] = std::cos(radians) * speed;
      wind[1] = std::sin(radians) * speed;
    }
    // A neve é leve: o mesmo vento a arrasta mais que a gota.
    const float snowWind[2]{wind[0] * 1.6f, wind[1] * 1.6f};
    const struct { u64 system; float rate; const float (&wind)[2]; } systems[]{
        {weather.rain, state.look.rain * weather.rainRate, wind},
        {weather.snow, state.look.snow * weather.snowRate, snowWind}};
    for (const auto &entry : systems) {
      const auto target = static_cast<ObjectId>(entry.system);
      if (!entry.system || !graph.exists(target)) continue;
      report.writes += applyPrecipitation(graph, target, entry.rate, entry.wind);
      if (weather.followCamera && camera)
        report.writes += followCamera(graph, target, *camera, weather.heightAboveCamera, entry.wind, writeTransform);
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
