#include "platform/free_camera_controller.h"

#include <algorithm>
#include <cmath>

namespace ae::platform {
namespace {
constexpr float MinimumPinchPixels = 1.0f;

bool finiteState(const FreeCameraState &state) {
  return std::isfinite(state.position[0]) && std::isfinite(state.position[1]) &&
         std::isfinite(state.position[2]) && std::isfinite(state.yaw) &&
         std::isfinite(state.pitch);
}

float distance(const FreeCameraTouch &a, const FreeCameraTouch &b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace

void FreeCameraController::setState(const FreeCameraState &state) {
  if (!finiteState(state)) return;
  state_ = state;
  state_.pitch = std::clamp(state_.pitch, -settings_.maximumPitchRadians,
                            settings_.maximumPitchRadians);
  cancelGesture();
}

void FreeCameraController::setSettings(const FreeCameraGestureSettings &settings) {
  if (!std::isfinite(settings.lookRadiansPerScreenX) ||
      !std::isfinite(settings.lookRadiansPerScreenY) ||
      !std::isfinite(settings.maximumPitchRadians) ||
      !std::isfinite(settings.panUnitsPerScreen) ||
      !std::isfinite(settings.dollyUnitsPerScreen) ||
      settings.lookRadiansPerScreenX <= 0.0f || settings.lookRadiansPerScreenY <= 0.0f ||
      settings.maximumPitchRadians <= 0.0f || settings.maximumPitchRadians >= 1.57079632679f ||
      settings.panUnitsPerScreen <= 0.0f || settings.dollyUnitsPerScreen <= 0.0f) {
    return;
  }
  settings_ = settings;
  state_.pitch = std::clamp(state_.pitch, -settings_.maximumPitchRadians,
                            settings_.maximumPitchRadians);
}

bool FreeCameraController::updateTouches(const FreeCameraTouch *touches, u32 count,
                                         float viewportWidth, float viewportHeight) {
  if ((touches == nullptr && count != 0) || count > MaxTrackedTouches ||
      viewportWidth <= 0.0f || viewportHeight <= 0.0f ||
      !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight)) {
    return false;
  }
  if (count == 0) {
    cancelGesture();
    return true;
  }

  // A pointer-set transition is a new gesture baseline. Comparing one-finger
  // coordinates with an earlier two-finger centroid would cause a camera jump.
  bool sameSet = count == previousCount_;
  for (u32 index = 0; sameSet && index < count; ++index) {
    sameSet = touches[index].id == previous_[index].id;
  }
  if (!sameSet) {
    for (u32 index = 0; index < count; ++index) previous_[index] = touches[index];
    previousCount_ = count;
    return true;
  }

  if (count == 1) {
    state_.yaw += (touches[0].x - previous_[0].x) / viewportWidth *
                  settings_.lookRadiansPerScreenX;
    state_.pitch = std::clamp(
        state_.pitch + (touches[0].y - previous_[0].y) / viewportHeight *
                           settings_.lookRadiansPerScreenY,
        -settings_.maximumPitchRadians, settings_.maximumPitchRadians);
  } else {
    const float previousCenterX = (previous_[0].x + previous_[1].x) * 0.5f;
    const float previousCenterY = (previous_[0].y + previous_[1].y) * 0.5f;
    const float centerX = (touches[0].x + touches[1].x) * 0.5f;
    const float centerY = (touches[0].y + touches[1].y) * 0.5f;
    const float rightAmount = (centerX - previousCenterX) / viewportWidth *
                              settings_.panUnitsPerScreen;
    const float upAmount = -(centerY - previousCenterY) / viewportHeight *
                           settings_.panUnitsPerScreen;
    const float previousDistance = distance(previous_[0], previous_[1]);
    const float currentDistance = distance(touches[0], touches[1]);
    const float forwardAmount = previousDistance >= MinimumPinchPixels
                                    ? (currentDistance - previousDistance) / viewportWidth *
                                          settings_.dollyUnitsPerScreen
                                    : 0.0f;

    // +Z is camera-forward in Aether's current projection convention.
    const float cosineYaw = std::cos(state_.yaw);
    const float sineYaw = std::sin(state_.yaw);
    const float cosinePitch = std::cos(state_.pitch);
    const float sinePitch = std::sin(state_.pitch);
    const float right[3] = {cosineYaw, 0.0f, -sineYaw};
    const float forward[3] = {sineYaw * cosinePitch, -sinePitch,
                              cosineYaw * cosinePitch};
    for (u32 axis = 0; axis < 3; ++axis) {
      state_.position[axis] += right[axis] * rightAmount + forward[axis] * forwardAmount;
    }
    state_.position[1] += upAmount;
  }

  for (u32 index = 0; index < count; ++index) previous_[index] = touches[index];
  return true;
}

void FreeCameraController::cancelGesture() {
  previousCount_ = 0;
  previous_[0] = {};
  previous_[1] = {};
}

} // namespace ae::platform
