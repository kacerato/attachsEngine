# CPU e apresentação da PoC-A

Instrumentação opt-in da cena de 5.000 cubos texturizados, em 28/08/2026.
O critério do plano principal é 60 FPS com CPU abaixo de 3 ms. Sucesso da
**coleta** não significa aprovação desse orçamento.

## Contrato da medição

- `native/profiler/frame_statistics.*` é portátil, sem Android/Vulkan, com
  armazenamento fixo para 600 frames × 7 métricas (~34 KB), sem alocação no heap
  por frame. Um único proprietário/thread; não é um profiler de jobs concorrentes.
- O adaptador Android lê relógios cumulativos após cada present bem-sucedido:
  `CLOCK_MONOTONIC`, `CLOCK_PROCESS_CPUTIME_ID` e `CLOCK_THREAD_CPUTIME_ID`.
  O delta do processo inclui todas as suas threads (inclusive .NET e workers do
  driver); o da thread é somente o loop nativo. Tempo dormindo/bloqueado não é CPU.
- Intervalos consecutivos incluem processamento de eventos, chamadas do driver
  e overhead da instrumentação/relatório. Não é simplesmente `FillInstanceBuffer`.
- Acquire (incluindo fence), interop, record/flush/submit e present têm métricas
  **wall time** próprias. Acquire/present não são medições de duração da GPU.
- Cada ativação/recriação reinicia o aquecimento de 5 s e descarta janela parcial.
  O primeiro present estabelece baseline. Picos durante os intervalos de warm-up
  são preservados separadamente; não representam o cold start inteiro.
- Cada janela de 600 intervalos publica média, p50/p95/p99 nearest-rank e máximo.
  A captura agrega médias ponderadas e **maior percentil de janela**, nunca uma
  média de percentis apresentada como percentil global.

`present_fps` conta retornos de present por tempo monotônico. Segundo o
[contrato Vulkan](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html),
isso não comprova scanout. O runner também tenta localizar a layer exata da
NativeActivity e ler `SurfaceFlinger --latency`. Usa a segunda coluna,
`actualPresentTime`, conforme o [FrameTracker do AOSP](https://android.googlesource.com/platform/frameworks/native/+/master/services/surfaceflinger/FrameTracker.cpp).
Zeros/fences pendentes são ignorados, timestamps deduplicados e snapshots devem
se sobrepor: perda de cobertura invalida o FPS, não vira queda de desempenho
inventada. Layer ausente/ambígua registra indisponibilidade, sem substituir
frames exibidos por envios Vulkan.

A série do compositor e as janelas de CPU têm intervalos próprios, sobrepostos,
cada um com duração registrada; não servem para atribuir um spike individual ao
mesmo frame. O clock do processo/thread segue o [contrato POSIX](https://pubs.opengroup.org/onlinepubs/000095399/functions/clock_getres.html).

## Execução reproduzível

```powershell
.\android\gradlew.bat -p android :app:assembleDebug :app:assembleRelease :app:lintDebug --offline
.\tools\validate-android-shell.ps1 -ProfileSeconds 60 -LifecycleCycles 0 -AllowScreenshotDifference -PreserveAppData
.\tests\tools\test-android-frame-profile.ps1
```

O runner lança a Activity com `--ez aether.profile_frames true`. Sem essa opção,
não lê relógios CPU por frame nem publica janelas. Não altera senha, frequência,
DVFS, governadores ou configurações térmicas; mantenha a tela desbloqueada.

Para medir o nativo otimizado, assine uma **cópia** do release unsigned com o
certificado de desenvolvimento já usado pelo debug e passe `-ApkPath` ao runner:

```powershell
& "$env:ANDROID_HOME\build-tools\35.0.0\apksigner.bat" sign --ks "$env:USERPROFILE\.android\debug.keystore" --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android --out build/poc-a-profile-release.apk android/app/build/outputs/apk/release/app-release-unsigned.apk
.\tools\validate-android-shell.ps1 -ApkPath build/poc-a-profile-release.apk -ProfileSeconds 60 -LifecycleCycles 0 -AllowScreenshotDifference -PreserveAppData
```

Essas credenciais são as padrão do keystore **debug**, não uma senha do usuário.
Não configure
certificado de desenvolvimento como assinatura de produção. As medições desta
revisão usaram Build Tools 35.0.0, NDK 27.1.12297006 e RelWithDebInfo no release.

O relatório conserva hash do APK, revisão Git/worktree sujo, PID, build nativa,
resolução, temperaturas/status térmico inicial/final, todas as distribuições de
janelas e timestamps válidos de apresentação. O APK usa o asset ARM64 existente
`Aether.Core.dll` (SHA-256 `53ECFE4780C8CDB63347D08D24549F7CEDEECC20ED5CD564EE32EE56BF9E95F6`);
o Gradle atual não recompila C# automaticamente. O rótulo `optimized` é do nativo,
não uma afirmação sobre configuração de cada assembly. Mudanças gerenciadas
precisam ser republicadas no asset antes de uma nova comparação.

## Critérios e limitações

O gate de CPU usa conservadoramente **máximo < 3 ms**, pois o plano não define
percentil; média e p95/p99 continuam visíveis. `accepted` permanece falso enquanto
houver critérios/matriz não atendidos. Não exportamos esta cena como se fosse
`cpu.editor_frame_time_ms` do editor completo, nem preenchemos um orçamento GPU
com tempo de acquire. Faltam timestamp queries GPU e atribuição por thread/stack
(Perfetto/JIT/GC/driver). Uma captura de aproximadamente 1 minuto não fecha o
soak térmico de 30 minutos nem a matriz Mali/perfil C.

Testes: 10 C++ para estatísticas/relógios/reset/warm-up e 17 PowerShell para
parsing, continuidade, campos inválidos, agregação e timestamps do compositor.

## Resultado no Xiaomi SM8735/Adreno, Android 16

Captura principal: `build/android-validation/poc-a-profile-release-display-20260828/report.json`.
Nativo otimizado, 2772×1280, PID 21850; 4.200 intervalos CPU em 69,940 s e
3.619 timestamps de apresentação em 60,244 s contínuos. Também passaram duas
retomadas e mudança/restauração de configuração após a coleta.

| Métrica | Média | Maior p95 de janela | Máximo |
|---|---:|---:|---:|
| CPU do processo | 2,928 ms | 3,880 ms | 15,429 ms |
| CPU da thread de render | 1,825 ms | 2,388 ms | 2,910 ms |
| Crossing/preenchimento C# (wall) | 1,189 ms | 1,588 ms | 1,777 ms |
| Acquire/fence (wall) | 14,962 ms | 17,615 ms | 21,839 ms |

SurfaceFlinger: **60,056 FPS exibidos**, p95 global dos intervalos 16,651 ms,
máximo 16,654 ms; captura contínua e válida. Taxa de envio Vulkan: 60,051 FPS.
Temperatura da bateria 30,9 °C nas duas leituras, status térmico 0 nas pontas;
isso não comprova ausência de throttle durante todo o período.

**PoC-A continua parcial:** a média ficou abaixo de 3 ms nessa rodada, mas os
p95 e máximos do processo excederam o limite. A thread de render isolada não
explica o custo total; a próxima investigação deve atribuir CPU das demais
threads e os picos (JIT/GC/driver são hipóteses, não causas demonstradas).

Rodadas preliminares preservadas: debug (`poc-a-profile-debug-60s-20260828/`)
teve média CPU 4,486 ms/máximo 23,395 ms; primeiro nativo otimizado
(`poc-a-profile-release-60s-20260828/`) teve 4,000/14,411 ms. São capturas
sequenciais com variação de estado do aparelho, não um A/B controlado que prove
ganho de compilação. O smoke curto está em `poc-a-profile-smoke-20260828/`.
O opt-out foi confirmado em `poc-a-profile-optout-20260828/`: 1.000 frames e
retomada sem nenhuma janela `[FrameProfile]`; o APK debug ficou instalado com
a coleta desligada e os dados do aplicativo preservados.
