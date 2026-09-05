#pragma once
#include "rhi/compute.h"
#include "rhi/resource.h"
#include <span>

namespace ae::rhi {
struct WaterSpectralMode final { float real=0, imaginary=0, angularFrequency=0, reserved=0; };
struct WaterSpectralSample final {
  float height, displacementX, displacementZ, slopeX;
  float slopeZ, displacementXX, displacementXZ, displacementZZ;
};
static_assert(sizeof(WaterSpectralMode)==16 && sizeof(WaterSpectralSample)==32);
struct WaterFoamComputeParameters final {
  float choppiness=1, compressionThreshold=.8f, growth=4, decay=.5f;
};

// One cascade, on a single queue family. Caller owns submission/fences; never
// initialize or destroy while recorded work is in flight. No per-frame CPU FFT.
class VulkanWaterSpectralCompute final {
public:
  bool initialize(VulkanMemoryAllocator &allocator, const ComputeLimits &limits,
                  const VkPhysicalDeviceLimits &physicalLimits, u32 resolution,
                  float patchLength, std::span<const WaterSpectralMode> modes,
                  VkPhysicalDevice physicalDevice, bool preferWideSlopes = false);
  void shutdown();
  bool record(VkCommandBuffer commandBuffer, float timeSeconds,
              const WaterFoamComputeParameters &foam = {});
  const VulkanBuffer &output() const noexcept { return output_; }
  VkImageView slopeView() const noexcept { return slopes_.view(); }
  VkSampler slopeSampler() const noexcept { return slopeSampler_.handle(); }
  bool isReady() const noexcept { return resolution_!=0; }
  // False means the device lacked half-float storage images and the wide
  // fallback is running: same image, four times the sampling bandwidth.
  bool usesNarrowSlopes() const noexcept { return narrowSlopes_; }
private:
  VulkanBuffer initial_, output_;
  VulkanComputeKernel evolve_, inverse_, foam_, pack_;
  VulkanImage slopes_;
  VulkanSampler slopeSampler_;
  bool slopesInitialized_=false;
  bool narrowSlopes_=false;
  u32 resolution_=0;
  float patchLength_=1;
  float previousTime_=0;
  bool historyValid_=false;
};
} // namespace ae::rhi
