#pragma once

#include "core/base.h"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace ae::rhi {

// Contrato do registro atual: combined image/sampler, somente no fragment.
// A extensão sozinha não garante nenhuma sub-feature nem capacidade útil.
inline u32 bindlessTextureCapacity(bool extensionEnabled,
    const VkPhysicalDeviceDescriptorIndexingFeatures &features,
    const VkPhysicalDeviceDescriptorIndexingProperties &limits) {
  if (!extensionEnabled || !features.shaderSampledImageArrayNonUniformIndexing ||
      !features.descriptorBindingPartiallyBound || !features.runtimeDescriptorArray ||
      !features.descriptorBindingSampledImageUpdateAfterBind) return 0;
  return std::min({limits.maxDescriptorSetUpdateAfterBindSamplers,
      limits.maxDescriptorSetUpdateAfterBindSampledImages,
      limits.maxPerStageDescriptorUpdateAfterBindSamplers,
      limits.maxPerStageDescriptorUpdateAfterBindSampledImages,
      limits.maxPerStageUpdateAfterBindResources,
      limits.maxUpdateAfterBindDescriptorsInAllPools});
}

inline VkPhysicalDeviceDescriptorIndexingFeatures enabledBindlessTextureFeatures(bool enabled) {
  VkPhysicalDeviceDescriptorIndexingFeatures result{};
  result.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  result.shaderSampledImageArrayNonUniformIndexing = enabled;
  result.descriptorBindingPartiallyBound = enabled;
  result.runtimeDescriptorArray = enabled;
  result.descriptorBindingSampledImageUpdateAfterBind = enabled;
  // UPDATE_UNUSED_WHILE_PENDING não é usado pelo layout: não solicitá-lo.
  return result;
}

} // namespace ae::rhi
