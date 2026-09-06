#include "physics/water_simulation.h"

#include <algorithm>
#include <cmath>

namespace ae::physics {

bool WaterSimulation::configure(const WaterSimulationSettings &settings) noexcept {
  if (world_ != nullptr || !std::isfinite(settings.fixedStep) || settings.fixedStep < 1.0 / 240 ||
      settings.fixedStep > 1.0 / 30 || !std::isfinite(settings.maximumFrameTime) ||
      settings.maximumFrameTime < settings.fixedStep || settings.maximumFrameTime > 1 ||
      settings.maximumSteps == 0 || settings.maximumSteps > 32 ||
      !validateWaterRuntimeSettings(settings.water)) return false;
  settings_ = settings;
  return true;
}

bool WaterSimulation::setWaterSettings(const WaterRuntimeSettings &settings) noexcept {
  if (!validateWaterRuntimeSettings(settings)) return false;
  settings_.water = settings;
  return true;
}

void WaterSimulation::reset() noexcept {
  runtime_.clear();
  world_ = nullptr;
  time_ = accumulator_ = 0;
  faulted_ = false;
}

WaterSimulationFrame WaterSimulation::advance(AetherPhysicsWorld *physics,
    const renderer::WaterWorld &water, double frameSeconds, double timeScale,
    bool paused, WaterSimulationHooks hooks) noexcept {
  WaterSimulationFrame result{};
  result.simulationTime = time_;
  result.interpolation = std::clamp(accumulator_ / settings_.fixedStep, 0.0, 1.0);
  if (physics == nullptr || (world_ && world_ != physics) || faulted_ ||
      !std::isfinite(frameSeconds) || frameSeconds < 0 ||
      !std::isfinite(timeScale) || timeScale < 0 || timeScale > 4 ||
      !std::isfinite(frameSeconds * timeScale)) {
    result.error = WaterSimulationError::InvalidInput;
    return result;
  }
  if (paused || timeScale == 0) return result;
  world_ = physics;
  const double accepted = std::min(frameSeconds, settings_.maximumFrameTime);
  result.droppedSimulationTime = (frameSeconds - accepted) * timeScale;
  accumulator_ += accepted * timeScale;
  const double epsilon = settings_.fixedStep * 1e-9;
  while (accumulator_ + epsilon >= settings_.fixedStep && result.steps < settings_.maximumSteps) {
    if (hooks.beforeStep && !hooks.beforeStep(hooks.context, time_)) {
      result.error = WaterSimulationError::Provider;
      break; // no force applied yet: retry the same step on the next frame
    }
    if (!runtime_.apply(physics, water, time_, settings_.water)) {
      result.error = WaterSimulationError::Forces;
      faulted_ = true;
      break;
    }
    const i32 collisionSteps = static_cast<i32>(std::ceil(settings_.fixedStep * 60.0));
    result.physicsErrors |= AetherPhysics_StepV2(physics, static_cast<float>(settings_.fixedStep), collisionSteps);
    ++result.steps;
    time_ += settings_.fixedStep;
    accumulator_ = std::max(0.0, accumulator_ - settings_.fixedStep);
    if (hooks.afterStep) hooks.afterStep(hooks.context, runtime_.events());
    if (result.physicsErrors != 0) {
      result.error = WaterSimulationError::Physics;
      faulted_ = true; // world already advanced: never replay this step
      break;
    }
  }
  if (result.error == WaterSimulationError::None && accumulator_ + epsilon >= settings_.fixedStep) {
    const double droppedSteps = std::floor((accumulator_ + epsilon) / settings_.fixedStep);
    const double dropped = std::min(accumulator_, droppedSteps * settings_.fixedStep);
    result.droppedSimulationTime += dropped;
    accumulator_ = std::max(0.0, accumulator_ - dropped);
  }
  result.simulationTime = time_;
  result.interpolation = std::clamp(accumulator_ / settings_.fixedStep, 0.0, 1.0);
  return result;
}

} // namespace ae::physics
