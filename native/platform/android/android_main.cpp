#include "editor/editor_import_transaction.h"
#include "resources/gltf_package.h"
#include "resources/import_cache.h"
#include "editor/editor_water_settings_component.h"
#include "editor/editor_scene_camera.h"
#include <cstring>
#include "platform/android/android_paths.h"
#include "platform/android/android_launch_options.h"
#include "platform/android/android_model_picker.h"
#include "platform/android/android_runtime_controls.h"
#include "platform/android/astc_encode_probe.h"
#include "platform/android/water_spectral_probe.h"
#include "platform/android/android_frame_profiler.h"
#include "renderer/rendering_policy.h"
#include "renderer/water_fft.h"
#include "platform/android/android_frame_pacer.h"
#include "platform/android/android_performance.h"
#include "platform/android/android_thermal_monitor.h"
#include "platform/android/android_vulkan_surface.h"
#include "platform/android/android_window.h"
#include "platform/android/dotnet_assets.h"
#include "platform/android/dotnet_host.h"
#include "platform/android/instanced_renderer.h"
#include "platform/android/ocean_validation.h"
#include "editor/editor_water_play.h"
#include "platform/android/lifecycle_trace.h"
#include "platform/app_lifecycle.h"
#include "platform/camera_route.h"
#include "platform/first_person_controller.h"
#include <android/configuration.h>

#include "editor/editor_history.h"
#include "editor/editor_session.h"
#include "platform/android/android_editor_text_input.h"
#include "platform/free_camera_controller.h"
#include "core/frame_policy.h"
#include "renderer/gpu_cost_isolation.h"
#include "physics/character_motor.h"

#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <future>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <time.h>

namespace {

constexpr const char *LogTag = "Aether.Android";
constexpr ae::u64 ValidationFrameMilestone = 1000;
// PoC-A (item 0.2 do plano): 5.000 objetos é o número que o critério de
// sucesso pede ("5.000 objetos renderizados a 60 fps com < 3 ms de CPU").
constexpr ae::u32 PocAInstanceCount = 5000;
constexpr ae::u32 ScenePreviewCapacity = 256;
constexpr ae::u64 SceneValidationIntervalFrames = 180;
constexpr ae::u64 PocAReportIntervalFrames = 300; // ~5s a 60fps — log periódico, não por frame

ae::u64 currentThreadCpuNanoseconds() {
  timespec value{};
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) return 0;
  return static_cast<ae::u64>(value.tv_sec) * 1'000'000'000ULL +
         static_cast<ae::u64>(value.tv_nsec);
}

ae::u64 currentMonotonicNanoseconds() {
  timespec value{};
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
  return static_cast<ae::u64>(value.tv_sec) * 1'000'000'000ULL +
         static_cast<ae::u64>(value.tv_nsec);
}

struct AndroidShell final {
  struct AdpfFrameSample final {
    ae::u64 workStartNs = 0;
    ae::u64 totalNs = 0;
    ae::u64 threadCpuNs = 0;
    bool valid = false;
  };
  android_app *app = nullptr;
  ae::platform::AppLifecycle lifecycle;
  ae::platform::android::AndroidFrameProfiler frameProfiler;
  ae::platform::android::AndroidFramePacer framePacer;
  ae::platform::android::AndroidPerformance performance;
  AdpfFrameSample pendingAdpfFrame{};
  ae::platform::android::AndroidThermalMonitor thermalMonitor;
  ae::platform::android::AndroidVulkanSurface vulkanSurface;
  ae::platform::android::InstancedRenderer instancedRenderer;
  bool instancedRendererReady = false;
  bool forceDescriptorFallback = false;
  // Public Android frame pacing is the production default. Diagnostics can
  // explicitly disable it to retain a reproducible FIFO/Choreographer control.
  bool useSwappy = true;
  bool pocABenchmark = false;
  bool scenePreview = false;
  bool materialPreview = false;
  bool dirtRoadPreview = false;
  bool oceanPreview = false;
  ae::platform::android::OceanValidation oceanValidation;
  ae::editor::EditorWaterPlay authoredWaterPlay;
  std::vector<ae::renderer::MapDrawState> authoredDraws;
  std::vector<ae::renderer::SceneLight> authoredLights;
  // Quantas luzes ficaram de fora no último aviso publicado. Sem isto o mesmo
  // excedente sairia no log a cada quadro e viraria ruído em vez de aviso.
  ae::u32 editorReportedLightOverflow = 0;
  bool forceTextureFallback = false;
  bool lockCamera = false;
  std::future<bool> rendererInitialization;
  std::atomic<bool> cancelRendererInitialization{false};
  bool sceneValidation = false;
  int sceneStep = 0;
  int sceneReportedStep = -1;
  using SceneStepFn = int (*)(int);
  using SceneShutdownFn = void (*)();
  SceneStepFn applySceneStep = nullptr;
  SceneShutdownFn shutdownScene = nullptr;
  ae::u64 presentedFrameCount = 0;
  ae::u64 activationCount = 0;
  ae::u64 lifecycleCommandSequence = 0;
  double activatedAtMs = 0.0;
  bool firstFrameAfterActivationPending = false;
  bool validationFrameMilestoneLogged = false;
  ae::platform::android::DotNetHost dotNetHost;
  using CodeBuildFn = int (*)(const ae::u8 *,int);
  using CodeReportFn = int (*)(ae::u8 *,int);
  using CodeCommitFn = int (*)();
  CodeBuildFn codeBuild=nullptr;
  CodeReportFn codeReport=nullptr;
  CodeCommitFn codeCommit=nullptr;
  ae::scene::ScriptRuntimeApi scriptRuntime{};
  // Declared after the CLR owner: joining the worker precedes host teardown.
  std::future<std::string> codeCompilation;
  CodeBuildFn languageQuery=nullptr;
  CodeReportFn languageReport=nullptr;
  void (*languageCancel)()=nullptr;
  std::future<std::string> languageWork;
  struct PreparedModel {
    std::string root,path,expectedHash,diagnostic,contentHash;
    ae::u64 epoch=0;
    std::vector<ae::u8> bytes;
    ae::resources::GltfImport model;
    bool accepted=false;
    // Entrega 4: manifesto das dependências de um .gltf empacotado (vazio para GLB autocontido).
    std::string manifest;
    ae::u32 dependencies=0,unusedCompanions=0;
  };
  // The cancellation token outlives its worker, including shell teardown.
  std::shared_ptr<std::atomic<bool>> importCancellation;
  std::future<PreparedModel> importWork;
  std::optional<PreparedModel> importPreview;
  std::string importPickerRoot;
  ae::u64 importPickerEpoch=0;
  // R1 — abertura do projeto sem tela preta. As fontes registradas são lidas e
  // interpretadas num worker; o loop continua apresentando quadros do editor.
  // A publicação (uma só, para todas as fontes) e a recuperação da cena salva
  // voltam à thread do editor quando o worker termina.
  struct ProjectReopen {
    std::string root;
    std::vector<ae::editor::EditorSession::ReopenedSource> sources;
    std::vector<std::string> missing;
    std::vector<std::pair<std::string,std::string>> refused;
    ae::usize cacheHits=0;
  };
  std::shared_ptr<std::atomic<bool>> reopenCancellation;
  // Etapa real do worker de abertura: qual fonte, quantas já passaram e o que
  // está acontecendo com ela. É o que a barra de estado mostra -- sem porcentagem.
  struct ReopenProgress {
    std::mutex lock;
    ae::usize done=0,total=0;
    std::string current,stage;
  };
  std::shared_ptr<ReopenProgress> reopenProgress;
  std::future<ProjectReopen> reopenWork;
  bool projectReopening=false;
  double reopenStartedMs=0;

  std::optional<ae::platform::android::EditorLanguageQuery> languageNext,languageActive;
  std::chrono::steady_clock::time_point shellStartTime = std::chrono::steady_clock::now();
  double pocAMaxFillMicroseconds = 0.0;
  ae::platform::FreeCameraController cameraController;
  ae::platform::FirstPersonController firstPersonController;
  ae::platform::FirstPersonTouchControls firstPersonTouches;
  ae::physics::CharacterMotor characterMotor;
  bool firstPersonEnabled = false;

  // Interface do editor. Ela e opt-in por opcao de lancamento porque os runners
  // de validacao comparam capturas de tela: ligar a interface por padrao faria
  // todos eles falharem por uma mudanca que nao e do renderer da cena.
  bool editorUi = false;
  ae::editor::EditorSession editorSession;
  bool editorMapImported = false;
  ae::u64 editorPackageFingerprint = 0;
  std::string editorSavePath;
  char editorProjectName[256]{};
  char editorProjectPath[1024]{};
  bool editorEmpty = false;
  bool independentWorkspace = false;
  ae::u64 editorSavedRevision = ~ae::u64{0};
  float editorLastSaveSeconds = 0.0f;
  bool editorWasPlaying = false;
  ae::u64 editorPublishedRevision = ~ae::u64{0};

  // Escala de pixel fisico para dp, resolvida uma vez na inicializacao. A
  // interface e montada em dp; sem isto um painel de 220 unidades sairia com 220
  // pixels num aparelho de 520 dpi -- um terco do tamanho pretendido.
  float editorScale = 1.0f;
  float editorSceneScale = 1.0f;
  std::chrono::steady_clock::time_point lastGameplayUpdate{};
  std::chrono::steady_clock::time_point lastPresentedAt{};
  std::chrono::steady_clock::time_point lastFpsPublishedAt{};
  float smoothedFps = 0.0f;
  ae::u32 displayedFps = 0;
  bool mapCameraInitialized = false;
  bool hasLaunchCamera = false;
  ae::platform::FreeCameraState launchCamera{};
  ae::FrameBudget frameBudget = ae::makeFrameBudget(60.0f, 60.0f, 60);
  ae::VisibilityBudget visibilityBudget{};
  ae::renderer::GpuCostIsolation gpuCostIsolation = ae::renderer::GpuCostIsolation::Full;
  // Escolha global do projeto e política resolvida (ADR-014). Uma única
  // resolução por época de configuração; nenhuma cena tem perfil próprio.
  ae::renderer::ProjectRenderingSettings renderingSettings{};
  ae::renderer::RenderingCapabilities renderingCapabilities{};
  ae::renderer::ResolvedRenderingPolicy renderingPolicy{};
  ae::renderer::ResolvedRenderingPolicy activeRenderingPolicy{};
  ae::renderer::ThermalPressure appliedThermalPressure = ae::renderer::ThermalPressure::None;
  bool renderingCapabilitiesReady = false;
  bool thermalPolicyApplied = false;
  float maximumDisplayHz = 60.0f;
  int displayRotation = -1;
  bool windowResizePending = false;
  std::chrono::steady_clock::time_point windowResizeAfter{};
  ae::u64 runtimeControlsRevision = ~ae::u64{0};
  ae::u64 waterDocumentRevision = ~ae::u64{0};
  ae::u64 waterLayoutRevision = ~ae::u64{0};
  ae::renderer::WaterSpectrumAuthoringSettings waterSpectrumAuthoring{};
  bool waterSpectrumAuthoringApplied = false;
  float waterInteractionStrength = 0.65f;
  float waterTimeSeconds = 0.0f;
  bool waterTapTracking = false;
  bool waterTapMoved = false;
  int32_t waterTapPointer = -1;
  float waterTapX = 0.0f;
  float waterTapY = 0.0f;

  // Deterministic camera route (benchmark A/B tooling, see camera_route.h).
  // Off by default; a launch option must explicitly opt in.
  ae::platform::CameraRouteMode cameraRouteMode = ae::platform::CameraRouteMode::Off;
  char cameraRoutePath[512]{};
  ae::platform::CameraRouteRecorder cameraRouteRecorder;
  ae::platform::CameraRoutePlayer cameraRoutePlayer;
  ae::u64 cameraRouteFrameOrdinal = 0;
  // Only true once a Replay route has been loaded AND its scene fingerprint
  // matches the map actually running -- never trust a stale/foreign route.
  bool cameraRouteReplayActive = false;
};

void applyThermalRenderingPolicy(AndroidShell &shell, bool force);

bool sameSpectrumAuthoring(const ae::renderer::WaterSpectrumAuthoringSettings &a,
                           const ae::renderer::WaterSpectrumAuthoringSettings &b) {
  const auto sameCross=[](const ae::renderer::WaterSwellSystem &x,
                          const ae::renderer::WaterSwellSystem &y) {
    return x.windSpeed==y.windSpeed && x.directionRadians==y.directionRadians &&
      x.fetch==y.fetch && x.swell==y.swell && x.spread==y.spread && x.weight==y.weight;
  };
  return a.windSpeed==b.windSpeed && a.fetch==b.fetch && a.depth==b.depth &&
    a.swell==b.swell && a.spread==b.spread &&
    a.shortWaveDamping==b.shortWaveDamping && sameCross(a.crossSwell,b.crossSwell) &&
    a.cascadeDisplacement==b.cascadeDisplacement &&
    a.cascadeChoppiness==b.cascadeChoppiness;
}

void applyRuntimeControls(AndroidShell &shell) {
  // Only the explicit legacy laboratory owns these controls. The independent
  // editor uses renderingSettings + device/thermal policy, without synthesizing
  // an ocean resource or overriding quality from laboratory defaults.
  if (shell.independentWorkspace) return;
  auto controls = ae::platform::android::runtimeControlsSnapshot();
  const auto &document=shell.editorSession.document();
  const auto *root=document.find(document.root());
  const bool authored=shell.editorUi && shell.editorMapImported && root && ae::editor::waterSettings(*root).enabled;
  ae::u64 documentRevision=authored?14695981039346656037ull:0;
  if(authored) for(ae::u32 index=0;index<34;++index) { const float value=ae::editor::waterSettings(*root).legacyField(index);
    ae::u32 bits;std::memcpy(&bits,&value,sizeof(bits));
    documentRevision^=bits;documentRevision*=1099511628211ull;
  }
  if(authored && ae::editor::waterSettings(*root).spectrumEnabled) documentRevision^=1;
  ae::u64 layoutRevision=0;
  if(authored && ae::editor::waterSettings(*root).layoutEnabled) {
    layoutRevision=14695981039346656037ull;
    for(ae::u32 index=0;index<9;++index) { const float value=ae::editor::waterSettings(*root).legacyField(34+index);
      ae::u32 bits;std::memcpy(&bits,&value,sizeof(bits));layoutRevision^=bits;layoutRevision*=1099511628211ull;
    }
    documentRevision^=layoutRevision;
  }
  if (controls.revision == shell.runtimeControlsRevision && documentRevision==shell.waterDocumentRevision) return;
  shell.waterDocumentRevision=documentRevision;
  if(authored) {
    controls.waveHeight=ae::editor::waterSettings(*root).waveHeight;controls.waveSpeed=ae::editor::waterSettings(*root).waveSpeed;controls.waveSteepness=ae::editor::waterSettings(*root).steepness;
    controls.microWaves=ae::editor::waterSettings(*root).microWaves;controls.surfaceOpacity=ae::editor::waterSettings(*root).opacity;controls.absorption=ae::editor::waterSettings(*root).absorption;
    controls.foam=ae::editor::waterSettings(*root).foam;controls.waterRoughness=ae::editor::waterSettings(*root).roughness;controls.waterTurbidity=ae::editor::waterSettings(*root).turbidity;
    controls.waterIor=ae::editor::waterSettings(*root).ior;controls.waveDirectionDegrees=ae::editor::waterSettings(*root).directionDegrees;
    controls.waterLevel=ae::editor::waterSettings(*root).level;controls.fluidDensity=ae::editor::waterSettings(*root).density;
    if(ae::editor::waterSettings(*root).spectrumEnabled) {
      controls.spectralWindSpeed=ae::editor::waterSettings(*root).windSpeed;controls.spectralFetch=ae::editor::waterSettings(*root).fetch;
      controls.spectralDepth=ae::editor::waterSettings(*root).depth;controls.spectralSwell=ae::editor::waterSettings(*root).swell;
      controls.spectralSpread=ae::editor::waterSettings(*root).spread;controls.spectralDamping=ae::editor::waterSettings(*root).damping;
      controls.crossWindSpeed=ae::editor::waterSettings(*root).crossWindSpeed;controls.crossDirectionDegrees=ae::editor::waterSettings(*root).crossDirection;
      controls.crossFetch=ae::editor::waterSettings(*root).crossFetch;controls.crossSwellShape=ae::editor::waterSettings(*root).crossSwell;
      controls.crossSpread=ae::editor::waterSettings(*root).crossSpread;controls.crossWeight=ae::editor::waterSettings(*root).crossWeight;
      for(ae::usize i=0;i<3;++i) {
        controls.cascadeDisplacement[i]=ae::editor::waterSettings(*root).legacyField(0+25+i);
        controls.cascadeChoppiness[i]=ae::editor::waterSettings(*root).legacyField(0+28+i);
      }
      controls.foamCompression=ae::editor::waterSettings(*root).foamThreshold;controls.foamGrowth=ae::editor::waterSettings(*root).foamGrowth;controls.foamDecay=ae::editor::waterSettings(*root).foamDecay;
    }
  }
  shell.waterInteractionStrength = controls.interactionStrength;

  ae::renderer::WaterSpectrumAuthoringSettings spectrum{};
  spectrum.windSpeed=controls.spectralWindSpeed;
  spectrum.fetch=controls.spectralFetch;
  spectrum.depth=controls.spectralDepth;
  spectrum.swell=controls.spectralSwell;
  spectrum.spread=controls.spectralSpread;
  spectrum.shortWaveDamping=controls.spectralDamping;
  spectrum.crossSwell.windSpeed=controls.crossWindSpeed;
  spectrum.crossSwell.directionRadians=controls.crossDirectionDegrees*0.017453292519943295f;
  spectrum.crossSwell.fetch=controls.crossFetch;
  spectrum.crossSwell.swell=controls.crossSwellShape;
  spectrum.crossSwell.spread=controls.crossSpread;
  spectrum.crossSwell.weight=controls.crossWeight;
  for(ae::usize index=0;index<3;++index) {
    spectrum.cascadeDisplacement[index]=controls.cascadeDisplacement[index];
    spectrum.cascadeChoppiness[index]=controls.cascadeChoppiness[index];
  }
  if(authored && ae::editor::waterSettings(*root).layoutEnabled) {
    spectrum.cascadeDisplacement[3]=ae::editor::waterSettings(*root).displacement4;
    spectrum.cascadeChoppiness[3]=ae::editor::waterSettings(*root).choppiness4;
  }
  if(!shell.waterSpectrumAuthoringApplied ||
     !sameSpectrumAuthoring(spectrum,shell.waterSpectrumAuthoring) || layoutRevision!=shell.waterLayoutRevision) {
    const auto defaults=ae::renderer::defaultWaterCascadeSettings();
    std::vector<ae::renderer::WaterCascadeSettings> base(defaults.begin(),defaults.end());
    ae::u64 budget=4ull*1024*1024;
    bool accepted=true;
    if(authored && ae::editor::waterSettings(*root).layoutEnabled) {
      ae::renderer::WaterCascadeLayout layout;
      layout.count=static_cast<ae::u32>(ae::editor::waterSettings(*root).cascadeCount);
      layout.resolution=1u<<static_cast<ae::u32>(ae::editor::waterSettings(*root).resolutionLog2);
      layout.minimumWavelength=ae::editor::waterSettings(*root).minimumWavelength;layout.maximumWavelength=ae::editor::waterSettings(*root).maximumWavelength;
      layout.seed=static_cast<ae::u32>(ae::editor::waterSettings(*root).seed);layout.domainScale=ae::editor::waterSettings(*root).domainScale;
      budget=static_cast<ae::u64>(ae::editor::waterSettings(*root).budgetMiB*1024*1024);
      accepted=ae::renderer::buildWaterCascadeLayout(layout,base);
    }
    std::vector<ae::renderer::WaterCascadeSettings> cascades;
    accepted=accepted && ae::renderer::authorWaterCascades(base,spectrum,cascades);
    if(accepted) accepted=shell.instancedRendererReady
        ? shell.instancedRenderer.reconfigureWaterCascades(cascades,budget)
        : shell.instancedRenderer.setWaterCascades(cascades,budget);
    if(!accepted) {
      __android_log_print(ANDROID_LOG_ERROR,LogTag,
          "[WaterFFT] configuracao autoral recusada; espectro anterior preservado.");
    } else {
      shell.waterSpectrumAuthoring=spectrum;
      shell.waterSpectrumAuthoringApplied=true;
      shell.waterLayoutRevision=layoutRevision;
    }
    if(authored) shell.editorSession.reportWaterConfiguration(accepted);
  }

  auto water = ae::renderer::defaultOceanWaterProfile();
  if (!ae::renderer::authorWaterWaves(water,
      {controls.swellLength,controls.directionalSpread,controls.crossSwell},water)) {
    __android_log_print(ANDROID_LOG_ERROR,"Aether.Android","[Water] invalid wave authoring settings");
    return;
  }
  const float direction = controls.waveDirectionDegrees * 0.017453292519943295f;
  const float rotationCos = std::cos(direction), rotationSin = std::sin(direction);
  for (ae::u32 index = 0; index < water.waveCount; ++index) {
    const auto original = water.waves[index].direction;
    water.waves[index].direction = {original.x * rotationCos - original.y * rotationSin,
                                    original.x * rotationSin + original.y * rotationCos};
    water.waves[index].amplitude *= controls.waveHeight;
    water.waves[index].speed *= controls.waveSpeed;
    water.waves[index].steepness = std::clamp(
        water.waves[index].steepness * controls.waveSteepness, 0.0f, 1.0f);
  }
  water.microWaveStrength = controls.microWaves;
  if(controls.longWaveAmplitude>0) {
    // The laboratory explicitly trades the last cross-swell component for a
    // long-wave slot; the reusable authoring API never overwrites silently.
    if(water.waveCount==ae::renderer::MaximumWaterWaves) --water.waveCount;
    if(!ae::renderer::appendLongWaterWave(water,controls.longWaveAmplitude,
         controls.longWaveLength,30,{rotationCos,rotationSin})) {
      __android_log_print(ANDROID_LOG_ERROR,"Aether.Android","[Water] invalid long-wave settings");
      return;
    }
  }
  water.surfaceOpacity = controls.surfaceOpacity;
  water.absorption.x *= controls.absorption;
  water.absorption.y *= controls.absorption;
  water.absorption.z *= controls.absorption;
  water.foamDecay = controls.foam;
  water.roughness = controls.waterRoughness;
  water.turbidity = controls.waterTurbidity;
  water.refractiveIndex = controls.waterIor;
  ae::renderer::WaterSpectralControls spectral;
  spectral.displacement=std::min(controls.waveHeight,3.0f); spectral.choppiness=controls.waveSteepness;
  spectral.timeScale=controls.waveSpeed; spectral.directionRadians=direction;
  spectral.foam={controls.foamCompression,controls.foamGrowth,controls.foamDecay};
  spectral.overrideFoam=true;
  if(!shell.instancedRenderer.setWaterSpectralControls(spectral))
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[RuntimeControls] controles espectrais recusados.");
  if (!shell.instancedRenderer.setWaterProfile(water,controls.waterLevel))
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[RuntimeControls] perfil de água recusado.");
  if (!shell.instancedRenderer.setWaterShading({controls.specularAntialiasing,
      controls.contactFoamWidth, controls.foamElevation, controls.foamCoverage,
      controls.microDisplacement, controls.microWavelength}))
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[RuntimeControls] shading de água recusado.");

  if (!shell.renderingCapabilitiesReady || !shell.instancedRendererReady) return;
  auto requestedSettings=shell.renderingSettings;
  if(controls.solidLodError>0) requestedSettings.lodPixelErrorBudget=controls.solidLodError;
  if(controls.foliageLodError>0) requestedSettings.coverageLodPixelErrorBudget=controls.foliageLodError;
  if(controls.lodTransition>0) requestedSettings.lodHysteresisBandRatio=controls.lodTransition;
  auto policy = ae::renderer::resolveRenderingPolicy(
      requestedSettings, shell.renderingCapabilities, shell.thermalMonitor.state().pressure);
  // Uma opção de lançamento é override de diagnóstico: se o painel puder
  // sobrescrevê-la, toda medição feita com ela é inválida sem aviso. Foi
  // exatamente assim que as capturas do oceano ficaram presas em escala 0,5,
  // com o padrão do painel (dinâmica ligada) anulando o pedido explícito.
  const bool scaleFromLaunchOption = shell.renderingSettings.resolutionScale > 0.0f;
  if (!scaleFromLaunchOption)
    policy.resolutionScale = std::min(policy.resolutionScale, controls.renderScale);
  policy.dynamicResolution.maximumScale = std::min(policy.dynamicResolution.maximumScale,
                                                   policy.resolutionScale);
  const bool dynamicFromLaunchOption =
      shell.renderingSettings.dynamicResolution != ae::renderer::FeatureOverride::Inherit;
  const bool dynamicEnabled = dynamicFromLaunchOption ? policy.dynamicResolution.enabled
                                                      : controls.dynamicResolution;
  policy.dynamicResolution.enabled = dynamicEnabled;
  if (!dynamicEnabled) {
    policy.dynamicResolution.minimumScale = policy.resolutionScale;
    policy.dynamicResolution.maximumScale = policy.resolutionScale;
  } else {
    policy.dynamicResolution.minimumScale = std::min(
        policy.dynamicResolution.minimumScale, policy.dynamicResolution.maximumScale);
  }
  if (controls.shadowQuality == 0) {
    policy.shadows.enabled = false; policy.shadows.cascadeCount = 0;
  } else if (controls.shadowQuality == 1) {
    policy.shadows.cascadeCount = std::min(policy.shadows.cascadeCount, 1u);
    policy.shadows.filterTaps = policy.shadows.farFilterTaps = 1;
  } else if (controls.shadowQuality == 2) {
    policy.shadows.cascadeCount = std::min(policy.shadows.cascadeCount, 2u);
    policy.shadows.filterTaps = std::min(policy.shadows.filterTaps, 9u);
    policy.shadows.farFilterTaps = 1;
  }
  policy.post.bloom = controls.bloomIntensity > 0.001f;
  policy.post.bloomIntensity = controls.bloomIntensity;
  policy.post.sharpen = controls.sharpen;
  shell.activeRenderingPolicy = policy;
  shell.instancedRenderer.setRuntimeRenderingPolicy(policy);
  shell.runtimeControlsRevision = controls.revision;
  // A escala efetiva e a origem dela aparecem juntas: sem a origem, uma captura
  // presa numa escala não distingue decisão de política de override ignorado.
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[RuntimeControls] scale=%.2f(%s) dynamic=%d(%s)[%.2f,%.2f] shadows=%u "
      "water=[height %.2f speed %.2f opacity %.2f].",
      static_cast<double>(policy.resolutionScale), scaleFromLaunchOption ? "launch" : "painel",
      dynamicEnabled ? 1 : 0, dynamicFromLaunchOption ? "launch" : "painel",
      static_cast<double>(policy.dynamicResolution.minimumScale),
      static_cast<double>(policy.dynamicResolution.maximumScale),
      controls.shadowQuality, static_cast<double>(controls.waveHeight),
      static_cast<double>(controls.waveSpeed), static_cast<double>(controls.surfaceOpacity));
}

bool waterHitFromScreen(const AndroidShell &shell, float x, float y, float width,
                        float height, ae::renderer::WaterVec2 &hit) {
  if (width <= 0.0f || height <= 0.0f) return false;
  const auto &camera = shell.cameraController.state();
  const float ndcX = x * 2.0f / width - 1.0f;
  const float ndcY = 1.0f - y * 2.0f / height;
  const float aspect = width / height;
  const float viewX = ndcX * aspect / 1.732050808f;
  const float viewY = -ndcY / 1.732050808f;
  const float cy = std::cos(camera.yaw), sy = std::sin(camera.yaw);
  const float cp = std::cos(camera.pitch), sp = std::sin(camera.pitch);
  const float directionX = cy * viewX + sy * sp * viewY + sy * cp;
  const float directionY = cp * viewY - sp;
  const float directionZ = -sy * viewX + cy * sp * viewY + cy * cp;
  if (std::abs(directionY) < 1.0e-5f) return false;
  const float distance = -camera.position[1] / directionY;
  if (!(distance > 0.0f) || distance > 2000.0f) return false;
  hit = {camera.position[0] + directionX * distance,
         camera.position[2] + directionZ * distance};
  return std::isfinite(hit.x) && std::isfinite(hit.y);
}

// Atomic-writes any pending recorded samples to disk. Safe to call repeatedly
// (each call re-writes the full route atomically, so it is called from every
// teardown path -- background/foreground cycling during a recording session
// must never lose samples already captured) and safe to call with zero
// samples recorded (a no-op).
void flushCameraRouteRecordingIfNeeded(AndroidShell &shell) {
  if (shell.cameraRouteMode != ae::platform::CameraRouteMode::Record) return;
  if (shell.cameraRouteRecorder.sampleCount() == 0) return;
  const bool ok = shell.cameraRouteRecorder.writeToFile(
      shell.cameraRoutePath, shell.instancedRenderer.contentFingerprint(),
      shell.frameBudget.renderHz);
  __android_log_print(ok ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, LogTag,
      "[CameraRoute] gravação de %u amostras em '%s': %s", shell.cameraRouteRecorder.sampleCount(),
      shell.cameraRoutePath, ok ? "sucesso" : "falhou");
}

const char *profileSceneId(const AndroidShell &shell) {
  if (shell.independentWorkspace) return "empty-workspace";
  if (shell.oceanPreview) return "ocean";
  if (shell.dirtRoadPreview) return "dirt-road";
  if (shell.materialPreview) return "material-preview";
  if (shell.scenePreview) return "scene-preview";
  return "poc-a-5000-textured-cubes";
}

// Extrai o runtime .NET vendorizado (se ainda não extraído) e hospeda o
// CoreCLR — item 0.1.4 do plano. Chamado uma vez na criação do shell, não a
// cada frame: hostfxr não tem (nem precisa de) um caminho de "reinicializar",
// o contexto vive pelo tempo de vida do processo (ver DotNetHost::shutdown).
// Falha aqui não é fatal para o shell gráfico — o app continua rodando sem
// CoreCLR (mesma disciplina de "continuar sem GPU" já usada para Vulkan),
// só o caminho gerenciado fica indisponível.
void initializeDotNetHost(AndroidShell &shell) {
  char dotnetRoot[512];
  if (!ae::platform::android::ensureDotNetAssetsExtracted(shell.app->activity, dotnetRoot,
                                                          sizeof(dotnetRoot))) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao extrair os assets do runtime .NET — CoreCLR não será hospedado.");
    return;
  }

  char nativeLibraryDir[512];
  if (!ae::platform::android::getNativeLibraryDir(shell.app->activity, nativeLibraryDir,
                                                  sizeof(nativeLibraryDir))) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "Falha ao resolver nativeLibraryDir — CoreCLR não será hospedado.");
    return;
  }

  char runtimeConfigPath[600];
  char managedAssemblyPath[600];
  std::snprintf(runtimeConfigPath, sizeof(runtimeConfigPath), "%s/Aether.Rendering.runtimeconfig.json",
               dotnetRoot);
  std::snprintf(managedAssemblyPath, sizeof(managedAssemblyPath), "%s/Aether.Rendering.dll", dotnetRoot);

  if (!shell.dotNetHost.initialize(nativeLibraryDir, dotnetRoot, runtimeConfigPath,
                                   managedAssemblyPath)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "DotNetHost::initialize falhou.");
    return;
  }

  if(shell.independentWorkspace) return;

  // Prova de vida mínima (mesmo teste validado manualmente antes de integrar
  // aqui — ver docs/ESTADO.md item 0.1.4): resolve e chama um método
  // gerenciado real, confirmando que a fronteira C++→C# funciona dentro do
  // processo do NativeActivity, não só num executável standalone via shell.
  using PingFn = int (*)(int, int);
  auto ping = reinterpret_cast<PingFn>(shell.dotNetHost.getManagedFunctionPointer(
      "Aether.Interop.NativeEntryPoints, Aether.Core", "Ping"));
  if (ping != nullptr) {
    __android_log_print(ANDROID_LOG_INFO, LogTag, "CoreCLR hospedado no shell: Ping(2,3)=%d.",
                        ping(2, 3));
  }
  if (shell.scenePreview) {
    shell.applySceneStep = reinterpret_cast<AndroidShell::SceneStepFn>(shell.dotNetHost.getManagedFunctionPointer(
        "Aether.Rendering.Interop.SceneEntryPoints, Aether.Rendering", "ApplyValidationStep"));
    shell.shutdownScene = reinterpret_cast<AndroidShell::SceneShutdownFn>(shell.dotNetHost.getManagedFunctionPointer(
        "Aether.Rendering.Interop.SceneEntryPoints, Aether.Rendering", "Shutdown"));
  }
}

// Build reads a saved source snapshot in a worker. Only the session thread may
// accept its generation and publish the applied pointer; compiling never starts
// a Behavior or modifies the running scene.
void updateEditorLanguage(AndroidShell &shell) {
  using namespace ae::platform::android;
  if(auto request=takeEditorLanguageQuery()) {
    shell.languageNext=std::move(request);
    if(shell.languageWork.valid() && shell.languageCancel) shell.languageCancel();
  }
  if(shell.languageWork.valid()) {
    if(shell.languageWork.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) return;
    const auto result=shell.languageWork.get();
    if(shell.languageActive) completeEditorLanguageQuery(*shell.languageActive,result);
    shell.languageActive.reset();
  }
  if(!shell.languageNext) return;
  if(!shell.dotNetHost.isReady()) initializeDotNetHost(shell);
  if(shell.dotNetHost.isReady() && !shell.languageQuery) {
    constexpr const char *type="Astra.Compilation.NativeLanguage, Astra.Scripting";
    shell.languageQuery=reinterpret_cast<AndroidShell::CodeBuildFn>(shell.dotNetHost.getManagedFunctionPointer(type,"Query"));
    shell.languageReport=reinterpret_cast<AndroidShell::CodeReportFn>(shell.dotNetHost.getManagedFunctionPointer(type,"CopyReply"));
    shell.languageCancel=reinterpret_cast<void(*)()>(shell.dotNetHost.getManagedFunctionPointer(type,"Cancel"));
  }
  shell.languageActive=std::move(shell.languageNext);shell.languageNext.reset();
  if(!shell.languageQuery || !shell.languageReport) {
    completeEditorLanguageQuery(*shell.languageActive,R"({"Message":"Serviço C# indisponível.","Items":[]})");return;
  }
  shell.languageWork=std::async(std::launch::async,[input=shell.languageActive->json,query=shell.languageQuery,copy=shell.languageReport] {
    query(reinterpret_cast<const ae::u8*>(input.data()),static_cast<int>(input.size()));
    const int size=copy(nullptr,0);
    if(size<=0 || size>1024*1024) return std::string(R"({"Message":"Resposta de linguagem excede o limite.","Items":[]})");
    std::string result(static_cast<size_t>(size),'\0');
    if(copy(reinterpret_cast<ae::u8*>(result.data()),size)!=size) return std::string(R"({"Message":"Resposta incompleta.","Items":[]})");
    return result;
  });
}
void updateEditorCodeCompiler(AndroidShell &shell) {
  if(!shell.editorUi || !shell.independentWorkspace) return;
  updateEditorLanguage(shell);
  shell.editorSession.setCodeCompilerAvailable(true);
  if(shell.editorSession.needsScriptRuntime()&&!shell.dotNetHost.isReady()) initializeDotNetHost(shell);
  if(shell.dotNetHost.isReady()&&!shell.scriptRuntime.available()) {
    constexpr const char *type="Astra.Runtime.NativeBehaviorRuntime, Astra.Scripting";
    auto &api=shell.scriptRuntime;
    api.start=reinterpret_cast<decltype(api.start)>(shell.dotNetHost.getManagedFunctionPointer(type,"Start"));
    api.update=reinterpret_cast<decltype(api.update)>(shell.dotNetHost.getManagedFunctionPointer(type,"Update"));
    api.fixedUpdate=reinterpret_cast<decltype(api.fixedUpdate)>(shell.dotNetHost.getManagedFunctionPointer(type,"FixedUpdate"));
    api.trigger=reinterpret_cast<decltype(api.trigger)>(shell.dotNetHost.getManagedFunctionPointer(type,"Trigger"));
    api.contact=reinterpret_cast<decltype(api.contact)>(shell.dotNetHost.getManagedFunctionPointer(type,"Contact"));
    api.stop=reinterpret_cast<decltype(api.stop)>(shell.dotNetHost.getManagedFunctionPointer(type,"Stop"));
    api.copyDiagnostics=reinterpret_cast<decltype(api.copyDiagnostics)>(shell.dotNetHost.getManagedFunctionPointer(type,"CopyDiagnostics"));
  }
  if(shell.scriptRuntime.available()) shell.editorSession.setScriptRuntime(shell.scriptRuntime);
  static constexpr const char *failure =
      "ASTRA_CODE 1 0 1 \"\" 1 1 1 \"ASTRA_HOST\" \"Compilador indisponível; código preservado\" 0";
  if(shell.codeCompilation.valid()) {
    if(shell.codeCompilation.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) return;
    const auto report=shell.codeCompilation.get();
    if(shell.editorSession.completeCodeBuild(report))
      shell.editorSession.reportCodeCommit(shell.codeCommit && shell.codeCommit()==0);
  }
  const auto root=shell.editorSession.takeCodeBuildRequest();
  if(root.empty()) return;
  if(!shell.dotNetHost.isReady()) initializeDotNetHost(shell);
  if(shell.dotNetHost.isReady() && !shell.codeBuild) {
    constexpr const char *type="Astra.Compilation.NativeCompiler, Astra.Scripting";
    shell.codeBuild=reinterpret_cast<AndroidShell::CodeBuildFn>(shell.dotNetHost.getManagedFunctionPointer(type,"Build"));
    shell.codeReport=reinterpret_cast<AndroidShell::CodeReportFn>(shell.dotNetHost.getManagedFunctionPointer(type,"CopyReport"));
    shell.codeCommit=reinterpret_cast<AndroidShell::CodeCommitFn>(shell.dotNetHost.getManagedFunctionPointer(type,"Commit"));
  }
  if(!shell.codeBuild || !shell.codeReport || !shell.codeCommit) {
    shell.editorSession.completeCodeBuild(failure);return;
  }
  shell.codeCompilation=std::async(std::launch::async,[root,build=shell.codeBuild,report=shell.codeReport] {
    if(build(reinterpret_cast<const ae::u8 *>(root.data()),static_cast<int>(root.size()))<0) return std::string(failure);
    const int size=report(nullptr,0);
    if(size<=0 || size>4*1024*1024) return std::string(failure);
    std::string text(static_cast<size_t>(size),'\0');
    if(report(reinterpret_cast<ae::u8 *>(text.data()),size)!=size) return std::string(failure);
    return text;
  });
}

// Reconstrói o renderer instanciado depois que a surface/swapchain existem
// (na criação) ou depois que a swapchain foi recriada (resize/rotação) —
// framebuffers referenciam VkImageView específicos da swapchain anterior,
// então não sobrevivem a uma recriação, mesmo que o renderer em si pudesse.
// Sem CoreCLR pronto (dotNetHost.isReady() falso), a PoC-A simplesmente não
// roda nesta sessão — degradação silenciosa, mesma disciplina de "continuar
// sem GPU" já usada para Vulkan.
// The worker exclusively owns the renderer and graphics queue until get() hands
// ownership back. No frame submits overlap upload. Surface teardown cancels and
// joins BEFORE destroying anything referenced by the worker.
namespace {
// O registro de recursos mora ao lado da cena, dentro do projeto.
std::string projectAssetRegistryPath(const char *projectPath) {
  return projectPath && projectPath[0] ? std::string(projectPath)+"/.astra/assets.astra" : std::string();
}
bool writeProjectAssetRegistry(const char *projectPath,const std::string &text) {
  const auto path=projectAssetRegistryPath(projectPath);
  if(path.empty()) return false;
  std::error_code code;
  std::filesystem::create_directories(std::string(projectPath)+"/.astra",code);
  return !code && ae::editor::EditorImportTransaction::writeText(ae::editor::EditorImportTransaction::fromUtf8(path),text);
}
// Reabre as fontes registradas e republica a geometria delas. Sem isto, uma cena
// salva com um modelo importado abriria com os objetos apontando para recursos
// que este processo ainda não carregou -- referência ausente, objeto invisível.
//
// R1: o registro é lido aqui (rápido); ler e interpretar cada GLB vai para um
// worker. Antes, isto rodava fonte por fonte no loop nativo e cada fonte
// republicava a biblioteca acumulada com espera de GPU, tudo antes do primeiro
// quadro do editor -- a tela preta entre o carregamento e o editor.
// Devolve falso quando não há fonte a reabrir: a cena pode ser recuperada já.
bool startProjectReopen(AndroidShell &shell) {
  const auto path=projectAssetRegistryPath(shell.editorProjectPath);
  if(path.empty()) return false;
  std::string text;
  if(FILE *file=std::fopen(path.c_str(),"rb")) {
    char chunk[4096];ae::usize read=0;
    while((read=std::fread(chunk,1,sizeof(chunk),file))>0) text.append(chunk,read);
    std::fclose(file);
  }
  if(text.empty()) return false;
  if(!shell.editorSession.loadAssets(text)) {
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Import] registro de recursos invalido; preservado no disco.");
    return false;
  }
  // Publication replaces the registry storage. Keep an owning snapshot: a span
  // (or a copy of only the current record) leaves later iterations dangling.
  std::vector<std::string> sources;
  for(const auto &record:shell.editorSession.assets().records())
    if(record.type==ae::resources::AssetType::Mesh && !record.source.empty()) sources.push_back(record.source);
  if(sources.empty()) return false;
  shell.reopenCancellation=std::make_shared<std::atomic<bool>>(false);
  shell.reopenProgress=std::make_shared<AndroidShell::ReopenProgress>();
  shell.reopenProgress->total=sources.size();
  shell.projectReopening=true;
  shell.reopenStartedMs=ae::platform::android::lifecycleUptimeMs();
  const std::string root(shell.editorProjectPath);
  const auto limits=shell.editorSession.importLimits();
  const auto count=sources.size();
  shell.reopenWork=std::async(std::launch::async,[root,sources=std::move(sources),limits,
                                                  cancel=shell.reopenCancellation,progress=shell.reopenProgress]() {
    using Transaction=ae::editor::EditorImportTransaction;
    AndroidShell::ProjectReopen result;
    result.root=root;
    const auto stage=[&progress](const std::string &source,const char *what) {
      std::lock_guard<std::mutex> hold(progress->lock);
      progress->current=source;progress->stage=what;
    };
    std::unordered_set<std::string> usedKeys;
    for(const auto &source:sources) {
      if(cancel->load()) break;
      const double started=ae::platform::android::lifecycleUptimeMs();
      stage(source,"lendo");
      std::vector<ae::u8> bytes;
      if(FILE *file=std::fopen((root+"/"+source).c_str(),"rb")) {
        char chunk[16384];ae::usize read=0;
        while((read=std::fread(chunk,1,sizeof(chunk),file))>0)
          bytes.insert(bytes.end(),reinterpret_cast<ae::u8 *>(chunk),reinterpret_cast<ae::u8 *>(chunk)+read);
        std::fclose(file);
      }
      if(bytes.empty()) {
        result.missing.push_back(source);
        std::lock_guard<std::mutex> hold(progress->lock);++progress->done;
        continue;
      }
      ae::editor::EditorSession::ReopenedSource reopened;
      reopened.sourceName=source;
      reopened.hash=ae::Sha256::hex(bytes);
      const double hashedAt=ae::platform::android::lifecycleUptimeMs();
      // R2: derivado regenerável por conteúdo + limites. Acerto pula o importador
      // inteiro; arquivo ausente, velho ou corrompido cai no importador.
      const auto key=ae::resources::importCacheKey(reopened.hash,limits);
      usedKeys.insert(key);
      const auto cachePath=Transaction::fromUtf8(root+"/"+ae::resources::importCacheRelativePath(key));
      bool cached=false;
      {
        stage(source,"lendo derivado");
        std::vector<ae::u8> derived;
        if(Transaction::read(cachePath,derived,ae::usize{1}<<30))
          cached=ae::resources::readImportCache(derived,key,reopened.model);
      }
      const double derivedAt=ae::platform::android::lifecycleUptimeMs();
      double writeMs=0;
      if(!cached) {
        stage(source,"importando");
        ae::resources::GltfImportProgress watch{};
        watch.context=cancel.get();
        watch.cancelled=[](void *context) {return static_cast<std::atomic<bool> *>(context)->load();};
        if(!ae::resources::importGlb(bytes,limits,watch,reopened.model)) {
          result.refused.emplace_back(source,reopened.model.diagnostic);
          std::lock_guard<std::mutex> hold(progress->lock);++progress->done;
          continue;
        }
        stage(source,"gravando derivado");
        const double writeStarted=ae::platform::android::lifecycleUptimeMs();
        std::vector<ae::u8> derived;
        std::error_code error;
        std::filesystem::create_directories(cachePath.parent_path(),error);
        if(error || !ae::resources::writeImportCache(reopened.model,key,derived) || !Transaction::write(cachePath,derived))
          __android_log_print(ANDROID_LOG_WARN,LogTag,"[Cache] derivado de %s não gravado; a próxima abertura importa de novo.",source.c_str());
        writeMs=ae::platform::android::lifecycleUptimeMs()-writeStarted;
      } else {
        ++result.cacheHits;
      }
      __android_log_print(ANDROID_LOG_INFO,LogTag,
          "[Open] fonte preparada: %s bytes=%zu cache=%s leitura_fonte_hash_ms=%.0f leitura_derivado_ms=%.0f preparo_ms=%.0f gravacao_derivado_ms=%.0f",
          source.c_str(),bytes.size(),cached?"acerto":"falta",hashedAt-started,derivedAt-hashedAt,
          ae::platform::android::lifecycleUptimeMs()-started-writeMs,writeMs);
      result.sources.push_back(std::move(reopened));
      std::lock_guard<std::mutex> hold(progress->lock);++progress->done;
    }
    // Derivados que nenhuma fonte atual usa (fonte trocada, limites mudados) só
    // ocupam disco. Numa abertura interrompida a lista está incompleta: não poda.
    if(!cancel->load()) {
      std::error_code error;
      const auto directory=Transaction::fromUtf8(root+"/.astra/cache/imports");
      for(std::filesystem::directory_iterator it(directory,error),end;!error && it!=end;it.increment(error)) {
        const auto name=it->path().filename().string();
        if(name.size()!=68 || !name.ends_with(".aic") || usedKeys.contains(name.substr(0,64))) continue;
        std::error_code removal;
        if(std::filesystem::remove(it->path(),removal))
          __android_log_print(ANDROID_LOG_INFO,LogTag,"[Cache] derivado sem fonte removido: %s",name.c_str());
      }
    }
    return result;
  });
  __android_log_print(ANDROID_LOG_INFO,LogTag,"[Open] preparando %zu fonte(s) em segundo plano.",count);
  shell.editorSession.setImportStatus("Abrindo recursos do projeto…");
  return true;
}

// Cena salva do projeto (ou migração do arquivo privado antigo). Só depois das
// fontes: a cena referencia recursos que precisam estar na biblioteca adotada.
// Até aqui `editorSavePath` fica vazio, e é isso que mantém o salvamento
// automático desligado enquanto o documento ainda não é a cena do projeto.
void recoverProjectScene(AndroidShell &shell) {
  if (!shell.editorMapImported || !shell.app->activity->internalDataPath) return;
  ae::u64 projectId=14695981039346656037ull;
  for(const unsigned char *p=reinterpret_cast<const unsigned char *>(shell.editorProjectPath);*p;++p) {projectId^=*p;projectId*=1099511628211ull;}
  const std::string legacyPath=std::string(shell.app->activity->internalDataPath)+"/editor-"+
      std::to_string(projectId)+"-"+std::to_string(shell.instancedRenderer.contentFingerprint())+".aescene";
  // The project owns authored data. Keep the old private archive as a migration backup.
  shell.editorSavePath=shell.editorProjectPath[0]
      ? std::string(shell.editorProjectPath)+"/scenes/editor.aescene" : legacyPath;
  FILE *existing=std::fopen(shell.editorSavePath.c_str(),"rb");
  if(existing) {
    std::fclose(existing);
    if(!shell.editorSession.load(shell.editorSavePath.c_str(),shell.editorPackageFingerprint)) {
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Cena salva invalida; arquivo preservado.");
      shell.editorSavePath += ".recovered";
      FILE *recovery=std::fopen(shell.editorSavePath.c_str(),"rb");
      if(recovery) {
        std::fclose(recovery);
        if(!shell.editorSession.load(shell.editorSavePath.c_str(),shell.editorPackageFingerprint)) {
          __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Recuperacao invalida; salvamento automatico suspenso.");
          shell.editorSavePath.clear();
        }
      }
    }
  } else if(shell.editorSavePath!=legacyPath) {
    FILE *legacy=std::fopen(legacyPath.c_str(),"rb");
    if(legacy) {
      std::fclose(legacy);
      if(!shell.editorSession.load(legacyPath.c_str(),shell.editorPackageFingerprint))
        __android_log_print(ANDROID_LOG_WARN,LogTag,"[Editor] Arquivo legado invalido; original preservado.");
    }
    if(!shell.editorSession.save(shell.editorSavePath.c_str(),shell.editorPackageFingerprint))
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Falha ao criar cena no projeto: %s",shell.editorSavePath.c_str());
  }
  auto &document=shell.editorSession.document();
  auto root=*document.find(document.root());
  if(!shell.independentWorkspace && !ae::editor::waterSettings(root).enabled) {
    const auto c=ae::platform::android::runtimeControlsSnapshot();
    const float values[]{c.waveHeight,c.waveSpeed,c.waveSteepness,c.microWaves,c.surfaceOpacity,
      c.absorption,c.foam,c.waterRoughness,c.waterTurbidity,c.waterIor,c.waveDirectionDegrees,c.waterLevel,c.fluidDensity};
    if(auto *settings=ae::editor::editWaterSettings(root)) {for(ae::u32 i=0;i<13;++i) settings->legacyField(i)=values[i];document.applyEntityValues(root.id,root);}
  }
  if(!shell.independentWorkspace && !ae::editor::waterSettings(root).spectrumEnabled) {
    const auto c=ae::platform::android::runtimeControlsSnapshot();
    const float values[]{c.spectralWindSpeed,c.spectralFetch,c.spectralDepth,c.spectralSwell,
      c.spectralSpread,c.spectralDamping,c.crossWindSpeed,c.crossDirectionDegrees,c.crossFetch,
      c.crossSwellShape,c.crossSpread,c.crossWeight,c.cascadeDisplacement[0],c.cascadeDisplacement[1],
      c.cascadeDisplacement[2],c.cascadeChoppiness[0],c.cascadeChoppiness[1],c.cascadeChoppiness[2],
      c.foamCompression,c.foamGrowth,c.foamDecay};
    if(auto *settings=ae::editor::editWaterSettings(root)) for(ae::u32 i=0;i<std::size(values);++i) settings->legacyField(13+i)=values[i];
    document.applyEntityValues(root.id,root);
  }
  shell.editorSavedRevision=shell.editorSession.document().revision();
}

// R1: volta à thread do editor com as fontes preparadas. Uma publicação para
// todas, depois a cena salva. Fonte ausente ou recusada vira aviso; o editor
// abre com o resto do projeto em vez de parar na primeira falha.
void finishProjectReopen(AndroidShell &shell) {
  auto result=shell.reopenWork.get();
  shell.projectReopening=false;
  const double preparedAt=ae::platform::android::lifecycleUptimeMs();
  if(result.root!=shell.editorProjectPath) {
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[Open] preparo descartado: o projeto mudou durante a abertura.");
    return;
  }
  std::vector<ae::editor::EditorSession::ModelImportReport> reports;
  std::string diagnostic;
  const bool published=shell.editorSession.reopenSources(result.sources,reports,diagnostic);
  ae::usize failures=result.missing.size()+result.refused.size();
  for(ae::usize i=0;i<result.sources.size() && i<reports.size();++i) {
    if(reports[i].diagnostic.empty())
      __android_log_print(ANDROID_LOG_INFO,LogTag,"[Import] fonte reaberta: %s",result.sources[i].sourceName.c_str());
    else {
      ++failures;
      __android_log_print(ANDROID_LOG_WARN,LogTag,"[Import] fonte %s recusada: %s",
                          result.sources[i].sourceName.c_str(),reports[i].diagnostic.c_str());
    }
  }
  for(const auto &source:result.missing)
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[Import] fonte ausente: %s",source.c_str());
  for(const auto &[source,reason]:result.refused)
    __android_log_print(ANDROID_LOG_WARN,LogTag,"[Import] fonte %s recusada: %s",source.c_str(),reason.c_str());
  if(!published)
    __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Open] publicacao das fontes falhou: %s",diagnostic.c_str());
  {
    const auto &residency=shell.editorSession.textureResidency();
    __android_log_print(ANDROID_LOG_INFO,LogTag,
        "[Residencia] texturas=%u pedidos_mb=%.1f residentes_mb=%.1f teto_mb=%.1f reduzidas=%u niveis_removidos=%u cabe=%s",
        residency.textures,residency.requestedBytes/1048576.0,residency.residentBytes/1048576.0,
        residency.budgetBytes/1048576.0,residency.reducedTextures,residency.droppedLevels,residency.withinBudget()?"sim":"nao");
  }
  const double publishedAt=ae::platform::android::lifecycleUptimeMs();
  recoverProjectScene(shell);
  shell.editorPublishedRevision=~ae::u64{0};
  const double doneAt=ae::platform::android::lifecycleUptimeMs();
  __android_log_print(ANDROID_LOG_INFO,LogTag,
      "[Open] projeto aberto: fontes=%zu derivados_reaproveitados=%zu falhas=%zu preparo_ms=%.0f publicacao_ms=%.0f cena_ms=%.0f total_ms=%.0f",
      result.sources.size(),result.cacheHits,failures,preparedAt-shell.reopenStartedMs,publishedAt-preparedAt,doneAt-publishedAt,
      doneAt-shell.reopenStartedMs);
  if(failures)
    shell.editorSession.setImportStatus("Projeto aberto; "+std::to_string(failures)+" recurso(s) não carregado(s). Detalhes no console.",
                                        ae::editor::EditorConsoleSeverity::Warning);
  else
    shell.editorSession.setImportStatus("Projeto aberto");
}
} // namespace

void collectRendererInitialization(AndroidShell &shell, bool cancel) {
  if (!shell.rendererInitialization.valid()) return;
  if (cancel) shell.cancelRendererInitialization.store(true);
  else if (shell.rendererInitialization.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
  const bool ready=shell.rendererInitialization.get();
  shell.instancedRendererReady=ready && !cancel;
  // A sessao de edicao so pode existir depois que os atlas chegaram a GPU: sao
  // eles que dizem quanto mede cada glifo e onde cada icone vive. Antes disso
  // `update` sai cedo e a tela fica sem interface -- que foi exatamente o
  // sintoma quando esta chamada faltava.
  if (shell.editorUi && shell.instancedRendererReady &&
      shell.instancedRenderer.uiRendererReady()) {
    // O editor não conhece Vulkan: quem sobe geometria importada para a GPU é o
    // shell, e é ele que devolve o pacote resultante para o editor adotar.
    // Antes das fontes reabrirem, a sessão sabe se o aparelho amostra ASTC 4x4.
    shell.editorSession.setImportAstc4x4(shell.instancedRenderer.supportsAstc4x4());
    __android_log_print(ANDROID_LOG_INFO,LogTag,"[Import] KTX2 com mips vira %s.",
                        shell.instancedRenderer.supportsAstc4x4()?"ASTC 4x4":"RGBA8 (aparelho sem ASTC 4x4)");
    shell.editorSession.setGeometryPublisher(
        [&shell](std::span<const ae::u8> vertices, std::span<const ae::u32> indices,
                 std::span<const ae::renderer::MapDrawRecord> draws,
                 std::span<const ae::renderer::MapMaterialRecord> materials,
                 std::span<const ae::renderer::SharedAuthoringTexture> textures,
                 ae::editor::EditorSession::PublishedGeometry &out) {
          if(!shell.instancedRenderer.rebuildAuthoringGeometry(vertices,indices,draws,materials,textures)) return false;
          out={shell.instancedRenderer.mapDraws(),shell.instancedRenderer.mapMaterials(),
               shell.instancedRenderer.pickingVertices(),shell.instancedRenderer.pickingIndices()};
          // A republicação da cena é obrigatória depois de trocar o pacote: as
          // poses publicadas descreviam a lista anterior de desenhos.
          shell.editorPublishedRevision=~ae::u64{0};
          return true;
        });
    shell.editorSession.initialize(&shell.instancedRenderer.uiFont(),
                                   &shell.instancedRenderer.uiIcons());
    shell.editorSession.usePlatformTextInput(true);
    shell.editorSession.setScriptLogSink([](ae::u64 id,std::string_view message) {
      __android_log_print(ANDROID_LOG_INFO,"Astra.Script","%llu: %.*s",
                          static_cast<unsigned long long>(id),static_cast<int>(message.size()),message.data());
    });
    // Já havia sessão antes desta inicialização? Então a superfície foi
    // recriada e este renderer é novo: a cena do editor sobrevive, a biblioteca
    // dele não.
    const bool rehydrating = shell.editorMapImported;
    if (!shell.editorMapImported && (shell.independentWorkspace || !shell.instancedRenderer.mapDraws().empty())) {
      shell.editorPackageFingerprint=shell.independentWorkspace ? 0 : shell.instancedRenderer.contentFingerprint();
      if (!shell.independentWorkspace) shell.editorSession.setProjection(shell.instancedRenderer.mapProjection());
      shell.editorMapImported = shell.editorSession.importMap(shell.instancedRenderer.mapDraws(),shell.instancedRenderer.mapMaterials(), !shell.editorEmpty,shell.instancedRenderer.pickingVertices(),shell.instancedRenderer.pickingIndices(),shell.editorPackageFingerprint);
      if (!shell.editorEmpty) {
        const auto initial=shell.instancedRenderer.defaultCamera();
        shell.editorSession.setCameraPose(initial.position,initial.yaw,initial.pitch);
      }
      // R1: com fontes registradas, a cena espera o worker (finishProjectReopen);
      // sem fontes, a cena é recuperada já, como antes.
      if (shell.editorMapImported && !startProjectReopen(shell)) recoverProjectScene(shell);
    }
    if (shell.independentWorkspace)
      __android_log_print(ANDROID_LOG_INFO, LogTag,"[Editor] independent resources=%zu fingerprint=%llu managed=off",
          shell.instancedRenderer.mapDraws().size(), static_cast<unsigned long long>(shell.editorPackageFingerprint));
    // Reidratação gráfica: sem isto a publicação do documento falha a cada
    // quadro contra uma biblioteca que não tem mais a geometria importada, e o
    // viewport fica vazio com a hierarquia inteira do lado.
    if (rehydrating) {
      std::string diagnostic;
      if (shell.editorSession.republishGeometry(diagnostic))
        __android_log_print(ANDROID_LOG_INFO, LogTag,
            "[Editor] Geometria republicada na superficie nova.");
      else
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
            "[Editor] Reidratacao grafica falhou: %s", diagnostic.c_str());
    }
    shell.editorPublishedRevision = ~ae::u64{0};
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Editor] sessao pronta: %u entidades no documento.",
        shell.editorSession.document().entityCount());
  }
  if (shell.instancedRendererReady && shell.dirtRoadPreview && !shell.mapCameraInitialized &&
      shell.instancedRenderer.hasDefaultCamera()) {
    const ae::platform::FreeCameraState initialCamera=shell.hasLaunchCamera?shell.launchCamera
        :(shell.firstPersonEnabled?shell.instancedRenderer.defaultGameplayCamera()
                                 :shell.instancedRenderer.defaultCamera());
    shell.cameraController.setState(initialCamera);
    if(shell.firstPersonEnabled){
      const auto &mesh=shell.instancedRenderer.staticCollisionMesh();
      const AetherVec3 spawn{initialCamera.position[0],initialCamera.position[1],
                             initialCamera.position[2]};
      if(!shell.characterMotor.initialize(mesh.vertices,mesh.indices,spawn)){
        __android_log_print(ANDROID_LOG_ERROR,LogTag,
            "[FirstPerson] Falha ao criar cápsula/malha estática de colisão.");
        shell.instancedRendererReady=false;
      }else{
        __android_log_print(ANDROID_LOG_INFO,LogTag,
            "[FirstPerson] colisão pronta vertices=%zu triangles=%zu.",
            mesh.vertices.size(),mesh.indices.size()/3);
      }
    }
    shell.instancedRenderer.releaseStaticCollisionCpuData();
    shell.mapCameraInitialized = shell.instancedRendererReady;
    if (shell.hasLaunchCamera) {
      const auto &camera=shell.cameraController.state();
      __android_log_print(ANDROID_LOG_INFO, LogTag,
          "[Camera] launch pose=(%.2f,%.2f,%.2f) yaw=%.3f pitch=%.3f locked=%s",
          camera.position[0],camera.position[1],camera.position[2],camera.yaw,camera.pitch,
          shell.lockCamera?"true":"false");
    }
    if (shell.cameraRouteMode == ae::platform::CameraRouteMode::Replay) {
      const bool loaded = shell.cameraRoutePlayer.loadFromFile(shell.cameraRoutePath);
      const ae::u64 sceneFingerprint = shell.instancedRenderer.contentFingerprint();
      shell.cameraRouteReplayActive = loaded &&
          shell.cameraRoutePlayer.data().sceneFingerprint == sceneFingerprint;
      if (!shell.cameraRouteReplayActive) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
            "[CameraRoute] replay de '%s' recusado (carregado=%s, fingerprint rota=%016llx cena=%016llx); "
            "câmera permanece na pose travada.",
            shell.cameraRoutePath, loaded ? "sim" : "não",
            loaded ? static_cast<unsigned long long>(shell.cameraRoutePlayer.data().sceneFingerprint) : 0ull,
            static_cast<unsigned long long>(sceneFingerprint));
      } else {
        __android_log_print(ANDROID_LOG_INFO, LogTag,
            "[CameraRoute] replay ativo: '%s', %u ticks, fingerprint=%016llx.",
            shell.cameraRoutePath, shell.cameraRoutePlayer.tickCount(),
            static_cast<unsigned long long>(shell.cameraRoutePlayer.data().sceneFingerprint));
      }
    } else if (shell.cameraRouteMode == ae::platform::CameraRouteMode::Record) {
      shell.cameraRouteRecorder.reset();
      shell.cameraRouteRecorder.reserve(shell.frameBudget.renderHz * 60u); // hint: ~60s a cadência alvo
      __android_log_print(ANDROID_LOG_INFO, LogTag,
          "[CameraRoute] gravação ativa, será escrita em '%s' ao encerrar.", shell.cameraRoutePath);
    }
  }
  if (shell.instancedRendererReady) applyThermalRenderingPolicy(shell, true);
  if (!shell.instancedRendererReady) shell.instancedRenderer.shutdown();
  if (shell.instancedRendererReady && shell.lifecycle.isActive())
    shell.performance.setActive(true, false);
  __android_log_print(cancel ? ANDROID_LOG_INFO : (ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR),
      LogTag,"[%s] initialization=%s",shell.independentWorkspace?"EmptyWorkspace":shell.dirtRoadPreview?"DirtRoad":"MaterialPreview",
      cancel?"cancelled":(ready?"ready":"failed"));
}

// A política só pode ser resolvida depois que o device Vulkan existe: antes
// disso `deviceFeatures()` é o valor default e todo aparelho pareceria perfil C.
// Esta é a "época de configuração" da ADR-014 — uma resolução por criação de
// device, imutável enquanto ela durar.
void resolveRenderingPolicyForDevice(AndroidShell &shell, float displayHz) {
  const auto &device = shell.vulkanSurface.device();
  const auto &features = device.deviceFeatures();
  auto &capabilities = shell.renderingCapabilities;
  capabilities = {};
  capabilities.profile = device.deviceProfile();
  capabilities.maximumImage2DSize = device.maximumImage2DSize();
  capabilities.maximumImageArrayLayers = device.maximumImageArrayLayers();
  // O suporte real a depth amostrável é decidido junto do formato, na criação do
  // depth; aqui entra otimista e o renderer reduz se recusar o formato.
  capabilities.supportsDepthSampling = true;
  capabilities.maximumSamplerAnisotropy = device.maximumSamplerAnisotropy();
  capabilities.displayHz = displayHz;
  shell.renderingCapabilitiesReady = true;
  // Recursos são sempre criados a partir da escolha do projeto sem pressão.
  // A política térmica ativa só reduz o uso destes recursos e pode recuperar
  // sem rebuild quando a histerese do monitor autorizar.
  shell.renderingPolicy = ae::renderer::resolveRenderingPolicy(
      shell.renderingSettings, capabilities, ae::renderer::ThermalPressure::None);
  shell.activeRenderingPolicy = shell.renderingPolicy;
  shell.thermalPolicyApplied = false;
  // VisibilityBudget also carries HZB knobs that are not part of the rendering
  // policy yet. Synchronize only the resolved LOD axes so diagnostics and the
  // renderer consume the same values without maintaining a second preset table.
  shell.visibilityBudget.lodPixelErrorBudget =
      shell.renderingPolicy.visibility.lodPixelErrorBudget;
  shell.visibilityBudget.coverageLodPixelErrorBudget =
      shell.renderingPolicy.visibility.coverageLodPixelErrorBudget;
  shell.visibilityBudget.lodHysteresisBandRatio =
      shell.renderingPolicy.visibility.lodHysteresisBandRatio;

  // Os fatos que produziram o perfil ficam ao lado da política: sem eles,
  // "auto escolheu C" é indistinguível de um bug da política.
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[RenderPolicy] features: vulkan1_3=%d descriptor_indexing=%d nonuniform=%d "
      "ray_query=%d mesh_shader=%d vrs=%d memoryless=%d -> perfil=%d",
      features.vulkan1_3 ? 1 : 0, features.descriptorIndexing ? 1 : 0,
      features.bindlessNonUniformIndexing ? 1 : 0, features.rayQuery ? 1 : 0,
      features.meshShader ? 1 : 0, features.variableRateShading ? 1 : 0,
      features.memorylessAttachments ? 1 : 0, static_cast<int>(device.deviceProfile()));
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[RenderPolicy] preset=%s perfil=%d sombras=%s(%u cascatas @%u, %u/%u taps, cache=%s margem=%.2f blend=%.2f fade=%.2f) "
      "ambiente=%s%s%s pos=%s%s malha_agua=%s(%u seg) textura_mip_bias=%u aniso=%.1f escala=%.2f dinamica=%s[%.2f,%.2f] clamps=%u",
      ae::renderer::qualityPresetName(shell.renderingSettings.preset),
      static_cast<int>(shell.renderingPolicy.effectiveProfile),
      shell.renderingPolicy.shadows.enabled ? "on" : "off",
      shell.renderingPolicy.shadows.cascadeCount,
      shell.renderingPolicy.shadows.cascadeResolution,
      shell.renderingPolicy.shadows.filterTaps,
      shell.renderingPolicy.shadows.farFilterTaps,
      shell.renderingPolicy.shadows.staticCasterCache ? "static" : "off",
      static_cast<double>(shell.renderingPolicy.shadows.cacheGuardBandRatio),
      static_cast<double>(shell.renderingPolicy.shadows.cascadeBlendRatio),
      static_cast<double>(shell.renderingPolicy.shadows.distanceFadeRatio),
      shell.renderingPolicy.ambient.hemispheric ? "hemisferio" : "constante",
      shell.renderingPolicy.ambient.specularProbe ? "+especular" : "",
      shell.renderingPolicy.ambient.splitSumBrdf ? "+split-sum" : "",
      shell.renderingPolicy.post.dedicatedPass ? "passe" : "inline",
      shell.renderingPolicy.post.bloom ? "+bloom" : "",
      // A densidade escolhida entra no relatório junto do resto: uma medição só
      // é comparável com outra se o estado que a produziu estiver ao lado dela.
      ae::renderer::waterMeshQualityName(shell.renderingPolicy.geometry.waterMesh),
      // O vento ainda não é parâmetro de cena: sai do espectro, que é quem o
      // define hoje. Quando a cena passar a autorá-lo, é daqui que se lê.
      ae::renderer::selectWaterGrid(shell.renderingPolicy.geometry.waterMesh,
                                    ae::renderer::WaterSpectrumSettings{}.windSpeed,
                                    8000.0f).segments,
      shell.renderingPolicy.textures.residencyMipBias,
      static_cast<double>(shell.renderingPolicy.textures.samplerAnisotropy),
      static_cast<double>(shell.renderingPolicy.resolutionScale),
      shell.renderingPolicy.dynamicResolution.enabled ? "on" : "off",
      static_cast<double>(shell.renderingPolicy.dynamicResolution.minimumScale),
      static_cast<double>(shell.renderingPolicy.dynamicResolution.maximumScale),
      shell.renderingPolicy.clampCount);
  for (ae::u32 index = 0; index < shell.renderingPolicy.clampCount; ++index) {
    const auto &clamp = shell.renderingPolicy.clamps[index];
    __android_log_print(ANDROID_LOG_INFO, LogTag, "[RenderPolicy] %s reduzido por %s.",
                        clamp.axis, ae::renderer::policyClampName(clamp.reason));
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[RenderPolicy] lod=%s erro_solido=%.2fpx erro_coverage=%.2fpx histerese=%.2f "
      "normal_ate=%.1f specular_ate=%.1f "
      "mr_ate=%.1f emissive_ate=%.1f variantes_material=%s aa=%s bloom=%d sharpen=%.2f contraste=%.2f saturacao=%.2f",
      shell.renderingPolicy.geometry.lodSelection ? "on" : "off",
      static_cast<double>(shell.renderingPolicy.visibility.lodPixelErrorBudget),
      static_cast<double>(shell.renderingPolicy.visibility.coverageLodPixelErrorBudget),
      static_cast<double>(shell.renderingPolicy.visibility.lodHysteresisBandRatio),
      static_cast<double>(shell.renderingPolicy.materialDistance.normalMapMaximumDistance),
      static_cast<double>(shell.renderingPolicy.materialDistance.specularProbeMaximumDistance),
      static_cast<double>(shell.renderingPolicy.materialDistance.metallicRoughnessMaximumDistance),
      static_cast<double>(shell.renderingPolicy.materialDistance.emissiveMaximumDistance),
      shell.renderingPolicy.geometry.materialShaderVariants ? "on" : "off",
      ae::renderer::antiAliasingModeName(shell.renderingPolicy.post.antiAliasing),
      shell.renderingPolicy.post.bloom ? 1 : 0,
      static_cast<double>(shell.renderingPolicy.post.sharpen),
      static_cast<double>(shell.renderingPolicy.post.contrast),
      static_cast<double>(shell.renderingPolicy.post.saturation));
}

void applyThermalRenderingPolicy(AndroidShell &shell, bool force) {
  if (!shell.instancedRendererReady || !shell.renderingCapabilitiesReady) return;
  const auto pressure = shell.thermalMonitor.state().pressure;
  if (!force && shell.thermalPolicyApplied && pressure == shell.appliedThermalPressure) return;

  shell.activeRenderingPolicy = ae::renderer::resolveRenderingPolicy(
      shell.renderingSettings, shell.renderingCapabilities, pressure);
  shell.instancedRenderer.setRuntimeRenderingPolicy(shell.activeRenderingPolicy);
  shell.appliedThermalPressure = pressure;
  shell.thermalPolicyApplied = true;
  shell.frameProfiler.reset();
  __android_log_print(
      ANDROID_LOG_INFO, LogTag,
      "[ThermalPolicy] applied=%s lod=%.2f/%.2f shadow=%s/%uc@%u/%ut "
      "post=%s%s scale=%.2f dynamic=[%.2f,%.2f]",
      ae::renderer::thermalPressureName(pressure),
      static_cast<double>(shell.activeRenderingPolicy.visibility.lodPixelErrorBudget),
      static_cast<double>(shell.activeRenderingPolicy.visibility.coverageLodPixelErrorBudget),
      shell.activeRenderingPolicy.shadows.enabled ? "on" : "off",
      shell.activeRenderingPolicy.shadows.cascadeCount,
      shell.activeRenderingPolicy.shadows.cascadeResolution,
      shell.activeRenderingPolicy.shadows.filterTaps,
      shell.activeRenderingPolicy.post.dedicatedPass ? "pass" : "inline",
      shell.activeRenderingPolicy.post.bloom ? "+bloom" : "",
      static_cast<double>(shell.activeRenderingPolicy.resolutionScale),
      static_cast<double>(shell.activeRenderingPolicy.dynamicResolution.minimumScale),
      static_cast<double>(shell.activeRenderingPolicy.dynamicResolution.maximumScale));
}

bool rebuildInstancedRenderer(AndroidShell &shell) {
  collectRendererInitialization(shell,true);
  shell.oceanValidation.shutdown();
  shell.authoredWaterPlay.stop();
  shell.frameProfiler.reset();
  ae::platform::android::ScopedLifecycleStage trace("rebuild-renderer");
  shell.instancedRenderer.shutdown();
  if (!shell.dotNetHost.isReady() && !shell.dirtRoadPreview && !shell.independentWorkspace) {
    shell.instancedRendererReady = false;
    return false;
  }
  shell.instancedRendererReady = false;
  shell.instancedRenderer.setRenderingPolicy(shell.renderingPolicy);
  if (shell.materialPreview || shell.dirtRoadPreview || shell.independentWorkspace) {
    shell.cancelRendererInitialization.store(false);
    shell.rendererInitialization=std::async(std::launch::async,[&shell] {
      return shell.instancedRenderer.initialize(shell.vulkanSurface.device(),shell.vulkanSurface.swapchain(),
          shell.dotNetHost,shell.independentWorkspace ? 1 : ScenePreviewCapacity,
          !shell.dirtRoadPreview && !shell.independentWorkspace,shell.app->activity->assetManager,
          shell.forceTextureFallback,&shell.cancelRendererInitialization,shell.dirtRoadPreview,
          shell.oceanPreview ? "ocean" : "dirt_road",shell.independentWorkspace);
    });
    __android_log_print(ANDROID_LOG_INFO,LogTag,"[%s] initialization=loading",
                        shell.independentWorkspace?"EmptyWorkspace":shell.dirtRoadPreview?"DirtRoad":"MaterialPreview");
    return true;
  }
  shell.instancedRendererReady = shell.instancedRenderer.initialize(
      shell.vulkanSurface.device(), shell.vulkanSurface.swapchain(), shell.dotNetHost,
      shell.scenePreview ? ScenePreviewCapacity : PocAInstanceCount, shell.scenePreview,
      shell.materialPreview ? shell.app->activity->assetManager : nullptr, shell.forceTextureFallback);
  if (shell.instancedRendererReady) applyThermalRenderingPolicy(shell, true);
  else shell.instancedRenderer.shutdown();
  return shell.instancedRendererReady;
}

bool recreateSwapchainAndRenderer(AndroidShell &shell) {
  collectRendererInitialization(shell,true);
  shell.oceanValidation.shutdown();
  shell.authoredWaterPlay.stop();
  ae::platform::android::ScopedLifecycleStage trace("recreate-swapchain-renderer");
  // Framebuffers precisam morrer ANTES das image views da swapchain antiga.
  // Inverter esta ordem viola o lifetime Vulkan mesmo depois de wait-idle.
  shell.instancedRenderer.shutdown();
  shell.instancedRendererReady = false;
  if (shell.app->window == nullptr ||
      !shell.vulkanSurface.recreateSwapchain(shell.app->window)) {
    return false;
  }
  return rebuildInstancedRenderer(shell);
}

bool recreateSurfaceAndRenderer(AndroidShell &shell) {
  collectRendererInitialization(shell,true);
  ae::platform::android::ScopedLifecycleStage trace("recreate-surface-renderer");
  shell.instancedRenderer.shutdown();
  shell.instancedRendererReady = false;
  shell.vulkanSurface.shutdown();
  if (shell.app->window == nullptr ||
      !shell.vulkanSurface.initialize(shell.app->activity, shell.app->window,
                                      shell.frameBudget.renderHz, shell.useSwappy,
                                      !shell.forceDescriptorFallback)) {
    return false;
  }
  resolveRenderingPolicyForDevice(shell, shell.maximumDisplayHz);
  return rebuildInstancedRenderer(shell);
}

void applyEvent(AndroidShell &shell, ae::platform::AppEvent event) {
  const ae::platform::LifecycleAction action = shell.lifecycle.apply(event);
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::CreateSurface)) {
    ae::platform::android::ScopedLifecycleStage trace("create-surface-renderer");
    ae::platform::android::requestRenderFrameRate(shell.app->window,
                                                   static_cast<float>(shell.frameBudget.renderHz));
    if (!shell.vulkanSurface.initialize(shell.app->activity, shell.app->window,
                                        shell.frameBudget.renderHz, shell.useSwappy,
                                        !shell.forceDescriptorFallback)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "O shell continuará ativo sem GPU; uma nova janela tentará novamente.");
    } else if (resolveRenderingPolicyForDevice(shell, shell.maximumDisplayHz),
               !rebuildInstancedRenderer(shell)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "Surface pronta, mas o pipeline de desenho falhou ao inicializar.");
    }
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::DestroySurface)) {
    collectRendererInitialization(shell,true);
    flushCameraRouteRecordingIfNeeded(shell);
    ae::platform::android::ScopedLifecycleStage trace("destroy-surface-renderer");
    shell.oceanValidation.shutdown();
  shell.authoredWaterPlay.stop();
    shell.instancedRenderer.shutdown();
    shell.instancedRendererReady = false;
    shell.vulkanSurface.shutdown();
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::BecameActive)) {
    ae::platform::android::requestRenderFrameRate(shell.app->window,
                                                   static_cast<float>(shell.frameBudget.renderHz));
    shell.framePacer.start();
    shell.frameProfiler.reset();
    ++shell.activationCount;
    shell.activatedAtMs = ae::platform::android::lifecycleUptimeMs();
    shell.firstFrameAfterActivationPending = true;
    shell.lastPresentedAt = {};
    shell.smoothedFps = 0.0f;
    shell.pendingAdpfFrame = {};
    shell.performance.setActive(true, !shell.instancedRendererReady);
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Aplicativo ativo.");
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::BecameInactive)) {
    shell.framePacer.stop();
    shell.frameProfiler.reset();
    shell.cameraController.cancelGesture();
    shell.firstPersonTouches.cancel();
    shell.performance.setActive(false, false);
    flushCameraRouteRecordingIfNeeded(shell);
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Aplicativo suspenso.");
  }
}

void handleCommand(android_app *app, int32_t command) {
  auto &shell = *static_cast<AndroidShell *>(app->userData);
  const ae::u64 sequence = ++shell.lifecycleCommandSequence;
  const double startedMs = ae::platform::android::lifecycleUptimeMs();
  ae::platform::android::traceLifecycleCommand(*app, shell.lifecycle, sequence, command,
                                               "begin", shell.instancedRendererReady, 0.0);
  switch (command) {
  case APP_CMD_INIT_WINDOW:
    applyEvent(shell, ae::platform::AppEvent::WindowCreated);
    break;
  case APP_CMD_TERM_WINDOW:
    shell.windowResizePending=false;
    shell.editorSession.cancelPointers();
    applyEvent(shell, ae::platform::AppEvent::WindowDestroyed);
    break;
  case APP_CMD_RESUME:
    applyEvent(shell, ae::platform::AppEvent::Resume);
    break;
  case APP_CMD_PAUSE:
    shell.editorSession.cancelPointers();
    if(!shell.editorSavePath.empty() && !shell.editorSession.save(shell.editorSavePath.c_str(),shell.editorPackageFingerprint))
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Falha ao salvar ao pausar.");
    applyEvent(shell, ae::platform::AppEvent::Pause);
    break;
  case APP_CMD_GAINED_FOCUS:
    ae::platform::android::applyImmersiveLandscapeWindow(app->activity);
    applyEvent(shell, ae::platform::AppEvent::GainFocus);
    break;
  case APP_CMD_LOST_FOCUS:
    shell.editorSession.cancelPointers();
    // The attached code panel owns keyboard focus while the same Activity
    // remains resumed. Keep draining its revision queue and drawing native
    // tabs/console. APP_CMD_PAUSE still suspends the whole editor normally.
    if (!ae::platform::android::editorCodePanelVisible())
      applyEvent(shell, ae::platform::AppEvent::LoseFocus);
    break;
  case APP_CMD_WINDOW_RESIZED:
    // CONFIG_CHANGED can precede the actual buffer resize. On Adreno the
    // intermediate swapchain can continue presenting successfully forever,
    // stretched into the new window. Reconcile after the resize has settled,
    // and after any asynchronous renderer initialization has completed.
    shell.windowResizePending=true;
    shell.windowResizeAfter=std::chrono::steady_clock::now()+std::chrono::milliseconds(120);
    shell.editorSession.cancelPointers();
    break;
  case APP_CMD_CONFIG_CHANGED:
    if (app->window != nullptr) {
      const int newRotation = ae::platform::android::queryDisplayRotation(
          app->activity, shell.displayRotation);
      const bool physicalRotationChanged = shell.displayRotation >= 0 &&
                                           newRotation != shell.displayRotation;
      shell.displayRotation = newRotation;
      // A pointer delta must never bridge two coordinate systems. This is
      // global for both the editor camera and runtime action controller.
      shell.editorSession.cancelPointers();
      shell.cameraController.cancelGesture();
      shell.firstPersonTouches.cancel();
      __android_log_print(ANDROID_LOG_INFO, LogTag, "Configuração alterada: janela=%dx%d.",
                          ANativeWindow_getWidth(app->window),
                          ANativeWindow_getHeight(app->window));
      // O evento do SO pode chegar antes do Vulkan reportar OutOfDate (mais
      // comum em drivers mobile que em desktop) — recriar aqui em vez de só
      // esperar o próximo acquire/present falhar, mesmo aviso documentado
      // em ISwapchain::recreate.
      // A 180-degree landscape flip keeps the same WxH. Some Android drivers
      // therefore leave an old currentTransform attached to the existing
      // VkSurfaceKHR and never report OUT_OF_DATE. A fresh Surface is required
      // only for a real Display rotation; UI-mode and keyboard changes retain
      // the cheaper swapchain path.
      const bool recreated = !shell.vulkanSurface.isReady() ||
          (physicalRotationChanged ? recreateSurfaceAndRenderer(shell)
                                   : recreateSwapchainAndRenderer(shell));
      if (!recreated) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
                            "Falha ao recriar surface/swapchain após mudança de configuração.");
      }
    }
    break;
  case APP_CMD_LOW_MEMORY:
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "Android solicitou redução imediata do uso de memória.");
    break;
  default:
    break;
  }
  ae::platform::android::traceLifecycleCommand(
      *app, shell.lifecycle, sequence, command, "end", shell.instancedRendererReady,
      ae::platform::android::lifecycleUptimeMs() - startedMs);
}

// AInputEvent remains platform-only. The reusable controller exposes a camera
// state that keyboard/gamepad/NoCode can drive later through the same contract.
int32_t handleInput(android_app *app, AInputEvent *event) {
  if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

  auto &shell = *static_cast<AndroidShell *>(app->userData);
  if(shell.editorUi && shell.instancedRendererReady) {
    // Drain text before tab/undo commands, then let Android own a gesture
    // begun inside the visible CodeField. Returning zero forwards it through
    // NativePostImeInputStage; forwarding only Down would break selection.
    ae::platform::android::updateEditorTextInput(shell.editorSession);
    if(ae::platform::android::editorCodeOwnsPointer(
        AMotionEvent_getX(event,0)/shell.editorScale,AMotionEvent_getY(event,0)/shell.editorScale,
        AMotionEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK)) {
      shell.cameraController.cancelGesture();shell.firstPersonTouches.cancel();return 0;
    }
  }
  if (shell.lockCamera) {
    shell.cameraController.cancelGesture();
    shell.firstPersonTouches.cancel();
    return 1;
  }
  else if(shell.instancedRendererReady&&shell.dirtRoadPreview&&shell.characterMotor.isReady())
    shell.instancedRenderer.releaseStaticCollisionCpuData();
  const int32_t action = AMotionEvent_getAction(event);
  const int32_t actionType = action & AMOTION_EVENT_ACTION_MASK;
  const size_t actionIndex = static_cast<size_t>(
      (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

  // A interface do editor ve o toque PRIMEIRO. Ela devolve se consumiu, e so o
  // que sobra chega aos controladores de camera e de personagem -- senao um
  // arraste no Inspector giraria a cena por tras do painel.
  // R1: enquanto as fontes do projeto abrem, o documento ainda não é a cena
  // salva. Um toque criaria edições que a recuperação da cena descartaria.
  if (shell.editorUi && shell.instancedRendererReady && shell.projectReopening) return 1;
  if (shell.editorUi && shell.instancedRendererReady) {
    const size_t pointerCount = AMotionEvent_getPointerCount(event);
    const auto toLogical = [&](size_t index) {
      return ae::ui::UiPoint{AMotionEvent_getX(event, index) / shell.editorScale,
                             AMotionEvent_getY(event, index) / shell.editorScale};
    };
    const int32_t editorAction = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const size_t editorIndex = static_cast<size_t>(
        (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
        AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    bool consumed = false;
    if (editorAction == AMOTION_EVENT_ACTION_CANCEL) {
      shell.editorSession.cancelPointers();
      consumed = true;
    } else if (editorAction == AMOTION_EVENT_ACTION_MOVE) {
      // Um MOVE carrega TODOS os dedos de uma vez. Repassar so o do indice da
      // acao perderia o segundo dedo da pinca em todo quadro.
      for (size_t index = 0; index < pointerCount; ++index)
        consumed |= shell.editorSession.handlePointer(
            {static_cast<ae::u32>(AMotionEvent_getPointerId(event, index)),
             ae::ui::UiPointerPhase::Move, toLogical(index), 0.0});
    } else if (editorAction == AMOTION_EVENT_ACTION_DOWN ||
               editorAction == AMOTION_EVENT_ACTION_POINTER_DOWN) {
      consumed = shell.editorSession.handlePointer(
          {static_cast<ae::u32>(AMotionEvent_getPointerId(event, editorIndex)),
           ae::ui::UiPointerPhase::Down, toLogical(editorIndex), 0.0});
    } else if (editorAction == AMOTION_EVENT_ACTION_UP ||
               editorAction == AMOTION_EVENT_ACTION_POINTER_UP) {
      consumed = shell.editorSession.handlePointer(
          {static_cast<ae::u32>(AMotionEvent_getPointerId(event, editorIndex)),
           ae::ui::UiPointerPhase::Up, toLogical(editorIndex), 0.0});
    }
    if (consumed) {
      // Preserve editor capture through Down -> Move -> Up; runtime loses ownership.
      shell.firstPersonTouches.cancel();
      shell.cameraController.cancelGesture();
      return 1;
    }
  }

  // A cena autorada não herda controles ou interações da demonstração.
  if (shell.editorUi) return 1;

  const int32_t bufferWidth = app->window != nullptr ? ANativeWindow_getWidth(app->window) : 0;
  const int32_t bufferHeight = app->window != nullptr ? ANativeWindow_getHeight(app->window) : 0;
  // The swapchain can use a 90-degree pre-transform: its buffer is portrait
  // while Android motion coordinates and the visible HUD are landscape.
  const int32_t width = std::max(bufferWidth, bufferHeight);
  const int32_t height = std::min(bufferWidth, bufferHeight);
  if (width <= 0 || height <= 0) return 0;
  if (shell.oceanPreview) {
    if (actionType == AMOTION_EVENT_ACTION_DOWN) {
      shell.waterTapTracking = true;
      shell.waterTapMoved = false;
      shell.waterTapPointer = AMotionEvent_getPointerId(event, actionIndex);
      shell.waterTapX = AMotionEvent_getX(event, actionIndex);
      shell.waterTapY = AMotionEvent_getY(event, actionIndex);
    } else if (actionType == AMOTION_EVENT_ACTION_MOVE && shell.waterTapTracking) {
      const size_t count = AMotionEvent_getPointerCount(event);
      for (size_t index = 0; index < count; ++index) {
        if (AMotionEvent_getPointerId(event, index) != shell.waterTapPointer) continue;
        const float dx = AMotionEvent_getX(event, index) - shell.waterTapX;
        const float dy = AMotionEvent_getY(event, index) - shell.waterTapY;
        if (dx * dx + dy * dy > 24.0f * 24.0f) shell.waterTapMoved = true;
      }
    } else if (actionType == AMOTION_EVENT_ACTION_UP && shell.waterTapTracking) {
      if (!shell.waterTapMoved) {
        ae::renderer::WaterVec2 hit{};
        if (waterHitFromScreen(shell, AMotionEvent_getX(event, actionIndex),
                               AMotionEvent_getY(event, actionIndex),
                               static_cast<float>(width), static_cast<float>(height), hit)) {
          ae::renderer::WaterImpulse impulse{};
          impulse.center = hit;
          impulse.startTime = shell.waterTimeSeconds;
          impulse.amplitude = shell.waterInteractionStrength;
          shell.instancedRenderer.addWaterImpulse(impulse);
          __android_log_print(ANDROID_LOG_INFO, LogTag,
              "[WaterInteraction] impulse=(%.2f,%.2f) amplitude=%.2f.",
              static_cast<double>(hit.x), static_cast<double>(hit.y),
              static_cast<double>(impulse.amplitude));
        }
      }
      shell.waterTapTracking = false;
    } else if (actionType == AMOTION_EVENT_ACTION_CANCEL) {
      shell.waterTapTracking = false;
    }
  }
  if (shell.firstPersonEnabled) {
    if (actionType == AMOTION_EVENT_ACTION_CANCEL) {
      shell.firstPersonTouches.cancel();
      return 1;
    }
    if (actionType == AMOTION_EVENT_ACTION_DOWN ||
        actionType == AMOTION_EVENT_ACTION_POINTER_DOWN) {
      shell.firstPersonTouches.pointerDown(
          AMotionEvent_getPointerId(event, actionIndex),
          AMotionEvent_getX(event, actionIndex), AMotionEvent_getY(event, actionIndex),
          static_cast<float>(width), static_cast<float>(height));
    } else if (actionType == AMOTION_EVENT_ACTION_MOVE) {
      const size_t pointerCount = AMotionEvent_getPointerCount(event);
      for (size_t index = 0; index < pointerCount; ++index) {
        shell.firstPersonTouches.pointerMove(
            AMotionEvent_getPointerId(event, index), AMotionEvent_getX(event, index),
            AMotionEvent_getY(event, index), static_cast<float>(width),
            static_cast<float>(height));
      }
    } else if (actionType == AMOTION_EVENT_ACTION_UP ||
               actionType == AMOTION_EVENT_ACTION_POINTER_UP) {
      const int32_t pointerId = AMotionEvent_getPointerId(event, actionIndex);
      const bool releasedMovement = shell.firstPersonTouches.joystickState().active &&
          shell.firstPersonTouches.joystickState().pointerId == pointerId;
      shell.firstPersonTouches.pointerUp(pointerId);
      if (releasedMovement) {
        const auto &camera = shell.cameraController.state();
        __android_log_print(ANDROID_LOG_INFO,LogTag,
            "[FirstPerson] position=(%.2f,%.2f,%.2f) yaw=%.3f pitch=%.3f ground=%u",
            camera.position[0],camera.position[1],camera.position[2],camera.yaw,camera.pitch,
            static_cast<ae::u32>(shell.characterMotor.groundState()));
      }
    }
    return 1;
  }
  if (actionType == AMOTION_EVENT_ACTION_CANCEL) {
    shell.cameraController.cancelGesture();
    return 1;
  }
  ae::platform::FreeCameraTouch touches[2]{};
  ae::u32 count = 0;
  const size_t pointerCount = AMotionEvent_getPointerCount(event);
  for (size_t index = 0; index < pointerCount && count < 2; ++index) {
    const bool lifted = (actionType == AMOTION_EVENT_ACTION_UP ||
                         actionType == AMOTION_EVENT_ACTION_POINTER_UP) && index == actionIndex;
    if (lifted) continue;
    touches[count++] = {AMotionEvent_getPointerId(event, index),
                        AMotionEvent_getX(event, index), AMotionEvent_getY(event, index)};
  }
  if (count == 2 && touches[0].id > touches[1].id) std::swap(touches[0], touches[1]);
  shell.cameraController.updateTouches(touches, count, static_cast<float>(width), static_cast<float>(height));
  if (actionType == AMOTION_EVENT_ACTION_UP) {
    const auto &camera = shell.cameraController.state();
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[Camera] position=(%.2f,%.2f,%.2f) yaw=%.3f pitch=%.3f",
        camera.position[0],camera.position[1],camera.position[2],camera.yaw,camera.pitch);
  }
  return 1;
}

} // namespace

void android_main(android_app *app) {
  AndroidShell shell{};
  shell.app = app;
  shell.displayRotation = ae::platform::android::queryDisplayRotation(app->activity);
  shell.forceDescriptorFallback = ae::platform::android::readBooleanLaunchOption(
      app->activity, "aether.force_descriptor_fallback");
  shell.scenePreview = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.scene_preview");
  const bool benchmarkPreview = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.poc_a");
  shell.pocABenchmark = benchmarkPreview;
  shell.materialPreview = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.material_preview");
  shell.oceanPreview = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.ocean_preview");
  shell.editorUi = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.editor_ui");
  if (shell.editorUi) {
    const float density = app->config != nullptr
        ? static_cast<float>(AConfiguration_getDensity(app->config)) : 0.0f;
    // Densidade do editor independente do DPI do sistema; input usa a mesma escala.
    float editorUiScale=0.60f;
    ae::platform::android::readFloatLaunchOption(app->activity,"aether.editor_scale",editorUiScale);
    if(!std::isfinite(editorUiScale)) editorUiScale=0.60f;
    shell.editorScale = std::clamp(editorUiScale,0.40f,1.50f) *
        (density > 0.0f && density < 10000.0f ? density / 160.0f : 1.0f);
    shell.editorSceneScale=shell.editorScale;

    ae::platform::android::readStringLaunchOption(app->activity,"astra.project_name",shell.editorProjectName,sizeof(shell.editorProjectName));
    ae::platform::android::readStringLaunchOption(app->activity,"astra.project_path",shell.editorProjectPath,sizeof(shell.editorProjectPath));
    shell.editorEmpty=ae::platform::android::readBooleanLaunchOption(app->activity,"aether.editor_empty");
    shell.independentWorkspace = shell.editorEmpty && ae::platform::android::readBooleanLaunchOption(app->activity,"aether.empty_workspace");
    shell.editorSession.setProjectName(*shell.editorProjectName?shell.editorProjectName:(shell.oceanPreview ? "Water Lab" : "Forest Road"));
    if(shell.editorProjectPath[0] && !shell.editorSession.setProjectDirectory(shell.editorProjectPath)) {
      __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Recovery] Projeto não aberto; arquivos preservados: %s",shell.editorSession.screen().status.c_str());
      ANativeActivity_finish(app->activity);return;
    }
  }
  shell.instancedRenderer.setWaterAuthoringEnabled(shell.editorUi && !shell.independentWorkspace);
  const bool explicitMap = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.map_preview");
  // The launcher opens the production-test map. Diagnostic fixtures remain
  // explicit so benchmark numbers can never silently include the full scene.
  shell.dirtRoadPreview = !shell.independentWorkspace && (shell.oceanPreview || explicitMap ||
      (!shell.scenePreview && !benchmarkPreview && !shell.materialPreview));
  shell.forceTextureFallback = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.force_texture_fallback");
  shell.lockCamera = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.lock_camera");
  ae::u32 requestedCameraRouteMode = 0;
  ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.camera_route_mode",
                                                   requestedCameraRouteMode);
  shell.cameraRouteMode = ae::platform::sanitizeCameraRouteMode(requestedCameraRouteMode);
  ae::platform::android::readStringLaunchOption(app->activity, "aether.camera_route_path",
      shell.cameraRoutePath, sizeof(shell.cameraRoutePath));
  if (shell.cameraRouteMode != ae::platform::CameraRouteMode::Off && shell.cameraRoutePath[0] == '\0') {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
        "[CameraRoute] modo '%s' pedido sem aether.camera_route_path; permanecendo Off.",
        ae::platform::cameraRouteModeName(shell.cameraRouteMode));
    shell.cameraRouteMode = ae::platform::CameraRouteMode::Off;
  }
  if (shell.cameraRouteMode == ae::platform::CameraRouteMode::Replay) {
    // A rota reproduzida dirige a câmera deterministicamente a cada frame;
    // input precisa ficar bloqueado exatamente como numa captura de câmera
    // travada, e o update manual de FirstPersonController/CharacterMotor não
    // pode competir com ela.
    shell.lockCamera = true;
  }
  // Controller mode is a global runtime policy. Diagnostic fixtures and locked
  // camera captures remain deterministic; `aether.free_camera` explicitly
  // selects the editor navigation controller instead.
  shell.firstPersonEnabled = !shell.editorUi && shell.dirtRoadPreview && !shell.oceanPreview && !shell.lockCamera &&
      !ae::platform::android::readBooleanLaunchOption(app->activity, "aether.free_camera");
  shell.instancedRenderer.setRuntimeHudEnabled(shell.firstPersonEnabled);
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[SceneSelection] assets=%s controller=%s hud=%d",
      shell.oceanPreview ? "ocean" : (shell.dirtRoadPreview ? "dirt_road" : "other"),
      shell.firstPersonEnabled ? "first-person" : "free/locked", shell.firstPersonEnabled ? 1 : 0);
  shell.hasLaunchCamera =
      ae::platform::android::readFloatLaunchOption(app->activity, "aether.camera_x", shell.launchCamera.position[0]) &&
      ae::platform::android::readFloatLaunchOption(app->activity, "aether.camera_y", shell.launchCamera.position[1]) &&
      ae::platform::android::readFloatLaunchOption(app->activity, "aether.camera_z", shell.launchCamera.position[2]) &&
      ae::platform::android::readFloatLaunchOption(app->activity, "aether.camera_yaw", shell.launchCamera.yaw) &&
      ae::platform::android::readFloatLaunchOption(app->activity, "aether.camera_pitch", shell.launchCamera.pitch);
  // Sem ProjectRenderingSettings serializado ainda, Auto usa a maior cadência
  // global suportada (até 120 Hz). A capability física e a preferência são
  // entradas distintas para não solicitar 120 Hz em painéis limitados a 60/90.
  const float maximumDisplayHz =
      ae::platform::android::queryMaximumDisplayRefreshRate(app->activity);
  float requestedMaximumRenderHz = static_cast<float>(ae::DefaultMaximumRenderHz);
  ae::platform::android::readFloatLaunchOption(
      app->activity, "aether.target_fps", requestedMaximumRenderHz);
  shell.frameBudget = ae::makeFrameBudget(maximumDisplayHz, requestedMaximumRenderHz, 60);
  shell.maximumDisplayHz = maximumDisplayHz;
  // Direct FIFO + Android VSYNC is the validated default. Swappy 2.1.3 is
  // retained as an explicit experiment, but its queuePresent path inserts
  // extra Vulkan work and, on the current Adreno driver, kept the app's
  // binary render-finished semaphore alive after the image was reacquired
  // (VUID-vkQueueSubmit-pSignalSemaphores-00067). Correct synchronization
  // outranks speculative pacing; never silently enable the invalid path.
  shell.useSwappy = ae::platform::android::readBooleanLaunchOption(
      app->activity, "aether.enable_swappy") &&
      !ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.disable_swappy");
  float performanceHintTargetRatio = 0.0f;
  ae::platform::android::readFloatLaunchOption(
      app->activity, "aether.adpf_target_ratio", performanceHintTargetRatio);
  ae::platform::android::AndroidPerformancePolicy performancePolicy{};
  performancePolicy.targetFrameDurationNs =
      ae::performanceHintTargetNanoseconds(shell.frameBudget, performanceHintTargetRatio);
  performancePolicy.preferSustainedPerformance =
      !ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.disable_sustained_performance");
  shell.performance.initialize(app->activity, performancePolicy);
  shell.thermalMonitor.initialize();
  shell.instancedRenderer.setAdpfGpuTimingEnabled(
      shell.performance.detailedWorkDurationAvailable());
  shell.framePacer.setTargetFrameRate(shell.frameBudget.renderHz);
  shell.scenePreview = shell.scenePreview || shell.materialPreview;
  shell.sceneValidation = shell.scenePreview &&
      ae::platform::android::readBooleanLaunchOption(app->activity, "aether.scene_validation");
  app->userData = &shell;
  app->onAppCmd = handleCommand;
  app->onInputEvent = handleInput;
  shell.instancedRenderer.setDisableTransientDepth(
      ae::platform::android::readBooleanLaunchOption(app->activity,
                                                     "aether.disable_transient_depth"));
  shell.frameProfiler.setEnabled(ae::platform::android::readFrameProfilingOption(app->activity));
  shell.instancedRenderer.setFrameProfilingEnabled(shell.frameProfiler.enabled());
  ae::u32 requestedIsolation = 0;
  if (ae::platform::android::readUnsignedLaunchOption(app->activity,
                                                       "aether.gpu_isolation",
                                                       requestedIsolation)) {
    shell.gpuCostIsolation = ae::renderer::sanitizeGpuCostIsolation(requestedIsolation);
  }
  shell.instancedRenderer.setGpuCostIsolation(shell.gpuCostIsolation);
  applyRuntimeControls(shell);

  // --- Política global de renderização (ADR-014) ---------------------------
  // As opções de lançamento são o override de diagnóstico do que, no produto,
  // virá das Project Settings serializadas. O vocabulário é o mesmo dos dois
  // lados porque o parsing mora na própria política, não aqui.
  {
    char buffer[64]{};
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_preset",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.preset = ae::renderer::parseQualityPreset(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_shadows",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.shadows = ae::renderer::parseShadowQuality(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_ambient",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.ambient = ae::renderer::parseAmbientQuality(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_post",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.post = ae::renderer::parsePostQuality(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_textures",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.textures = ae::renderer::parseTextureQuality(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.quality_water_mesh",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.waterMesh = ae::renderer::parseWaterMeshQuality(buffer);
    }
    if (ae::platform::android::readStringLaunchOption(app->activity, "aether.anti_aliasing",
                                                      buffer, sizeof(buffer))) {
      shell.renderingSettings.antiAliasing = ae::renderer::parseAntiAliasingMode(buffer);
    }
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.shadow_cascades",
                                                     shell.renderingSettings.shadowCascadeCount);
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.shadow_resolution",
                                                     shell.renderingSettings.shadowCascadeResolution);
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.shadow_filter_taps",
                                                     shell.renderingSettings.shadowFilterTaps);
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.shadow_far_filter_taps",
                                                     shell.renderingSettings.shadowFarFilterTaps);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_distance",
                                                  shell.renderingSettings.shadowMaximumDistance);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_bias_constant",
                                                  shell.renderingSettings.shadowDepthBiasConstant);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_bias_slope",
                                                  shell.renderingSettings.shadowDepthBiasSlope);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_normal_offset",
                                                  shell.renderingSettings.shadowNormalOffsetTexels);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_cache_guard_band",
                                                  shell.renderingSettings.shadowCacheGuardBandRatio);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_cascade_blend_ratio",
                                                  shell.renderingSettings.shadowCascadeBlendRatio);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.shadow_distance_fade_ratio",
                                                  shell.renderingSettings.shadowDistanceFadeRatio);
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.shadow_static_cache"))
      shell.renderingSettings.staticShadowCache = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.disable_shadow_static_cache"))
      shell.renderingSettings.staticShadowCache = ae::renderer::FeatureOverride::Disabled;
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.normal_map_distance",
                                                  shell.renderingSettings.normalMapMaximumDistance);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.specular_probe_distance",
                                                  shell.renderingSettings.specularProbeMaximumDistance);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.metallic_roughness_distance",
        shell.renderingSettings.metallicRoughnessMaximumDistance);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.emissive_distance",
                                                  shell.renderingSettings.emissiveMaximumDistance);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.material_detail_fade_ratio",
        shell.renderingSettings.materialDetailFadeBandRatio);
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.thermal_distance_scaling"))
      shell.renderingSettings.thermalDistanceScaling = ae::renderer::FeatureOverride::Enabled;
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.bloom_threshold",
                                                  shell.renderingSettings.bloomThreshold);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.bloom_intensity",
                                                  shell.renderingSettings.bloomIntensity);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.post_contrast",
                                                  shell.renderingSettings.postContrast);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.post_saturation",
                                                  shell.renderingSettings.postSaturation);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.post_sharpen",
                                                  shell.renderingSettings.postSharpen);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.taa_history_weight",
                                                  shell.renderingSettings.temporalHistoryWeight);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.lod_pixel_error_budget",
                                                  shell.renderingSettings.lodPixelErrorBudget);
    ae::platform::android::readFloatLaunchOption(
        app->activity, "aether.coverage_lod_pixel_error_budget",
        shell.renderingSettings.coverageLodPixelErrorBudget);
    ae::platform::android::readFloatLaunchOption(
        app->activity, "aether.lod_hysteresis_band_ratio",
        shell.renderingSettings.lodHysteresisBandRatio);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.resolution_scale",
                                                  shell.renderingSettings.resolutionScale);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.dynamic_resolution_min_scale",
                                                  shell.renderingSettings.dynamicResolutionMinimumScale);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.dynamic_resolution_down_step",
                                                  shell.renderingSettings.dynamicResolutionDecreaseStep);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.dynamic_resolution_up_step",
                                                  shell.renderingSettings.dynamicResolutionIncreaseStep);
    ae::platform::android::readFloatLaunchOption(app->activity, "aether.dynamic_resolution_headroom",
        shell.renderingSettings.dynamicResolutionRecoveryHeadroomRatio);
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.dynamic_resolution_overload_frames",
        shell.renderingSettings.dynamicResolutionOverloadFrames);
    ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.dynamic_resolution_recovery_frames",
        shell.renderingSettings.dynamicResolutionRecoveryFrames);
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.dynamic_resolution"))
      shell.renderingSettings.dynamicResolution = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.disable_dynamic_resolution"))
      shell.renderingSettings.dynamicResolution = ae::renderer::FeatureOverride::Disabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.post_fxaa"))
      shell.renderingSettings.postFxaa = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.post_vignette"))
      shell.renderingSettings.postVignette = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.lod_selection"))
      shell.renderingSettings.lodSelection = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.disable_lod_selection"))
      shell.renderingSettings.lodSelection = ae::renderer::FeatureOverride::Disabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.material_shader_variants"))
      shell.renderingSettings.materialShaderVariants = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(
            app->activity, "aether.disable_material_shader_variants"))
      shell.renderingSettings.materialShaderVariants = ae::renderer::FeatureOverride::Disabled;
    if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.environment_split_sum"))
      shell.renderingSettings.environmentSplitSumBrdf = ae::renderer::FeatureOverride::Enabled;
    if (ae::platform::android::readBooleanLaunchOption(
            app->activity, "aether.disable_environment_split_sum"))
      shell.renderingSettings.environmentSplitSumBrdf = ae::renderer::FeatureOverride::Disabled;
  }
  shell.instancedRenderer.setCoveragePrepassEnabled(
      !ae::platform::android::readBooleanLaunchOption(app->activity,
                                                       "aether.disable_coverage_prepass"));
  // HZB occlusion is a new, opt-in experiment (unlike coverage prepass above,
  // which is opt-out): default false until a physical A/B validates it. See
  // InstancedRenderer::setHzbOcclusionEnabled and
  // PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md.
  shell.instancedRenderer.setHzbOcclusionEnabled(
      ae::platform::android::readBooleanLaunchOption(app->activity, "aether.hzb_occlusion"));
  const bool hzbComputeValidation = ae::platform::android::readBooleanLaunchOption(
      app->activity, "aether.hzb_compute_validation");
  shell.instancedRenderer.setHzbComputeEnabled(
      hzbComputeValidation || ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.hzb_compute"));
  shell.instancedRenderer.setHzbComputeReadbackValidationEnabled(hzbComputeValidation);
  if (!shell.independentWorkspace) {
    shell.instancedRenderer.setWaterDisplacementCapacity((shell.oceanPreview || shell.editorUi)?128.0f:5.0f);
    shell.instancedRenderer.setSpectralWaterEnabled(shell.editorUi || ae::platform::android::readBooleanLaunchOption(
        app->activity,"aether.water_fft"));
    shell.instancedRenderer.setWideWaterSlopes(ae::platform::android::readBooleanLaunchOption(
        app->activity, "aether.water_slope_wide"));
  }
  ae::u32 requestedWaterIsolation = 0;
  if (!shell.independentWorkspace && ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.water_isolation",
                                                      requestedWaterIsolation)) {
    const auto mode = ae::renderer::sanitizeWaterCostIsolation(requestedWaterIsolation);
    shell.instancedRenderer.setWaterCostIsolation(mode);
    __android_log_print(ANDROID_LOG_INFO, LogTag, "[WaterIsolation] modo=%s (%u solicitado).",
                        ae::renderer::waterCostIsolationName(mode), requestedWaterIsolation);
  }
  // Consumidor GPU da piramide (ADR-016 C2). Implica o produtor compute: sem
  // ele nao ha piramide residente para consumir, e pedir culling sem produtor
  // seria uma opcao que nao faz nada em silencio.
  const bool hzbGpuCulling = ae::platform::android::readBooleanLaunchOption(
      app->activity, "aether.hzb_gpu_culling");
  if (hzbGpuCulling) shell.instancedRenderer.setHzbComputeEnabled(true);
  shell.instancedRenderer.setHzbGpuCullingEnabled(hzbGpuCulling);
  // Padrao ligado (ganho medido); a opcao existe para desligar num A/B.
  shell.instancedRenderer.setDepthOrderedBatchesEnabled(
      !ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.disable_depth_ordered_batches"));
  // Idem para os impostores de folhagem assados no pacote: o padrao usa o que
  // esta no asset, e a opcao devolve o pacote sem impostores no mesmo binario.
  shell.instancedRenderer.setFoliageImpostorsEnabled(
      !ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.disable_foliage_impostors"));
  ae::u32 requestedHysteresisFrames = shell.visibilityBudget.hzbHysteresisFrames;
  if (ae::platform::android::readUnsignedLaunchOption(app->activity, "aether.hzb_hysteresis_frames",
                                                       requestedHysteresisFrames)) {
    shell.visibilityBudget.hzbHysteresisFrames = requestedHysteresisFrames;
  }
  shell.instancedRenderer.setHzbHysteresisFrames(shell.visibilityBudget.hzbHysteresisFrames);
  ae::u32 hzbMinimumCandidateDraws = shell.visibilityBudget.hzbMinimumCandidateDraws;
  if (ae::platform::android::readUnsignedLaunchOption(
          app->activity, "aether.hzb_minimum_candidate_draws", hzbMinimumCandidateDraws)) {
    shell.visibilityBudget.hzbMinimumCandidateDraws = hzbMinimumCandidateDraws;
  }
  shell.instancedRenderer.setHzbMinimumCandidateDraws(
      shell.visibilityBudget.hzbMinimumCandidateDraws);
  float hzbNormalizedDepthBias = shell.visibilityBudget.hzbNormalizedDepthBias;
  if (ae::platform::android::readFloatLaunchOption(app->activity, "aether.hzb_depth_bias",
                                                    hzbNormalizedDepthBias)) {
    shell.visibilityBudget.hzbNormalizedDepthBias = hzbNormalizedDepthBias;
  }
  shell.instancedRenderer.setHzbNormalizedDepthBias(
      shell.visibilityBudget.hzbNormalizedDepthBias);
  // LOD overrides were parsed into ProjectRenderingSettings above. Applying
  // them only through the resolved policy avoids a second default path that
  // previously disabled policy-driven LOD whenever no explicit launch flag
  // was present.
  if (shell.frameProfiler.enabled()) {
    // A long benchmark must not time out into the keyguard. This window flag
    // only keeps an already-unlocked foreground window awake; it changes no
    // global timeout, cannot unlock a device and has no effect in background.
    ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    __android_log_print(ANDROID_LOG_INFO, LogTag, "[FrameProfile] KEEP_SCREEN_ON na janela de medição.");
  }

  ae::platform::android::applyImmersiveLandscapeWindow(app->activity);

  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo iniciado.");
  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "[FramePolicy] render=%u Hz (%.3f ms), fixed tick=%u Hz, CPU<=%.3f ms, GPU<=%.3f ms.",
                      shell.frameBudget.renderHz, static_cast<double>(shell.frameBudget.frameIntervalMs),
                      shell.frameBudget.simulationHz, static_cast<double>(shell.frameBudget.cpuLaneBudgetMs),
                      static_cast<double>(shell.frameBudget.gpuLaneBudgetMs));

  if (!shell.independentWorkspace) initializeDotNetHost(shell);
  shell.framePacer.initialize();
  shell.lastGameplayUpdate = std::chrono::steady_clock::now();
  shell.lastFpsPublishedAt = shell.lastGameplayUpdate;

  // Diagnóstico opt-in, fora da thread de eventos/render. Device e fila são
  // exclusivos do worker: não há vkQueueSubmit concorrente na fila do shell.
  std::future<void> astcProbe;
  std::future<void> waterProbe;
  if (!shell.independentWorkspace && ae::platform::android::readBooleanLaunchOption(app->activity, "aether.water_fft_probe")) {
    waterProbe = std::async(std::launch::async, [] {
      ae::rhi::VulkanDevice probeDevice;
      const bool passed=probeDevice.initialize("Aether water FFT diagnostic") &&
          ae::platform::android::runWaterSpectralProbe(probeDevice);
      __android_log_print(passed?ANDROID_LOG_INFO:ANDROID_LOG_ERROR,"Aether.WaterProbe",
          "[WaterFFT] complete passed=%s",passed?"true":"false");
    });
  }
  if (ae::platform::android::readBooleanLaunchOption(app->activity, "aether.astc_probe")) {
    astcProbe = std::async(std::launch::async, [] {
      const auto start = std::chrono::steady_clock::now();
      ae::rhi::VulkanDevice probeDevice;
      if (!probeDevice.initialize("Aether ASTC diagnostic")) {
        __android_log_print(ANDROID_LOG_ERROR, "Aether.AstcProbe", "[AstcProbe] {\"succeeded\":false,\"error\":\"device_initialization\"}");
        return;
      }
      const auto result = ae::platform::android::runAstcEncodeProbe(probeDevice, 4096, 4096);
      const double wallMs = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count();
      char gpuMs[64] = "null";
      if (result.timingValid) std::snprintf(gpuMs, sizeof(gpuMs), "%.6f", result.encodeMilliseconds);
      __android_log_print(ANDROID_LOG_INFO, "Aether.AstcProbe",
          "[AstcProbe] {\"schemaVersion\":1,\"succeeded\":%s,\"gpu_encode_ms\":%s,\"probe_wall_ms\":%.6f,"
          "\"tested_texels\":%llu,\"max_channel_difference\":%.1f,\"corpus\":\"opaque_blocks_ramps_checker\"}",
          result.succeeded ? "true" : "false", gpuMs, wallMs,
          static_cast<unsigned long long>(result.testedTexels), static_cast<double>(result.maxChannelDifference));
    });
  }

  while (!shell.lifecycle.isDestroyed()) {
    ae::platform::android::DotNetHost::NativeRegion nativeGc(shell.dotNetHost);
    // Item 5.2 do plano de lacunas: só sai do poll bloqueante (-1, custo
    // térmico zero em repouso — comportamento original preservado) quando
    // há de fato um frame para desenhar. Sem surface pronta ou app
    // suspenso, o loop continua bloqueando sem evento, exatamente como
    // antes deste item existir.
    const bool shouldDraw = shell.lifecycle.isActive() && shell.instancedRendererReady;
    android_poll_source *source = nullptr;
    int events = 0;
    const bool choreographerAdmission = shell.framePacer.available() &&
                                        !shell.vulkanSurface.swappyActive();
    const int pollTimeout = shell.lifecycle.isActive() && choreographerAdmission
                                ? -1
                                : (shouldDraw ? 0 : (shell.rendererInitialization.valid() ? 16 : -1));
    const int result = ALooper_pollOnce(pollTimeout, nullptr, &events,
                                        reinterpret_cast<void **>(&source));
    if (result >= 0 && source != nullptr) source->process(app, source);

    if (app->destroyRequested != 0) {
      // R1: o worker de reabertura para entre fontes; o future junta na saída
      // sem esperar o resto do projeto ser interpretado.
      if (shell.reopenCancellation) shell.reopenCancellation->store(true);
      applyEvent(shell, ae::platform::AppEvent::Destroy);
      continue;
    }
    collectRendererInitialization(shell,false);
    if(shell.windowResizePending && app->window && shell.vulkanSurface.isReady() &&
       !shell.rendererInitialization.valid() && std::chrono::steady_clock::now()>=shell.windowResizeAfter) {
      shell.windowResizePending=false;
      const auto width=ANativeWindow_getWidth(app->window),height=ANativeWindow_getHeight(app->window);
      const auto &swapchain=shell.vulkanSurface.swapchain();
      const auto display=swapchain.displayExtent();
      // Android reports window coordinates; Vulkan's raw extent may be
      // portrait even for a landscape window when preTransform is 90 degrees.
      if(width>0 && height>0 && (static_cast<ae::u32>(width)!=display.width ||
                               static_cast<ae::u32>(height)!=display.height)) {
        __android_log_print(ANDROID_LOG_INFO,LogTag,"[EditorSurface] resize settled: window=%dx%d display=%ux%u.",
                            width,height,display.width,display.height);
        if(!recreateSurfaceAndRenderer(shell))
          __android_log_print(ANDROID_LOG_ERROR,LogTag,"Falha ao reconciliar superfície após redimensionamento.");
      }
    }
    ae::platform::android::publishWaterProviderStatus(shell.instancedRendererReady?
        shell.instancedRenderer.waterProviderStatus():0);
    applyRuntimeControls(shell);
    if (shell.thermalMonitor.poll()) applyThermalRenderingPolicy(shell, false);

    // O evento processado acima pode ter destruído o renderer/janela.
    const bool frameAdmitted = !choreographerAdmission || shell.framePacer.consumeFrame();
    if (shell.lifecycle.isActive() && shell.instancedRendererReady && frameAdmitted) {
      const auto frameWorkStarted = std::chrono::steady_clock::now();
      const ae::u64 frameMonotonicStarted = currentMonotonicNanoseconds();
      const ae::u64 frameThreadCpuStarted = currentThreadCpuNanoseconds();
      if (shell.cameraRouteReplayActive) {
        // Determinism comes from indexing by frame ordinal, never by wall
        // clock -- see camera_route.h. Looping by modulo lets a short route
        // drive an arbitrarily long soak run.
        shell.cameraController.setState(
            shell.cameraRoutePlayer.sample(shell.cameraRouteFrameOrdinal));
        ++shell.cameraRouteFrameOrdinal;
      } else if (shell.firstPersonEnabled && !shell.editorUi) {
        auto camera = shell.cameraController.state();
        const float deltaSeconds = std::chrono::duration<float>(
            frameWorkStarted - shell.lastGameplayUpdate).count();
        const ae::platform::FirstPersonInput input=shell.firstPersonTouches.consumeInput();
        ae::platform::FirstPersonInput lookOnly=input;
        lookOnly.moveRight=0.0f;
        lookOnly.moveForward=0.0f;
        shell.firstPersonController.update(camera,lookOnly,deltaSeconds);
        if(shell.characterMotor.update(input.moveRight,input.moveForward,camera.yaw,deltaSeconds)){
          const AetherVec3 eye=shell.characterMotor.eyePosition();
          camera.position[0]=eye.x;camera.position[1]=eye.y;camera.position[2]=eye.z;
        }
        shell.cameraController.setState(camera);
      }
      shell.lastGameplayUpdate = frameWorkStarted;
      if (shell.sceneValidation && shell.sceneStep < 3 &&
          shell.presentedFrameCount >= SceneValidationIntervalFrames * static_cast<ae::u64>(shell.sceneStep + 1)) {
        if (shell.applySceneStep == nullptr || shell.applySceneStep(shell.sceneStep + 1) != 0) {
          __android_log_print(ANDROID_LOG_ERROR, LogTag, "[ScenePreview] Falha na mutação de validação.");
          shell.sceneValidation = false;
        } else {
          ++shell.sceneStep;
        }
      }
      float timeSeconds = std::chrono::duration<float>(
                                    std::chrono::steady_clock::now() - shell.shellStartTime)
                                    .count();
      // Em modo de edicao a cena fica congelada; a aba Play e o que a solta. O
      // relogio da cena vive na sessao, onde e testavel.
      const bool editorActive = shell.editorUi && shell.instancedRenderer.uiRendererReady();
      shell.editorSession.advanceClock(timeSeconds);
      const bool editorPlaying = !editorActive || shell.editorSession.isPlaying();
      if (editorActive) timeSeconds = shell.editorSession.sceneTime();
      if (editorActive && shell.editorWasPlaying && !editorPlaying) {
        shell.editorPublishedRevision=~ae::u64{0};
        shell.cameraController.cancelGesture();shell.firstPersonTouches.cancel();
      }
      shell.editorWasPlaying=editorPlaying;

      auto waterControls=ae::platform::android::runtimeControlsSnapshot();
      const auto *waterRoot=shell.editorSession.document().find(shell.editorSession.document().root());
      if(editorActive && waterRoot && ae::editor::waterSettings(*waterRoot).enabled)
        waterControls.fluidDensity=ae::editor::waterSettings(*waterRoot).density;
      const ae::renderer::WaterWakeSettings wakeSettings{
          waterControls.wakeStrength, waterControls.wakeMinimumSpeed,
          waterControls.wakeSpacing, waterControls.wakeWidthScale,
          waterControls.wakeMaximumImpulse};
      if (shell.oceanPreview && !editorActive && editorPlaying && !shell.oceanValidation.update(shell.instancedRenderer,timeSeconds,timeSeconds,
          waterControls.fluidDensity,waterControls.waterPaused || !editorPlaying,
          waterControls.bodyRippleGain, wakeSettings)) {
        __android_log_print(ANDROID_LOG_ERROR,LogTag,"[OceanValidation] simulation update failed");
      }
      shell.waterTimeSeconds = timeSeconds;
      ae::renderer::RuntimeHudState hud{};
      hud.visible = shell.firstPersonEnabled && !editorActive;
      if (shell.firstPersonEnabled) {
        const auto &joystick = shell.firstPersonTouches.joystickState();
        hud.joystickActive = joystick.active;
        hud.joystickCenterX = joystick.originX;
        hud.joystickCenterY = joystick.originY;
        hud.joystickKnobX = joystick.knobX;
        hud.joystickKnobY = joystick.knobY;
        hud.joystickRadiusPixels = joystick.radiusPixels;
        hud.framesPerSecond = shell.displayedFps;
      }
      if (shell.editorUi && shell.instancedRenderer.uiRendererReady()) {
        const VkExtent2D display = shell.vulkanSurface.swapchain().displayExtent();
        // Portrait authoring has phone-sized touch targets, independent of the
        // dense landscape scene panels. Input consumes this same scale.
        shell.editorScale=shell.editorSceneScale*
            (shell.editorSession.screen().workspace==ae::editor::EditorWorkspace::Code && display.height>display.width?1.4f:1.0f);
        const float logicalWidth = static_cast<float>(display.width) / shell.editorScale;
        const float logicalHeight = static_cast<float>(display.height) / shell.editorScale;
        shell.editorSession.setSurface({0.0f, 0.0f, logicalWidth, logicalHeight}, {});
        ae::platform::android::updateEditorTextInput(shell.editorSession);
        // O build sai sozinho quando a digitacao para. O relogio e o mesmo do
        // shell; a sessao so precisa de um instante que ande para frente.
        shell.editorSession.pumpCodeAutoBuild(
            std::chrono::duration<double>(std::chrono::steady_clock::now()-shell.shellStartTime).count());
        updateEditorCodeCompiler(shell);
        shell.editorSession.update();
        shell.instancedRenderer.setEnvironmentAdjustment(shell.editorSession.document().find(shell.editorSession.document().root())->environment);
        const float editorWallSeconds=std::chrono::duration<float>(std::chrono::steady_clock::now()-shell.shellStartTime).count();
        if(!shell.editorSession.requestedScenePath().empty() && !editorPlaying && !shell.editorSession.history().isOpen()) {
          const auto requested=shell.editorSession.requestedScenePath();
          shell.editorSession.clearSceneOpenRequest();
          if(requested!=shell.editorSavePath) {
          const bool saved=shell.editorSavePath.empty() || shell.editorSavedRevision==shell.editorSession.document().revision() ||
            shell.editorSession.save(shell.editorSavePath.c_str(),shell.editorPackageFingerprint);
          if(saved && shell.editorSession.load(requested.c_str(),shell.editorPackageFingerprint)) {
            shell.editorSavePath=requested;
            shell.editorSavedRevision=shell.editorSession.document().revision();
            shell.editorPublishedRevision=~ae::u64{0};
            shell.editorSession.frameAll();
          } else shell.editorSession.reportSceneOpenFailure();
          }
        }
        if(!shell.editorSavePath.empty() && !shell.editorSession.history().isOpen() &&
           (shell.editorSession.saveRequested() ||
            (shell.editorSavedRevision!=shell.editorSession.document().revision() &&
             editorWallSeconds-shell.editorLastSaveSeconds>=3.0f))) {
          shell.editorLastSaveSeconds=editorWallSeconds;
          if(shell.editorSession.save(shell.editorSavePath.c_str(),shell.editorPackageFingerprint))
            shell.editorSavedRevision=shell.editorSession.document().revision();
          else __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Falha ao salvar cena.");
          // O registro acompanha a cena: uma cena que referencia recursos e um
          // registro que não sabe deles abririam com objetos sem malha.
          if(shell.editorSession.assets().size() &&
             !writeProjectAssetRegistry(shell.editorProjectPath,shell.editorSession.serializeAssets()))
            __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Falha ao salvar o registro de recursos.");
        }
        // Renomear ou apagar um recurso mexeu no disco AGORA. O registro vai
        // junto, sem esperar o proximo salvar: no intervalo ele apontaria para
        // um caminho que nao existe mais, e o projeto reaberto ali abriria com
        // os objetos sem malha.
        if(shell.editorSession.assetRegistryDirty()) {
          if(writeProjectAssetRegistry(shell.editorProjectPath,shell.editorSession.serializeAssets()))
            shell.editorSession.clearAssetRegistryDirty();
          else
            __android_log_print(ANDROID_LOG_ERROR,LogTag,"[Editor] Falha ao gravar o registro de recursos.");
        }
        if(!editorPlaying && shell.authoredWaterPlay.active()) {shell.authoredWaterPlay.stop();shell.instancedRenderer.clearWaterRipples();shell.instancedRenderer.setWaterSimulationClock(-1);}
        // The worker owns bytes and parser state only. All scene/GPU/filesystem
        // publication happens below on the editor thread after explicit review.
        auto &session=shell.editorSession;
        if(shell.importPreview && (shell.importPreview->root!=session.codeProjectRoot() || shell.importPreview->epoch!=session.sceneVersion().epoch)) {
          shell.importPreview.reset();session.closeImportPreview();
        }
        if(session.takeImportCancel()) {
          ae::platform::android::cancelModelPick();
          if(shell.importCancellation) shell.importCancellation->store(true);
          shell.importPreview.reset();session.setImportStatus("Importação cancelada; projeto preservado.");
        }
        const auto launchImport=[&](ae::platform::android::ModelPickerResult picked,std::string path) {
          if(shell.importWork.valid() || shell.importPreview) {session.setImportStatus("Finalize a importação em andamento.");return;}
          if(shell.projectReopening) {session.setImportStatus("Aguarde os recursos do projeto terminarem de abrir.");return;}
          session.beginImportPreparation();
          shell.importCancellation=std::make_shared<std::atomic<bool>>(false);
          auto cancel=shell.importCancellation;
          const auto root=session.codeProjectRoot();const auto epoch=session.sceneVersion().epoch;
          const auto limits=session.importLimits();
          shell.importWork=std::async(std::launch::async,[picked=std::move(picked),path=std::move(path),root,epoch,cancel,limits]() mutable {
            AndroidShell::PreparedModel result;result.root=root;result.path=path;result.epoch=epoch;
            {
              std::filesystem::path absolute;
              if(!ae::editor::EditorImportTransaction::safePath(ae::editor::EditorImportTransaction::fromUtf8(root),path,absolute)) {
                result.diagnostic="Destino fora do projeto.";return result;
              }
              std::error_code error;
              const bool exists=std::filesystem::exists(absolute,error);
              std::vector<ae::u8> previous;
              if(error || (exists && !ae::editor::EditorImportTransaction::read(absolute,previous))) {
                result.diagnostic="Não foi possível ler a fonte anterior.";return result;
              }
              result.expectedHash=exists?ae::Sha256::hex(previous):std::string();
              result.bytes=picked.accepted?std::move(picked.bytes):std::move(previous);
              // .gltf, ou GLB com URI externa: as dependências vêm dos arquivos
              // escolhidos junto e entram num GLB autocontido, que é o que o
              // projeto guarda. Reabrir não depende da pasta nem da permissão do seletor.
              if(ae::resources::gltfNeedsPackage(result.bytes)) {
                std::vector<ae::resources::GltfPackageFile> files;
                for(const auto &companion:picked.companions) files.push_back({companion.name,companion.bytes});
                ae::resources::GltfPackage package;
                if(!ae::resources::packGltf(result.bytes,files,256ull<<20,package,result.diagnostic)) return result;
                result.manifest=ae::resources::serializeGltfManifest(picked.displayName,result.bytes,package);
                result.dependencies=static_cast<ae::u32>(package.dependencies.size());
                result.unusedCompanions=package.unusedFiles;
                result.bytes=std::move(package.glb);
              }
              ae::resources::GltfImportProgress progress{};
              progress.context=cancel.get();
              progress.cancelled=[](void *context) {return static_cast<std::atomic<bool> *>(context)->load();};
              result.accepted=ae::resources::importGlb(result.bytes,limits,progress,result.model);
              // Hash no worker: a revisão compara com o mapa publicado sem
              // percorrer dezenas de MiB na thread do editor.
              if(result.accepted) result.contentHash=ae::Sha256::hex(result.bytes);
              result.diagnostic=result.model.diagnostic;
            }
            return result;
          });
        };
        if(session.consumeModelImportRequest()) {
          if(!shell.importWork.valid() && !shell.importPreview) {
            shell.importPickerRoot=session.codeProjectRoot();shell.importPickerEpoch=session.sceneVersion().epoch;
            session.beginImportPreparation();
            ae::platform::android::requestModelPick();
          }
        }
        if(auto path=session.takeReimportPath();!path.empty()) launchImport({},std::move(path));
        if(ae::platform::android::ModelPickerResult picked;ae::platform::android::takeModelPickResult(picked)) {
          if(shell.importPickerRoot!=session.codeProjectRoot() || shell.importPickerEpoch!=session.sceneVersion().epoch)
            {session.closeImportPreview();session.setImportStatus("Seleção descartada: o projeto ou a cena mudou.");}
          else if(!picked.accepted) {
            if(picked.diagnostic.empty()) {session.closeImportPreview();session.setImportStatus("Importação cancelada");}
            else session.showImportFailure(picked.diagnostic);
          }
          else {
            std::string name=picked.displayName.empty()?"modelo.glb":picked.displayName;
            for(auto &character:name) if(character=='/' || character=='\\' || character==':' || static_cast<unsigned char>(character)<32) character='_';
            if(name=="." || name=="..") name="modelo.glb";
            // Um .gltf é guardado empacotado: o projeto recebe cena.glb, não cena.gltf.
            if(name.size()>5 && name.ends_with(".gltf")) name.replace(name.size()-5,5,".glb");
            if(!name.ends_with(".glb")) name+=".glb";
            launchImport(std::move(picked),"Fontes/"+name);
          }
        }
        // R1: etapa real da abertura na barra de estado enquanto o worker trabalha.
        if(shell.projectReopening && shell.reopenProgress) {
          std::string text;
          {
            std::lock_guard<std::mutex> hold(shell.reopenProgress->lock);
            const auto &p=*shell.reopenProgress;
            text="Abrindo projeto · recurso "+std::to_string(std::min(p.done+1,p.total))+" de "+std::to_string(p.total);
            if(!p.current.empty()) text+=" · "+p.stage+" "+p.current;
          }
          shell.editorSession.setWorkStatus(text);
        }
        // R1: fontes do projeto prontas no worker -> uma publicação e a cena salva.
        if(shell.projectReopening && shell.reopenWork.valid() &&
           shell.reopenWork.wait_for(std::chrono::seconds(0))==std::future_status::ready)
          finishProjectReopen(shell);
        if(shell.importWork.valid() && shell.importWork.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
          auto prepared=shell.importWork.get();
          if(shell.importCancellation->load() || prepared.root!=session.codeProjectRoot() || prepared.epoch!=session.sceneVersion().epoch) {
            session.closeImportPreview();session.setImportStatus("Preparação descartada; projeto preservado.");
          } else if(!prepared.accepted) {
            session.showImportFailure(prepared.diagnostic.empty()?"O importador não conseguiu preparar este arquivo.":prepared.diagnostic);
          } else {
            session.showImportPreview(prepared.path,prepared.model,prepared.contentHash);
            if(prepared.dependencies)
              session.noteImportPreview("Dependências copiadas para o projeto: "+std::to_string(prepared.dependencies)+" arquivo(s)"+
                (prepared.unusedCompanions?"; "+std::to_string(prepared.unusedCompanions)+" escolhido(s) sem uso":std::string())+
                ". Reabre sem a pasta original.");
            shell.importPreview=std::move(prepared);
          }
        }
        if(session.takeImportAccept() && shell.importPreview) {
          const bool intoScene=session.takeImportIntoScene();
          session.closeImportPreview();
          auto prepared=std::move(*shell.importPreview);shell.importPreview.reset();
          ae::editor::EditorSession::ModelImportReport report;
          if(prepared.root!=session.codeProjectRoot() || prepared.epoch!=session.sceneVersion().epoch)
            session.setImportStatus("Publicação descartada: o projeto ou a cena mudou.",ae::editor::EditorConsoleSeverity::Warning);
          else if(!session.commitModelImport(prepared.bytes,prepared.model,prepared.path,prepared.expectedHash,report,session.importAmbiguityPolicy()))
            session.showImportFailure(report.diagnostic);
          else {
            // Manifesto ao lado da fonte, só depois da fonte gravada: descreve de
            // onde veio cada byte do GLB empacotado. Falhar aqui não desfaz a importação.
            if(!prepared.manifest.empty()) {
              std::filesystem::path manifestPath;
              if(!ae::editor::EditorImportTransaction::safePath(ae::editor::EditorImportTransaction::fromUtf8(prepared.root),prepared.path+".deps",manifestPath) ||
                 !ae::editor::EditorImportTransaction::writeText(manifestPath,prepared.manifest))
                __android_log_print(ANDROID_LOG_WARN,LogTag,"[Import] manifesto de dependências não gravado: %s.deps",prepared.path.c_str());
              else
                __android_log_print(ANDROID_LOG_INFO,LogTag,"[Import] manifesto de dependências: %s.deps (%u arquivos)",prepared.path.c_str(),prepared.dependencies);
            }
            std::string message=report.reimported?"Recurso reimportado; instâncias locais preservadas.":"Recurso registrado. Use Instanciar em Arquivos para adicioná-lo à cena.";
            bool instanceFailed=false;
            if(intoScene) {
              ae::editor::EditorSession::ModelImportReport instance;
              if(session.instantiateModel(report.source,instance)) message="Modelo importado na cena: "+std::to_string(instance.objects)+" objetos. Seleção e câmera enquadradas.";
              else {session.showImportFailure("Recurso guardado; instanciação falhou: "+instance.diagnostic);instanceFailed=true;}
            }
            if(report.skippedTextures || report.skippedAnimations || report.skippedSkins)
              message+=" Há omissões do perfil (texturas não aplicadas, animações ou skins); detalhes na preparação.";
            if(!instanceFailed) session.setImportStatus(message,(report.skippedTextures || report.skippedAnimations || report.skippedSkins)?
                ae::editor::EditorConsoleSeverity::Warning:ae::editor::EditorConsoleSeverity::Info);
          }
        }
        // Material compartilhado editado: a cena não mudou de revisão, mas a
        // aparência de todos os slots que o usam mudou.
        if (shell.editorSession.takeAppearanceChanged()) shell.editorPublishedRevision=~ae::u64{0};
        if (shell.editorMapImported && (editorPlaying || shell.editorPublishedRevision != shell.editorSession.document().revision())) {
          auto &authored=shell.authoredDraws;
          const bool changed=shell.editorPublishedRevision!=shell.editorSession.document().revision();
          bool ready=!changed || shell.editorSession.extractMap(authored);
          if(editorPlaying && shell.independentWorkspace)
            ready=shell.editorSession.extractPlayMap(authored);
          if(changed && shell.authoredWaterPlay.active()) shell.authoredWaterPlay.stop();
          if(ready && editorPlaying && !shell.independentWorkspace) {
            if(!shell.authoredWaterPlay.active()) {
              ready=shell.authoredWaterPlay.start(shell.editorSession.document(),authored,
                shell.instancedRenderer.waterQuerySetup(),shell.instancedRenderer.activeWaterCascades(),
                shell.instancedRenderer.waterSpectralControls(),waterControls.fluidDensity);
              if(ready) __android_log_print(ANDROID_LOG_INFO,LogTag,"[WaterPlay] volumes=%u bodies=%u fixedStep=60Hz",shell.authoredWaterPlay.volumeCount(),shell.authoredWaterPlay.bodyCount());
            }
            if(ready) ready=shell.authoredWaterPlay.update(timeSeconds,authored) && shell.instancedRenderer.setWaterRipples(shell.authoredWaterPlay.ripples());
            if(ready) shell.instancedRenderer.setWaterSimulationClock(shell.authoredWaterPlay.simulationTime());
          }
          if (ready && (changed?shell.instancedRenderer.queueMapScene(authored):shell.instancedRenderer.queueAuthoredPoses(authored))) {
            shell.editorPublishedRevision = shell.editorSession.document().revision();
            // As luzes seguem o mesmo quadro dos desenhos. Republicar sempre é
            // barato (são poucas) e evita um segundo conceito de "sujo" para um
            // estado que muda por script no meio do Play.
            if(shell.editorSession.extractLights(shell.authoredLights))
              shell.instancedRenderer.queueSceneLights(shell.authoredLights);
            // O relatório é o do último quadro desenhado: quem escolhe as luzes
            // é o quadro, com a câmera dele. Um excedente aparece no aviso
            // seguinte, nunca some.
            const auto &budget=shell.instancedRenderer.lightBudget();
            if(!budget.complete() && budget.punctualDropped+budget.directionalDropped!=shell.editorReportedLightOverflow) {
              shell.editorReportedLightOverflow=budget.punctualDropped+budget.directionalDropped;
              __android_log_print(ANDROID_LOG_WARN,LogTag,
                "[Editor] Orcamento de luzes: %u pontuais acesas, %u fora; %u direcional, %u fora.",
                budget.punctualAccepted,budget.punctualDropped,budget.directionalAccepted,budget.directionalDropped);
            } else if(budget.complete()) shell.editorReportedLightOverflow=0;
          }
          else {
            __android_log_print(ANDROID_LOG_ERROR, LogTag, "[Editor] Falha ao publicar documento no renderer.");
            if(editorPlaying) shell.editorSession.reportPlayFailure();
          }
        }
        // A grade é publicada como plano para o renderer desenhar dentro da
        // cena. Antes ela era uma lista de segmentos na interface, por cima de
        // tudo; agora ela testa profundidade como qualquer outro desenho.
        shell.instancedRenderer.setEditorGrid(shell.editorSession.gridPlan());
        shell.instancedRenderer.setUiInstances(shell.editorSession.instances());
        // A mesma escala vai ao renderer: e ela que o vertex shader usa para
        // levar as coordenadas logicas ao NDC da tela inteira.
        shell.instancedRenderer.setUiSurfaceSize(logicalWidth, logicalHeight);
        const auto &rect = shell.editorSession.layout().viewport;
        shell.instancedRenderer.setSceneViewport({rect.x/logicalWidth, rect.y/logicalHeight,
                                                  rect.width/logicalWidth, rect.height/logicalHeight});
      }
      // A CENA e desenhada com a camera do EDITOR. Ate aqui ela usava a camera
      // livre do jogo e a de orbita movia so a grade e o gizmo: orbitar girava a
      // sobreposicao sobre uma cena parada em outro lugar, que e exatamente a
      // sensacao de "isto e uma cena rodando, nao um editor".
      ae::platform::FreeCameraState sceneCamera = shell.cameraController.state();
      shell.instancedRenderer.setSceneClipPlanes(0, 0);
      shell.instancedRenderer.setSceneFieldOfView(0);
      shell.instancedRenderer.setEditorBackground(editorActive && !editorPlaying);
      if (editorActive) {
        const auto &projection = shell.editorSession.view().frustum;
        shell.instancedRenderer.setSceneClipPlanes(projection.nearPlane, projection.farPlane);
        shell.instancedRenderer.setSceneFieldOfView(2*std::atan(projection.tangentHalfVertical));
        const ae::editor::EditorCamera &editorCamera = shell.editorSession.camera();
        ae::editor::editorCameraPosition(editorCamera, sceneCamera.position);
        sceneCamera.yaw = editorCamera.yaw;
        sceneCamera.pitch = editorCamera.pitch;
        if(editorPlaying) {
          const auto authoredCamera=shell.editorSession.sceneCameraPose();
          if(authoredCamera.entity) {
            std::copy(authoredCamera.position,authoredCamera.position+3,sceneCamera.position);
            sceneCamera.yaw=authoredCamera.yaw;sceneCamera.pitch=authoredCamera.pitch;
            shell.instancedRenderer.setSceneClipPlanes(authoredCamera.nearPlane,authoredCamera.farPlane);
            shell.instancedRenderer.setSceneFieldOfView(authoredCamera.verticalFov*0.017453292519943295f);
          }
        }
      }
      const ae::rhi::SwapchainStatus frameStatus = shell.instancedRenderer.drawFrame(
          timeSeconds, sceneCamera, hud);
      const ae::u64 frameThreadCpuFinished = currentThreadCpuNanoseconds();
      const ae::u64 frameMonotonicFinished = currentMonotonicNanoseconds();
      const ae::u64 threadCpuNs = frameThreadCpuFinished > frameThreadCpuStarted
                                      ? frameThreadCpuFinished - frameThreadCpuStarted : 0;
      const ae::u64 totalNs = frameMonotonicFinished > frameMonotonicStarted
                                  ? frameMonotonicFinished - frameMonotonicStarted : 0;
      if (shell.performance.detailedWorkDurationAvailable()) {
        // O timer Vulkan devolve aqui a GPU do frame anterior, depois da fence.
        // Pareamos com CPU/start guardados daquele mesmo ciclo; nunca atribuímos
        // o timestamp atrasado ao frame atual.
        const double gpuMs = shell.instancedRenderer.lastFrameTimings().gpuFrameMs;
        const ae::u64 gpuNs = std::isfinite(gpuMs) && gpuMs > 0.0
                                  ? static_cast<ae::u64>(gpuMs * 1'000'000.0) : 0;
        if (shell.pendingAdpfFrame.valid && gpuNs > 0) {
          shell.performance.reportFrameWorkDuration(
              static_cast<ae::i64>(shell.pendingAdpfFrame.workStartNs),
              ae::performanceHintActualTotalNanoseconds(
                  static_cast<ae::i64>(shell.pendingAdpfFrame.threadCpuNs),
                  static_cast<ae::i64>(gpuNs)),
              static_cast<ae::i64>(shell.pendingAdpfFrame.threadCpuNs),
              static_cast<ae::i64>(gpuNs));
        }
        shell.pendingAdpfFrame = {frameMonotonicStarted, totalNs, threadCpuNs,
                                  frameMonotonicStarted > 0 && totalNs > 0 && threadCpuNs > 0};
      } else if (threadCpuNs > 0) {
        shell.performance.reportThreadWorkDuration(static_cast<ae::i64>(threadCpuNs));
      }
      if (frameStatus == ae::rhi::SwapchainStatus::Ok ||
          frameStatus == ae::rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
        const auto presentedAt = std::chrono::steady_clock::now();
        if (shell.lastPresentedAt != std::chrono::steady_clock::time_point{}) {
          const float interval = std::chrono::duration<float>(presentedAt-shell.lastPresentedAt).count();
          if (interval > 0.0f && interval < 0.25f) {
            const float instantaneous = 1.0f / interval;
            shell.smoothedFps = shell.smoothedFps > 0.0f
                                    ? shell.smoothedFps*.90f+instantaneous*.10f
                                    : instantaneous;
          }
        }
        shell.lastPresentedAt = presentedAt;
        if (presentedAt-shell.lastFpsPublishedAt >= std::chrono::milliseconds(250)) {
          shell.displayedFps = static_cast<ae::u32>(std::clamp(
              std::lround(shell.smoothedFps),0l,
              static_cast<long>(std::min(shell.frameBudget.renderHz,999u))));
          shell.lastFpsPublishedAt = presentedAt;
        }
        ++shell.presentedFrameCount;
        if (shell.cameraRouteMode == ae::platform::CameraRouteMode::Record) {
          shell.cameraRouteRecorder.pushSample(shell.cameraController.state());
        }
        if (shell.scenePreview && shell.instancedRenderer.lastExtractionStatus() == 0 &&
            (shell.sceneReportedStep != shell.sceneStep || shell.firstFrameAfterActivationPending)) {
          __android_log_print(ANDROID_LOG_INFO, LogTag,
              "[ScenePreview] snapshot instances=%u hash=%llu yaw=%.6f pitch=%.6f",
              shell.instancedRenderer.drawnInstanceCount(),
              static_cast<unsigned long long>(shell.instancedRenderer.snapshotFingerprint()),
              shell.cameraController.state().yaw, shell.cameraController.state().pitch);
        }
        if (shell.scenePreview && shell.sceneReportedStep != shell.sceneStep &&
            shell.instancedRenderer.lastExtractionStatus() == 0) {
          shell.sceneReportedStep = shell.sceneStep;
          __android_log_print(ANDROID_LOG_INFO, LogTag,
              "[ScenePreview] presented step=%d instances=%u abi=1 stride=88",
              shell.sceneStep, shell.instancedRenderer.drawnInstanceCount());
        }
        if (shell.instancedRenderer.lastFillMicroseconds() > shell.pocAMaxFillMicroseconds) {
          shell.pocAMaxFillMicroseconds = shell.instancedRenderer.lastFillMicroseconds();
        }
        if (shell.firstFrameAfterActivationPending) {
          shell.firstFrameAfterActivationPending = false;
          __android_log_print(ANDROID_LOG_INFO, LogTag,
                              "Primeiro frame após ativação apresentado: ciclo=%llu. latency_ms=%.3f uptime_ms=%.3f",
                              static_cast<unsigned long long>(shell.activationCount),
                              ae::platform::android::lifecycleUptimeMs() - shell.activatedAtMs,
                              ae::platform::android::lifecycleUptimeMs());
        }
        if (!shell.validationFrameMilestoneLogged &&
            shell.presentedFrameCount >= ValidationFrameMilestone) {
          shell.validationFrameMilestoneLogged = true;
          __android_log_print(ANDROID_LOG_INFO, LogTag,
                              "Marco de renderização atingido: %llu frames apresentados.",
                              static_cast<unsigned long long>(shell.presentedFrameCount));
        }
        // PoC-A (item 0.2): reporta o pico de custo do crossing C++→C# a
        // cada ~5s, não a cada frame — o objetivo é ter evidência legível
        // no log contra o critério de < 3 ms de CPU, sem inundar o logcat.
        if (shell.pocABenchmark && shell.presentedFrameCount % PocAReportIntervalFrames == 0) {
          __android_log_print(
              ANDROID_LOG_INFO, LogTag,
              "PoC-A: %u instâncias, crossing C++<->C# pico=%.1f us (orçamento: 3000 us) — %s.",
              PocAInstanceCount, shell.pocAMaxFillMicroseconds,
              shell.pocAMaxFillMicroseconds < 3000.0 ? "dentro do orçamento" : "ACIMA do orçamento");
          shell.pocAMaxFillMicroseconds = 0.0;
        }
      }
      if (frameStatus == ae::rhi::SwapchainStatus::Ok) {
        const VkExtent2D display = shell.vulkanSurface.swapchain().displayExtent();
        const auto &camera = shell.cameraController.state();
        ae::platform::android::FrameProfileContext context{};
        context.sceneId = profileSceneId(shell);
        context.contentFingerprint = shell.instancedRenderer.contentFingerprint();
        context.targetFps = shell.frameBudget.renderHz;
        context.frameBudget = shell.frameBudget;
        context.adpfAvailable = shell.performance.hintSessionAvailable();
        context.adpfGpuWorkAvailable =
            shell.performance.detailedWorkDurationAvailable();
        context.gameMode = shell.performance.gameMode();
        context.sustainedPerformanceSupported =
            shell.performance.sustainedPerformanceSupported();
        context.sustainedPerformanceEnabled =
            shell.performance.sustainedPerformanceEnabled();
        const auto &thermal = shell.thermalMonitor.state();
        context.thermalApiAvailable = thermal.apiAvailable;
        context.thermalHeadroomValid = thermal.headroomValid;
        context.thermalHeadroom = thermal.headroom;
        context.thermalStatus = thermal.status;
        context.thermalPressure =
            ae::renderer::thermalPressureName(thermal.pressure);
        context.gpuIsolation = ae::renderer::gpuCostIsolationName(shell.gpuCostIsolation);
        context.cameraLocked = shell.lockCamera;
        context.cameraMode = shell.cameraRouteReplayActive ? "route" : (shell.lockCamera ? "locked" : "free");
        if (shell.cameraRouteReplayActive) {
          context.cameraRouteFingerprint = shell.cameraRoutePlayer.data().sceneFingerprint;
          // frameOrdinal was already advanced past the sample used for this
          // frame; report the sample actually consumed, not the next one.
          context.cameraRouteFrameOrdinal = shell.cameraRouteFrameOrdinal - 1;
          context.cameraRouteTickCount = shell.cameraRoutePlayer.tickCount();
        }
        std::copy(camera.position, camera.position + 3, context.cameraPosition);
        context.cameraYaw = camera.yaw;
        context.cameraPitch = camera.pitch;
        context.drawCount = shell.instancedRenderer.profileDrawCount();
        context.materialCount = shell.instancedRenderer.profileMaterialCount();
        context.textureCount = shell.instancedRenderer.profileTextureCount();
        context.triangleCount = shell.instancedRenderer.profileTriangleCount();
        context.packageVersion = shell.instancedRenderer.profilePackageVersion();
        context.renderDrawCount = shell.instancedRenderer.profileRenderDrawCount();
        context.lodGroupCount = shell.instancedRenderer.profileLodGroupCount();
        context.hzbEnabled = shell.instancedRenderer.hzbOcclusionEnabled();
        context.lodEnabled = shell.instancedRenderer.lodSelectionEnabled();
        context.lodPixelErrorBudget = shell.instancedRenderer.lodPixelErrorBudget();
        context.coverageLodPixelErrorBudget =
            shell.instancedRenderer.coverageLodPixelErrorBudget();
        context.renderScale = shell.instancedRenderer.currentRenderScale();
        context.renderWidth = shell.instancedRenderer.currentRenderWidth();
        context.renderHeight = shell.instancedRenderer.currentRenderHeight();
        const auto &visibility = shell.instancedRenderer.visibilityTelemetry();
        context.visibleDrawCount = visibility.visibleDraws;
        context.culledDrawCount = visibility.culledDraws;
        context.submittedDrawCallCount = visibility.submittedDrawCalls;
        context.visibleTriangleCount = visibility.visibleTriangles;
        context.submittedTriangleCount = visibility.submittedTriangles;
        context.hzbTestedDrawCount = visibility.hzbTestedDraws;
        context.hzbOccludedDrawCount = visibility.hzbOccludedDraws;
        context.hzbRevivedDrawCount = visibility.hzbRevivedDraws;
        context.hzbSkippedCameraMotionDrawCount = visibility.hzbSkippedCameraMotionDraws;
        context.hzbSkippedBudgetDrawCount = visibility.hzbSkippedBudgetDraws;
        context.deviceMemory = shell.instancedRenderer.deviceMemorySnapshot();
        shell.frameProfiler.record(shell.instancedRenderer.lastFrameTimings(), context,
                                    shell.instancedRenderer.drawnInstanceCount(), display.width, display.height);
      } else {
        shell.frameProfiler.reset();
      }
      if (frameStatus == ae::rhi::SwapchainStatus::SuboptimalNeedsRecreate ||
          frameStatus == ae::rhi::SwapchainStatus::OutOfDateMustRecreate) {
        if (!recreateSwapchainAndRenderer(shell)) {
          __android_log_print(ANDROID_LOG_ERROR, LogTag,
                              "Falha ao recriar swapchain após acquire/present.");
        }
      } else if (frameStatus == ae::rhi::SwapchainStatus::SurfaceLost) {
        if (!recreateSurfaceAndRenderer(shell)) {
          __android_log_print(ANDROID_LOG_ERROR, LogTag,
                              "Surface Vulkan perdida e recuperação imediata falhou.");
        }
      } else if (frameStatus == ae::rhi::SwapchainStatus::FatalError) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
                            "Erro Vulkan fatal no frame; recursos gráficos serão encerrados.");
        shell.instancedRenderer.shutdown();
        shell.instancedRendererReady = false;
        shell.vulkanSurface.shutdown();
      }
    }
  }

  {
    ae::platform::android::DotNetHost::NativeRegion nativeGc(shell.dotNetHost);
    if (shell.languageCancel) shell.languageCancel();
    if (shell.languageWork.valid()) shell.languageWork.wait();
    if (shell.codeCompilation.valid()) shell.codeCompilation.wait();
    collectRendererInitialization(shell,true);
  }
  shell.characterMotor.shutdown();
  shell.performance.setActive(false, false);
  shell.thermalMonitor.shutdown();
  shell.performance.shutdown();
  if (shell.shutdownScene != nullptr) shell.shutdownScene();
  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo encerrado.");
}
