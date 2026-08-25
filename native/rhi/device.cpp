// Implementação real de VulkanDevice. Não exercitada por teste nesta
// máquina — sem GPU nem loader Vulkan funcional (ver nota em device.h).
// Escrita para compilar e para ser correta contra a spec, revisada
// manualmente; validação em hardware fica para quando houver device real.
#include "rhi/device.h"

#include <cstring>

namespace ae::rhi {

VulkanDevice::~VulkanDevice() {
  shutdown();
}

void VulkanDevice::shutdown() {
  // Ordem de destruição importa: device antes de instance. Nenhum comando
  // pendente deve existir aqui — é responsabilidade do caller ter esperado
  // (vkDeviceWaitIdle) antes de destruir o device.
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
  physicalDevice_ = VK_NULL_HANDLE;
  graphicsQueueFamily_ = 0;
}

bool VulkanDevice::initialize(const char *appName) {
  if (!initializeInstance(appName, nullptr, 0)) return false;
  if (!initializeDevice(VK_NULL_HANDLE)) {
    shutdown();
    return false;
  }
  return true;
}

bool VulkanDevice::initializeInstance(const char *appName,
                                      const char *const *requiredExtensions,
                                      u32 requiredExtensionCount) {
  if (instance_ != VK_NULL_HANDLE || appName == nullptr) return false;

  u32 availableCount = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr) != VK_SUCCESS) {
    return false;
  }
  VkExtensionProperties available[64];
  availableCount = availableCount > 64 ? 64 : availableCount;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available) != VK_SUCCESS) {
    return false;
  }
  for (u32 required = 0; required < requiredExtensionCount; ++required) {
    bool found = false;
    for (u32 candidate = 0; candidate < availableCount; ++candidate) {
      if (std::strcmp(requiredExtensions[required], available[candidate].extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = appName;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Aether";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  // Vulkan 1.1 é o piso de hardware definido pelo plano. Recursos 1.3 são
  // habilitados por capability profile, nunca presumidos na criação base.
  appInfo.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo instInfo{};
  instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instInfo.pApplicationInfo = &appInfo;
  instInfo.enabledExtensionCount = requiredExtensionCount;
  instInfo.ppEnabledExtensionNames = requiredExtensions;

  if (vkCreateInstance(&instInfo, nullptr, &instance_) != VK_SUCCESS) {
    return false;
  }

  return true;
}

bool VulkanDevice::initializeDevice(VkSurfaceKHR presentationSurface) {
  if (instance_ == VK_NULL_HANDLE || device_ != VK_NULL_HANDLE) return false;

  u32 physCount = 0;
  vkEnumeratePhysicalDevices(instance_, &physCount, nullptr);
  if (physCount == 0) {
    return false;
  }
  // Aloca em pilha para o caso comum (poucos GPUs físicas); um dispositivo
  // mobile real tem exatamente uma.
  VkPhysicalDevice candidates[8];
  physCount = physCount > 8 ? 8 : physCount;
  vkEnumeratePhysicalDevices(instance_, &physCount, candidates);
  bool found = false;
  for (u32 physicalIndex = 0; physicalIndex < physCount && !found; ++physicalIndex) {
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidates[physicalIndex], &queueFamilyCount, nullptr);
    VkQueueFamilyProperties queueFamilies[16];
    queueFamilyCount = queueFamilyCount > 16 ? 16 : queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(candidates[physicalIndex], &queueFamilyCount, queueFamilies);

    for (u32 queueIndex = 0; queueIndex < queueFamilyCount; ++queueIndex) {
      if ((queueFamilies[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
      VkBool32 supportsPresentation = VK_TRUE;
      if (presentationSurface != VK_NULL_HANDLE) {
        if (vkGetPhysicalDeviceSurfaceSupportKHR(candidates[physicalIndex], queueIndex,
                                                presentationSurface,
                                                &supportsPresentation) != VK_SUCCESS) {
          supportsPresentation = VK_FALSE;
        }
      }
      if (supportsPresentation == VK_TRUE) {
        physicalDevice_ = candidates[physicalIndex];
        graphicsQueueFamily_ = queueIndex;
        found = true;
        break;
      }
    }
  }
  if (!found) return false;

  float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo{};
  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.queueFamilyIndex = graphicsQueueFamily_;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &priority;

  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queueInfo;
  const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  if (presentationSurface != VK_NULL_HANDLE) {
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;
  }

  if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS) {
    return false;
  }

  return true;
}

} // namespace ae::rhi
