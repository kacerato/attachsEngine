#include "rhi/memory_allocator.h"
#include "rhi/resource.h"

#include <cmath>
#include <limits>
#include <bit>
#include <algorithm>

namespace ae::rhi {

bool isImageDescValid(const ImageDesc &desc) {
  const bool validMemoryClass = desc.memoryClass == MemoryClass::Texture ||
                                desc.memoryClass == MemoryClass::RenderTarget;
  // VUID-VkImageCreateInfo-usage-00963: um anexo transitório só pode ser usado
  // como anexo. Recusar aqui transforma um erro de device em erro de contrato,
  // no lugar onde o autor da política consegue lê-lo.
  constexpr VkImageUsageFlags kNonAttachmentUsage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (desc.transient && ((desc.usage & kNonAttachmentUsage) != 0 || desc.mipLevels != 1)) {
    return false;
  }
  return desc.width > 0 && desc.height > 0 && desc.format != VK_FORMAT_UNDEFINED &&
         desc.usage != 0 && desc.aspectMask != 0 && validMemoryClass &&
         desc.mipLevels > 0 && desc.mipLevels <= static_cast<u32>(std::bit_width(std::max(desc.width, desc.height)));
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
  if (!isImageDescValid(desc) || desc.mipLevels != 1 || desc.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT ||
      (desc.usage & requiredUsage) != requiredUsage ||
      (desc.format != VK_FORMAT_R8G8B8A8_UNORM && desc.format != VK_FORMAT_R8G8B8A8_SRGB)) {
    return false;
  }
  const u64 pixels = static_cast<u64>(desc.width) * desc.height;
  return pixels <= std::numeric_limits<usize>::max() / 4 && sourceSizeBytes == pixels * 4;
}

u64 sampledMipByteSize(VkFormat format, u32 width, u32 height) {
  if (width == 0 || height == 0) return 0;
  u64 units = 0;
  u64 bytes = 0;
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB:
    units = static_cast<u64>(width) * height; bytes = 4; break;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    units = static_cast<u64>(width) * height; bytes = 8; break;
  case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    units = ((static_cast<u64>(width)+3)/4) * ((static_cast<u64>(height)+3)/4); bytes = 16; break;
  case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    units = ((static_cast<u64>(width)+5)/6) * ((static_cast<u64>(height)+5)/6); bytes = 16; break;
  case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    units = ((static_cast<u64>(width)+7)/8) * ((static_cast<u64>(height)+7)/8); bytes = 16; break;
  default: return 0;
  }
  return units <= std::numeric_limits<u64>::max()/bytes ? units*bytes : 0;
}

u64 sampledChainByteSize(const ImageDesc &desc) {
  if (!isImageDescValid(desc)) return 0;
  u64 total = 0;
  for (u32 mip = 0; mip < desc.mipLevels; ++mip) {
    const u64 bytes = sampledMipByteSize(desc.format, std::max(1u,desc.width>>mip), std::max(1u,desc.height>>mip));
    if (bytes == 0 || bytes > std::numeric_limits<u64>::max()-total) return 0;
    total += bytes;
  }
  return total;
}

} // namespace ae::rhi
