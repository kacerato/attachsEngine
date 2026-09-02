#pragma once

#include "rhi/device.h"

struct ANativeActivity;
struct ANativeWindow;

namespace ae::platform::android {

// Android implementation of the optional RHI presentation scheduler. Swappy
// owns pacing only when explicitly requested and successfully initialized;
// every failure falls back to direct FIFO presentation.
class AndroidSwappyScheduler final : public rhi::IVulkanPresentationScheduler {
public:
  void configure(ANativeActivity *activity, ANativeWindow *window, u32 targetFramesPerSecond,
                 bool requested);
  void setWindow(ANativeWindow *window) { window_ = window; }
  bool requested() const { return requested_; }
  bool active() const { return active_; }

  void requiredDeviceExtensions(VkPhysicalDevice physicalDevice,
                                std::vector<std::string> &extensions) override;
  void onQueueReady(VkDevice device, VkQueue queue, u32 queueFamilyIndex) override;
  void onSwapchainCreated(VkPhysicalDevice physicalDevice, VkDevice device,
                          VkSwapchainKHR swapchain) override;
  VkResult queuePresent(VkQueue queue, const VkPresentInfoKHR *presentInfo) override;
  void onSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain) override;
  void onDeviceDestroyed(VkDevice device) override;

private:
  ANativeActivity *activity_ = nullptr;
  ANativeWindow *window_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  u64 targetIntervalNs_ = 16'666'667;
  bool requested_ = false;
  bool active_ = false;
};

} // namespace ae::platform::android
