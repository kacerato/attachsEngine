#pragma once

#include <span>
#include <vulkan/vulkan.h>

namespace ae::rhi {

// Shaders emit linear color; an sRGB attachment performs the output transfer.
// When only UNORM is available the renderer's existing explicit encoding is
// retained. Do not depend on the driver's enumeration order or channel order.
inline bool chooseSurfaceFormat(std::span<const VkSurfaceFormatKHR> formats,
                                VkSurfaceFormatKHR &out) {
  if (formats.empty()) return false;
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
    out = {VK_FORMAT_B8G8R8A8_SRGB, formats[0].colorSpace};
    return true;
  }
  constexpr VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
                                   VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
  for (VkFormat candidate : preferred) {
    for (const auto &format : formats) {
      if (format.format == candidate && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        out = format;
        return true;
      }
    }
  }
  out = formats.front();
  return true;
}

} // namespace ae::rhi
