#pragma once

#include "rhi/device.h"

struct ANativeWindow;

namespace ae::platform::android {

// Vertical slice do item 0.1.3: prova instance, device, fila de apresentação
// e surface Android. Swapchain e renderização pertencem ao próximo marco.
class AndroidVulkanSurface final {
public:
  AndroidVulkanSurface() = default;
  ~AndroidVulkanSurface();

  AndroidVulkanSurface(const AndroidVulkanSurface &) = delete;
  AndroidVulkanSurface &operator=(const AndroidVulkanSurface &) = delete;

  bool initialize(ANativeWindow *window);
  void shutdown();
  bool isReady() const { return surface_ != VK_NULL_HANDLE; }

private:
  rhi::VulkanDevice device_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};

} // namespace ae::platform::android
