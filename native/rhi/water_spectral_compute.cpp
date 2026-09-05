#include "rhi/water_spectral_compute.h"
#include "rhi/shaders/water_spectrum_evolve_spirv.h"
#include "rhi/shaders/water_fft_inverse_spirv.h"
#include "rhi/shaders/water_foam_update_spirv.h"
#include "rhi/shaders/water_slope_pack_spirv.h"
#include "rhi/shaders/water_slope_pack_wide_spirv.h"
#include <cmath>
#include <cstring>

namespace ae::rhi {
bool VulkanWaterSpectralCompute::initialize(VulkanMemoryAllocator &allocator,
    const ComputeLimits &limits, const VkPhysicalDeviceLimits &physicalLimits,
    u32 resolution, float patchLength, std::span<const WaterSpectralMode> modes,VkPhysicalDevice physicalDevice,
    bool preferWideSlopes) {
  shutdown();
  if(physicalDevice==VK_NULL_HANDLE) return false;
  constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  const auto supports=[&](VkFormat candidate) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice,candidate,&properties);
    return (properties.optimalTilingFeatures&required)==required;
  };
  // Two slope scalars per texel, read once per cascade per water fragment. The
  // narrow format is preferred for bandwidth; the wide one is a real fallback
  // with its own kernel, not a silent quality change.
  const bool narrowSlopes=!preferWideSlopes && supports(VK_FORMAT_R16G16_SFLOAT);
  if(!narrowSlopes && !supports(VK_FORMAT_R32G32B32A32_SFLOAT)) return false;
  const u64 count=static_cast<u64>(resolution)*resolution;
  if(!allocator.isReady() || resolution<8 || resolution>256 ||
      (resolution&(resolution-1))!=0 || modes.size()!=count ||
      !std::isfinite(patchLength) || patchLength<=0 ||
      physicalLimits.maxComputeSharedMemorySize<8192 ||
      physicalLimits.maxStorageBufferRange<count*(sizeof(WaterSpectralSample)+sizeof(float))) return false;
  for(const auto &mode:modes)
    if(!std::isfinite(mode.real) || !std::isfinite(mode.imaginary) ||
       !std::isfinite(mode.angularFrequency) || mode.angularFrequency<0) return false;
  BufferDesc desc{};
  desc.sizeBytes=count*sizeof(WaterSpectralMode); desc.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  desc.cpuAccess=CpuAccess::SequentialWrite;
  if(!allocator.createBuffer(desc,&initial_) || !initial_.mappedData()) { shutdown(); return false; }
  std::memcpy(initial_.mappedData(),modes.data(),desc.sizeBytes);
  if(!allocator.flushBuffer(initial_)) { shutdown(); return false; }
  desc.sizeBytes=count*(sizeof(WaterSpectralSample)+sizeof(float));
  desc.cpuAccess=CpuAccess::None;
  desc.usage|=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  if(!allocator.createBuffer(desc,&output_)) { shutdown(); return false; }
  const ComputeBindingDesc bindings[]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
  ComputeKernelDesc kernel{};
  kernel.spirv=shaders::kWater_Spectrum_EvolveCompSpirv;
  kernel.spirvBytes=shaders::kWater_Spectrum_EvolveCompSpirvSize;
  kernel.bindings=bindings; kernel.bindingCount=2; kernel.pushConstantBytes=16;
  kernel.debugName="WaterSpectrumEvolve";
  if(!evolve_.initialize(allocator.device(),limits,kernel) ||
     !evolve_.writeBuffer(0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,initial_) ||
     !evolve_.writeBuffer(1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output_)) { shutdown(); return false; }
  kernel.spirv=shaders::kWater_Fft_InverseCompSpirv;
  kernel.spirvBytes=shaders::kWater_Fft_InverseCompSpirvSize;
  kernel.bindingCount=1; kernel.pushConstantBytes=8; kernel.debugName="WaterInverseFFT";
  if(!inverse_.initialize(allocator.device(),limits,kernel) ||
     !inverse_.writeBuffer(0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output_)) { shutdown(); return false; }
  kernel.spirv=shaders::kWater_Foam_UpdateCompSpirv;
  kernel.spirvBytes=shaders::kWater_Foam_UpdateCompSpirvSize;
  kernel.pushConstantBytes=32; kernel.debugName="WaterFoamHistory";
  if(!foam_.initialize(allocator.device(),limits,kernel) ||
     !foam_.writeBuffer(0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output_)) { shutdown(); return false; }
  ImageDesc image{}; image.width=resolution; image.height=resolution;
  image.format=narrowSlopes?VK_FORMAT_R16G16_SFLOAT:VK_FORMAT_R32G32B32A32_SFLOAT;
  image.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  image.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  if(!allocator.createImage(image,&slopes_) || !slopeSampler_.initialize(allocator.device(),{})) { shutdown(); return false; }
  const ComputeBindingDesc packing[]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
  kernel.spirv=narrowSlopes?shaders::kWater_Slope_PackCompSpirv:shaders::kWater_Slope_Pack_WideCompSpirv;
  kernel.spirvBytes=narrowSlopes?shaders::kWater_Slope_PackCompSpirvSize
                                :shaders::kWater_Slope_Pack_WideCompSpirvSize;
  kernel.bindings=packing; kernel.bindingCount=2; kernel.pushConstantBytes=0; kernel.debugName="WaterSlopePack";
  if(!pack_.initialize(allocator.device(),limits,kernel) ||
     !pack_.writeBuffer(0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output_) ||
     !pack_.writeImage(1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,slopes_.view(),VK_IMAGE_LAYOUT_GENERAL)) { shutdown(); return false; }
  resolution_=resolution; patchLength_=patchLength; narrowSlopes_=narrowSlopes;
  return true;
}

void VulkanWaterSpectralCompute::shutdown() {
  evolve_.shutdown(); inverse_.shutdown(); foam_.shutdown(); output_.reset(); initial_.reset(); resolution_=0;
  historyValid_=false; previousTime_=0;
  pack_.shutdown(); slopes_.reset(); slopeSampler_.shutdown(); slopesInitialized_=false; narrowSlopes_=false;
}

bool VulkanWaterSpectralCompute::record(VkCommandBuffer commandBuffer,float timeSeconds,
                                       const WaterFoamComputeParameters &foam) {
  if(!isReady() || commandBuffer==VK_NULL_HANDLE || !std::isfinite(timeSeconds)) return false;
  if(!std::isfinite(foam.choppiness) || foam.choppiness<0 ||
     !std::isfinite(foam.compressionThreshold) || foam.compressionThreshold<0 || foam.compressionThreshold>2 ||
     !std::isfinite(foam.growth) || foam.growth<0 || foam.growth>100 ||
     !std::isfinite(foam.decay) || foam.decay<0 || foam.decay>100) return false;
  // Also closes the previous frame's graphics/transfer reads before overwrite.
  cmdComputeBufferBarrier(commandBuffer,output_.handle(),0,output_.sizeBytes(),
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
  struct EvolutionPush { u32 size; float time, patch; u32 unused; };
  const EvolutionPush evolution{resolution_,timeSeconds,patchLength_,0};
  if(!evolve_.recordDispatch(commandBuffer,{(resolution_*resolution_+63)/64,1,1},&evolution,sizeof(evolution))) return false;
  for(u32 axis=0;axis<2;++axis) {
    cmdComputeMemoryBarrier(commandBuffer,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    const u32 push[]={resolution_,axis};
    if(!inverse_.recordDispatch(commandBuffer,{resolution_,1,1},push,sizeof(push))) return false;
  }
  cmdComputeMemoryBarrier(commandBuffer,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
  const bool reset=!historyValid_ || timeSeconds<previousTime_;
  struct FoamPush { u32 count; float dt,choppiness,threshold,growth,decay; u32 reset,unused; };
  const FoamPush push{resolution_*resolution_,reset?0.0f:timeSeconds-previousTime_,
    foam.choppiness,foam.compressionThreshold,foam.growth,foam.decay,reset?1u:0u,0};
  if(!foam_.recordDispatch(commandBuffer,{(resolution_*resolution_+63)/64,1,1},&push,sizeof(push))) return false;
  previousTime_=timeSeconds; historyValid_=true;
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  cmdComputeImageBarrier(commandBuffer,slopes_.handle(),range,
    slopesInitialized_?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,
    slopesInitialized_?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    slopesInitialized_?VK_ACCESS_SHADER_READ_BIT:0,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT);
  cmdComputeMemoryBarrier(commandBuffer,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  if(!pack_.recordDispatch(commandBuffer,{(resolution_+7)/8,(resolution_+7)/8,1})) return false;
  cmdComputeImageBarrier(commandBuffer,slopes_.handle(),range,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT);
  slopesInitialized_=true;
  cmdComputeBufferBarrier(commandBuffer,output_.handle(),0,output_.sizeBytes(),
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT);
  return true;
}
} // namespace ae::rhi
