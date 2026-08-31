#pragma once

#include "core/base.h"

namespace ae::renderer {

// Platform-independent runtime overlay state. Input owns the joystick and the
// renderer only consumes this immutable snapshot, so future projects can skin
// or replace the presentation without changing controller behavior.
struct RuntimeHudState {
  bool visible = false;
  bool joystickActive = false;
  float joystickCenterX = 0.0f;
  float joystickCenterY = 0.0f;
  float joystickKnobX = 0.0f;
  float joystickKnobY = 0.0f;
  float joystickRadiusPixels = 0.0f;
  u32 framesPerSecond = 0;
};

} // namespace ae::renderer
