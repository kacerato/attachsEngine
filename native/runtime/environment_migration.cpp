#include "runtime/environment_migration.h"

#include "scene/camera.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "scene/lighting_settings.h"
#include "scene/time_of_day.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {
namespace {
// O sol de referência do ambiente de autoria (dirt_road_resources.cpp,
// initializePrimitives): 2 unidades de shader, multiplicado por "Sol potência ×".
constexpr float kLegacyReferenceSun = 2.0f;

struct SunCandidate {
  ObjectId id = 0;
  float intensity = 0.0f;
  bool photometric = false;
  bool timeOfDay = false;
};
}

EnvironmentMigrationReport migrateLegacyEnvironmentLighting(SceneGraph &graph) {
  EnvironmentMigrationReport report;
  const auto *root = graph.find(graph.root());
  if (!root) return report;
  const float multipliers[4]{root->environment[0], root->environment[1], root->environment[2], root->environment[3]};
  report.rootMultipliers = multipliers[0] != 1.0f || multipliers[1] != 1.0f || multipliers[2] != 1.0f ||
                           multipliers[3] != 0.0f;
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);

  bool legacy = false;
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    const auto *environment = object ? static_cast<const scene::Environment *>(
                                           object->components.find(scene::Environment::descriptor)) : nullptr;
    legacy |= environment && environment->loadedVersion < scene::Environment::CurrentVersion;
  }
  if (!legacy && !report.rootMultipliers) return report;

  // O sol como o renderer o escolhia: a Fonte do sol, senão o de Hora do dia,
  // senão a direcional mais brilhante (renderer::selectDirectionalLight).
  ObjectId sunSource = 0;
  SunCandidate best;
  std::vector<SunCandidate> suns;
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) continue;
    if (const auto *settings = static_cast<const scene::LightingSettings *>(
            object->components.find(scene::LightingSettings::descriptor)); settings && !sunSource)
      sunSource = settings->sunSource;
    const auto *light = static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor));
    if (!light || !light->enabled || light->kind != scene::LightKind::Directional || !graph.activeInHierarchy(id))
      continue;
    SunCandidate candidate{id,
        scene::lightIntensityForShader(light->kind, light->unit, light->intensity, light->innerHalfAngle(),
                                       light->outerHalfAngle()),
        light->unit != scene::LightUnit::Engine,
        object->components.find(scene::TimeOfDay::descriptor) != nullptr};
    suns.push_back(candidate);
  }
  for (const auto &candidate : suns) {
    if (candidate.id == sunSource) { best = candidate; break; }
    if (!best.id || (candidate.timeOfDay && !best.timeOfDay) ||
        (candidate.timeOfDay == best.timeOfDay && candidate.intensity > best.intensity)) best = candidate;
  }
  const float reference = kLegacyReferenceSun * std::max(multipliers[0], 1e-6f);
  if (legacy && best.id && best.photometric && best.intensity > 0.0f && std::isfinite(best.intensity))
    report.skyScale = best.intensity / reference;

  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object || !object->components.find(scene::Environment::descriptor)) continue;
    auto values = *object;
    bool changed = false;
    for (u32 index = 0; index < values.components.size(); ++index) {
      auto *component = values.components.editInstance(values.components.at(index)->instanceId());
      if (!component || &component->type() != &scene::Environment::descriptor) continue;
      auto &environment = static_cast<scene::Environment &>(*component);
      auto &look = environment.values;
      if (environment.loadedVersion < scene::Environment::CurrentVersion && report.skyScale != 1.0f) {
        // Exposure do céu = 1,3 × escala antiga; EV do HDRI += log2(escala).
        look.skyExposure = std::clamp(1.3f * report.skyScale, 0.0f, 1000.0f);
        look.hdriExposureEv = std::clamp(look.hdriExposureEv + std::log2(report.skyScale), -16.0f, 16.0f);
      }
      if (report.rootMultipliers) {
        if (environment.overrideIndirect)
          look.indirectDiffuse = std::clamp(look.indirectDiffuse * multipliers[1], 0.0f, 8.0f);
        // "Exposição ×" clareava a imagem: na Exposure do HDRP isso é a
        // compensação (automática) ou um EV100 fixo menor (fixa).
        if (environment.overrideExposure) {
          const float brighten = std::log2(std::max(multipliers[2], 1e-6f));
          if (look.automaticExposure())
            look.exposureCompensation = std::clamp(look.exposureCompensation + brighten, -16.0f, 16.0f);
          else look.fixedExposure = std::clamp(look.fixedExposure - brighten, -16.0f, 24.0f);
        }
        if (environment.overrideSky) {
          float rotation = std::fmod(look.hdriRotationDegrees + multipliers[3], 360.0f);
          look.hdriRotationDegrees = rotation;
        }
      }
      environment.loadedVersion = scene::Environment::CurrentVersion;
      changed = true;
    }
    if (changed && graph.applyEntityValues(id, values)) ++report.environments;
  }
  if (report.rootMultipliers) {
    auto values = *graph.find(graph.root());
    values.environment[0] = values.environment[1] = values.environment[2] = 1.0f;
    values.environment[3] = 0.0f;
    graph.applyEntityValues(graph.root(), values);
  }
  return report;
}

u32 migrateLegacyVolumeLayers(SceneGraph &graph) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  struct Volume { ObjectId id; u32 legacy; };
  std::vector<Volume> volumes;
  std::vector<ObjectId> cameras;
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) continue;
    if (const auto *environment = static_cast<const scene::Environment *>(
            object->components.find(scene::Environment::descriptor)); environment)
      volumes.push_back({id, environment->legacyLayer});
    if (const auto *camera = static_cast<const scene::Camera *>(object->components.find(scene::Camera::descriptor));
        camera && camera->legacyEnvironmentMask) cameras.push_back(id);
  }
  const bool layered = std::any_of(volumes.begin(), volumes.end(), [](const Volume &v) { return v.legacy != 0; });
  if (!layered && cameras.empty()) return 0;
  auto layers = graph.layers();
  // "Ambiente N" -> camada de objeto: a que já tem esse nome, senão a primeira
  // vaga sem nome (a 0, "Padrão", nunca é usada para isso).
  u32 mapped[32];
  for (u32 legacy = 0; legacy < 32; ++legacy) mapped[legacy] = ~0u;
  const auto layerFor = [&](u32 legacy) -> u32 {
    if (mapped[legacy] != ~0u) return mapped[legacy];
    const std::string name = "Ambiente " + std::to_string(legacy);
    for (u32 layer = 1; layer < GameplayLayers::kCount; ++layer)
      if (layers.name(layer) == name) return mapped[legacy] = layer;
    for (u32 layer = 1; layer < GameplayLayers::kCount; ++layer)
      if (!layers.named(layer) && layers.setName(layer, name)) return mapped[legacy] = layer;
    return mapped[legacy] = 0; // sem vaga: o volume fica na camada em que já está
  };
  u32 changed = 0;
  // Camadas de objeto ocupadas pelos volumes de cada "Ambiente N" antigo.
  u32 reach[32]{};
  for (const auto &volume : volumes) {
    auto values = *graph.find(volume.id);
    const u32 before = values.layer;
    if (volume.legacy != 0) {
      const u32 layer = layerFor(volume.legacy);
      if (layer != 0 && values.layer != layer) values.layer = layer;
    }
    reach[volume.legacy] |= 1u << std::min(values.layer, 31u);
    if (const auto *environment = values.components.find(scene::Environment::descriptor))
      if (auto *edit = static_cast<scene::Environment *>(values.components.editInstance(environment->instanceId())))
        edit->legacyLayer = 0;
    if (graph.applyEntityValues(volume.id, values) && (volume.legacy != 0 || before != values.layer)) ++changed;
  }
  for (const auto id : cameras) {
    auto values = *graph.find(id);
    const auto *found = values.components.find(scene::Camera::descriptor);
    auto *camera = found ? static_cast<scene::Camera *>(values.components.editInstance(found->instanceId())) : nullptr;
    if (!camera) continue;
    u32 mask = 0;
    for (u32 legacy = 0; legacy < 32; ++legacy) if (camera->environmentMask & (1u << legacy)) mask |= reach[legacy];
    camera->environmentMask = mask;camera->legacyEnvironmentMask = false;
    if (graph.applyEntityValues(id, values)) ++changed;
  }
  graph.setLayers(layers);
  return changed;
}

} // namespace ae::runtime
