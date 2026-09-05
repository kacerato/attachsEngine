#include "platform/android/instanced_renderer.h"
#include <android/log.h>

namespace ae::platform::android {
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
      properties.limits.maxPerStageDescriptorStorageBuffers>=4 &&
      properties.limits.maxPerStageDescriptorSamplers>=9 &&
      properties.limits.maxPerStageDescriptorSampledImages>=9;
  for(usize cascade=0;ready && cascade<spectra.size();++cascade) {
    const auto &s=spectra[cascade].settings.spectrum;
    const auto &initial=spectra[cascade].amplitudes;
    std::vector<rhi::WaterSpectralMode> modes(initial.size());
    double heightBound=0;
    for(u32 z=0;z<s.resolution;++z) for(u32 x=0;x<s.resolution;++x) {
      const int kx=x<=s.resolution/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(s.resolution);
      const int kz=z<=s.resolution/2?static_cast<int>(z):static_cast<int>(z)-static_cast<int>(s.resolution);
      const double k=6.2831853071795864769*std::hypot(kx,kz)/s.patchLength;
      const usize i=z*s.resolution+x;
      modes[i]={initial[i].real(),initial[i].imag(),static_cast<float>(std::sqrt(9.81*k*std::tanh(k*s.depth))),0};
      heightBound+=2*std::abs(initial[i])/initial.size();
    }
    const auto &settings=spectra[cascade].settings;
    // Triangle-inequality bound valid at every phase, not one captured frame.
    // Reserve the whole live-control envelope: gain <=3, choppiness gain <=2.
    spectralWaterBoundsExpansion_+=static_cast<float>(heightBound*settings.displacementScale*3*
      std::sqrt(1.0+8.0*settings.choppiness*settings.choppiness));
    ready=waterSpectralCompute_[cascade].initialize(*memoryAllocator_,rhiDevice_->computeLimits(),
      properties.limits,s.resolution,s.patchLength,modes,rhiDevice_->physicalDevice());
  }
  if(!ready) {
    for(auto &cascade:waterSpectralCompute_) cascade.shutdown();
    spectralWaterBoundsExpansion_=0;
    __android_log_print(ANDROID_LOG_WARN,"Aether.Android","[WaterFFT] unavailable; analytical provider retained");
    return true;
  }
  spectralWaterCount_=static_cast<u32>(spectra.size());
  __android_log_print(ANDROID_LOG_INFO,"Aether.Android",
    "[WaterFFT] experimental provider cascades=%u buffer_bytes=%llu bounds=%.3f slopes=%s",
    spectralWaterCount_,static_cast<unsigned long long>(renderer::waterCascadeGpuBufferBytes(waterCascadeSettings_)),
    static_cast<double>(spectralWaterBoundsExpansion_),
    waterSpectralCompute_[0].usesNarrowSlopes()?"rg16f":"rgba32f(fallback)");
  return true;
}
}
