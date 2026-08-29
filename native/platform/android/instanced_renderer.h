#pragma once

#include "core/base.h"
#include "profiler/frame_statistics.h"
#include "platform/android/dotnet_host.h"
#include "rhi/bindless_registry.h"
#include "rhi/device.h"
#include "rhi/gpu_frame_timer.h"
#include "rhi/memory_allocator.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"
#include "renderer/render_instance.h"
#include "platform/android/material_preview_resources.h"
#include "platform/android/dirt_road_resources.h"
#include "platform/free_camera_controller.h"

#include <vector>

namespace ae::platform::android {

// PoC-A (item 0.2 do plano): "o overhead de interop C#↔Vulkan mata o
// desempenho?". Desenha N instâncias de um cubo (rhi/shaders/instanced.vert)
// cuja posição/cor vem inteiramente do lado C# — Aether.Interop.
// NativeEntryPoints.FillInstanceBuffer, chamada uma vez por frame através de
// DotNetHost, nunca uma vez por instância (docs/CONVENCOES.md §2: "chamadas
// nativas sempre em lote"). lastFillMicroseconds mede wall time do crossing +
// preenchimento; CPU total e distribuição de frames pertencem ao profiler do
// shell, que usa relógios CPU do processo/thread, não apenas esse trecho.
class InstancedRenderer final {
public:
  InstancedRenderer() = default;
  ~InstancedRenderer();

  InstancedRenderer(const InstancedRenderer &) = delete;
  InstancedRenderer &operator=(const InstancedRenderer &) = delete;

  // `dotNetHost` precisa já estar inicializado (ver DotNetHost::isReady) —
  // resolve o ponteiro de FillInstanceBuffer uma vez aqui, não a cada frame.
  bool initialize(rhi::VulkanDevice &device, rhi::VulkanSwapchain &swapchain,
                  DotNetHost &dotNetHost, u32 instanceCount, bool scenePreview = false,
                  AAssetManager *materialAssets = nullptr, bool forceTextureFallback = false,
                  const std::atomic<bool> *cancel = nullptr, bool dirtRoadPreview = false);
  void shutdown();

  // Os ângulos de órbita vêm da camada de input, em radianos. O renderer
  // permanece independente de AInputEvent/touch IDs e pode futuramente
  // receber a mesma ação de teclado, gamepad ou NoCode.
  rhi::SwapchainStatus drawFrame(float timeSeconds, const platform::FreeCameraState &camera);
  bool hasDefaultCamera() const { return dirtRoadPreview_ && dirtRoadResources_.header().drawCount != 0; }
  platform::FreeCameraState defaultCamera() const { return dirtRoadResources_.defaultCamera(); }

  // Microssegundos de wall time no crossing C++→C# de FillInstanceBuffer do frame
  // mais recente (só o crossing + preenchimento, não o frame Vulkan inteiro
  // — separar os dois é o que permite atribuir custo à fronteira de
  // interop especificamente, a pergunta que a PoC-A faz).
  double lastFillMicroseconds() const { return lastFillMicroseconds_; }
  u32 drawnInstanceCount() const { return drawnInstanceCount_; }
  int lastExtractionStatus() const { return lastExtractionStatus_; }
  // Diagnostic only: computed on demand at lifecycle/mutation checkpoints.
  u64 snapshotFingerprint() const;
  void setFrameProfilingEnabled(bool enabled) { frameProfilingEnabled_ = enabled; }
  // Chave diagnóstica A/B. O padrão da engine permanece ativado; jogos não
  // precisam configurar nada para receber o caminho otimizado.
  void setCoveragePrepassEnabled(bool enabled) { coveragePrepassEnabled_ = enabled; }
  const profiler::RenderPhaseTimings &lastFrameTimings() const { return lastFrameTimings_; }

private:
  bool createRenderPass();
  bool createPipeline();
  bool createFramebuffers();
  void destroyFramebuffers();
  bool createCommandResources();
  bool createInstanceBuffer();
  bool createDepthImage();
  bool createTextureResources();
  bool createBindlessRegistry();
  bool createTextureDescriptors();
  bool createEnvironmentDescriptors();
  bool createSkyPipeline();

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  // Referência não proprietária para capabilities e diagnóstico. O device
  // deve sobreviver ao renderer; a referência é limpa em shutdown().
  rhi::VulkanDevice *rhiDevice_ = nullptr;
  rhi::VulkanMemoryAllocator *memoryAllocator_ = nullptr;
  rhi::VulkanSwapchain *swapchain_ = nullptr;
  u32 graphicsQueueFamily_ = 0;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;

  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  // Item 2.1.4 do plano: em vez de um descriptor set por-objeto com 1 binding,
  // o pipeline instanciado consome o registro bindless (array único indexado
  // por materialIndex no shader) — ver rhi/bindless_registry.h. Esta PoC tem
  // um único consumidor hoje, então o registro é dono deste objeto, não
  // compartilhado entre renderers; se um segundo pipeline bindless aparecer,
  // promover a dono no VulkanDevice passa a valer a pena.
  rhi::BindlessTextureRegistry bindlessRegistry_{};
  bool useBindless_ = false;
  VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool texturePool_ = VK_NULL_HANDLE;
  VkDescriptorSet textureSet_ = VK_NULL_HANDLE;
  rhi::VulkanImage dummyTexture_{};
  rhi::VulkanSampler dummySampler_{};
  u32 baseTextureIndex_ = rhi::kBindlessIndexInvalid;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkPipeline coveragePipeline_ = VK_NULL_HANDLE;
  VkPipeline coverageShadePipeline_ = VK_NULL_HANDLE;
  VkPipeline transparentPipeline_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout environmentSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool environmentPool_ = VK_NULL_HANDLE;
  VkDescriptorSet environmentSet_ = VK_NULL_HANDLE;
  rhi::VulkanBuffer environmentUniform_{};
  VkPipelineLayout skyPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline skyPipeline_ = VK_NULL_HANDLE;

  static constexpr u32 kMaxFramebuffers = 8;
  VkFramebuffer framebuffers_[kMaxFramebuffers]{};
  u32 framebufferCount_ = 0;

  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  rhi::VulkanUploadContext uploadContext_{};
  rhi::VulkanGpuFrameTimer gpuFrameTimer_{};

  // Buffer host-visible persistente: 5.000 instâncias * 20 bytes = ~100 KB.
  // A textura imutável abaixo usa o caminho de staging do RHI; este buffer
  // dinâmico é preenchido em lote todo frame e explicitamente flushed.
  rhi::VulkanBuffer instanceBuffer_{};
  rhi::VulkanImage depthImage_{};
  rhi::VulkanImage baseTexture_{};
  rhi::VulkanSampler baseSampler_{};
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
  u32 instanceCount_ = 0;
  u32 drawnInstanceCount_ = 0;
  bool scenePreview_ = false;
  bool materialPreview_ = false;
  bool dirtRoadPreview_ = false;
  MaterialPreviewResources materialResources_;
  DirtRoadResources dirtRoadResources_;
  std::vector<u32> dirtTextureSlots_;
  std::vector<VkDescriptorSet> dirtMaterialSets_;
  std::vector<u32> solidDrawOrder_;
  std::vector<u32> coverageDrawOrder_;
  std::vector<u32> transparentDrawOrder_;
  struct MaterialParameters { float roughness=1, metallic=1, normalScale=1; };
  MaterialParameters materialParameters_;
  int lastExtractionStatus_ = 0;
  using ExtractSceneFn = int (*)(renderer::RenderInstance *, int capacity, int stride, int version);
  ExtractSceneFn extractScene_ = nullptr;
  u32 instanceStride() const { return (scenePreview_ || dirtRoadPreview_) ? sizeof(renderer::RenderInstance) : 5 * sizeof(float); }

  using FillInstanceBufferFn = void (*)(float *outBuffer, int instanceCount, float timeSeconds);
  FillInstanceBufferFn fillInstanceBuffer_ = nullptr;
  double lastFillMicroseconds_ = 0.0;
  bool frameProfilingEnabled_ = false;
  bool coveragePrepassEnabled_ = true;
  profiler::RenderPhaseTimings lastFrameTimings_{};
};

} // namespace ae::platform::android
