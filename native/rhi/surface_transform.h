#pragma once

#include <vulkan/vulkan.h>

namespace ae::rhi {

// Clip-space transform, row-major. Kept separate from Android and GPU calls
// so extent/projection agreement can be regression-tested without a device.
struct SurfaceTransform {
  float xx = 1.0f;
  float xy = 0.0f;
  float yx = 0.0f;
  float yy = 1.0f;
  bool swapsAxes = false;
};

inline bool describeSurfaceTransform(VkSurfaceTransformFlagBitsKHR flags, SurfaceTransform &out) {
  switch (flags) {
  case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR: out = {1, 0, 0, 1, false}; break;
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR: out = {0, -1, 1, 0, true}; break;
  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR: out = {-1, 0, 0, -1, false}; break;
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR: out = {0, 1, -1, 0, true}; break;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR: out = {-1, 0, 0, 1, false}; break;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR: out = {0, -1, -1, 0, true}; break;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR: out = {1, 0, 0, -1, false}; break;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR: out = {0, 1, 1, 0, true}; break;
  default: return false; // INHERIT does not describe a concrete projection.
  }
  return true;
}

inline VkExtent2D transformSurfaceExtent(VkExtent2D extent, const SurfaceTransform &transform) {
  return transform.swapsAxes ? VkExtent2D{extent.height, extent.width} : extent;
}

} // namespace ae::rhi
