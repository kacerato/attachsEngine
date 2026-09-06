#include "platform/android/instanced_renderer.h"
#include <android/log.h>

namespace ae::platform::android {
namespace {
std::vector<rhi::WaterSpectralMode> makeModes(const renderer::WaterCascadeSpectrum &cascade) {
  const auto &s=cascade.settings.spectrum;
  std::vector<rhi::WaterSpectralMode> modes(cascade.amplitudes.size());
  for(u32 z=0;z<s.resolution;++z) for(u32 x=0;x<s.resolution;++x) {
    const int kx=x<=s.resolution/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(s.resolution);
    const int kz=z<=s.resolution/2?static_cast<int>(z):static_cast<int>(z)-static_cast<int>(s.resolution);
    const double k=6.2831853071795864769*std::hypot(kx,kz)/s.patchLength;
    const usize i=z*s.resolution+x;
    modes[i]={cascade.amplitudes[i].real(),cascade.amplitudes[i].imag(),
              static_cast<float>(std::sqrt(9.81*k*std::tanh(k*s.depth))),0};
  }
  return modes;
}

float displacementBound(std::span<const renderer::WaterCascadeSpectrum> spectra) {
  double result=0;
  for(const auto &cascade:spectra) {
    double heightBound=0;
    for(const auto amplitude:cascade.amplitudes)
      heightBound+=2*std::abs(amplitude)/cascade.amplitudes.size();
    const auto &settings=cascade.settings;
    result+=heightBound*settings.displacementScale*3*
      std::sqrt(1.0+8.0*settings.choppiness*settings.choppiness);
  }
  return static_cast<float>(result);
}
}

bool InstancedRenderer::createSpectralWaterResources() {
  spectralWaterCount_=0; spectralWaterBoundsExpansion_=0;
  waterSpectralClock_={};
  if(!spectralWaterEnabled_ || !waterSubpassActive_) return true;
  if(waterCascadeSettings_.empty()) {
    const auto defaults=renderer::defaultWaterCascadeSettings();
    waterCascadeSettings_.assign(defaults.begin(),defaults.end());
  }
  std::vector<renderer::WaterCascadeSpectrum> spectra;
  if(!renderer::generateWaterCascades(waterCascadeSettings_,waterCascadeBufferBudget_,spectra)) return false;
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(rhiDevice_->physicalDevice(),&properties);
  u32 familyCount=0;
  vkGetPhysicalDeviceQueueFamilyProperties(rhiDevice_->physicalDevice(),&familyCount,nullptr);
  std::vector<VkQueueFamilyProperties> families(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(rhiDevice_->physicalDevice(),&familyCount,families.data());
  bool ready=(families[rhiDevice_->graphicsQueueFamily()].queueFlags&VK_QUEUE_COMPUTE_BIT)!=0 &&
      properties.limits.maxPerStageDescriptorStorageBuffers>=5 &&
      properties.limits.maxPerStageDescriptorSamplers>=9 &&
      properties.limits.maxPerStageDescriptorSampledImages>=9;
  for(usize cascade=0;ready && cascade<spectra.size();++cascade) {
    const auto &s=spectra[cascade].settings.spectrum;
    const auto modes=makeModes(spectra[cascade]);
    ready=waterSpectralCompute_[cascade].initialize(*memoryAllocator_,rhiDevice_->computeLimits(),
      properties.limits,s.resolution,s.patchLength,modes,rhiDevice_->physicalDevice(),wideWaterSlopes_);
  }
  if(!ready) {
    for(auto &cascade:waterSpectralCompute_) cascade.shutdown();
    spectralWaterBoundsExpansion_=0;
    __android_log_print(ANDROID_LOG_WARN,"Aether.Android","[WaterFFT] unavailable; analytical provider retained");
    return true;
  }
  spectralWaterCount_=static_cast<u32>(spectra.size());
  spectralWaterBoundsExpansion_=displacementBound(spectra);
  ++waterSpectrumRevision_;
  __android_log_print(ANDROID_LOG_INFO,"Aether.Android",
    "[WaterFFT] spectral provider active cascades=%u buffer_bytes=%llu bounds=%.3f slopes=%s",
    spectralWaterCount_,static_cast<unsigned long long>(renderer::waterCascadeGpuBufferBytes(waterCascadeSettings_)),
    static_cast<double>(spectralWaterBoundsExpansion_),
    waterSpectralCompute_[0].usesNarrowSlopes()?"rg16f":"rgba32f(fallback)");
  return true;
}

bool InstancedRenderer::reconfigureWaterCascades(
    std::span<const renderer::WaterCascadeSettings> settings) {
  if(device_==VK_NULL_HANDLE || spectralWaterCount_==0 ||
     settings.size()!=spectralWaterCount_ ||
     renderer::validateWaterCascades(settings,waterCascadeBufferBudget_)!=renderer::WaterCascadeError::None)
    return false;
  for(usize index=0;index<settings.size();++index) {
    const auto &before=waterCascadeSettings_[index].spectrum;
    const auto &after=settings[index].spectrum;
    // Descriptor layout is independent of N, but the allocated buffers and
    // push-constant domain are not. Structural edits rebuild the scene later.
    if(before.resolution!=after.resolution || before.patchLength!=after.patchLength)
      return false;
  }

  std::vector<renderer::WaterCascadeSpectrum> nextSpectra, previousSpectra;
  if(!renderer::generateWaterCascades(settings,waterCascadeBufferBudget_,nextSpectra) ||
     !renderer::generateWaterCascades(waterCascadeSettings_,waterCascadeBufferBudget_,previousSpectra))
    return false;
  std::array<std::vector<rhi::WaterSpectralMode>,renderer::MaximumWaterCascades> nextModes;
  std::array<std::vector<rhi::WaterSpectralMode>,renderer::MaximumWaterCascades> previousModes;
  for(usize index=0;index<settings.size();++index) {
    nextModes[index]=makeModes(nextSpectra[index]);
    previousModes[index]=makeModes(previousSpectra[index]);
  }
  if(vkDeviceWaitIdle(device_)!=VK_SUCCESS) return false;
  for(usize index=0;index<settings.size();++index) {
    if(waterSpectralCompute_[index].updateModes(nextModes[index])) continue;
    bool rollback=true;
    for(usize restored=0;restored<index;++restored)
      rollback&=waterSpectralCompute_[restored].updateModes(previousModes[restored]);
    __android_log_print(ANDROID_LOG_ERROR,"Aether.Android",
      "[WaterFFT] reconfiguracao falhou na cascata=%zu rollback=%s.",
      index,rollback?"ok":"FALHOU");
    return false;
  }
  waterCascadeSettings_.assign(settings.begin(),settings.end());
  spectralWaterBoundsExpansion_=displacementBound(nextSpectra);
  ++waterSpectrumRevision_;
  __android_log_print(ANDROID_LOG_INFO,"Aether.Android",
    "[WaterFFT] espectro reconfigurado revisao=%llu vento=%.2f fetch=%.0f profundidade=%.1f swell=%.2f spread=%.2f.",
    static_cast<unsigned long long>(waterSpectrumRevision_),
    static_cast<double>(settings[0].spectrum.windSpeed),
    static_cast<double>(settings[0].spectrum.fetch),
    static_cast<double>(settings[0].spectrum.depth),
    static_cast<double>(settings[0].spectrum.swell),
    static_cast<double>(settings[0].spectrum.spread));
  return true;
}
}
