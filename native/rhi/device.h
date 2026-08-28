// Wrappers de device/swapchain/command buffer Vulkan.
//
// NOTA IMPORTANTE (leia antes de mexer): esta máquina de build não tem GPU
// nem um ICD/loader Vulkan funcional. Nenhum teste em tests/native exercita
// o conteúdo de device.cpp além de compilar — é código escrito com cuidado
// e revisado, mas não validado em execução real nesta máquina. Toda a
// lógica que PODE ser testada sem device (perfis, cache de descritores,
// render graph) foi extraída para arquivos separados propositalmente, para
// que o núcleo de decisão da engine não dependa deste código não testável.
//
// A interface abaixo existe para que o resto da engine (rendergraph, jobs de
// submissão) dependa só de `IDevice`/`ISwapchain`, nunca de VkDevice direto —
// isso permite, no futuro, um "fake device" para testes de integração sem
// GPU real.
#pragma once

#include "core/base.h"
#include "rhi/device_profile.h"
#include "rhi/memory_allocator.h"
#include "rhi/surface_transform.h"

#include <vulkan/vulkan.h>

// Item 2.1.6 do plano ("camadas de validação, marcadores de debug"): só
// existe fora de __ANDROID__ com build debug (!NDEBUG) real — esta máquina
// de build host compila device.h/.cpp só para linkar em aether_rhi/aether_tests
// (ver nota no topo do arquivo), nunca executa este código, então os campos
// de VK_EXT_debug_utils ficariam sem uso ali (-Werror rejeitaria). Definida
// aqui, não em device.cpp, porque controla o layout da própria classe abaixo.
#if defined(__ANDROID__) && !defined(NDEBUG)
#define AETHER_VULKAN_VALIDATION 1
#else
#define AETHER_VULKAN_VALIDATION 0
#endif

namespace ae::rhi {

// Resultado de uma tentativa de aquisição/apresentação de swapchain. Modela
// explicitamente os estados que forçam recriação, porque esse é o ponto
// onde o plano identifica o erro nº 1 em apps Vulkan mobile.
enum class SwapchainStatus : u32 {
  Ok = 0,
  SuboptimalNeedsRecreate = 1, // ainda utilizável neste frame, mas recriar em seguida
  OutOfDateMustRecreate = 2,   // resize/rotação/perda de surface: recriar antes de usar
  SurfaceLost = 3,             // superfície perdida (ex.: app foi para background); recriar surface inteira
  FatalError = 4,              // device/queue/comando falhou; não tentar reutilizar o frame atual
};

// Interface mínima de device — implementada por VulkanDevice (real) e, no
// futuro, por um fake para testes de integração sem GPU.
class IDevice {
public:
  virtual ~IDevice() = default;
  virtual VkDevice handle() const = 0;
  virtual VkPhysicalDevice physicalDevice() const = 0;
};

// Interface mínima de swapchain. A invariante central deste tipo:
// nunca usar `extent()`/`images()` sem antes checar o status devolvido por
// `acquireNextImage`/`present` — um OutOfDateMustRecreate ou SurfaceLost
// invalida imediatamente os handles antigos.
class ISwapchain {
public:
  virtual ~ISwapchain() = default;

  // Recria a swapchain para o `newExtent` atual da superfície. Deve ser
  // chamada:
  //  1. na primeira criação;
  //  2. sempre que acquire/present retornar OutOfDateMustRecreate;
  //  3. quando o sistema operacional notificar mudança de orientação/resize
  //     (mesmo que o Vulkan ainda não tenha reportado OutOfDate — em muitos
  //     drivers mobile o evento do SO chega antes do VK_ERROR_OUT_OF_DATE_KHR).
  // Invariante: a swapchain antiga só pode ser destruída depois que todos os
  // command buffers que a referenciam terminaram de executar na GPU (via
  // vkDeviceWaitIdle ou fences específicas) — destruir cedo demais é a causa
  // mais comum de crash em resize, exatamente o erro nº 1 citado no plano.
  virtual bool recreate(u32 newWidth, u32 newHeight) = 0;

  // Adquire o próximo índice de imagem. Retorna o status para o caller
  // decidir se deve pular o frame (SurfaceLost / OutOfDate) ou seguir e
  // recriar só depois de apresentar (Suboptimal).
  virtual SwapchainStatus acquireNextImage(u32 *outImageIndex) = 0;

  // Apresenta a imagem adquirida. Mesmo contrato de status de acquire.
  virtual SwapchainStatus present(u32 imageIndex) = 0;

  virtual u32 width() const = 0;
  virtual u32 height() const = 0;
};

// Implementação real de ISwapchain. Item 5.2 do plano de lacunas ("shell
// gráfico mínimo"): a primeira peça do RHI que efetivamente desenha algo na
// tela, não só cria handles. Como IDevice/ISwapchain acima, não é exercitada
// por teste nesta máquina de build (sem GPU/loader) — validada em execução
// real no shell Android (ver docs/ESTADO.md, seção "Shell Android").
class VulkanSwapchain final : public ISwapchain {
public:
  VulkanSwapchain() = default;
  ~VulkanSwapchain() override;

  VulkanSwapchain(const VulkanSwapchain &) = delete;
  VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

  // `device`/`physicalDevice`/`surface`/`graphicsQueueFamily` sobrevivem ao
  // VulkanSwapchain (posse é de VulkanDevice/AndroidVulkanSurface) — este
  // tipo só guarda os handles que ele mesmo cria (swapchain, image views,
  // sync objects), nunca os que recebeu de fora.
  bool initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                  u32 graphicsQueueFamily, u32 width, u32 height);
  void shutdown();

  bool recreate(u32 newWidth, u32 newHeight) override;
  SwapchainStatus acquireNextImage(u32 *outImageIndex) override;
  SwapchainStatus present(u32 imageIndex) override;

  u32 width() const override { return extent_.width; }
  u32 height() const override { return extent_.height; }

  // Acesso para quem grava comandos de desenho (fora desta interface mínima
  // porque ISwapchain não deveria expor detalhe de formato/view a quem só
  // quer orquestrar frames — RenderGraph e afins consomem só a interface).
  VkImageView imageView(u32 index) const { return imageViews_[index]; }
  VkFormat imageFormat() const { return format_; }
  u32 imageCount() const { return imageCount_; }
  const SurfaceTransform &surfaceTransform() const { return surfaceTransform_; }
  VkExtent2D displayExtent() const { return transformSurfaceExtent(extent_, surfaceTransform_); }
  bool isReady() const { return swapchain_ != VK_NULL_HANDLE && imageCount_ > 0; }
  VkSemaphore imageAvailableSemaphore() const { return imageAvailableSemaphore_; }
  // Item 2.1.6 (achado via validation layer em hardware real): o semáforo de
  // "render terminou" precisa ser um por IMAGEM de swapchain, não um global
  // — vkQueuePresentKHR consome esse semáforo de forma assíncrona ao
  // compositor da plataforma, e nada garante que o present da imagem N-1
  // terminou de consumir seu semáforo antes do acquire devolver a mesma
  // imagem de novo (a fence de "um frame em voo" só garante que a GPU
  // terminou de EXECUTAR, não que o compositor terminou de PRESENTAR).
  // last*() existe só para o caller montar o wait de present com o índice
  // certo — ver VulkanSwapchain::present(imageIndex).
  VkSemaphore renderFinishedSemaphore(u32 imageIndex) const {
    return renderFinishedSemaphores_[imageIndex];
  }
  VkFence inFlightFence() const { return inFlightFence_; }

private:
  void destroySwapchainObjects();

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  u32 graphicsQueueFamily_ = 0;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{0, 0};
  SurfaceTransform surfaceTransform_{};
  static constexpr u32 kMaxSwapchainImages = 8;
  VkImage images_[kMaxSwapchainImages]{};
  VkImageView imageViews_[kMaxSwapchainImages]{};
  u32 imageCount_ = 0;

  // Sincronização de um frame em voo só na CPU (a fence abaixo — suficiente
  // para o item 5.2, múltiplos frames em voo é otimização do RHI completo,
  // Onda 3), mas renderFinishedSemaphores_ precisa de um handle por imagem
  // de swapchain independente disso — ver comentário do getter acima.
  VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
  VkSemaphore renderFinishedSemaphores_[kMaxSwapchainImages]{};
  VkFence inFlightFence_ = VK_NULL_HANDLE;
};

// Implementação real sobre a API Vulkan. Não instanciada em nenhum teste
// nesta máquina (sem loader/ICD) — ver nota no topo do arquivo.
class VulkanDevice final : public IDevice {
public:
  VulkanDevice() = default;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;

  // Atalho para contextos sem apresentação, usado por ferramentas headless.
  bool initialize(const char *appName);

  // A inicialização é dividida porque plataformas de janela precisam da
  // VkInstance para criar VkSurfaceKHR antes de escolher uma fila capaz de
  // apresentar. Retornam false em falha e deixam o objeto reutilizável após
  // shutdown().
  bool initializeInstance(const char *appName,
                          const char *const *requiredExtensions,
                          u32 requiredExtensionCount);
  bool initializeDevice(VkSurfaceKHR presentationSurface);
  void shutdown();

  VkDevice handle() const override { return device_; }
  VkPhysicalDevice physicalDevice() const override { return physicalDevice_; }
  VkInstance instance() const { return instance_; }
  u32 graphicsQueueFamily() const { return graphicsQueueFamily_; }
  VulkanMemoryAllocator &memoryAllocator() { return memoryAllocator_; }
  const VulkanMemoryAllocator &memoryAllocator() const { return memoryAllocator_; }

  // Item 2.1.4 do plano: features/perfil detectados de verdade em initializeDevice() via
  // vkGetPhysicalDeviceFeatures2 (não presumidos) — classifyDeviceProfile/derivePaths
  // (device_profile.h, já testados headless) decidem a partir destes valores reais.
  const DeviceFeatures &deviceFeatures() const { return deviceFeatures_; }
  DeviceProfile deviceProfile() const { return deviceProfile_; }
  const EnabledPaths &enabledPaths() const { return enabledPaths_; }

  // Item 2.1.6 do plano ("camadas de validação, marcadores de debug, captura
  // de frame"): true só em build debug (!NDEBUG) E quando VK_EXT_debug_utils
  // foi de fato habilitada em initializeInstance — nunca presumido. Os
  // helpers abaixo são no-ops seguros quando false, para que o resto do RHI
  // possa chamá-los incondicionalmente (mesmo padrão de "consulta, nunca
  // presume" já usado para ASTC/descriptor_indexing).
#if AETHER_VULKAN_VALIDATION
  bool debugUtilsEnabled() const { return setDebugUtilsObjectNameFn_ != nullptr; }
#else
  bool debugUtilsEnabled() const { return false; }
#endif
  void setObjectName(VkObjectType type, u64 handle, const char *name) const;
  void cmdBeginDebugLabel(VkCommandBuffer commandBuffer, const char *label, float r, float g,
                          float b) const;
  void cmdEndDebugLabel(VkCommandBuffer commandBuffer) const;

private:
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  u32 graphicsQueueFamily_ = 0;
  VulkanMemoryAllocator memoryAllocator_{};
  DeviceFeatures deviceFeatures_{};
  DeviceProfile deviceProfile_ = DeviceProfile::C;
  EnabledPaths enabledPaths_{};

#if AETHER_VULKAN_VALIDATION
  VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
  PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameFn_ = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginDebugUtilsLabelFn_ = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT cmdEndDebugUtilsLabelFn_ = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerFn_ = nullptr;
#endif
};

} // namespace ae::rhi
