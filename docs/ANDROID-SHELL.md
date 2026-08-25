# Shell Android nativo

Implementação do item **0.1.3** da Fase 0. O objetivo deste marco é estabelecer
a fronteira de plataforma real antes de adicionar um pipeline de renderização.

## Responsabilidades

- `android/`: empacotamento Gradle, manifesto, recursos e configuração NDK.
- `native/platform/app_lifecycle.*`: máquina de estado portátil e testável.
- `native/platform/android/android_main.cpp`: tradução de `APP_CMD_*` para a
  máquina de estado; não contém regras de renderização.
- `native/platform/android/android_window.*`: landscape imersivo e restauração
  das flags quando o Android devolve foco.
- `native/platform/android/android_vulkan_surface.*`: ownership de instance,
  device e `VkSurfaceKHR` enquanto a janela nativa existe.
- `native/rhi/device.*`: criação Vulkan genérica, sem dependência Android.

## Lifecycle e ownership

```text
INIT_WINDOW  -> cria instance, surface e device com fila de apresentação
RESUME+FOCUS -> aplicativo ativo
PAUSE/FOCUS  -> suspende trabalho; mantém surface se a janela ainda existe
TERM_WINDOW  -> espera o device e destrói surface, device e instance
DESTROY      -> garante liberação e encerra o loop
```

O loop usa `ALooper_pollOnce(-1)`: como ainda não há frame para produzir, a
thread permanece bloqueada entre eventos e não desperdiça CPU ou orçamento
térmico.

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

## Limite deste marco

Não há swapchain, shaders, command buffers ou apresentação. A surface só pode
ser considerada validada depois que o APK for aberto num aparelho físico e o
log `Surface Vulkan pronta` aparecer no Logcat, incluindo o teste de ir para o
background e voltar. Esse limite é intencional para não misturar o item 0.1.3
com a PoC-A de renderização e interoperabilidade.
