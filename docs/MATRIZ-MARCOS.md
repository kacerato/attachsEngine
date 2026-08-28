# Matriz de marcos — inventário executável 0.1.1–9.6.7

> **Origem da exigência:** `docs/PLANO-FECHAMENTO-LACUNAS.md` §4.1 ("Inventário
> executável"). Este documento é a tabela item-a-item pedida
> ali: todo item numerado de `docs/PLANO-ENGINE-MOBILE.md` (0.1.1 até 9.6.7,
> ~328 itens) recebe uma linha própria com estado individual e evidência
> concreta — nunca um cabeçalho de faixa (ex.: "5.1–5.6") marcado como um bloco
> só.
>
> **Fonte de evidência:** `docs/ESTADO.md` é tratado como a fonte mais
> confiável — ele só registra o que compila e passa em teste, com evidência
> (contagem de testes, arquivo, benchmark, limitação conhecida). Quando
> `ESTADO.md` é omisso ou ambíguo sobre um item específico, o estado abaixo foi
> cruzado com leitura direta do código/testes do repositório. Nenhum item foi
> marcado "implementado" ou "validado em hardware" por proximidade a um item
> vizinho ou por pertencer a uma fase com avanço geral — cada linha é avaliada
> isoladamente.
>
> **Vocabulário de estado:**
>
> | Estado | Significado |
> |---|---|
> | `não iniciado` | Nenhum código/artefato relevante existe no repositório |
> | `PoC` | Existe uma prova de conceito isolada (ex.: protótipo HTML, biblioteca mínima) que não integra o produto |
> | `parcial` | Parte do escopo do item existe e tem alguma evidência, mas o item como descrito no plano não está completo |
> | `implementado` | O escopo do item compila e passa em teste (headless/host), sem validação em GPU/hardware físico quando isso é relevante ao item |
> | `validado em hardware` | Além de implementado, o comportamento foi comprovado rodando num aparelho físico real (não emulador, não headless) |
> | `aceito` | Item cuja lacuna foi formalmente aceita/documentada como decisão de escopo (ex.: decisão por ADR-like registrada em ESTADO.md), não uma pendência simples |

## Resumo por estado

| Estado | Contagem |
|---|---|
| não iniciado | 249 |
| PoC | 8 |
| parcial | 39 |
| implementado | 31 |
| validado em hardware (inclui variantes "só Android"/"Android") | 5 |
| aceito | 1 |
| **Total** | **333** |

(O total real de linhas é 333, não os ~328 itens estritamente `X.Y.Z` do plano
principal — a Etapa 0.2 tem 5 PoCs de risco identificados só como "PoC-A" a
"PoC-E" dentro do item 0.2, sem numeração própria em `X.Y.Z`; cada um recebeu
linha individual aqui por ter critério de aceite próprio no plano. "Validado
em hardware" inclui os itens 0.1.3 e 1.1.3, cuja validação cobre apenas
Android — a lacuna iOS de cada um está anotada na respectiva linha.)

Nenhum item desta matriz está marcado `implementado` ou superior sem uma
referência de evidência concreta na coluna Evidência. Cabeçalhos de fase/etapa
não recebem estado agregado — servem apenas de navegação.

---

## Fase 0 — Fundação e prova de conceito

### Etapa 0.1 — Espinha dorsal técnica

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 0.1.1 | Monorepo, build C# (.NET SDK) + CMake/Ninja nativo, CI com dispositivos físicos | parcial | ESTADO.md linha 61 (build C#+CMake/Ninja e runner de testes próprio ✅); CI com dispositivos físicos não existe — só `tools/validate-android-shell.ps1` rodado manualmente |
| 0.1.2 | Farm de dispositivos: mínimo 12 aparelhos cobrindo Adreno/Mali/PowerVR/Apple, perfis S/A/B/C | parcial | ESTADO.md "Shell Android — validação atual": 1 aparelho físico (Xiaomi SM8735/Adreno). Faltam Mali, PowerVR, Apple e perfil C — matriz mínima de 6 (§6.1 do plano de lacunas, `GAP-HW-01`) não atingida |
| 0.1.3 | Shell nativo Android (NativeActivity/GameActivity) + iOS, loop de app, surface Vulkan, ciclo de vida | validado em hardware (só Android) | Shell Android no Adreno; 100 ciclos históricos do triângulo, agora cubo/depth/touch com 3 retomadas, configuração e screen off/on em `lifecycle-keyguard-20260828-verified/`. Timeout investigado: keyguard com senha, não trava gráfica demonstrada. Runner separa desbloqueio de retomada; ver `ANDROID-SHELL.md`. iOS não iniciado |
| 0.1.4 | Integração .NET no processo nativo: carregar CoreCLR/Mono, chamar C# do C++ e vice-versa | implementado | ESTADO.md "0.1.4": `DotNetHost` hospeda CoreCLR via `hostfxr` e chama C# do C++ com sucesso, integrado ao `android_main.cpp`/Gradle, empacotado no APK de produção (assets + jniLibs), validado em hardware real (Xiaomi SM8735) — log real do dispositivo confirma `CoreCLR hospedado no shell: Ping(2,3)=5` coexistindo com o shell gráfico. Binários vendorizados em `native/third_party/dotnet-runtime/` |
| 0.1.5 | Telemetria e logging desde o dia 1 | parcial | Logging via logcat usado no shell Android (ESTADO.md linhas 109-121); não há sistema de telemetria estruturado além disso |

### Etapa 0.2 — Provas de conceito de risco

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 0.2 (PoC-A) | Vulkan + .NET: 5.000 objetos a 60 fps, < 3 ms CPU | parcial | Simpleperf identificou trigonometria repetida e trabalho em Binder/BLAST. Cache sem alocação e build C# automático integrados: smoke release 60,039 FPS, CPU média 2,412 ms/pico 18,237 ms, interop wall médio 0,537 ms (antes 1,192). CPU máxima não atende ao orçamento; matriz e causa individual dos picos pendentes. Ver `PROFILING-ANDROID.md` e `m0-batch-20260828/optimized-smoke/report.json` |
| 0.2 (PoC-B) | Gestos de edição: 10 testadores completam tarefa em < 30 s | PoC | `prototype/editor.html` prova UX de gizmos/câmera (ESTADO.md linha 63, 130-132), mas nenhum teste formal com 10 usuários foi registrado |
| 0.2 (PoC-C) | Térmica: 30 min sem throttle, < 4 W | parcial | Runner integrado CPU/FPS/potência com cobertura, média temporal, KEEP_SCREEN_ON local e evidência incremental. Smoke de 28/08: 60,039 FPS, mínimo 59/1 s, 2,273 W médios, status térmico 0. Rodada longa interrompida pelo usuário; CPU parcial recuperada, sem série longa de energia/FPS válida. 30 min, sensor no PowerGovernor e matriz continuam pendentes; ver `PROFILING-ANDROID.md` |
| 0.2 (PoC-D) | Hot reload C#: editar → ver mudança em < 2 s | parcial | `HotReloadHost` e 11 testes; loader validado no Android (10 ciclos load/invoke/unload em 128 ms). A DLL foi compilada antes da medição: isso não prova editar/compilar/aplicar/ver em <2 s no aparelho. Coleta do ALC no Android e estabilidade prolongada ainda precisam de evidência; pequeno delta de heap em 10 ciclos não comprova ausência de vazamento. Pipeline completo e iOS pendentes. |
| 0.2 (PoC-E) | Compressão ASTC em GPU: < 300 ms para 4096×4096 | validado em hardware | Revalidado em 28/08 com Khronos validation ativa: 2,829844 ms GPU, corpus RGB opaco com sólidos/rampas/checker, comparação de todos os 16.777.216 texels RGBA e erro máximo 7/255. Probe total debug: 2.393,012655 ms (não confundir com GPU encode/importação). Transfer usage, barreiras, falhas de comandos, timestamps e cleanup corrigidos. `tools/validate-android-rendering.ps1` reproduz a evidência sem editar o shell. Escopo sintético, não encoder/importador de produção; ver `EXECUCAO-EDITOR-ANDROID.md`. |

### Etapa 0.3 — Pesquisa de UX

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 0.3.1 | Estudo com 20 usuários das personas P1/P2/P3 | não iniciado | Nenhum artefato de pesquisa de usuário no repositório |
| 0.3.2 | Protótipo interativo de câmera, gizmos, menu radial (Flutter/nativo, sem engine) | implementado | ESTADO.md linha 63: `prototype/editor.html` ✅; arquivos `prototype/editor.html`, `prototype/verify.mjs`, screenshots `shot-*.png` |
| 0.3.3 | Teste de usabilidade do protótipo com métricas de tempo-para-tarefa | não iniciado | Nenhum relatório de teste de usabilidade no repositório |
| 0.3.4 | Sistema de design (tokens, tipografia, ícones, hápticos) | parcial | `managed/Aether.Core/Design/`: `ColorToken`/`ColorPalette` (temas `Dark`/`Light` transcritos exatamente de `prototype/editor.html`, incluindo cores de eixo X/Y/Z conforme CONVENCOES.md §5), `SpacingScale` (progressão geométrica base-4 + dimensões estruturais citadas pelo plano — TopBar 40dp/Dock 64dp/Rail 44dp), `TypographyScale` (6 degraus nomeados cobrindo a faixa 9-19px observada no protótipo, duas famílias de fonte transcritas), `HapticVocabulary` (mapeia `HapticCue`→`HapticIntensity` para seleção/snap/duplicar/erro/confirmação/menu radial). ✅ 20 testes (`DesignTokensTests.cs`) — cobre conversão hex exata, paletas dark/light distintas com contraste básico, progressão crescente de espaçamento/tipografia, todo `HapticCue` com intensidade mapeada. Continua `parcial`: ícones vetoriais ficam de fora (protótipo usa só glifos de texto, sem pipeline SDF real — trabalho genuíno do item 3.1.1/3.1.2), e nenhuma integração de plataforma (Vibrator/UIFeedbackGenerator) existe ainda para o vocabulário háptico |

### Etapa 0.4 — Decisões arquiteturais registradas

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 0.4.1 | ADRs para linguagem, ECS vs cena, formato de arquivo, build, backend gráfico | implementado | `docs/adr/ADR-01` a `ADR-12` (mais a `ADR-013` pré-existente): as 12 decisões do Apêndice A do plano principal, cada uma com contexto, alternativas descartadas e evidência real do código quando implementada (7 das 12 têm implementação testada com paths/contagens de teste citados; 4 são registradas como decisão preventiva sem código ainda — Play separado, build em nuvem, pipeline único, menu radial de produto; ADR-03/04/06 têm divergência ou limitação documentada explicitamente). `docs/adr/README.md` indexa todas com estado |
| 0.4.2 | Especificação do IDL de fronteira C#↔C++ | implementado | `docs/idl/FORMATO-IDL.md`: formato texto próprio (sem YAML/JSON externo — zero dependência), descritivo e validado, não gerador (decisão registrada explicitamente, evita reescrever bindings já testados/validados em hardware). `docs/idl/physics.idl`, `sqlite.idl`, `transform.idl` descrevem os três bindings reais. `tests/Aether.Tests/IdlValidationTests.cs` compara cada `.idl` contra o `Native*.cs` real via reflection. ✅ 7 testes — validado empiricamente que detecta divergência (corrupção deliberada introduzida e pega pelo teste); o processo de escrita já achou 2 discrepâncias reais no código |
| 0.4.3 | Orçamentos (memória, energia, frame time) como testes automatizados | parcial | `metrics/budgets.v1.json` versiona limites de P/Invoke, alocação, CPU, GPU, memória e energia; `tools/validate-metrics-budget.ps1` valida contrato, cobertura e violações no CI, incluindo fixture negativa. Os coletores CPU/GPU/memória/energia em aparelho ainda são `device-required`, portanto o item só fica completo após produzir séries reais na matriz de hardware |

---

## Fase 1 — Núcleo da engine

### Etapa 1.1 — Camada de plataforma

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 1.1.1 | Abstração de sistema de arquivos (assets, escopo Android, iCloud/Files iOS) | parcial | `managed/Aether.Core/Platform/IFileSystem.cs`: interface por escopo nomeado (`FileSystemScope.Assets`/`PersistentData`/`Cache`), sem caminho absoluto livre. `StandardFileSystem` implementa sobre `System.IO` (raiz por escopo configurável), usada em desktop/testes e como base para a implementação Android real injetar `internalDataPath`/`externalDataPath` de `ANativeActivity` (já resolvidos nativamente em `native/platform/android/android_paths.h`, sem JNI). ✅ 19 testes (`PlatformFileSystemTests.cs`) — cobre round-trip via bytes e streams, escape via `..` bloqueado, caminho absoluto rejeitado, escrita em `Assets` lança, isolamento entre escopos, enumeração não-recursiva. Continua `parcial`: a implementação Android real (consumindo os paths nativos) e a migração de `WriteAheadLog`/persistência de `ConfigurationStore` para usar esta abstração são integração futura, fora do critério objetivo desta fatia |
| 1.1.2 | Entrada: toque multi-ponto, caneta, teclado, mouse, gamepad, sensores | parcial | `managed/Aether.Core/Input/`: `TouchPoint`/`TouchPhase` (ciclo Began→Moved/Stationary→Ended/Cancelled, delta e predição linear via `InputState.PredictPosition`), `PenInfo` (pressão/tilt/rotação/botão lateral associado por Id de toque), `KeyCode`/`KeyEvent`, `MouseState`, `GamepadState`. `InputState` agrega tudo por frame com modelo push (`PushTouch`/`PushKey`/`SetMouse`/`SetGamepad`) + `EndFrame` consolidando fases e limpando eventos do frame. ✅ 25 testes (`InputTests.cs`) — ciclo de vida completo de multi-touch, predição, caneta desassociada ao finalizar toque, zero GC na leitura de `ActiveTouches`. Continua `parcial`: nenhuma captação real de Android (`AInputEvent`)/iOS (`UITouch`)/sensores existe ainda — é tradução futura para os tipos já definidos aqui, mesma disciplina de 1.1.1 |
| 1.1.3 | Ciclo de vida robusto: pausa, retomada, perda/recriação de surface, memória baixa | validado em hardware (Android) | 7 testes C++ portáteis + 12 regressões do runner; comandos e tempos instrumentados. 100 ciclos históricos do triângulo e regressão de cubo com 3 retomadas/configuração/screen-cycle; bloqueio seguro recebe classificação própria. `SurfaceLost`/`OutOfDate` tratados; pressão real e matriz de GPUs pendentes. Ver `ANDROID-SHELL.md` |
| 1.1.4 | Janela/display: taxa variável, notch/safe area, multi-janela, display externo | parcial | `managed/Aether.Core/Platform/`: `SafeAreaInsets` (margem por borda para notch/barra de gestos, consumida futuramente pela UI do item 4.6.1), `DisplayInfo` (resolução, densidade, `RefreshRateHz` variável, `SafeAreaSize()` saturando em zero) e `WindowState` (display principal + displays externos via modelo push `SetPrimaryDisplay`/`AddOrUpdateExternalDisplay`/`RemoveDisplay` — "multi-janela" aqui é múltiplos displays simultâneos, não split-screen de processo). ✅ 23 testes (`WindowStateTests.cs`) — cobre validação de dimensão/densidade/taxa negativa, saturação de safe-area, troca de Id do principal rejeitada, display externo declarado `IsBuiltIn` rejeitado, múltiplos displays externos coexistindo. Continua `parcial`: nenhuma captação real de `Display`/`DisplayCutout` (Android) ou `UIScreen` (iOS) existe — shell Android segue landscape fixo simples |
| 1.1.5 | Energia e térmica: leitura de estado, API do PowerGovernor | parcial | `PowerGovernor` com histerese implementado e testado (ESTADO.md linha 67), mas sem leitura real de `thermalStatus` do Android integrada |

### Etapa 1.2 — Memória e concorrência

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 1.2.1 | Alocadores: arena de frame, pools por tipo, slab para chunks, alocador de rastreamento | implementado | ESTADO.md linha 65: FrameArena, PoolAllocator, NativeList/Array, MemoryBudget ✅ 22 testes (`MemoryTests.cs`); alocador de rastreamento/debug não confirmado separadamente |
| 1.2.2 | Job system com work-stealing, dependências, afinidade big.LITTLE | parcial | ESTADO.md linha 66: job system com work-stealing e dependências + `GAP-JOB-01` fechado (grafo de ciclos) ✅ 13 testes; afinidade big.LITTLE não implementada (não mencionada em nenhum teste) |
| 1.2.3 | Primitivas sem trava (filas SPSC/MPMC, contadores atômicos) | implementado | `managed/Aether.Core/Concurrency/`: `AtomicCounter` (wrapper sobre `Interlocked`) e `SpscRingBuffer<T>` (fila circular SPSC wait-free sobre `NativeArray<T>`). ✅ 12 testes (`ConcurrencyTests.cs`) — inclui teste de concorrência real (produtor/consumidor em threads separadas, 200k itens) e teste de atomicidade real (8 threads concorrentes). MPMC genérico continua sendo `ConcurrentQueue<T>` do BCL (já em uso em `JobSystem`), não reimplementado — SPSC é o caso mais específico que valia primitiva própria |
| 1.2.4 | Instrumentação: rastreador de alocações por subsistema, visualizador de jobs | implementado | `managed/Aether.Core/Diagnostics/AllocationTracker.cs` + `managed/Aether.Core/Jobs/JobDiagnostics.cs`: mede alocação real por thread, `JobSystem.Schedule` aceita `label` de subsistema e instrumenta cada execução (duração + alocação + falha), snapshot via `JobSystem.Diagnostics` (buffer circular + totais agregados por label). ✅ 13 testes (`DiagnosticsTests.cs`) |
| 1.2.5 | Analisador Roslyn `[NoAlloc]` | implementado | `managed/Aether.Analyzers/NoAllocAnalyzer.cs` (projeto novo, referencia Roslyn direto do SDK via `$(MSBuildToolsPath)`, sem NuGet) + `Diagnostics/NoAllocAttribute.cs`. 5 regras (AETH001-005: new de tipo referência, lambda capturante, LINQ, concatenação de string, foreach sobre interface) rodando em tempo de build sobre `Aether.Core` via `ProjectReference OutputItemType="Analyzer"`. Validado com build real (arquivo probe descartável): as 5 regras dispararam nas linhas exatas esperadas, sem falso positivo em código limpo equivalente (incluindo `foreach` sobre array não acionar AETH005). Suíte completa (363 testes) segue verde |

### Etapa 1.3 — ECS

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 1.3.1 | Armazenamento por arquétipo em chunks, versionamento de componentes | implementado | Chunks SoA de 16 KB e consultas sem alocação; `World.Read/Write` e `Chunk.GetReadOnlySpan/GetWritableSpan` separam intenção e versionam somente a coluna/chunk escrito. Regressões provam ausência de dirty em leitura, incremento único e isolamento entre chunks (`GAP-ECS-01` fechado; 29 testes ECS) |
| 1.3.2 | Sistema de consultas compiladas e cacheadas, com filtros | implementado | `CompiledQuery` usa o cache versionado de arquétipos e incorpora arquétipos posteriores; filtros `With`, `Without` e `ComponentChangeFilter<T>` combinam assinatura e versão por chunk/coluna. Iteração estável zero-GC e regressões de invalidação/correção passam |
| 1.3.3 | Buffers de comando estruturais aplicados em pontos de sincronização | implementado | `EntityCommandBuffer.cs` existe; benchmark de 10 mil add/remove mediu 3/4 ms na execução completa mais recente, preservando ids/dados; integra os 29 testes ECS |
| 1.3.4 | Hierarquia como componente + propagação de transform | implementado | Hierarquia encadeada + plano topológico invalidável + backend nativo em lote/fallback gerenciado ✅ 20 testes (`HierarchyTests.cs`) |
| 1.3.5 | Fachada `Node` sobre o ECS, com API amigável | implementado | `managed/Aether.Core/ECS/Node.cs`: `readonly struct` sem ownership sobre `World`+`EntityId`, com componentes, parent/children sem alocação, geração/invalidação e proteção contra reparent entre mundos; 3 regressões em `HierarchyTests.cs` |
| 1.3.6 | Benchmark: 100k entidades com transform + hierarquia a 60 fps classe A | validado em hardware | Árvore fator 8/profundidade 7, 60 amostras, plano topológico O(N), uma chamada nativa em lote e zero GC/frame. Host p50 0,96 ms. Xiaomi SM8735: três execuções consecutivas p50 total 0,59/0,59/2,29 ms, p95 0,66/0,66/2,47 ms e p99 0,68/0,69/3,52 ms; backend `NativeBatch`. Meta <6 ms atendida mesmo na corrida afetada por DVFS; `taskset f0` fixa afinidade, não clock |

### Etapa 1.4 — Reflexão, serialização e recursos

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 1.4.1 | Source generator de metadados de componente | parcial | ESTADO.md linha 70: "registro de metadados de componente (runtime, não source generator — ver ComponentDescriptor)" — implementado via reflexão em runtime, não via Source Generator como o plano pede |
| 1.4.2 | Serializador binário + texto, com migração de versão de esquema | implementado | Texto v2 persiste `ComponentField.Id`, lê v1 e resolve aliases de campo/componente; fixtures cobrem rename/adição/remoção/compatibilidade/futuro e v1→v2→v3. Binário prova dois elos v1→v2→v3 e alias; `Joint` prova referências `EntityId`, limites e motor nos dois formatos. 22 testes (`GAP-SER-01` fechado) |
| 1.4.3 | Sistema de recursos: GUID, referência fraca/forte, carregamento assíncrono, contagem de uso | implementado | ESTADO.md linha 71: ResourceId, ResourceRef/WeakResourceRef, ResourceHandleTable, carregamento assíncrono via JobSystem ✅ 18 testes |
| 1.4.4 | Sistema de comandos de edição (undo/redo) + WAL de recuperação | implementado | ESTADO.md linha 72: UndoStack + WAL com fsync, checksum FNV-1a, recuperação parcial ✅ 16 testes |
| 1.4.5 | Índice de dependências em SQLite | parcial | SQLite 3.53.4 vendorizado (`native/third_party/sqlite/`, amálgama oficial verificado por hash SHA3-256) + binding nativo `native/resources/sqlite_bridge.h/.cpp` + binding C# `managed/Aether.Core/Resources/` (`NativeSqlite` P/Invoke cru, `SqliteConnection`/`SqliteStatement` de alto nível com conversão UTF-8 e `SqliteException`). ✅ 7 testes C++ (`test_sqlite_bridge.cpp`) + 12 testes C# (`SqliteBridgeTests.cs`, incluindo round-trip com acentuação/emoji), build nativo 119/119 e suíte C# 442/442 verdes com a DLL carregada de fato. Continua `parcial` só pelo schema de domínio do índice de dependências em si — deliberadamente adiado para a Fase 7, quando existir consumidor real (evita abstração precoce) |

### Etapa 1.5 — Matemática, tempo e utilitários

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 1.5.1 | Biblioteca math com SIMD NEON e testes de precisão | parcial | ESTADO.md linha 64: float2/3/4, quaternion, float4x4, Transform, Bounds, Ray, Plane, Frustum ✅ 27 testes (`MathTests.cs`); o kernel nativo ARM64 de composição está validado, mas SIMD NEON explícito no restante da biblioteca ainda é trabalho do próprio item 1.5.1 |
| 1.5.2 | Tempo: fixed step, interpolação, escala de tempo, pausa | implementado | `managed/Aether.Core/Time/FixedClock.cs`: `Advance(realDeltaTime)` devolve a contagem de passos fixos e mantém `InterpolationAlpha`; `TimeScale`/`Paused` distintos; teto `MaxStepsPerAdvance` com `DroppedSteps` evita espiral da morte. ✅ 20 testes (`TimeTests.cs`) — cobre drift de precisão em 10k frames, espiral da morte, pausa/retomada, `NoAlloc`. Ainda não consumido por nenhum sistema (`PhysicsSyncSystem`/`TransformSystem` continuam recebendo `deltaTime` solto do chamador) — integração é trabalho futuro, não deste item |
| 1.5.3 | Eventos e sinais tipados | implementado | `managed/Aether.Core/Events/Signal.cs`: `Signal<T>` publicador/assinante com array de slots pré-alocado e alça geracional `SignalSubscription` (mesmo padrão de `PhysicsBodyHandle`). ✅ 13 testes (`SignalTests.cs`) — cobre reciclagem de slot, auto-remoção durante a própria publicação, idempotência de `Unsubscribe`, crescimento de array. Ainda não consumido por nenhum sistema existente (física/ECS continuam com seus próprios padrões de polling, ex. `GetTriggerEvents`) — integração é trabalho futuro |
| 1.5.4 | Sistema de configuração/preferências | implementado | `managed/Aether.Core/Configuration/ConfigurationStore.cs`: dicionário chave→valor tipado (int/float/bool/string), leitura tolerante e estrita, serialização texto determinística com round-trip completo. ✅ 25 testes (`ConfigurationStoreTests.cs`). Persistência em disco (onde salvar por plataforma) pode agora usar 1.1.1 (`IFileSystem`, parcial), mas a integração em si — chamar `WriteAllBytes`/`ReadAllBytes` a partir do `ConfigurationStore` — ainda não foi feita, deliberadamente fora do escopo deste tipo, que só serializa para `string` |

> **Gate M1: não formalmente encerrado.** O desempenho de 1.3.6 possui
> evidência em aparelho (<6 ms para 100 mil transforms, zero GC), mas esse
> subgate não substitui o aceite completo de M1. Há itens de plataforma e
> metadados/recursos ainda parciais nesta própria matriz. Mantém-se o estado
> formal de `ESTADO.md`: 0/10 gates encerrados.

---

## Fase 2 — Renderizador Vulkan

### Etapa 2.1 — RHI Vulkan

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.1.1 | Inicialização: instância, dispositivo, filas, swapchain com recriação robusta | validado em hardware | ESTADO.md "Shell Android — validação atual": instance/device/surface/swapchain confirmados em hardware físico real; recriação após `OUT_OF_DATE`/surface loss tratada (linhas 210-223) |
| 2.1.2 | Alocação de memória com VMA + budgets por categoria | parcial | VMA 3.4.0 vendorizado; buffers, textura RGBA8, depth e staging reais passam por `VulkanMemoryAllocator`, com quotas e picos por categoria. Staging liberado após fence; recursos liberados antes do device. Calibração por perfil/pressão real (`VK_EXT_memory_budget`) pendente. Contrato: `RHI-RECURSOS.md` |
| 2.1.3 | Objetos: buffers, imagens, samplers, pipelines, com cache hasheado | parcial | `VulkanBuffer`, `VulkanImage`/view e `VulkanSampler` move-only integrados ao cubo Android, com upload RGBA8/staging, descritor, depth e órbita touch. 5 testes novos de descritores/upload/pré-rotação; suíte nativa 127/127. Pipeline genérico/cache hasheado e sub-recursos avançados continuam pendentes |
| 2.1.4 | Bindless via descriptor_indexing | parcial | Registro de texturas combinado com samplers integrado ao renderer e validado no Android. Quatro sub-features e seis limites verificados, sem solicitar update-unused não usado. Fallback convencional completo, com shader próprio e features realmente desabilitadas no device, também validado com Khronos ativa. Faltam tabelas globais de buffers/samplers e uso entre consumidores para o escopo integral; ver `EXECUCAO-EDITOR-ANDROID.md` e `rendering-20260828-140330/report.json`. |
| 2.1.5 | Gravação de command buffers multi-thread; timeline semaphores | não iniciado | Shell atual usa command buffer único, sem gravação multi-thread; ESTADO.md linha 144 confirma "um único frame em voo" |
| 2.1.6 | Camadas de validação, marcadores de debug, captura de frame | parcial | Camada Khronos vendorizada só em debug, callback de mensagens e marcadores integrados; commit d56c992 corrigiu negociação de bindless e semáforos por imagem. Revalidação desta execução sem VUID nos caminhos bindless/fallback e ASTC. Falta evidência de captura de frame inspecionada em RenderDoc/AGI: screenshot e logcat não substituem esse requisito. Ver `EXECUCAO-EDITOR-ANDROID.md`. |
| 2.1.7 | Detecção de capabilities e perfis de dispositivo (S/A/B/C) | parcial | Lógica pura de perfis testada; consulta real de descriptor indexing e limites alimenta o renderer com fallback validado no Adreno. Restam banco de GPUs, consulta dos demais recursos avançados e matriz física de fabricantes/perfis. Fallback forçado não equivale a hardware perfil C; ver `EXECUCAO-EDITOR-ANDROID.md`. |

### Etapa 2.2 — Compilação de shaders

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.2.1 | Pipeline Slang/HLSL → SPIR-V, com reflexão automática de bindings | não iniciado | Existe apenas a fatia GLSL → SPIR-V reprodutível (`generate-embedded-shaders.ps1`, `glslc`/`spirv-val` do NDK pinado). Slang/HLSL e reflection automática continuam sem implementação |
| 2.2.2 | Sistema de variantes com orçamento e cache em disco | não iniciado | Nenhum sistema de variantes de shader encontrado |
| 2.2.3 | Pipeline cache persistido + pré-aquecimento | não iniciado | Nenhuma persistência de pipeline cache encontrada no shell atual |
| 2.2.4 | Compilação em background com material de fallback rosa | não iniciado | Nenhum material de fallback ou compilação em background implementado |
| 2.2.5 | Biblioteca de shaders base com precisão explícita | parcial | Shaders GLSL de triângulo e cubo texturizado; geração validada e verificação `-Check` no CI Android. Não constituem ainda a biblioteca de materiais/shaders do produto |

### Etapa 2.3 — Render Graph

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.3.1 | Declaração de passes, recursos transitórios, dependências | implementado | ESTADO.md linha 75: Render graph topológico ✅ 41 testes (`test_render_graph.cpp`) — headless |
| 2.3.2 | Compilação do grafo: ordenação topológica, poda de passes mortos | implementado | Incluído nos 41 testes de render graph (ESTADO.md linha 75: "topológico... poda") |
| 2.3.3 | Inserção automática de barreiras synchronization2 | implementado | ESTADO.md linha 75: "barreiras" incluída nos 41 testes headless |
| 2.3.4 | Aliasing de memória transitória com verificação de correção | implementado | ESTADO.md linha 75: "aliasing" incluído nos 41 testes headless |
| 2.3.5 | Fusão de passes em subpasses e anexos memoryless | implementado | ESTADO.md linha 75: "memoryless, fusão de subpasses" incluído nos 41 testes headless |
| 2.3.6 | Visualizador do grafo (base do recurso de inspeção do usuário) | não iniciado | Nenhum visualizador/UI para o render graph existe; a implementação é puramente headless (ESTADO.md linha 134-135, "Device profiles e render graph... Implementação headless testada... não Execução dessas decisões numa GPU real") |

> **Nota crítica:** toda a Etapa 2.3 é testada apenas headless (CPU/lógica), nunca executada contra uma GPU real. ESTADO.md é explícito: device profiles e render graph não provam a execução dessas decisões numa GPU. A integração permanece nos itens 2.1–2.3 do plano principal; não é uma lacuna separada.

### Etapa 2.4 — Pipeline de renderização direta

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.4.1 | Depth prepass + Forward+ com clusterização de luzes | não iniciado | O cubo usa depth attachment no mesmo pass de cor. Não há depth prepass, luzes clusterizadas nem Forward+ |
| 2.4.2 | BRDF PBR completo (GGX multiscatter, Burley, Fresnel) | parcial | Esfera com GGX single-scatter, Smith correlacionado, Burley, Schlick, normal mapping e mapas 8K reais; validada em Adreno nos dois caminhos de descritores. Multiscatter e comparação quantitativa com render offline pendentes; MATERIAL-PREVIEW.md |
| 2.4.3 | Sombras: cascaded shadow maps, spot/point | não iniciado | Nenhuma implementação de sombras encontrada |
| 2.4.4 | IBL: skybox HDR, pré-filtragem especular, SH, reflection probes | parcial | Ambiente HDR analítico RGBA16F pré-filtrado GGX offline e LUT BRDF split-sum amostrados pela esfera. Skybox visual, SH e probes ainda ausentes; MATERIAL-PREVIEW.md |
| 2.4.5 | Transparência ordenada + partículas básicas | não iniciado | Nenhuma implementação de transparência/partículas encontrada |
| 2.4.6 | Pós-processamento: exposição, bloom, tonemapping, LUT, TAA | parcial | Exposição fixa/Reinhard e transferência sRGB correta no shader do sample. Sem passes de pós, AgX, bloom, LUT de grading ou TAA; LUT BRDF pertence ao IBL, não ao grading. MATERIAL-PREVIEW.md |

### Etapa 2.5 — Culling e batching

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.5.1 | BVH de cena com atualização incremental | não iniciado | Nenhuma implementação de BVH de cena encontrada |
| 2.5.2 | Frustum + occlusion culling (HZB) | não iniciado | Nenhuma implementação de culling encontrada |
| 2.5.3 | Instancing automático por malha+material | parcial | RenderSceneExtractor extrai MeshRenderer/transform em lote para Vulkan; fixtures cubo/checker e esfera/PBR, cada uma com um par conhecido. IDs persistentes, ABI e validação de matrizes; agrupamento simultâneo de múltiplos pares/GPU-driven pendente. SCENE-RENDER-INTEGRATION.md e MATERIAL-PREVIEW.md |
| 2.5.4 | Sistema de LOD com transição por dither temporal | não iniciado | Nenhuma implementação encontrada |
| 2.5.5 | Ordenação de draws por PSO e profundidade | não iniciado | Nenhuma implementação encontrada |

### Etapa 2.6 — Renderizador 2D

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 2.6.1 | Sprite batcher, atlas dinâmico, ordenação por camada | não iniciado | Nenhum renderizador 2D encontrado no repositório |
| 2.6.2 | Tilemap com chunking em GPU | não iniciado | Nenhuma implementação encontrada |
| 2.6.3 | Iluminação 2D e sombras | não iniciado | Nenhuma implementação encontrada |

> **Gate M2:** não fechado. Nenhuma cena real (Sponza-equivalente) roda no renderizador — o pipeline gráfico direto (2.4), culling/batching (2.5) e 2D (2.6) estão inteiramente não iniciados; apenas RHI básico (2.1, parcial) e Render Graph headless (2.3) existem.

---

## Fase 3 — O editor mobile

### Etapa 3.1 — Framework de UI

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.1.1 | UI retida com layout flex/constraint, virtualização, SDF | não iniciado | Nenhum framework de UI retida em C#/nativo; `prototype/editor.html` usa HTML/Canvas, fora do runtime de produto (ESTADO.md linha 132) |
| 3.1.2 | Sistema de design implementado: tokens, tema, ícones | não iniciado | 0.3.4 já entrega tokens/tema/tipografia/hápticos como dados testados em `Aether.Design`, mas nenhuma UI de produto os consome ainda — ícones vetoriais também continuam ausentes (ver 0.3.4). Este item é sobre a implementação consumindo os tokens numa UI real, não sobre a definição dos tokens em si |
| 3.1.3 | Animação e física de UI (molas, momentum, snap) | não iniciado | Nenhuma implementação encontrada fora do protótipo HTML |
| 3.1.4 | Feedback háptico com vocabulário definido | não iniciado | Nenhuma implementação de háptico encontrada |
| 3.1.5 | Acessibilidade: leitor de tela, escala, alto contraste | não iniciado | Nenhuma implementação encontrada |
| 3.1.6 | Bottom Sheet Stack com estados e navegação por gesto | não iniciado | Nenhuma implementação encontrada no runtime; conceito só existe no protótipo HTML |

### Etapa 3.2 — Reconhecimento de gestos

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.2.1 | Máquina de estados de gestos com resolução de conflito | PoC | `prototype/editor.html` implementa gestos de orbit/pan/gizmo em JS/Canvas (ESTADO.md linha 130-132), mas é um protótipo de UX, não a máquina de estados de produto sobre input real do dispositivo |
| 3.2.2 | Predição de toque para reduzir latência percebida | não iniciado | Nenhuma implementação encontrada |
| 3.2.3 | Gestos bimanuais e modificadores (segundo dedo = precisão) | PoC | Mencionado como testado no protótipo interativo (0.3.2), não no runtime de produto |
| 3.2.4 | Menu radial com seleção direcional, sub-anéis | PoC | `prototype/editor.html` (screenshot `shot-radial.png`) prova o conceito de UX; não é o runtime de produto |
| 3.2.5 | Suporte a caneta com pressão/inclinação/hover | não iniciado | Nenhuma implementação encontrada |
| 3.2.6 | Camada de atalhos para teclado/mouse externos | não iniciado | Nenhuma implementação encontrada |

### Etapa 3.3 — Viewport

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.3.1 | Controle de câmera completo com inércia e limites | parcial | Cena Vulkan com perspectiva e órbita touch, pitch limitado. CameraComponent, pan/zoom, inércia e configuração completa ainda pendentes; SCENE-RENDER-INTEGRATION.md |
| 3.3.2 | Gimbal de eixos, grade adaptativa, overlays de debug | PoC | Protótipo HTML tem gimbal básico; não é o viewport de produto |
| 3.3.3 | Gizmos touch-first com HUD numérico, anti-oclusão, lupa | PoC | ESTADO.md linha 78 (item 4.1.3) menciona gizmo de junta adicionado ao protótipo (`jointPoint`/`jointAxis`), reforçando que o gizmo vive só no protótipo HTML, não no viewport Vulkan de produto |
| 3.3.4 | Seleção: toque, laço, hierarquia, material, realce | PoC | Seleção básica existe no protótipo HTML; não no runtime real |
| 3.3.5 | Snap: grade, vértice, superfície, ângulo, háptico | não iniciado | Nenhuma implementação de snap além de gestos básicos no protótipo |
| 3.3.6 | Modos de visualização de debug (overdraw, mipmaps) | não iniciado | Nenhuma implementação encontrada |

### Etapa 3.4 — Painéis principais

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.4.1 | Hierarquia: árvore virtualizada, reparent, busca | não iniciado | Nenhum painel de hierarquia de produto implementado; hierarquia ECS (1.3.4) é infraestrutura de dados, não UI |
| 3.4.2 | Inspector adaptativo gerado por reflexão | não iniciado | Nenhum Inspector de produto implementado; `ComponentDescriptor`/`ComponentRegistry` (1.4.1) fornecem metadados, mas não há UI de Inspector consumindo-os |
| 3.4.3 | Navegador de assets com previews, busca, drag-and-drop | não iniciado | Nenhuma implementação encontrada |
| 3.4.4 | Console com filtros, agrupamento, navegação para origem | não iniciado | Nenhuma implementação encontrada |
| 3.4.5 | Cartão contextual do viewport | não iniciado | Nenhuma implementação encontrada |
| 3.4.6 | Barra superior, doca de modos, troca de modos | PoC | Protótipo HTML tem doca de modos visual (ver `prototype/editor.html`); não é produto |

### Etapa 3.5 — Fluxos de projeto

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.5.1 | Tela inicial: projetos recentes, templates, tutoriais | não iniciado | Nenhuma implementação encontrada |
| 3.5.2 | Criação de projeto com templates jogáveis | não iniciado | Nenhuma implementação encontrada |
| 3.5.3 | Sistema de prefabs com overrides e merge visual | não iniciado | Nenhuma implementação encontrada |
| 3.5.4 | Salvamento contínuo + recuperação de crash (WAL) | parcial | WAL de recuperação existe e é testado no nível de dados (1.4.4, ESTADO.md linha 72), mas não há fluxo de projeto/UI consumindo isso como "salvamento contínuo" de produto |
| 3.5.5 | Importação de arquivos: galeria, câmera, nuvem, ZIP | não iniciado | Nenhuma implementação encontrada |

### Etapa 3.6 — Play in Editor

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.6.1 | Processo Play separado com IPC por memória compartilhada | não iniciado | Nenhuma implementação de processo Play separado encontrada |
| 3.6.2 | Exibição do jogo dentro do viewport (AHardwareBuffer/IOSurface) | não iniciado | Nenhuma implementação encontrada |
| 3.6.3 | Inspeção ao vivo durante o play | não iniciado | Nenhuma implementação encontrada |
| 3.6.4 | Pausa, avanço quadro a quadro, câmera livre durante o play | não iniciado | Nenhuma implementação encontrada |
| 3.6.5 | Isolamento de crash com stack trace apresentável | não iniciado | Nenhuma implementação encontrada |

### Etapa 3.7 — Onboarding

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 3.7.1 | Tutorial interativo integrado | não iniciado | Nenhuma implementação encontrada |
| 3.7.2 | Três níveis de UI (Essencial/Padrão/Completo) | não iniciado | Nenhuma implementação encontrada |
| 3.7.3 | Dicas contextuais | não iniciado | Nenhuma implementação encontrada |

> **Gate M3:** não fechado — nem próximo. O editor continua sendo protótipo HTML, não produto integrado. Isso permanece detalhado na Fase 3 do plano principal e não é uma lacuna separada.

---

## Fase 4 — Simulação

### Etapa 4.1 — Física

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.1.1 | Integração do Jolt: mundo, corpos, formas, dormência, camadas | implementado | ESTADO.md linha 76: física Jolt + ABI V2 (`GAP-PHY-01` fechado) ✅ 7 testes físicos + 3 testes de ABI/capacidade (`test_jolt_bridge.cpp`, `test_physics_capacity.cpp`) |
| 4.1.2 | Fachada C# com RigidBody/Collider/Trigger e sync ECS↔Jolt | implementado | `GAP-PHY-02/03/04` fechados: autoridade cinemática, sensor/filtro/eventos/serialização e criação/destruição transacional em lote. `PhysicsSyncSystem` agrega o spawn do Step; telemetria mede crossings/bytes. ✅ 9 testes C++ + 15 C#; escala 100/1.000/10.000 com 1 crossing create + 1 destroy e zero GC dentro do crossing |
| 4.1.3 | Juntas e motores, com gizmos de edição no viewport | parcial | `GAP-JOINT-01` fechado: Point/Hinge/Slider/Distance usam ABI V2 `World`/`LocalToBody1`/`LocalToBody2`, componente declarativo `Joint`, referências `EntityId`, resolução tardia, sync idempotente, hot-edit e serialização ✅ 11 testes C++ + 13 testes C# de juntas + round-trip binário/texto. O item permanece parcial porque o gizmo existe só no protótipo HTML, não no viewport de produto; SixDOF e juntas especializadas continuam futuras |
| 4.1.4 | Queries (raycast/shapecast/overlap) expostas a script e a nós | parcial | ESTADO.md linha 79: RayCastAll/ShapeCastClosest/OverlapShape ✅ 10 testes C++ + 9 testes C# (`test_query_bridge.cpp`, `PhysicsQueryTests.cs`) — mas só "script" está feito; "nós" (Flow) não existem (ESTADO.md linha 154: nenhum nó `physics.raycast` etc.) |
| 4.1.5 | Character Controller: degraus, rampas, plataformas, agachar, correr, deslizar, escalar, nadar | parcial | CharacterVirtual cobre cápsula, degraus, rampas, plataformas e stance; `CharacterMotorSystem` centraliza gravidade/plataforma/stick-to-floor e estados `Grounded/Rising/Falling/Sliding` (`GAP-CHAR-01` fechado). ✅ 11 testes C++ + 17 testes C#. Escalar, nadar e nascer agachado continuam não implementados no item principal |
| 4.1.6 | Física 2D (benchmark Jolt-2D vs Box2D v3 → decisão) | parcial | Jolt Plane2D ✅ 7 testes C++ + 6 C#; Box2D v3.1.1 vendorizado só para benchmark. Três runners A/B medem cenário/qualidade equivalente, percentis, RSS isolado, tamanho e estabilidade; host e Android A integrais 50–5.000 verdes. `ADR-013-PHYSICS-2D-BACKEND.md` permanece proposta: faltam perfis Android B/C antes da decisão, portanto Box2D não entrou no runtime |
| 4.1.7 | Geração automática de colisores (convex decomposition) na importação | não iniciado | ESTADO.md linha 90: bloqueado por dependência real ausente — Jolt só tem `ConvexHullShape` de hull único; decomposição convexa de verdade exigiria V-HACD/CoACD, nunca vendorizado; também não há importador de malha no repositório |
| 4.1.8 | Determinismo em ponto fixo (modo opcional) e testes de reprodutibilidade | aceito | ESTADO.md linha 81 e linhas 161-162, 364-385: decisão deliberada de implementar `CROSS_PLATFORM_DETERMINISTIC` do Jolt (que usa float, não ponto fixo) em vez do que o nome do item pede — decisão registrada e documentada como divergência aceita, com 3 testes C++ novos (`test_determinism.cpp`) provando determinismo run-to-run (não cross-platform real, que exigiria hardware/SO diferentes) |
| 4.1.9 | Escalonamento térmico (sub-steps, islands) | não iniciado | ESTADO.md linha 90: "4.1.9 (sub-stepping adaptativo ligado ao PowerGovernor térmico)" listado explicitamente como não implementado |

### Etapa 4.2 — Animação

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.2.1 | Esqueleto, poses, skinning em compute | não iniciado | Nenhum código de animação no repositório (ESTADO.md linha 89: "Animação... Fases 6 e 8, não iniciadas" — nota: a numeração de fases desse comentário em ESTADO.md se refere à ausência geral, animação é Fase 4/6 no plano; nenhum arquivo de animação existe de fato) |
| 4.2.2 | Compressão de clips (ACL) e amostragem otimizada | não iniciado | Nenhuma implementação encontrada |
| 4.2.3 | Grafo de animação: state machine, blend 1D/2D, camadas | não iniciado | Nenhuma implementação encontrada |
| 4.2.4 | Editor de grafo de animação | não iniciado | Nenhuma implementação encontrada |
| 4.2.5 | IK: two-bone, FABRIK, look-at, foot placement | não iniciado | Nenhuma implementação encontrada |
| 4.2.6 | Root motion + integração com character controller | não iniciado | Nenhuma implementação encontrada |
| 4.2.7 | Blend shapes e animação facial | não iniciado | Nenhuma implementação encontrada |
| 4.2.8 | Retargeting humanoide automático | não iniciado | Nenhuma implementação encontrada |
| 4.2.9 | Eventos de animação | não iniciado | Nenhuma implementação encontrada |
| 4.2.10 | Timeline/Sequencer touch | não iniciado | Nenhuma implementação encontrada |

### Etapa 4.3 — Áudio

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.3.1 | Backend AAudio/AudioUnit com thread real-time | não iniciado | Nenhum código de áudio no repositório |
| 4.3.2 | Grafo de áudio: fontes, buses, submixes | não iniciado | Nenhuma implementação encontrada |
| 4.3.3 | Espacialização HRTF, atenuação, cones, doppler | não iniciado | Nenhuma implementação encontrada |
| 4.3.4 | Efeitos: reverb, EQ, compressor, limiter, delay | não iniciado | Nenhuma implementação encontrada |
| 4.3.5 | Zonas de reverb com blend | não iniciado | Nenhuma implementação encontrada |
| 4.3.6 | Sistema de música adaptativa | não iniciado | Nenhuma implementação encontrada |
| 4.3.7 | Gravação por microfone no editor | não iniciado | Nenhuma implementação encontrada |
| 4.3.8 | Streaming de áudio longo e gerenciamento de vozes | não iniciado | Nenhuma implementação encontrada |

### Etapa 4.4 — Partículas e VFX

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.4.1 | Simulação GPU em compute com pools de partículas | não iniciado | Nenhuma implementação encontrada |
| 4.4.2 | Editor de VFX por nós | não iniciado | Nenhuma implementação encontrada |
| 4.4.3 | Renderização: billboard, mesh, ribbon/trail | não iniciado | Nenhuma implementação encontrada |
| 4.4.4 | Decals com clustering | não iniciado | Nenhuma implementação encontrada |

### Etapa 4.5 — IA e navegação

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.5.1 | Geração de navmesh no dispositivo (Recast) | não iniciado | Nenhuma implementação encontrada |
| 4.5.2 | Pathfinding + funil + evitação local (ORCA) | não iniciado | Nenhuma implementação encontrada |
| 4.5.3 | Behavior trees e state machines com editor visual | não iniciado | Nenhuma implementação encontrada |
| 4.5.4 | Percepção e steering behaviors prontos | não iniciado | Nenhuma implementação encontrada |

### Etapa 4.6 — UI de jogo

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 4.6.1 | Canvas com layout responsivo, sprites 9-slice, texto SDF | não iniciado | Nenhuma implementação encontrada |
| 4.6.2 | Componentes: botão, slider, toggle, scroll, lista | não iniciado | Nenhuma implementação encontrada |
| 4.6.3 | Sistema de navegação por gamepad/teclado | não iniciado | Nenhuma implementação encontrada |
| 4.6.4 | Data binding exposto ao AetherFlow | não iniciado | Nenhuma implementação encontrada |
| 4.6.5 | Editor de UI touch com preview multi-resolução | não iniciado | Nenhuma implementação encontrada |
| 4.6.6 | Localização (strings, pluralização, RTL) | não iniciado | Nenhuma implementação encontrada |

> **Gate M4:** não fechado. Só a Física (4.1) tem avanço real, e mesmo essa com lacunas por item (ver acima). Animação, Áudio, VFX, IA/navegação e UI de jogo (4.2–4.6) estão inteiramente não iniciados — confirmado por ESTADO.md linha 89.

---

## Fase 5 — AetherFlow (no-code) e scripting C#

### Etapa 5.1 — Fundação da linguagem

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.1.1 | Definição da AST do AetherFlow | implementado | AST implementada (`FlowNode.cs`, `FlowGraph.cs`, `FlowConnection.cs`, `FlowPin.cs`, `FlowVariable.cs`, `FlowType.cs`) ✅ parte dos 48 testes Flow |
| 5.1.2 | Sistema de tipos, inferência, coerção segura, genéricos limitados | parcial | `FlowType.cs` existe com tipos básicos; genéricos limitados e inferência avançada não confirmados como completos — ESTADO.md linha 142 confirma catálogo de nós mínimo (~12), sugerindo sistema de tipos também mínimo |
| 5.1.3 | Formato de arquivo `.aflow` + serialização | implementado | ESTADO.md linha 73 e 187-188 (`GAP-FLOW-01`): serializador `.aflow` com round-trip testado, incluindo controle terminal simétrico (`FlowSerializer.cs`) |
| 5.1.4 | Validador semântico (ciclos, tipos, pinos obrigatórios) | implementado | `FlowValidator.cs` também valida capabilities do contexto de execução; integra os 48 testes Flow |
| 5.1.5 | Compilador AST → C# gerado legível | implementado | `FlowToCSharp.cs`; ESTADO.md linha 73 confirma "gerador de C#" testado, incluindo `GAP-FLOW-01` (return/break/continue simétricos) |
| 5.1.6 | Interpretador de AST para iteração instantânea | implementado | `FlowInterpreter.cs`; ESTADO.md linha 73 confirma "interpretador" testado |

### Etapa 5.2 — Round-trip C# ⇄ Grafo

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.2.1 | Parser C# (Roslyn) → AST do Flow para subconjunto suportado | parcial | `CSharpToFlow.cs` existe e é exercitado pelo teste diferencial de `GAP-FLOW-01` (ESTADO.md linha 30-34: "compila e executa offline o C# original e o regenerado"), mas ESTADO.md linha 133 classifica isso como "round-trip parcial com C#", não completo |
| 5.2.2 | Regras de preservação: comentários, formatação, nomes | não iniciado | Nenhuma evidência de preservação de comentários/formatação no round-trip; não mencionado em ESTADO.md |
| 5.2.3 | Nó "caixa de código" para construções fora do subconjunto | não iniciado | Nenhuma menção a "caixa de código"/`code.raw` implementada em ESTADO.md ou no código lido |
| 5.2.4 | Testes de ida-e-volta (property-based) | parcial | Testes diferenciais de `GAP-FLOW-01` cobrem round-trip para controle terminal (ESTADO.md linha 187-188: "nove regressões... round-trip .aflow"), mas não são testes property-based com geração aleatória de grafos como o item pede |

### Etapa 5.3 — Canvas de nós

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.3.1 | Canvas com pan/zoom, virtualização, LOD de nós | não iniciado | Nenhum canvas de nós de produto implementado; `Aether.Flow` é uma biblioteca sem UI (ESTADO.md linha 133: "PoC arquitetural funcional") |
| 5.3.2 | Criação de nós: paleta com busca | não iniciado | Nenhuma UI de criação de nós implementada |
| 5.3.3 | Conexão de fios por arrasto, validação de tipo | não iniciado | Nenhuma UI implementada (validação de tipo existe só no validador de dados, não como interação de UI) |
| 5.3.4 | Layout automático (dagre/sugiyama) | não iniciado | Nenhuma implementação encontrada |
| 5.3.5 | Grupos, comentários, cores, colapso em sub-grafo | não iniciado | Nenhuma implementação encontrada |
| 5.3.6 | Reroute nodes e organização de fios | não iniciado | Nenhuma implementação encontrada |

### Etapa 5.4 — Modo Lista e Modo Blocos

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.4.1 | Modo Lista: renderização indentada da AST | não iniciado | Nenhuma UI de Modo Lista implementada |
| 5.4.2 | Modo Blocos: encaixe físico estilo Scratch | não iniciado | Nenhuma UI de Modo Blocos implementada |
| 5.4.3 | Transição animada entre as três representações | não iniciado | Nenhuma implementação encontrada |
| 5.4.4 | Escolha automática de representação por tela/nível | não iniciado | Nenhuma implementação encontrada |

### Etapa 5.5 — Biblioteca de nós

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.5.1 | Implementação de todas as categorias (~400 nós) | parcial | Biblioteca mínima (~13 nós), incluindo `log.message` como prova do contexto externo; não entrega as ~400 categorias do plano |
| 5.5.2 | Geração automática de nós a partir de C# (`[FlowNode]`) | não iniciado | Nenhum atributo `[FlowNode]` ou gerador associado encontrado no código lido |
| 5.5.3 | Documentação inline de cada nó | não iniciado | Nenhuma documentação inline por nó encontrada |
| 5.5.4 | Macros e sub-grafos reutilizáveis, publicáveis | não iniciado | Nenhuma implementação de macros/sub-grafos encontrada |

### Etapa 5.6 — Depuração visual

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.6.1 | Fluxo animado nos fios; valores exibidos nos fios | não iniciado | Nenhuma UI de depuração visual implementada (não há canvas de produto, ver 5.3) |
| 5.6.2 | Breakpoints, step, watch | não iniciado | Nenhuma implementação encontrada |
| 5.6.3 | Time-travel debugging | não iniciado | Nenhuma implementação encontrada |
| 5.6.4 | Mapa de calor de custo por nó | não iniciado | Nenhuma implementação encontrada |

> **Nota sobre a faixa "5.1–5.6" em ESTADO.md:** os 48 testes cobrem a fundação e o contrato externo (`GAP-FLOW-01/02`), não toda a faixa. Esta matriz continua desagregando o roadmap: canvas, modos de autoria, catálogo completo e depuração visual permanecem não iniciados ou parciais.

### Etapa 5.7 — Scripting C# completo

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.7.1 | Compilação Roslyn no dispositivo, incremental | não iniciado | Nenhuma implementação de compilação Roslyn no dispositivo encontrada |
| 5.7.2 | Hot reload com migração de estado | não iniciado | A viabilidade técnica do ciclo load/unload de assembly está provada e validada em hardware (0.2 PoC-D), mas migração de estado entre versões do script (preservar valores de campo ao trocar de assembly) e integração com o pipeline de edição do AetherFlow não existem — este item é sobre o produto final, não a fundação técnica que 0.2 já entrega |
| 5.7.3 | Editor de código touch: símbolos, autocompletar, snippets | não iniciado | Nenhuma implementação encontrada |
| 5.7.4 | Ditado por voz com gramática de código [opcional] | não iniciado | Nenhuma implementação encontrada |
| 5.7.5 | Depurador de C# no dispositivo | não iniciado | Nenhuma implementação encontrada |
| 5.7.6 | API de gameplay completa (Behavior → System → IJobSystem) | não iniciado | Nenhuma API de gameplay de produto implementada; `IJobSystem`-like existe só como job system genérico (1.2.2), não como API de gameplay em 3 níveis |
| 5.7.7 | Sandbox de segurança para código de projetos baixados | não iniciado | Nenhuma implementação encontrada |

### Etapa 5.8 — Assistente de IA

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 5.8.1 | Modelo local pequeno para autocompletar/sugestões | não iniciado | Nenhuma implementação encontrada |
| 5.8.2 | Serviço em nuvem "descrever → grafo", "explicar", "consertar" | não iniciado | Nenhuma implementação encontrada |
| 5.8.3 | Apresentação sempre como diff revisável | não iniciado | Nenhuma implementação encontrada |
| 5.8.4 | Controles de privacidade explícitos e opt-in | não iniciado | Nenhuma implementação encontrada |

> **Gate M5:** não fechado. Fundação da AST/interpretador/gerador (5.1) é sólida e testada, mas canvas, modos de visualização, catálogo completo de nós, scripting C# de produto e assistente de IA (5.2–5.8 em sua maior parte) não existem.

---

## Fase 6 — Ferramentas de criação de conteúdo

### Etapa 6.1 — Pipeline de assets completo

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.1.1 | Importadores: glTF, FBX, OBJ, USD/USDZ, VRM, Collada, STL/PLY | não iniciado | Nenhum importador de asset no repositório |
| 6.1.2 | Imagens: PNG/JPG/WebP/EXR/HDR/TGA/PSD | não iniciado | Nenhuma implementação encontrada |
| 6.1.3 | Processamento de malha: cache, LOD, tangentes, quantização | não iniciado | Nenhuma implementação encontrada |
| 6.1.4 | Compressão ASTC/ETC2 em compute shader + KTX2/Basis | não iniciado | A viabilidade técnica do encoder ASTC em compute shader está provada e validada em hardware (0.2 PoC-E), mas é um encoder mínimo (bounding box, sem particionamento múltiplo), sem ETC2, sem empacotamento KTX2/Basis, e sem integração ao pipeline de import — este item é sobre o encoder de produção completo, não a fundação técnica que 0.2 já entrega |
| 6.1.5 | Cache por hash, import incremental, preview progressivo | não iniciado | Nenhuma implementação encontrada |
| 6.1.6 | Streaming de texturas por mip | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.2 — Modelagem poligonal

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.2.1 | Estrutura half-edge com edição paralela e histórico | não iniciado | Nenhuma implementação encontrada |
| 6.2.2 | Modo de edição de malha: vértice/aresta/face | não iniciado | Nenhuma implementação encontrada |
| 6.2.3 | Operadores: extrude, inset, bevel, loop cut, knife, bridge | não iniciado | Nenhuma implementação encontrada |
| 6.2.4 | Snap, simetria, proportional editing | não iniciado | Nenhuma implementação encontrada |
| 6.2.5 | Pilha de modificadores não-destrutivos | não iniciado | Nenhuma implementação encontrada |
| 6.2.6 | Booleanas robustas (biblioteca C++) | não iniciado | Nenhuma implementação encontrada |
| 6.2.7 | Primitivas paramétricas editáveis | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.3 — Escultura

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.3.1 | Motor de escultura com dyntopo e multiresolução | não iniciado | Nenhuma implementação encontrada |
| 6.3.2 | ~15 pincéis com falloff, alpha e textura | não iniciado | Nenhuma implementação encontrada |
| 6.3.3 | Suporte a pressão/inclinação de caneta | não iniciado | Nenhuma implementação encontrada |
| 6.3.4 | Máscaras, camadas de escultura, simetria radial | não iniciado | Nenhuma implementação encontrada |
| 6.3.5 | Voxel remesh e quad remesh em compute | não iniciado | Nenhuma implementação encontrada |
| 6.3.6 | Retopologia manual por traçado | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.4 — UV e texturização

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.4.1 | Unwrap automático (xatlas) + marcação manual | não iniciado | Nenhuma implementação encontrada |
| 6.4.2 | Editor de UV touch | não iniciado | Nenhuma implementação encontrada |
| 6.4.3 | Pintura 3D direta na malha | não iniciado | Nenhuma implementação encontrada |
| 6.4.4 | Camadas procedurais (curvatura/AO/altura/inclinação) | não iniciado | Nenhuma implementação encontrada |
| 6.4.5 | Baking (normal, AO, curvatura, position, ID) em GPU | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.5 — Editor de materiais e shaders

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.5.1 | Shader Graph no canvas de nós compartilhado | não iniciado | Nenhuma implementação encontrada |
| 6.5.2 | Biblioteca de nós de shader (~150) | não iniciado | Nenhuma implementação encontrada |
| 6.5.3 | Preview em tempo real com pipeline real | não iniciado | Nenhuma implementação encontrada |
| 6.5.4 | Sistema de instâncias de material com overrides | não iniciado | Nenhuma implementação encontrada |
| 6.5.5 | Validação de custo (instruções, precisão) | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.6 — Terreno, vegetação e mundo aberto

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.6.1 | Terreno com CDLOD, chunking e streaming | não iniciado | Nenhuma implementação encontrada |
| 6.6.2 | Pincéis de escultura de terreno + erosão | não iniciado | Nenhuma implementação encontrada |
| 6.6.3 | Pintura de camadas com splat + triplanar | não iniciado | Nenhuma implementação encontrada |
| 6.6.4 | Espalhamento de vegetação por regras | não iniciado | Nenhuma implementação encontrada |
| 6.6.5 | Água com Gerstner/FFT e flutuação | não iniciado | Nenhuma implementação encontrada |
| 6.6.6 | World Partition: células, data layers, streaming, HLOD | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.7 — Geometry Nodes

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.7.1 | Sistema de campos de atributo e avaliação de grafo geométrico | não iniciado | Nenhuma implementação encontrada |
| 6.7.2 | ~100 nós procedurais | não iniciado | Nenhuma implementação encontrada |
| 6.7.3 | Cache e avaliação em runtime | não iniciado | Nenhuma implementação encontrada |
| 6.7.4 | Exemplos prontos (cidade, escada, cerca, dungeon, floresta) | não iniciado | Nenhuma implementação encontrada |

### Etapa 6.8 — Inovações de captura

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 6.8.1 | Fotogrametria in-app (SfM + MVS) | não iniciado | Nenhuma implementação encontrada |
| 6.8.2 | LiDAR/Depth scan | não iniciado | Nenhuma implementação encontrada |
| 6.8.3 | Material a partir de foto (ML) | não iniciado | Nenhuma implementação encontrada |
| 6.8.4 | Mocap por câmera | não iniciado | Nenhuma implementação encontrada |
| 6.8.5 | Auto-rigging por ML | não iniciado | Nenhuma implementação encontrada |

> **Gate M6:** não fechado. Fase 6 inteira não iniciada — ESTADO.md linha 89 confirma "assets... Fases 6 e 8, não iniciadas".

---

## Fase 7 — Gráficos de última geração

### Etapa 7.1 — GPU-Driven Rendering e MicroMesh

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.1.1 | Geração de meshlets e DAG de LOD no import | não iniciado | Nenhuma implementação encontrada |
| 7.1.2 | Cull hierárquico por cluster em compute | não iniciado | Nenhuma implementação encontrada |
| 7.1.3 | Seleção de nível por erro projetado em pixels | não iniciado | Nenhuma implementação encontrada |
| 7.1.4 | Desenho indireto + mesh shaders | não iniciado | Nenhuma implementação encontrada |
| 7.1.5 | Residência e prefetch de níveis de geometria | não iniciado | Nenhuma implementação encontrada |
| 7.1.6 | Fallback completo para perfis B/C | não iniciado | Nenhuma implementação encontrada |

### Etapa 7.2 — Iluminação global (GlowField)

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.2.1 | SDF global da cena | não iniciado | Nenhuma implementação encontrada |
| 7.2.2 | DDGI: grades cascateadas de sondas | não iniciado | Nenhuma implementação encontrada |
| 7.2.3 | Ray marching no SDF; filtragem temporal | não iniciado | Nenhuma implementação encontrada |
| 7.2.4 | Caminho com ray_query (hardware RT) | não iniciado | Nenhuma implementação encontrada |
| 7.2.5 | Lightmap baking em nuvem | não iniciado | Nenhuma implementação encontrada |
| 7.2.6 | Reflexões híbridas (SSR → probes → GI → RT) | não iniciado | Nenhuma implementação encontrada |
| 7.2.7 | GTAO com bent normals | não iniciado | Nenhuma implementação encontrada |

### Etapa 7.3 — Sombras avançadas

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.3.1 | Virtual Shadow Maps: atlas esparso | não iniciado | Nenhuma implementação encontrada |
| 7.3.2 | Cache de páginas estáticas | não iniciado | Nenhuma implementação encontrada |
| 7.3.3 | PCSS com penumbra por tamanho de fonte | não iniciado | Nenhuma implementação encontrada |
| 7.3.4 | Sombras de contato (screen space) | não iniciado | Nenhuma implementação encontrada |
| 7.3.5 | Sombras de transparências e vegetação | não iniciado | Nenhuma implementação encontrada |

### Etapa 7.4 — Volumétricos e atmosfera

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.4.1 | Froxel volumétrico (fog, luz volumétrica) | não iniciado | Nenhuma implementação encontrada |
| 7.4.2 | Atmosfera fisicamente baseada (Bruneton) | não iniciado | Nenhuma implementação encontrada |
| 7.4.3 | Nuvens volumétricas [opcional S] | não iniciado | Nenhuma implementação encontrada |
| 7.4.4 | Ciclo dia/noite | não iniciado | Nenhuma implementação encontrada |

### Etapa 7.5 — Anti-aliasing, upscaling e pós

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.5.1 | TAA robusto | não iniciado | Nenhuma implementação encontrada |
| 7.5.2 | AetherSR: reconstrução temporal | não iniciado | Nenhuma implementação encontrada |
| 7.5.3 | Integração GSR/MetalFX/FSR | não iniciado | Nenhuma implementação encontrada |
| 7.5.4 | Resolução dinâmica com histerese | não iniciado | Nenhuma implementação encontrada |
| 7.5.5 | VRS dirigido por buffer de importância | não iniciado | Nenhuma implementação encontrada |
| 7.5.6 | Pós avançado (DOF, motion blur, HDR10) | não iniciado | Nenhuma implementação encontrada |

### Etapa 7.6 — Escalabilidade e perfis

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 7.6.1 | Sistema de perfis gráficos (S/A/B/C) com detecção automática | parcial | ESTADO.md linha 74: perfis de dispositivo S/A/B/C testados no RHI headless (`test_device_profile.cpp`); mas detecção automática de dispositivo real e override de usuário no produto não confirmados |
| 7.6.2 | Base de dados de dispositivos conhecidos | não iniciado | Nenhuma base de dados de dispositivos encontrada, além do registro manual do único aparelho em ESTADO.md |
| 7.6.3 | Benchmark de calibração na primeira execução | não iniciado | Nenhuma implementação encontrada |
| 7.6.4 | Integração completa com PowerGovernor | não iniciado | `PowerGovernor` existe (3.4/1.1.5) mas sem integração com perfis gráficos S/A/B/C |
| 7.6.5 | Comparador visual dos 4 perfis lado a lado | não iniciado | Nenhuma implementação encontrada |

> **Gate M7:** não fechado. Fase 7 inteira não iniciada, exceto a base headless de perfis de dispositivo (7.6.1 parcial), que é compartilhada com 2.1.7.

---

## Fase 8 — Build, publicação e serviços

### Etapa 8.1 — Aether Player

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.1.1 | App leve (< 60 MB) que carrega `.aetherpack` | não iniciado | Nenhuma implementação encontrada |
| 8.1.2 | Geração do pacote no dispositivo | não iniciado | Nenhuma implementação encontrada |
| 8.1.3 | Compartilhamento por link/QR | não iniciado | Nenhuma implementação encontrada |
| 8.1.4 | Sandbox de segurança para projetos de terceiros | não iniciado | Nenhuma implementação encontrada |
| 8.1.5 | Feed de projetos da comunidade | não iniciado | Nenhuma implementação encontrada |

### Etapa 8.2 — Build farm em nuvem

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.2.1 | Infraestrutura de build (Android/iOS) com fila e cache | não iniciado | Nenhuma implementação encontrada |
| 8.2.2 | Upload delta do projeto; build incremental | não iniciado | Nenhuma implementação encontrada |
| 8.2.3 | AOT, IL trimming, empacotamento | não iniciado | Nenhuma implementação encontrada |
| 8.2.4 | Assinatura sem expor a chave | não iniciado | Nenhuma implementação encontrada |
| 8.2.5 | Alvos: APK, AAB, IPA, desktop, WebGPU | parcial | APK debug/release ARM64 gerado localmente via Gradle (ESTADO.md linha 104-105), mas não via build farm em nuvem, e nenhum outro alvo (AAB/IPA/desktop/WebGPU) existe |
| 8.2.6 | Logs de build legíveis, diagnóstico em português | não iniciado | Nenhuma implementação encontrada além de logs de build padrão do Gradle |

### Etapa 8.3 — Publicação assistida

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.3.1 | Checklist guiado (ícone, splash, descrição) | não iniciado | Nenhuma implementação encontrada |
| 8.3.2 | Geração automática de assets de loja | não iniciado | Nenhuma implementação encontrada |
| 8.3.3 | Integração Google Play Console / App Store Connect | não iniciado | Nenhuma implementação encontrada |
| 8.3.4 | Gestão de versões e canais | não iniciado | Nenhuma implementação encontrada |

### Etapa 8.4 — Serviços de jogo

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.4.1 | Contas, saves em nuvem, perfis | não iniciado | Nenhuma implementação encontrada |
| 8.4.2 | Leaderboards, conquistas, estatísticas | não iniciado | Nenhuma implementação encontrada |
| 8.4.3 | Analytics (funil, retenção, mapas de calor) | não iniciado | Nenhuma implementação encontrada |
| 8.4.4 | Remote config e A/B testing | não iniciado | Nenhuma implementação encontrada |
| 8.4.5 | Monetização: anúncios, IAP, assinaturas | não iniciado | Nenhuma implementação encontrada |
| 8.4.6 | Relatórios de crash simbolizados | não iniciado | Nenhuma implementação encontrada |

### Etapa 8.5 — Multiplayer

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.5.1 | Transporte UDP confiável + fallback WebSocket | não iniciado | Nenhuma implementação encontrada |
| 8.5.2 | Replicação de componentes com delta/quantização | não iniciado | Nenhuma implementação encontrada |
| 8.5.3 | RPCs com autoridade, expostos ao Flow | não iniciado | Nenhuma implementação encontrada |
| 8.5.4 | Predição de cliente + reconciliação | não iniciado | Nenhuma implementação encontrada |
| 8.5.5 | Lag compensation no servidor | não iniciado | Nenhuma implementação encontrada |
| 8.5.6 | Matchmaking, salas e relay | não iniciado | Nenhuma implementação encontrada |
| 8.5.7 | Build de servidor dedicado headless | não iniciado | Nenhuma implementação encontrada |
| 8.5.8 | Ferramentas: simulador de latência, visualizador de tráfego | não iniciado | Nenhuma implementação encontrada |

### Etapa 8.6 — Profiler e ferramentas de diagnóstico

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 8.6.1 | Profiler de CPU com timeline por thread | parcial | `native/profiler/frame_statistics.*` + adaptador Android opt-in, relógios CPU processo/thread, fases wall-time, percentis e coleta reproduzível no aparelho. 10 testes C++ + 17 do parser/coletores. Ainda não existe timeline por thread/job nem atribuição de stack/GC/JIT. Ver `PROFILING-ANDROID.md` |
| 8.6.2 | Profiler de GPU por pass do render graph | não iniciado | Nenhuma implementação encontrada |
| 8.6.3 | Profiler de memória por categoria | não iniciado | Nenhuma implementação encontrada |
| 8.6.4 | Monitor térmico e de energia com histórico | não iniciado | Nenhuma implementação encontrada |
| 8.6.5 | Assistente de otimização | não iniciado | Nenhuma implementação encontrada |
| 8.6.6 | Captura de frame para RenderDoc/AGI | não iniciado | Nenhuma implementação encontrada |

> **Gate M8:** não fechado. Fase 8 quase inteiramente não iniciada; único avanço é a geração manual local de APK debug/release (8.2.5, parcial).

---

## Fase 9 — Colaboração, ecossistema e lançamento

### Etapa 9.1 — Versionamento e colaboração

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.1.1 | Git embutido com camada visual | não iniciado | Nenhuma implementação encontrada (o próprio repositório usa Git de forma convencional via CLI, não uma camada visual de produto) |
| 9.1.2 | Diff visual de cena e prefab; merge assistido | não iniciado | Nenhuma implementação encontrada |
| 9.1.3 | LFS automático; políticas de armazenamento | não iniciado | Nenhuma implementação encontrada |
| 9.1.4 | Co-edição CRDT em tempo real | não iniciado | Nenhuma implementação encontrada |
| 9.1.5 | Comentários espaciais e chat de voz | não iniciado | Nenhuma implementação encontrada |
| 9.1.6 | Modo Sala de Aula | não iniciado | Nenhuma implementação encontrada |

### Etapa 9.2 — Asset Store e comunidade

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.2.1 | Loja integrada com preview interativo | não iniciado | Nenhuma implementação encontrada |
| 9.2.2 | Publicação de assets/sub-grafos/templates/plugins | não iniciado | Nenhuma implementação encontrada |
| 9.2.3 | Curadoria, avaliações, moderação | não iniciado | Nenhuma implementação encontrada |
| 9.2.4 | Pacotes iniciais gratuitos | não iniciado | Nenhuma implementação encontrada |
| 9.2.5 | Remix: fork de projeto público com um toque | não iniciado | Nenhuma implementação encontrada |

### Etapa 9.3 — Extensibilidade

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.3.1 | SDK de plugins em C# | não iniciado | Nenhuma implementação encontrada |
| 9.3.2 | API de automação/scripting do editor | não iniciado | Nenhuma implementação encontrada |
| 9.3.3 | Sandbox e permissões de plugin | não iniciado | Nenhuma implementação encontrada |
| 9.3.4 | Documentação de arquitetura interna e política open source | não iniciado | Nenhuma implementação encontrada (existe documentação de estado/plano, mas não de arquitetura interna voltada a plugins/licenciamento) |

### Etapa 9.4 — XR [opcional/paralelo]

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.4.1 | AR: ARCore/ARKit, âncoras, oclusão | não iniciado | Nenhuma implementação encontrada |
| 9.4.2 | VR mobile (Quest via Android) | não iniciado | Nenhuma implementação encontrada |
| 9.4.3 | Modo de edição em AR | não iniciado | Nenhuma implementação encontrada |

### Etapa 9.5 — Documentação e aprendizado

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.5.1 | Documentação completa da API (PT-BR/EN) | não iniciado | A documentação existente (`README.md`, `docs/ESTADO.md`, `docs/PLANO-*.md`) é de estado/planejamento do projeto, não documentação de API de produto para usuários finais |
| 9.5.2 | Tutoriais interativos dentro do app | não iniciado | Nenhuma implementação encontrada |
| 9.5.3 | Cursos em vídeo e projetos de exemplo comentados | não iniciado | Nenhuma implementação encontrada |
| 9.5.4 | Templates de jogos completos (10+ gêneros) | não iniciado | Nenhuma implementação encontrada |
| 9.5.5 | Programa de embaixadores e conteúdo da comunidade | não iniciado | Nenhuma implementação encontrada |

### Etapa 9.6 — Endurecimento e lançamento

| Item | Descrição curta | Estado | Evidência |
|---|---|---|---|
| 9.6.1 | Beta aberta ampla; triagem de bugs | não iniciado | Nenhuma implementação encontrada |
| 9.6.2 | Compatibilidade: matriz de 100+ dispositivos | não iniciado | Apenas 1 aparelho testado (ver 0.1.2) — muito aquém de 100+ |
| 9.6.3 | Otimização final guiada por telemetria real | não iniciado | Nenhuma implementação encontrada |
| 9.6.4 | Auditoria de segurança e privacidade (LGPD/GDPR) | não iniciado | Nenhuma implementação encontrada |
| 9.6.5 | Localização (PT-BR, EN, ES, ZH, HI, ID, RU, JA) | não iniciado | Nenhuma implementação encontrada |
| 9.6.6 | Infraestrutura de suporte e SLA | não iniciado | Nenhuma implementação encontrada |
| 9.6.7 | Lançamento 1.0 com campanha e showcase | não iniciado | Nenhuma implementação encontrada |

> **Gate M9/1.0:** não fechado. Fase 9 inteiramente não iniciada.

---

## Contradições encontradas entre README/ESTADO.md e a matriz real

Nenhuma contradição factual grave. `README.md` usa "em construção" de forma
uniforme para todos os módulos — postura conservadora que não superestima
nada, e já afirma "Estado atual: Fase 1 (Núcleo) em execução", consistente
com `ESTADO.md` (0/10 gates fechados).

O único ponto notável não é uma contradição factual, mas de **forma**, e é a
própria motivação desta matriz existir: `ESTADO.md` linha 73 agrupa "5.1–5.6
(fundação)" numa única linha marcada ⚠️ (não ✅ — portanto não viola a letra
da regra "não existe item verde sem evidência"), mas esse agrupamento mistura
itens com evidência real (5.1.1, 5.1.3–5.1.6, fundação do AetherFlow) com
itens totalmente ausentes (5.2–5.6: canvas, modo lista/blocos, catálogo de
~400 nós, depuração visual/time-travel). A tabela acima desfaz esse
agrupamento item a item — é a correção que `PLANO-FECHAMENTO-LACUNAS.md`
§4.1 item 4 pede explicitamente ("proibir que um cabeçalho amplo... fique
verde quando só a infraestrutura foi concluída").

Nenhuma edição foi feita em `README.md` ou
`docs/ESTADO.md` como parte da criação desta matriz.
