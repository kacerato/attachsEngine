#include "runtime/scene_lights.h"
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"
#include "scene/light.h"

namespace ae::runtime {
bool collectSceneLights(const SceneGraph &graph, std::vector<renderer::SceneLight> &out) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::vector<renderer::SceneLight> collected;
  for (auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    const auto *light = static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor));
    if (!light || !light->enabled || !light->valid()) continue;
    bool active = true;
    for (const auto *ancestor = object; ancestor; ancestor = graph.find(ancestor->parent))
      if (!ancestor->active) { active = false; break; }
    if (!active) continue;
    float world[16];
    if (!worldMatrix(graph, id, world)) return false;
    renderer::SceneLight entry{};
    entry.objectId = id;
    entry.modality = static_cast<renderer::LightModality>(light->kind);
    float temperature[3]{1, 1, 1};
    if (light->useColorTemperature) scene::lightTemperatureColor(light->colorTemperature, temperature);
    for (u32 axis = 0; axis < 3; ++axis) {
      entry.position[axis] = world[12 + axis];
      // Coluna 2 é o +Z de mundo do objeto: a frente, pela convenção da Câmera.
      entry.direction[axis] = world[8 + axis];
      entry.color[axis] = light->color[axis] * temperature[axis];
    }
    entry.intensity = scene::lightIntensityForShader(light->kind, light->unit, light->intensity,
                                                     light->innerAngle, light->outerAngle);
    entry.range = light->range;
    entry.innerAngle = light->innerAngle;
    entry.outerAngle = light->outerAngle;
    collected.push_back(entry);
  }
  out = std::move(collected);
  return true;
}
} // namespace ae::runtime
