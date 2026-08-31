#include "platform/android/android_paths.h"
#include "platform/android/android_launch_options.h"
#include "platform/android/astc_encode_probe.h"
#include "platform/android/android_frame_profiler.h"
#include "platform/android/android_frame_pacer.h"
#include "platform/android/android_performance.h"
#include "platform/android/android_vulkan_surface.h"
#include "platform/android/android_window.h"
#include "platform/android/dotnet_assets.h"
#include "platform/android/dotnet_host.h"
#include "platform/android/instanced_renderer.h"
#include "platform/android/lifecycle_trace.h"
#include "platform/app_lifecycle.h"
#include "platform/camera_route.h"
#include "platform/first_person_controller.h"
#include "platform/free_camera_controller.h"
#include "core/frame_policy.h"
#include "renderer/gpu_cost_isolation.h"
#include "physics/character_motor.h"

#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <future>
#include <atomic>

namespace {

constexpr const char *LogTag = "Aether.Android";
constexpr ae::u64 ValidationFrameMilestone = 1000;
// PoC-A (item 0.2 do plano): 5.000 objetos é o número que o critério de
// sucesso pede ("5.000 objetos renderizados a 60 fps com < 3 ms de CPU").
constexpr ae::u32 PocAInstanceCount = 5000;
constexpr ae::u32 ScenePreviewCapacity = 256;
constexpr ae::u64 SceneValidationIntervalFrames = 180;
constexpr ae::u64 PocAReportIntervalFrames = 300; // ~5s a 60fps — log periódico, não por frame

struct AndroidShell final {
  android_app *app = nullptr;
  ae::platform::AppLifecycle lifecycle;
  ae::platform::android::AndroidFrameProfiler frameProfiler;
  ae::platform::android::AndroidFramePacer framePacer;
  ae::platform::android::AndroidPerformance performance;
  ae::platform::android::AndroidVulkanSurface vulkanSurface;
  ae::platform::android::InstancedRenderer instancedRenderer;
  bool instancedRendererReady = false;
  bool forceDescriptorFallback = false;
  bool pocABenchmark = false;
  bool scenePreview = false;
  bool materialPreview = false;
  bool dirtRoadPreview = false;
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
  std::chrono::steady_clock::time_point shellStartTime = std::chrono::steady_clock::now();
  double pocAMaxFillMicroseconds = 0.0;
  ae::platform::FreeCameraController cameraController;
  ae::platform::FirstPersonController firstPersonController;
  ae::platform::FirstPersonTouchControls firstPersonTouches;
  ae::physics::CharacterMotor characterMotor;
  bool firstPersonEnabled = false;
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
  int displayRotation = -1;

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
void collectRendererInitialization(AndroidShell &shell, bool cancel) {
  if (!shell.rendererInitialization.valid()) return;
  if (cancel) shell.cancelRendererInitialization.store(true);
  else if (shell.rendererInitialization.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
  const bool ready=shell.rendererInitialization.get();
  shell.instancedRendererReady=ready && !cancel;
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
  if (!shell.instancedRendererReady) shell.instancedRenderer.shutdown();
  if (shell.instancedRendererReady && shell.lifecycle.isActive())
    shell.performance.setActive(true, false);
  __android_log_print(cancel ? ANDROID_LOG_INFO : (ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR),
      LogTag,"[%s] initialization=%s",shell.dirtRoadPreview?"DirtRoad":"MaterialPreview",
      cancel?"cancelled":(ready?"ready":"failed"));
}

bool rebuildInstancedRenderer(AndroidShell &shell) {
  collectRendererInitialization(shell,true);
  shell.frameProfiler.reset();
  ae::platform::android::ScopedLifecycleStage trace("rebuild-renderer");
  shell.instancedRenderer.shutdown();
  if (!shell.dotNetHost.isReady() && !shell.dirtRoadPreview) {
    shell.instancedRendererReady = false;
    return false;
  }
  shell.instancedRendererReady = false;
  if (shell.materialPreview || shell.dirtRoadPreview) {
    shell.cancelRendererInitialization.store(false);
    shell.rendererInitialization=std::async(std::launch::async,[&shell] {
      return shell.instancedRenderer.initialize(shell.vulkanSurface.device(),shell.vulkanSurface.swapchain(),
          shell.dotNetHost,ScenePreviewCapacity,!shell.dirtRoadPreview,shell.app->activity->assetManager,
          shell.forceTextureFallback,&shell.cancelRendererInitialization,shell.dirtRoadPreview);
    });
    __android_log_print(ANDROID_LOG_INFO,LogTag,"[%s] initialization=loading",
                        shell.dirtRoadPreview?"DirtRoad":"MaterialPreview");
    return true;
  }
  shell.instancedRendererReady = shell.instancedRenderer.initialize(
      shell.vulkanSurface.device(), shell.vulkanSurface.swapchain(), shell.dotNetHost,
      shell.scenePreview ? ScenePreviewCapacity : PocAInstanceCount, shell.scenePreview,
      shell.materialPreview ? shell.app->activity->assetManager : nullptr, shell.forceTextureFallback);
  if (!shell.instancedRendererReady) shell.instancedRenderer.shutdown();
  return shell.instancedRendererReady;
}

bool recreateSwapchainAndRenderer(AndroidShell &shell) {
  collectRendererInitialization(shell,true);
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
  if (shell.app->window == nullptr || !shell.vulkanSurface.initialize(shell.app->window, !shell.forceDescriptorFallback)) {
    return false;
  }
  return rebuildInstancedRenderer(shell);
}

void applyEvent(AndroidShell &shell, ae::platform::AppEvent event) {
  const ae::platform::LifecycleAction action = shell.lifecycle.apply(event);
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::CreateSurface)) {
    ae::platform::android::ScopedLifecycleStage trace("create-surface-renderer");
    ae::platform::android::requestRenderFrameRate(shell.app->window,
                                                   static_cast<float>(shell.frameBudget.renderHz));
    if (!shell.vulkanSurface.initialize(shell.app->window, !shell.forceDescriptorFallback)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "O shell continuará ativo sem GPU; uma nova janela tentará novamente.");
    } else if (!rebuildInstancedRenderer(shell)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "Surface pronta, mas o pipeline de desenho falhou ao inicializar.");
    }
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::DestroySurface)) {
    collectRendererInitialization(shell,true);
    flushCameraRouteRecordingIfNeeded(shell);
    ae::platform::android::ScopedLifecycleStage trace("destroy-surface-renderer");
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
    applyEvent(shell, ae::platform::AppEvent::WindowDestroyed);
    break;
  case APP_CMD_RESUME:
    applyEvent(shell, ae::platform::AppEvent::Resume);
    break;
  case APP_CMD_PAUSE:
    applyEvent(shell, ae::platform::AppEvent::Pause);
    break;
  case APP_CMD_GAINED_FOCUS:
    ae::platform::android::applyImmersiveLandscapeWindow(app->activity);
    applyEvent(shell, ae::platform::AppEvent::GainFocus);
    break;
  case APP_CMD_LOST_FOCUS:
    applyEvent(shell, ae::platform::AppEvent::LoseFocus);
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

  const int32_t bufferWidth = app->window != nullptr ? ANativeWindow_getWidth(app->window) : 0;
  const int32_t bufferHeight = app->window != nullptr ? ANativeWindow_getHeight(app->window) : 0;
  // The swapchain can use a 90-degree pre-transform: its buffer is portrait
  // while Android motion coordinates and the visible HUD are landscape.
  const int32_t width = std::max(bufferWidth, bufferHeight);
  const int32_t height = std::min(bufferWidth, bufferHeight);
  if (width <= 0 || height <= 0) return 0;
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
  const bool explicitMap = ae::platform::android::readBooleanLaunchOption(app->activity, "aether.map_preview");
  // The launcher opens the production-test map. Diagnostic fixtures remain
  // explicit so benchmark numbers can never silently include the full scene.
  shell.dirtRoadPreview = explicitMap || (!shell.scenePreview && !benchmarkPreview && !shell.materialPreview);
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
  shell.firstPersonEnabled = shell.dirtRoadPreview && !shell.lockCamera &&
      !ae::platform::android::readBooleanLaunchOption(app->activity, "aether.free_camera");
  shell.instancedRenderer.setRuntimeHudEnabled(shell.firstPersonEnabled);
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
  ae::platform::android::AndroidPerformancePolicy performancePolicy{};
  performancePolicy.targetFrameDurationNs =
      static_cast<ae::i64>(1'000'000'000ULL / shell.frameBudget.renderHz);
  performancePolicy.preferSustainedPerformance =
      !ae::platform::android::readBooleanLaunchOption(
          app->activity, "aether.disable_sustained_performance");
  shell.performance.initialize(app->activity, performancePolicy);
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
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[GpuIsolation] mode=%s scope=diagnostic-only",
                      ae::renderer::gpuCostIsolationName(shell.gpuCostIsolation));
  shell.instancedRenderer.setCoveragePrepassEnabled(
      !ae::platform::android::readBooleanLaunchOption(app->activity,
                                                       "aether.disable_coverage_prepass"));
  // HZB occlusion is a new, opt-in experiment (unlike coverage prepass above,
  // which is opt-out): default false until a physical A/B validates it. See
  // InstancedRenderer::setHzbOcclusionEnabled and
  // PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md.
  shell.instancedRenderer.setHzbOcclusionEnabled(
      ae::platform::android::readBooleanLaunchOption(app->activity, "aether.hzb_occlusion"));
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
  // LOD selection: same opt-in-and-off-by-default discipline as HZB above.
  // See InstancedRenderer::setLodSelectionEnabled and
  // PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md.
  shell.instancedRenderer.setLodSelectionEnabled(
      ae::platform::android::readBooleanLaunchOption(app->activity, "aether.lod_selection"));
  float lodPixelErrorBudget = shell.visibilityBudget.lodPixelErrorBudget;
  if (ae::platform::android::readFloatLaunchOption(app->activity, "aether.lod_pixel_error_budget",
                                                    lodPixelErrorBudget)) {
    shell.visibilityBudget.lodPixelErrorBudget = lodPixelErrorBudget;
  }
  shell.instancedRenderer.setLodPixelErrorBudget(shell.visibilityBudget.lodPixelErrorBudget);
  float lodHysteresisBandRatio = shell.visibilityBudget.lodHysteresisBandRatio;
  if (ae::platform::android::readFloatLaunchOption(app->activity, "aether.lod_hysteresis_band_ratio",
                                                    lodHysteresisBandRatio)) {
    shell.visibilityBudget.lodHysteresisBandRatio = lodHysteresisBandRatio;
  }
  shell.instancedRenderer.setLodHysteresisBandRatio(
      shell.visibilityBudget.lodHysteresisBandRatio);
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

  initializeDotNetHost(shell);
  shell.framePacer.initialize();
  shell.lastGameplayUpdate = std::chrono::steady_clock::now();
  shell.lastFpsPublishedAt = shell.lastGameplayUpdate;

  // Diagnóstico opt-in, fora da thread de eventos/render. Device e fila são
  // exclusivos do worker: não há vkQueueSubmit concorrente na fila do shell.
  std::future<void> astcProbe;
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
    // Item 5.2 do plano de lacunas: só sai do poll bloqueante (-1, custo
    // térmico zero em repouso — comportamento original preservado) quando
    // há de fato um frame para desenhar. Sem surface pronta ou app
    // suspenso, o loop continua bloqueando sem evento, exatamente como
    // antes deste item existir.
    const bool shouldDraw = shell.lifecycle.isActive() && shell.instancedRendererReady;
    android_poll_source *source = nullptr;
    int events = 0;
    const int pollTimeout = shell.lifecycle.isActive() && shell.framePacer.available()
                                ? -1
                                : (shouldDraw ? 0 : (shell.rendererInitialization.valid() ? 16 : -1));
    const int result = ALooper_pollOnce(pollTimeout, nullptr, &events,
                                        reinterpret_cast<void **>(&source));
    if (result >= 0 && source != nullptr) source->process(app, source);

    if (app->destroyRequested != 0) {
      applyEvent(shell, ae::platform::AppEvent::Destroy);
      continue;
    }
    collectRendererInitialization(shell,false);

    // O evento processado acima pode ter destruído o renderer/janela.
    const bool frameAdmitted = !shell.framePacer.available() || shell.framePacer.consumeFrame();
    if (shell.lifecycle.isActive() && shell.instancedRendererReady && frameAdmitted) {
      const auto frameWorkStarted = std::chrono::steady_clock::now();
      if (shell.cameraRouteReplayActive) {
        // Determinism comes from indexing by frame ordinal, never by wall
        // clock -- see camera_route.h. Looping by modulo lets a short route
        // drive an arbitrarily long soak run.
        shell.cameraController.setState(
            shell.cameraRoutePlayer.sample(shell.cameraRouteFrameOrdinal));
        ++shell.cameraRouteFrameOrdinal;
      } else if (shell.firstPersonEnabled) {
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
      const float timeSeconds = std::chrono::duration<float>(
                                    std::chrono::steady_clock::now() - shell.shellStartTime)
                                    .count();
      ae::renderer::RuntimeHudState hud{};
      hud.visible = shell.firstPersonEnabled;
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
      const ae::rhi::SwapchainStatus frameStatus = shell.instancedRenderer.drawFrame(
          timeSeconds, shell.cameraController.state(), hud);
      shell.performance.reportFrameDuration(std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - frameWorkStarted).count());
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

  collectRendererInitialization(shell,true);
  shell.characterMotor.shutdown();
  shell.performance.setActive(false, false);
  shell.performance.shutdown();
  if (shell.shutdownScene != nullptr) shell.shutdownScene();
  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo encerrado.");
}
