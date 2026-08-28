# Shell Android nativo

Fatia de fundação dos itens 0.1.3 e 2.1 e do critério visual M0. O APK é um
shell Vulkan/.NET, não um editor de cenas. Estado atualizado em 28/08/2026.

## Responsabilidades

- `android/`: Gradle, manifesto, recursos e NDK.
- `native/platform/app_lifecycle.*`: máquina de estado portátil/testável.
- `android_main.cpp`: traduz lifecycle e toque Android; envia yaw/pitch em
  radianos, nunca `AInputEvent`, para o renderer.
- `android_window.*`: landscape imersivo e restauração ao receber foco.
- `lifecycle_trace.h`: comandos/estados antes e depois do handler, durações de
  recriação e latência até o primeiro present após ativação; somente por evento.
- `android_vulkan_surface.*`: possui surface, device e swapchain.
- `instanced_renderer.*`: cubo checker central + 4.999 mini-cubos, depth,
  descritores e draw instanciado; um crossing C++→C# por frame.
- `dotnet_host.*`/`dotnet_assets.*`: hospedagem CoreCLR e extração dos assets.
- `native/rhi/`: recursos Vulkan/VMA, budgets, upload, swapchain e pré-rotação
  independentes de Android. Contratos detalhados em [RHI-RECURSOS.md](RHI-RECURSOS.md).
- `android_triangle_renderer.*`: fixture histórico, não o renderer ativo.

## Lifecycle e ownership

INIT_WINDOW cria surface/device/swapchain e renderer. RESUME+FOCUS ativa
apresentação; PAUSE/LOST_FOCUS suspende desenho e cancela o arraste.
TERM_WINDOW espera GPU e destrói renderer antes da swapchain/device.
DESTROY libera recursos e encerra o loop.

O poll é não bloqueante somente com lifecycle ativo e renderer pronto.
Após processar cada evento, o loop verifica novamente essas condições:
um TERM_WINDOW não pode deixar o frame seguinte acessar recursos destruídos.

Na recriação, framebuffers/comandos morrem antes das image views/swapchain.
`SurfaceLost` recria a surface inteira; `OutOfDate`/`Suboptimal` e configuração
recriam swapchain+renderer. Estado de órbita sobrevive às recriações no processo.

## Build e shaders

Requer Java 17, SDK 35, NDK `27.1.12297006`, CMake `3.22.1` e Gradle 8.10.2
(wrapper). ABI arm64-v8a, segmentos ELF alinhados a páginas de 16 KB.

```powershell
# Na raiz; SDK deve estar configurado em ANDROID_HOME
.\tools\generate-embedded-shaders.ps1 -ShaderName instanced -Check
.\tools\generate-embedded-shaders.ps1 -ShaderName triangle -Check
.\android\gradlew.bat -p android :app:assembleDebug :app:assembleRelease :app:lintDebug --offline
```

Sem `-Check`, o gerador recompila GLSL, valida com spirv-val e atualiza o header.
O CI Android verifica ambos os shaders contra o NDK pinado. Esse processo não
é hot reload nem compilação Slang/HLSL.

## Validação em aparelho

Xiaomi 25053PC47G (onyx, SM8735/Adreno, Android 16):

- APK instala, hospeda .NET e apresenta 1.000 frames;
- display 2772×1280, imagem da swapchain 1280×2772 com pré-rotação de 90°;
- cubo texturizado com depth, upload staging e rotação por arraste;
- 2 retomadas, configuração/uiMode e tela off/on verdes na regressão de 28/08;
- capturas inspecionadas; a distorção de proporção da primeira versão foi corrigida.

Evidência: `build/android-validation/rhi-cube-final-20260828/report.json`.
A rodada recompilada `rhi-cube-verified-20260828/` excedeu 20 s ao reacender
a tela; a falha histórica continua preservada. A investigação posterior está
em `lifecycle-trace-20260828-01/` e `lifecycle-trace-20260828-02/`: o Android
manteve o keyguard visível e não devolveu foco ao aplicativo. O usuário confirmou
que o aparelho tem senha. No segundo ensaio a tela de bloqueio voltou a apagar;
o snapshot foi coletado antes do cleanup. Não foi demonstrada trava do renderer.

O runner corrigido passou 3 retomadas, configuração/restauração e screen off/on
em `lifecycle-keyguard-20260828-verified/report.json`, PID 4256 preservado.
A espera pelo desbloqueio foi 6.218 ms; depois, a recriação gráfica levou
43,901 ms e o primeiro present retornou 1,540 ms após a ativação. Esta última
métrica não inclui o tempo anterior à ativação nem mede scanout/FPS sustentado.
Essa rodada usou até três solicitações de dismissal; a versão final solicita
uma única vez, para não interferir na autenticação humana.
A versão final também passou screen off/on com uma solicitação, PID 6119,
em `lifecycle-keyguard-20260828-blocked/report.json`: apesar do nome do diretório
(destinado a teste negativo), houve desbloqueio em 16,183 s e o resultado correto
foi PASS. O caso de bloqueio persistente foi validado pelos testes headless, não
por essa rodada. O ensaio auxiliar `lifecycle-keyguard-20260828-negative/` falhou
por usar apenas 5 s para atingir 1.000 frames; não é evidência de bloqueio nem
regressão gráfica. Todos os relatórios permanecem preservados.
O fluxo normal sem ciclo de tela é registrado separadamente em
`rhi-cube-normal-20260828/`.
Os 100 ciclos e capturas bit-idênticas históricos pertencem ao antigo triângulo,
não à cena animada. Não foi executada validation layer Vulkan nesta rodada.

### Runner reproduzível

`tools/validate-android-shell.ps1` instala, inventaria, espera frames apresentados,
exercita lifecycle, coleta Logcat/capturas e detecta erros/crash/ANR. Por padrão
limpa dados do pacote; use `-PreserveAppData` para conservá-los. Descobre um único
aparelho autorizado; com vários, passe `-DeviceSerial`.

```powershell
.\tools\validate-android-shell.ps1 -AllowScreenshotDifference -PreserveAppData
.\tools\validate-android-shell.ps1 -LifecycleCycles 100 -ExerciseConfigurationChange -AllowScreenshotDifference -PreserveAppData
.\tools\validate-android-shell.ps1 -ExerciseScreenCycle -AllowScreenshotDifference -PreserveAppData
.\tests\tools\test-android-shell-lifecycle.ps1
```

`-AllowScreenshotDifference` é necessário para a animação: comparação de hash não
é teste de correção visual. uiMode e tela são opcionais e restaurados ao terminar;
bloqueio seguro exige desbloqueio humano, nunca envio de senha pelo runner.
`-ScreenCycles N` repete o teste quando `-ExerciseScreenCycle` está habilitado.
O runner aguarda `SCREEN_STATE_ON`, solicita `wm dismiss-keyguard` uma vez e
confirma que delegate/monitor já não mostram keyguard antes de iniciar a Activity.
Código de saída zero de `wm` não é prova de desbloqueio. O marcador de retomada
é criado depois de confirmar tela apagada, evitando aceitar ativação anterior.

O relatório v3 separa `keyguardWaits`/`keyguardDismissRequests` das evidências
de lifecycle. Sem desbloqueio no prazo, registra `status=blocked`,
`failureKind=device-keyguard-blocked` e retorna código 1: não é PASS nem prova
de lentidão gráfica. Saída desconhecida do dumpsys falha explicitamente.
O timeout de renderização permanece em 20 s por padrão; não foi aumentado.
Falhas salvam `lifecycle-diagnostics.json` antes de restaurar o aparelho;
`lifecycle-timeline.txt` mantém timestamps e estados nativos.

Os helpers possuem 12 regressões headless integradas ao CI Android; a máquina
de lifecycle tem 7 testes C++, incluindo as seis ordens de retomada e
resume/pause transitório atrás do keyguard. O engine continua aguardando
RESUME + foco + janela, sem polling periódico enquanto suspenso.
O contrato de desbloqueio seguro segue a [API Android KeyguardManager](https://developer.android.com/reference/android/app/KeyguardManager#requestDismissKeyguard(android.app.Activity,%20android.app.KeyguardManager.KeyguardDismissCallback)).

`send-trim-memory` é apenas proxy
de pressão de memória, não prova de APP_CMD_LOW_MEMORY real.

O runner aceita `-SoakMinutes`, coleta ibat×vbat/status térmico e pode exigir
orçamento médio com `-RequirePowerBudget`. O checkpoint de 1 minuto de 26/08
valida o coletor, não o critério de 30 min da PoC-C.

## Limites e próximo gate

CPU total/thread, distribuição de frames e FPS exibidos agora têm coleta opt-in
com `-ProfileSeconds 60`. Resultado: 60,056 FPS no compositor, CPU processo média
2,928 ms e pico 15,429 ms na captura otimizada; o orçamento < 3 ms ainda não foi
atendido. Método, resultados e limitações em [PROFILING-ANDROID.md](PROFILING-ANDROID.md).

Cubo/texture/depth/touch estão integrados. Permanecem: geometria importada,
Material/Scene de produto, InputState gerenciado ligado ao Android, Render Graph
executado, uploads assíncronos, múltiplos frames em voo, hot reload C#, profiling
com timeline/atribuição por thread, timestamp queries GPU, validation layers e
matriz Mali/perfil C. A coleta atual não fecha esses gates. M0 continua aberto.
