#include "platform/virtual_joystick.h"

#include <algorithm>
#include <cmath>

namespace ae::platform {

void VirtualJoystick::setSettings(const VirtualJoystickSettings &settings) {
  if (!std::isfinite(settings.radiusFractionOfShortEdge) ||
      !std::isfinite(settings.deadZone) || settings.radiusFractionOfShortEdge < 0.04f ||
      settings.radiusFractionOfShortEdge > 0.30f || settings.deadZone < 0.0f ||
      settings.deadZone >= 0.95f) return;
  settings_ = settings;
}

bool VirtualJoystick::begin(i32 pointerId, float x, float y,
                            float viewportWidth, float viewportHeight) {
  if (state_.active || pointerId < 0 || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
      viewportWidth <= 0.0f || viewportHeight <= 0.0f) return false;
  state_ = {};
  state_.active = true;
  state_.pointerId = pointerId;
  state_.originX = state_.knobX = x;
  state_.originY = state_.knobY = y;
  state_.radiusPixels = std::min(viewportWidth, viewportHeight) *
                        settings_.radiusFractionOfShortEdge;
  return true;
}

bool VirtualJoystick::update(i32 pointerId, float x, float y) {
  if (!state_.active || state_.pointerId != pointerId ||
      !std::isfinite(x) || !std::isfinite(y)) return false;
  recalculate(x, y);
  return true;
}

bool VirtualJoystick::end(i32 pointerId) {
  if (!state_.active || state_.pointerId != pointerId) return false;
  cancel();
  return true;
}

void VirtualJoystick::cancel() { state_ = {}; }

void VirtualJoystick::recalculate(float x, float y) {
  const float dx = x - state_.originX;
  const float dy = y - state_.originY;
  const float distance = std::hypot(dx, dy);
  const float scale = distance > state_.radiusPixels && distance > 0.0f
                          ? state_.radiusPixels / distance : 1.0f;
  const float clampedX = dx * scale;
  const float clampedY = dy * scale;
  state_.knobX = state_.originX + clampedX;
  state_.knobY = state_.originY + clampedY;
  const float magnitude = std::min(distance / state_.radiusPixels, 1.0f);
  const float remapped = magnitude <= settings_.deadZone
                             ? 0.0f
                             : (magnitude - settings_.deadZone) / (1.0f - settings_.deadZone);
  const float inverse = distance > 0.0f ? 1.0f / distance : 0.0f;
  state_.axisX = dx * inverse * remapped;
  state_.axisY = -dy * inverse * remapped;
}

} // namespace ae::platform
