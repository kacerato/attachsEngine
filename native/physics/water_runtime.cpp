#include "physics/water_runtime.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::physics {
namespace {
bool validBinding(const WaterBodyBinding &binding) noexcept {
  return binding.body != AetherBodyHandle_Invalid &&
      (binding.shape.kind == BuoyantShapeKind::Sphere || binding.shape.kind == BuoyantShapeKind::Box) &&
      std::isfinite(buoyantShapeVolume(binding.shape)) && buoyantShapeVolume(binding.shape) > 0 &&
      std::isfinite(binding.referenceArea) && binding.referenceArea >= 0;
}
}

bool validateWaterRuntimeSettings(const WaterRuntimeSettings &settings) noexcept {
  return validateBuoyancySettings(settings.forces) && std::isfinite(settings.enterFraction) &&
      std::isfinite(settings.exitFraction) && settings.exitFraction >= 0 &&
      settings.enterFraction > settings.exitFraction && settings.enterFraction <= 1;
}

bool WaterRuntime::bind(const WaterBodyBinding &binding) noexcept {
  if (!validBinding(binding)) return false;
  Slot *target = nullptr;
  for (auto &slot : slots_) {
    if (slot.binding.body == binding.body) { slot.binding = binding; return true; }
    if (target == nullptr && slot.binding.body == AetherBodyHandle_Invalid) target = &slot;
  }
  if (target == nullptr) return false;
  *target = {binding, renderer::InvalidWaterVolume};
  return true;
}

bool WaterRuntime::unbind(AetherBodyHandle body) noexcept {
  if (body == AetherBodyHandle_Invalid) return false;
  for (auto &slot : slots_) if (slot.binding.body == body) { slot = {}; return true; }
  return false;
}

void WaterRuntime::clear() noexcept {
  for (auto &slot : slots_) slot = {};
  eventCount_ = 0;
  stats_ = {};
  applied_ = false;
}

bool WaterRuntime::apply(AetherPhysicsWorld *physics, const renderer::WaterWorld &water,
                         double time, const WaterRuntimeSettings &settings) noexcept {
  if (physics == nullptr || !std::isfinite(time) || time < 0 ||
      time > std::numeric_limits<float>::max() || (applied_ && time <= lastTime_) ||
      !validateWaterRuntimeSettings(settings)) return false;

  u32 count = 0;
  WaterRuntimeStats stats{};
  for (u32 i = 0; i < Capacity; ++i) {
    const auto &binding = slots_[i].binding;
    if (binding.body == AetherBodyHandle_Invalid) continue;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    AetherVec3 position{nan, nan, nan};
    AetherQuat rotation{nan, nan, nan, nan};
    if (!AetherPhysics_TryGetBodyPoseV2(physics, binding.body, &position, &rotation) ||
        !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
      ++stats.unavailableBodies;
      continue;
    }
    positions_[count] = {position.x, position.z};
    bodyPositions_[count] = position;
    rotations_[count] = rotation;
    indices_[count++] = i;
  }
  if (!water.sample({positions_.data(), count}, time, settings.layers, {queries_.data(), count})) return false;
  eventCount_ = 0;
  u32 forceCount = 0;
  for (u32 i = 0; i < count; ++i) {
    auto &slot = slots_[indices_[i]];
    const auto &query = queries_[i];
    float fraction = 0;
    AetherWaterBodySample force{};
    if (query.volume != renderer::InvalidWaterVolume &&
        makeWaterBodySample(slot.binding.body, slot.binding.shape, positions_[i],
                            query.surface, slot.binding.referenceArea, force)) {
      const auto submerged = submergedVolume(slot.binding.shape, bodyPositions_[i], rotations_[i],
                                             {force.planeNormal, force.planeOffset});
      fraction = std::clamp(submerged.volume / buoyantShapeVolume(slot.binding.shape), 0.0f, 1.0f);
      forces_[forceCount++] = force;
    }
    const bool sameVolume = slot.wetVolume != renderer::InvalidWaterVolume && slot.wetVolume == query.volume;
    const bool wet = query.volume != renderer::InvalidWaterVolume &&
        fraction >= (sameVolume ? settings.exitFraction : settings.enterFraction) && fraction > 0;
    const auto next = wet ? query.volume : renderer::InvalidWaterVolume;
    if (next != slot.wetVolume) {
      if (slot.wetVolume != renderer::InvalidWaterVolume)
        events_[eventCount_++] = {slot.binding.body, slot.wetVolume, WaterContactPhase::Exit, fraction, time};
      if (next != renderer::InvalidWaterVolume)
        events_[eventCount_++] = {slot.binding.body, next, WaterContactPhase::Enter, fraction, time};
      slot.wetVolume = next;
    }
  }
  if (AetherPhysics_ApplyWaterForces(physics, forces_.data(), static_cast<i32>(forceCount),
                                    &settings.forces, &stats.forces) < 0) return false;
  stats.queried = count;
  stats_ = stats;
  lastTime_ = time;
  applied_ = true;
  return true;
}

} // namespace ae::physics
