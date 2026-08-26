#include "platform/android/android_triangle_renderer.h"
#include "platform/android/android_vulkan_surface.h"
#include "platform/android/android_window.h"
#include "platform/app_lifecycle.h"

#include <android/log.h>
#include <android_native_app_glue.h>

namespace {

constexpr const char *LogTag = "Aether.Android";
constexpr ae::u64 ValidationFrameMilestone = 1000;

struct AndroidShell final {
  android_app *app = nullptr;
  ae::platform::AppLifecycle lifecycle;
  ae::platform::android::AndroidVulkanSurface vulkanSurface;
  ae::platform::android::TriangleRenderer triangleRenderer;
  bool triangleRendererReady = false;
  ae::u64 presentedFrameCount = 0;
  ae::u64 activationCount = 0;
  bool firstFrameAfterActivationPending = false;
  bool validationFrameMilestoneLogged = false;
};

// Reconstrói o renderer do triângulo depois que a surface/swapchain existem
// (na criação) ou depois que a swapchain foi recriada (resize/rotação) —
// framebuffers referenciam VkImageView específicos da swapchain anterior,
// então não sobrevivem a uma recriação, mesmo que o renderer em si pudesse.
bool rebuildTriangleRenderer(AndroidShell &shell) {
  shell.triangleRenderer.shutdown();
  shell.triangleRendererReady = shell.triangleRenderer.initialize(
      shell.vulkanSurface.device(), shell.vulkanSurface.swapchain());
  if (!shell.triangleRendererReady) shell.triangleRenderer.shutdown();
  return shell.triangleRendererReady;
}

bool recreateSwapchainAndRenderer(AndroidShell &shell) {
  // Framebuffers precisam morrer ANTES das image views da swapchain antiga.
  // Inverter esta ordem viola o lifetime Vulkan mesmo depois de wait-idle.
  shell.triangleRenderer.shutdown();
  shell.triangleRendererReady = false;
  if (shell.app->window == nullptr ||
      !shell.vulkanSurface.recreateSwapchain(shell.app->window)) {
    return false;
  }
  return rebuildTriangleRenderer(shell);
}

bool recreateSurfaceAndRenderer(AndroidShell &shell) {
  shell.triangleRenderer.shutdown();
  shell.triangleRendererReady = false;
  shell.vulkanSurface.shutdown();
  if (shell.app->window == nullptr || !shell.vulkanSurface.initialize(shell.app->window)) {
    return false;
  }
  return rebuildTriangleRenderer(shell);
}

void applyEvent(AndroidShell &shell, ae::platform::AppEvent event) {
  const ae::platform::LifecycleAction action = shell.lifecycle.apply(event);
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::CreateSurface)) {
    if (!shell.vulkanSurface.initialize(shell.app->window)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "O shell continuará ativo sem GPU; uma nova janela tentará novamente.");
    } else if (!rebuildTriangleRenderer(shell)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "Surface pronta, mas o pipeline de desenho falhou ao inicializar.");
    }
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::DestroySurface)) {
    shell.triangleRenderer.shutdown();
    shell.triangleRendererReady = false;
    shell.vulkanSurface.shutdown();
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::BecameActive)) {
    ++shell.activationCount;
    shell.firstFrameAfterActivationPending = true;
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Aplicativo ativo.");
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::BecameInactive)) {
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Aplicativo suspenso.");
  }
}

void handleCommand(android_app *app, int32_t command) {
  auto &shell = *static_cast<AndroidShell *>(app->userData);
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
      __android_log_print(ANDROID_LOG_INFO, LogTag, "Configuração alterada: janela=%dx%d.",
                          ANativeWindow_getWidth(app->window),
                          ANativeWindow_getHeight(app->window));
      // O evento do SO pode chegar antes do Vulkan reportar OutOfDate (mais
      // comum em drivers mobile que em desktop) — recriar aqui em vez de só
      // esperar o próximo acquire/present falhar, mesmo aviso documentado
      // em ISwapchain::recreate.
      if (shell.vulkanSurface.isReady() && !recreateSwapchainAndRenderer(shell)) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag,
                            "Falha ao recriar swapchain/pipeline após mudança de configuração.");
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
}

} // namespace

void android_main(android_app *app) {
  AndroidShell shell{};
  shell.app = app;
  app->userData = &shell;
  app->onAppCmd = handleCommand;

  ae::platform::android::applyImmersiveLandscapeWindow(app->activity);

  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo iniciado.");

  while (!shell.lifecycle.isDestroyed()) {
    // Item 5.2 do plano de lacunas: só sai do poll bloqueante (-1, custo
    // térmico zero em repouso — comportamento original preservado) quando
    // há de fato um frame para desenhar. Sem surface pronta ou app
    // suspenso, o loop continua bloqueando sem evento, exatamente como
    // antes deste item existir.
    const bool shouldDraw = shell.lifecycle.isActive() && shell.triangleRendererReady;
    android_poll_source *source = nullptr;
    int events = 0;
    const int result = ALooper_pollOnce(shouldDraw ? 0 : -1, nullptr, &events,
                                        reinterpret_cast<void **>(&source));
    if (result >= 0 && source != nullptr) source->process(app, source);

    if (app->destroyRequested != 0) {
      applyEvent(shell, ae::platform::AppEvent::Destroy);
      continue;
    }

    if (shouldDraw) {
      const ae::rhi::SwapchainStatus frameStatus = shell.triangleRenderer.drawFrame();
      if (frameStatus == ae::rhi::SwapchainStatus::Ok ||
          frameStatus == ae::rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
        ++shell.presentedFrameCount;
        if (shell.firstFrameAfterActivationPending) {
          shell.firstFrameAfterActivationPending = false;
          __android_log_print(ANDROID_LOG_INFO, LogTag,
                              "Primeiro frame após ativação apresentado: ciclo=%llu.",
                              static_cast<unsigned long long>(shell.activationCount));
        }
        if (!shell.validationFrameMilestoneLogged &&
            shell.presentedFrameCount >= ValidationFrameMilestone) {
          shell.validationFrameMilestoneLogged = true;
          __android_log_print(ANDROID_LOG_INFO, LogTag,
                              "Marco de renderização atingido: %llu frames apresentados.",
                              static_cast<unsigned long long>(shell.presentedFrameCount));
        }
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
        shell.triangleRenderer.shutdown();
        shell.triangleRendererReady = false;
        shell.vulkanSurface.shutdown();
      }
    }
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo encerrado.");
}
