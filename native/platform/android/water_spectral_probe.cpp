#include "platform/android/water_spectral_probe.h"
#include "rhi/water_spectral_compute.h"
#include "renderer/water_fft.h"
#include "renderer/water_foam.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::platform::android {
bool runWaterSpectralProbe(rhi::VulkanDevice &device) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(device.physicalDevice(),&properties);
  u32 familyCount=0;
  vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice(),&familyCount,nullptr);
  std::vector<VkQueueFamilyProperties> families(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice(),&familyCount,families.data());
  if(!(families[device.graphicsQueueFamily()].queueFlags&VK_QUEUE_COMPUTE_BIT)) return false;
  auto &allocator=device.memoryAllocator();
  for(u32 resolution:{8u,128u,256u}) {
    renderer::WaterSpectrumSettings settings;
    settings.resolution=resolution;
    std::vector<std::complex<float>> initial;
    renderer::WaterSpectralField reference;
    if(!renderer::generateWaterSpectrum(settings,initial) || !reference.initialize(settings)) return false;
    std::vector<rhi::WaterSpectralMode> modes(initial.size());
    for(u32 z=0;z<resolution;++z) for(u32 x=0;x<resolution;++x) {
      const int kx=x<=resolution/2?static_cast<int>(x):static_cast<int>(x)-static_cast<int>(resolution);
      const int kz=z<=resolution/2?static_cast<int>(z):static_cast<int>(z)-static_cast<int>(resolution);
      const double k=6.2831853071795864769*std::hypot(kx,kz)/settings.patchLength;
      const usize i=z*resolution+x;
      modes[i]={initial[i].real(),initial[i].imag(),static_cast<float>(std::sqrt(9.81*k*std::tanh(k*settings.depth))),0};
    }
    rhi::VulkanWaterSpectralCompute simulation;
    if(!simulation.initialize(allocator,device.computeLimits(),properties.limits,resolution,settings.patchLength,modes,device.physicalDevice())) return false;
    rhi::VulkanBuffer readback;
    rhi::BufferDesc desc{};
    desc.sizeBytes=simulation.output().sizeBytes(); desc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    desc.cpuAccess=rhi::CpuAccess::Random; desc.preferDeviceMemory=false;
    if(!allocator.createBuffer(desc,&readback) || !readback.mappedData()) return false;
    // Context is destroyed first so any submitted work completes before buffers.
    rhi::VulkanComputeContext context;
    if(!context.initialize(device.handle(),device.graphicsQueue(),device.graphicsQueueFamily())) return false;
    float worst=0;
    std::vector<float> foamReference(initial.size(),0);
    float previousTime=0;
    for(float time:{0.0f,1.0f,60.0f}) {
      auto command=context.begin();
      if(!command || !simulation.record(command,time)) return false;
      const VkBufferCopy copy{0,0,desc.sizeBytes};
      vkCmdCopyBuffer(command,simulation.output().handle(),readback.handle(),1,&copy);
      rhi::cmdComputeBufferBarrier(command,readback.handle(),0,desc.sizeBytes,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_HOST_READ_BIT);
      if(!context.submit() || !context.wait() || !allocator.invalidateBuffer(readback) || !reference.update(time)) return false;
      const auto *samples=static_cast<const rhi::WaterSpectralSample*>(readback.mappedData());
      const auto *foam= reinterpret_cast<const float*>(samples+initial.size());
      for(usize i=0;i<initial.size();++i) {
        const auto &sample=samples[i];
        const float actual[]={sample.height,sample.displacementX,sample.displacementZ,sample.slopeX,
          sample.slopeZ,sample.displacementXX,sample.displacementXZ,sample.displacementZZ};
        for(u32 channel=0;channel<8;++channel) {
          const float expected=channel==0?reference.heights()[i].real():
            reference.channel(static_cast<renderer::WaterSpectralField::Channel>(channel-1))[i].real();
          const float error=std::abs(actual[channel]-expected);
          if(!std::isfinite(error)) return false;
          worst=std::max(worst,error);
        }
        foamReference[i]=renderer::evolveWaterFoam(foamReference[i],reference.jacobian(i,1),time-previousTime,{});
        const float foamError=std::abs(foam[i]-foamReference[i]);
        if(!std::isfinite(foamError) || foam[i]<0 || foam[i]>1) return false;
        worst=std::max(worst,foamError);
      }
      previousTime=time;
    }
    __android_log_print(worst<.001f?ANDROID_LOG_INFO:ANDROID_LOG_ERROR,"Aether.WaterProbe",
      "[WaterFFT] resolution=%u samples=%u channels=9 times=3 max_error=%.8f passed=%s",
      resolution,resolution*resolution,static_cast<double>(worst),worst<.001f?"true":"false");
    if(worst>=.001f) return false;
  }
  return true;
}
}
