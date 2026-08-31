#pragma once

#include "platform/free_camera_controller.h"
#include "platform/virtual_joystick.h"

namespace ae::platform {

struct FirstPersonInput {
  float moveRight = 0.0f;
  float moveForward = 0.0f;
  float lookScreenX = 0.0f;
  float lookScreenY = 0.0f;
  bool sprint = false;
};

struct FirstPersonSettings {
  float movementUnitsPerSecond = 24.0f;
  float sprintMultiplier = 1.65f;
  float lookRadiansPerScreenX = 5.2f;
  float lookRadiansPerScreenY = 3.4f;
  float maximumPitchRadians = 1.45f;
};

// Gameplay/controller primitive. It maps actions to an existing camera pose;
// physics collision/ground resolution can be supplied by a CharacterController
// consumer later without coupling touch IDs or Android APIs to this class.
class FirstPersonController final {
public:
  void setSettings(const FirstPersonSettings &settings);
  const FirstPersonSettings &settings() const { return settings_; }
  bool update(FreeCameraState &state, const FirstPersonInput &input, float deltaSeconds) const;

private:
  FirstPersonSettings settings_{};
};

// Routes independent left/right pointers into Move and Look actions. Pointer
// ownership is stable across multi-touch changes, so lifting one finger cannot
// make the other control jump between roles.
class FirstPersonTouchControls final {
public:
  bool pointerDown(i32 pointerId, float x, float y,
                   float viewportWidth, float viewportHeight);
  bool pointerMove(i32 pointerId, float x, float y,
                   float viewportWidth, float viewportHeight);
  bool pointerUp(i32 pointerId);
  void cancel();
  FirstPersonInput consumeInput();

  const VirtualJoystickState &joystickState() const { return joystick_.state(); }

private:
  VirtualJoystick joystick_{};
  i32 lookPointerId_ = -1;
  float previousLookX_ = 0.0f;
  float previousLookY_ = 0.0f;
  float accumulatedLookX_ = 0.0f;
  float accumulatedLookY_ = 0.0f;
};

} // namespace ae::platform
