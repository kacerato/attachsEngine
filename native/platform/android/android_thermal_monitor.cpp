#include "platform/android/android_thermal_monitor.h"

#include <android/api-level.h>
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <dlfcn.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr i32 Android11Api = 30;
constexpr i32 Android12Api = 31;

template <typename Function>
Function loadSymbol(void *library, const char *name) {
  return reinterpret_cast<Function>(dlsym(library, name));
}
} // namespace

AndroidThermalMonitor::~AndroidThermalMonitor() { shutdown(); }

bool AndroidThermalMonitor::initialize(const AndroidThermalMonitorPolicy &policy) {
  shutdown();
  policy_ = policy;
  policy_.sampleIntervalSeconds = std::max(10u, policy_.sampleIntervalSeconds);
  policy_.forecastSeconds = std::clamp(policy_.forecastSeconds, 0, 60);
  pressureController_.reset(policy_.pressure);
  if (android_get_device_api_level() < Android11Api) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
                        "[ThermalPolicy] NDK Thermal indisponível antes da API 30.");
    return false;
  }

  androidLibrary_ = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  if (androidLibrary_ == nullptr) return false;
  acquireManager_ = loadSymbol<AcquireManagerFn>(androidLibrary_, "AThermal_acquireManager");
  releaseManager_ = loadSymbol<ReleaseManagerFn>(androidLibrary_, "AThermal_releaseManager");
  getStatus_ = loadSymbol<GetStatusFn>(androidLibrary_, "AThermal_getCurrentThermalStatus");
  if (android_get_device_api_level() >= Android12Api) {
    getHeadroom_ = loadSymbol<GetHeadroomFn>(androidLibrary_, "AThermal_getThermalHeadroom");
  }
  if (acquireManager_ == nullptr || releaseManager_ == nullptr || getStatus_ == nullptr) {
    shutdown();
    return false;
  }
  manager_ = acquireManager_();
  if (manager_ == nullptr) {
    shutdown();
    return false;
  }
  state_.apiAvailable = true;
  nextSampleAt_ = {};
  poll();
  return true;
}

void AndroidThermalMonitor::shutdown() {
  if (manager_ != nullptr && releaseManager_ != nullptr) releaseManager_(manager_);
  manager_ = nullptr;
  acquireManager_ = nullptr;
  releaseManager_ = nullptr;
  getStatus_ = nullptr;
  getHeadroom_ = nullptr;
  if (androidLibrary_ != nullptr) dlclose(androidLibrary_);
  androidLibrary_ = nullptr;
  state_ = {};
  policy_ = {};
  pressureController_.reset();
  nextSampleAt_ = {};
}

bool AndroidThermalMonitor::poll() {
  if (manager_ == nullptr || getStatus_ == nullptr) return false;
  const auto now = std::chrono::steady_clock::now();
  if (nextSampleAt_ != std::chrono::steady_clock::time_point{} && now < nextSampleAt_)
    return false;
  nextSampleAt_ = now + std::chrono::seconds(policy_.sampleIntervalSeconds);

  state_.status = getStatus_(manager_);
  state_.headroomValid = false;
  state_.headroom = -1.0f;
  if (getHeadroom_ != nullptr) {
    const float value = getHeadroom_(manager_, policy_.forecastSeconds);
    if (std::isfinite(value) && value >= 0.0f) {
      state_.headroom = value;
      state_.headroomValid = true;
      state_.headroomEverValid = true;
    }
  }
  const renderer::ThermalPressureUpdate update = pressureController_.observe({
      state_.status, state_.headroom, state_.status >= 0, state_.headroomValid});
  state_.pressure = update.pressure;
  ++state_.sequence;
  __android_log_print(
      ANDROID_LOG_INFO, LogTag,
      "[ThermalPolicy] sample=%llu status=%d headroom=%.3f forecast_s=%d pressure=%s changed=%s.",
      static_cast<unsigned long long>(state_.sequence), state_.status,
      static_cast<double>(state_.headroom), policy_.forecastSeconds,
      renderer::thermalPressureName(state_.pressure), update.changed ? "true" : "false");
  return true;
}

} // namespace ae::platform::android
