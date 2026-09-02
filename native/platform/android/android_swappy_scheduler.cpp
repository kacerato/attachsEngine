#include "platform/android/android_swappy_scheduler.h"

#include <android/log.h>
#include <android/native_activity.h>
#include <algorithm>
#include <array>
#include <swappy/swappyVk.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";

bool attach(ANativeActivity *activity, JNIEnv *&environment, bool &attachedHere) {
  if (activity == nullptr || activity->vm == nullptr) return false;
  attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (result == JNI_OK) return true;
  if (result != JNI_EDETACHED ||
      activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return false;
  attachedHere = true;
  return true;
}
} // namespace

void AndroidSwappyScheduler::configure(ANativeActivity *activity, ANativeWindow *window,
                                       u32 targetFramesPerSecond, bool requested) {
  activity_ = activity;
  window_ = window;
  requested_ = requested;
  active_ = false;
  targetIntervalNs_ = 1'000'000'000ULL /
                      static_cast<u64>(std::max(1u, targetFramesPerSecond));
}

void AndroidSwappyScheduler::requiredDeviceExtensions(
    VkPhysicalDevice physicalDevice, std::vector<std::string> &extensions) {
  extensions.clear();
  if (!requested_ || physicalDevice == VK_NULL_HANDLE) return;
  u32 availableCount = 0;
  if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableCount, nullptr) !=
          VK_SUCCESS ||
      availableCount == 0) return;
  std::vector<VkExtensionProperties> available(availableCount);
  if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableCount,
                                           available.data()) != VK_SUCCESS) return;
  u32 requiredCount = 0;
  SwappyVk_determineDeviceExtensions(physicalDevice, availableCount, available.data(),
                                     &requiredCount, nullptr);
  if (requiredCount == 0) return;
  std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE + 1>> storage(requiredCount);
  std::vector<char *> names(requiredCount);
  for (u32 index = 0; index < requiredCount; ++index) names[index] = storage[index].data();
  SwappyVk_determineDeviceExtensions(physicalDevice, availableCount, available.data(),
                                     &requiredCount, names.data());
  extensions.reserve(requiredCount);
  for (u32 index = 0; index < requiredCount; ++index) extensions.emplace_back(names[index]);
}

void AndroidSwappyScheduler::onQueueReady(VkDevice device, VkQueue queue, u32 queueFamilyIndex) {
  if (!requested_) return;
  device_ = device;
  SwappyVk_setQueueFamilyIndex(device, queue, queueFamilyIndex);
}

void AndroidSwappyScheduler::onSwapchainCreated(VkPhysicalDevice physicalDevice, VkDevice device,
                                                VkSwapchainKHR swapchain) {
  active_ = false;
  swapchain_ = swapchain;
  if (!requested_ || activity_ == nullptr || window_ == nullptr) return;
  JNIEnv *environment = nullptr;
  bool attachedHere = false;
  if (!attach(activity_, environment, attachedHere)) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[FramePacer] Swappy sem JNIEnv; fallback AChoreographer/FIFO.");
    return;
  }
  u64 refreshDuration = 0;
  const bool initialized = SwappyVk_initAndGetRefreshCycleDuration(
      environment, activity_->clazz, physicalDevice, device, swapchain, &refreshDuration);
  if (attachedHere) activity_->vm->DetachCurrentThread();
  if (!initialized) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[FramePacer] Swappy recusou a swapchain; fallback FIFO.");
    return;
  }
  SwappyVk_setWindow(device, swapchain, window_);
  SwappyVk_setAutoSwapInterval(false);
  SwappyVk_setAutoPipelineMode(false);
  SwappyVk_setSwapIntervalNS(device, swapchain, targetIntervalNs_);
  active_ = true;
  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "[FramePacer] Swappy ativo target_ms=%.3f refresh_ms=%.3f.",
                      static_cast<double>(targetIntervalNs_) / 1e6,
                      static_cast<double>(refreshDuration) / 1e6);
}

VkResult AndroidSwappyScheduler::queuePresent(VkQueue queue,
                                              const VkPresentInfoKHR *presentInfo) {
  return active_ ? SwappyVk_queuePresent(queue, presentInfo)
                 : vkQueuePresentKHR(queue, presentInfo);
}

void AndroidSwappyScheduler::onSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain) {
  if (active_ && swapchain_ == swapchain && swapchain != VK_NULL_HANDLE)
    SwappyVk_destroySwapchain(device, swapchain);
  if (swapchain_ == swapchain) {
    swapchain_ = VK_NULL_HANDLE;
    active_ = false;
  }
}

void AndroidSwappyScheduler::onDeviceDestroyed(VkDevice device) {
  if (requested_ && device != VK_NULL_HANDLE) SwappyVk_destroyDevice(device);
  device_ = VK_NULL_HANDLE;
  swapchain_ = VK_NULL_HANDLE;
  active_ = false;
}

} // namespace ae::platform::android
