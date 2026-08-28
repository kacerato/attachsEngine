#pragma once

#include "core/base.h"

namespace ae::platform {

struct FreeCameraState {
  float position[3]{};
  float yaw = 0.0f;
  float pitch = 0.0f;
};

struct FreeCameraTouch {
  i32 id = -1;
  float x = 0.0f;
  float y = 0.0f;
};

struct FreeCameraGestureSettings {
  float lookRadiansPerScreenX = 6.28318530718f;
  float lookRadiansPerScreenY = 3.14159265359f;
  float maximumPitchRadians = 1.50f;
  float panUnitsPerScreen = 80.0f;
  float dollyUnitsPerScreen = 180.0f;
};

// Converts touch geometry into a renderer-agnostic six-degree camera state.
// One finger looks. Two-finger centroid motion pans right/up and pinch moves
// forward/back. Pointer-count/ID changes rebase the gesture, preventing jumps.
class FreeCameraController final {
public:
  void setState(const FreeCameraState &state);
  const FreeCameraState &state() const { return state_; }
  void setSettings(const FreeCameraGestureSettings &settings);
  const FreeCameraGestureSettings &settings() const { return settings_; }

  bool updateTouches(const FreeCameraTouch *touches, u32 count,
                     float viewportWidth, float viewportHeight);
  void cancelGesture();

private:
  static constexpr u32 MaxTrackedTouches = 2;
  FreeCameraState state_{};
  FreeCameraGestureSettings settings_{};
  FreeCameraTouch previous_[MaxTrackedTouches]{};
  u32 previousCount_ = 0;
};

} // namespace ae::platform
