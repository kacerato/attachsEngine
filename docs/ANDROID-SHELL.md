# Shell Android nativo

Implementação do item **0.1.3** da Fase 0, estendida pelo shell gráfico mínimo
da Onda 1 §5.2 de `PLANO-FECHAMENTO-LACUNAS.md`. O marco estabelece a fronteira
de plataforma e prova um pipeline Vulkan completo em hardware Android real.

## Responsabilidades

- `android/`: empacotamento Gradle, manifesto, recursos e configuração NDK.
- `native/platform/app_lifecycle.*`: máquina de estado portátil e testável.
- `native/platform/android/android_main.cpp`: tradução de `APP_CMD_*` para a
  máquina de estado; não contém regras de renderização.
- `native/platform/android/android_window.*`: landscape imersivo e restauração
  das flags quando o Android devolve foco.
- `native/platform/android/android_vulkan_surface.*`: ownership de instance,
  device, `VkSurfaceKHR` e swapchain enquanto a janela nativa existe.
- `native/platform/android/android_triangle_renderer.*`: vertical slice de
  render pass, pipeline, framebuffers, command buffer e draw do triângulo.
- `native/rhi/device.*`: device e swapchain Vulkan genéricos, sem dependência
  Android; diferencia resize, perda de surface e erro fatal.
- `native/rhi/shaders/`: fonte GLSL do triângulo e SPIR-V embutido temporário.

## Lifecycle e ownership

```text
INIT_WINDOW  -> cria instance, surface, device, swapchain e pipeline
RESUME+FOCUS -> aplicativo ativo; acquire/draw/submit/present contínuo
PAUSE/FOCUS  -> suspende desenho; mantém recursos se a janela ainda existe
TERM_WINDOW  -> wait-idle; destrói renderer, swapchain, surface e device
DESTROY      -> garante liberação e encerra o loop
```

O loop usa `ALooper_pollOnce(0)` somente quando lifecycle e renderer estão
ativos. Suspenso, sem janela ou sem pipeline, volta a `ALooper_pollOnce(-1)` e
permanece bloqueado entre eventos, sem busy-loop fora de foreground.

Na recriação, a ordem é obrigatória: framebuffers e command pool morrem antes
das image views/swapchain; depois surface/device. `SurfaceLost` recria a surface
inteira, enquanto `OutOfDate`/`Suboptimal` recriam apenas swapchain+renderer.

## Build

Pré-requisitos fixados pelo build atual:

- Java 17;
- Android SDK/compileSdk 35;
- NDK `27.1.12297006`;
- CMake `3.22.1`;
- Gradle 8.10.2 (wrapper incluído);
- ABI `arm64-v8a`.
- segmentos ELF alinhados para páginas Android de 16 KB.

```powershell
cd android
.\gradlew.bat :app:lintDebug :app:assembleDebug --offline
```

## Validação e limite deste marco

Validado no Xiaomi 25053PC47G (`onyx`, Snapdragon SM8735/Adreno, Android 16):

- APK instala e abre;
- surface 2772×1280 e swapchain de 5 imagens são criadas;
- triângulo RGB é apresentado;
- 100 ciclos background→foreground preservam o PID e retomam a apresentação;
- mudança e restauração de `uiMode` recriam swapchain e pipeline;
- screenshots antes/depois são bit-idênticas (SHA-256
  `949E1829D4D53BDC63F1B548C88ECF05ECD27FEDD47E8E03FDF4FDB9B2A59719`).

Nos 100 ciclos automatizados, o Android reteve a mesma `ANativeWindow`; isso é
permitido e evita trabalho desnecessário. A retomada com recursos retidos e a
recriação por configuração são invariantes distintas e ambas foram verificadas.
Uma rodada manual anterior percorreu a destruição/recriação integral da janela,
mas a injeção determinística de `SurfaceLost` ainda não faz parte do runner.

### Runner reproduzível

`tools/validate-android-shell.ps1` automatiza a validação sem incorporar ADB ao
runtime. Ele descobre um único aparelho autorizado (ou aceita
`-DeviceSerial`), instala e limpa o app, confirma os logs de inicialização,
espera 1.000 frames realmente apresentados, exercita retomadas preservando o
PID e comprova que a apresentação volta após cada retomada. Como o Android pode
reter legitimamente a mesma `ANativeWindow` em background, o runner registra
quantas recriações ocorreram nesses ciclos sem exigi-las artificialmente. A
opção de mudança de configuração alterna temporariamente o `uiMode` e verifica
separadamente a recriação obrigatória de swapchain/pipeline. O teste também
simula pressão de memória e compara capturas SHA-256. Erros do shell, crash
nativo, exceção Java ou ANR tornam a
execução vermelha. O app é encerrado e os temporários remotos são removidos em
`finally`, inclusive quando o teste falha. Se o aparelho estiver dormindo, o
runner preserva a proteção de proximidade, desperta/dispensa o keyguard e devolve
o aparelho ao estado de energia original ao terminar. No ciclo de tela, ele
sincroniza as transições `Asleep`/`Awake` com `dumpsys power` e aguarda o modo
imersivo convergir antes de comparar a captura, em vez de depender de sleeps
fixos sujeitos a corrida.

```powershell
# Regressão rápida local (3 retomadas)
.\tools\validate-android-shell.ps1

# Critério do shell gráfico (100 retomadas) e mudança de configuração
.\tools\validate-android-shell.ps1 -LifecycleCycles 100 -ExerciseConfigurationChange

# Aparelho específico e ciclo de tela, se o laboratório permitir desbloqueio
.\tools\validate-android-shell.ps1 -DeviceSerial SERIAL -ExerciseScreenCycle
```

Cada execução grava capturas, Logcat e `report.json` em
`build/android-validation/<data-hora>/`, diretório ignorado pelo Git. A mudança
de configuração é opcional porque alterna o modo noturno global do aparelho;
quando usada, o runner salva e restaura o valor original mesmo após falha. O
ciclo de tela também é opcional porque aparelhos com bloqueio seguro podem
exigir intervenção humana ao despertar.

O marco ainda não é o renderer da engine. Não possui depth, vertex/index buffer,
textura, cubo, Render Graph executado, múltiplos frames em voo, toolchain de
shader ou interop .NET. Também falta validar Mali/perfil C e outros drivers.
