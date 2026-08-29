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
resolução, temperaturas/status térmico inicial/final, distribuições de janelas
e timestamps de apresentação. O build agora publica C# Release framework-dependent
para `linux-bionic-arm64` automaticamente. Não usa a DLL legada de assets.
`measurements.managedBuild` registra SHA-256 da DLL no APK e compara o build ID
do APK com o confirmado pelo processo. `optimized` continua sendo o rótulo nativo;
a configuração C# é definida explicitamente pelo publish no Gradle.

Capturas anteriores a `m0-batch-20260828/` usaram o asset ARM64 legado (SHA-256
`53ECFE4780C8CDB63347D08D24549F7CEDEECC20ED5CD564EE32EE56BF9E95F6`). Esse detalhe
histórico permanece importante para não misturar binários diferentes.

## Critérios e limitações

O gate de CPU usa conservadoramente **máximo < 3 ms**, pois o plano não define
percentil; média e p95/p99 continuam visíveis. `accepted` permanece falso enquanto
houver critérios/matriz não atendidos. Não exportamos esta cena como se fosse
`cpu.editor_frame_time_ms` do editor completo, nem preenchemos um orçamento GPU
com tempo de acquire. O FrameProfile v3 registra timestamps GPU do frame e
checkpoints Geometry/Background/Transparent. Em TBDR, checkpoints dentro do mesmo
render pass podem ser resolvidos no fim do tile e não substituem uma captura AGI;
a correlação exata dos picos com threads/stacks também permanece pendente. A
amostragem Simpleperf abaixo localiza custo agregado, mas
não prova a causa de todos os picos. Um minuto não fecha o soak de 30 minutos,
e um aparelho não fecha a matriz Mali/perfil C.

Testes: 11 C++ para estatísticas, 5 C++ para extração atômica/build ID,
6 C# para equivalência/ABI/zero alocação do workload, 19 PowerShell para frames,
13 para térmica/FPS de janela e 4 verificações dos assets gerados após build.

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

## Rodada integrada de 28/08: diagnóstico, correção e validação

Evidências em `build/android-validation/m0-batch-20260828/`.

### Atribuição antes da correção

Simpleperf do Android, `task-clock:u`, 500 Hz, 20 s, app debug: 1.310 amostras,
nenhuma perdida. `cpu-before.data`, `cpu-before-threads.txt` e
`cpu-before-symbols.txt` conservam a captura e as reduções.
Thread de render (`Thread-13`): 80,53% das amostras; Binder: 17,63%; BLAST: 1,83%.
`sinf` e `cosf` somaram 19,54%; libm completa, 22,21%. Há amostras sem símbolos
(possivelmente JIT, não identificação comprovada). Não há evidência suficiente
para culpar GC nem justificar migrar esse cálculo para C++.
São porcentagens de **CPU de usuário amostrada**, não do relógio total do processo
nem atribuição individual dos spikes.

O [Simpleperf oficial](https://android.googlesource.com/platform/system/extras/+/android16-release/simpleperf/doc/android_application_profiling.md)
requer app debuggable/profileable. O coletor não muda essa permissão nem faz root:

```powershell
# App debug aberto/desbloqueado; executar separado do benchmark térmico.
.\tools\profile-android-cpu.ps1 -DurationSeconds 20 -OutputDirectory build/android-cpu/minha-captura
```

### Correções e comparação curta

- `InstancingWorkload` prepara uma vez posições-base e cores: cache fixo de
  100.000 bytes para a fixture de 5.000 instâncias. Seno/cosseno da animação
  continuam por frame, produzindo os mesmos floats da referência. Crossing
  único, sem alocação estável; outros counts mantêm fallback sem cache.
  O caso unitário agora é finito/centrado.
- Gradle gera assets em `build/generated/aetherAssets`, publica C# e identifica
  conteúdo com manifesto/build ID. A extração não confunde DLLs de tamanho igual:
  substitui arquivos atomicamente e confirma a geração por último. Interrupções
  deixam o marcador inválido e a próxima inicialização refaz a extração.
- Runner v4 coleta CPU, compositor e energia na mesma rodada, conserva janelas
  mesmo após rotação do buffer Logcat e detecta lacunas na apresentação.

| Captura (release nativo + C# Release) | CPU média | Render thread média | Interop wall média | CPU máxima |
|---|---:|---:|---:|---:|
| `baseline/` | 2,899 ms | 1,813 ms | 1,192 ms | 19,342 ms |
| `optimized-smoke/` | 2,412 ms | 1,217 ms | 0,537 ms | 18,237 ms |

Smoke: 60,039 FPS exibidos/61,193 s; mínimo 59 em janelas móveis de 1 s.
Potência amostrada média 2,273 W, pico 4,316 W, status térmico máximo 0;
duas retomadas passaram. Capturas sequenciais no mesmo aparelho, não ensaio
controlado de DVFS/temperatura: não garantem ganho universal de 55% no interop
ou 17% no processo. O orçamento CPU máximo < 3 ms continua aberto.

### Soak contínuo e critérios

```powershell
.\tools\validate-android-shell.ps1 -ApkPath build/poc-a-profile-release.apk -ProfileSeconds 60 -SoakMinutes 30 -RequireSoakBudget -LifecycleCycles 2 -ExerciseConfigurationChange -PreserveAppData -AllowScreenshotDifference
```

`ProfileSeconds` e `SoakMinutes` usam a maior duração, não somam duas esperas.
O FPS mínimo usa janelas móveis de 1 s sobre timestamps reais; uma média de
60 FPS não esconde stalls. Potência usa integração trapezoidal por tempo
monotônico, com amostragem aproximadamente a cada 5 s. Lacunas > 15 s, sensores
ausentes, NaN e alimentação externa invalidam a média. É estimativa da bateria
do aparelho inteiro, não potência GPU/app isolada nem medição de rail.
Status térmico ausente não vira zero; amostragem não exclui throttle entre leituras.

`-RequireSoakBudget` exige >= 30 min, mínimo >= 55 FPS (`MinimumSoakFps`),
média < 4 W (`PowerBudgetWatts`), cobertura contínua e nenhum aviso térmico
observado. `deviceCriteriaPassed` não fecha M0: `accepted` continua falso pela
matriz/demais PoCs. `-RequirePowerBudget` exige somente energia e requer soak > 0.

A janela opt-in usa `KEEP_SCREEN_ON` em primeiro plano. A inspeção encontrou
timeout de tela de 10 min; a primeira tentativa foi interrompida e documentada
em `soak-30min/INTERRUPTED.md`. Nenhuma senha, frequência ou configuração global
de timeout foi alterada. O flag não desbloqueia o telefone nem impede bloqueio
manual.

O usuário solicitou encerrar antes de 30 minutos. O processo do runner foi
interrompido; o app foi reiniciado sem profiling/KEEP_SCREEN_ON e o timeout global
permaneceu 600.000 ms. Evidência recuperada:
`soak-30min-keepscreen/interrupted-report.json`, PID 3599, APK SHA-256
`D4F942144D16E399386DA0D1FCFD503F9A139BDA065DB2B3FF9FDD69CB750CD3`.
O Logcat preservou as janelas 8–84: 46.200 intervalos/769,373 s, CPU média
2,456 ms/máximo 9,577 ms, interop wall médio 0,586 ms. Taxa de envios Vulkan
60,049/s; **não** é FPS exibido. As janelas iniciais já haviam saído do buffer,
logo esse máximo não representa a sessão inteira. Maior intervalo wall de
133,004 ms indica espera/atraso que não deve ser confundido com CPU.

A interrupção perdeu a série longa de potência/SurfaceFlinger mantida em memória,
portanto não há relatório térmico/FPS contínuo válido desse período. O smoke
curto anterior continua sendo a evidência completa dessas métricas.
O runner foi corrigido para persistir incrementalmente `frame-windows.jsonl`,
`power-samples.jsonl` e `surface-batches.jsonl`, com `capture-context.json`:
interrupções futuras preservam registros completos já escritos, sem depender de
`finally`/relatório final. Não mistura arquivos de outra captura no mesmo diretório.
O smoke final `incremental-evidence-smoke/` passou com uma retomada e confirmou
os arquivos incrementais de CPU/compositor: 60,056 FPS exibidos em 6,977 s,
CPU média 2,260 ms/máximo 4,059 ms. Não incluiu soak nem valida persistência
de potência em hardware; essa parte permanece coberta pelos testes do coletor.
Ao terminar, o app foi novamente iniciado no modo normal, sem extras de profiling.
O teste formal de 30 minutos permanece aberto por decisão do usuário; M0 não foi fechado.
