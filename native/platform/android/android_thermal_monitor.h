#pragma once

#include "core/base.h"
#include "renderer/thermal_pressure_controller.h"

#include <chrono>

namespace ae::platform::android {

struct AndroidThermalMonitorPolicy final {
  u32 sampleIntervalSeconds = 10;
  i32 forecastSeconds = 10;
  renderer::ThermalPressureSettings pressure{};
};

struct AndroidThermalState final {
  bool apiAvailable = false;
  bool headroomValid = false;
  bool headroomEverValid = false;
  float headroom = -1.0f;
  i32 status = -1;
  renderer::ThermalPressure pressure = renderer::ThermalPressure::None;
  u64 sequence = 0;
};

// Adaptador NDK carregado dinamicamente. O APK continua compatível com minSdk
// 26; API 30/31 ausente vira estado indisponível, nunca falha de carregamento.
// poll() pode ser chamado por frame e só acessa o serviço no intervalo fixado.
class AndroidThermalMonitor final {
public:
  AndroidThermalMonitor() = default;
  ~AndroidThermalMonitor();

  AndroidThermalMonitor(const AndroidThermalMonitor &) = delete;
  AndroidThermalMonitor &operator=(const AndroidThermalMonitor &) = delete;

  bool initialize(const AndroidThermalMonitorPolicy &policy = {});
  void shutdown();
  bool poll();

  const AndroidThermalState &state() const { return state_; }

private:
  using AcquireManagerFn = void *(*)();
  using ReleaseManagerFn = void (*)(void *);
  using GetStatusFn = i32 (*)(void *);
  using GetHeadroomFn = float (*)(void *, i32);

  void *androidLibrary_ = nullptr;
  void *manager_ = nullptr;
  AcquireManagerFn acquireManager_ = nullptr;
  ReleaseManagerFn releaseManager_ = nullptr;
  GetStatusFn getStatus_ = nullptr;
  GetHeadroomFn getHeadroom_ = nullptr;
  AndroidThermalMonitorPolicy policy_{};
  AndroidThermalState state_{};
  renderer::ThermalPressureController pressureController_{};
  std::chrono::steady_clock::time_point nextSampleAt_{};
};

} // namespace ae::platform::android
