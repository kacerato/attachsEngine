// Implementação real de VulkanDevice/VulkanSwapchain. VulkanDevice não é
// exercitada por teste nesta máquina — sem GPU nem loader Vulkan funcional
// (ver nota em device.h) — escrita para compilar e para ser correta contra
// a spec, revisada manualmente. VulkanSwapchain (item 5.2 do plano de
// lacunas) foi validada em execução real no shell Android — ver
// docs/ESTADO.md.
#include "rhi/device.h"

#include <algorithm>
#include <cstring>
#include "rhi/descriptor_indexing_support.h"
#include <vector>

// AETHER_VULKAN_VALIDATION é definida por device.h (controla o layout da
// própria classe VulkanDevice) — aqui só inclui <android/log.h> quando o
// bloco está de fato ativo; o host não tem esse header (ver nota em device.h).
#if AETHER_VULKAN_VALIDATION
#include <android/log.h>
#endif

namespace ae::rhi {

#if AETHER_VULKAN_VALIDATION
namespace {
constexpr const char *kValidationLogTag = "Aether.Vulkan";

// Item 2.1.6 do plano: mensagens de VK_LAYER_KHRONOS_validation/debug_utils
// chegam aqui (build debug apenas — ver initializeInstance) e são roteadas
// para o logcat, para aparecerem junto do resto do log do shell em vez de um
// canal separado que ninguém olha.
VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void * /*userData*/) {
  const int priority = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                           ? ANDROID_LOG_ERROR
                       : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                           ? ANDROID_LOG_WARN
                       : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? ANDROID_LOG_INFO
                                                                                   : ANDROID_LOG_VERBOSE;
  __android_log_print(priority, kValidationLogTag, "[%s] %s",
                      callbackData->pMessageIdName != nullptr ? callbackData->pMessageIdName : "?",
                      callbackData->pMessage != nullptr ? callbackData->pMessage : "");
  // VK_FALSE: nunca abortar a chamada Vulkan que gerou o aviso — a camada de
  // validação já é opt-in só em debug; abortar a chamada mudaria o
  // comportamento observável do app entre build debug e release, o oposto
  // do que uma ferramenta de diagnóstico deve fazer.
  return VK_FALSE;
}
}  // namespace
#endif

VulkanSwapchain::~VulkanSwapchain() {
  shutdown();
}

void VulkanSwapchain::destroySwapchainObjects() {
  for (u32 i = 0; i < imageCount_; ++i) {
    if (imageViews_[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, imageViews_[i], nullptr);
      imageViews_[i] = VK_NULL_HANDLE;
    }
  }
  imageCount_ = 0;
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {0, 0};
}

void VulkanSwapchain::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    // O caller (AndroidVulkanSurface/shell) é responsável por já ter
    // esperado o device ficar ocioso antes de chamar shutdown — mesma
    // invariante documentada em ISwapchain::recreate. Repetir aqui a espera
    // seria mascarar um shutdown fora de ordem em vez de expor o bug.
    destroySwapchainObjects();
    if (inFlightFence_ != VK_NULL_HANDLE) {
      vkDestroyFence(device_, inFlightFence_, nullptr);
      inFlightFence_ = VK_NULL_HANDLE;
    }
    for (u32 i = 0; i < kMaxSwapchainImages; ++i) {
      if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
        renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
      }
    }
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
      imageAvailableSemaphore_ = VK_NULL_HANDLE;
    }
  }
  device_ = VK_NULL_HANDLE;
  physicalDevice_ = VK_NULL_HANDLE;
  surface_ = VK_NULL_HANDLE;
  graphicsQueue_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {0, 0};
}

bool VulkanSwapchain::initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkSurfaceKHR surface, u32 graphicsQueueFamily,
                                 u32 width, u32 height) {
  if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE ||
      surface == VK_NULL_HANDLE) {
    return false;
  }
  device_ = device;
  physicalDevice_ = physicalDevice;
  surface_ = surface;
  graphicsQueueFamily_ = graphicsQueueFamily;
  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

  // renderFinishedSemaphores_ não é criado aqui — depende de imageCount_,
  // só conhecido depois de recreate() (abaixo) consultar a swapchain real.
  VkSemaphoreCreateInfo semInfo{};
  semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  if (vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS) {
    shutdown();
    return false;
  }
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // primeiro wait não deve bloquear
  if (vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  if (!recreate(width, height)) {
    shutdown();
    return false;
  }
  return true;
}

bool VulkanSwapchain::recreate(u32 newWidth, u32 newHeight) {
  if (device_ == VK_NULL_HANDLE) return false;

  // Nenhum comando pendente pode referenciar a swapchain antiga neste ponto
  // — mesma invariante documentada em ISwapchain::recreate (device.h):
  // destruir cedo demais é a causa mais comum de crash em resize/rotação em
  // apps Vulkan mobile. Um único frame em voo (item 5.2) simplifica isso: a
  // fence já garante que o frame anterior terminou antes do caller chamar
  // recreate (via acquireNextImage/present retornando OutOfDate).
  vkDeviceWaitIdle(device_);

  VkSurfaceCapabilitiesKHR caps{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps) != VK_SUCCESS) {
    return false;
  }

  u32 formatCount = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr) !=
      VK_SUCCESS) {
    return false;
  }
  if (formatCount == 0) return false;
  VkSurfaceFormatKHR formats[32];
  formatCount = std::min(formatCount, 32u);
  const VkResult formatsResult =
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats);
  if (formatsResult != VK_SUCCESS && formatsResult != VK_INCOMPLETE) return false;
  VkSurfaceFormatKHR chosen = formats[0];
  if (formatCount == 1 && chosen.format == VK_FORMAT_UNDEFINED) {
    chosen.format = VK_FORMAT_B8G8R8A8_SRGB;
    chosen.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }
  for (u32 i = 0; i < formatCount; ++i) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = formats[i];
      break;
    }
  }

  // currentExtent == 0xFFFFFFFF é o sinal (spec Vulkan) de que a surface
  // permite ao app escolher o extent dentro de min/max — acontece em vários
  // drivers Android. Fora isso, currentExtent é a fonte de verdade, não o
  // width/height que o SO reportou (podem divergir por 1px em alguns
  // compositores; a spec é clara que currentExtent manda quando definido).
  VkExtent2D extent = caps.currentExtent;
  if (extent.width == 0xFFFFFFFFu) {
    extent.width = std::clamp(newWidth, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(newHeight, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
  if (extent.width == 0 || extent.height == 0) {
    // Janela minimizada/surface sem área — recriar não faz sentido agora;
    // o caller deve tentar de novo quando a janela voltar a ter tamanho.
    return false;
  }

  SurfaceTransform transform{};
  VkSurfaceTransformFlagBitsKHR preTransform = caps.currentTransform;
  if (!describeSurfaceTransform(preTransform, transform)) {
    if ((caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0) return false;
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }
  // preTransform=currentTransform promises pre-rotated pixels. The image must
  // use the natural extent, while projection uses the visible display extent.
  // Using landscape dimensions for both made the cube appear flattened.
  extent = transformSurfaceExtent(extent, transform);

  if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0 ||
      caps.minImageCount > kMaxSwapchainImages) {
    return false;
  }

  u32 desiredImageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0) {
    desiredImageCount = std::min(desiredImageCount, caps.maxImageCount);
  }
  desiredImageCount = std::min(desiredImageCount, kMaxSwapchainImages);

  constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaCandidates[] = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };
  VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  bool foundCompositeAlpha = false;
  for (VkCompositeAlphaFlagBitsKHR candidate : compositeAlphaCandidates) {
    if ((caps.supportedCompositeAlpha & candidate) != 0) {
      compositeAlpha = candidate;
      foundCompositeAlpha = true;
      break;
    }
  }
  if (!foundCompositeAlpha) return false;

  VkSwapchainKHR oldSwapchain = swapchain_;

  VkSwapchainCreateInfoKHR swapchainInfo{};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = surface_;
  swapchainInfo.minImageCount = desiredImageCount;
  swapchainInfo.imageFormat = chosen.format;
  swapchainInfo.imageColorSpace = chosen.colorSpace;
  swapchainInfo.imageExtent = extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.preTransform = preTransform;
  swapchainInfo.compositeAlpha = compositeAlpha;
  swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // sempre suportado, vsync — ponto de partida seguro
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain = oldSwapchain;

  VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
  const VkResult createResult = vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &newSwapchain);

  if (createResult != VK_SUCCESS) {
    // A swapchain antiga NÃO é aposentada quando a criação falha e continua
    // sendo a única opção válida. Destruí-la aqui transformaria uma falha
    // recuperável de resize em perda total do renderer.
    return false;
  }

  u32 actualImageCount = 0;
  if (vkGetSwapchainImagesKHR(device_, newSwapchain, &actualImageCount, nullptr) != VK_SUCCESS ||
      actualImageCount == 0 || actualImageCount > kMaxSwapchainImages) {
    vkDestroySwapchainKHR(device_, newSwapchain, nullptr);
    // vkCreateSwapchainKHR teve sucesso: oldSwapchain foi aposentada e não
    // pode voltar a apresentar. Limpa os objetos antigos para que isReady()
    // exponha a perda em vez de permitir uso de handles inválidos.
    destroySwapchainObjects();
    return false;
  }

  VkImage newImages[kMaxSwapchainImages]{};
  VkImageView newImageViews[kMaxSwapchainImages]{};
  u32 queriedImageCount = actualImageCount;
  if (vkGetSwapchainImagesKHR(device_, newSwapchain, &queriedImageCount, newImages) != VK_SUCCESS ||
      queriedImageCount != actualImageCount) {
    vkDestroySwapchainKHR(device_, newSwapchain, nullptr);
    destroySwapchainObjects();
    return false;
  }

  for (u32 i = 0; i < actualImageCount; ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = newImages[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = chosen.format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &newImageViews[i]) != VK_SUCCESS) {
      for (u32 created = 0; created < i; ++created) {
        vkDestroyImageView(device_, newImageViews[created], nullptr);
      }
      vkDestroySwapchainKHR(device_, newSwapchain, nullptr);
      destroySwapchainObjects();
      return false;
    }
  }

  // O caller precisa ter destruído framebuffers que referenciem as views
  // antigas antes de entrar em recreate(). Depois dessa troca, os objetos
  // antigos podem ser liberados e os novos publicados de forma atômica.
  destroySwapchainObjects();
  swapchain_ = newSwapchain;
  format_ = chosen.format;
  extent_ = extent;
  surfaceTransform_ = transform;
  imageCount_ = actualImageCount;
  for (u32 i = 0; i < imageCount_; ++i) {
    images_[i] = newImages[i];
    imageViews_[i] = newImageViews[i];
  }

  // Item 2.1.6 (achado via validation layer): um renderFinishedSemaphore_
  // por imagem, criado/recriado aqui porque imageCount_ só é conhecido
  // depois da troca acima — destrói e recria todos mesmo que a contagem não
  // tenha mudado (recreate() já passou por vkDeviceWaitIdle no topo desta
  // função, então nenhum comando pendente pode estar referenciando os
  // semáforos antigos neste ponto).
  for (u32 i = 0; i < kMaxSwapchainImages; ++i) {
    if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
      renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
    }
  }
  VkSemaphoreCreateInfo renderFinishedSemInfo{};
  renderFinishedSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (u32 i = 0; i < imageCount_; ++i) {
    if (vkCreateSemaphore(device_, &renderFinishedSemInfo, nullptr, &renderFinishedSemaphores_[i]) !=
        VK_SUCCESS) {
      return false;
    }
  }

  return true;
}

SwapchainStatus VulkanSwapchain::acquireNextImage(u32 *outImageIndex) {
  if (outImageIndex == nullptr || !isReady() ||
      vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    return SwapchainStatus::FatalError;
  }

  u32 imageIndex = 0;
  const VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                imageAvailableSemaphore_, VK_NULL_HANDLE,
                                                &imageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) return SwapchainStatus::OutOfDateMustRecreate;
  if (result == VK_ERROR_SURFACE_LOST_KHR) return SwapchainStatus::SurfaceLost;
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return SwapchainStatus::FatalError;

  // Fence só é resetada depois de sabermos que vamos de fato submeter este
  // frame — resetar antes e depois falhar em acquire deixaria a fence
  // sinalizada errado (o próximo acquireNextImage não esperaria nada).
  if (vkResetFences(device_, 1, &inFlightFence_) != VK_SUCCESS) {
    return SwapchainStatus::FatalError;
  }

  *outImageIndex = imageIndex;
  return result == VK_SUBOPTIMAL_KHR ? SwapchainStatus::SuboptimalNeedsRecreate : SwapchainStatus::Ok;
}

SwapchainStatus VulkanSwapchain::present(u32 imageIndex) {
  if (imageIndex >= imageCount_) return SwapchainStatus::FatalError;
  const VkSemaphore waitSemaphore = renderFinishedSemaphores_[imageIndex];
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &waitSemaphore;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain_;
  presentInfo.pImageIndices = &imageIndex;

  const VkResult result = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) return SwapchainStatus::OutOfDateMustRecreate;
  if (result == VK_ERROR_SURFACE_LOST_KHR) return SwapchainStatus::SurfaceLost;
  if (result == VK_SUBOPTIMAL_KHR) return SwapchainStatus::SuboptimalNeedsRecreate;
  if (result != VK_SUCCESS) return SwapchainStatus::FatalError;
  return SwapchainStatus::Ok;
}

VulkanDevice::~VulkanDevice() {
  shutdown();
}

void VulkanDevice::shutdown() {
  // Ordem de destruição importa: device antes de instance. Nenhum comando
  // pendente deve existir aqui — é responsabilidade do caller ter esperado
  // (vkDeviceWaitIdle) antes de destruir o device.
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    pipelineCache_.shutdown();
    memoryAllocator_.shutdown();
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  if (instance_ != VK_NULL_HANDLE) {
#if AETHER_VULKAN_VALIDATION
    if (debugMessenger_ != VK_NULL_HANDLE && destroyDebugUtilsMessengerFn_ != nullptr) {
      destroyDebugUtilsMessengerFn_(instance_, debugMessenger_, nullptr);
    }
    debugMessenger_ = VK_NULL_HANDLE;
    setDebugUtilsObjectNameFn_ = nullptr;
    cmdBeginDebugUtilsLabelFn_ = nullptr;
    cmdEndDebugUtilsLabelFn_ = nullptr;
    destroyDebugUtilsMessengerFn_ = nullptr;
#endif
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
  physicalDevice_ = VK_NULL_HANDLE;
  graphicsQueueFamily_ = 0;
  bindlessTextureCapacity_ = 0;
  deviceFeatures_ = {};
  enabledPaths_ = {};
  deviceProfile_ = DeviceProfile::C;
}

#if AETHER_VULKAN_VALIDATION
void VulkanDevice::setObjectName(VkObjectType type, u64 handle, const char *name) const {
  if (setDebugUtilsObjectNameFn_ == nullptr || handle == 0) return;
  VkDebugUtilsObjectNameInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  info.objectType = type;
  info.objectHandle = handle;
  info.pObjectName = name;
  setDebugUtilsObjectNameFn_(device_, &info);
}

void VulkanDevice::cmdBeginDebugLabel(VkCommandBuffer commandBuffer, const char *label, float r,
                                      float g, float b) const {
  if (cmdBeginDebugUtilsLabelFn_ == nullptr) return;
  VkDebugUtilsLabelEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  info.pLabelName = label;
  info.color[0] = r;
  info.color[1] = g;
  info.color[2] = b;
  info.color[3] = 1.0f;
  cmdBeginDebugUtilsLabelFn_(commandBuffer, &info);
}

void VulkanDevice::cmdEndDebugLabel(VkCommandBuffer commandBuffer) const {
  if (cmdEndDebugUtilsLabelFn_ == nullptr) return;
  cmdEndDebugUtilsLabelFn_(commandBuffer);
}
#else
void VulkanDevice::setObjectName(VkObjectType, u64, const char *) const {}
void VulkanDevice::cmdBeginDebugLabel(VkCommandBuffer, const char *, float, float, float) const {}
void VulkanDevice::cmdEndDebugLabel(VkCommandBuffer) const {}
#endif

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
  if (instance_ != VK_NULL_HANDLE || appName == nullptr ||
      (requiredExtensionCount > 0 && requiredExtensions == nullptr)) return false;

  u32 availableExtensionCount = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr) != VK_SUCCESS) {
    return false;
  }
  VkExtensionProperties available[64];
  availableExtensionCount = availableExtensionCount > 64 ? 64 : availableExtensionCount;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, available) != VK_SUCCESS) {
    return false;
  }
  for (u32 required = 0; required < requiredExtensionCount; ++required) {
    bool found = false;
    for (u32 candidate = 0; candidate < availableExtensionCount; ++candidate) {
      if (std::strcmp(requiredExtensions[required], available[candidate].extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  std::vector<const char *> enabledExtensions;
  if (requiredExtensionCount > 0) {
    if (requiredExtensions == nullptr) return false;
    enabledExtensions.assign(requiredExtensions, requiredExtensions + requiredExtensionCount);
  }
  std::vector<const char *> enabledLayers;

  // Item 2.1.4 (bindless): VK_KHR_get_physical_device_properties2 foi
  // promovida a core no Vulkan 1.1, mas "core" não significa "habilitada
  // automaticamente" — descoberto em hardware real (Xiaomi via validation
  // layer, item 2.1.6): sem pedir esta extensão de instância explicitamente,
  // vkGetInstanceProcAddr ainda resolve um ponteiro não-nulo para
  // vkGetPhysicalDeviceFeatures2KHR, mas a chamada silenciosamente devolve a
  // struct encadeada zerada em vez das sub-features reais do device — nunca
  // rejeitada pela validation layer (nenhuma VUID dispara), só incorreta.
  // Mesma disciplina de "consultar, nunca presumir" do resto do arquivo:
  // habilita só se realmente enumerada como disponível.
  bool physicalDeviceProperties2Available = false;
  for (u32 candidate = 0; candidate < availableExtensionCount; ++candidate) {
    if (std::strcmp(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                    available[candidate].extensionName) == 0) {
      physicalDeviceProperties2Available = true;
      break;
    }
  }
  if (physicalDeviceProperties2Available) {
    enabledExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  }

#if AETHER_VULKAN_VALIDATION
  // Item 2.1.6 do plano: só em build debug (!NDEBUG — CMAKE_BUILD_TYPE=Debug
  // via AGP, ver android/app/build.gradle.kts buildTypes.debug), e só se a
  // camada/extensão de fato existirem — nunca presumidas, mesma disciplina
  // de ASTC/descriptor_indexing em initializeDevice(). Sem
  // libVkLayer_khronos_validation.so empacotado no APK (ver
  // native/third_party/vulkan-validation-layers/) a camada simplesmente não
  // aparece na enumeração e o shell sobe normalmente sem ela.
  bool debugUtilsExtensionAvailable = false;
  for (u32 candidate = 0; candidate < availableExtensionCount; ++candidate) {
    if (std::strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, available[candidate].extensionName) == 0) {
      debugUtilsExtensionAvailable = true;
      break;
    }
  }

  bool validationLayerAvailable = false;
  {
    u32 availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    if (availableLayerCount > 0) {
      std::vector<VkLayerProperties> layers(availableLayerCount);
      vkEnumerateInstanceLayerProperties(&availableLayerCount, layers.data());
      for (const auto &layer : layers) {
        if (std::strcmp("VK_LAYER_KHRONOS_validation", layer.layerName) == 0) {
          validationLayerAvailable = true;
          break;
        }
      }
    }
  }

  if (debugUtilsExtensionAvailable) enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  if (validationLayerAvailable) enabledLayers.push_back("VK_LAYER_KHRONOS_validation");

  VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
  if (debugUtilsExtensionAvailable) {
    messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.pfnUserCallback = debugUtilsCallback;
  }
#endif

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
  instInfo.enabledExtensionCount = static_cast<u32>(enabledExtensions.size());
  instInfo.ppEnabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();
  instInfo.enabledLayerCount = static_cast<u32>(enabledLayers.size());
  instInfo.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();
#if AETHER_VULKAN_VALIDATION
  // Encadeado no pNext (não só criado depois de vkCreateInstance): captura
  // mensagens de validação emitidas durante a própria criação da instância,
  // que é quando a spec Vulkan é mais fácil de violar por acidente (structs
  // de criação mal preenchidas) — prática recomendada pelo próprio Khronos.
  if (debugUtilsExtensionAvailable) instInfo.pNext = &messengerInfo;
#endif

  if (vkCreateInstance(&instInfo, nullptr, &instance_) != VK_SUCCESS) {
    return false;
  }

#if AETHER_VULKAN_VALIDATION
  __android_log_print(ANDROID_LOG_INFO, "Aether.Vulkan",
      "[VulkanDiagnostics] validation=%s debug_utils=%s",
      validationLayerAvailable ? "enabled" : "disabled",
      debugUtilsExtensionAvailable ? "enabled" : "disabled");
#endif

#if AETHER_VULKAN_VALIDATION
  if (debugUtilsExtensionAvailable) {
    auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    destroyDebugUtilsMessengerFn_ = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (createMessenger != nullptr) {
      createMessenger(instance_, &messengerInfo, nullptr, &debugMessenger_);
    }
    setDebugUtilsObjectNameFn_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT"));
    cmdBeginDebugUtilsLabelFn_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance_, "vkCmdBeginDebugUtilsLabelEXT"));
    cmdEndDebugUtilsLabelFn_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance_, "vkCmdEndDebugUtilsLabelEXT"));
  }
#endif

  return true;
}

bool VulkanDevice::initializeDevice(VkSurfaceKHR presentationSurface, bool allowBindless) {
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

  // ASTC é o formato de compressão de textura obrigatório no piso de hardware do plano
  // (RNF-11: "Vulkan 1.1 + ASTC + 6 GB RAM"), então habilitá-lo aqui não é uma feature opcional
  // condicional — todo aparelho-alvo já suporta. Consultada via GetPhysicalDeviceFeatures (não
  // presumida) porque uma feature não solicitada continua desligada mesmo se o hardware suportar,
  // e vkCreateDevice falha se pedirmos uma feature que o GetFeatures não confirmou disponível.
  VkPhysicalDeviceFeatures supportedFeatures{};
  vkGetPhysicalDeviceFeatures(physicalDevice_, &supportedFeatures);
  VkPhysicalDeviceFeatures enabledFeatures{};
  enabledFeatures.textureCompressionASTC_LDR = supportedFeatures.textureCompressionASTC_LDR;

  // Item 2.1.4 (bindless via descriptor_indexing): VK_EXT_descriptor_indexing é core no Vulkan
  // 1.2+, mas continua exigindo consulta explícita de suporte — extensão core não significa
  // feature automaticamente habilitada (mesma disciplina de ASTC acima). Checamos a extensão via
  // enumeração (não presumimos "é core, logo existe") porque o piso mínimo do plano é só Vulkan
  // 1.1 (RNF-11) — em 1.1/1.2 sem a extensão explícita, bindless simplesmente fica desligado e o
  // dispositivo cai para DeviceProfile::C (ver device_profile.cpp), não é um erro fatal.
  bool descriptorIndexingExtensionSupported = false;
  {
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, nullptr);
    if (extensionCount > 0) {
      std::vector<VkExtensionProperties> extensions(extensionCount);
      vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, extensions.data());
      for (const auto &ext : extensions) {
        if (std::strcmp(ext.extensionName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) {
          descriptorIndexingExtensionSupported = true;
          break;
        }
      }
    }
  }

  VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
  descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  if (descriptorIndexingExtensionSupported) {
    // vkGetPhysicalDeviceFeatures2 (core, sem sufixo) é legal de chamar em
    // tempo de execução — a instância pede apiVersion 1.1 acima — mas o stub
    // libvulkan.so vendorizado pelo NDK para minSdk 26 (RNF-11 admite 1.1)
    // não exporta esse símbolo estaticamente em toda revisão de NDK; a
    // variante KHR tem ABI idêntica e é sempre resolvível via
    // vkGetInstanceProcAddr, que é como o próprio Khronos loader trata
    // "extensão core promovida" — resolver em runtime em vez de linkar
    // estático evita depender de qual símbolo aquele stub específico expõe.
    auto getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
        vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceFeatures2KHR"));
    if (getFeatures2 == nullptr) {
      getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
          vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceFeatures2"));
    }
    if (getFeatures2 != nullptr) {
      VkPhysicalDeviceFeatures2 features2{};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features2.pNext = &descriptorIndexingFeatures;
      getFeatures2(physicalDevice_, &features2);
    }
  }

  // As quatro sub-features mínimas que compõem "bindless" de verdade (plano §5.2: "shaders acessam
  // textures[materialIndex], sem descriptor set por objeto"): array runtime-sized no shader,
  // indexação não-uniforme (materialIndex varia por invocação, não é constante de compilação),
  // slots parcialmente vinculados (nem todo índice do array precisa ter uma textura real ainda —
  // ver bindless_registry.h, o padrão "dummy" documentado lá depende exatamente disso), e update
  // de binding de imagem/sampler após o bind (BindlessTextureRegistry usa
  // VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT num binding COMBINED_IMAGE_SAMPLER — sem esta
  // quarta sub-feature especificamente para tipos de imagem amostrada, vkCreateDescriptorSetLayout
  // viola VUID-...-descriptorBindingSampledImageUpdateAfterBind-03006, confirmado em hardware real
  // via validation layer, item 2.1.6 — a lista de "3 mínimas" estava incompleta).
  VkPhysicalDeviceDescriptorIndexingProperties indexingProperties{};
  indexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
  if (descriptorIndexingExtensionSupported) {
    auto getProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties2"));
    if (getProperties2 != nullptr) {
      VkPhysicalDeviceProperties2 properties{};
      properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      properties.pNext = &indexingProperties;
      getProperties2(physicalDevice_, &properties);
    }
  }
  bindlessTextureCapacity_ = ae::rhi::bindlessTextureCapacity(
      descriptorIndexingExtensionSupported, descriptorIndexingFeatures, indexingProperties);
  const bool bindlessSupported = allowBindless && bindlessTextureCapacity_ > 0;
  if (!bindlessSupported) bindlessTextureCapacity_ = 0;
  auto enabledDescriptorIndexingFeatures = enabledBindlessTextureFeatures(bindlessSupported);

  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.pNext = bindlessSupported ? &enabledDescriptorIndexingFeatures : nullptr;
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queueInfo;
  deviceInfo.pEnabledFeatures = &enabledFeatures;

  std::vector<const char *> deviceExtensions;
  if (presentationSurface != VK_NULL_HANDLE) {
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  if (bindlessSupported) {
    deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
  }
  if (!deviceExtensions.empty()) {
    deviceInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
  }

  if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS) {
    return false;
  }

  if (!memoryAllocator_.initialize(instance_, physicalDevice_, device_,
                                   deriveMobileMemoryBudget(physicalDevice_))) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
    return false;
  }
  pipelineCache_.initialize(device_);

  // Preenche DeviceFeatures com os dados reais consultados acima e deriva perfil/caminhos
  // habilitados via a lógica pura já testada headless (device_profile.cpp) — não reimplementamos
  // a decisão aqui, só alimentamos com dados reais em vez dos valores à mão que os testes usam.
  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties);
  deviceFeatures_ = DeviceFeatures{};
  deviceFeatures_.vulkan1_3 = deviceProperties.apiVersion >= VK_API_VERSION_1_3;
  deviceFeatures_.descriptorIndexing = descriptorIndexingExtensionSupported;
  deviceFeatures_.bindlessNonUniformIndexing = bindlessSupported;
  deviceFeatures_.maxBoundDescriptorSets = deviceProperties.limits.maxBoundDescriptorSets;
  maximumImage2DSize_ = deviceProperties.limits.maxImageDimension2D;
  maximumImageArrayLayers_ = deviceProperties.limits.maxImageArrayLayers;
  // samplerAnisotropy é uma feature opcional: sem ela o limite reportado não vale,
  // e pedir anisotropia > 1 seria uso inválido do sampler.
  maximumSamplerAnisotropy_ = enabledFeatures.samplerAnisotropy == VK_TRUE
                                  ? deviceProperties.limits.maxSamplerAnisotropy
                                  : 1.0f;
  deviceProfile_ = classifyDeviceProfile(deviceFeatures_);
  enabledPaths_ = derivePaths(deviceProfile_, deviceFeatures_);

  return true;
}

} // namespace ae::rhi
