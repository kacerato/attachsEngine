#pragma once

#include "renderer/water_shading.h"
#include "renderer/water_field.h"
#include "renderer/water_ripples.h"
#include "renderer/map_draw_update.h"
#include "renderer/punctual_lights.h"

#include "core/base.h"
#include "profiler/frame_statistics.h"
#include "platform/android/dotnet_host.h"
#include "rhi/bindless_registry.h"
#include "rhi/compute.h"
#include "rhi/water_spectral_compute.h"
#include "renderer/water_cascades.h"
#include "rhi/device.h"
#include "rhi/gpu_frame_timer.h"
#include "rhi/memory_allocator.h"
#include "rhi/resource.h"
#include "rhi/upload_context.h"
#include "renderer/gpu_mesh_instance.h"
#include "renderer/gpu_cost_isolation.h"
#include "renderer/frame_graph.h"
#include "renderer/frustum_visibility.h"
#include "renderer/gpu_draw_compaction.h"
#include "rhi/ui_renderer.h"
#include "renderer/gpu_draw_culling.h"
#include "renderer/hzb_visibility.h"

#include <algorithm>
#include <memory>
#include "renderer/lod_selection.h"
#include "renderer/render_instance.h"
#include "renderer/rendering_policy.h"
#include "renderer/runtime_hud.h"
#include "renderer/shadow_cascades.h"
#include "renderer/water_surface.h"
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
                  const std::atomic<bool> *cancel = nullptr, bool dirtRoadPreview = false,
                  const char *mapAssetRoot = "dirt_road", bool emptyScene = false);
  void shutdown();
  // Single render-owner thread. Queues up to 64 distinct draw poses per frame;
  // GPU writes happen after acquire/fence, before shadows and culling. Dynamic
  // LOD groups and water camera grids are not accepted by this initial path.
  bool queueMapDrawPose(u32 drawIndex, const float *model, const float *localCenter,
                        float localRadius);

  // Os ângulos de órbita vêm da camada de input, em radianos. O renderer
  // permanece independente de AInputEvent/touch IDs e pode futuramente
  // receber a mesma ação de teclado, gamepad ou NoCode.
  rhi::SwapchainStatus drawFrame(float timeSeconds, const platform::FreeCameraState &camera,
                                 const renderer::RuntimeHudState &hud = {});

  // Interface do editor. As instâncias são construídas fora daqui (a composição
  // da tela é do editor, não do renderer) e valem para UM frame: quem não as
  // publicar de novo simplesmente não desenha interface, sem estado preso.
  void setUiInstances(std::span<const ui::UiInstance> instances);
  // Tamanho da superficie NO ESPACO EM QUE AS INSTANCIAS FORAM CONSTRUIDAS --
  // pixels logicos, nao fisicos. Zero volta a usar a extensao do display, que e
  // o comportamento certo so quando as duas escalas coincidem.
  void setUiSurfaceSize(float width, float height);
  // Normalized logical display rectangle; empty restores a full display scene.
  void setSceneViewport(const ui::UiRect &rect) {
    sceneViewport_ = ui::intersect(rect, {0,0,1,1});
    // Current temporal reprojection assumes a full-target camera projection.
    if (!sceneViewport_.isEmpty()) { temporalAaActive_=false; temporalHistoryInitialized_=false; }
  }
  float sceneAspectRatio() const;
  // A zero pair restores the imported camera range (Play/runtime).
  void setSceneClipPlanes(float nearPlane, float farPlane) {
    sceneNearPlane_ = nearPlane;
    sceneFarPlane_ = farPlane;
  }
  // Zero restores the renderer's configured field of view.
  void setSceneFieldOfView(float radians) { sceneFieldOfView_=radians; }
  void setEditorBackground(bool enabled) { editorBackground_=enabled; }
  void setEnvironmentAdjustment(const float values[4]) { std::copy(values,values+4,environmentAdjustment_); }
  const std::vector<renderer::MapMaterialRecord> &mapMaterials() const { return dirtRoadResources_.materials(); }
  bool queueMapScene(std::span<const renderer::MapDrawState> draws);
  bool queueAuthoredPoses(std::span<const renderer::MapDrawState> draws);
  // As luzes da cena, sem orçamento aplicado. Quem escolhe é o quadro, porque a
  // escolha depende da câmera: publicar aqui uma lista já cortada faria a luz
  // relevante depender de onde a câmera estava quando a cena foi publicada.
  void queueSceneLights(std::span<const renderer::SceneLight> lights) {
    sceneLights_.assign(lights.begin(), lights.end());
  }
  // O que o último quadro conseguiu acender. Quem chama publica no console: uma
  // luz excedente precisa aparecer como aviso, nunca sumir calada.
  const renderer::LightBudgetReport &lightBudget() const noexcept { return lightBudget_; }


  bool uiRendererReady() const { return uiRenderer_.isReady(); }
  const ui::UiFont &uiFont() const { return uiFont_; }
  const ui::UiIconAtlas &uiIcons() const { return uiIcons_; }
  bool hasDefaultCamera() const { return dirtRoadPreview_ && dirtRoadResources_.header().drawCount != 0; }
  renderer::PerspectiveVisibilitySettings mapProjection() const {
    auto settings=visibilitySettings_;
    settings.nearPlane=dirtRoadResources_.header().nearPlane;
    settings.farPlane=dirtRoadResources_.header().farPlane;
    return settings;
  }
  platform::FreeCameraState defaultCamera() const { return dirtRoadResources_.defaultCamera(); }
  platform::FreeCameraState defaultGameplayCamera() const {
    return dirtRoadResources_.defaultGameplayCamera();
  }
  const renderer::StaticCollisionMesh &staticCollisionMesh() const {
    return dirtRoadResources_.staticCollisionMesh();
  }
  void releaseStaticCollisionCpuData(){dirtRoadResources_.releaseStaticCollisionCpuData();}
  std::span<const u8> pickingVertices() const { return dirtRoadResources_.pickingVertices(); }
  std::span<const u32> pickingIndices() const { return dirtRoadResources_.pickingIndices(); }
  const std::vector<renderer::MapDrawRecord> &mapDraws() const { return dirtRoadResources_.draws(); }
  renderer::WaterFieldSetup waterQuerySetup() const {
    renderer::WaterFieldSetup setup;
    setup.profile = waterProfile_;
    setup.baseHeight = waterBaseHeight_;
    setup.interaction = waterInteractions_;
    return setup;
  }

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
  u32 profileLodGroupCount() const {
    return static_cast<u32>(lodGroups_.size() + coverageLodGroups_.size());
  }
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
  void setSpectralWaterEnabled(bool enabled) { spectralWaterEnabled_ = enabled; }
  void setWaterAuthoringEnabled(bool enabled) { waterAuthoringEnabled_ = enabled; }
  // Diagnóstico de atribuição: força o formato largo de inclinação para que o
  // A/B do formato caiba num único APK e possa ser intercalado no mesmo estado
  // de clock. Comparar dois APKs instalados em momentos diferentes mede o
  // aparelho, não a mudança.
  void setWideWaterSlopes(bool wide) { wideWaterSlopes_ = wide; }
  void setWaterCostIsolation(renderer::WaterCostIsolation mode) { waterCostIsolation_ = mode; }
  // UI protocol: 0 inactive, 1 analytical, 2 spectral, 3 analytical fallback.
  u32 waterProviderStatus() const noexcept {
    if(!waterSubpassActive_) return 0;
    return spectralWaterCount_>0?2u:(spectralWaterEnabled_?3u:1u);
  }
  // Configuracao efetivamente usada pelo provider GPU. O owner da simulacao
  // pode construir um espelho CPU deterministico sem conhecer Vulkan nem
  // duplicar defaults de cascata no gameplay.
  std::span<const renderer::WaterCascadeSettings> activeWaterCascades() const noexcept {
    return {waterCascadeSettings_.data(),
            std::min<usize>(spectralWaterCount_, waterCascadeSettings_.size())};
  }
  const renderer::WaterSpectralControls &waterSpectralControls() const noexcept {
    return waterSpectralControls_;
  }
  bool setWaterSpectralControls(const renderer::WaterSpectralControls &controls) {
    if(!renderer::validateWaterSpectralControls(controls)) return false;
    waterSpectralControls_=controls; return true;
  }
  bool setWaterCascades(std::span<const renderer::WaterCascadeSettings> settings, u64 bufferBudget) {
    if(device_!=VK_NULL_HANDLE || renderer::validateWaterCascades(settings,bufferBudget)!=renderer::WaterCascadeError::None) return false;
    waterCascadeSettings_.assign(settings.begin(),settings.end());
    waterCascadeBufferBudget_=bufferBudget;
    return true;
  }
  // Atualiza modos espectrais sem recriar layouts/pipelines. Quantidade,
  // resolução e domínio permanecem invariantes nesta operação; mudanças
  // estruturais pertencem à reconstrução de recursos da cena.
  bool reconfigureWaterCascades(std::span<const renderer::WaterCascadeSettings> settings, u64 budget=0);
  u64 waterSpectrumRevision() const noexcept { return waterSpectrumRevision_; }
  void setAdpfGpuTimingEnabled(bool enabled) { adpfGpuTimingEnabled_ = enabled; }
  // Water authoring remains a backend-neutral Resource. The Vulkan renderer
  // only retains its validated value representation and uploads it once per
  // frame with the other scene globals.
  bool setWaterProfile(const renderer::WaterProfile &profile, float baseHeight = 0.0f) {
    if (renderer::validateWaterProfile(profile) != renderer::WaterValidationError::None ||
        !std::isfinite(baseHeight) ||
        std::abs(baseHeight)+renderer::maximumWaterDisplacement(profile) +
        renderer::maximumWaterDetailDisplacement(waterShading_) > waterDisplacementCapacity_) return false;
    waterProfile_ = profile;
    waterBaseHeight_ = baseHeight;
    return true;
  }
  // Publica a grade de ondulação para o próximo quadro. O campo é copiado, não
  // referenciado: a simulação roda no dono da cena e o renderer lê no laço de
  // quadro, e emprestar aqui obrigaria os dois a concordarem sobre tempo de
  // vida sem nenhum mecanismo que garanta isso.
  bool setWaterRipples(const renderer::WaterRippleField &field) noexcept;
  void clearWaterRipples() noexcept { waterRippleGain_ = 0.0f; }
  // An external fixed-step owner may synchronize optics and geometry to physics.
  void setWaterSimulationClock(float seconds) { authoredWaterTime_=seconds; }

  bool addWaterImpulse(const renderer::WaterImpulse &impulse) {
    if (std::abs(waterBaseHeight_)+std::abs(impulse.amplitude) +
        renderer::maximumWaterDisplacement(waterProfile_) +
        renderer::maximumWaterDetailDisplacement(waterShading_) >
        waterDisplacementCapacity_) return false;
    return waterInteractions_.addImpulse(impulse);
  }
  bool setWaterShading(const renderer::WaterShadingSettings &settings) noexcept {
    if (!renderer::validateWaterShading(settings) || std::abs(waterBaseHeight_) +
        renderer::maximumWaterDisplacement(waterProfile_) +
        renderer::maximumWaterDetailDisplacement(settings) > waterDisplacementCapacity_) return false;
    waterShading_ = settings;
    return true;
  }
  // Resource-epoch visibility budget. Raising it later requires rebuilding
  // spatial chunks; profile/touch edits inside it remain allocation-free.
  void setWaterDisplacementCapacity(float capacity) {
    if (std::isfinite(capacity) && capacity >= 0.0f) waterDisplacementCapacity_ = capacity;
  }
  // Uma cópia imutável por época de renderer. Nenhum passe consulta nome de
  // preset; todos consomem somente estes budgets já resolvidos.
  void setRenderingPolicy(const renderer::ResolvedRenderingPolicy &policy) {
    resourceRenderingPolicy_ = policy;
    applyRuntimeRenderingPolicy(policy, false);
  }
  // Reconfigura somente eixos que não alteram o layout dos recursos Vulkan.
  // A política usada na criação permanece em resourceRenderingPolicy_: assim
  // pressão térmica pode reduzir e depois recuperar qualidade sem reconstruir
  // atlas, render pass, pipelines, descritores ou assets.
  void setRuntimeRenderingPolicy(const renderer::ResolvedRenderingPolicy &policy);
  // Scene/transform systems call this when any static caster, material alpha or
  // sun configuration changes. A câmera não precisa invalidar manualmente: o
  // renderer testa contenção de cada cascata antes de reutilizá-la.
  void invalidateStaticShadowCache() {
    shadowCacheInitialized_ = false;
    shadowCascadeDirtyMask_ = 0xffffffffu;
  }
  // HZB (Hi-Z) conservative occlusion culling -- see native/renderer/
  // hzb_visibility.h for the pure CPU decision layer this feeds, and
  // PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md for the sync/barrier design this
  // integration follows. Off by default; opt-in via aether.hzb_occlusion.
  // NOT validated on physical hardware yet -- keep default false until a
  // physical A/B (Khronos validation layers clean, SSIM/FLIP visual diff)
  // passes, matching every other experimental flag in this renderer.
  void setHzbOcclusionEnabled(bool enabled) { hzbOcclusionEnabled_ = enabled; }
  bool hzbOcclusionEnabled() const { return hzbOcclusionEnabled_; }
  // GPU-only depth-pyramid producer. This is independent from the legacy
  // CPU-readback occlusion consumer: C1 can be validated without silently
  // claiming C2 (GPU culling/indirect compaction) is already delivered.
  void setHzbComputeEnabled(bool enabled) { hzbComputeEnabled_ = enabled; }
  bool hzbComputeEnabled() const { return hzbComputeEnabled_; }
  bool hzbComputeActive() const { return hzbComputeActive_; }
  // Diagnostic only. Production compute keeps the pyramid resident on GPU;
  // enabling this copies it back one frame later and validates every 2x2 max.
  void setHzbComputeReadbackValidationEnabled(bool enabled) {
    hzbComputeReadbackValidationEnabled_ = enabled;
  }
  // C2: o consumidor GPU da pirâmide. Sem ele o produtor compute custa tempo de
  // GPU medido e não remove um único triângulo, porque a decisão continuava na
  // CPU e a CPU perdeu o readback. O kernel escreve o instanceCount dos
  // comandos indiretos que o passe opaco já submete; nenhum estágio gráfico
  // precisa saber que ele existe. Exige o produtor compute e multi-draw
  // indirect, e permanece opt-in (aether.hzb_gpu_culling) enquanto não houver
  // A/B físico com gate de imagem, como todo experimento deste renderer.
  // Lotes indiretos que preservam a ordem de profundidade.
  //
  // O agrupamento por material submete TODOS os draws de um material antes de
  // qualquer draw do proximo -- inclusive os mais distantes antes dos mais
  // proximos. A lista ja chega ordenada front-to-back e o agrupamento a
  // desmancha, o que e exatamente o que o early-Z/LRZ do tiler precisa que
  // nao aconteca: um fragmento distante sombreado antes do proximo que o
  // esconde nao pode ser rejeitado depois.
  //
  // Ligado, o lote fecha por RUN-LENGTH: um lote novo a cada troca de material
  // ao longo da lista ordenada. Custa mais lotes (push constants e um
  // vkCmdDrawIndexedIndirect por lote, na CPU, que tem folga medida) e nao
  // muda um pixel -- opaco e alpha-mask sao independentes de ordem.
  void setDepthOrderedBatchesEnabled(bool enabled) { depthOrderedBatches_ = enabled; }
  bool depthOrderedBatchesEnabled() const { return depthOrderedBatches_; }
  // Impostores de folhagem distante (renderer::MapMaterialImpostor). Desligado,
  // os draws marcados nao entram em nenhuma fila e o grupo volta a terminar no
  // nivel simplificado mais grosseiro que o cooker gerou -- e exatamente o
  // pacote de antes do baker, no MESMO binario e na MESMA sessao. Sem isso o
  // A/B exigiria dois APKs de 334 MiB e duas instalacoes entre as amostras,
  // que e o oposto da comparacao intercalada que docs/ORCAMENTO-120HZ.md exige.
  void setFoliageImpostorsEnabled(bool enabled) { foliageImpostors_ = enabled; }
  bool foliageImpostorsEnabled() const { return foliageImpostors_; }
  void setHzbGpuCullingEnabled(bool enabled) { hzbGpuCullingEnabled_ = enabled; }
  bool hzbGpuCullingEnabled() const { return hzbGpuCullingEnabled_; }
  bool hzbGpuCullingActive() const { return hzbGpuCullingActive_; }
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
  // Policy-controlled and overrideable via aether.lod_selection. Disabling
  // adaptive selection fixes every group at LOD0; it never submits all stored
  // levels simultaneously.
  void setLodSelectionEnabled(bool enabled) { lodSelectionEnabled_ = enabled; }
  bool lodSelectionEnabled() const { return lodSelectionEnabled_; }
  // Screen-space error budget in pixels and the hysteresis band ratio (see
  // renderer::selectLodLevel) -- data-driven budgets, never hardcoded
  // per-scene (ADR-014).
  void setLodPixelErrorBudget(float budget) { lodPixelErrorBudget_ = budget; }
  void setCoverageLodPixelErrorBudget(float budget) {
    coverageLodPixelErrorBudget_ = budget;
  }
  void setLodHysteresisBandRatio(float ratio) { lodHysteresisBandRatio_ = ratio; }
  float lodPixelErrorBudget() const { return lodPixelErrorBudget_; }
  float coverageLodPixelErrorBudget() const { return coverageLodPixelErrorBudget_; }
  renderer::GpuCostIsolation gpuCostIsolation() const { return gpuCostIsolation_; }
  const renderer::VisibilityTelemetry &visibilityTelemetry() const {
    return visibilityTelemetry_;
  }
  const profiler::RenderPhaseTimings &lastFrameTimings() const { return lastFrameTimings_; }
  float currentRenderScale() const { return dynamicResolution_.scale(); }
  u32 currentRenderWidth() const { return renderWidth(); }
  u32 currentRenderHeight() const { return renderHeight(); }
  rhi::DeviceMemorySnapshot deviceMemorySnapshot() const {
    return memoryAllocator_ != nullptr ? memoryAllocator_->deviceMemorySnapshot()
                                       : rhi::DeviceMemorySnapshot{};
  }

private:
  bool editorBackground_=false;
  float sceneNearPlane_ = 0, sceneFarPlane_ = 0, sceneFieldOfView_=0;
  float previousSceneProjection_[4]{};
  float sceneFieldOfView() const {return sceneFieldOfView_>0?sceneFieldOfView_:visibilitySettings_.verticalFieldOfViewRadians;}
  float sceneNearPlane() const { return sceneNearPlane_ > 0 ? sceneNearPlane_ : dirtRoadResources_.header().nearPlane; }
  float sceneFarPlane() const { return sceneFarPlane_ > sceneNearPlane_ ? sceneFarPlane_ : dirtRoadResources_.header().farPlane; }
  ui::UiRect sceneViewport_{};
  std::vector<renderer::MapDrawState> pendingScene_;
  std::vector<renderer::MapDrawRecord> sourceMapDraws_;
  bool commitAuthoredScene();
  std::vector<u8> authoredVisibility_, authoredShadows_;
  std::vector<renderer::MaterialOverride> authoredMaterials_;
  float environmentAdjustment_[4]{1,1,1,0};

  void applyRuntimeRenderingPolicy(const renderer::ResolvedRenderingPolicy &policy,
                                   bool preserveDynamicScale);
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
  bool createSpectralWaterResources();
  bool createSkyPipeline();
  bool createRuntimeHudPipeline();
  // Carrega os atlas do APK e monta a pipeline. Falhar aqui NÃO derruba o
  // renderer: uma cena sem interface ainda é uma cena, e o log diz o motivo.
  void createUiRenderer(AAssetManager *assets);
  void recordUiOverlay(u32 imageIndex);
  bool createPostResources();
  void destroyPostResources();
  void recordPostProcess(u32 imageIndex, const platform::FreeCameraState &camera);
  bool recordTemporalHistoryCopy(u32 imageIndex);
  bool createShadowResources();
  void destroyShadowResources();
  void recordShadowPass(const platform::FreeCameraState &camera);
  bool createHzbResources();
  void destroyHzbResources();
  bool createHzbPipeline(const u32 *vertSpirv, u32 vertSpirvSize, const u32 *fragSpirv, u32 fragSpirvSize,
                         u32 pushConstantBytes, VkPipelineLayout &outLayout, VkPipeline &outPipeline);
  // Records the full reduction chain (depth -> base level -> coarser levels)
  // into commandBuffer_. Must be called after the main render pass's
  // vkCmdEndRenderPass and before the frame's vkQueueSubmit; depthImage_ must
  // already be in VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL (the shared
  // final state used by HZB and temporal resolve after the main pass).
  void recordHzbReductionPass(const platform::FreeCameraState &camera);
  // Reads back the pyramid built by the PREVIOUS frame's recordHzbReductionPass
  // into hzbPyramid_. Called once near the top of drawFrame, at the same point
  // gpuFrameTimer_.collectPrevious() reads last frame's GPU timestamps -- the
  // acquireNextImage fence wait already guarantees that copy fully landed, so
  // this never introduces a new stall (see PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md).
  void readHzbPyramidFromPreviousFrame();
  // Oclusão GPU-driven (ADR-016, consumidor C2). Os recursos vivem ao lado dos
  // do HZB porque dependem das mesmas imagens; createHzbResources() chama a
  // criação no fim, e destroyHzbResources() a destruição no início.
  bool createDrawCullResources();
  void destroyDrawCullResources();
  bool createDrawCompactionResources();
  void destroyDrawCompactionResources();
  // Grava o segundo kernel do estagio de visibilidade. Chamado logo depois de
  // recordDrawCullDispatch e, como ele, fora de qualquer render pass.
  void recordDrawCompactDispatch();
  // Preenche o buffer de lotes a partir dos lotes que a CPU acabou de montar.
  // Falso quando os lotes nao cabem no contrato: o frame entao submete pela
  // lista original, que continua correta, apenas com os buracos.
  bool publishCompactionBatches();
  // Grava o dispatch que escreve o instanceCount dos comandos indiretos. Tem de
  // ficar FORA de qualquer render pass e ANTES do passe opaco. A contagem
  // despachada é a capacidade da lista de comandos, não o número de comandos
  // deste frame: a lista só é construída dentro do render pass, e um dispatch
  // já gravado não pode reler push constants. Registros de sobra ficam com
  // flags=0 e o kernel os ignora sem escrever nada.
  void recordDrawCullDispatch(const platform::FreeCameraState &camera);
  // Telemetria dos contadores atômicos do kernel, lida um frame depois no mesmo
  // ponto de collectPrevious(). Diagnóstico: nunca realimenta uma decisão.
  void readDrawCullTelemetryFromPreviousFrame();
  // Frustum do frame. Duas etapas o consomem (o dispatch de culling, antes do
  // render pass, e a seleção de LOD/visibilidade dentro dele) e as duas têm de
  // enxergar exatamente o mesmo volume.
  renderer::PerspectiveFrustum buildFrameFrustum(const platform::FreeCameraState &camera) const;
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
  // Água usa um subpass próprio: lê o depth opaco como input attachment e
  // compõe transmissão/reflexão sem uma cópia full-resolution da cena.
  VkPipeline waterPipeline_ = VK_NULL_HANDLE;
  bool waterSubpassActive_ = false;
  bool waterAuthoringEnabled_ = false;
  // Uma grade de agua camera-relative e finita por construcao. Quando a
  // camera olha quase paralela ao plano, a cunha entre sua ultima aresta e o
  // horizonte geometrico deve receber o prolongamento refletido do ceu, nao a
  // metade inferior do panorama HDRI (que aparece como uma faixa chapada).
  bool cameraWaterHorizonFillActive_ = false;
  // Distance-material LOD pipelines. They keep the same geometry/material and
  // remove only normal-map work after an entire draw bound leaves the global
  // normal-detail radius.
  VkPipeline opaqueDistantPipeline_ = VK_NULL_HANDLE;
  VkPipeline coverageDistantPipeline_ = VK_NULL_HANDLE;
  VkPipeline transparentDistantPipeline_ = VK_NULL_HANDLE;
  // Lazily populated only for feature combinations present in the cooked map.
  // Each specialization removes absent normal/MR/emissive paths before driver
  // optimization; arrays remain backend implementation detail, not public API.
  VkPipeline opaqueMaterialPipelines_[renderer::MaterialFeatureVariantCount]{};
  VkPipeline coverageMaterialPipelines_[renderer::MaterialFeatureVariantCount]{};
  VkPipeline transparentMaterialPipelines_[renderer::MaterialFeatureVariantCount]{};
  VkDescriptorSetLayout environmentSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool environmentPool_ = VK_NULL_HANDLE;
  VkDescriptorSet environmentSet_ = VK_NULL_HANDLE;
  rhi::VulkanBuffer environmentUniform_{};
  VkPipelineLayout skyPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline skyPipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout runtimeHudPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline runtimeHudPipeline_ = VK_NULL_HANDLE;
  static constexpr u32 kMaxFramebuffers = 8;
  VkRenderPass postRenderPass_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout postSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool postDescriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet postDescriptorSet_ = VK_NULL_HANDLE;
  VkPipelineLayout postPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline postPipeline_ = VK_NULL_HANDLE;
  rhi::VulkanImage postSceneColor_{};
  rhi::VulkanImage postHistory_{};
  rhi::VulkanSampler postSampler_{};
  rhi::VulkanSampler postDepthSampler_{};
  VkFramebuffer postFramebuffers_[kMaxFramebuffers]{};
  bool temporalAaActive_ = false;
  // Image layout and color validity are separate states. The descriptor is
  // statically used by the temporal shader even on its first-frame branch, so
  // the image must already be shader-readable before it contains history.
  bool temporalHistoryLayoutInitialized_ = false;
  bool temporalHistoryInitialized_ = false;
  platform::FreeCameraState temporalPreviousCamera_{};
  float temporalCurrentJitter_[2]{};
  float temporalPreviousJitter_[2]{};
  u64 temporalFrameIndex_ = 0;
  VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
  VkRenderPass shadowCachedRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
  VkFramebuffer shadowCachedFramebuffer_ = VK_NULL_HANDLE;
  VkPipelineLayout shadowPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline shadowOpaquePipeline_ = VK_NULL_HANDLE;
  VkPipeline shadowMaskedPipeline_ = VK_NULL_HANDLE;
  rhi::VulkanImage shadowAtlas_{};
  rhi::VulkanSampler shadowSampler_{};
  VkFormat shadowDepthFormat_ = VK_FORMAT_UNDEFINED;
  renderer::ShadowCascade shadowCascades_[renderer::MaximumShadowCascades]{};
  u32 shadowCascadeCount_ = 0;
  u32 shadowCascadeDirtyMask_ = 0xffffffffu;
  bool shadowCacheInitialized_ = false;
  u32 shadowCandidateDraws_ = 0;
  u32 shadowSubmittedDraws_ = 0;
  u32 shadowRenderedCascades_ = 0;
  u64 shadowCacheHitFrames_ = 0;

  VkFramebuffer framebuffers_[kMaxFramebuffers]{};
  u32 framebufferCount_ = 0;

  u32 renderTargetWidth() const {
    return std::max(1u, static_cast<u32>(static_cast<float>(swapchain_->width()) *
                                        renderingPolicy_.resolutionScale));
  }
  u32 renderTargetHeight() const {
    return std::max(1u, static_cast<u32>(static_cast<float>(swapchain_->height()) *
                                        renderingPolicy_.resolutionScale));
  }
  u32 renderWidth() const {
    return dynamicResolution_.scale() >= renderingPolicy_.resolutionScale - 1.0e-4f
               ? renderTargetWidth()
               : std::min(renderTargetWidth(), renderer::scaledRenderExtent(
                                                   swapchain_->width(), dynamicResolution_.scale()));
  }
  u32 renderHeight() const {
    return dynamicResolution_.scale() >= renderingPolicy_.resolutionScale - 1.0e-4f
               ? renderTargetHeight()
               : std::min(renderTargetHeight(), renderer::scaledRenderExtent(
                                                    swapchain_->height(), dynamicResolution_.scale()));
  }

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
  rhi::VulkanImage waterDetailTexture_{};
  rhi::VulkanBuffer routeVertices_,routeIndices_;
  std::vector<std::array<float,4>> authoredWaterLayers_;
  rhi::VulkanSampler waterDetailSampler_{};
  rhi::VulkanSampler baseSampler_{};
  VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
  u32 instanceCount_ = 0;
  u32 drawnInstanceCount_ = 0;
  bool scenePreview_ = false;
  bool emptyScene_ = false;
  bool materialPreview_ = false;
  bool dirtRoadPreview_ = false;
  MaterialPreviewResources materialResources_;
  DirtRoadResources dirtRoadResources_;
  std::vector<u32> dirtTextureSlots_;
  std::vector<VkDescriptorSet> dirtMaterialSets_;
  std::vector<u32> solidDrawOrder_;
  std::vector<u32> coverageDrawOrder_;
  std::vector<u32> transparentDrawOrder_;
  std::vector<u32> waterDrawOrder_;
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
    bool distantMaterial = false;
    // Posicao deste lote no buffer de contagens da compactacao. Atribuida em
    // publishCompactionBatches, na mesma varredura que preenche o buffer, para
    // que a submissao nao precise recalcular a correspondencia lote->contador.
    u32 compactionSlot = 0;
  };
  std::vector<VkDrawIndexedIndirectCommand> indirectCommands_;
  std::vector<IndirectBatch> indirectSolidBatches_;
  std::vector<IndirectBatch> indirectCoverageBatches_;
  // Reaproveitado entre frames para que montar os lotes da compactacao nao
  // aloque no laco de frame, pela mesma razao que indirectCommands_ persiste.
  std::vector<renderer::GpuCompactBatch> compactionBatchScratch_;
  bool useMultiDrawIndirect_ = false;
  // Ligado por padrao desde o A/B intercalado de 02/09: com PCF de hardware, a
  // ordem preservada vale -2,14 ms na pose do hotspot (controles reproduzindo
  // em 0,049 e 0,062 ms). aether.disable_depth_ordered_batches desliga para
  // A/B; nao existe caminho de qualidade dependendo disto.
  bool depthOrderedBatches_ = true;
  // aether.disable_foliage_impostors desliga; ver setFoliageImpostorsEnabled.
  bool foliageImpostors_ = true;
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
  bool gpuTimingEnabled() const {
    return frameProfilingEnabled_ || adpfGpuTimingEnabled_ ||
           renderingPolicy_.dynamicResolution.enabled;
  }
  bool adpfGpuTimingEnabled_ = false;
  bool coveragePrepassEnabled_ = true;
  bool runtimeHudEnabled_ = false;

  // Interface do editor. Os bytes dos assets ficam vivos porque UiFont e
  // UiIconAtlas apontam para dentro deles (ver a nota de tempo de vida em
  // ui_font.h); só os pixels já foram para a GPU.
  std::vector<u8> uiFontBytes_;
  std::vector<u8> uiIconBytes_;
  ui::UiFont uiFont_{};
  ui::UiIconAtlas uiIcons_{};
  rhi::VulkanUiRenderer uiRenderer_{};
  VkRenderPass uiRenderPass_=VK_NULL_HANDLE;
  std::vector<VkFramebuffer> uiFramebuffers_;
  std::vector<ui::UiInstance> uiInstances_;
  float uiSurfaceWidth_ = 0.0f;
  float uiSurfaceHeight_ = 0.0f;
  renderer::PerspectiveVisibilitySettings visibilitySettings_{};
  renderer::VisibilityTelemetry visibilityTelemetry_{};
  u64 renderedFrameCount_ = 0;
  renderer::GpuCostIsolation gpuCostIsolation_ = renderer::GpuCostIsolation::Full;
  profiler::RenderPhaseTimings lastFrameTimings_{};
  // Limites para os quais os recursos foram realmente criados. A política
  // ativa abaixo pode apenas reduzir estes limites durante a época atual.
  renderer::ResolvedRenderingPolicy resourceRenderingPolicy_{};
  renderer::ResolvedRenderingPolicy renderingPolicy_{};
  renderer::WaterProfile waterProfile_ = renderer::defaultOceanWaterProfile();
  renderer::WaterShadingSettings waterShading_{};
  std::array<renderer::MapDrawUpdate, 128> pendingMapPoses_{};
  u32 pendingMapPoseCount_ = 0;
  // Estado por instância que NÃO é pose: material, visibilidade e sombra. Ele
  // muda durante o Play sem que a hierarquia mude -- um script escrevendo
  // `base_color`, por exemplo -- e a publicação de poses sozinha o deixaria
  // congelado no valor do último `queueMapScene`. As camadas de água NÃO entram:
  // são estado de autoria de água, e a simulação escreve nos mesmos lotes depois
  // da extração -- republicá-las aqui sobrescreveria o que ela acabou de
  // calcular.
  struct AuthoredInstanceState {
    renderer::MaterialOverride material{};
    bool visible = true;
    bool castShadow = true;
  };
  std::vector<AuthoredInstanceState> pendingAuthoredState_;
  bool pendingAuthoredStateValid_ = false;
  std::vector<renderer::SceneLight> sceneLights_;
  renderer::LightBudgetReport lightBudget_{};
  std::vector<u8> dynamicMapDraws_;
  bool spectralWaterEnabled_=false;
  bool wideWaterSlopes_=false;
  renderer::WaterCostIsolation waterCostIsolation_=renderer::WaterCostIsolation::Full;
  renderer::WaterSpectralControls waterSpectralControls_{};
  renderer::WaterSpectralClock waterSpectralClock_{};
  float authoredWaterTime_=-1;
  std::vector<std::array<float,4>> authoredWaterFlowDepth_;
  u32 spectralWaterCount_=0;
  u64 waterSpectrumRevision_=0;
  float spectralWaterBoundsExpansion_=0;
  u64 waterCascadeBufferBudget_=4ull*1024*1024;
  std::vector<renderer::WaterCascadeSettings> waterCascadeSettings_;
  using WaterComputeBank=std::array<rhi::VulkanWaterSpectralCompute,renderer::MaximumWaterCascades>;
  std::unique_ptr<WaterComputeBank> waterSpectralCompute_=std::make_unique<WaterComputeBank>();
  renderer::WaterInteractionField waterInteractions_{};
  // Alturas da ondulação, num buffer mapeado escrito por quadro. É um storage
  // buffer e não uma imagem pelo mesmo motivo das cascatas espectrais: o
  // caminho de upload existente é síncrono e pertence à importação, e abrir
  // imagem nova exigiria staging, transição de layout e barreira por quadro
  // para dados que o vértice lê com interpolação manual de qualquer forma.
  rhi::VulkanBuffer waterRippleBuffer_{};
  u32 waterRippleResolution_ = 0;
  float waterRippleCentre_[2]{0.0f, 0.0f};
  float waterRippleArea_ = 0.0f;
  float waterRippleGain_ = 0.0f;
  float waterDisplacementCapacity_ = 5.0f;
  float waterBaseHeight_ = 0.0f;
  renderer::DynamicResolutionController dynamicResolution_{};

  // HZB (Hi-Z) occlusion culling -- see setHzbOcclusionEnabled() above and
  // native/renderer/hzb_visibility.h. All levels share one small render pass
  // (R32_SFLOAT, MAX-reduction fragment shaders); level 0 samples depthImage_,
  // level i>0 samples level i-1. Recreated whenever the swapchain-dependent
  // resources are (createHzbResources()/destroyHzbResources() mirror
  // createDepthImage()/destroyFramebuffers()'s lifecycle exactly).
  bool hzbOcclusionEnabled_ = false;
  bool hzbComputeEnabled_ = false;
  bool hzbComputeReadbackValidationEnabled_ = false;
  bool hzbComputeActive_ = false;
  bool hzbComputeImagesInitialized_ = false;
  bool hzbReadbackRecordedThisFrame_ = false;
  bool hzbComputeValidationLogged_ = false;
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
    rhi::VulkanComputeKernel computeKernel{};
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

  // Oclusão GPU-driven (ver setHzbGpuCullingEnabled). Os três buffers são
  // host-visible pela mesma razão que instanceBuffer_/indirectBuffer_ já são:
  // a CPU os preenche por frame e o custo de um staging dedicado superaria os
  // ~20 KiB em jogo. drawCullStateBuffer_ é o único persistente entre frames.
  bool hzbGpuCullingEnabled_ = false;
  bool hzbGpuCullingActive_ = false;
  bool drawCullDispatchedThisFrame_ = false;
  bool drawCullContractLogged_ = false;
  rhi::VulkanComputeKernel drawCullKernel_{};
  rhi::VulkanBuffer drawCullRecordBuffer_{};
  rhi::VulkanBuffer drawCullStateBuffer_{};
  rhi::VulkanBuffer drawCullTelemetryBuffer_{};
  u32 drawCullCapacity_ = 0;
  // [0]=testados [1]=ocluidos [2]=revividos [3]=visiveis, do dispatch do frame
  // ANTERIOR. Diagnostico: nunca realimenta uma decisao deste frame.
  std::array<u32, 4> drawCullTelemetry_{};

  // Compactacao dos comandos indiretos (renderer/gpu_draw_compaction.h). E o
  // consumidor final da oclusao: sem ela um draw ocluido continua sendo buscado
  // e decodificado pelo front-end com instanceCount zero. Depende de
  // VK_KHR_draw_indirect_count; sem a extensao o estagio fica inativo e o frame
  // continua exatamente como antes -- correto, apenas com os buracos.
  bool drawCompactionActive_ = false;
  bool drawCompactionDispatchedThisFrame_ = false;
  bool drawCompactionContractLogged_ = false;
  rhi::VulkanComputeKernel drawCompactKernel_{};
  rhi::VulkanBuffer compactedIndirectBuffer_{};
  rhi::VulkanBuffer compactBatchBuffer_{};
  rhi::VulkanBuffer compactCountBuffer_{};
  u32 drawCompactionBatchCapacity_ = 0;
  // Quantos lotes o frame publicou. Zero significa que a submissao deste frame
  // usa a lista original, nao que nao ha nada a desenhar.
  u32 drawCompactionBatchCount_ = 0;

  // LOD selection (see setLodSelectionEnabled above). Built once at load in
  // initialize() (see the lodGroupId bucketing next to
  // solidDrawOrder_/coverageDrawOrder_'s own construction); lodGroups_[i] is
  // a list of draw indices for one lodGroupId, sorted by lodLevel ascending,
  // present only for groups with more than one level. lodGroupHysteresis_ is
  // index-aligned with lodGroups_ (NOT keyed by lodGroupId directly).
  bool lodSelectionEnabled_ = false;
  float lodPixelErrorBudget_ = 2.0f;
  float coverageLodPixelErrorBudget_ = 32.0f;
  float lodHysteresisBandRatio_ = 0.75f;
  // One imported lodGroupId may contain MANY spatial chunks per LOD level.
  // Grouping levels as buckets (instead of assuming one draw == one level)
  // preserves chunk-level frustum/indirect submission after LOD selection.
  std::vector<renderer::LodRenderGroup> lodGroups_;
  // Alpha-tested vegetation writes depth and uses the same dither-capable
  // shader, but remains in a distinct material pipeline.  Keep its LOD
  // groups separate so filtering never changes opaque/coverage ordering.
  std::vector<renderer::LodRenderGroup> coverageLodGroups_;
  // Built once at load: the subset of solidDrawOrder_ that belongs to no
  // multi-level group (always a candidate, LOD selection never touches it).
  std::vector<u32> ungroupedSolidDrawOrder_;
  std::vector<u32> ungroupedCoverageDrawOrder_;
  // Maximum-quality fallback when adaptive selection is disabled.  A package
  // stores every discrete level, but "LOD off" must mean LOD0 only -- drawing
  // all levels simultaneously duplicates the same surface and is never a
  // legitimate quality mode.
  std::vector<u32> levelZeroSolidDrawOrder_;
  std::vector<u32> levelZeroCoverageDrawOrder_;
  // Scratch, rebuilt every frame LOD selection runs: ungroupedSolidDrawOrder_
  // plus each group's currently-active (and, mid-transition, neighbor)
  // draw. Reserved once in initialize(); zero-alloc per frame like the
  // visible*DrawOrder_ scratch lists.
  std::vector<u32> lodFilteredSolidDrawOrder_;
  std::vector<u32> lodFilteredCoverageDrawOrder_;
  bool lodSelectionAppliedLastFrame_ = false;
};

} // namespace ae::platform::android
