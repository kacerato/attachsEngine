#include "rhi/memory_allocator.h"
#include "rhi/resource.h"

#include <cmath>
#include <limits>

namespace ae::rhi {

bool isImageDescValid(const ImageDesc &desc) {
  const bool validMemoryClass = desc.memoryClass == MemoryClass::Texture ||
                                desc.memoryClass == MemoryClass::RenderTarget;
  return desc.width > 0 && desc.height > 0 && desc.format != VK_FORMAT_UNDEFINED &&
         desc.usage != 0 && desc.aspectMask != 0 && validMemoryClass;
}

bool isSamplerDescValid(const SamplerDesc &desc) {
  return std::isfinite(desc.minLod) && std::isfinite(desc.maxLod) &&
         std::isfinite(desc.maxAnisotropy) && desc.minLod >= 0.0f &&
         desc.maxLod >= desc.minLod && desc.maxAnisotropy >= 1.0f &&
         (!desc.enableAnisotropy || desc.maxAnisotropy > 1.0f);
}

bool isRgba8UploadValid(const ImageDesc &desc, u64 sourceSizeBytes) {
  constexpr VkImageUsageFlags requiredUsage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (!isImageDescValid(desc) || desc.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT ||
      (desc.usage & requiredUsage) != requiredUsage ||
      (desc.format != VK_FORMAT_R8G8B8A8_UNORM && desc.format != VK_FORMAT_R8G8B8A8_SRGB)) {
    return false;
  }
  const u64 pixels = static_cast<u64>(desc.width) * desc.height;
  return pixels <= std::numeric_limits<usize>::max() / 4 && sourceSizeBytes == pixels * 4;
}

} // namespace ae::rhi
