#include "runtime/scene_time_of_day.h"
#include "renderer/time_of_day.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include "scene/time_of_day.h"

#include <algorithm>
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

// Gira a direcional `id` para emitir na direção oposta a `toBody`.
bool aimLightAt(const SceneGraph &graph, ObjectId id, const float toBody[3],
                const std::function<bool(ObjectId, const Transform &)> &write) {
  const auto *object = graph.find(id);
  if (!object) return false;
  float world[16], parent[16];
  if (!worldMatrix(graph, id, world) || !parentWorldMatrix(graph, id, parent)) return false;
  float scale[3];
  for (u32 c = 0; c < 3; ++c)
    scale[c] = std::sqrt(world[c * 4] * world[c * 4] + world[c * 4 + 1] * world[c * 4 + 1] + world[c * 4 + 2] * world[c * 4 + 2]);
  // +Z do objeto é a direção de emissão: aponta para longe do corpo celeste.
  float forward[3]{-toBody[0], -toBody[1], -toBody[2]};
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
  if (!localTransformForWorld(target, parent, local)) return false;
  const Transform &current = object->transform;
  bool differs = false;
  for (u32 axis = 0; axis < 3; ++axis) {
    const float delta = std::remainder(local.rotationDegrees[axis] - current.rotationDegrees[axis], 360.0f);
    if (std::abs(delta) > 1e-3f) differs = true;
  }
  if (!differs) return false;
  // Só a rotação: posição e escala locais ficam exatamente as autorais.
  Transform next = current;
  for (u32 axis = 0; axis < 3; ++axis) next.rotationDegrees[axis] = local.rotationDegrees[axis];
  return write(id, next);
}

bool directionalLight(const SceneGraph &graph, ObjectId id) {
  const auto *object = graph.find(id);
  const auto *light = object ? static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor)) : nullptr;
  return light && light->kind == scene::LightKind::Directional;
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
    if (!time || !time->enabled || !time->valid() || !directionalLight(graph, id)) continue;
    renderer::TimeOfDayInput input;
    input.hour = renderer::advanceTimeOfDayHour(time->hour, time->dayLengthMinutes, sceneSeconds);
    input.dayOfYear = time->dayOfYear;
    input.latitude = time->latitude;
    input.northDegrees = time->northDegrees;
    const auto sun = renderer::evaluateTimeOfDay(input);
    if (aimLightAt(graph, id, sun.toSun, write)) ++rotated;
    // V-2: a Lua referenciada gira junto, para a lua da mesma hora.
    if (time->moonLight != 0 && time->moonLight != id && directionalLight(graph, time->moonLight) &&
        aimLightAt(graph, time->moonLight, renderer::evaluateMoon(input, sun).toMoon, write)) ++rotated;
  }
  return rotated;
}

SceneTimeOfDayClock::Entry &SceneTimeOfDayClock::entry(ObjectId object, u64 instance) {
  for (auto &e : entries_) if (e.object == object && e.instance == instance) return e;
  entries_.push_back({object, instance});
  return entries_.back();
}

u32 SceneTimeOfDayClock::advance(GameWorld &world, double elapsed, ComponentEventQueue *events) {
  if (!world.running() || !std::isfinite(elapsed) || elapsed < 0) return 0;
  std::vector<ObjectId> ids;
  world.graph().collectSubtree(world.graph().root(), ids);
  u32 advanced = 0;
  const auto elevationAt = [](const scene::TimeOfDay &time, float hour) {
    renderer::TimeOfDayInput input;
    input.hour = hour;input.dayOfYear = time.dayOfYear;input.latitude = time.latitude;input.northDegrees = time.northDegrees;
    return renderer::evaluateTimeOfDay(input).elevation;
  };
  for (ObjectId id : ids) {
    const auto handle = world.handle(id);
    const auto component = world.findComponent(handle, scene::TimeOfDay::descriptor.id);
    if (!component.valid() || !world.activeInHierarchy(handle)) continue;
    const auto *time = static_cast<const scene::TimeOfDay *>(world.readComponent(component));
    if (!time || !time->enabled || !time->valid()) continue;
    // Cópia: escrever a hora substitui a instância e invalida `time`.
    scene::TimeOfDay current = *time;
    float hour = current.hour;
    float day = current.dayOfYear;
    u32 wrapped = 0;
    if (current.dayLengthMinutes > 0 && elapsed > 0) {
      double next = static_cast<double>(hour) + elapsed * 24.0 / (static_cast<double>(current.dayLengthMinutes) * 60.0);
      while (next >= 24.0 && wrapped < 366) { next -= 24.0; ++wrapped; }
      hour = static_cast<float>(std::clamp(next, 0.0, 23.9999));
      if (wrapped) day = static_cast<float>(1 + (static_cast<i32>(day) - 1 + static_cast<i32>(wrapped)) % 366);
      // Hora e dia passam pelo mesmo caminho validado que os scripts usam.
      if (world.setProperty(component, "hour", scene::ComponentPropertyValue{hour}) != WorldStatus::Ok) continue;
      if (wrapped && world.setProperty(component, "day_of_year", scene::ComponentPropertyValue{day}) != WorldStatus::Ok) continue;
      ++advanced;
    }
    current.dayOfYear = day;
    auto &state = entry(id, component.instance);
    const i32 whole = static_cast<i32>(std::floor(hour));
    const float elevation = elevationAt(current, hour);
    if (!state.seen) { state.seen = true;state.hour = whole;state.elevation = elevation;continue; }
    if (events) {
      const auto &type = scene::TimeOfDay::descriptor;
      // Cada hora inteira atravessada gera um evento, inclusive a virada do dia.
      if (whole != state.hour || wrapped) {
        i32 crossed = whole - state.hour + static_cast<i32>(wrapped) * 24;
        if (crossed <= 0 || crossed > 24) crossed = 1;
        for (i32 step = crossed - 1; step >= 0; --step) {
          const auto value = scene::ComponentOperationValue::makeInteger(((whole - step) % 24 + 24) % 24);
          events->emit(world, id, component.instance, type, "hour_changed", std::span(&value, 1));
        }
      }
      if (state.elevation < 0 && elevation >= 0) events->emit(world, id, component.instance, type, "sunrise");
      if (state.elevation >= 0 && elevation < 0) events->emit(world, id, component.instance, type, "sunset");
      if (wrapped) {
        const auto value = scene::ComponentOperationValue::makeInteger(static_cast<i64>(day));
        events->emit(world, id, component.instance, type, "day_changed", std::span(&value, 1));
      }
    }
    state.hour = whole;state.elevation = elevation;
  }
  return advanced;
}
} // namespace ae::runtime
