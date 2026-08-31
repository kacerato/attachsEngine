#pragma once

#include "core/base.h"

namespace ae::platform {

struct VirtualJoystickSettings {
  float radiusFractionOfShortEdge = 0.13f;
  float deadZone = 0.12f;
};

struct VirtualJoystickState {
  bool active = false;
  i32 pointerId = -1;
  float originX = 0.0f;
  float originY = 0.0f;
  float knobX = 0.0f;
  float knobY = 0.0f;
  float radiusPixels = 0.0f;
  float axisX = 0.0f;
  float axisY = 0.0f;
};

// A reusable floating touch joystick. Coordinates are in platform pixels;
// output is a normalized action vector with circular clamping and radial
// dead-zone remapping. Rendering consumes State but never owns touch logic.
class VirtualJoystick final {
public:
  void setSettings(const VirtualJoystickSettings &settings);
  const VirtualJoystickSettings &settings() const { return settings_; }
  const VirtualJoystickState &state() const { return state_; }

  bool begin(i32 pointerId, float x, float y, float viewportWidth, float viewportHeight);
  bool update(i32 pointerId, float x, float y);
  bool end(i32 pointerId);
  void cancel();

private:
  void recalculate(float x, float y);
  VirtualJoystickSettings settings_{};
  VirtualJoystickState state_{};
};

} // namespace ae::platform
