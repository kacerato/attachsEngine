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
#include "renderer/gpu_mesh_instance.h"
#include "renderer/gpu_cost_isolation.h"
#include "renderer/frame_graph.h"
#include "renderer/frustum_visibility.h"
#include "renderer/hzb_visibility.h"
#include "renderer/lod_selection.h"
#include "renderer/render_instance.h"
#include "renderer/runtime_hud.h"
#include "platform/android/material_preview_resources.h"
#include "platform/android/dirt_road_resources.h"
#include "platform/free_camera_controller.h"

#include <array>
#include <cmath>
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
  rhi::SwapchainStatus drawFrame(float timeSeconds, const platform::FreeCameraState &camera,
                                 const renderer::RuntimeHudState &hud = {});
  bool hasDefaultCamera() const { return dirtRoadPreview_ && dirtRoadResources_.header().drawCount != 0; }
  platform::FreeCameraState defaultCamera() const { return dirtRoadResources_.defaultCamera(); }
  platform::FreeCameraState defaultGameplayCamera() const {
    return dirtRoadResources_.defaultGameplayCamera();
  }
  const renderer::StaticCollisionMesh &staticCollisionMesh() const {
    return dirtRoadResources_.staticCollisionMesh();
  }
  void releaseStaticCollisionCpuData(){dirtRoadResources_.releaseStaticCollisionCpuData();}

  // Microssegundos de wall time no crossing C++→C# de FillInstanceBuffer do frame
  // mais recente (só o crossing + preenchimento, não o frame Vulkan inteiro
  // — separar os dois é o que permite atribuir custo à fronteira de
  // interop especificamente, a pergunta que a PoC-A faz).
  double lastFillMicroseconds() const { return lastFillMicroseconds_; }
  u32 drawnInstanceCount() const { return drawnInstanceCount_; }
  // Identidade e contagens do pacote realmente carregado (item O0 do plano de
  // otimização): o relatório de perfil recebe a cena do renderer em vez de
  // inferi-la. Fora do mapa cozido não há pacote, e as contagens ficam em zero
  // em vez de reportar números de outra cena.
  u64 contentFingerprint() const {
    return dirtRoadPreview_ ? dirtRoadResources_.packageFingerprint() : 0;
  }
  u32 profileDrawCount() const {
    return dirtRoadPreview_ ? dirtRoadResources_.header().drawCount : 0;
  }
  u32 profileMaterialCount() const {
    return dirtRoadPreview_ ? dirtRoadResources_.header().materialCount : 0;
  }
  u32 profileTextureCount() const {
    return dirtRoadPreview_ ? dirtRoadResources_.header().textureCount : 0;
  }
  u32 profileTriangleCount() const {
    return dirtRoadPreview_ ? dirtRoadResources_.header().triangleCount : 0;
  }
  u32 profilePackageVersion() const {
    return dirtRoadPreview_ ? dirtRoadResources_.header().version : 0;
  }
  u32 profileRenderDrawCount() const {
    return dirtRoadPreview_ ? static_cast<u32>(dirtRoadResources_.draws().size()) : 0;
  }
  u32 profileLodGroupCount() const { return static_cast<u32>(lodGroups_.size()); }
  int lastExtractionStatus() const { return lastExtractionStatus_; }
  // Diagnostic only: computed on demand at lifecycle/mutation checkpoints.
  u64 snapshotFingerprint() const;
  void setFrameProfilingEnabled(bool enabled) { frameProfilingEnabled_ = enabled; }
  // Chave diagnóstica A/B. O padrão da engine permanece ativado; jogos não
  // precisam configurar nada para receber o caminho otimizado.
  void setCoveragePrepassEnabled(bool enabled) { coveragePrepassEnabled_ = enabled; }
  void setGpuCostIsolation(renderer::GpuCostIsolation mode) { gpuCostIsolation_ = mode; }
  // Chave diagnóstica de A/B, no mesmo espírito de GpuCostIsolation: força o
  // anexo de profundidade a ser alocado como render target comum, mesmo quando
  // o render graph provou que ninguém o lê. Existe para medir o que o caminho
  // memoryless entrega; nunca é um preset de qualidade.
  void setDisableTransientDepth(bool disabled) { disableTransientDepth_ = disabled; }
  void setRuntimeHudEnabled(bool enabled) { runtimeHudEnabled_ = enabled; }
  // HZB (Hi-Z) conservative occlusion culling -- see native/renderer/
  // hzb_visibility.h for the pure CPU decision layer this feeds, and
  // PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md for the sync/barrier design this
  // integration follows. Off by default; opt-in via aether.hzb_occlusion.
  // NOT validated on physical hardware yet -- keep default false until a
  // physical A/B (Khronos validation layers clean, SSIM/FLIP visual diff)
  // passes, matching every other experimental flag in this renderer.
  void setHzbOcclusionEnabled(bool enabled) { hzbOcclusionEnabled_ = enabled; }
  bool hzbOcclusionEnabled() const { return hzbOcclusionEnabled_; }
  // Consecutive occluded frames required before an object is actually culled
  // (grace period; 0 means cull on the first occluded test). See
  // renderer::updateHzbHysteresis for the exact contract.
  void setHzbHysteresisFrames(u32 frames) { hzbHysteresisFrames_ = frames; }
  void setHzbMinimumCandidateDraws(u32 draws) { hzbMinimumCandidateDraws_ = draws; }
  void setHzbNormalizedDepthBias(float bias) {
    if (std::isfinite(bias) && bias >= 0.0f) hzbNormalizedDepthBias_ = bias;
  }
  // LOD selection by projected screen-space error -- see
  // native/renderer/lod_selection.h and tools/cook-gltf-map.py's simplifier.
  // Off by default; opt-in via aether.lod_selection. Also NOT validated on
  // physical hardware yet -- see setHzbOcclusionEnabled's note, the same
  // gate applies here.
  void setLodSelectionEnabled(bool enabled) { lodSelectionEnabled_ = enabled; }
  bool lodSelectionEnabled() const { return lodSelectionEnabled_; }
  // Screen-space error budget in pixels and the hysteresis band ratio (see
  // renderer::selectLodLevel) -- data-driven budgets, never hardcoded
  // per-scene (ADR-014).
  void setLodPixelErrorBudget(float budget) { lodPixelErrorBudget_ = budget; }
  void setLodHysteresisBandRatio(float ratio) { lodHysteresisBandRatio_ = ratio; }
  renderer::GpuCostIsolation gpuCostIsolation() const { return gpuCostIsolation_; }
  const renderer::VisibilityTelemetry &visibilityTelemetry() const {
    return visibilityTelemetry_;
  }
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
  bool createRuntimeHudPipeline();
  bool createHzbResources();
  void destroyHzbResources();
  bool createHzbPipeline(const u32 *vertSpirv, u32 vertSpirvSize, const u32 *fragSpirv, u32 fragSpirvSize,
                         u32 pushConstantBytes, VkPipelineLayout &outLayout, VkPipeline &outPipeline);
  // Records the full reduction chain (depth -> base level -> coarser levels)
  // into commandBuffer_. Must be called after the main render pass's
  // vkCmdEndRenderPass and before the frame's vkQueueSubmit; depthImage_ must
  // already be in VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL (its state
  // right after the main pass, before this function transitions it).
  void recordHzbReductionPass(const platform::FreeCameraState &camera);
  // Reads back the pyramid built by the PREVIOUS frame's recordHzbReductionPass
  // into hzbPyramid_. Called once near the top of drawFrame, at the same point
  // gpuFrameTimer_.collectPrevious() reads last frame's GPU timestamps -- the
  // acquireNextImage fence wait already guarantees that copy fully landed, so
  // this never introduces a new stall (see PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md).
  void readHzbPyramidFromPreviousFrame();
  // Uma região de GPU é sempre marcador de debug + timestamp, nunca um dos
  // dois: um marcador sem métrica é uma captura que não fecha com o relatório,
  // e uma métrica sem marcador é um número que a captura não consegue
  // explicar. Por isso os dois só são gravados por estes dois métodos, em par.
  // endGpuRegion deve ser chamado mesmo quando a classe não desenhou nada no
  // frame, para que a classe seguinte não absorva o custo dela.
  void beginGpuRegion(GpuPassClass pass);
  void endGpuRegion(GpuPassClass pass);

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
  VkPipelineLayout runtimeHudPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline runtimeHudPipeline_ = VK_NULL_HANDLE;

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
  rhi::VulkanBuffer indirectBuffer_{};
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
  // Scratch lists retain capacity across frames. Visibility must never allocate
  // in drawFrame(), particularly while the player is moving through the map.
  std::vector<u32> visibleSolidDrawOrder_;
  std::vector<u32> visibleCoverageDrawOrder_;
  std::vector<u32> visibleTransparentDrawOrder_;
  struct IndirectBatch final {
    u32 materialIndex = 0;
    u32 firstCommand = 0;
    u32 commandCount = 0;
    u64 triangles = 0;
  };
  std::vector<VkDrawIndexedIndirectCommand> indirectCommands_;
  std::vector<IndirectBatch> indirectSolidBatches_;
  std::vector<IndirectBatch> indirectCoverageBatches_;
  bool useMultiDrawIndirect_ = false;
  struct MaterialParameters { float roughness=1, metallic=1, normalScale=1; };
  MaterialParameters materialParameters_;
  int lastExtractionStatus_ = 0;
  using ExtractSceneFn = int (*)(renderer::RenderInstance *, int capacity, int stride, int version);
  ExtractSceneFn extractScene_ = nullptr;
  u32 instanceStride() const {
    // O mapa cozido carrega, além do modelo, as três colunas da normal matrix
    // preparadas uma vez por transform, para o vertex shader não recomputar
    // transpose(inverse(mat3)) por vértice.
    if (dirtRoadPreview_) return sizeof(renderer::GpuMeshInstance);
    return scenePreview_ ? sizeof(renderer::RenderInstance) : 5 * sizeof(float);
  }

  using FillInstanceBufferFn = void (*)(float *outBuffer, int instanceCount, float timeSeconds);
  FillInstanceBufferFn fillInstanceBuffer_ = nullptr;
  double lastFillMicroseconds_ = 0.0;
  bool frameProfilingEnabled_ = false;
  // Timestamps de GPU acompanham o perfil. Uma tentativa de desacoplar os dois
  // para alimentar cadência adaptativa foi medida e retirada — ver
  // PROFILING-ANDROID.md, "Cadência adaptativa rejeitada".
  bool gpuTimingEnabled() const { return frameProfilingEnabled_; }
  bool coveragePrepassEnabled_ = true;
  bool runtimeHudEnabled_ = false;
  renderer::PerspectiveVisibilitySettings visibilitySettings_{};
  renderer::VisibilityTelemetry visibilityTelemetry_{};
  u64 renderedFrameCount_ = 0;
  renderer::GpuCostIsolation gpuCostIsolation_ = renderer::GpuCostIsolation::Full;
  profiler::RenderPhaseTimings lastFrameTimings_{};

  // HZB (Hi-Z) occlusion culling -- see setHzbOcclusionEnabled() above and
  // native/renderer/hzb_visibility.h. All levels share one small render pass
  // (R32_SFLOAT, MAX-reduction fragment shaders); level 0 samples depthImage_,
  // level i>0 samples level i-1. Recreated whenever the swapchain-dependent
  // resources are (createHzbResources()/destroyHzbResources() mirror
  // createDepthImage()/destroyFramebuffers()'s lifecycle exactly).
  bool hzbOcclusionEnabled_ = false;
  // Static workload gate resolved after render packets are built and before
  // depth/render-pass allocation. Keeps a requested HZB observable while
  // avoiding sampled depth, STORE and reduction resources for small scenes.
  bool hzbWorkloadEligible_ = false;
  // Derivada do render graph em createDepthResources e lida por
  // createRenderPass. Não é um cache do hzbWorkloadEligible_: é a resposta
  // compilada de "quem lê o depth depois do pass principal".
  renderer::FrameAttachmentPolicy frameAttachmentPolicy_{};
  bool disableTransientDepth_ = false;
  u32 hzbHysteresisFrames_ = 3;
  u32 hzbMinimumCandidateDraws_ = 128;
  float hzbNormalizedDepthBias_ = 1.0e-5f;
  static constexpr u32 kHzbLevelCount = 6;
  struct HzbLevelResources final {
    rhi::VulkanImage image{};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    // Descriptor set bound while PRODUCING this level: level 0's set samples
    // depthImage_; level i>0's set samples hzbLevels_[i-1].image.
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    u32 width = 0;
    u32 height = 0;
    u32 readbackOffsetFloats = 0;
  };
  HzbLevelResources hzbLevels_[kHzbLevelCount]{};
  VkRenderPass hzbRenderPass_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout hzbDescriptorSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool hzbDescriptorPool_ = VK_NULL_HANDLE;
  VkPipelineLayout hzbFirstPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline hzbFirstPipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout hzbReducePipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline hzbReducePipeline_ = VK_NULL_HANDLE;
  rhi::VulkanSampler hzbSampler_{};
  // Host-visible, GPU-written readback of the full pyramid (all kHzbLevelCount
  // levels, tightly packed via renderer::computeHzbLevelOffsets). Read one
  // frame late -- see readHzbPyramidFromPreviousFrame().
  rhi::VulkanBuffer hzbReadbackBuffer_{};
  bool hzbResourcesReady_ = false;
  renderer::HzbPyramid hzbPyramid_{};
  bool hzbPyramidValid_ = false;
  std::array<renderer::HzbLevelDims, kHzbLevelCount> hzbLevelDims_{};
  // The CPU consumes a pyramid one frame after it was rendered. Comparing it
  // against a different camera is not conservative (newly revealed pixels
  // could be culled), so motion frames fail open until the future same-frame
  // GPU-driven HZB path exists.
  platform::FreeCameraState hzbRecordedCamera_{};
  platform::FreeCameraState hzbPyramidCamera_{};
  bool hzbRecordedCameraValid_ = false;
  bool hzbPyramidCameraValid_ = false;
  bool hzbFrameEligible_ = false;
  bool hzbPreviousFrameEligible_ = false;
  // Indexed directly by drawIndex into dirtRoadResources_.draws() (stable
  // across frames: render chunks are built once at load, never reordered).
  // Sized/reset in createInstanceBuffer() alongside instanceCount_.
  std::vector<renderer::HzbHysteresisState> hzbHysteresis_;

  // LOD selection (see setLodSelectionEnabled above). Built once at load in
  // initialize() (see the lodGroupId bucketing next to
  // solidDrawOrder_/coverageDrawOrder_'s own construction); lodGroups_[i] is
  // a list of draw indices for one lodGroupId, sorted by lodLevel ascending,
  // present only for groups with more than one level. lodGroupHysteresis_ is
  // index-aligned with lodGroups_ (NOT keyed by lodGroupId directly).
  bool lodSelectionEnabled_ = false;
  float lodPixelErrorBudget_ = 2.0f;
  float lodHysteresisBandRatio_ = 0.75f;
  // One imported lodGroupId may contain MANY spatial chunks per LOD level.
  // Grouping levels as buckets (instead of assuming one draw == one level)
  // preserves chunk-level frustum/indirect submission after LOD selection.
  std::vector<renderer::LodRenderGroup> lodGroups_;
  // Built once at load: the subset of solidDrawOrder_ that belongs to no
  // multi-level group (always a candidate, LOD selection never touches it).
  std::vector<u32> ungroupedSolidDrawOrder_;
  // Scratch, rebuilt every frame LOD selection runs: ungroupedSolidDrawOrder_
  // plus each group's currently-active (and, mid-transition, neighbor)
  // draw. Reserved once in initialize(); zero-alloc per frame like the
  // visible*DrawOrder_ scratch lists.
  std::vector<u32> lodFilteredSolidDrawOrder_;
};

} // namespace ae::platform::android
