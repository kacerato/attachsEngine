#pragma once

#include "core/base.h"

#include <cstdint>

struct ANativeActivity;

namespace ae::platform::android {

struct AndroidPerformancePolicy final {
  i64 targetFrameDurationNs = 16'666'667;
  // Public Window contract (API 24+). This requests a stable operating point
  // for prolonged play; it never relies on OEM/private clock controls.
  bool preferSustainedPerformance = true;
};

// Android-facing implementation of the engine's performance-policy bridge.
// It uses only public platform contracts: ADPF Performance Hint for the render
// thread and Game State for lifecycle intent. Unsupported OS/OEM combinations
// remain fully functional with a no-op fallback.
class AndroidPerformance final {
public:
  AndroidPerformance() = default;
  ~AndroidPerformance();

  AndroidPerformance(const AndroidPerformance &) = delete;
  AndroidPerformance &operator=(const AndroidPerformance &) = delete;

  bool initialize(ANativeActivity *activity, const AndroidPerformancePolicy &policy);
  void shutdown();
  void setActive(bool active, bool loading);
  void updateTargetFrameDuration(i64 targetFrameDurationNs);
  void reportFrameDuration(i64 actualFrameDurationNs);

  bool hintSessionAvailable() const { return hintSession_ != nullptr; }
  bool sustainedPerformanceSupported() const { return sustainedPerformanceSupported_; }
  bool sustainedPerformanceEnabled() const { return sustainedPerformanceEnabled_; }
  i32 gameMode() const { return gameMode_; }

private:
  using GetManagerFn = void *(*)();
  using CreateSessionFn = void *(*)(void *, const i32 *, usize, i64);
  using PreferredRateFn = i64 (*)(void *);
  using UpdateTargetFn = int (*)(void *, i64);
  using ReportActualFn = int (*)(void *, i64);
  using CloseSessionFn = void (*)(void *);

  bool publishGameState(bool active, bool loading);
  bool querySustainedPerformanceSupport();
  bool setSustainedPerformance(bool enabled);

  ANativeActivity *activity_ = nullptr;
  void *androidLibrary_ = nullptr;
  void *hintManager_ = nullptr;
  void *hintSession_ = nullptr;
  GetManagerFn getManager_ = nullptr;
  CreateSessionFn createSession_ = nullptr;
  PreferredRateFn preferredRate_ = nullptr;
  UpdateTargetFn updateTarget_ = nullptr;
  ReportActualFn reportActual_ = nullptr;
  CloseSessionFn closeSession_ = nullptr;
  i64 targetFrameDurationNs_ = 0;
  i64 preferredUpdateRateNs_ = 0;
  i32 gameMode_ = 0;
  u32 reportFailures_ = 0;
  bool preferSustainedPerformance_ = true;
  bool sustainedPerformanceSupported_ = false;
  bool sustainedPerformanceEnabled_ = false;
};

} // namespace ae::platform::android
