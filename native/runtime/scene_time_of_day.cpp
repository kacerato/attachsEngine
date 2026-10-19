#include "runtime/scene_time_of_day.h"
#include "renderer/time_of_day.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include "scene/time_of_day.h"

#include <cmath>
#include <vector>

namespace ae::runtime {
namespace {
void normalize(float v[3]) {
  const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.0f) for (u32 i = 0; i < 3; ++i) v[i] /= length;
}
void cross(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}
} // namespace

u32 applyTimeOfDay(const SceneGraph &graph, double sceneSeconds,
                   const std::function<bool(ObjectId, const Transform &)> &write) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  u32 rotated = 0;
  for (ObjectId id : ids) {
    const auto *object = graph.find(id);
    if (!object) continue;
    const auto *time = static_cast<const scene::TimeOfDay *>(object->components.find(scene::TimeOfDay::descriptor));
    const auto *light = static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor));
    if (!time || !time->enabled || !time->valid() || !light || light->kind != scene::LightKind::Directional) continue;
    renderer::TimeOfDayInput input;
    input.hour = renderer::advanceTimeOfDayHour(time->hour, time->dayLengthMinutes, sceneSeconds);
    input.dayOfYear = time->dayOfYear;
    input.latitude = time->latitude;
    input.northDegrees = time->northDegrees;
    const auto sun = renderer::evaluateTimeOfDay(input);
    float world[16], parent[16];
    if (!worldMatrix(graph, id, world) || !parentWorldMatrix(graph, id, parent)) continue;
    float scale[3];
    for (u32 c = 0; c < 3; ++c)
      scale[c] = std::sqrt(world[c * 4] * world[c * 4] + world[c * 4 + 1] * world[c * 4 + 1] + world[c * 4 + 2] * world[c * 4 + 2]);
    // +Z do objeto é a direção de emissão: aponta para longe do sol.
    float forward[3]{-sun.toSun[0], -sun.toSun[1], -sun.toSun[2]};
    normalize(forward);
    float up[3]{0.0f, 1.0f, 0.0f}, right[3];
    if (std::abs(forward[1]) > 0.999f) up[1] = 0.0f, up[2] = 1.0f;
    cross(up, forward, right);normalize(right);
    float trueUp[3];cross(forward, right, trueUp);
    float target[16]{right[0] * scale[0], right[1] * scale[0], right[2] * scale[0], 0,
                     trueUp[0] * scale[1], trueUp[1] * scale[1], trueUp[2] * scale[1], 0,
                     forward[0] * scale[2], forward[1] * scale[2], forward[2] * scale[2], 0,
                     world[12], world[13], world[14], 1};
    Transform local{};
    if (!localTransformForWorld(target, parent, local)) continue;
    const Transform &current = object->transform;
    bool differs = false;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float delta = std::remainder(local.rotationDegrees[axis] - current.rotationDegrees[axis], 360.0f);
      if (std::abs(delta) > 1e-3f) differs = true;
    }
    if (!differs) continue;
    // Só a rotação: posição e escala locais ficam exatamente as autorais.
    Transform next = current;
    for (u32 axis = 0; axis < 3; ++axis) next.rotationDegrees[axis] = local.rotationDegrees[axis];
    if (write(id, next)) ++rotated;
  }
  return rotated;
}
} // namespace ae::runtime
