#include "platform/android/android_paths.h"
#include "platform/android/android_vulkan_surface.h"
#include "platform/android/android_window.h"
#include "platform/android/dotnet_assets.h"
#include "platform/android/dotnet_host.h"
#include "platform/android/instanced_renderer.h"
#include "platform/app_lifecycle.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <cstdio>

namespace {

constexpr const char *LogTag = "Aether.Android";
constexpr ae::u64 ValidationFrameMilestone = 1000;
// PoC-A (item 0.2 do plano): 5.000 objetos é o número que o critério de
// sucesso pede ("5.000 objetos renderizados a 60 fps com < 3 ms de CPU").
constexpr ae::u32 PocAInstanceCount = 5000;
constexpr ae::u64 PocAReportIntervalFrames = 300; // ~5s a 60fps — log periódico, não por frame

struct AndroidShell final {
  android_app *app = nullptr;
  ae::platform::AppLifecycle lifecycle;
  ae::platform::android::AndroidVulkanSurface vulkanSurface;
  ae::platform::android::InstancedRenderer instancedRenderer;
  bool instancedRendererReady = false;
  ae::u64 presentedFrameCount = 0;
  ae::u64 activationCount = 0;
  bool firstFrameAfterActivationPending = false;
  bool validationFrameMilestoneLogged = false;
  ae::platform::android::DotNetHost dotNetHost;
  std::chrono::steady_clock::time_point shellStartTime = std::chrono::steady_clock::now();
  double pocAMaxFillMicroseconds = 0.0;
};

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
  std::snprintf(runtimeConfigPath, sizeof(runtimeConfigPath), "%s/Aether.Core.runtimeconfig.json",
               dotnetRoot);
  std::snprintf(managedAssemblyPath, sizeof(managedAssemblyPath), "%s/Aether.Core.dll", dotnetRoot);

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
}

// Reconstrói o renderer instanciado depois que a surface/swapchain existem
// (na criação) ou depois que a swapchain foi recriada (resize/rotação) —
// framebuffers referenciam VkImageView específicos da swapchain anterior,
// então não sobrevivem a uma recriação, mesmo que o renderer em si pudesse.
// Sem CoreCLR pronto (dotNetHost.isReady() falso), a PoC-A simplesmente não
// roda nesta sessão — degradação silenciosa, mesma disciplina de "continuar
// sem GPU" já usada para Vulkan.
bool rebuildInstancedRenderer(AndroidShell &shell) {
  shell.instancedRenderer.shutdown();
  if (!shell.dotNetHost.isReady()) {
    shell.instancedRendererReady = false;
    return false;
  }
  shell.instancedRendererReady = shell.instancedRenderer.initialize(
      shell.vulkanSurface.device(), shell.vulkanSurface.swapchain(), shell.dotNetHost,
      PocAInstanceCount);
  if (!shell.instancedRendererReady) shell.instancedRenderer.shutdown();
  return shell.instancedRendererReady;
}

bool recreateSwapchainAndRenderer(AndroidShell &shell) {
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
  shell.instancedRenderer.shutdown();
  shell.instancedRendererReady = false;
  shell.vulkanSurface.shutdown();
  if (shell.app->window == nullptr || !shell.vulkanSurface.initialize(shell.app->window)) {
    return false;
  }
  return rebuildInstancedRenderer(shell);
}

void applyEvent(AndroidShell &shell, ae::platform::AppEvent event) {
  const ae::platform::LifecycleAction action = shell.lifecycle.apply(event);
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::CreateSurface)) {
    if (!shell.vulkanSurface.initialize(shell.app->window)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "O shell continuará ativo sem GPU; uma nova janela tentará novamente.");
    } else if (!rebuildInstancedRenderer(shell)) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag,
                          "Surface pronta, mas o pipeline de desenho falhou ao inicializar.");
    }
  }
  if (ae::platform::hasAction(action, ae::platform::LifecycleAction::DestroySurface)) {
    shell.instancedRenderer.shutdown();
    shell.instancedRendererReady = false;
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

  initializeDotNetHost(shell);

  while (!shell.lifecycle.isDestroyed()) {
    // Item 5.2 do plano de lacunas: só sai do poll bloqueante (-1, custo
    // térmico zero em repouso — comportamento original preservado) quando
    // há de fato um frame para desenhar. Sem surface pronta ou app
    // suspenso, o loop continua bloqueando sem evento, exatamente como
    // antes deste item existir.
    const bool shouldDraw = shell.lifecycle.isActive() && shell.instancedRendererReady;
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
      const float timeSeconds = std::chrono::duration<float>(
                                    std::chrono::steady_clock::now() - shell.shellStartTime)
                                    .count();
      const ae::rhi::SwapchainStatus frameStatus = shell.instancedRenderer.drawFrame(timeSeconds);
      if (frameStatus == ae::rhi::SwapchainStatus::Ok ||
          frameStatus == ae::rhi::SwapchainStatus::SuboptimalNeedsRecreate) {
        ++shell.presentedFrameCount;
        if (shell.instancedRenderer.lastFillMicroseconds() > shell.pocAMaxFillMicroseconds) {
          shell.pocAMaxFillMicroseconds = shell.instancedRenderer.lastFillMicroseconds();
        }
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
        // PoC-A (item 0.2): reporta o pico de custo do crossing C++→C# a
        // cada ~5s, não a cada frame — o objetivo é ter evidência legível
        // no log contra o critério de < 3 ms de CPU, sem inundar o logcat.
        if (shell.presentedFrameCount % PocAReportIntervalFrames == 0) {
          __android_log_print(
              ANDROID_LOG_INFO, LogTag,
              "PoC-A: %u instâncias, crossing C++<->C# pico=%.1f us (orçamento: 3000 us) — %s.",
              PocAInstanceCount, shell.pocAMaxFillMicroseconds,
              shell.pocAMaxFillMicroseconds < 3000.0 ? "dentro do orçamento" : "ACIMA do orçamento");
          shell.pocAMaxFillMicroseconds = 0.0;
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
        shell.instancedRenderer.shutdown();
        shell.instancedRendererReady = false;
        shell.vulkanSurface.shutdown();
      }
    }
  }

  __android_log_print(ANDROID_LOG_INFO, LogTag, "Shell nativo encerrado.");
}
