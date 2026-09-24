#pragma once

#include "core/base.h"

#include <vulkan/vulkan.h>

namespace ae::rhi {

struct SamplerDesc {
  VkFilter minFilter = VK_FILTER_LINEAR;
  VkFilter magFilter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  VkSamplerAddressMode addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  float minLod = 0.0f;
  float maxLod = 0.0f;
  // Viés de LOD; limitado por VkPhysicalDeviceLimits::maxSamplerLodBias.
  float mipLodBias = 0.0f;
  bool enableAnisotropy = false;
  float maxAnisotropy = 1.0f;
  // Amostragem de comparacao (sampler2DShadow). O Adreno tem PCF em hardware:
  // uma unica busca faz o compare E o filtro bilinear 2x2. Comparar a mao no
  // shader gasta quatro buscas para o mesmo resultado e ainda interpola
  // profundidades ANTES de comparar, o que nem sequer e o filtro correto.
  bool enableCompare = false;
  VkCompareOp compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
};

bool isSamplerDescValid(const SamplerDesc &desc);

class VulkanSampler final {
public:
  VulkanSampler() = default;
  ~VulkanSampler();

  VulkanSampler(const VulkanSampler &) = delete;
  VulkanSampler &operator=(const VulkanSampler &) = delete;
  VulkanSampler(VulkanSampler &&other) noexcept;
  VulkanSampler &operator=(VulkanSampler &&other) noexcept;

  bool initialize(VkDevice device, const SamplerDesc &desc);
  void shutdown();
  bool isReady() const { return sampler_ != VK_NULL_HANDLE; }
  VkSampler handle() const { return sampler_; }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
};

} // namespace ae::rhi
