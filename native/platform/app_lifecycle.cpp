#include "platform/app_lifecycle.h"

namespace ae::platform {

LifecycleAction AppLifecycle::apply(AppEvent event) {
  if (destroyed_) return LifecycleAction::None;

  LifecycleAction action = LifecycleAction::None;
  const bool wasActive = active_;

  switch (event) {
  case AppEvent::Resume:
    resumed_ = true;
    break;
  case AppEvent::Pause:
    resumed_ = false;
    break;
  case AppEvent::GainFocus:
    focused_ = true;
    break;
  case AppEvent::LoseFocus:
    focused_ = false;
    break;
  case AppEvent::WindowCreated:
    if (!hasWindow_) action = action | LifecycleAction::CreateSurface;
    hasWindow_ = true;
    break;
  case AppEvent::WindowDestroyed:
    if (hasWindow_) action = action | LifecycleAction::DestroySurface;
    hasWindow_ = false;
    break;
  case AppEvent::Destroy:
    if (hasWindow_) action = action | LifecycleAction::DestroySurface;
    hasWindow_ = false;
    resumed_ = false;
    focused_ = false;
    destroyed_ = true;
    action = action | LifecycleAction::Quit;
    break;
  }

  active_ = resumed_ && focused_ && hasWindow_ && !destroyed_;
  if (!wasActive && active_) action = action | LifecycleAction::BecameActive;
  if (wasActive && !active_) action = action | LifecycleAction::BecameInactive;
  return action;
}

} // namespace ae::platform
