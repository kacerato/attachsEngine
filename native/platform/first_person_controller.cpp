#include "platform/first_person_controller.h"

#include <algorithm>
#include <cmath>

namespace ae::platform {

void FirstPersonController::setSettings(const FirstPersonSettings &settings) {
  if (!std::isfinite(settings.movementUnitsPerSecond) ||
      !std::isfinite(settings.sprintMultiplier) ||
      !std::isfinite(settings.lookRadiansPerScreenX) ||
      !std::isfinite(settings.lookRadiansPerScreenY) ||
      !std::isfinite(settings.maximumPitchRadians) ||
      settings.movementUnitsPerSecond <= 0.0f || settings.sprintMultiplier < 1.0f ||
      settings.lookRadiansPerScreenX <= 0.0f || settings.lookRadiansPerScreenY <= 0.0f ||
      settings.maximumPitchRadians <= 0.0f || settings.maximumPitchRadians >= 1.57079632679f) return;
  settings_ = settings;
}

bool FirstPersonController::update(FreeCameraState &state, const FirstPersonInput &input,
                                   float deltaSeconds) const {
  const float values[] = {state.position[0], state.position[1], state.position[2],
                          state.yaw, state.pitch, input.moveRight, input.moveForward,
                          input.lookScreenX, input.lookScreenY, deltaSeconds};
  for (float value : values) if (!std::isfinite(value)) return false;
  if (deltaSeconds < 0.0f) return false;
  deltaSeconds = std::min(deltaSeconds, 0.05f);
  state.yaw += input.lookScreenX * settings_.lookRadiansPerScreenX;
  state.pitch = std::clamp(state.pitch + input.lookScreenY * settings_.lookRadiansPerScreenY,
                           -settings_.maximumPitchRadians, settings_.maximumPitchRadians);

  float rightAmount = input.moveRight;
  float forwardAmount = input.moveForward;
  const float magnitude = std::hypot(rightAmount, forwardAmount);
  if (magnitude > 1.0f) {
    rightAmount /= magnitude;
    forwardAmount /= magnitude;
  }
  const float distance = settings_.movementUnitsPerSecond *
                         (input.sprint ? settings_.sprintMultiplier : 1.0f) * deltaSeconds;
  const float cosineYaw = std::cos(state.yaw);
  const float sineYaw = std::sin(state.yaw);
  state.position[0] += (cosineYaw * rightAmount + sineYaw * forwardAmount) * distance;
  state.position[2] += (-sineYaw * rightAmount + cosineYaw * forwardAmount) * distance;
  return true;
}

bool FirstPersonTouchControls::pointerDown(i32 pointerId, float x, float y,
                                           float viewportWidth, float viewportHeight) {
  if (pointerId < 0 || !std::isfinite(x) || !std::isfinite(y) ||
      viewportWidth <= 0.0f || viewportHeight <= 0.0f) return false;
  if (x < viewportWidth * 0.5f && !joystick_.state().active)
    return joystick_.begin(pointerId, x, y, viewportWidth, viewportHeight);
  if (lookPointerId_ < 0) {
    lookPointerId_ = pointerId;
    previousLookX_ = x;
    previousLookY_ = y;
    return true;
  }
  return false;
}

bool FirstPersonTouchControls::pointerMove(i32 pointerId, float x, float y,
                                           float viewportWidth, float viewportHeight) {
  if (!std::isfinite(x) || !std::isfinite(y) || viewportWidth <= 0.0f ||
      viewportHeight <= 0.0f) return false;
  if (joystick_.update(pointerId, x, y)) return true;
  if (pointerId != lookPointerId_) return false;
  accumulatedLookX_ += (x - previousLookX_) / viewportWidth;
  accumulatedLookY_ += (y - previousLookY_) / viewportHeight;
  previousLookX_ = x;
  previousLookY_ = y;
  return true;
}

bool FirstPersonTouchControls::pointerUp(i32 pointerId) {
  if (joystick_.end(pointerId)) return true;
  if (pointerId != lookPointerId_) return false;
  lookPointerId_ = -1;
  previousLookX_ = previousLookY_ = 0.0f;
  return true;
}

void FirstPersonTouchControls::cancel() {
  joystick_.cancel();
  lookPointerId_ = -1;
  previousLookX_ = previousLookY_ = 0.0f;
  accumulatedLookX_ = accumulatedLookY_ = 0.0f;
}

FirstPersonInput FirstPersonTouchControls::consumeInput() {
  FirstPersonInput input{};
  input.moveRight = joystick_.state().axisX;
  input.moveForward = joystick_.state().axisY;
  input.lookScreenX = accumulatedLookX_;
  input.lookScreenY = accumulatedLookY_;
  accumulatedLookX_ = accumulatedLookY_ = 0.0f;
  return input;
}

} // namespace ae::platform
