# CPU e apresentação da PoC-A

Instrumentação opt-in da cena de 5.000 cubos texturizados, em 28/08/2026.
O critério do plano principal é 60 FPS com CPU abaixo de 3 ms. Sucesso da
**coleta** não significa aprovação desse orçamento.

## Contrato da medição

- `native/profiler/frame_statistics.*` é portátil, sem Android/Vulkan, com
  armazenamento fixo para 600 frames × 14 métricas (~67 KB), sem alocação no heap
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

### Pressão das trilhas e escala dinâmica (schema 5)

Cada janela schema 5 exige um terceiro registro, `[FrameProfilePressure]`, pareado
por `pid/epoch/window`. Ele classifica o p95 como `within-budget`, `cpu`, `gpu`,
`mixed`, `presentation` ou `unknown`, publica as razões p95/budget e registra
`render_scale_min/max/end`. A captura recusa janela schema 5 sem esse par.

A classificação não tenta igualar porcentagens de CPU e GPU. Os dois processadores
trabalham em pipeline; CPU com folga enquanto a GPU executa é normal. Em 120 Hz, o
intervalo é 8,33 ms e a política atual reserva 6,50 ms para a trilha CPU e 7,33 ms
para GPU. Há pressão quando o p95 consome a margem de uma dessas trilhas. Espera de
acquire/present só vira `presentation` quando CPU e GPU estão abaixo de seus budgets
e o próprio intervalo está atrasado; VSYNC normal não vira um falso gargalo.

Para CPU, a decisão usa `thread_cpu_ms`, que representa a trilha crítica atual do
loop nativo. `process_cpu_ms` permanece diagnóstico agregado: com workers paralelos
ele pode exceder wall time e não pode ser comparado como se fosse uma única trilha.
Sem timestamp GPU válido o veredito é `unknown`, nunca uma inferência por utilização.

O ADPF Performance Hint usa a mesma disciplina: como a sessão atual contém apenas a
render thread, `reportActualWorkDuration` recebe `CLOCK_THREAD_CPUTIME_ID`. O wall time
do frame inclui bloqueio em acquire/present e reportá-lo como CPU faria o Android
tentar corrigir com clock de CPU um atraso que pode ser inteiramente da GPU.

Na validação física Release de 01/09/2026, a pose fixa da floresta em 120 Hz produziu
115,92 eventos exibidos/s e foi classificada `gpu`: GPU p95 7,335/7,333 ms, CPU da
render thread p95 1,397/6,50 ms e escala dinâmica no piso 0,58. O log confirmou ADPF
ativo com alvo 8,333 ms, e `cmd game list-modes` confirmou o modo atual
`performance`; não havia intervenção OEM configurada para o pacote.

### Regiões de GPU do frame (schema 5; leitura retrocompatível do schema 4)

O tempo de GPU é medido em oito regiões declaradas uma única vez em
`native/core/gpu_pass_class.h`, na ordem em que o frame as grava: **Opaque**
(geometria sólida, incluindo os binds que a precedem), **Coverage** (prepass e
shade de alpha-mask — a vegetação), **Sky**, **Transparent**, **UI** (HUD) e
**HZB** (cadeia de redução Hi-Z, fora do render pass principal), além de **Shadow**
e **Post**, que possuem passes próprios.

Cada região é gravada como marcador de debug **e** timestamp, sempre em par. Os
dois falham de formas opostas: um marcador sem métrica dá uma captura AGI que não
fecha com o relatório, e uma métrica sem marcador dá um número que a captura não
explica. Um marcador não depende de `aether.profile_frames` — captura acontece
fora de uma sessão de profiling — mas é no-op sem `VK_EXT_debug_utils`.

Uma região sem trabalho no frame vale **zero**, não desaparece: zero é o dado
"não custou nada nesta pose"; ausência não é dado nenhum. Regiões não marcadas por
um renderer simples (cubo, material preview) são preenchidas no mesmo ponto, de
modo que ficam zeradas em vez de somarem seu tempo à região seguinte.

Em GPU móvel TBDR, checkpoints dentro de um mesmo render pass podem ser resolvidos
no fim do tile: as regiões localizam custo e servem de âncora para a captura, mas
não substituem AGI/APA na atribuição de tiler, bandwidth e overdraw.

As regiões viajam num registro `[FrameProfilePasses]` separado da janela
`[FrameProfile]`, pareado por `pid/epoch/window`. O motivo é o limite de ~1023
bytes por entrada do Logcat, que trunca em silêncio: a janela já ocupava 937 bytes
com três passes. O consumidor exige o par — uma janela órfã é recusada, porque
aceitá-la atribuiria 0 ms a opaco/folhagem/céu num relatório de aparência válida —
e recusa regiões cuja soma exceda o tempo total de GPU do frame.

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
.\tools\validate-android-shell.ps1 -Scene dirt-road -ProfileSeconds 60 -LifecycleCycles 0 -CameraRouteMode Record -AllowScreenshotDifference -PreserveAppData
.\tests\tools\test-android-frame-profile.ps1
```

O runner lança a Activity com `--ez aether.profile_frames true`. Sem essa opção,
não lê relógios CPU por frame nem publica janelas. Não altera senha, frequência,
DVFS, governadores ou configurações térmicas; mantenha a tela desbloqueada.
Para `dirt-road`, uma coleta exige `-CameraPose` explícita ou
`-CameraRouteMode Record/Replay`; o runner recusa usar silenciosamente a câmera
de overview fora do mapa. Replay preserva o mesmo fingerprint e cada janela
registra o ordinal da rota, draws e triângulos visíveis no seu fechamento.
`-DisableCoveragePrepass` existe somente para A/B de diagnóstico, é registrado em
`configuration.coveragePrepassEnabled` e não altera o default do produto.
A pose `-15.71,145.27,-25.72,2.75,0.11` corresponde ao fechamento aproximado da
pior janela observada perto do frame 1734 de `forest-walk-v1`; ela é o ponto fixo
para atribuição shader/descriptor, enquanto a rota inteira continua sendo o gate de produto.

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
`FrameProfileContext` schema 4 com sceneId autoritativo, fingerprint do pacote,
câmera/lock, alvo de FPS, contagens de conteúdo, escala/extensão interna efetiva e
budgets resolvidos de LOD sólido/coverage. O host persiste esse contrato em
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

### Precisão dos varyings — mantida sem ganho declarado, 31/08/2026

Continuação do item 2.2.5 sobre o que sobrou em `highp` depois do shading: as
saídas do vertex shader. Cada varying é gravado na memória de tile e
reinterpolado por fragmento, e são 12 floats por pixel entre normal, tangente,
cor e dither — em teoria, tráfego que cai pela metade em fp16.

`vNormal`, `vTangent`, `vColor` e `vDither` passaram a `mediump` **nos dois
estágios** (vertex e os dois includes de fragmento que os consomem: shading e
coverage). `vPosition` e as UV permaneceram `highp` — a primeira alimenta o vetor
de visão em coordenadas de mundo, as segundas endereçam texturas de até 4096 px,
onde a mantissa do fp16 já não resolve um texel. Toda a cadeia de posição/view/
clip do vertex ficou com `highp` explícito: fp16 ali produz tremor de vértice e
z-fighting.

O SPIR-V confirma que a interface casa: `vColor`, `vDither`, `vNormal` e
`vTangent` recebem `RelaxedPrecision` nos dois estágios; `vPosition` e as UV, não.

**Gate de imagem:** máximo de **1/255**, zero pixels acima — e a divergência
total contra fp32 caiu de 7,06% para **5,72%** dos pixels, ou seja, ficou
*mais próxima* da referência que a fatia anterior.

**A/B intercalado, com a primeira execução após cada `adb install` descartada
como aquecimento:**

| rodada | variante | GPU média |
|---|---|---:|
| w1 | só shading | 8,678 ms |
| w2 | shading + varyings | 8,638 ms |
| w3 | só shading | 9,039 ms |
| w4 | shading + varyings | 8,876 ms |

A diferença entre variantes (0,10 ms) é **menor que o espalhamento dentro de cada
variante** (0,36 ms e 0,24 ms). **Nenhum ganho é declarado.** A mudança permanece
porque é semanticamente correta — declara explicitamente uma interface que antes
dependia do padrão implícito `highp` — e porque o gate de imagem melhorou; é o
mesmo tratamento dado à remoção do `nonuniformEXT` em 31/08.

**Piso de ruído desta bancada, medido:** repetições da mesma build variaram
8,458–9,142 ms (~8%), e 8,638–9,039 ms (~4,6%) já com aquecimento descartado.
Uma primeira execução logo após `adb install` chegou a 9,282 ms contra 8,618 ms
da mesma build minutos depois. Consequência prática para os próximos ciclos:
**efeito abaixo de ~0,4 ms não é distinguível em duas rodadas** e exige mais
repetições ou pose fixa. Os dois ganhos aceitos até aqui (0,44 e 0,61 ms) estão
acima desse piso; este não está.

### Precisão explícita no PBR — A/B aceito em 31/08/2026

Item 2.2.5 do plano ("biblioteca de shaders base com precisão explícita"), que
estava marcado *parcial* e na prática nunca tinha sido puxado: o fragmento do mapa
não tinha **um único** qualificador de precisão, então tudo rodava em `highp`
(fp32) por omissão. Em Adreno/Mali a ALU fp16 roda ao dobro da taxa e ocupa metade
dos registradores — e registrador livre vira wave em voo, que é o que esconde
latência de textura.

**A divisão não é "tudo mediump".** Cor, normal, tangente e parâmetros de material
vivem em [0,1] e cabem folgados em fp16. A numérica do GGX não: `rough` é limitado
a 0,07, logo `alpha*alpha` vale 2,4e-5, **abaixo do menor normal do fp16**
(6,1e-5). Em mediump esse termo vira subnormal ou zero e o brilho especular
desaparece justamente nas superfícies mais lisas. Por isso `distribution`,
`visibility` e os produtos escalares que as alimentam permanecem `highp`. A
subtração `eye - vPosition` também fica em highp: são coordenadas de mundo num
mapa de centenas de unidades; só o resultado normalizado desce para mediump.

O SPIR-V compilado carrega **90 decorações `RelaxedPrecision`**, que é o que
autoriza o driver a executar em fp16.

**Gate de imagem, pose fixa `(-15,71; 145,27; -25,72; yaw 2,75; pitch 0,11)`:**

| medida | valor |
|---|---:|
| pixels diferentes | 250.431 / 3.548.160 (7,06%) |
| **erro máximo por canal** | **1/255** |
| pixels com erro > 1/255 | **0** |
| brilho médio | 173,927 → 173,901 |

Nenhum pixel diverge por mais de um passo de quantização. O gate passa.

**A/B na rota `forest-walk-v1`, intercalado, 60 s por rodada:**

| rodada | precisão | GPU média | apresentado |
|---|---|---:|---:|
| controle | mediump | **8,458 ms** | 100,24 fps |
| variante | highp (fp32) | **9,142 ms** | 94,17 fps |
| controle | mediump | **8,604 ms** | 99,45 fps |

Os controles reproduzem dentro de 1,7% e o fp32 bate com os controles fp32 das
sessões anteriores (9,034 / 9,065 ms). Ganho: **−0,61 ms de GPU (−6,7%)** e
**+5,7 fps (+6,1%)**, sem diferença visual admissível.

**Acumulado do programa de margem, medido:** 0,44 ms (depth memoryless) + 0,61 ms
(precisão) = **1,05 ms** dos ~2,9 ms que separavam 9,06 ms do gate de 6,20 ms.
Aproximadamente **36% do caminho**, sem tirar um pixel da cena.

### Depth memoryless — A/B aceito em 31/08/2026

Primeiro ganho **medido** do programa de margem. Aparelho `25053PC47G`/Adreno,
Release assinado, rota `forest-walk-v1`, 60 s por rodada, intercalado.

Antes de comparar tempo, a pergunta anterior: o driver **concede** o que a
política pede? `vmaGetAllocationMemoryProperties` responde por alocação, e o
Adreno concedeu — `lazily_allocated_concedido=sim`. Sem essa checagem a engine
afirmaria economia de banda a partir de uma preferência que o device poderia ter
ignorado em silêncio.

| rodada | anexo de depth | GPU média | apresentado |
|---|---|---:|---:|
| controle | transitório (memoryless) | **9,065 ms** | 95,27 fps |
| variante | render target comum | **9,491 ms** | 90,48 fps |
| controle | transitório (memoryless) | **9,034 ms** | 94,59 fps |

Os dois controles reproduzem dentro de 0,3%, então a comparação é válida.
Desligar o transitório custa **+0,44 ms de GPU (+4,8%)** e **−4,4 fps (−4,6%)**.

O ganho fica na ponta baixa da faixa hipotética de P1 (0,3–1,2 ms) e deixa de ser
hipótese. Em escala: são 0,44 ms dos ~2,9 ms que separam os 9,06 ms atuais do
gate de 6,20 ms — cerca de 15% do caminho, com uma única mudança que não altera
um pixel.

A chave `aether.disable_transient_depth` existe só para reproduzir este A/B, no
mesmo espírito de `GpuCostIsolation`: nunca é preset de qualidade.

**Pendência conhecida:** a contabilidade de budget ainda soma o tamanho virtual
da alocação (`render-target uso=14344192 bytes`) mesmo quando a memória é
LAZILY_ALLOCATED e o driver pode não comprometer nada disso. O número superestima
a residência e, por consequência, aperta sem necessidade a quota de texturas.
`vkGetDeviceMemoryCommitment` é a consulta correta.

### Cadência de apresentação e DVFS — medido em 31/08/2026

Origem: FPS percebido como instável, e "melhora" ao ligar o gravador de tela.
Aparelho `25053PC47G`/Adreno, Release assinado, rota `forest-walk-v1`
(6.611 poses, fingerprint `18faf0f1d9d8ee90`), 60 s por rodada.

**O que a média escondia.** A engine reportava 95,7 presents/s, mas a cadência que
o painel realmente entrega é quantizada em vsyncs de 8,3333 ms. Distribuição dos
intervalos do SurfaceFlinger com o voto padrão de 120 Hz:

| intervalo | equivale a | ocorrência |
|---|---:|---:|
| 1 vsync | 120 fps | 74,8% / 75,4% |
| 2 vsyncs | 60 fps | 24,3% / 23,7% |
| 3 vsyncs | 40 fps | 0,9% / 0,9% |

Um quarto dos quadros dura o dobro do anterior. A média de 95 fps é real e a
imagem ainda assim treme — é isso que o usuário percebe, e nenhuma métrica de
média o expõe. Numa medição anterior, com o aparelho em outro estado de
governador, a divisão chegou a **48,3% / 49,6%**: alternância quase perfeita
entre 120 e 60, o pior padrão possível.

**Cadência adaptativa — implementada, medida e retirada.** A hipótese era
escolher o menor múltiplo de vsync que o custo medido do frame sustenta e
segurá-lo, produzindo cadência uniforme. Implementada com histerese assimétrica
(afrouxa em 4 quadros, aperta em 90) e alimentada pelo tempo de GPU do frame,
deliberadamente não pelo intervalo entre presents — que já contém a espera
imposta pela própria política.

Ela se auto-alimentou mesmo assim, por um caminho que não estava previsto:

| rodada | cadência | GPU média | apresentado | bateria |
|---|---|---:|---:|---:|
| sem pacing | livre (120 Hz) | **8,90 ms** | 96,71 fps | 35,2→36,0 °C |
| sem pacing | livre (120 Hz) | **11,61 ms** | 76,00 fps | 34,6→34,6 °C |
| com pacing | travou em 4 vsyncs | **22,11 ms** | 30,93 fps | 32,9→33,0 °C |

O mesmo trabalho passou de 8,90 para 22,11 ms de GPU. `thermalStatus` foi **0**
nas três, e a rodada mais lenta foi a **mais fria** — o oposto do que throttling
térmico produziria. A causa é o governador: menos carga, menos clock de GPU,
frame mais caro em tempo de parede. O laço fecha sozinho — afrouxar a cadência
reduz a carga, o clock cai, o frame fica mais caro, a política afrouxa de novo —
e estabilizou em 30 fps a 33 °C.

A implementação foi **retirada**. Uma medição que é função da decisão que ela
alimenta não sustenta uma política de controle.

**Voto fixo de 60 Hz — A/B intercalado, também rejeitado.** Sem laço nenhum:
`-TargetFps 60` decidido antes da rodada. Controles de 120 reproduzem, então a
comparação é válida.

| rodada | SF fps | GPU média | intervalo dominante | pior segundo |
|---|---:|---:|---:|---:|
| 120 Hz (controle) | 95,23 | 9,06 ms | 74,8% em 1 vsync | 71 fps |
| **60 Hz** | 56,15 | **15,13 ms** | **93,0% em 2 vsyncs** | **45 fps** |
| 120 Hz (controle) | 95,67 | 9,00 ms | 75,4% em 1 vsync | 72 fps |

O voto de 60 Hz **melhora a uniformidade** (93% contra 75%) e **piora o piso**
(45 contra 71 fps no pior segundo), porque a mesma queda de clock aparece de novo
— GPU de 9,0 para 15,1 ms — e 7% dos quadros passam a estourar para 4 vsyncs, ou
seja, 30 fps. Trocar tremor de 120→60 por queda a 30 não é melhora.

**Conclusão operacional.** Nesta GPU, *qualquer* redução da cadência solicitada
custa cerca de 65% de clock, e o efeito é maior quanto mais fundo se vai
(9,0 → 15,1 → 22,1 ms). Pacing não compra estabilidade aqui: ele a vende. O voto
padrão de 120 Hz permanece a escolha correta, e a instabilidade restante — o
quarto de quadros que cai para 60 — só sai **tornando o frame mais barato**, não
redistribuindo o tempo. Isso valida numericamente o gate de 6,20 ms da seção 0.6
do plano principal: é o custo em que praticamente todo quadro cabe em 1 vsync.

**Nota sobre o gravador de tela.** `screenrecord` sobe o voto de energia do
sistema, então mais quadros alcançam o prazo de 1 vsync e a proporção fica mais
uniforme. É o mesmo mecanismo por outro lado, e não é um cap de 60 na engine.

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

### Schema 6: pressão, ADPF CPU/GPU e térmica no mesmo intervalo

`[FrameProfilePressure]` schema 6 acrescenta `adpf`, `adpf_gpu_work`, `game_mode`,
`sustained_supported`, `sustained_enabled`, `thermal_api`, `thermal_status`,
`thermal_headroom_valid`, `thermal_headroom` e `thermal_pressure`. O parser exige todos
esses campos no schema 6 e continua aceitando schemas 4/5 sem inventar medições.

Em Android 15+, `adpf_gpu_work=true` significa que a sessão usa `AWorkDuration`: o
tempo de CPU guardado para um frame é pareado no frame seguinte com o timestamp Vulkan
resolvido após a fence. O total reportado considera a maior cauda CPU/GPU, pois elas
podem se sobrepor. Sem essa API, o fallback reporta somente CPU da render thread.

Thermal Headroom é consultado no máximo a cada 10 segundos. Valor próximo de 1 indica
aproximação do throttling; valor inválido é publicado como `-1` e nunca força uma falsa
recuperação. A classificação de gargalo continua baseada nos tempos medidos, não no
percentual de utilização mostrado pelo sistema.

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

### ADPF A/B, material LOD e gravação — hotspot de 01/09/2026

O runner expõe `-AdpfTargetRatio 0.5..1.0`; zero herda o intervalo completo.
Essa opção é apenas de laboratório e viaja no `report.json`. O APK de produto usa
8,333 ms em 120 Hz. No Xiaomi/API 35 a sessão também chama publicamente
`APerformanceHint_setPreferPowerEfficiency(session, false)`, reporta o resultado e
continua enviando `AWorkDuration` com CPU/GPU. Não existe chamada para clock privado.

Com cena `dirt-road`, câmera `-19.04,143.47,-43.73,13.109,0.087`, escala fixa 0,58
e dinâmica desligada, os controles ADPF 1,0 deram 87,83 e 86,02 FPS; razão 0,88 deu
64,63 FPS. O alvo menor foi rejeitado. `coverageLodPixelErrorBudget=96` removeu cerca
de 9,4% dos triângulos submetidos (317.419→287.545), mas a GPU ficou em ~10,1 ms:
redução de geometria não resolve o fill/texture hotspot sozinha.

O isolamento compilado `no-normal` chegou a 111,90 FPS, contra 84–88 no caminho
completo sob operating point semelhante. Isso motivou material-detail LOD por bounds:
somente packets totalmente além do alcance de normal usam a variante compilada sem
normal map. O branch por distância permanece para o fade dentro do packet; a variante
remove de fato sample/TBN/registradores quando todo ele já teria peso zero.

Uma coleta com `screenrecord` temporário confirmou 120,113 presents/s, GPU média
6,401 ms, p95 7,104 ms e CPU média 1,221 ms. Sem gravação o mesmo APK variou entre
86,31 e 106,52 FPS, mesmo com status térmico 0; o R6 com preferência ADPF explícita
registrou 96,24 FPS e GPU média 8,885 ms já sob pressão térmica interna `light`.
Esses números provam que a cena cabe no budget no operating point alto, mas não que
o app controla o governador. Promoção exige runs frios/aquecidos intercalados e
Swappy como próximo experimento de frame pacing.

Artefatos principais:

- `build/adpf-hotspot-control-100*/report.json`;
- `build/adpf-hotspot-margin-088/report.json`;
- `build/hotspot-no-normal-r4/report.json`;
- `build/hotspot-material-distance-lod-r5*/report.json`;
- `build/hotspot-material-distance-adpf-r6/report.json`.

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

## Oclusão GPU-driven e decomposição do frame no hotspot — 02/09/2026

Primeiro A/B físico do consumidor C2. Xiaomi `25053PC47G`/SM8735/Adreno,
Android 16, Release assinado (`build/gpu-cull-c2-release.apk`, SHA-256
`897E460DEE346396A5E906E85810109E4807FD5D9EACACBC5536C955024FA288`), cena
`dirt-road` na pose fixa do hotspot `-15.71,145.27,-25.72,2.75,0.11`,
**1280×2772 nativo, escala 1,00**, alvo 120 Hz, 30 s por rodada,
`Thermal Status: 0` do início ao fim.

| Variante | frame ms | opaque ms | post ms | hzb ms | culling ms | presents/s |
|---|---:|---:|---:|---:|---:|---:|
| controle A | 15,270 | 14,014 | 1,252 | — | — | 56,95 |
| C1+C2 (culling GPU) | 16,530 | 13,449 | 1,482 | 1,505 | 0,090 | 54,65 |
| C1 só (produtor) | 16,240 | 13,238 | 1,439 | 1,560 | — | 52,51 |
| controle B | 15,239 | 13,927 | 1,308 | — | — | 59,62 |
| `no-ibl` | 13,138 | 11,859 | 1,274 | — | — | 76,02 |
| `base-color` | 10,839 | 8,811 | 2,020 | — | — | 90,03 |
| escala 0,75 | 11,335 | 9,932 | 1,397 | — | — | 70,93 |
| `post=none` (tonemap inline) | 21,222 | 21,207 | 0,007 | — | — | 42,85 |

Os dois controles reproduziram em **0,031 ms** de tempo de frame (15,270 e
15,239). O piso de ruído desta bancada é ~0,4 ms; a bancada estava
excepcionalmente estável nesta sessão, e todo efeito abaixo de 0,4 ms continua
não sendo distinguível.

**O kernel funciona.** `gpu_cull=ativo gpu_cull_tested=244 gpu_cull_occluded=28
gpu_cull_visible=216`, com `screenshot idêntica=True` contra o controle: a
oclusão em compute remove draws reais sem mudar a imagem.

**E mesmo assim é rejeitado nesta pose.** O produtor custa **+0,99 ms** de frame
(16,240 contra 15,255 de média dos controles) e o consumidor não devolve isso:
com culling ligado o frame é **+1,28 ms**, não menos. Ocluir 28 de 244 draws —
os pequenos e distantes — não move o passe principal o bastante para pagar a
cadeia de redução. É o mesmo veredito que a cadência adaptativa e o backface
culling por semântica já receberam: implementado, medido, não aceito. O estágio
permanece opt-in e desligado por padrão; o código fica porque a fatia seguinte
(produtor mais barato, ou candidatos maiores via HLOD) muda só um dos dois lados
da conta.

**A decomposição do opaco é o resultado que importa.** Com dois pontos de
resolução na mesma pose (13,927 ms a 1,00 e 9,932 ms a 0,75, ou seja 56,25% dos
pixels), o passe separa em:

- **custo fixo ≈ 4,8 ms** — geometria, binning e o que não escala com pixel;
- **custo por pixel ≈ 9,1 ms** a 1280×2772.

E as variantes de isolamento dividem o termo por pixel: material completo menos
`base-color` = **5,1 ms** (dos quais IBL responde por 2,1 ms), sobrando ~4,0 ms
de base color, depth e overdraw.

Isso fecha a pergunta de viabilidade com aritmética, não com opinião: 120 Hz
exige 8,33 ms de frame; nesta pose o opaco **só com base color** já custa
8,811 ms, com pós ainda por cima. **Nenhum ajuste de shading leva esta pose a
120 Hz na resolução nativa.** O caminho tem de reduzir fragmentos sombreados
(overdraw) ou o número de vezes que cada pixel é sombreado — não o custo de cada
amostra.

**O passe de pós dedicado não é custo, é economia.** Trocá-lo por tonemap inline
(`-QualityPost none`, desenhando direto na imagem do swapchain) piorou o frame de
15,24 para **21,22 ms**. A hipótese de trabalho é a pré-rotação da surface: com
alvo offscreen o tiler trabalha na orientação nativa e o pós resolve a rotação
uma vez. Ainda **não** está provado — exige captura AGI antes de virar regra —
mas remover o pós "para economizar" está medido como errado neste aparelho.

Capabilities relevantes deste device, do próprio log: `vrs=0` e `memoryless=0`.
VRS não é opção aqui, e `LAZILY_ALLOCATED` nunca é concedido — a política P1 de
depth memoryless continua correta, mas não tem efeito neste hardware.

Reprodução:

```powershell
.\tools\validate-android-shell.ps1 -ApkPath build/gpu-cull-c2-release.apk -Scene dirt-road `
  -CameraPose "-15.71,145.27,-25.72,2.75,0.11" -ProfileSeconds 30 -LifecycleCycles 0 `
  -TargetFps 120 -EnableHzbGpuCulling -AllowScreenshotDifference -PreserveAppData `
  -OutputDirectory build/android-validation/c2-pose-gpucull
```

`-EnableHzbCompute` liga só o produtor e `-EnableHzbGpuCulling` liga produtor e
consumidor; os dois viajam em `configuration.hzbComputeProducerEnabled` e
`configuration.hzbGpuCullingEnabled` do relatório, para que nenhuma rodada possa
ser reinterpretada depois como se fosse a outra.
