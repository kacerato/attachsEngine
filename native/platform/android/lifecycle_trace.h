#pragma once

#include "platform/app_lifecycle.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <chrono>

namespace ae::platform::android {

using LifecycleClock = std::chrono::steady_clock;

inline double lifecycleUptimeMs() {
  return std::chrono::duration<double, std::milli>(LifecycleClock::now().time_since_epoch()).count();
}

inline const char *appCommandName(int32_t command) {
  switch (command) {
  case APP_CMD_INPUT_CHANGED: return "INPUT_CHANGED";
  case APP_CMD_INIT_WINDOW: return "INIT_WINDOW";
  case APP_CMD_TERM_WINDOW: return "TERM_WINDOW";
  case APP_CMD_WINDOW_RESIZED: return "WINDOW_RESIZED";
  case APP_CMD_WINDOW_REDRAW_NEEDED: return "WINDOW_REDRAW_NEEDED";
  case APP_CMD_CONTENT_RECT_CHANGED: return "CONTENT_RECT_CHANGED";
  case APP_CMD_GAINED_FOCUS: return "GAINED_FOCUS";
  case APP_CMD_LOST_FOCUS: return "LOST_FOCUS";
  case APP_CMD_CONFIG_CHANGED: return "CONFIG_CHANGED";
  case APP_CMD_LOW_MEMORY: return "LOW_MEMORY";
  case APP_CMD_START: return "START";
  case APP_CMD_RESUME: return "RESUME";
  case APP_CMD_SAVE_STATE: return "SAVE_STATE";
  case APP_CMD_PAUSE: return "PAUSE";
  case APP_CMD_STOP: return "STOP";
  case APP_CMD_DESTROY: return "DESTROY";
  default: return "UNKNOWN";
  }
}

// Event-only diagnostics: no frame allocation, timer, thread or polling while
// suspended. Raw Android command and portable state are recorded separately.
inline void traceLifecycleCommand(const android_app &app, const AppLifecycle &state,
                                  u64 sequence, int32_t command, const char *phase,
                                  bool rendererReady, double elapsedMs) {
  __android_log_print(ANDROID_LOG_INFO, "Aether.Android",
                      "[Lifecycle] seq=%llu cmd=%s(%d) phase=%s uptime_ms=%.3f elapsed_ms=%.3f "
                      "resumed=%d focus=%d window=%d active=%d renderer=%d native_window=%d glue_state=%d",
                      static_cast<unsigned long long>(sequence), appCommandName(command), command,
                      phase, lifecycleUptimeMs(), elapsedMs, state.isResumed(), state.hasFocus(),
                      state.hasWindow(), state.isActive(), rendererReady, app.window != nullptr,
                      app.activityState);
}

class ScopedLifecycleStage final {
public:
  explicit ScopedLifecycleStage(const char *name) : name_(name), startedMs_(lifecycleUptimeMs()) {
    __android_log_print(ANDROID_LOG_INFO, "Aether.Android",
                        "[Lifecycle] stage=%s phase=begin uptime_ms=%.3f", name_, startedMs_);
  }
  ~ScopedLifecycleStage() {
    const double nowMs = lifecycleUptimeMs();
    __android_log_print(ANDROID_LOG_INFO, "Aether.Android",
                        "[Lifecycle] stage=%s phase=end uptime_ms=%.3f elapsed_ms=%.3f",
                        name_, nowMs, nowMs - startedMs_);
  }
  ScopedLifecycleStage(const ScopedLifecycleStage &) = delete;
  ScopedLifecycleStage &operator=(const ScopedLifecycleStage &) = delete;

private:
  const char *name_;
  double startedMs_;
};

} // namespace ae::platform::android
