#include "rhi/resource.h"

#include <utility>

namespace ae::rhi {

VulkanSampler::~VulkanSampler() {
  shutdown();
}

VulkanSampler::VulkanSampler(VulkanSampler &&other) noexcept {
  *this = std::move(other);
}

VulkanSampler &VulkanSampler::operator=(VulkanSampler &&other) noexcept {
  if (this == &other) return *this;
  shutdown();
  device_ = other.device_;
  sampler_ = other.sampler_;
  other.device_ = VK_NULL_HANDLE;
  other.sampler_ = VK_NULL_HANDLE;
  return *this;
}

bool VulkanSampler::initialize(VkDevice device, const SamplerDesc &desc) {
  if (device == VK_NULL_HANDLE || sampler_ != VK_NULL_HANDLE || !isSamplerDescValid(desc)) {
    return false;
  }

  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = desc.magFilter;
  info.minFilter = desc.minFilter;
  info.mipmapMode = desc.mipmapMode;
  info.addressModeU = desc.addressU;
  info.addressModeV = desc.addressV;
  info.addressModeW = desc.addressW;
  info.minLod = desc.minLod;
  info.maxLod = desc.maxLod;
  info.anisotropyEnable = desc.enableAnisotropy ? VK_TRUE : VK_FALSE;
  info.maxAnisotropy = desc.maxAnisotropy;
  info.compareEnable = desc.enableCompare ? VK_TRUE : VK_FALSE;
  info.compareOp = desc.compareOp;
  info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  info.unnormalizedCoordinates = VK_FALSE;

  if (vkCreateSampler(device, &info, nullptr, &sampler_) != VK_SUCCESS) return false;
  device_ = device;
  return true;
}

void VulkanSampler::shutdown() {
  if (device_ != VK_NULL_HANDLE && sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
  }
  sampler_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
}

} // namespace ae::rhi
