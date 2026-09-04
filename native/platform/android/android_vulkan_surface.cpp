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

bool AndroidVulkanSurface::initialize(ANativeActivity *activity, ANativeWindow *window,
                                      u32 targetFramesPerSecond, bool useSwappy,
                                      bool allowBindless) {
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
  swappyScheduler_.configure(activity, window, targetFramesPerSecond, useSwappy);
  device_.setPresentationScheduler(useSwappy ? &swappyScheduler_ : nullptr);

  VkAndroidSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.window = window;
  if (vkCreateAndroidSurfaceKHR(device_.instance(), &createInfo, nullptr, &surface_) != VK_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao criar a surface Vulkan para a janela Android.");
    shutdown();
    return false;
  }

  if (!device_.initializeDevice(surface_, allowBindless)) {
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
                      "Surface Vulkan pronta: janela=%dx%d, imagens=%u..%u, fila=%u, transform=%u, suportados=%u.",
                      ANativeWindow_getWidth(window), ANativeWindow_getHeight(window),
                      capabilities.minImageCount, capabilities.maxImageCount,
                      device_.graphicsQueueFamily(), capabilities.currentTransform,
                      capabilities.supportedTransforms);

  if (!swapchain_.initialize(device_.handle(), device_.physicalDevice(), surface_,
                             device_.graphicsQueueFamily(),
                             static_cast<ae::u32>(ANativeWindow_getWidth(window)),
                             static_cast<ae::u32>(ANativeWindow_getHeight(window)),
                             useSwappy ? &swappyScheduler_ : nullptr)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha ao criar a swapchain de apresentação.");
    shutdown();
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag, "Swapchain pronta: %ux%u, %u imagens, formato=%u.",
                      swapchain_.width(), swapchain_.height(), swapchain_.imageCount(),
                      static_cast<unsigned>(swapchain_.imageFormat()));
  return true;
}

bool AndroidVulkanSurface::recreateSwapchain(ANativeWindow *window) {
  if (window == nullptr || !isReady()) return false;
  swappyScheduler_.setWindow(window);
  return swapchain_.recreate(static_cast<ae::u32>(ANativeWindow_getWidth(window)),
                             static_cast<ae::u32>(ANativeWindow_getHeight(window)));
}

void AndroidVulkanSurface::shutdown() {
  if (device_.handle() != VK_NULL_HANDLE) vkDeviceWaitIdle(device_.handle());
  swapchain_.shutdown();
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(device_.instance(), surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  device_.shutdown();
}

} // namespace ae::platform::android
