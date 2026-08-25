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

#include <vulkan/vulkan.h>

namespace ae::rhi {

// Resultado de uma tentativa de aquisição/apresentação de swapchain. Modela
// explicitamente os estados que forçam recriação, porque esse é o ponto
// onde o plano identifica o erro nº 1 em apps Vulkan mobile.
enum class SwapchainStatus : u32 {
  Ok = 0,
  SuboptimalNeedsRecreate = 1, // ainda utilizável neste frame, mas recriar em seguida
  OutOfDateMustRecreate = 2,   // resize/rotação/perda de surface: recriar antes de usar
  SurfaceLost = 3,             // superfície perdida (ex.: app foi para background); recriar surface inteira
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

private:
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  u32 graphicsQueueFamily_ = 0;
};

} // namespace ae::rhi
