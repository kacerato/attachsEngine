#pragma once

#include "core/base.h"
#include "rhi/device.h"

namespace ae::platform::android {

// Item 5.2 do plano de lacunas ("shell gráfico mínimo"): primeira prova de
// que o pipeline gráfico completo (render pass, pipeline, command buffer,
// submissão sincronizada) funciona em hardware Android real, não só que
// device/surface/swapchain podem ser criados. Desenho hardcoded (triângulo
// sem vertex buffer, ver rhi/shaders/triangle.vert) — deliberadamente sem
// nenhuma conexão com RenderGraph ainda: essa integração é o item 7.3
// (Onda 3), que precisa do RHI completo (buffers, pipelines dirigidos por
// metadata de material), não deste vertical slice.
class TriangleRenderer final {
public:
  TriangleRenderer() = default;
  ~TriangleRenderer();

  TriangleRenderer(const TriangleRenderer &) = delete;
  TriangleRenderer &operator=(const TriangleRenderer &) = delete;

  // `device`/`swapchain` sobrevivem a este objeto (posse é de
  // AndroidVulkanSurface) — mesma convenção de não-posse de VulkanSwapchain
  // sobre VulkanDevice.
  bool initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain);
  void shutdown();

  // Desenha um frame: acquire → grava comandos → submete → present. Preserva
  // a diferença entre resize, perda da surface e erro fatal para o shell não
  // tentar a recuperação errada.
  rhi::SwapchainStatus drawFrame();

private:
  bool createRenderPass();
  bool createPipeline();
  bool createFramebuffers();
  void destroyFramebuffers();

  VkDevice device_ = VK_NULL_HANDLE;
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
};

} // namespace ae::platform::android
