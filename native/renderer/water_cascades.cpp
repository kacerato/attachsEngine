#include "renderer/water_cascades.h"
#include <cmath>

namespace ae::renderer {
bool validateWaterSpectrumAuthoring(const WaterSpectrumAuthoringSettings &settings) noexcept {
  WaterSpectrumSettings probe{};
  probe.windSpeed = settings.windSpeed;
  probe.fetch = settings.fetch;
  probe.depth = settings.depth;
  probe.swell = settings.swell;
  probe.spread = settings.spread;
  probe.shortWaveDamping = settings.shortWaveDamping;
  probe.crossSwell = settings.crossSwell;
  if (!validateWaterSpectrum(probe)) return false;
  for (usize index = 0; index < MaximumWaterCascades; ++index) {
    if (!std::isfinite(settings.cascadeDisplacement[index]) ||
        settings.cascadeDisplacement[index] < 0.0f ||
        settings.cascadeDisplacement[index] > 3.0f ||
        !std::isfinite(settings.cascadeChoppiness[index]) ||
        settings.cascadeChoppiness[index] < 0.0f ||
        settings.cascadeChoppiness[index] > 4.0f) return false;
  }
  return true;
}

bool authorWaterCascades(std::span<const WaterCascadeSettings> base,
                         const WaterSpectrumAuthoringSettings &settings,
                         std::vector<WaterCascadeSettings> &output) {
  if (!validateWaterSpectrumAuthoring(settings) || base.empty() ||
      base.size() > MaximumWaterCascades) return false;
  std::vector<WaterCascadeSettings> result(base.begin(), base.end());
  for (usize index = 0; index < result.size(); ++index) {
    auto &cascade = result[index];
    cascade.spectrum.windSpeed = settings.windSpeed;
    cascade.spectrum.fetch = settings.fetch;
    cascade.spectrum.depth = settings.depth;
    cascade.spectrum.swell = settings.swell;
    cascade.spectrum.spread = settings.spread;
    cascade.spectrum.shortWaveDamping = settings.shortWaveDamping;
    cascade.spectrum.crossSwell = settings.crossSwell;
    cascade.displacementScale = settings.cascadeDisplacement[index];
    cascade.choppiness = settings.cascadeChoppiness[index];
  }
  if (validateWaterCascades(result, UINT64_MAX) != WaterCascadeError::None) return false;
  output = std::move(result);
  return true;
}

std::array<WaterCascadeSettings,3> defaultWaterCascadeSettings() {
  std::array<WaterCascadeSettings,3> result{};
  const float patches[]={32,128,2048};
  const float boundaries[]={2,8,32,2048};
  for(usize i=0;i<result.size();++i) {
    auto &s=result[i].spectrum;
    s.patchLength=patches[i]; s.resolution=128; s.seed=static_cast<u32>(i+1);
    s.minimumWavelength=boundaries[i]; s.maximumWavelength=boundaries[i+1];
  }
  return result;
}
u64 waterCascadeGpuBufferBytes(std::span<const WaterCascadeSettings> settings) noexcept {
  u64 result=0;
  for(const auto &cascade:settings) {
    // Mode (16 B), spatial channels (32 B), foam (4 B), filtered slopes (16 B).
    // Historical API name includes required images; excludes allocator alignment/mips.
    if(cascade.spectrum.resolution>256) return UINT64_MAX;
    result+=68ull*cascade.spectrum.resolution*cascade.spectrum.resolution;
  }
  return result;
}

WaterCascadeError validateWaterCascades(std::span<const WaterCascadeSettings> settings,
                                       u64 gpuBufferBudgetBytes) noexcept {
  if(settings.empty() || settings.size()>MaximumWaterCascades) return WaterCascadeError::Count;
  float previousEnd=0;
  for(const auto &cascade:settings) {
    const auto &s=cascade.spectrum;
    if(!validateWaterSpectrum(s) || !validateWaterFoam(cascade.foam) || s.resolution>256 ||
        !std::isfinite(cascade.displacementScale) || cascade.displacementScale<0 || cascade.displacementScale>10 ||
        !std::isfinite(cascade.choppiness) || cascade.choppiness<0 || cascade.choppiness>4)
      return WaterCascadeError::InvalidSettings;
    if(s.minimumWavelength<previousEnd) return WaterCascadeError::OverlappingBands;
    previousEnd=s.maximumWavelength;
  }
  return waterCascadeGpuBufferBytes(settings)>gpuBufferBudgetBytes?
      WaterCascadeError::MemoryBudget:WaterCascadeError::None;
}

bool generateWaterCascades(std::span<const WaterCascadeSettings> settings,
                          u64 gpuBufferBudgetBytes,std::vector<WaterCascadeSpectrum> &output) {
  if(validateWaterCascades(settings,gpuBufferBudgetBytes)!=WaterCascadeError::None) return false;
  std::vector<WaterCascadeSpectrum> result(settings.size());
  for(usize i=0;i<settings.size();++i) {
    result[i].settings=settings[i];
    auto band=settings[i].spectrum;
    // Spectrum API uses inclusive bounds. Give a shared boundary exclusively
    // to the longer-wave cascade, without shifting the stored authoring data.
    if(i+1<settings.size() && band.maximumWavelength==settings[i+1].spectrum.minimumWavelength)
      band.maximumWavelength=std::nextafter(band.maximumWavelength,0.0f);
    if(!generateWaterSpectrum(band,result[i].amplitudes)) return false;
  }
  output=std::move(result);
  return true;
}
} // namespace ae::renderer
