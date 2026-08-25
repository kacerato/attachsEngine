#include "platform/android/android_vulkan_surface.h"

#include <android/log.h>
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";
}

AndroidVulkanSurface::~AndroidVulkanSurface() {
  shutdown();
}

bool AndroidVulkanSurface::initialize(ANativeWindow *window) {
  if (window == nullptr || isReady()) return false;

  const char *extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
  };
  if (!device_.initializeInstance("Aether Editor", extensions, 2)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao criar a instância Vulkan com suporte Android.");
    return false;
  }

  VkAndroidSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.window = window;
  if (vkCreateAndroidSurfaceKHR(device_.instance(), &createInfo, nullptr, &surface_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao criar a surface Vulkan para a janela Android.");
    shutdown();
    return false;
  }

  if (!device_.initializeDevice(surface_)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Nenhuma GPU possui fila gráfica capaz de apresentar nesta surface.");
    shutdown();
    return false;
  }

  VkSurfaceCapabilitiesKHR capabilities{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_.physicalDevice(), surface_,
                                                &capabilities) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "A surface Vulkan foi criada, mas suas capacidades não puderam ser consultadas.");
    shutdown();
    return false;
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "Surface Vulkan pronta: janela=%dx%d, imagens=%u..%u, fila=%u.",
                      ANativeWindow_getWidth(window), ANativeWindow_getHeight(window),
                      capabilities.minImageCount, capabilities.maxImageCount,
                      device_.graphicsQueueFamily());
  return true;
}

void AndroidVulkanSurface::shutdown() {
  if (surface_ != VK_NULL_HANDLE) {
    if (device_.handle() != VK_NULL_HANDLE) vkDeviceWaitIdle(device_.handle());
    vkDestroySurfaceKHR(device_.instance(), surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  device_.shutdown();
}

} // namespace ae::platform::android
