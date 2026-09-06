#pragma once
#include "renderer/water_fft.h"
#include "renderer/water_foam.h"

namespace ae::renderer {
constexpr u32 MaximumWaterCascades=4;
struct WaterSpectralControls final {
  float displacement=1, choppiness=1, timeScale=1, directionRadians=0;
  WaterFoamSettings foam{};
  bool overrideFoam=false;
};
inline bool validateWaterSpectralControls(const WaterSpectralControls &c) noexcept {
  return validateWaterFoam(c.foam) && std::isfinite(c.displacement) && c.displacement>=0 && c.displacement<=3 &&
    std::isfinite(c.choppiness) && c.choppiness>=0 && c.choppiness<=2 &&
    std::isfinite(c.timeScale) && c.timeScale>=0 && c.timeScale<=3 && std::isfinite(c.directionRadians);
}
class WaterSpectralClock final {
public:
  double advance(double wallTime,float speed) noexcept {
    if(!std::isfinite(wallTime) || !std::isfinite(speed) || speed<0 || speed>3) return simulation_;
    if(initialized_ && wallTime>=previous_) simulation_+=(wallTime-previous_)*speed;
    previous_=wallTime; initialized_=true;
    return simulation_;
  }
private:
  double simulation_=0,previous_=0;
  bool initialized_=false;
};
struct WaterCascadeSettings final {
  WaterSpectrumSettings spectrum{};
  float displacementScale=1;
  float choppiness=1;
  WaterFoamSettings foam{};
};
struct WaterCascadeSpectrum final {
  WaterCascadeSettings settings{};
  std::vector<std::complex<float>> amplitudes;
};
std::array<WaterCascadeSettings,3> defaultWaterCascadeSettings();
enum class WaterCascadeError { None, Count, InvalidSettings, OverlappingBands, MemoryBudget };
// Array order is low to high wavelength. Gaps are permitted intentionally;
// overlap is rejected rather than silently changing artist-authored energy.
WaterCascadeError validateWaterCascades(std::span<const WaterCascadeSettings> settings,
                                       u64 gpuBufferBudgetBytes) noexcept;
u64 waterCascadeGpuBufferBytes(std::span<const WaterCascadeSettings> settings) noexcept;
bool generateWaterCascades(std::span<const WaterCascadeSettings> settings,
                          u64 gpuBufferBudgetBytes,
                          std::vector<WaterCascadeSpectrum> &output);
} // namespace ae::renderer
