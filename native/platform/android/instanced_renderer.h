#pragma once

#include "core/base.h"
#include "platform/android/dotnet_host.h"
#include "rhi/device.h"

namespace ae::platform::android {

// PoC-A (item 0.2 do plano): "o overhead de interop C#↔Vulkan mata o
// desempenho?". Desenha N instâncias de um quad (rhi/shaders/instanced.vert)
// cuja posição/cor vem inteiramente do lado C# — Aether.Interop.
// NativeEntryPoints.FillInstanceBuffer, chamada uma vez por frame através de
// DotNetHost, nunca uma vez por instância (docs/CONVENCOES.md §2: "chamadas
// nativas sempre em lote"). Mede o próprio tempo de CPU do crossing +
// preenchimento (ver lastFillMicroseconds()) para dar evidência objetiva
// contra o critério de <3 ms da PoC-A — não é só "funciona", é "quanto
// custa".
class InstancedRenderer final {
public:
  InstancedRenderer() = default;
  ~InstancedRenderer();

  InstancedRenderer(const InstancedRenderer &) = delete;
  InstancedRenderer &operator=(const InstancedRenderer &) = delete;

  // `dotNetHost` precisa já estar inicializado (ver DotNetHost::isReady) —
  // resolve o ponteiro de FillInstanceBuffer uma vez aqui, não a cada frame.
  bool initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain,
                  DotNetHost &dotNetHost, u32 instanceCount);
  void shutdown();

  rhi::SwapchainStatus drawFrame(float timeSeconds);

  // Microssegundos gastos no crossing C++→C# de FillInstanceBuffer do frame
  // mais recente (só o crossing + preenchimento, não o frame Vulkan inteiro
  // — separar os dois é o que permite atribuir custo à fronteira de
  // interop especificamente, a pergunta que a PoC-A faz).
  double lastFillMicroseconds() const { return lastFillMicroseconds_; }

private:
  bool createRenderPass();
  bool createPipeline();
  bool createFramebuffers();
  void destroyFramebuffers();
  bool createInstanceBuffer();

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  rhi::VulkanSwapchain *swapchain_ = nullptr;
  u32 graphicsQueueFamily_ = 0;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;

  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;

  static constexpr u32 kMaxFramebuffers = 8;
  VkFramebuffer framebuffers_[kMaxFramebuffers]{};
  u32 framebufferCount_ = 0;

  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;

  // Buffer host-visible/host-coherent (sem staging — 5.000 instâncias * 20
  // bytes = ~100 KB, pequeno o bastante para não justificar o custo de um
  // upload em duas etapas nesta PoC; RHI de produção com staging é escopo
  // da Onda 3, item 7.1).
  VkBuffer instanceBuffer_ = VK_NULL_HANDLE;
  VkDeviceMemory instanceBufferMemory_ = VK_NULL_HANDLE;
  void *instanceBufferMapped_ = nullptr;
  u32 instanceCount_ = 0;

  using FillInstanceBufferFn = void (*)(float *outBuffer, int instanceCount, float timeSeconds);
  FillInstanceBufferFn fillInstanceBuffer_ = nullptr;
  double lastFillMicroseconds_ = 0.0;
};

} // namespace ae::platform::android
