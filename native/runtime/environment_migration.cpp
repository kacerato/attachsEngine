#include "runtime/environment_migration.h"

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

} // namespace ae::runtime
