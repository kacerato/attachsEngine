# Aether — Engine de Jogos Nativa Mobile

Editor **landscape fullscreen** que roda no próprio dispositivo. Vulkan 1.3, C# em cima
de um núcleo C++20, no-code (AetherFlow) com round-trip para C#.

Ver `docs/PLANO-ENGINE-MOBILE.md` para o plano completo.

## Estado atual: Fase 1 (Núcleo) em execução

| Módulo | Estado |
|---|---|
| `managed/Aether.Core` — math, alocadores, jobs, ECS, serialização, undo/WAL | em construção |
| `managed/Aether.Flow` — AST, validador, compilador, round-trip | em construção |
| `native/core` — plataforma, alocadores, jobs | em construção |
| `native/rhi` — RHI Vulkan | em construção |
| `native/rendergraph` — compilador de render graph (testável headless) | em construção |
| `android/app` — shell NativeActivity ARM64 + surface Vulkan | build validado; hardware pendente |
| `prototype/` — protótipo do editor landscape | em construção |

## Build

```bash
dotnet build managed/Aether.sln          # camada C#
cmake -S native -B build -G Ninja && ninja -C build   # núcleo nativo
dotnet test tests/                        # testes
```

### APK Android

Requer Android SDK 35, NDK `27.1.12297006`, CMake `3.22.1` e Java 17.
Configure `ANDROID_HOME` (ou `android/local.properties`) e execute:

```powershell
cd android
.\gradlew.bat :app:lintDebug :app:assembleDebug --offline
```

Saída: `android/app/build/outputs/apk/debug/app-debug.apk`.

O APK atual é o shell técnico da Fase 0. Ele valida empacotamento, lifecycle e
criação da surface Vulkan; ainda não contém editor, swapchain ou renderização.
