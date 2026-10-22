#pragma once

#include "renderer/water_shading.h"
#include "renderer/water_field.h"
#include "renderer/water_ripples.h"
#include "renderer/map_draw_update.h"
#include "renderer/punctual_lights.h"
#include "renderer/light_probes.h"
#include "renderer/reflection_probes.h"
#include "renderer/scene_environment.h"
#include "renderer/color_grading.h"
#include "renderer/physical_atmosphere.h"
#include "renderer/grid_plan.h"

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
#include "rhi/temporal_upscaler.h"
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
#include "renderer/texture_streaming.h"
#include "renderer/texture_level_reader.h"
#include "renderer/runtime_hud.h"
#include "renderer/shadow_cascades.h"
#include "renderer/shadow_atlas.h"
#include "renderer/water_surface.h"
#include "platform/android/material_preview_resources.h"
#include "platform/android/material_standalone_preview_resources.h"
#include "renderer/render_view.h"
#include "platform/android/dirt_road_resources.h"
#include "platform/free_camera_controller.h"

#include <array>
#include <cmath>
#include <deque>
#include <vector>

namespace ae::platform::android {

struct PostPushConstants;

// Diagnóstico das entradas temporais. Histórico e rejeição só existem no TAA
// nativo; profundidade, vetor e máscaras também valem com Arm ASR e FSR 2.
enum class TemporalDebugView : u32 { Final = 0, Depth = 1, HistoryWeight = 2,
                                     RejectedDepth = 3, Motion = 4, Reactivity = 5,
                                     Composition = 6 };

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
  void setTemporalDebugView(TemporalDebugView view) { temporalDebugView_ = view; }
  bool temporalDebugAvailable() const {
    return temporalInputsActive_ &&
           (temporalUpscaler_.ready() || (temporalAaActive_ && sceneOrthographicHalfHeight_ == 0.0f));
  }
  bool motionVectorsAvailable() const { return motionVectorsActive_ && temporalDebugAvailable(); }
  bool temporalHistoryDiagnosticsAvailable() const {
    return temporalAaActive_ && sceneOrthographicHalfHeight_ == 0.0f;
  }
  // G6-B: o que o aparelho permite e o que rodou no último quadro. O estado
  // executado é separado da política: um contexto pode falhar depois dela.
  // Chamado antes de `initialize`, a partir do device e dos formatos reais,
  // para a política já nascer com o motivo de recusa.
  static void probeTemporalUpscalers(const rhi::VulkanDevice &device,
                                     renderer::TemporalUpscalerAvailability &armAsr,
                                     renderer::TemporalUpscalerAvailability &fsr2);
  renderer::UpscalingFilter executedUpscaler() const noexcept { return executedUpscaler_; }
  renderer::TemporalUpscalerAvailability executedUpscalerStatus() const noexcept { return executedUpscalerStatus_; }
  bool nativeTaaExecuted() const noexcept { return temporalAaActive_ && temporalHistoryInitialized_; }
  // R4: atlas de prévia de texturas composto pelo editor; enviado à GPU no
  // início da gravação da interface do próximo quadro.
  void setUiGuiAtlas(std::span<const u8> rgba,u32 size,u64 revision);
  void setUiPreviewAtlas(std::span<const u8> rgba, u32 width, u32 height);
  void setUiImmediateAtlas(std::span<const u8> rgba,u32 width,u32 height);
  void requestCameraPreview(const renderer::RenderViewSnapshot &view) {pendingPreview_=view;}
  void closeCameraPreview() {
    pendingPreview_={};
    if(previewColor_.isReady() || submittedPreview_.requestId) previewCloseRequested_=true;
  }
  bool takeCameraPreviewCompletion(renderer::RenderViewSnapshot &view,bool &success) {
    if(!completedPreview_.requestId) return false;
    view=completedPreview_;success=previewCompletionSuccess_;completedPreview_={};return true;
  }
  const std::string &cameraPreviewDiagnostic() const {return previewDiagnostic_;}
  // PK-6: sondas de reflexo e de luz (platform/android/instanced_probes.inl).
  // A cena publica as sondas acesas junto das luzes; os recursos assados e o
  // conjunto tetraedralizado das sondas de luz chegam quando o editor os carrega.
  struct ProbeCaptureRequest {
    u64 id=0;
    u32 kind=0; // 0 reflexo (bake de Reflection Probe), 1 irradiância (sonda de luz), 2 reflexo do céu
    u64 objectId=0,instance=0; // a sonda residente que já mostra o resultado
    float position[3]{};
    u32 resolution=128;
    float nearClip=.3f,farClip=1000;
    u32 cullingMask=~0u;
    bool staticOnly=true;
    bool solidBackground=false;
    float background[3]{};
    bool hdr=true;
  };
  struct ProbeCaptureResult {
    u64 id=0;u32 kind=0;bool success=false;std::string error;
    renderer::ReflectionProbeTexture texture; // kind 0
    renderer::CubeFaces faces;               // kind 1, radiância linear
  };
  struct ProbeFrameStats {
    u32 sceneProbes=0,residentProbes=0,readyProbes=0,facesCaptured=0,mipsFiltered=0;
    u32 drawsUpdated=0,drawsWithLightProbes=0,drawsWithReflection=0,lightProbes=0,tetrahedra=0;
    float cpuMs=0;
  };
  void queueSceneReflectionProbes(std::span<const renderer::SceneReflectionProbe> probes,bool playing);
  void setReflectionProbeTextures(std::vector<std::shared_ptr<const renderer::ReflectionProbeTexture>> textures);
  void setLightProbeSet(std::shared_ptr<const renderer::LightProbeSet> set);
  bool requestProbeCapture(const ProbeCaptureRequest &request);
  bool takeProbeCaptureResult(ProbeCaptureResult &out);
  bool probeCaptureBusy() const noexcept {return !probeCaptureQueue_.empty()||probeJob_.active||probeReadbackPending_;}
  const ProbeFrameStats &probeStats() const noexcept {return probeStats_;}
  // Conta-gotas do editor (Unity: eyedropper da janela de cor): um pixel da
  // imagem final, depois da interface, copiado para a CPU e lido no quadro
  // seguinte. Coordenadas da interface (as mesmas do toque). Falso quando a
  // swapchain deste aparelho não aceita cópia (sem TRANSFER_SRC).
  bool pixelSampleSupported() const;
  bool requestPixelSample(float x,float y);
  // Cor lida (bytes como exibidos, sRGB, em RGBA); uma vez por pedido.
  bool takePixelSample(u8 (&rgba)[4]) {
    if(!pixelSampleReady_) return false;
    for(u32 i=0;i<4;++i) rgba[i]=pixelSampleValue_[i];
    pixelSampleReady_=false;return true;
  }
  const std::string &autoExposureDiagnostic() const {return autoExposureDiagnostic_;}
  void setAutoExposureSceneEpoch(u64 epoch,u32 cameraEntity=0) {
    autoExposureSceneEpoch_=epoch;autoExposureCameraEntity_=cameraEntity;
  }
  void setAutoExposurePaused(bool paused) {autoExposurePaused_=paused;}
  // Tamanho da superficie NO ESPACO EM QUE AS INSTANCIAS FORAM CONSTRUIDAS --
  // pixels logicos, nao fisicos. Zero volta a usar a extensao do display, que e
  // o comportamento certo so quando as duas escalas coincidem.
  void setUiSurfaceSize(float width, float height);
  // Normalized logical display rectangle; empty restores a full display scene.
  void setSceneViewport(const ui::UiRect &rect) {
    const auto next = ui::intersect(rect, {0,0,1,1});
    if(next.x!=sceneViewport_.x || next.y!=sceneViewport_.y ||
       next.width!=sceneViewport_.width || next.height!=sceneViewport_.height) {
      temporalHistoryInitialized_=false;
      hzbPyramidValid_=false;hzbPyramidCameraValid_=false;hzbRecordedCameraValid_=false;
      hzbPreviousFrameEligible_=false;
    }
    sceneViewport_ = next;
  }
  ui::UiRect physicalSceneViewport() const;
  float sceneAspectRatio() const;
  // A zero pair restores the imported camera range (Play/runtime).
  void setSceneClipPlanes(float nearPlane, float farPlane) {
    if(sceneNearPlane_!=nearPlane || sceneFarPlane_!=farPlane) temporalHistoryInitialized_=false;
    sceneNearPlane_ = nearPlane;
    sceneFarPlane_ = farPlane;
  }
  // Zero restores the renderer's configured field of view.
  void setSceneFieldOfView(float radians) {
    if(sceneFieldOfView_!=radians) temporalHistoryInitialized_=false;
    sceneFieldOfView_=radians;
  }
  // Zero selects perspective; positive values are a world-space half height.
  // Orthographic currently uses spatial AA and CPU visibility (no GPU HZB).
  void setSceneOrthographicHalfHeight(float halfHeight);
  void setEditorBackground(bool enabled) {
    if(editorBackground_!=enabled) {
      editorSceneCacheValid_=false;
      hzbPyramidValid_=false;hzbPyramidCameraValid_=false;hzbPreviousFrameEligible_=false;
    }
    editorBackground_=enabled;
  }
  void setEditorViewportOptions(bool lighting,bool effects,bool sky,bool fog,bool post) {
    editorSceneLighting_=lighting;editorSceneEffects_=effects;editorSceneSky_=sky;
    editorSceneFog_=fog;editorScenePost_=post;
  }
  void setEnvironmentAdjustment(const float values[4]) { std::copy(values,values+4,environmentAdjustment_); }
  void setSceneEnvironment(const renderer::SceneEnvironment &environment) {
    if (environment.valid()) sceneEnvironment_ = environment;
  }
  void setEnvironmentMaps(std::span<const std::pair<resources::AssetGuid,renderer::SharedEnvironmentMap>> maps) {
    editorSceneCacheValid_=false;
    environmentMaps_.assign(maps.begin(),maps.end());
  }
  const std::string &environmentMapDiagnostic() const { return environmentMapDiagnostic_; }
  const MaterialTextureResidencyReport &packageTextureResidency() const { return dirtRoadResources_.packageTextureResidency(); }
  const MaterialTextureResidencyReport &authoringTextureResidency() const { return dirtRoadResources_.authoringTextureResidency(); }
  const std::string &textureResidencyDiagnostic() const { return dirtRoadResources_.textureResidencyDiagnostic(); }
  void queueSceneEnvironmentVolumes(std::span<const renderer::SceneEnvironmentVolume> volumes) {
    sceneEnvironmentVolumes_.assign(volumes.begin(),volumes.end());
    if(sceneEnvironmentVolumes_.empty()) sceneEnvironment_={};
  }
  void setSceneEnvironmentLayerMask(u32 mask) { sceneEnvironmentLayerMask_=mask; }
  // PK-7: o que a câmera de jogo decide sobre o quadro principal. Fora do Play
  // o shell publica os padrões (céu, todas as camadas, pós/sombras/HDR ligados).
  // Culling Mask: bit por camada de objeto; um objeto fora dela não desenha nem
  // projeta sombra nesta vista.
  void setSceneCullingMask(u32 mask) { sceneCullingMask_=mask; }
  // 0 céu, 1 cor sólida (RGB linear), 2 não inicializado (sem céu nem cor).
  // `partial`: o Viewport Rect não cobre a vista inteira (o resto fica preto).
  void setSceneBackground(u32 mode,const float color[3],bool partial=false) {
    sceneBackground_=mode<=2?mode:0;sceneBackgroundPartial_=partial;
    if(color) std::copy(color,color+3,sceneBackgroundColor_);
  }
  void setScenePostProcessing(bool enabled) { scenePostProcessing_=enabled; }
  void setSceneShadows(bool enabled) {
    if(sceneShadows_!=enabled) invalidateStaticShadowCache();
    sceneShadows_=enabled;
  }
  void setSceneHdr(bool enabled) { sceneHdr_=enabled; }
  // 0 do projeto, 1 nenhum, 2 FXAA. TAA continua sendo do projeto.
  void setSceneAntialiasing(u32 mode) {
    if(mode!=sceneAntialiasing_) temporalHistoryInitialized_=false;
    sceneAntialiasing_=mode<=2?mode:0;
  }
  // Gate Fit "Nenhum": a projeção usa a proporção do sensor. Zero = da vista.
  void setSceneAspectOverride(float aspect) {
    const float next=std::isfinite(aspect)&&aspect>0?aspect:0.f;
    if(next!=sceneAspectOverride_) temporalHistoryInitialized_=false;
    sceneAspectOverride_=next;
  }
  const renderer::SceneEnvironmentBlendReport &environmentBlendReport() const noexcept {
    return environmentBlendReport_;
  }
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
  // A grade editorial. Ela é desenhada DENTRO da cena, com teste e escrita de
  // profundidade, e não na lista de interface: é o que faz uma caixa opaca
  // esconder as linhas atrás dela.
  void setEditorGrid(const renderer::GridPlan &plan) {
    if(plan.enabled!=editorGrid_.enabled || plan.planeHeight!=editorGrid_.planeHeight ||
       plan.minorSpacing!=editorGrid_.minorSpacing || plan.majorSpacing!=editorGrid_.majorSpacing ||
       plan.minorOpacity!=editorGrid_.minorOpacity || plan.fadeDistance!=editorGrid_.fadeDistance)
      editorSceneCacheValid_=false;
    editorGrid_ = plan;
  }
  // Reconstrói a biblioteca de autoria com as primitivas internas MAIS a
  // geometria importada no aparelho, e refaz tudo o que depende da lista de
  // desenhos. Só por ação explícita do usuário: espera a GPU ficar ociosa antes
  // de trocar os buffers, porque trocá-los com um quadro em voo desenharia
  // memória liberada. Uma importação é rara e o usuário já está esperando.
  // Verdadeiro quando o aparelho amostra ASTC 4x4 (sRGB e UNORM) com filtro
  // linear: o importador então transcodifica KTX2 direto para blocos ASTC.
  bool supportsAstc4x4() const noexcept { return astc4x4_; }
  // R4: as pipelines de mapa descartam a face de trás por material (dupla face real).
  bool materialCulling() const noexcept { return materialCulling_; }
  // Deformação da biblioteca (G6-B): `skinInfluences` é paralelo a `vertices`
  // (16 bytes por vértice, vazio sem skin) e `drawJoints` a `draws` (juntas do
  // skin; zero = sem skin). `morphDeltas` traz os blend shapes (9 floats por
  // alvo e vértice) e, por desenho, `drawMorphOffsets` o início e
  // `drawMorphTargets` o número de alvos (zero = sem blend shapes).
  struct DeformationLibrary {
    std::span<const u8> skinInfluences;
    std::span<const u32> drawJoints;
    std::span<const float> morphDeltas;
    std::span<const u32> drawMorphOffsets, drawMorphTargets;
    // S3: níveis de detalhe dos desenhos recebidos (índices de desenho e de
    // faixa relativos a esta publicação), na ordem desenho → nível.
    std::span<const renderer::MeshLodLevel> meshLods;
  };
  bool rebuildAuthoringGeometry(std::span<const u8> vertices, std::span<const u32> indices,
                                std::span<const renderer::MapDrawRecord> draws,
                                std::span<const renderer::MapMaterialRecord> materials,
                                std::span<const renderer::SharedAuthoringTexture> textures = {},
                                const DeformationLibrary &deformation = {});
  // Desenhos com skin deformados pelo compute no último quadro.
  u32 profileSkinnedDraws() const noexcept { return skinnedDrawsThisFrame_; }


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
    return dirtRoadPreview_ ? static_cast<u32>(sourceMapDraws_.size()) : 0;
  }
  u32 profileMaterialCount() const {
    return dirtRoadPreview_ ? static_cast<u32>(dirtRoadResources_.materials().size()) : 0;
  }
  u32 profileTextureCount() const {
    return dirtRoadPreview_ ? dirtRoadResources_.textureCount() : 0;
  }
  u32 profileTriangleCount() const {
    return dirtRoadPreview_ ? profileSourceTriangleCount_ : 0;
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
  // Same-APK diagnostic comparison; never a quality setting.
  void setPostUiFusionEnabled(bool enabled) { postUiFusionEnabled_ = enabled; }
  bool postUiFusedLastFrame() const { return postUiFusedLastFrame_; }
  void setSpatialGeometryEnabled(bool enabled) { spatialGeometryEnabled_=enabled; }
  void setEditorSceneReuseEnabled(bool enabled) { editorSceneReuseEnabled_=enabled;editorSceneCacheValid_=false; }
  bool editorSceneReuseEnabled() const {return editorSceneReuseEnabled_;}
  bool editorSceneReusedLastFrame() const {return editorSceneReusedLastFrame_;}
  u32 spatialGeometryChunkCount() const { return static_cast<u32>(spatialGeometryChunks_.size()); }
  // Chave diagnóstica A/B. O padrão da engine permanece ativado; jogos não
  // precisam configurar nada para receber o caminho otimizado.
  void setCoveragePrepassEnabled(bool enabled) { coveragePrepassEnabled_ = enabled; }
  void setGpuCostIsolation(renderer::GpuCostIsolation mode) { gpuCostIsolation_ = mode; }
  // Diagnóstico do G1: desliga etapas do passe de pós para atribuir custo no
  // aparelho (1 neblina, 2 AO, 4 bloom, 8 FXAA, 16 nitidez). Nunca é opção de jogo.
  void setPostIsolation(u32 mask) { postIsolation_ = mask & 511u; }
  u32 postIsolation() const { return postIsolation_; }
  // G1: névoa resolvida uma vez por texel antes do pós. Ligada por padrão desde
  // o A/B de 17/10/2026; desligar só para comparação.
  void setFogResolveEnabled(bool enabled) { fogResolveEnabled_ = enabled; }
  bool fogResolvedLastFrame() const { return fogResolvedThisFrame_; }
  // Candidatas do G1/G2: bloom em cadeia de mips e AO em 1/2. Desligadas por
  // padrão até o A/B; precisam valer antes da criação dos recursos do pós.
  void setBloomChainEnabled(bool enabled) { bloomChainEnabled_ = enabled; }
  void setHalfAmbientOcclusionEnabled(bool enabled) { halfAmbientOcclusionEnabled_ = enabled; }
  // Bancada do G2: liga a LUT mesmo no Neutro, para medir o custo da leitura 3D
  // e comparar a imagem com o caminho sem LUT (aceite: até 3/255).
  void setColorGradingForced(bool forced) { colorGradingForced_ = forced; }
  u32 colorGradingRebuilds() const { return colorGradingRebuilds_; }
  void setOpaqueNoClipEnabled(bool enabled) { opaqueNoClipEnabled_ = enabled; }
  bool opaqueNoClipLastFrame() const { return opaqueNoClipLastFrame_; }
  void setFullDetailSamplingEnabled(bool enabled) { fullDetailSamplingEnabled_ = enabled; }
  bool fullDetailSamplingEnabled() const { return fullDetailSamplingEnabled_; }
  void setPointLightingSpecializationEnabled(bool enabled) { pointLightingSpecializationEnabled_ = enabled; }
  // G1: máscara por desenho das luzes pontuais que o alcançam (candidata, desligada por padrão).
  void setPunctualLightMaskEnabled(bool enabled) { punctualLightMaskEnabled_ = enabled; }
  bool punctualLightMaskEnabled() const { return punctualLightMaskEnabled_; }
  bool pointLightingSpecializationLastFrame() const { return pointLightingSpecializationLastFrame_; }
  // Chave diagnóstica de A/B, no mesmo espírito de GpuCostIsolation: força o
  // anexo de profundidade a ser alocado como render target comum, mesmo quando
  // o render graph provou que ninguém o lê. Existe para medir o que o caminho
  // memoryless entrega; nunca é um preset de qualidade.
  void setDisableTransientDepth(bool disabled) { disableTransientDepth_ = disabled; }
  void setRuntimeHudEnabled(bool enabled) { runtimeHudEnabled_ = enabled; }
  void setPlayPostUiFusionEnabled(bool enabled) { playPostUiFusionEnabled_ = enabled; }
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
  const renderer::ResolvedRenderingPolicy &activeRenderingPolicy() const noexcept {
    return renderingPolicy_;
  }
  // S3/S5: LOD na própria malha no último quadro: desenhos com níveis, quantos
  // estão num nível reduzido e os triângulos do nível 0 contra os escolhidos.
  struct MeshLodStats {u32 drawsWithLods=0,reducedDraws=0;u64 baseTriangles=0,selectedTriangles=0;};
  const MeshLodStats &meshLodStats() const noexcept { return meshLodStats_; }
  // Desenhos e triângulos realmente enviados no último quadro da cena (S5).
  u32 submittedDrawCalls() const noexcept { return visibilityTelemetry_.submittedDrawCalls; }
  u64 submittedTriangles() const noexcept { return visibilityTelemetry_.submittedTriangles; }
  // S2: estado do streaming de mipmaps no último quadro (Quality, Inspector, API).
  const renderer::TextureStreamingStats &textureStreamingStats() const noexcept { return textureStreamingStats_; }
  // Por textura de autoria, na ordem da publicação: mip pela tela e carregado.
  std::span<const u32> textureStreamingDesiredMips() const noexcept { return streamingPlan_.calculatedMip; }
  std::span<const u32> textureStreamingLoadedMips() const noexcept { return streamingLoaded_; }
  // Escolhas autorais por textura de autoria (perfil da fonte ou da textura),
  // na ordem da publicação. Tamanho diferente do publicado é ignorado.
  void setTextureStreamingParameters(std::span<const renderer::TextureStreamingParameters> parameters) {
    streamingParameters_.assign(parameters.begin(), parameters.end());
  }
  // Vista de depuração do streaming (a de Mipmap Streaming da Unity): tinge a
  // cor base de cada desenho pelo estado da textura de cor dele.
  void setTextureStreamingDebugView(bool enabled) { textureStreamingDebugView_ = enabled; }
  // Scene/transform systems call this when any static caster, material alpha or
  // sun configuration changes. A câmera não precisa invalidar manualmente: o
  // renderer testa contenção de cada cascata antes de reutilizá-la.
  void invalidateStaticShadowCache() {
    editorSceneCacheValid_=false;
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
  // Resolução interna da cena neste quadro, para o painel Qualidade.
  u32 sceneRenderWidth() const { return swapchain_ ? renderWidth() : 0u; }
  u32 sceneRenderHeight() const { return swapchain_ ? renderHeight() : 0u; }
  // Mede a GPU enquanto o painel Qualidade está aberto, mesmo sem escala dinâmica.
  void setEditorGpuTiming(bool enabled) { editorGpuTimingEnabled_ = enabled; }
  u32 currentRenderWidth() const { return renderWidth(); }
  u32 currentRenderHeight() const { return renderHeight(); }
  rhi::DeviceMemorySnapshot deviceMemorySnapshot() const {
    return memoryAllocator_ != nullptr ? memoryAllocator_->deviceMemorySnapshot()
                                       : rhi::DeviceMemorySnapshot{};
  }

private:
  bool editorBackground_=false;
  bool editorSceneLighting_=true,editorSceneEffects_=true,editorSceneSky_=true;
  bool editorSceneFog_=true,editorScenePost_=true;
  // Câmera de jogo (PK-7); ver setSceneCullingMask e vizinhos.
  u32 sceneCullingMask_=~0u,appliedCullingMask_=~0u;
  u32 sceneBackground_=0;
  bool sceneBackgroundPartial_=false;
  float sceneBackgroundColor_[3]{.0307f,.0742f,.1912f};
  bool scenePostProcessing_=true,sceneShadows_=true,sceneHdr_=true;
  u32 sceneAntialiasing_=0;
  float sceneAspectOverride_=0;
  // Os efeitos de pós da vista principal: os do volume e da política, salvo
  // quando o editor os desliga na vista de Cena ou a câmera os desliga.
  bool viewPostEnabled() const {return (!editorBackground_||(editorSceneEffects_&&editorScenePost_))&&scenePostProcessing_;}
  float sceneNearPlane_ = 0, sceneFarPlane_ = 0, sceneFieldOfView_=0;
  float previousSceneProjection_[5]{};
  float sceneOrthographicHalfHeight_=0;
  float sceneFieldOfView() const {return sceneFieldOfView_>0?sceneFieldOfView_:visibilitySettings_.verticalFieldOfViewRadians;}
  float sceneNearPlane() const { return sceneNearPlane_ > 0 ? sceneNearPlane_ : dirtRoadResources_.header().nearPlane; }
  float sceneFarPlane() const { return sceneFarPlane_ > sceneNearPlane_ ? sceneFarPlane_ : dirtRoadResources_.header().farPlane; }
  ui::UiRect sceneViewport_{};
  std::vector<renderer::MapDrawState> pendingScene_;
  std::vector<renderer::MapDrawRecord> sourceMapDraws_;
  bool commitAuthoredScene();
  // Visibilidade NA CÂMERA (falsa também em "Só sombra") e modo de Projetar
  // sombras do Mesh Renderer (0 desligado, 1 ligado, 2 dupla face, 3 só
  // sombra), já zerado quando o objeto não renderiza. Os passes de sombra leem
  // só `authoredShadows_`; o passe de cor, só `authoredVisibility_`.
  std::vector<u8> authoredVisibility_, authoredShadows_;
  // Visibilidade autoral antes da Culling Mask e a camada de cada desenho.
  // `authoredVisibility_` é a da vista principal: base E camada na máscara.
  // `authoredBaseShadows_` é o modo de sombra (0..3) antes da máscara.
  std::vector<u8> authoredBaseVisibility_, authoredBaseShadows_, authoredLayers_;
  bool authoredLayerIn(u32 draw,u32 mask) const {
    return draw>=authoredLayers_.size() || ((mask>>(authoredLayers_[draw]&31u))&1u);
  }
  bool authoredVisibleIn(u32 draw,u32 mask) const {
    if(draw>=authoredBaseVisibility_.size()) return authoredVisibility_.empty() || (draw<authoredVisibility_.size()&&authoredVisibility_[draw]);
    return authoredBaseVisibility_[draw] && authoredLayerIn(draw,mask);
  }
  u8 authoredShadowsIn(u32 draw,u32 mask) const {
    if(draw>=authoredBaseShadows_.size()) return draw<authoredShadows_.size()?authoredShadows_[draw]:u8{1};
    return authoredLayerIn(draw,mask)?authoredBaseShadows_[draw]:u8{0};
  }
  // Reaplica a máscara à vista principal; verdadeiro quando algo mudou.
  bool applySceneCullingMask() {
    bool changed=false;
    for(u32 i=0;i<authoredBaseVisibility_.size() && i<authoredVisibility_.size();++i) {
      const u8 next=authoredVisibleIn(i,sceneCullingMask_)?1:0;
      changed|=authoredVisibility_[i]!=next;authoredVisibility_[i]=next;
      if(i<authoredShadows_.size()) {
        const u8 shadow=authoredShadowsIn(i,sceneCullingMask_);
        changed|=authoredShadows_[i]!=shadow;authoredShadows_[i]=shadow;
      }
    }
    appliedCullingMask_=sceneCullingMask_;
    return changed;
  }
  // Camada do objeto, Rendering Layer Mask e Receive Shadows de cada desenho.
  std::vector<renderer::DrawLightingKey> authoredLighting_;
  std::vector<renderer::MaterialOverride> authoredMaterials_;
  struct AuthoredDrawIdentity {
    u64 objectId;
    u32 sourceDrawIndex;
    std::shared_ptr<const renderer::WaterRoute> route;
  };
  std::vector<AuthoredDrawIdentity> authoredDrawIdentities_;
  // R4: tabela de transformações de UV por binding (set=1, binding=16). Só os
  // desenhos que transformam ocupam entrada; o índice+1 vai em materialFlags.z.
  static constexpr u32 MaterialUvTransformCapacity = 1024;
  rhi::VulkanBuffer materialUvTransformBuffer_{};
  std::vector<u32> authoredUvTransformEntries_;
  bool uvTransformOverflowReported_ = false;
  bool lightmapUnsupportedReported_ = false;
  bool writeAuthoredUvTransforms();
  u32 transparentBlendOutput(u32 drawIndex) const;
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
  bool createEditorGridPipeline();
  bool createRuntimeHudPipeline();
  bool createUiRenderPass();
  void recordRuntimeHud(u32 imageIndex, const renderer::RuntimeHudState &hud);
  // Carrega os atlas do APK e monta a pipeline. Falhar aqui NÃO derruba o
  // renderer: uma cena sem interface ainda é uma cena, e o log diz o motivo.
  void createUiRenderer(AAssetManager *assets);
  void recordUiOverlay(u32 imageIndex);
  bool prepareUiOverlay(u32 imageIndex);
  void recordUiContents();
  bool createPostResources();
  void destroyPostResources();
  bool createMotionResources();
  void destroyMotionResources();
  void recordMotionPass(const platform::FreeCameraState &camera,float timeSeconds);
  // --- Skin por compute (G6-B, instanced_skinning.inl) ----------------------
  // A saída vive num buffer próprio: por desenho publicado com skin, a pose
  // atual (n vértices) e, logo depois, a posição anterior (n vértices). Os
  // desenhos com skin trocam o buffer de vértice e o deslocamento; o resto do
  // pacote não muda de lugar.
  struct SkinnedDraw {
    u32 drawIndex = 0, sourceVertex = 0, vertexCount = 0, influenceVertex = 0;
    u32 targetVertex = 0, joints = 0, paletteBase = 0, influences = 4;
    u32 morphTargets = 0, morphBase = 0, weightBase = 0;
    // Identidade do desenho entre republicações: a pose anterior segue por ela.
    u64 objectId = 0;
    u32 sourceDrawIndex = 0;
    bool motion = true, hasPrevious = false;
    std::vector<float> previous, previousWeights;
  };
  struct PendingSkin {
    std::shared_ptr<const std::vector<float>> palette, weights;
    u8 influences = 4;
    bool motion = true;
  };
  struct DrawGeometry { VkBuffer buffer; i32 vertexOffset; };
  bool createSkinningPipeline();
  void destroySkinningResources();
  bool uploadSkinningLibrary(std::span<const u8> influences, std::span<const u32> drawJoints,
                             usize extraVertices, std::span<const float> morphDeltas,
                             std::span<const u32> drawMorphOffsets, std::span<const u32> drawMorphTargets);
  bool layoutSkinnedDraws();
  void queueSkinPalettes(std::span<const renderer::MapDrawState> draws);
  bool recordSkinning();
  DrawGeometry drawGeometry(u32 drawIndex) const;
  std::vector<u32> sourceSkinJoints_, sourceSkinVertices_, sourceMorphTargets_, sourceMorphOffsets_;
  u32 skinInfluenceVertexBase_ = 0;
  rhi::VulkanBuffer skinInfluences_{}, skinVertices_{}, skinPalettes_{}, morphDeltas_{}, morphWeights_{};
  std::vector<SkinnedDraw> skinnedDraws_;
  std::vector<i32> drawSkinSlot_;
  std::vector<PendingSkin> pendingSkins_;
  bool pendingSkinsValid_ = false;
  VkDescriptorSetLayout skinningSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool skinningPool_ = VK_NULL_HANDLE;
  VkDescriptorSet skinningSet_ = VK_NULL_HANDLE;
  VkPipelineLayout skinningLayout_ = VK_NULL_HANDLE;
  VkPipeline skinningPipeline_ = VK_NULL_HANDLE;
  VkPipeline motionSkinnedPipeline_ = VK_NULL_HANDLE;
  bool skinningDescriptorsDirty_ = true;
  bool skinningUnavailableReported_ = false;
  u32 skinnedDrawsThisFrame_ = 0;
  // Máscaras R8 de reatividade e composição, anexos 2 e 3 do passe principal.
  bool createTemporalInputResources();
  void destroyTemporalInputResources();
  u32 sceneColorAttachmentCount() const { return temporalMasksActive_ ? 3u : 1u; }
  // Arm ASR / AMD FSR 2: composição HDR na resolução interna, exposição 1x1 e
  // o dispatch da biblioteca; a exibição reaproveita o pós na resolução final.
  bool createTemporalUpscalerResources();
  void destroyTemporalUpscalerResources();
  bool recordTemporalUpscale(const platform::FreeCameraState &camera, float timeSeconds);
  bool createFogResolveResources();
  void destroyFogResolveResources();
  bool recordFogResolve(const platform::FreeCameraState &camera);
  // Auxiliares dos passes do pós em modo (névoa, bloom, AO em 1/2).
  bool createPostTargetRenderPass(VkFormat format, bool load, VkRenderPass &out);
  bool createPostShaderPipeline(VkRenderPass renderPass, bool additive, VkPipeline &out);
  bool allocatePostSet(VkImageView input, VkImageView bloom, VkImageView ambientOcclusion, VkDescriptorSet &out);
  void fillSceneAuxPush(const platform::FreeCameraState &camera, PostPushConstants &push) const;
  bool createPostChainResources();
  bool createPostChainSets();
  void destroyPostChainResources();
  void preparePostChainTargets();
  void recordPostChainPass(VkRenderPass pass, VkFramebuffer framebuffer, VkPipeline pipeline, VkDescriptorSet set,
                           u32 width, u32 height, const PostPushConstants &push);
  bool recordHalfAmbientOcclusion(const platform::FreeCameraState &camera);
  bool recordBloomChain(const platform::FreeCameraState &camera, float threshold);
  VkImageView postBloomView() const;
  VkImageView postAmbientOcclusionView() const;
  // G2: LUT 32³ de gradação (instanced_color_grading.inl). Vaga 0 = vista
  // principal, 1 = prévia da Camera, que pode estar em outro volume.
  bool createColorGradingResources();
  void destroyColorGradingResources();
  bool prepareColorGrading(u32 slot, const renderer::SceneEnvironment &look, bool authoredPost);
  VkImageView colorGradingView(u32 slot) const;
  struct PostBloomState { bool active = false; float threshold = 1.0f; float intensity = 0.0f; };
  PostBloomState postBloomState() const;
  // `upscaledInput`: a cor de entrada é a saída do Arm ASR/FSR 2 na resolução
  // final, já composta (névoa e AO); o pós só faz a parte de exibição.
  bool recordPostProcess(u32 imageIndex, const platform::FreeCameraState &camera,
                         bool upscaledInput = false);
  bool createAutoExposureResources();
  void destroyAutoExposureResources();
  void bindAutoExposureSource(u32 view, VkImageView source);
  bool recordAutoExposure(u32 view, const renderer::SceneEnvironment &look,
                          const platform::FreeCameraState &camera, float timeSeconds,
                          const ui::UiRect &viewport, u32 activeWidth, u32 activeHeight,
                          u32 allocatedWidth, u32 allocatedHeight,
                          u64 sceneEpoch=0, u32 cameraEntity=0);
  bool createFsrResources();
  void destroyFsrResources();
  void recordFsr(u32 imageIndex, const platform::FreeCameraState &camera);
  bool recordTemporalHistoryCopy();
  bool createShadowResources();
  void destroyShadowResources();
  void recordShadowPass(const platform::FreeCameraState &camera);
  // Atlas de sombra das luzes locais (G6-B). Sempre começa e termina o passe,
  // mesmo sem tile: a imagem precisa sair limpa e no layout de leitura para o
  // descritor continuar válido.
  void recordLocalShadowPass();
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
  VkRenderPass previewRenderPass_=VK_NULL_HANDLE;
  VkFramebuffer previewFramebuffer_=VK_NULL_HANDLE;
  VkRenderPass previewPostRenderPass_=VK_NULL_HANDLE;
  VkFramebuffer previewPostFramebuffer_=VK_NULL_HANDLE;
  rhi::VulkanImage previewSceneColor_,previewColor_,previewDepth_;
  // O passe da prévia é compatível com o principal: com máscaras temporais,
  // o framebuffer precisa dos mesmos anexos (a prévia não os consome).
  rhi::VulkanImage previewReactive_,previewComposition_;
  rhi::VulkanBuffer previewUniform_;
  VkDescriptorPool previewPool_=VK_NULL_HANDLE;
  VkDescriptorSet previewSet_=VK_NULL_HANDLE;
  MaterialStandalonePreviewResources standalonePreview_;
  VkDescriptorPool standalonePreviewPool_=VK_NULL_HANDLE;
  VkDescriptorSet standalonePreviewSet_=VK_NULL_HANDLE;
  VkDescriptorPool previewPostPool_=VK_NULL_HANDLE;
  VkDescriptorSet previewPostSet_=VK_NULL_HANDLE;
  renderer::RenderViewSnapshot pendingPreview_{},submittedPreview_{},completedPreview_{};
  bool previewCloseRequested_=false,previewCompletionSuccess_=false;
  std::string previewDiagnostic_;
  std::string autoExposureDiagnostic_;
  u64 autoExposureSceneEpoch_=0;
  u32 autoExposureCameraEntity_=0;
  bool autoExposurePaused_=false;
  bool prepareCameraPreview();
  bool prepareCameraPreviewEnvironment();
  bool setCameraPreviewEnvironmentMap(renderer::SharedEnvironmentMap);
  void releaseCameraPreviewEnvironment();
  void recordCameraPreview(float timeSeconds);
  void destroyCameraPreview();
  renderer::SceneEnvironment previewSceneEnvironment_{};
  renderer::SharedEnvironmentMap previewEnvironmentMap_{};
  std::array<rhi::VulkanImage,3> previewEnvironmentImages_{};
  std::array<rhi::VulkanSampler,3> previewEnvironmentSamplers_{};
  renderer::PhysicalAtmosphereGroundIrradianceCache previewAtmosphereGroundCache_{};
  bool previewEnvironmentReady_=false;
  bool previewEnvironmentUsesPrimary_=true;
  // --- PK-6: sondas --------------------------------------------------------
  struct ProbeLayer {
    bool used=false,ready=false,awakeDone=false,hdr=true;
    u64 key=0,contentKey=0;
    u32 renderRequests=0,baseMip=0;
    renderer::SceneReflectionProbe probe{};
  };
  struct ProbeJob {
    bool active=false,bake=false,mipsGenerated=false;
    ProbeCaptureRequest request{};
    i32 layer=-1;
    u32 size=0,facesDone=0,nextMip=0,baseMip=0,renderRequests=0;
    u64 contentKey=0;
    renderer::ReflectionProbeSlicing slicing=renderer::ReflectionProbeSlicing::NoTimeSlicing;
  };
  struct ProbeDrawCache {
    float center[3]{};
    i32 tetrahedron=-1;
    u64 lightRevision=0,layerRevision=0;
    u8 lightProbes=0,reflectionProbes=0;
    bool valid=false;
  };
  bool createProbeResources();
  void destroyProbeResources();
  void destroyProbeCaptureTarget();
  bool ensureProbeDrawCapacity(u32 draws);
  bool ensureProbeCaptureTarget(u32 size);
  bool ensureProbeReadback(u64 bytes);
  bool writeProbeFaceSets();
  void collectProbeReadback();
  void updateProbeResidency(const float camera[3]);
  void scheduleProbeWork();
  bool prepareProbes(const platform::FreeCameraState &camera);
  void recordProbes(float timeSeconds);
  void recordProbeFace(u32 face,float timeSeconds);
  rhi::VulkanImage probeAtlas_,probeScratch_,probeCapture_,probeCaptureDepth_,probeCaptureMaskA_,probeCaptureMaskB_;
  // V-1, "Environment Reflections › Source: Skybox" da Unity: o céu da cena
  // capturado só com o céu (sem objetos), filtrado em GGX num octaedro próprio
  // e trocado pelo mapa especular empacotado no binding 3. Recapturado quando
  // o sol ou o céu mudam, como o reflexo do céu do HDRP (On Changed).
  rhi::VulkanImage skyReflection_;
  bool skyReflectionWanted_=false,skyReflectionReady_=false,skyReflectionKeyValid_=false;
  std::array<float,renderer::SkyReflectionKeySize> skyReflectionKey_{};
  VkImageView environmentSpecularBound_=VK_NULL_HANDLE;
  bool skyReflectionActive() const {return skyReflectionWanted_&&skyReflectionReady_;}
  bool previewSkyReflection() const {
    return skyReflectionActive()&&!previewEnvironmentMap_&&previewEnvironmentUsesPrimary_&&
           previewSceneEnvironment_.active&&previewSceneEnvironment_.sky!=renderer::SkyModel::Hdri;
  }
  bool bindEnvironmentSpecular();
  rhi::VulkanSampler probeAtlasSampler_,probeCaptureSampler_;
  rhi::VulkanBuffer probeBlock_,probeDisabledBlock_,probeReadback_,probeUpload_;
  std::array<rhi::VulkanBuffer,6> probeFaceUniforms_{};
  std::array<VkImageView,6> probeFaceViews_{};
  std::array<VkFramebuffer,6> probeFramebuffers_{};
  std::array<VkDescriptorSet,6> probeFaceSets_{};
  std::array<VkImageView,renderer::ReflectionProbeAtlasMips> probeScratchViews_{};
  std::array<VkDescriptorSet,renderer::ReflectionProbeAtlasMips> probeFilterSets_{};
  VkDescriptorPool probeFacePool_=VK_NULL_HANDLE,probeFilterPool_=VK_NULL_HANDLE;
  VkDescriptorSetLayout probeFilterSetLayout_=VK_NULL_HANDLE;
  VkPipelineLayout probeFilterLayout_=VK_NULL_HANDLE;
  VkPipeline probeFilterPipeline_=VK_NULL_HANDLE;
  u32 probeBlockCapacity_=0,probeCaptureSize_=0,probeCaptureMips_=1;
  bool probeBlitMips_=false,probeAtlasInitialized_=false;
  std::array<ProbeLayer,renderer::MaximumFrameReflectionProbes> probeLayers_{};
  ProbeJob probeJob_{};
  u32 probeFrameFaces_=0;
  bool probeFrameMipGen_=false,probeFrameFinish_=false;
  std::vector<u32> probeFrameMips_;
  std::deque<ProbeCaptureRequest> probeCaptureQueue_;
  std::deque<ProbeCaptureResult> probeResults_;
  bool probeReadbackPending_=false;
  ProbeCaptureResult probeReadbackResult_{};
  std::vector<std::pair<u32,std::shared_ptr<const renderer::ReflectionProbeTexture>>> probePendingUploads_;
  std::vector<renderer::SceneReflectionProbe> sceneProbes_;
  bool sceneProbesPlaying_=false;
  std::vector<std::shared_ptr<const renderer::ReflectionProbeTexture>> probeTextures_;
  std::shared_ptr<const renderer::LightProbeSet> lightProbeSet_;
  u64 lightProbeRevision_=1,probeLayerRevision_=1;
  std::vector<ProbeDrawCache> probeDrawCache_;
  ProbeFrameStats probeStats_{};
  // Item 2.1.4 do plano: em vez de um descriptor set por-objeto com 1 binding,
  // o pipeline instanciado consome o registro bindless (array único indexado
  // por materialIndex no shader) — ver rhi/bindless_registry.h. Esta PoC tem
  // um único consumidor hoje, então o registro é dono deste objeto, não
  // compartilhado entre renderers; se um segundo pipeline bindless aparecer,
  // promover a dono no VulkanDevice passa a valer a pena.
  rhi::BindlessTextureRegistry bindlessRegistry_{};
  bool useBindless_ = false;
  bool astc4x4_ = false;
  bool materialCulling_ = false;
  std::vector<u8> pendingUiPreview_;
  std::vector<u8> uiImmediateAtlas_;
  u32 uiImmediateWidth_=0,uiImmediateHeight_=0;
  bool uiImmediatePending_=false;
  u32 pendingUiPreviewWidth_ = 0, pendingUiPreviewHeight_ = 0;
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
  // PK-9: Premultiply, Additive e Multiply (índice renderer::MaterialBlendOutput*;
  // o 0, reto, é transparentPipeline_ e suas variantes).
  VkPipeline transparentBlendPipelines_[renderer::MaterialBlendOutputCount]{};
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
  VkPipeline opaqueNoClipPipeline_ = VK_NULL_HANDLE;
  VkPipeline opaqueNoClipDistantPipeline_ = VK_NULL_HANDLE;
  VkPipeline opaqueNoClipMaterialPipelines_[renderer::MaterialFeatureVariantCount]{};
  bool opaqueNoClipEnabled_ = true;
  bool fullDetailSamplingEnabled_ = false;
  bool opaqueNoClipLastFrame_ = false;
  VkPipeline pointLightingPipeline_ = VK_NULL_HANDLE;
  VkPipeline pointLightingDistantPipeline_ = VK_NULL_HANDLE;
  VkPipeline pointLightingMaterialPipelines_[renderer::MaterialFeatureVariantCount]{};
  bool pointLightingSpecializationEnabled_ = true;
  bool punctualLightMaskEnabled_ = false;
  bool pointLightingSpecializationLastFrame_ = false;
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
  VkPipelineLayout editorGridPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline editorGridPipeline_ = VK_NULL_HANDLE;
  renderer::GridPlan editorGrid_{};
  VkPipelineLayout runtimeHudPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline runtimeHudPipeline_ = VK_NULL_HANDLE;
  static constexpr u32 kMaxFramebuffers = 8;
  VkRenderPass postRenderPass_ = VK_NULL_HANDLE;
  VkRenderPass editorPostRenderPass_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout postSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool postDescriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet postDescriptorSet_ = VK_NULL_HANDLE;
  VkPipelineLayout postPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline postPipeline_ = VK_NULL_HANDLE;
  struct AutoExposureView {
    rhi::VulkanBuffer histogram,state;
    VkDescriptorSet set=VK_NULL_HANDLE;
    bool initialized=false,enabled=false,cameraValid=false,paused=false;
    float previousTime=0;
    platform::FreeCameraState previousCamera{};
    u32 activeWidth=0,activeHeight=0,allocatedWidth=0,allocatedHeight=0;
    u64 sceneEpoch=0;
    u32 cameraEntity=0;
    float lastLog=-1.0e9f;
  };
  std::array<AutoExposureView,2> autoExposureViews_{};
  VkDescriptorSetLayout autoExposureSetLayout_=VK_NULL_HANDLE;
  VkDescriptorPool autoExposurePool_=VK_NULL_HANDLE;
  VkPipelineLayout autoExposurePipelineLayout_=VK_NULL_HANDLE;
  VkPipeline autoExposurePipelines_[2]{};
  rhi::VulkanImage postSceneColor_{};
  // Entradas temporais unificadas (temporal_projection.glsl).
  bool temporalInputsActive_ = false;
  bool temporalMasksActive_ = false;
  rhi::VulkanImage motionImage_{}; // RG16F: atual -> anterior, UV da extensão, sem jitter
  rhi::VulkanImage reactiveMaskImage_{}, compositionMaskImage_{};
  VkRenderPass motionRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer motionFramebuffer_ = VK_NULL_HANDLE;
  VkPipeline motionPipeline_ = VK_NULL_HANDLE;
  VkPipeline motionCameraPipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout motionCameraLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout motionCameraSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool motionCameraPool_ = VK_NULL_HANDLE;
  VkDescriptorSet motionCameraSet_ = VK_NULL_HANDLE;
  rhi::VulkanSampler motionDepthSampler_{};
  bool motionVectorsActive_ = false;
  // Ampliador temporal de terceiros (rhi/temporal_upscaler.h).
  rhi::TemporalUpscaler temporalUpscaler_{};
  renderer::UpscalingFilter temporalUpscalerRequested_ = renderer::UpscalingFilter::Bilinear;
  renderer::UpscalingFilter executedUpscaler_ = renderer::UpscalingFilter::Bilinear;
  renderer::TemporalUpscalerAvailability executedUpscalerStatus_ =
      renderer::TemporalUpscalerAvailability::Available;
  rhi::VulkanImage compositeColor_{}, upscaledColor_{}, exposureImage_{};
  VkImageLayout upscaledColorLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  VkRenderPass compositeRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer compositeFramebuffer_ = VK_NULL_HANDLE;
  VkPipeline compositePipeline_ = VK_NULL_HANDLE;
  VkDescriptorSet upscaledDescriptorSet_ = VK_NULL_HANDLE;
  // Névoa resolvida por texel antes do pós (instanced_post_fog.inl).
  rhi::VulkanImage fogResolveColor_{};
  VkRenderPass fogResolveRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer fogResolveFramebuffer_ = VK_NULL_HANDLE;
  VkPipeline fogResolvePipeline_ = VK_NULL_HANDLE;
  VkDescriptorSet fogResolveDescriptorSet_ = VK_NULL_HANDLE;
  bool fogResolveEnabled_ = true;
  bool fogActiveThisFrame_ = false;
  bool fogResolvedThisFrame_ = false;
  bool fogResolveReported_ = false;
  // Bloom em cadeia de mips e AO em 1/2 (instanced_post_chain.inl); 10 é a LUT 3D.
  static constexpr u32 kPostBindingCount = 11;
  struct ColorGradingSlot {
    rhi::VulkanImage image{};
    rhi::VulkanBuffer staging{};
    renderer::ColorGradingLook look{};
    bool uploaded = false;
  };
  std::array<ColorGradingSlot, 2> colorGradingSlots_{};
  bool colorGradingLayoutsReady_ = false;
  bool colorGradingForced_ = false;
  bool colorGradingThisFrame_ = false;
  bool colorGradingReported_ = false;
  u32 colorGradingRebuilds_ = 0;
  static constexpr u32 kBloomLevelCount = 5;
  std::array<rhi::VulkanImage, kBloomLevelCount> bloomLevels_{};
  std::array<VkExtent2D, kBloomLevelCount> bloomLevelExtents_{};
  std::array<VkFramebuffer, kBloomLevelCount> bloomFramebuffers_{};
  std::array<VkDescriptorSet, kBloomLevelCount> bloomLevelSets_{};
  VkDescriptorSet bloomSceneSet_ = VK_NULL_HANDLE;
  VkDescriptorSet bloomFogSet_ = VK_NULL_HANDLE;
  VkDescriptorSet postPrepassDescriptorSet_ = VK_NULL_HANDLE;
  VkRenderPass bloomDownRenderPass_ = VK_NULL_HANDLE;
  VkRenderPass bloomUpRenderPass_ = VK_NULL_HANDLE;
  VkPipeline bloomDownPipeline_ = VK_NULL_HANDLE;
  VkPipeline bloomUpPipeline_ = VK_NULL_HANDLE;
  rhi::VulkanImage halfAmbientOcclusion_{};
  VkRenderPass halfAmbientOcclusionRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer halfAmbientOcclusionFramebuffer_ = VK_NULL_HANDLE;
  VkPipeline halfAmbientOcclusionPipeline_ = VK_NULL_HANDLE;
  bool bloomChainEnabled_ = false;
  bool halfAmbientOcclusionEnabled_ = false;
  bool bloomChainThisFrame_ = false;
  bool halfAmbientOcclusionThisFrame_ = false;
  bool aoActiveThisFrame_ = false;
  bool postChainLayoutsReady_ = false;
  bool postChainReported_ = false;
  VkDescriptorSetLayout exposureExportSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool exposureExportPool_ = VK_NULL_HANDLE;
  VkDescriptorSet exposureExportSet_ = VK_NULL_HANDLE;
  VkPipelineLayout exposureExportLayout_ = VK_NULL_HANDLE;
  VkPipeline exposureExportPipeline_ = VK_NULL_HANDLE;
  bool exposureImageInitialized_ = false;
  float temporalUpscalerJitterPixels_[2]{};
  float temporalPreviousTime_ = -1.0f;
  std::string temporalUpscalerDiagnostic_;
  rhi::VulkanImage postHistory_{};
  rhi::VulkanImage postResolved_{};
  rhi::VulkanImage fsrSource_{}, fsrUpscaled_{};
  VkRenderPass fsrRenderPasses_[2]{};
  VkPipeline fsrPipelines_[2]{};
  VkPipelineLayout fsrPipelineLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout fsrSetLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool fsrPool_ = VK_NULL_HANDLE;
  VkDescriptorSet fsrSets_[2]{};
  VkFramebuffer fsrEasuFramebuffer_ = VK_NULL_HANDLE;
  VkFramebuffer fsrPresentFramebuffers_[kMaxFramebuffers]{};
  bool fsrActive_ = false;
  rhi::VulkanImage previewResolved_{};
  rhi::VulkanSampler postSampler_{};
  rhi::VulkanSampler postDepthSampler_{};
  VkFormat sceneColorFormat_ = VK_FORMAT_UNDEFINED;
  bool hdrSceneColor_ = false;
  VkFramebuffer postFramebuffers_[kMaxFramebuffers]{};
  bool temporalAaActive_ = false;
  TemporalDebugView temporalDebugView_ = TemporalDebugView::Final;
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
  // "Projetar sombras: Ligado" num material de face única: o mapa de sombra
  // descarta as faces de trás, como o passe ShadowCaster da Unity com o Cull
  // do material. Dupla face e materiais de face dupla usam as de cima.
  VkPipeline shadowOpaqueCullPipeline_ = VK_NULL_HANDLE;
  VkPipeline shadowMaskedCullPipeline_ = VK_NULL_HANDLE;
  void splitShadowDraws(const std::vector<u32> &solid,const std::vector<u32> &coverage);
  template<class Draw> void recordShadowCasters(const Draw &draw);
  std::vector<u32> shadowSolidNone_,shadowSolidCull_,shadowCoverageNone_,shadowCoverageCull_;
  // Sol principal deste quadro (Luz v4): escala do desvio de profundidade em
  // relação à política e máscaras autorais. O cache de cascatas estáticas é
  // invalidado quando a escala muda, porque o mapa guardado usou a anterior.
  float sunShadowBiasScale_ = 1.0f;
  renderer::PunctualLightFilter sunFilter_{};
  // Cópia CPU das luzes adicionais aceitas no quadro, nos dois bancos do bloco,
  // e as máscaras autorais de cada vaga: a máscara por desenho lê daqui.
  renderer::PunctualLight frameLights_[renderer::MaximumPunctualLights]{};
  renderer::PunctualLightFilter frameLightFilters_[renderer::MaximumPunctualLights]{};
  u32 frameLightCount_ = 0;
  rhi::VulkanImage shadowAtlas_{};
  // Atlas local: imagem própria em vez de dividir a das cascatas, para o cache
  // estático do sol continuar valendo quando só uma lâmpada muda. O passe é
  // compatível com o do sol, então as mesmas pipelines de caster servem.
  rhi::VulkanImage localShadowAtlas_{};
  VkRenderPass localShadowRenderPass_ = VK_NULL_HANDLE;
  VkFramebuffer localShadowFramebuffer_ = VK_NULL_HANDLE;
  renderer::ShadowAtlasTile localShadowTiles_[renderer::MaximumLocalShadowTiles]{};
  // Esfera de alcance da luz de cada tile (xyz centro, w raio) e o desvio de
  // profundidade autoral, para o passe recortar casters e ajustar a acne.
  float localShadowSpheres_[renderer::MaximumLocalShadowTiles][4]{};
  float localShadowBias_[renderer::MaximumLocalShadowTiles]{};
  u32 localShadowTileCount_ = 0;
  renderer::ShadowAtlasReport localShadowReport_{};
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
    // Attachment extents belong to the resource epoch. Thermal pressure only
    // changes the rectangle drawn into them; post sampling must still divide
    // by the allocated image size, otherwise it reads the uncleared margins.
    return std::max(1u, static_cast<u32>(static_cast<float>(swapchain_->width()) *
                                        resourceRenderingPolicy_.resolutionScale));
  }
  u32 renderTargetHeight() const {
    return std::max(1u, static_cast<u32>(static_cast<float>(swapchain_->height()) *
                                        resourceRenderingPolicy_.resolutionScale));
  }
  u32 renderWidth() const {
    return dynamicResolution_.scale() >= resourceRenderingPolicy_.resolutionScale - 1.0e-4f
               ? renderTargetWidth()
               : std::min(renderTargetWidth(), renderer::scaledRenderExtent(
                                                   swapchain_->width(), dynamicResolution_.scale()));
  }
  u32 renderHeight() const {
    return dynamicResolution_.scale() >= resourceRenderingPolicy_.resolutionScale - 1.0e-4f
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
  // G4b: mapa procedural do Cloud Layer (binding 18 do conjunto de ambiente).
  rhi::VulkanImage cloudMapTexture_{};
  rhi::VulkanSampler cloudMapSampler_{};
  // Relógio do vento das nuvens: o mesmo do quadro, também para a prévia.
  double cloudTimeSeconds_ = 0.0;
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
  // Slots bindless das texturas importadas; devolvidos a cada republicação.
  std::vector<u32> authoringTextureSlots_;
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
  u32 profileSourceTriangleCount_ = 0;
  bool postUiFusionEnabled_ = true;
  bool postUiFusedLastFrame_ = false;
  // Timestamps de GPU acompanham o perfil. Uma tentativa de desacoplar os dois
  // para alimentar cadência adaptativa foi medida e retirada — ver
  // PROFILING-ANDROID.md, "Cadência adaptativa rejeitada".
  bool gpuTimingEnabled() const {
    return frameProfilingEnabled_ || adpfGpuTimingEnabled_ || editorGpuTimingEnabled_ ||
           renderingPolicy_.dynamicResolution.enabled;
  }
  bool editorGpuTimingEnabled_ = false;
  bool adpfGpuTimingEnabled_ = false;
  bool coveragePrepassEnabled_ = true;
  bool runtimeHudEnabled_ = false;
  bool playPostUiFusionEnabled_ = false;

  // Interface do editor. Os bytes dos assets ficam vivos porque UiFont e
  // UiIconAtlas apontam para dentro deles (ver a nota de tempo de vida em
  // ui_font.h); só os pixels já foram para a GPU.
  std::vector<u8> uiFontBytes_;
  std::vector<u8> uiIconBytes_;
  ui::UiFont uiFont_{};
  ui::UiIconAtlas uiIcons_{};
  rhi::VulkanUiRenderer uiRenderer_{};
  std::vector<u8> uiGuiAtlas_;
  u64 uiGuiRevision_=~u64{0};u32 uiGuiSize_=0;bool uiGuiPending_=false;
  VkRenderPass uiRenderPass_=VK_NULL_HANDLE;
  std::vector<VkFramebuffer> uiFramebuffers_;
  std::vector<ui::UiInstance> uiInstances_;
  float uiSurfaceWidth_ = 0.0f;
  float uiSurfaceHeight_ = 0.0f;
  renderer::PerspectiveVisibilitySettings visibilitySettings_{};
  renderer::VisibilityTelemetry visibilityTelemetry_{};
  u64 renderedFrameCount_ = 0;
  renderer::GpuCostIsolation gpuCostIsolation_ = renderer::GpuCostIsolation::Full;
  u32 postIsolation_ = 0;
  profiler::RenderPhaseTimings lastFrameTimings_{};
  // Limites para os quais os recursos foram realmente criados. A política
  // ativa abaixo pode apenas reduzir estes limites durante a época atual.
  renderer::ResolvedRenderingPolicy resourceRenderingPolicy_{};
  renderer::ResolvedRenderingPolicy renderingPolicy_{};
  renderer::WaterProfile waterProfile_ = renderer::defaultOceanWaterProfile();
  renderer::WaterShadingSettings waterShading_{};
  std::array<renderer::MapDrawUpdate, 128> pendingMapPoses_{};
  u32 pendingMapPoseCount_ = 0;
  // Um vetor por desenho que se moveu no quadro, sem teto fixo: exceder uma
  // capacidade pequena descartava o histórico da cena inteira.
  std::vector<u32> motionDrawIndices_;
  u32 motionDrawCount_ = 0;
  bool temporalStaticCoverage_ = false;
  u32 temporalStaticFrameCount_ = 0;
  // Dither/coverage updates still invalidate HZB, but only changed model
  // transforms invalidate the camera-only temporal history.
  bool pendingMapTransformChanged_ = false;
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
    u8 castShadows = 1;
    renderer::DrawLightingKey lighting{};
    u8 layer = 0;  // camada do objeto (Culling Mask), cópia de lighting.objectLayer
  };
  std::vector<AuthoredInstanceState> pendingAuthoredState_;
  bool pendingAuthoredStateValid_ = false;
  bool rebuildDrawOrders();
  bool rebuildSpatialGeometry();
  struct SpatialSourceRange {u32 first=0,count=0;};
  std::vector<SpatialSourceRange> spatialSourceRanges_;
  std::vector<renderer::MapDrawRecord> spatialGeometryChunks_;
  rhi::VulkanBuffer spatialIndexBuffer_,spatialIndirectBuffer_;
  std::vector<VkDrawIndexedIndirectCommand> spatialCommands_;
  // Candidate path stays opt-in until the same-device visual/performance A/B.
  bool spatialGeometryEnabled_=false;
  // Reuses the existing offscreen color/depth, never the present image or UI.
  // No extra render target; exact camera/uniform identity, fail-open for dynamics.
  bool editorSceneReuseEnabled_=true,editorSceneCacheValid_=false,editorSceneReusedLastFrame_=false;
  platform::FreeCameraState editorSceneCachedCamera_{};
  std::vector<u8> editorSceneCachedUniform_;
  u32 editorSceneCachedWidth_=0,editorSceneCachedHeight_=0,editorSceneCachedOptions_=0;
  float editorSceneCachedTime_=0;
  u32 spatialIndirectCapacity_=0,spatialMaxDrawCount_=0;
  std::vector<renderer::SceneLight> sceneLights_;
  renderer::SceneEnvironment sceneEnvironment_{};
  std::vector<std::pair<resources::AssetGuid,renderer::SharedEnvironmentMap>> environmentMaps_;
  renderer::SharedEnvironmentMap activeEnvironmentMap_;
  std::string environmentMapDiagnostic_;
  renderer::PhysicalAtmosphereGroundIrradianceCache atmosphereGroundCache_;
  std::vector<renderer::SceneEnvironmentVolume> sceneEnvironmentVolumes_;
  u32 sceneEnvironmentLayerMask_=~0u;
  renderer::SceneEnvironmentBlendReport environmentBlendReport_{};
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
  // S2: streaming de mipmaps das texturas de autoria. Roda logo depois da fence
  // do quadro anterior (um único quadro em voo), único ponto em que trocar a
  // imagem de um slot bindless não alcança um comando ainda na GPU.
  void updateTextureStreaming(const platform::FreeCameraState &camera);
  // S3: nível de detalhe de cada desenho autoral, escolhido uma vez por quadro
  // pela mesma regra do pacote (`selectLodLevel`, orçamento em pixels da tela
  // final e histerese do painel Qualidade). `drawIndexRange` é a faixa que as
  // passagens emitem — cena, indireta e sombra usam a mesma escolha.
  void updateAuthoredMeshLods(const platform::FreeCameraState &camera);
  struct IndexRange {u32 firstIndex=0,indexCount=0;};
  IndexRange drawIndexRange(u32 drawIndex) const;
  std::vector<u8> authoredLodLevel_;
  std::vector<renderer::LodHysteresisState> authoredLodState_;
  MeshLodStats meshLodStats_{};
  renderer::TextureStreamingStats textureStreamingStats_{};
  renderer::TextureStreamingPlan streamingPlan_{};
  std::vector<renderer::TextureStreamingTexture> streamingTextures_;
  std::vector<renderer::TextureStreamingUse> streamingUses_;
  std::vector<u32> streamingLoaded_;
  std::vector<renderer::TextureStreamingParameters> streamingParameters_;
  bool textureStreamingDebugView_ = false;
  // Tinta da vista de depuração para a textura de autoria `authoring`, ou nada.
  bool textureStreamingDebugTint(u32 texture, float tint[3]) const;
  // Métrica de UV por desenho da biblioteca (`sourceMapDraws_`), medida uma vez
  // por publicação e só dos desenhos que usam textura de autoria. Negativo = a medir.
  std::vector<float> sourceDrawUvMetric_;
  u32 streamingFrame_ = 0;
  bool streamingFailureLogged_ = false;
  // Bloco C: níveis que estão só no derivado em disco são lidos fora do quadro.
  renderer::TextureLevelReader textureLevelReader_;

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
  // Conta-gotas: buffer de 4 bytes visível à CPU, pedido pendente e cópia do
  // quadro anterior (lida depois da espera de fence do acquire, como o HZB).
  void recordPixelSample(u32 imageIndex);
  void readPixelSampleFromPreviousFrame();
  rhi::VulkanBuffer pixelSampleBuffer_{};
  bool pixelSampleRequested_=false,pixelSampleInFlight_=false,pixelSampleReady_=false;
  float pixelSampleUi_[2]{};
  u8 pixelSampleValue_[4]{};
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
  platform::FreeCameraState hzbLastEditorCamera_{};
  bool hzbLastEditorCameraValid_ = false;
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
