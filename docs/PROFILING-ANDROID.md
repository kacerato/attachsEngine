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
se sobrepor ou tocar a fronteira dentro da cadência observada: perda de cobertura
invalida a série, não vira queda de desempenho inventada. A consulta roda em job
dedicado porque o ring do FrameTracker pode girar enquanto o processo principal
coleta logcat/potência via ADB. Layer ausente/ambígua registra indisponibilidade,
sem substituir a série do compositor por envios Vulkan.

`actualPresentTime` mede quando cada frame novo acompanhado para a layer ficou visível;
o FrameTracker avança quando a layer precisa de latência para um buffer latched, não em
cada varredura repetida do painel. A janela dessa série, porém, é independente das
janelas fechadas de 600 frames do runtime. Taxas de durações diferentes não devem ser
subtraídas como se fossem o mesmo intervalo; o diagnóstico usa em conjunto presents,
timestamps GPU, frames visíveis e modo físico ativo.

A série do compositor e as janelas de CPU têm intervalos próprios, sobrepostos,
cada um com duração registrada; não servem para atribuir um spike individual ao
mesmo frame. O clock do processo/thread segue o [contrato POSIX](https://pubs.opengroup.org/onlinepubs/000095399/functions/clock_getres.html).

## Execução reproduzível

```powershell
.\android\gradlew.bat -p android :app:assembleDebug :app:assembleRelease :app:lintDebug --offline
.\tools\validate-android-shell.ps1 -Scene dirt-road -ProfileSeconds 60 -LifecycleCycles 0 -AllowScreenshotDifference -PreserveAppData
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
e timestamps de apresentação. Para cada PID/epoch, o runtime emite separadamente
`FrameProfileContext` schema 1 com sceneId autoritativo, fingerprint do pacote,
câmera/lock, alvo de FPS e contagens de conteúdo. O host persiste esse contrato em
`frame-contexts.jsonl`; a captura schema 2 falha se o contexto faltar, divergir da
cena solicitada ou mudar silenciosamente. O build agora publica C# Release framework-dependent
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
6 C# para equivalência/ABI/zero alocação do workload, 25 PowerShell para frames,
13 para térmica/FPS de janela e 4 verificações dos assets gerados após build.

Para regressão sintética no Android Studio, o perfil versionado não pretende emular
a GPU nem a térmica do A32:

```powershell
.\tools\ensure-android-performance-avd.ps1 -PlanOnly
# Após instalar system-images;android-35;google_apis;arm64-v8a pelo SDK Manager:
.\tools\ensure-android-performance-avd.ps1 -Launch
```

O APK/runtime atuais são ARM64-only. Em host x64, WHPX não acelera essa ABI; o plano
do script expõe `cpuAccelerationExpected=false`. Criar um AVD x86_64 sem antes portar
NDK + CoreCLR só produziria um ambiente no qual o APK não instala.

## Resultado no Xiaomi SM8735/Adreno, Android 16

### Cena real da floresta, Release e 120 Hz — 29/08/2026

Captura longa:
`build/android-validation/dirt-road-close-release-120hz-60s-20260829/report.json`.
O contexto autoritativo foi `dirt-road`, fingerprint `dd907ec34bbebc21`, câmera
travada `0,160,-100,0,0.08`, 2772×1280, 27 draws, 26 materiais, 70 texturas e
341.109 triângulos. O Android confirmou `Surface.setFrameRate(120)` e modo físico
ativo de 120 Hz.

| Métrica | Resultado |
|---|---:|
| Presents concluídos | 78,124/s; pior janela de 600 frames 73,431/s |
| CPU do processo | 1,408 ms média; 2,643 ms no pior p95; 10,806 ms máximo |
| Thread de render | 0,849 ms média; 1,356 ms no pior p95 |
| GPU do frame/geometria | 11,396 ms média; 18,409 ms no pior p95; 20,810 ms máximo |
| Acquire/fence | 12,007 ms média; 19,654 ms no pior p95 |
| Temperatura/status | 38,3→40,0 °C; status 0 nas pontas; aparelho alimentado externamente |

A CPU possui folga; a GPU e sua variância dominam. O p95 de 18,409 ms não cabe no
orçamento de 8,33 ms de 120 Hz e ainda excede o alvo confortável de 60 Hz. A série
SurfaceFlinger dessa captura foi invalidada porque o coletor sequencial perdeu a
janela circular; ela não é usada para alegar FPS exibido.

Após mover a leitura para o job dedicado, o smoke
`dirt-road-close-release-120hz-display-fix-v5-20260829/` obteve 1.710 eventos
`actualPresentTime` em 16,784 s contínuos (101,82/s) e 2.400 presents da engine em
27,330 s (87,82/s). As janelas têm início/fim diferentes e atravessam estados de carga
distintos; essa diferença não mede descarte nem duplicação. O smoke valida a cobertura do coletor, não substitui o
baseline longo nem fecha um gate de 120 FPS. O parser ADB também aceita o sufixo mDNS
com espaço que o Android adiciona ao republicar um serviço duplicado.

### A/B de backface culling — rejeitado em 29/08/2026

Foi testada uma política global, sem consulta a nome de cena ou aparelho: opacos e
blends single-sided usavam backface culling com winding corrigido pela handedness da
transform; `doubleSided` e `MASK` AEMAP v1 permaneciam dupla face. A exceção conservadora
para `MASK` reduziu a perda da primeira variante, mas não fechou o gate visual. O runner
marcou cada execução como estável antes/depois; isso não significa igualdade entre APKs.
Na comparação cruzada, os hashes diferiram, 334.281 de 3.548.160 pixels (9,42%) mudaram,
o erro absoluto médio RGB foi 2,536 e ainda havia remoção visível de terreno/folhagem.

O par consecutivo, mesma cena/fingerprint/câmera/resolução/alvo e `thermalStatus=0`, foi:

| Variante | Engine | GPU média | GPU pior p95 | CPU média | SurfaceFlinger | Temperatura |
|---|---:|---:|---:|---:|---:|---:|
| baseline sem culling | 97,463 presents/s | 9,035 ms | 9,891 ms | 1,244 ms | 92,047/s | 33,6→33,6 °C |
| culling conservador | 86,339 presents/s | 9,983 ms | 13,652 ms | 1,548 ms | 88,413/s | 34,2→34,2 °C |

Relatórios: `dirt-road-close-release-ab-baseline-120hz-20260829/report.json` e
`dirt-road-close-release-ab-culling-120hz-20260829/report.json`. Um smoke anterior da
variante marcou 90,264 presents/s e GPU média 9,744 ms, confirmando que uma rodada curta
isolada não basta. Como imagem e caminho crítico pioraram, a implementação foi removida.
A próxima tentativa exige atribuição AGI e semântica de cobertura versionada no AEMAP;
estes dados permanecem como regressão negativa, não como benchmark de uma feature integrada.

### Isolamento compilado de custo GPU — 30/08/2026

O runner aceita `-GpuIsolation full|no-normal|no-ibl|base-color` exclusivamente para
diagnóstico. O runtime valida o valor, publica o modo no `FrameProfileContext` e mantém
`full` como fallback/default. O modo é uma specialization constant do fragment shader
na criação do pipeline Vulkan: o compilador pode eliminar o caminho isolado e o passe
de cobertura, que usa outro shader, não recebe uma constante inexistente. Nenhum desses
modos é Project Setting, preset de qualidade ou condição por cena.

A primeira matriz usava branch uniforme em push constant. Ela validou automação e
identidade, mas não permite atribuição fina: o controle `full` caiu de 90,808 para
70,786 presents/s no começo/fim da sequência, sem troca de APK, cena ou câmera. Os
resultados intermediários `no-normal`/`no-ibl` não são tratados como custo dessas
features.

Após converter os modos em variantes compiladas, foi executado o A/B intercalado no
mesmo APK Release, Xiaomi SM8735/Adreno, 2772×1280, 120 Hz, cena/fingerprint
`dirt-road`/`dd907ec34bbebc21` e pose `0,160,-100,0,0.08`:

| Execução | Engine | GPU média | pior p95 GPU | CPU média | SurfaceFlinger | Temperatura |
|---|---:|---:|---:|---:|---:|---:|
| `full-a` | 92,148/s | 9,715 ms | 9,943 ms | 1,130 ms | 93,446/s | 35,0→36,0 °C |
| `base-color` | 86,937/s | 10,080 ms | 12,660 ms | 1,510 ms | 88,507/s | 36,3→36,3 °C |
| `full-b` | 82,124/s | 11,040 ms | 12,770 ms | 1,130 ms | 91,612/s | 37,0→37,0 °C |

Todas tiveram `thermalStatus=0`, 27 draws, 341.109 triângulos e o mesmo fingerprint.
`base-color` preserva geometria, depth, base texture e cobertura alpha, retirando normal,
MR, iluminação, IBL, emissivo e tonemap; como ela não superou os controles, não há base
para degradar materiais nem priorizar micro-otimização PBR. A diferença entre os dois
controles também impede uma porcentagem causal precisa. A próxima captura deve separar
vertex/raster/tiles, bandwidth e espera de apresentação com AGI/Android Performance
Analyzer. Relatórios: `gpu-specialized-full-a-120hz-20260830/`,
`gpu-specialized-base-color-120hz-20260830/` e
`gpu-specialized-full-b-120hz-20260830/`.

### AEMAP v2 — compactação de vértices, 30/08/2026

Como o fragment mínimo não trouxe ganho e os 27 bounds grosseiros continuaram todos
visíveis, o próximo A/B preservou conteúdo e reduziu tráfego de vertex input. O AEMAP
v2 usa 48 bytes/vértice em vez de 72: posição/UV float32, normal/tangente SNORM16 e cor
UNORM8. O runtime continua decodificando v1; não há flag de cena ou preset reduzido.

O asset caiu de 34.688.380 para 24.491.996 bytes e o APK de 402.443.803 para
392.244.763 bytes. A comparação com o último screenshot v1, mesma câmera e resolução,
teve 523/3.548.160 pixels diferentes (0,0147%), máximo 1/255, p99 zero e erro RGB médio
0,000049.

| Pacote/run | Engine | pior janela | GPU média | pior p95 GPU | CPU média | SurfaceFlinger | Temperatura |
|---|---:|---:|---:|---:|---:|---:|---:|
| v1 `full-a` | 92,148/s | 90,828/s | 9,715 ms | 9,943 ms | 1,130 ms | 93,446/s | 35,0→36,0 °C |
| v1 `full-b` | 82,124/s | 72,140/s | 11,040 ms | 12,770 ms | 1,130 ms | 91,612/s | 37,0→37,0 °C |
| v2 `full` | 99,395/s | 88,946/s | 8,887 ms | 10,011 ms | 1,299 ms | 110,202/s | 36,3→36,3 °C |
| v2 `repeat` | 117,294/s | 116,356/s | 7,420 ms | 8,147 ms | 1,444 ms | 111,855/s | 37,1→37,1 °C |

Todos usaram 120 Hz, 2772×1280, 341.109 triângulos e `thermalStatus=0`. O fingerprint
mudou de `dd907ec34bbebc21` para `18faf0f1d9d8ee90`, como deve ocorrer quando os bytes
do asset mudam. As faixas melhoraram sem perder detalhe, mas as janelas Engine e
SurfaceFlinger são independentes e ainda há variação entre runs; fechar com sequência
A/B longa, soak e Mali antes de promover o resultado a gate multi-hardware.

Relatórios: `aemap-v2-full-120hz-20260830/` e
`aemap-v2-full-repeat-120hz-20260830/`. APK SHA-256:
`8DC0ACE8559EAFA5F70BAE950EDA233119DBE109117D7D4DE9148F26E9BCC375`.

### PoC-A de 5.000 cubos — 28/08/2026

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
