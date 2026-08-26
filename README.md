# Aether — Engine de Jogos Nativa Mobile

Editor **landscape fullscreen** que roda no próprio dispositivo. Vulkan 1.3, C# em cima
de um núcleo C++20, no-code (AetherFlow) com round-trip para C#.

Ver `docs/PLANO-ENGINE-MOBILE.md` para o plano completo.
Ver `docs/PLANO-FECHAMENTO-LACUNAS.md` para a ordem executável de fechamento
dos itens parciais, limitações conhecidas e critérios de validação M0–M9.

## Estado atual: Fase 1 (Núcleo) em execução

A implementação local da verdade operacional (§4 do plano de fechamento) está concluída. Isso não
fecha o gate M0: a primeira execução dos workflows no GitHub, o runner Android
físico e as medições sustentadas de hardware ainda precisam produzir evidência.

| Módulo | Estado |
|---|---|
| `managed/Aether.Core` — math, alocadores, jobs, ECS, serialização, undo/WAL | em construção |
| `managed/Aether.Flow` — AST, validador, compilador, round-trip | em construção |
| `native/core` — plataforma, alocadores, jobs | em construção |
| `native/rhi` — RHI Vulkan | em construção |
| `native/rendergraph` — compilador de render graph (testável headless) | em construção |
| `android/app` — shell NativeActivity ARM64 + frame Vulkan | triângulo validado em 1 aparelho físico; matriz de GPUs pendente |
| `prototype/` — protótipo do editor landscape | em construção |

## Build

```bash
dotnet build Aether.sln -c Release       # camada C#
cmake -S native -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native               # núcleo nativo
dotnet run --project tests/Aether.Tests  # runner C# próprio
```

### APK Android

Requer Android SDK 35, NDK `27.1.12297006`, CMake `3.22.1` e Java 17.
Configure `ANDROID_HOME` (ou `android/local.properties`) e execute:

```powershell
cd android
.\gradlew.bat :app:lintDebug :app:assembleDebug --offline
```

Saída: `android/app/build/outputs/apk/debug/app-debug.apk`.

Com um aparelho ADB conectado, a regressão reproduzível do shell pode ser
executada da raiz do repositório:

```powershell
.\tools\validate-android-shell.ps1
```

O gate prolongado usa `-LifecycleCycles 100 -ExerciseConfigurationChange` e
gera evidências em `build/android-validation/`.

O APK atual é o shell gráfico técnico da Fase 0. Ele cria surface, swapchain,
pipeline e apresenta um triângulo RGB em Vulkan. A regressão atual cobre 1.000
frames, 100 retomadas e reconstrução após mudança de configuração. Ainda não
contém editor, cubo texturizado, depth buffer ou integração .NET↔renderer.
