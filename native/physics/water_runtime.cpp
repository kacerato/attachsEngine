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
  // O ganho da ondulação entra na validação como qualquer outro: um valor não
  // finito aqui chegaria ao campo como amplitude não finita, e uma célula NaN
  // se espalha pela grade inteira em poucos passos.
  return validateBuoyancySettings(settings.forces) && std::isfinite(settings.enterFraction) &&
      std::isfinite(settings.exitFraction) && settings.exitFraction >= 0 &&
      settings.enterFraction > settings.exitFraction && settings.enterFraction <= 1 &&
      std::isfinite(settings.rippleGain) && settings.rippleGain >= 0 && settings.rippleGain <= 100;
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

float WaterRuntime::submergedFraction(AetherBodyHandle body) const noexcept {
  if (body == AetherBodyHandle_Invalid) return 0.0f;
  for (const auto &slot : slots_)
    if (slot.binding.body == body) return slot.submergedFraction;
  return 0.0f;
}

void WaterRuntime::clear() noexcept {
  for (auto &slot : slots_) slot = {};
  eventCount_ = 0;
  stats_ = {};
  applied_ = false;
}

bool WaterRuntime::apply(AetherPhysicsWorld *physics, const renderer::WaterWorld &water,
                         double time, const WaterRuntimeSettings &settings,
                         renderer::WaterRippleField *ripples) noexcept {
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
    // A pegada é o raio horizontal da forma, girado pela orientação do corpo:
    // um casco de proa para o norte precisa das sondagens ao longo do casco, não
    // dos eixos do mundo.
    const auto &extent = binding.shape.halfExtent;
    const float reach = (binding.shape.kind == BuoyantShapeKind::Sphere)
                            ? extent.x
                            : std::max(extent.x, extent.z);
    const float forwardX = 1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z);
    const float forwardZ = 2.0f * (rotation.x * rotation.y + rotation.w * rotation.z);
    const float scale = std::hypot(forwardX, forwardZ);
    // Quaternion degenerado cai nos eixos do mundo em vez de propagar NaN.
    const float axisX = scale > 1e-4f ? forwardX / scale : 1.0f;
    const float axisZ = scale > 1e-4f ? forwardZ / scale : 0.0f;

    const u32 base = count * ProbesPerBody;
    positions_[base + 0] = {position.x, position.z};
    positions_[base + 1] = {position.x + axisX * reach, position.z + axisZ * reach};
    positions_[base + 2] = {position.x - axisX * reach, position.z - axisZ * reach};
    positions_[base + 3] = {position.x - axisZ * reach, position.z + axisX * reach};
    positions_[base + 4] = {position.x + axisZ * reach, position.z - axisX * reach};
    bodyPositions_[count] = position;
    rotations_[count] = rotation;
    indices_[count++] = i;
  }
  const u32 probeCount = count * ProbesPerBody;
  if (!water.sample({positions_.data(), probeCount}, time, settings.layers,
                    {queries_.data(), probeCount})) return false;
  eventCount_ = 0;
  u32 forceCount = 0;
  for (u32 i = 0; i < count; ++i) {
    auto &slot = slots_[indices_[i]];
    const u32 base = i * ProbesPerBody;
    // O volume e o resto do estado saem da sondagem central: as pontas existem
    // para inclinar o plano, não para trocar o corpo de volume d'água.
    const auto &query = queries_[base];
    float fraction = 0;
    AetherWaterBodySample force{};
    if (query.volume != renderer::InvalidWaterVolume &&
        makeWaterBodySample(slot.binding.body, slot.binding.shape, positions_[base],
                            query.surface, slot.binding.referenceArea, force)) {
      // Só as pontas que caíram no mesmo volume entram no ajuste. Uma ponta
      // fora d'água, ou noutro volume, descreve outra superfície e inclinaria o
      // plano na direção errada exatamente na borda, que é onde importa.
      u32 probeCount = 0;
      for (u32 probe = 0; probe < ProbesPerBody; ++probe) {
        const auto &sample = queries_[base + probe];
        if (sample.volume != query.volume) continue;
        if (!renderer::hasWaterFieldFlag(sample.surface.flags, renderer::WaterFieldFlag::Valid))
          continue;
        planeSamples_[probeCount++] = {positions_[base + probe].x, positions_[base + probe].y,
                                       sample.surface.height};
      }
      WaterPlane fitted{};
      if (fitWaterPlane(planeSamples_.data(), probeCount, fitted)) {
        force.planeNormal = fitted.normal;
        force.planeOffset = fitted.offset;
      }
      const float depth=renderer::hasWaterFieldFlag(query.surface.flags,renderer::WaterFieldFlag::DepthKnown)?
          (query.surface.instantaneousDepth>=0?query.surface.instantaneousDepth:query.surface.depth):-1;
      const auto submerged = submergedWaterVolume(slot.binding.shape, bodyPositions_[i], rotations_[i],
                                             {force.planeNormal, force.planeOffset},depth);
      fraction = std::clamp(submerged.volume / buoyantShapeVolume(slot.binding.shape), 0.0f, 1.0f);
      depths_[forceCount]=depth;forces_[forceCount++] = force;

      // O corpo empurra a água de volta. A taxa é a velocidade vertical: é ela
      // que mede quanto volume está sendo deslocado por segundo, e é o que
      // separa um casco batendo na onda de um casco parado boiando.
      //
      // Usar o calado em vez da velocidade faria a água afundar sob um corpo
      // imóvel para sempre, porque a depressão estática já é responsabilidade
      // da flutuação — este campo existe para o que é transitório.
      if (ripples != nullptr && ripples->isReady() && settings.rippleGain > 0.0f &&
          fraction > 0.0f && applied_) {
        const auto velocity = AetherPhysics_GetLinearVelocity(physics, slot.binding.body);
        const float elapsed = static_cast<float>(time - lastTime_);
        if (std::isfinite(velocity.y) && elapsed > 0.0f) {
          const auto &extent = slot.binding.shape.halfExtent;
          const float radius = (slot.binding.shape.kind == BuoyantShapeKind::Sphere)
                                   ? extent.x
                                   : std::max(extent.x, extent.z);
          ripples->addImpulse(bodyPositions_[i].x, bodyPositions_[i].z, radius,
                              -velocity.y * fraction * settings.rippleGain * elapsed);
        }
      }
    }
    const bool sameVolume = slot.wetVolume != renderer::InvalidWaterVolume && slot.wetVolume == query.volume;
    slot.submergedFraction = fraction;
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
  if (AetherPhysics_ApplyWaterForcesV2(physics, forces_.data(),depths_.data(),static_cast<i32>(forceCount),
                                    &settings.forces, &stats.forces) < 0) return false;
  stats.queried = count;
  stats_ = stats;
  lastTime_ = time;
  applied_ = true;
  return true;
}

} // namespace ae::physics
