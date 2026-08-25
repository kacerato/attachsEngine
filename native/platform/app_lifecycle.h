#pragma once

#include "core/base.h"

namespace ae::platform {

enum class AppEvent : u8 {
  Resume,
  Pause,
  GainFocus,
  LoseFocus,
  WindowCreated,
  WindowDestroyed,
  Destroy,
};

enum class LifecycleAction : u8 {
  None = 0,
  CreateSurface = 1 << 0,
  DestroySurface = 1 << 1,
  BecameActive = 1 << 2,
  BecameInactive = 1 << 3,
  Quit = 1 << 4,
};

constexpr LifecycleAction operator|(LifecycleAction left, LifecycleAction right) {
  return static_cast<LifecycleAction>(static_cast<u8>(left) | static_cast<u8>(right));
}

constexpr bool hasAction(LifecycleAction value, LifecycleAction expected) {
  return (static_cast<u8>(value) & static_cast<u8>(expected)) != 0;
}

// Estado portátil do shell. O adaptador Android apenas converte APP_CMD_* em
// eventos; as invariantes ficam testáveis sem emulador e sem GPU.
class AppLifecycle final {
public:
  LifecycleAction apply(AppEvent event);

  bool isActive() const { return active_; }
  bool hasWindow() const { return hasWindow_; }
  bool isDestroyed() const { return destroyed_; }

private:
  bool resumed_ = false;
  bool focused_ = false;
  bool hasWindow_ = false;
  bool active_ = false;
  bool destroyed_ = false;
};

} // namespace ae::platform
