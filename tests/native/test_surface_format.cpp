#include "harness.h"
#include "rhi/surface_format.h"

using namespace ae::rhi;

AE_TEST(surface_format_uses_rgba_srgb_when_bgra_is_not_advertised) {
  const VkSurfaceFormatKHR formats[] = {
      {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  VkSurfaceFormatKHR chosen{};
  AE_EXPECT_TRUE(chooseSurfaceFormat(formats, chosen), "advertised formats");
  AE_EXPECT_EQ(chosen.format, VK_FORMAT_R8G8B8A8_SRGB, "hardware output transfer on RGBA too");
}

AE_TEST(surface_format_preserves_bgra_preference_and_unorm_fallback) {
  const VkSurfaceFormatKHR formats[] = {
      {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  VkSurfaceFormatKHR chosen{};
  AE_EXPECT_TRUE(chooseSurfaceFormat(formats, chosen), "formats available");
  AE_EXPECT_EQ(chosen.format, VK_FORMAT_B8G8R8A8_SRGB, "existing preference unchanged");
  const VkSurfaceFormatKHR fallback[] = {
      {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  AE_EXPECT_TRUE(chooseSurfaceFormat(fallback, chosen), "UNORM-only devices remain supported");
  AE_EXPECT_EQ(chosen.format, VK_FORMAT_R8G8B8A8_UNORM, "explicit shader encoding remains available");
}

AE_TEST(surface_format_handles_empty_and_unrestricted_surfaces) {
  VkSurfaceFormatKHR chosen{};
  AE_EXPECT_TRUE(!chooseSurfaceFormat({}, chosen), "empty enumeration fails closed");
  const VkSurfaceFormatKHR unrestricted[] = {
      {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  AE_EXPECT_TRUE(chooseSurfaceFormat(unrestricted, chosen), "unrestricted surface");
  AE_EXPECT_EQ(chosen.format, VK_FORMAT_B8G8R8A8_SRGB, "existing unrestricted choice");
}
