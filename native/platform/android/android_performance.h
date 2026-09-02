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
  // A sessão contém apenas a render thread. Portanto o valor reportado é CPU
  // executada por essa thread, nunca wall time com acquire/present bloqueado.
  void reportThreadWorkDuration(i64 actualThreadCpuDurationNs);
  // API 35+: reporta o mesmo ciclo com decomposição CPU/GPU. O timestamp usa
  // CLOCK_MONOTONIC; total inclui a maior cauda observada porque CPU e GPU se
  // sobrepõem. Em plataformas antigas há fallback para a duração de CPU.
  void reportFrameWorkDuration(i64 workPeriodStartNs, i64 actualTotalDurationNs,
                               i64 actualCpuDurationNs, i64 actualGpuDurationNs);

  bool hintSessionAvailable() const { return hintSession_ != nullptr; }
  bool detailedWorkDurationAvailable() const { return workDuration_ != nullptr; }
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
  using CreateWorkDurationFn = void *(*)();
  using ReleaseWorkDurationFn = void (*)(void *);
  using SetWorkDurationValueFn = void (*)(void *, i64);
  using ReportActual2Fn = int (*)(void *, void *);
  using SetPreferPowerEfficiencyFn = int (*)(void *, bool);

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
  CreateWorkDurationFn createWorkDuration_ = nullptr;
  ReleaseWorkDurationFn releaseWorkDuration_ = nullptr;
  SetWorkDurationValueFn setWorkStart_ = nullptr;
  SetWorkDurationValueFn setWorkTotal_ = nullptr;
  SetWorkDurationValueFn setWorkCpu_ = nullptr;
  SetWorkDurationValueFn setWorkGpu_ = nullptr;
  ReportActual2Fn reportActual2_ = nullptr;
  SetPreferPowerEfficiencyFn setPreferPowerEfficiency_ = nullptr;
  void *workDuration_ = nullptr;
  i64 targetFrameDurationNs_ = 0;
  i64 preferredUpdateRateNs_ = 0;
  i32 gameMode_ = 0;
  u32 reportFailures_ = 0;
  bool preferSustainedPerformance_ = true;
  bool sustainedPerformanceSupported_ = false;
  bool sustainedPerformanceEnabled_ = false;
};

} // namespace ae::platform::android
