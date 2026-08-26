# Estado da execução — o que existe de verdade

Instantâneo do repositório contra o roadmap de `PLANO-ENGINE-MOBILE.md`.
Regra: só entra nesta tabela o que **compila e passa em teste**.

O plano de resolução das lacunas deste instantâneo está em
`PLANO-FECHAMENTO-LACUNAS.md`. Ele define prioridades, dependências, migrações,
testes em hardware e critérios de aceite sem substituir o roadmap principal.

## Resumo

| | |
|---|---|
| Testes C# | **242 passando**, 0 falhando |
| Testes C++ | **96 passando**, 0 falhando (18 de Render Graph + 13 de RHI/perfis + 10 de core/jobs + 4 de lifecycle + 9 de física + 8 de juntas + 10 de queries + 11 de character controller + 7 de física 2D + 3 de determinismo + 3 de capacidade/ABI V2) |
| Linhas C# | ~11.000 |
| Linhas C++ (próprias, sem código vendorizado) | ~3.400 |
| Dependências externas | **nenhuma** — build e testes rodam offline, de propósito (Jolt Physics é vendorizado em `native/third_party/`, não baixado no build — ver `VENDORED_COMMIT.txt`) |

## Progresso do plano de fechamento

| Medida formal | Estado |
|---|---|
| Lacunas registradas em `PLANO-FECHAMENTO-LACUNAS.md` | 18 |
| Lacunas integralmente fechadas | **5/18** — `GAP-FLOW-01`, `GAP-PHY-01`, `GAP-PHY-02`, `GAP-JOB-01`, `GAP-ECS-01` |
| Gates M0–M9 fechados | **0/10** |
| Hardware M0 | parcial — 1 aparelho Adreno; matriz mínima, Mali e perfil C pendentes |
| Shell gráfico §5.2 | parcial — triângulo, 1.000 frames, 100 retomadas e screen-cycle verdes; cubo/depth/textura e captura com validation layers pendentes |
| §4.1 Inventário executável (Onda 0) | ✅ `docs/MATRIZ-MARCOS.md` — 332 itens do plano principal (0.1.1–9.6.7 + PoCs de risco da Etapa 0.2) com estado individual e evidência; nenhum cabeçalho de faixa agregado. 277 não iniciados, 25 parciais, 18 implementados, 9 PoC, 2 validados em hardware, 1 aceito |
| §4.2 CI confiável (Onda 0) | ✅ implementação concluída — `.github/workflows/ci.yml` separa `unit`/`native`/`interop`/`android-host`/`benchmark`/`clean-build-nightly`; integração e clean build exigem a DLL nativa; cada job publica evidência; o clean build instala headers Vulkan isolados do NDK e foi reproduzido localmente com 242/242 testes C# e 96/96 nativos. `metrics/budgets.v1.json` versiona P/Invoke, alocação, CPU, GPU, memória e energia, com gate positivo e negativo. `android-device`/`soak` ficam num workflow manual para runner físico. A execução hospedada e as medições reais de hardware continuam pendências de evidência para M0/M1, pois ainda não há remote nem runner `android-device-lab` |
| Onda 0 — verdade operacional/correções | ✅ implementação local completa (§4.1–§4.4); execução CI hospedada, laboratório Android e métricas coletadas em hardware permanecem evidências operacionais dos gates seguintes |

`GAP-FLOW-01` está fechado porque `return`, `break` e `continue` agora são nós
terminais explícitos, possuem escopo validado, propagam controle corretamente no
interpretador, sobrevivem à serialização e ao round-trip C#. O teste diferencial
compila e executa offline o C# original e o regenerado e compara ambos com o
estado determinístico do interpretador.

`GAP-PHY-01` está fechado com `AetherPhysicsWorldDescV2` versionado por
`structSize`/`apiVersion`, capacidades independentes de corpos, pares, contatos e
buffer da broad phase, `StepV2` com flags e contadores cumulativos e símbolo V1
preservado com defaults conservadores. Debug/teste falha imediatamente pelo
assert do Jolt; Release foi exercitado por probe induzindo as três categorias de
overflow, com warning e contadores, sem encerramento. Pilhas densas de 500, 1.000
e 5.000 corpos passam sem descarte e o benchmark não infla mais `maxBodies`.

`GAP-PHY-02` está fechado com `AetherPhysics_MoveKinematicV2`, autoridade de
transform explícita por tipo de corpo e sincronização ECS→Jolt antes do `Step`.
O cache exato de alvo por corpo evita crossings estáveis sem depender da versão
suja ainda imprecisa do ECS; após uma mudança, um único comando final zera a
velocidade cinemática calculada pelo Jolt. Plataformas transladando e girando
transportam tanto corpo dinâmico quanto character, sem feedback Jolt→ECS.

`GAP-JOB-01` está fechado com um grafo explícito de pré-requisitos e esperas
runtime. Cada `Complete()` feito dentro de um job publica sua aresta sob lock e
procura o caminho inverso antes de esperar; ciclos de autoespera, dependência,
dois jobs e três jobs são recusados com o caminho completo. O timeout de 3 s
permanece apenas como watchdog para trabalho externo que o grafo não observa.

`GAP-ECS-01` está fechado com APIs explícitas `Read<T>`/`Write<T>` no mundo e
`GetReadOnlySpan<T>`/`GetWritableSpan<T>` no chunk. Leitura não altera versão;
cada acesso mutável incrementa uma vez somente a coluna do chunk acessado. As APIs
antigas permanecem deprecated e conservadoras para não quebrar consumidores. Duas
regressões cobrem leitura, escrita, coluna e isolamento entre chunks.

## Por fase

| Fase | Item | Estado |
|---|---|---|
| **0.1** | Monorepo, build C# + CMake/Ninja, runner de testes próprio | ✅ |
| **0.1.3 + shell gráfico mínimo** | NativeActivity ARM64, Gradle/NDK, landscape imersivo, lifecycle, surface, swapchain e frame Vulkan | ⚠️ Triângulo RGB apresentado em 1 aparelho físico real (Xiaomi SM8735/Adreno, Android 16); 100 retomadas, configuração e screen-cycle automatizados com captura final idêntica. Ainda ⚠️ porque faltam a matriz mínima de GPUs e o cubo texturizado/interoperabilidade exigidos pelo M0 |
| **0.3.2** | Protótipo de interação: câmera, gizmos, menu radial, inspector | ✅ `prototype/editor.html` |
| **1.5** | Matemática: float2/3/4, quaternion, float4x4, Transform, Bounds, Ray, Plane, Frustum | ✅ 25 testes |
| **1.2** | Memória: FrameArena, PoolAllocator, NativeList/Array, MemoryBudget | ✅ 22 testes |
| **1.2 + GAP-JOB-01** | Job system com work-stealing, dependências, política big.LITTLE e grafo explícito de pré-requisitos/esperas runtime. Ciclos são detectados atomicamente antes do bloqueio e diagnosticados como caminho; timeout não é mais o detector primário | ✅ 13 testes, incluindo ciclos de 1, 2 e 3 jobs e ciclo prerequisito↔dependente; suíte repetida 10 vezes sem flutuação |
| **3.4** | PowerGovernor com histerese | ✅ 6 testes |
| **1.3 + GAP-ECS-01** | ECS por arquétipos: chunks de 16 KB, consultas por chunk sem alocação, command buffer e leitura/escrita com versões exatas por chunk/coluna | ✅ 26 testes |
| **1.3.4–1.3.5** | Hierarquia (lista encadeada de irmãos), propagação de transform e fachada `Node` sem ownership sobre `World`+`EntityId` | ✅ 13 testes |
| **1.4.1–1.4.2** | Registro de metadados de componente (runtime, não source generator — ver `ComponentDescriptor`) + serializador binário bit-exato e texto determinístico, migração de esquema em cadeia | ✅ 14 testes |
| **1.4.3** | Sistema de recursos: `ResourceId`, `ResourceRef`/`WeakResourceRef` com posse linear, `ResourceHandleTable` com contagem de uso, carregamento assíncrono via JobSystem | ✅ 18 testes |
| **1.4.4** | Undo/redo (`UndoStack`) + WAL de recuperação com fsync, checksum FNV-1a por registro, recuperação parcial ante corrupção | ✅ 16 testes |
| **5.1–5.6 (fundação)** | AetherFlow: AST, validador, gerador de C#, parser de volta, interpretador, serializador e controle terminal simétrico | ⚠️ 43 testes verdes; `GAP-FLOW-01` fechado, mas catálogo, serviços externos, editor e API de produto de 5.1–5.6 continuam incompletos |
| **2.1** | RHI Vulkan: cache de descritores, perfis de dispositivo S/A/B/C | ✅ lógica testada |
| **2.3** | **Render graph**: topológico, poda, aliasing, barreiras, load/store ops, memoryless, fusão de subpasses | ✅ 41 testes |
| **4.1.1 + GAP-PHY-01** | Física Jolt e ABI de capacidade segura: mundo/corpos/Step/queries básicos, mais `AetherPhysicsWorldDescV2` com quatro limites independentes, política de overflow, `StepV2` com `EPhysicsUpdateError` espelhado e estatísticas cumulativas. O símbolo V1 permanece e mapeia para defaults conservadores; o alocador temporário acompanha as capacidades configuradas | ✅ 7 testes físicos existentes + 3 testes de ABI/capacidade; pilhas densas de 500/1.000/5.000 sem overflow; probe Release retorna `0x7` e contabiliza as três categorias induzidas |
| **4.1.2 + GAP-PHY-02** | Fachada C# de física: componentes ECS `RigidBody`/`Collider` (`managed/Aether.Core/Physics/PhysicsComponents.cs`), bindings `[LibraryImport]` sobre `jolt_bridge.h` (`NativePhysics.cs`), wrapper `PhysicsWorld` dono do ponteiro nativo, e `PhysicsSyncSystem` (mesmo padrão de `TransformSystem`) que cria corpo nativo na primeira aparição de `RigidBody`+`Collider`, avança a simulação e sincroniza `WorldTransform` de volta para corpos dinâmicos. A autoridade é estático=autoria, dinâmico=Jolt→ECS e cinemático=ECS/animação→Jolt via `AetherPhysics_MoveKinematicV2`; cache de alvo impede crossings em frames estáveis e o caminho dinâmico separado evita feedback. `PhysicsWorldConfiguration`, flags e estatísticas expõem a ABI V2 sem vazar tipos Jolt | ✅ 2 testes C++ cinemáticos + 11 testes C# de fachada/sync, incluindo plataforma transladando e girando, corpo apoiado e regressão de crossings |
| **4.1.3** | Juntas e motores: extensão de `jolt_bridge.h/.cpp` com `AetherPhysics_CreateJoint/DestroyJoint/SetJointMotor/GetJointPosition` sobre as 4 juntas mais comuns do Jolt (Point, Hinge, Slider, Distance — `JPH::PointConstraint`/`HingeConstraint`/`SliderConstraint`/`DistanceConstraint`), com motor real (`JPH::MotorSettings`/`EMotorState`) em Hinge/Slider. Tabela de handles índice+geração própria em `AetherPhysicsWorld` (constraint não tem um `BodyID`-like embutido, diferente de corpo). Fachada C# em `PhysicsWorld.CreateJoint/DestroyJoint/SetJointMotor/GetJointPosition` + `JointDesc`/`JointMotor`/`PhysicsJointHandle`. Gizmo de edição no protótipo do editor (`prototype/editor.html`): novo modo "Junta" no dock, handles `jointPoint` (arrasta a âncora) e `jointAxis` (gira o eixo da dobradiça/slider), reusando `project`/`distToSegment`/`rayPlane` já existentes; seção "Junta" no Inspector com ponto, eixo, limites e motor editáveis | ✅ 8 testes C++ (`test_joint_bridge.cpp`) + 8 testes C# (`PhysicsJointTests.cs`) — corpo preso por Point não cai, Hinge respeita limites configurados, motor de Hinge mantém velocidade angular alvo, motor de Slider converge para posição alvo, Distance limita afastamento, `DestroyJoint` reativa e libera os corpos, handles inválidos/reciclados não crasham |
| **4.1.4** | Queries: extensão de `jolt_bridge.h/.cpp` com `AetherPhysics_RayCastAll` (multi-hit, via `JPH::AllHitCollisionCollector`), `AetherPhysics_ShapeCastClosest` (varredura de forma, `JPH::NarrowPhaseQuery::CastShape`) e `AetherPhysics_OverlapShape` (overlap parado, `JPH::NarrowPhaseQuery::CollideShape`) — complementam `RayCastClosest` (4.1.1). Filtro de camada (`AetherQueryLayerMask`, estático/dinâmico combinável) e de corpo a ignorar (espelha `JPH::IgnoreSingleBodyFilter`). `AetherShapeQueryHit` novo (ponto de contato nos dois lados + eixo de penetração) para os dois tipos com contato real; `RayCastAll` continua só corpo+fração (raio não gera contato Jolt sem uma segunda consulta). Todas as três seguem o padrão "buffer do chamador, devolve contagem real" (a fronteira C ABI não pode devolver `std::vector`). Fachada C# em `PhysicsWorld.RayCastAll/ShapeCastClosest/OverlapShape` sobre `Span<T>`, `QueryLayerMask`, `ShapeQueryHit` | ✅ 10 testes C++ (`test_query_bridge.cpp`) + 9 testes C# (`PhysicsQueryTests.cs`) — multi-hit ordenado por distância, contagem real excede buffer sem truncar silenciosamente, filtro de camada, `ignoreBody`, shapecast acerta o ponto certo de contato, overlap encontra/ignora por raio e por `ignoreBody` |
| **4.1.5** | Character controller sobre `JPH::CharacterVirtual` (não `JPH::Character`, o corpo rígido de verdade — `CharacterVirtual` é a classe do próprio Jolt com degraus/deslizar/troca-de-forma nativos, `Character` não tem nenhum): `AetherPhysics_CreateCharacter/DestroyCharacter/SetCharacterVelocity/GetCharacterVelocity/UpdateCharacter/GetCharacterTransform/GetCharacterGroundState/GetCharacterGroundVelocity/GetCharacterGroundNormal/SetCharacterCrouching`. `UpdateCharacter` usa `CharacterVirtual::ExtendedUpdate` (não `Update` puro) — dá degraus (`WalkStairs`, step-up 40cm default) e stick-to-floor (50cm default) de graça. `AetherShapeKind` ganhou `Capsule` (forma padrão de personagem — cilindro com tampas esféricas via `JPH::CapsuleShapeSettings`, offset para base em (0,0,0) via `RotatedTranslatedShape`, mesma exigência de `CharacterBaseSettings::mShape`). Agachar troca a forma via `CharacterVirtual::SetShape` com checagem de penetração (5cm de tolerância) — falha (sem efeito colateral) se não houver espaço. Plataforma móvel: `GetGroundVelocity()` calcula certo mesmo com corpo de suporte girando (não é `linear_velocity*dt` ingênuo); "grudar" não é automático, é o chamador somando essa velocidade à velocidade desejada antes de `SetCharacterVelocity` — documentado explicitamente na fronteira, é o padrão que o próprio Jolt pede no comentário de `ExtendedUpdate`. Tabela de handles índice+geração generalizada (antes só de juntas, `PackJointHandle`→`PackGenerationalHandle` etc, compartilhada agora entre juntas e characters). **Escalar (subir paredes) e nadar (buoyancy) não implementados** — o Jolt não tem NENHUM suporte nativo para nenhum dos dois (confirmado por leitura completa de `CharacterVirtual.h/.cpp`); ver limitação abaixo | ✅ 12 testes C++ (`test_character_bridge.cpp`) + 11 testes C# (`PhysicsCharacterTests.cs`) — inclui transporte por plataforma cinemática dirigida pelo ECS, além de queda/piso, rampas, degrau, herança de velocidade, agachar/levantar e handles geracionais |
| **4.1.8** | Determinismo cross-platform (**modo opcional de build**, não "ponto fixo" literal — o nome do item no plano diverge do que existe de verdade a implementar; ver limitação abaixo): opção CMake `AETHER_PHYSICS_DETERMINISTIC` (`native/CMakeLists.txt`) que liga `CROSS_PLATFORM_DETERMINISTIC` — nativo do próprio Jolt, não código nosso — desligando fusão multiplicação-adição (FMA, via `/fp:precise`/`-ffp-contract=off` em vez de fast-math) e travando o nível de instrução SIMD em SSE4 (`USE_AVX`/`USE_AVX2`/`USE_FMADD`/`USE_F16C` todos `OFF`; SSE4.1/4.2/LZCNT/TZCNT continuam ligados, são baseline em qualquer x86-64 relevante). Mesmo padrão de opção de build que `DOUBLE_PRECISION`/`USE_ASSERTS` já usavam neste arquivo — alterna entre duas configurações de build da MESMA lib vendorizada, escolhida em tempo de `cmake -D...` (não dois binários coexistindo escolhidos em runtime). Validado que a flag realmente muda o código gerado (não é um no-op silencioso): a mesma simulação produz posições finais numericamente DIFERENTES entre as duas builds (ex.: `x=-0.0412826203` na build normal vs. `x=-0.0412814654` na determinística, mesma semente/cenário) | ✅ 3 testes C++ novos (`test_determinism.cpp`) + reexecução dos 43 testes de física pré-existentes em ambas as configurações de build (89 execuções extras, todas idênticas) — mesma simulação (cabo de 4 corpos com juntas Hinge motorizadas caindo sobre rampa) rodada 2x e 3x seguidas no mesmo processo produz resultado bit-exato (posição+rotação+velocidade via `memcmp`), com teste de guarda contra falso positivo de "bit-exato porque nada se moveu" |
| **4.1.6** | Física 2D: **decisão documentada, não uma segunda biblioteca vendorizada.** Estende `AetherBodyDesc` com `allowedDOFs` (`AetherAllowedDOFs`, espelha `JPH::EAllowedDOFs`) — corpo 2D é o Jolt 3D já vendorizado restrito ao plano XY via `AllowedDOFs.Plane2D` (trava `TranslationZ`/`RotationX`/`RotationY`), não Box2D v3. Trancar DOFs **não reduz custo de CPU por corpo**; o ganho é arquitetural, por compartilhar mundo/broadphase. O benchmark agora usa capacidades V2 reais e inclui 5.000 corpos; na execução local mais recente, picos foram 4,24 ms (500), 43,14 ms (1.000, spike acima do orçamento) e 9,46 ms (5.000, janela reduzida). Esses números não fecham `GAP-2D-01`: ainda faltam Box2D lado a lado, p50/p95/p99 e hardware mobile | ✅ 7 testes C++ (`test_physics2d_bridge.cpp`) + 6 testes C# (`Physics2DTests.cs`) — comportamento Plane2D, assentamento, validação de DOFs e compatibilidade do default |

## O que ainda não existe

- **Validado em uma GPU física, ainda não numa matriz representativa.** O shell Android cria instance, device, surface, swapchain, pipeline e apresenta frames contra um driver Adreno real (ver "Shell Android — validação atual" abaixo), mas isso cobre só 1 dos ~6 aparelhos pedidos antes de M0 representativo. Faltam Mali e perfil C fraco.
- Existe app Android empacotável e um command loop Vulkan mínimo. Ainda não existem app iOS, cubo texturizado, depth buffer, upload de geometria/textura, toolchain reprodutível de shaders ou interop .NET↔renderer da PoC-A.
- O protótipo do editor é HTML/Canvas, não a engine — valida **interação**, não desempenho gráfico. Usa as mesmas convenções de espaço do núcleo em C# de propósito, para que o que se aprende ali transfira.
- Animação, áudio, assets, build: Fases 6 e 8, não iniciadas.
- **Física (Fase 4): 4.1.1, 4.1.2, 4.1.3, 4.1.4, 4.1.5, 4.1.6 e 4.1.8 estão prontos.** Deliberadamente ainda não implementados, para não fingir que um item complexo foi "riscado" com uma versão capenga: 4.1.7 (decomposição convexa de malhas — bloqueado por dependência real ausente: o Jolt só oferece `ConvexHullShape` de hull ÚNICO via quickhull, decomposição convexa de verdade exige uma biblioteca própria como V-HACD/CoACD, nunca vendorizada aqui; e não existe importador de malha no repositório para alimentar qualquer um dos dois), 4.1.9 (sub-stepping adaptativo ligado ao PowerGovernor térmico). Dentro do próprio 4.1.3: `SixDOFConstraint` (genérica, 6 eixos independentes) e as juntas de veículo/engrenagem/polia/path do Jolt ficam de fora — só Point/Hinge/Slider/Distance, as 4 mais comuns em jogos; cada uma das demais merece sua própria fatia testada, não um apêndice apressado. Dentro do próprio 4.1.4: **os nós Flow de física NÃO foram implementados** — decisão de escopo deliberada, ver limitação abaixo. Dentro do próprio 4.1.5: **escalar (subir paredes) e nadar (buoyancy) NÃO foram implementados** — o plano pede os dois na mesma lista de degraus/rampas/plataformas/agachar/correr, mas o Jolt não tem nenhum suporte nativo para nenhum dos dois (confirmado por leitura completa de `CharacterVirtual.h/.cpp`); cada um exigiria um subsistema próprio construído do zero (detecção de parede + override de movimento para escalar; overlap com volume de água + flutuação manual para nadar), não uma extensão isolada desta fatia. Dentro do próprio 4.1.6: **Box2D v3 NÃO foi vendorizado nem comparado lado a lado** — decisão deliberada de usar só o Jolt já vendorizado restrito ao plano XY, documentada com benchmark real (não uma omissão silenciosa); ver limitação abaixo.
- **1.4.5 (índice de dependências em SQLite) deliberadamente não implementado ainda.** Não existe um SQLite de verdade vendorizado no repositório; implementar uma versão simplificada só para "riscar o item" seria a gambiarra que este projeto se recusa a fazer. Quando entrar, será o amálgama C `sqlite3.c` vendorizado em `native/` (não um pacote NuGet — mantém a política de zero dependência externa) exposto por P/Invoke.

## Shell Android — validação atual

**Aparelho de referência (primeira entrada do laboratório de dispositivos, §5.1 de
`PLANO-FECHAMENTO-LACUNAS.md`):** Xiaomi 25053PC47G (device `onyx`, placa `sun`), SoC Snapdragon
SM8735, arm64-v8a, 11.5 GB RAM, Android 16 (API 36), Vulkan 1.3 com
`android.hardware.vulkan.compute` e deqp level 132645633 — conectado via ADB-over-WiFi.
Cobre o perfil S/A do laboratório; falta ainda um aparelho Mali e um perfil C (fraco)
para a matriz mínima de 6 aparelhos que o plano pede antes de considerar M0 representativo.

| Verificação | Resultado |
|---|---|
| APK debug ARM64 | ✅ gerado e assinado com certificado de desenvolvimento |
| APK release ARM64 | ✅ gerado sem assinatura de publicação |
| Android Lint debug/release | ✅ sem erros |
| Lifecycle portátil | ✅ 4 testes: ativação, pausa/retomada, perda de janela e encerramento |
| Instalação em aparelho físico | ✅ `adb install -r` — sucesso, sem erro de assinatura/ABI |
| Carregamento da lib nativa | ✅ confirmado via logcat (`nativeloader: Load .../libaether_android.so ... ok`) — `android_main` executa (`Aether.Android: Shell nativo iniciado.`) |
| Abertura em aparelho físico | ✅ `NativeActivity` inicia, fica em foreground (`mFocusedApp` confirmado via `dumpsys activity`), sem `FATAL EXCEPTION`/crash no logcat |
| **Criação real de surface Vulkan** | ✅ **confirmado em hardware físico** — log `Aether.Android: Surface Vulkan pronta: janela=2772x1280, imagens=4..64, fila=0.` contra o driver Adreno real (`AdrenoVK-0`, `vulkan.adreno.so` versão 0800.71, Qualcomm build `208ca19915`) |
| Criação real de swapchain | ✅ `Swapchain pronta: 2772x1280, 5 imagens.`; formato/present usam capability selection e FIFO |
| Pipeline e apresentação | ✅ render pass, pipeline, command pool/buffer, semáforos/fence, submit e present produzem triângulo RGB visível em screenshot 2772×1280 |
| Runner ADB reproduzível | ✅ `tools/validate-android-shell.ps1` instala/limpa, inventaria o aparelho, espera 1.000 frames, marca cada transição no Logcat, detecta crash/ANR/erro Vulkan, captura evidências e restaura o estado global do aparelho em caso de sucesso ou falha |
| Ciclo background→foreground | ✅ **100/100 ciclos automatizados**: mesmo PID `14366`, suspensão e retomada observadas, primeiro frame apresentado após cada retorno, sem ANR/crash. Neste driver a mesma `ANativeWindow` foi retida nos 100 ciclos, um comportamento válido do Android; o teste não exige recriação inexistente |
| Recriação por mudança de configuração | ✅ alternar e restaurar `uiMode` gerou `APP_CMD_CONFIG_CHANGED` nas duas direções, com swapchain/pipeline reconstruídos e PID preservado. Uma rodada manual anterior também percorreu destruição/recriação completa da janela; injeção determinística de `SurfaceLost` ainda não existe no runner |
| Estabilidade visual | ✅ screenshots antes/depois dos 100 ciclos e das duas mudanças de configuração possuem o mesmo SHA-256 `949E1829D4D53BDC63F1B548C88ECF05ECD27FEDD47E8E03FDF4FDB9B2A59719` |
| Memória baixa simulada | ✅ `am send-trim-memory RUNNING_CRITICAL` — processo sobrevive, sem crash (não dispara `APP_CMD_LOW_MEMORY` de verdade, é um proxy do ADB, não o teste completo do plano) |
| Tela em landscape imersivo | ✅ confirmado por screenshot (`screencap`) — 2772×1280, sem barras de sistema visíveis |
| Frame visível | ✅ fundo `#05050d` e triângulo RGB interpolado; a antiga tela preta deixou de ser o resultado esperado |
| Screen off/on | ✅ ensaio automatizado separado: `Asleep`/`Awake` sincronizados via `dumpsys power`, PID preservado, surface/swapchain/pipeline recriados, primeiro frame apresentado, modo imersivo restaurado e captura final idêntica |
| Rotação para além de landscape e matriz de múltiplos aparelhos | ⏳ `uiMode` foi exercitado; rotação fora de landscape é bloqueada por design por `screenOrientation="sensorLandscape"`; só há um aparelho no laboratório (faltam Mali e perfil C) |

O APK é um **shell gráfico de fundação**, não um editor demonstrativo. Ele
desenha continuamente apenas quando lifecycle está ativo e bloqueia o looper
quando suspenso ou sem renderer, evitando consumo térmico fora de foreground.

## O que é protótipo ou prova de conceito

| Artefato | Natureza | O que prova | O que não prova |
|---|---|---|---|
| `prototype/editor.html` | Protótipo interativo HTML/Canvas | UX de câmera, seleção, gizmos, menu radial e Inspector em landscape | Renderer, desempenho mobile ou integração com a engine |
| `Aether.Flow` | PoC arquitetural funcional | AST única, validação, interpretação, serialização e round-trip parcial com C# | Catálogo completo de nós, CFG geral, integração com Inspector/runtime |
| `native/rhi/device.cpp` + shell Android | Vertical slice gráfico validado em 1 GPU física | Instance/device/surface, swapchain, acquire/submit/present e reconstrução completa de um pipeline mínimo | Compatibilidade na matriz de drivers, RHI de recursos, Render Graph executado ou desempenho da cena M2 |
| Device profiles e render graph | Implementação headless testada | Regras de capability, ordenação, barreiras, aliasing e fusão | Execução dessas decisões numa GPU real |

## Limitações conhecidas que um leitor precisa saber

| Onde | Limitação | Consequência |
|---|---|---|
| `.github/workflows/*.yml` | Nenhum workflow rodou no GitHub ainda — o repositório não tem remote configurado (`git remote -v` vazio) | YAML foi parseado e o caminho limpo exato foi reproduzido localmente, incluindo headers Vulkan isolados, 242/242 testes C# e 96/96 nativos. A primeira execução hospedada ainda é necessária para validar permissões, cache, publicação de artefatos e ambiente dos runners |
| `World.GetComponent<T>`/`Chunk.GetSpan<T>` | APIs legadas continuam marcando escrita em toda chamada porque devolvem acesso mutável | Compatibilidade é preservada sem falsos negativos. Código novo e todos os sistemas internos usam `Read`/`Write` e spans explícitos; remoção das APIs antigas exige janela de depreciação |
| `Aether.Flow` | Biblioteca de nós mínima (~12 nós), não o catálogo da Parte 9.5 | Prova a tese, não entrega o produto |
| `prototype/editor.html` | Rasterização por painter's algorithm em Canvas 2D | Artefatos de ordenação entre objetos grandes que se interpenetram. Irrelevante para o que o protótipo testa |
| `VulkanSwapchain` | Um único frame em voo e máximo técnico atual de 8 imagens | Correto para o shell gráfico mínimo; múltiplos frames em voo, pacing e política dinâmica pertencem ao RHI completo da Onda 3 |
| `TriangleRenderer` | Render hardcoded, sem depth, vertex buffer, textura ou Render Graph | Prova o pipeline Vulkan e lifecycle, não fecha PoC-A/M0 nem representa a arquitetura final de cena |
| `triangle_spirv.h` | SPIR-V embutido foi gerado manualmente; alterar `.vert/.frag` exige regenerar o header | Ainda não há toolchain reprodutível de shaders/cache/reflection; isso permanece no item 2.2/Onda 3 |
| `TextSerializer` | Migração de esquema por nome/valor de campo lido no esquema ATUAL, não por bytes crus como o binário | Um campo renomeado entre versões perde o valor num arquivo texto antigo; a migração de verdade só é garantida no formato binário. Intencional (o plano só pede migração para o binário), documentado no cabeçalho de `TextSerializer.cs` |
| `PhysicsSyncSystem` × dirty tracking do ECS | A sincronização cinemática mantém cache de alvo por body mesmo com versões exatas por chunk/coluna | A versão elimina trabalho quando uma coluna inteira está estável, mas não identifica qual body mudou nem conserva o alvo anterior necessário à parada final. Integrar a versão como fast-path é otimização futura; o cache por body continua sendo a fonte correta |
| `Aether.Physics` | `Collider`/`RigidBody` não têm um componente `Trigger` (citado no plano junto dos outros dois) | Sem volume de detecção de overlap sem resposta física PERMANENTE (corpo continua com colisão sólida). `OverlapShape` (4.1.4) já resolve a query pontual "quem está sobrepondo esta forma agora" — o que falta é `mIsSensor` na fronteira nativa (não exposto hoje) para um CORPO inteiro nunca gerar resposta física, mais eventos de entrada/saída persistentes entre frames (nenhum dos dois é OverlapShape, que é uma pergunta instantânea, não um corpo permanente) |
| `Aether.Physics` | Cada `CreateBody`/`DestroyBody` é uma chamada P/Invoke individual (não em lote) | Dentro do orçamento de 200 chamadas nativas/frame (`docs/CONVENCOES.md` §2) para criação/destruição normal (evento raro), mas um spawn de centenas de corpos no mesmo frame estouraria o orçamento sem alguém perceber — não há guarda automática contra isso ainda |
| `jolt_bridge.h` (juntas) | `AetherJointDesc` só expõe `EConstraintSpace::WorldSpace` — não há como descrever uma junta no referencial local de um corpo | Criar uma junta antes de posicionar os corpos no lugar final não funciona direito (os pontos são absolutos, não relativos); é preciso posicionar os corpos primeiro, depois criar a junta com coordenadas mundiais. Espaço local é um incremento futuro, documentado no comentário de `AetherJointDesc` |
| `HingeConstraint` do Jolt | `mLimitsMin`/`mLimitsMax` são exigidos em `[-pi,0]`/`[0,pi]` — não existe "sem limite" fora dessa faixa como em Slider (`FLT_MAX`) | Uma dobradiça de rotação livre contínua (ex.: roda motorizada) precisa usar exatamente `-pi`/`+pi`, não um valor "bem grande" qualquer — é o ponto exato em que o Jolt desliga a checagem de limite internamente (`HingeConstraint::SetLimits`). Documentado em `jolt_bridge.h`, `PhysicsWorld.cs` (`JointDesc`) e nos testes |
| `PhysicsSyncSystem`/fachada de juntas | Nenhuma sincronização automática entre `RigidBody`/componente ECS de junta — `PhysicsWorld.CreateJoint` é chamado direto pelo código do usuário, não por um `JointSyncSystem` análogo ao `PhysicsSyncSystem` de corpos | Não existe hoje um componente ECS `Joint`/`HingeJoint` que o `PhysicsSyncSystem` resolva automaticamente a partir de duas entidades — a fachada 4.1.3 é a API C# (`PhysicsWorld.CreateJoint`), não um componente declarativo. Adicionar isso é extensão natural, não escopo deste item (que era "juntas e motores" na física, não "juntas no ECS") |
| `Aether.Flow` | Nenhum nó de física (`physics.raycast`, `physics.overlap`, etc.) — o plano (tabela 9.5) reserva a categoria "Física" com nós nomeados, mas eles não existem | O item 4.1.4 diz "queries expostas a script E a nós" — a parte "script" está feita (`PhysicsWorld` é chamável direto por qualquer C#, mesmo padrão que os próprios testes usam); a parte "nós" foi deliberadamente adiada. Motivo: o Aether.Flow hoje só conhece o ECS `World`, nunca um `PhysicsWorld` — não existe um jeito do interpretador/gerador de código referenciar um mundo de física em contexto (não há `Behavior`/API de gameplay da Parte 11.3 do plano ainda implementada, que é onde o plano prevê essa ponte). Resolver isso de verdade é decisão arquitetural maior (como um nó Flow acessa recursos externos ao ECS), não uma extensão isolada de "adicionar mais um nó" — forçar um `PhysicsContext.Current` estático só para destravar os nós seria a gambiarra que este projeto se recusa a fazer |
| `AetherPhysics_CreateCharacter` | Sempre cria com a forma DE PÉ (`StandingHalfHeight`) — não há "criar já agachado" | Nascer num espaço apertado demais para a forma de pé (ex.: dentro de um vão baixo) faz a resolução de penetração da CRIAÇÃO empurrar o personagem para uma posição inesperada — geralmente para cima de um teto fino, não para o chão abaixo — antes mesmo de um `SetCharacterCrouching(1)` seguinte ter qualquer efeito (`SetShape` só age sobre a posição atual, não reposiciona pela geometria da forma NOVA). Para entrar num vão baixo, crie o personagem num espaço livre, agache, e só então mova-o até lá — documentado em `jolt_bridge.h` |
| `CharacterVirtual::Update`/`UpdateCharacter` | Nunca integra gravidade na velocidade vertical do próprio personagem — só usa `gravity` para empurrar objetos abaixo dele | Responsabilidade inteira do chamador (mesmo padrão documentado no comentário oficial de `CharacterVirtual::ExtendedUpdate` do Jolt, não invenção nossa): esquecer de somar gravidade a `SetCharacterVelocity` a cada frame faz o personagem flutuar. Ver `PhysicsCharacterTests.cs`/`test_character_bridge.cpp` para o padrão de acumulação correto — e note que o padrão usado nos testes de DEGRAU é deliberadamente diferente (velocidade vertical reinicia a cada frame, não acumula) porque acumular atrapalha `WalkStairs`; ver limitação abaixo |
| Testes de degrau (`Character_SobeDegrauComUpdateCharacter`) | A velocidade vertical usada para "andar" NÃO acumula frame a frame (reinicia em `-g*dt` a cada chamada, não em `v_anterior - g*dt`) | Padrão deliberado, não descuido: acumular gravidade livremente faz o personagem ganhar momento de queda vertical significativo mesmo andando devagar sobre um piso, o que atrapalha `WalkStairs` a "ver" o degrau como subível (o character parece estar caindo rápido demais para o algoritmo tentar). É diferente do padrão de queda livre pura (`StepCharacterFreefall`, que acumula normalmente) — cada cenário de movimento tem sua própria composição de velocidade, não existe uma fórmula universal |
| Character sobre objeto fino no caminho vertical | O algoritmo de recuperação de penetração do Jolt sempre resolve pelo caminho de MENOR penetração — um personagem que penetra pouco um teto fino por cima mas muito o chão por baixo é empurrado para CIMA do teto, não mantido no chão | Medido experimentalmente ao desenhar o teste de "agachar sob teto baixo": um vão menor que a altura da forma agachada (ex.: 1.2m de vão para uma cápsula de 1.4m agachada) faz o personagem "vazar" para cima de um teto de 0.1m, mesmo estando exatamente encostado no chão (penetração zero) — não é bug nosso, é o comportamento correto do algoritmo para uma geometria que de fato não comporta a forma. O vão precisa ter folga real (testado com 0.2m de sobra) para o teste ser sobre a mecânica de agachar, não sobre essa interação de geometria-limite |
| Física 2D (`AetherAllowedDOFs`) | Box2D v3 nunca foi trazido ao repositório — a "decisão por benchmark" que o plano pede é entre Jolt-restrito e o Jolt-restrito em si (não há um segundo concorrente medido lado a lado) | O benchmark (`benchmark_physics2d.cpp`) mede throughput real do Jolt restrito ao plano XY, não uma comparação Jolt-vs-Box2D — documentado explicitamente no cabeçalho do arquivo e na tabela acima para não sugerir uma comparação que não existe. Se no futuro Box2D v3 for vendorizado para comparação de verdade, este item precisa ser revisitado (não é definitivo, é a decisão tomada com a informação e o orçamento de escopo desta fatia) |
| `EAllowedDOFs` travado | Travar graus de liberdade no Jolt NÃO reduz o custo de CPU por corpo no solver — é uma restrição de comportamento/correção, não uma otimização de performance | Confirmado por leitura completa de `MotionProperties.h/.cpp/.inl`: toda operação de massa/inércia/força é calculada em SIMD 3-wide completo e só MASCARADA (zerada seletivamente) depois — não há early-exit por DOF travado em nenhum lugar do motor. Um corpo `Plane2D` custa aproximadamente o mesmo que um corpo 3D pleno de mesma forma; o benchmark de 4.1.6 mede throughput absoluto do Jolt, não um ganho de "modo 2D restrito" sobre "modo 3D" |
| `AETHER_PHYSICS_DETERMINISTIC` (4.1.8) | O nome do item no plano ("determinismo em ponto fixo") diverge do que foi implementado (`CROSS_PLATFORM_DETERMINISTIC` do Jolt, que continua usando `float`, não ponto fixo inteiro) | Ponto fixo de verdade exigiria reescrever o solver/matemática interna do Jolt inteiros para um tipo fixed-point — reescrita profunda de código de terceiros vendorizado, não uma opção de configuração; fora de escopo desta fatia. `CROSS_PLATFORM_DETERMINISTIC` é o que o próprio Jolt oferece nativamente para o mesmo objetivo prático (reprodutibilidade entre plataformas), sem tocar uma linha do código dele — ver comentário completo em `native/CMakeLists.txt` |
| `AETHER_PHYSICS_DETERMINISTIC` (4.1.8) | Os testes de `test_determinism.cpp` provam determinismo RUN-TO-RUN (mesma máquina/build, múltiplas execuções bit-exatas), não determinismo CROSS-PLATFORM de verdade | Provar cross-platform de verdade exigiria rodar a mesma simulação em hardware/SO fisicamente diferentes e comparar os resultados — fora do alcance de uma suíte de teste rodando numa única máquina de CI/dev. Run-to-run é pré-requisito necessário (se a mesma máquina não reproduz a própria simulação, nenhuma outra reproduziria) mas não suficiente sozinho; a garantia cross-platform vem da CONFIGURAÇÃO de build (documentada e aceita pelo próprio Jolt), não de algo que este teste consiga verificar sozinho. Documentado no cabeçalho de `test_determinism.cpp` |

## Correções desta revisão (não são limitações — já resolvidas)

- **§4.2 (CI confiável) — skip silencioso de teste de física deixou de ser indistinguível de
  "passou de verdade".** Os 43 testes C# que tocam `aether_physics` (Physics/PhysicsJoint/
  PhysicsQuery/PhysicsCharacter/Physics2DTests) tinham cada um sua própria cópia de
  `NativeLibraryAvailable()` e retornavam cedo silenciosamente quando a DLL nativa não estava
  presente — um `TUDO VERDE` local não distinguia "43 testes passaram" de "43 testes nunca
  rodaram". Centralizado em `NativeInterop.PhysicsLibraryAvailable()` (conta quantos testes
  pularam por ausência da lib) e o `TestRunner` agora aplica a regra do plano: com
  `AETHER_REQUIRE_NATIVE=1` (o job de integração), qualquer skip por lib nativa ausente vira
  falha explícita com contagem; sem a variável (fluxo de desenvolvimento local sem toolchain
  nativa), o comportamento continua idêntico ao de antes. Validado nos dois sentidos: com a DLL
  presente `AETHER_REQUIRE_NATIVE=1` também dá `TUDO VERDE` (não é um modo mais rígido por
  acaso, só quando a causa real de skip é ausência de lib); com a DLL removida, falha com exit
  code 1 e a mensagem nomeia os 43 testes silenciosos. `unit` (padrão) e `interop`
  (`AETHER_REQUIRE_NATIVE=1`) continuam o mesmo executável, mas agora rodam em jobs separados.
  O registro `metrics/budgets.v1.json` cobre os seis domínios exigidos e o validador
  rejeita tanto contrato incompleto quanto valor fora do limite; a regressão de 301
  chamadas/frame falha de propósito. O clean build também foi corrigido para compilar o
  nativo antes do gerenciado e usa somente os headers `vulkan`/`vk_video` extraídos do NDK,
  evitando contaminar o build host com headers C do Android.
- **`GAP-FLOW-01` — controle terminal simétrico no AetherFlow.** `flow.return`,
  `flow.break` e `flow.continue` são nós explícitos sem saída de execução. Parser,
  gerador, interpretador, validador e serializer compartilham o mesmo contrato;
  sinais atravessam `if` aninhados, `return` encerra o evento e `break`/`continue`
  são consumidos apenas pelo laço mais interno. O parser rejeita retorno com valor
  em eventos `void` e instrução inalcançável em vez de reinterpretá-los. Nove
  regressões novas incluem laços aninhados, round-trip `.aflow` e compilação real
  offline do C# original/regenerado; 43/43 testes Flow e 242/242 testes C# passam.
- **`GAP-PHY-01` — capacidades e overflow de física explícitos.** A ABI V2
  separa `maxBodies`, `maxBodyPairs`, `maxContactConstraints` e
  `maxBroadPhasePairs` (`mMaxInFlightBodyPairs` no Jolt), versiona descritores,
  preserva V1 e dimensiona memória transitória pela carga declarada. `StepV2`
  devolve flags e mantém contadores por categoria; logs usam primeira ocorrência
  e potências de dois para limitar repetição. Debug/teste permanece fail-fast;
  o probe Release comprovou warning recuperável e `flags=0x7`. Três regressões
  nativas exercitam ABI, V1 e pilhas densas; três regressões C# exercitam a fachada.
- **`GAP-PHY-02` — cinemáticos com autoridade ECS→Jolt.** A ABI versionada
  expõe `AetherPhysics_MoveKinematicV2`; `PhysicsSyncSystem` envia o alvo antes do
  step e nunca devolve a transform cinemática pelo caminho dinâmico. Um cache
  exato por body envia somente mudanças reais e uma parada final, mantendo frames
  estáveis sem P/Invoke. Regressões cobrem alvo traduzido/rotacionado, transporte
  de corpo apoiado e character, e a ausência de feedback/crossings permanentes.
- **`GAP-JOB-01` — ciclos gerais detectados antes da espera.** `JobEntry`
  registra pré-requisitos permanentes e a aresta temporária de cada `Complete()`
  interno. A publicação e a busca de caminho ocorrem sob o mesmo lock, então
  duas threads não conseguem fechar um ciclo sem detecção. A mensagem lista o
  caminho `job#N -> ... -> job#N`; o timeout ficou restrito a watchdog externo.
  Três regressões adicionais cobrem ciclos por dependência e ciclos concorrentes
  de 2–3 jobs; 10 repetições da suíte completa passaram.
- **Shell gráfico Vulkan — quatro falhas de lifecycle que a captura visual feliz não
  denunciava.** (1) A primeira implementação recriava a swapchain antes de destruir os
  framebuffers do `TriangleRenderer`; `vkDeviceWaitIdle` evita uso pela GPU, mas não muda a
  regra de lifetime: uma `VkImageView` não pode ser destruída enquanto um `VkFramebuffer`
  ainda a referencia. A ordem agora é renderer/framebuffers → image views/swapchain →
  surface/device. (2) Se `vkCreateSwapchainKHR` falhasse, a swapchain antiga era destruída,
  embora a spec só a aposente quando a criação da nova tem sucesso; uma falha recuperável de
  resize virava perda completa do renderer. A troca agora preserva a antiga na falha e só
  publica os novos handles depois de obter imagens/views válidas. (3) `SurfaceLost` e
  `OutOfDate` eram reduzidos ao mesmo `bool`; tentar resolver surface perdida recriando só a
  swapchain não pode funcionar. `SwapchainStatus` ganhou `FatalError`, o renderer preserva os
  estados e o shell escolhe entre reconstruir swapchain ou surface inteira. (4)
  `compositeAlpha=OPAQUE` era presumido; agora é escolhido entre os bits realmente suportados,
  e uso de color attachment/limite de imagens são validados antes da criação.
- **Build host Windows — alocação alinhada portável.** O clean build com LLVM-MinGW revelou
  que essa CRT não fornece `std::aligned_alloc` de forma utilizável. `alignedAlloc/alignedFree`
  agora usam o par obrigatório `_aligned_malloc/_aligned_free` em `_WIN32` e preservam
  `std::aligned_alloc/free` nas demais plataformas. Os seis testes nativos de allocator passam
  no build host limpo e o mesmo header compila pelo NDK no APK Android.
- **Shaders do triângulo conferidos contra a fonte.** `triangle.vert` e `triangle.frag` foram
  recompilados com o `glslc -O` do NDK 27.1, validados com `spirv-val` e comparados byte a byte
  com os arrays de `triangle_spirv.h`: SHA-256 idênticos para vertex e fragment. Isso elimina
  divergência atual entre fonte e embed, embora a geração automática continue pendente na
  toolchain de shaders 2.2.
- **`ComponentRegistry` calculava offset de campo com `Marshal.OffsetOf`**, que reflete o layout de
  interop/marshaling, não o layout gerenciado real que o resto da engine usa (`Unsafe`/`MemoryMarshal`).
  `bool` marshala como o `BOOL` de 4 bytes do Win32 por padrão; comprovado experimentalmente nesta
  máquina que isso diverge do offset real sempre que um `bool` é seguido por um campo sem exigência de
  alinhamento de 4 bytes (ex.: outro `bool`/`byte`) — nenhum componente registrado até agora tinha
  `bool`, então o bug era invisível. Corrigido para calcular o offset observando o layout gerenciado de
  verdade (preenche o campo com um valor "todo-bits-1" via reflexão direta, sem marshaling, e observa
  qual byte mudou). Teste de regressão:
  `OffsetDeCampoAposBool_NaoUsaLayoutMarshaledDoInteropERespeitaOTamanhoRealDaStruct`.
- **`Aether.Flow`: `if` já pode ser seguido de mais instruções no mesmo bloco.** `flow.if` ganhou um
  terceiro pino de execução, `"depois"` — o ponto de convergência dos ramos `"entao"`/`"senao"`, existindo
  `else` ou não — espelhando exatamente o padrão que `flow.while`/`"fim"` já usava. Gerador, parser e
  interpretador dos três atualizados de forma simétrica; validador e serializador de `.aflow` já eram
  genéricos sobre pinos e não precisaram mudar. Corrige também um bug latente (nunca exercitado, porque a
  checagem antiga sempre lançava antes): `CSharpToFlow.ParseIf` devolvia `"senao"` como o pino de
  continuação, quando esse pino é a ENTRADA do ramo else, não uma saída de convergência.
- **Integração do Jolt (4.1.1): três bugs reais pegos só ao compilar/testar de verdade, não por inspeção.**
  (1) `JPH::RVec3` é um `using RVec3 = Vec3` (não um tipo distinto) quando `DOUBLE_PRECISION=OFF` — a
  configuração que escolhemos, porque a engine usa float em toda a matemática — então um segundo overload
  `FromJolt(JPH::RVec3)` ao lado de `FromJolt(JPH::Vec3)` era uma redefinição da mesma função, não uma
  sobrecarga; o compilador acusou. (2) `BroadPhaseLayerInterface::GetBroadPhaseLayerName` é puro quando
  `JPH_PROFILE_ENABLED` está definido (ligado por padrão no build Debug do Jolt) — `BroadPhaseLayerInterfaceImpl`
  não a implementava, então a classe ficava abstrata e nem podia ser instanciada como campo de
  `AetherPhysicsWorld`; corrigido implementando-a (só usada para rotular camadas em captura de profile, não
  afeta simulação). (3) Os handlers padrão do Jolt para `Trace`/`AssertFailed` (`DummyTrace`/`DummyAssertFailed`,
  ver `Jolt/Core/IssueReporting.cpp`) descartam a mensagem e o assert simplesmente devolve `true` — dispara
  um trap sem nenhum diagnóstico legível. Instalados handlers reais em `jolt_bridge.cpp`
  (`TraceImpl`/`AssertFailedImpl`), no mesmo estilo stderr de `core/assert.cpp` (`AE_CHECK`), antes de
  qualquer uso do Jolt (`EnsureGlobalTypesRegistered`). Além disso, o teste de queda livre precisou de uma
  tolerância maior para a velocidade (0.15 m/s, não 0.05) depois de medir que o `mLinearDamping` padrão do
  Jolt (0.05, `dv/dt = -c*v`, ligado por padrão em todo corpo dinâmico) desvia a velocidade real da fórmula
  ideal `v = -g*t` em ~0.06 m/s mesmo em queda livre "pura" — não é imprecisão do teste, é o Jolt simulando
  um amortecimento real que a fórmula ideal não modela.
- **Fachada C# de física (4.1.2): primeiro P/Invoke real do repositório — não havia nenhum precedente**
  (`[DllImport]`/`[LibraryImport]`) em `managed/`. Duas decisões de fronteira que valem registrar: (1) o
  CMake só produzia `aether_physics` como biblioteca ESTÁTICA (`native/CMakeLists.txt`); P/Invoke exige uma
  biblioteca dinâmica carregável em runtime, então foi adicionado um alvo irmão `aether_physics_shared`
  (`SHARED`) compilando o mesmo `jolt_bridge.cpp` — os dois alvos coexistem (o estático continua linkado
  direto no executável de teste nativo `aether_tests`, sem carregamento em runtime). (2) O nome do artefato
  gerado por padrão diverge da convenção de resolução do .NET: MinGW/CMake no Windows prefixam `lib` por
  padrão (`libaether_physics.dll`), mas `[LibraryImport("aether_physics")]` resolve para
  `aether_physics.dll` (sem prefixo) no Windows e `libaether_physics.so` (com prefixo) no Linux — são
  convenções de plataforma DIFERENTES. Corrigido com `set_target_properties(... PROPERTIES OUTPUT_NAME
  "aether_physics")` mais `PREFIX ""` condicionado a `WIN32` (o Linux já usa o prefixo default do CMake, que
  bate com o que o runtime espera). Sem isso, `DllNotFoundException` em runtime — nenhum erro de compilação
  denunciaria o problema. `float3`/`quaternion` (item 1.5) são reusados diretamente como parâmetros
  blittable dos bindings — têm o mesmo layout de `AetherVec3`/`AetherQuat`, evitando duplicar um par de
  structs só para a fronteira P/Invoke.
- **Juntas e motores (4.1.3): três bugs reais pegos só ao rodar o teste real contra o Jolt, não por
  inspeção.** (1) Criar uma junta trava os dois corpos com `JPH::BodyLockWrite` (necessário porque
  `Constraint::Create` pede `Body&`, não `BodyID` — diferente do resto da fronteira, que só usa
  `BodyInterface`); dois `BodyLockWrite` sequenciais disparam o assert de possível deadlock do Jolt
  (`PhysicsLock.h`: "A lock of same or higher priority was already taken") porque o segundo pega a
  MESMA categoria de mutex (`PerBody`) que o primeiro já detém. Corrigido trocando por
  `JPH::BodyLockMultiWrite`, que trava N corpos de uma vez resolvendo a ordem de mutex internamente —
  e o mesmo problema reapareceu entre o escopo desse lock e a chamada de `ActivateBody` logo depois
  (ver item 3), corrigido isolando a criação da constraint num bloco `{}` próprio para o lock ser
  liberado antes do `ActivateBody`. (2) Um corpo mantido parado por uma junta (ex.: pêndulo em
  repouso) é colocado para dormir pelo Jolt como qualquer corpo dinâmico inativo — e destruir a junta
  NÃO o acorda sozinho: ficava "congelado" no ar indefinidamente até algo mais o acordar por acidente.
  Corrigido chamando `JPH::BodyInterface::ActivateBody` nos dois corpos tanto em `CreateJoint`
  (senão a junta nova "não pega" num corpo já dormindo) quanto em `DestroyJoint` (senão o corpo solto
  não recomeça a cair). (3) Os testes de motor (Hinge velocidade, Slider posição) inicialmente
  "falhavam" com o motor girando/convergindo bem mais devagar/menos que o esperado — não era bug de
  sinal ou eixo, era `maxForceOrTorque`/limite de força do motor de teste baixo demais para a
  inércia/massa do corpo (mesma classe de pegadinha do `mLinearDamping` documentada para 4.1.1): com
  torque/força uma ou duas ordens de grandeza maiores, ambos os motores convergem corretamente para o
  alvo. Documentado nos comentários dos testes (`test_joint_bridge.cpp`, `PhysicsJointTests.cs`) para
  não ser "corrigido" de volta por engano numa revisão futura achando que é imprecisão de teste.
- **`prototype/editor.html` não tinha `<meta charset="utf-8">`** (é um fragmento HTML solto, sem
  `<head>`). Servido por um servidor HTTP que não declara `charset=utf-8` no header
  `Content-Type` (ex.: `python -m http.server` puro), o navegador cai para Latin-1 por padrão e
  corrompe todo texto acentuado do editor ("Chão" vira "ChÃ£o") — bug pré-existente, não introduzido
  nesta revisão, mas só descoberto ao validar o gizmo de junta num servidor local de teste (abrir o
  arquivo direto via `file://` não expõe o problema, porque não há header HTTP envolvido). Corrigido
  adicionando a meta tag.
- **Queries (4.1.4): um bug de ergonomia de API pego na revisão do próprio código, antes de rodar
  qualquer teste.** O parâmetro opcional `ignoreBody` de `RayCastAll`/`ShapeCastClosest`/`OverlapShape`
  tinha `PhysicsBodyHandle ignoreBody = default` — mas `PhysicsBodyHandle.Invalid` é `static readonly`
  (construído em runtime), não `const`, então não pode ser o valor default de um parâmetro; o C#
  aceita silenciosamente `default(PhysicsBodyHandle)` (campo `Value=0`) em seu lugar. O problema:
  `Value=0` é um handle REAL e válido (corpo de índice 0), não "nenhum corpo para ignorar" — um
  chamador que não passasse `ignoreBody` explicitamente estaria, sem saber, ignorando qualquer corpo
  que por acaso fosse o primeiro criado no mundo. Corrigido trocando a assinatura para
  `PhysicsBodyHandle? ignoreBody = null`: `null` (ausência real de valor) é inequivocamente distinto
  de qualquer handle, válido ou `Invalid`, que o chamador possa passar. Também extraída a conversão
  `AetherShapeDesc→JPH::Shape` de dentro de `AetherPhysics_CreateBody` para uma função `ToJoltShape`
  compartilhada (antes só existia inline em `CreateBody`), reusada pelas três queries que também
  precisam de forma sem criar um corpo.
- **Character controller (4.1.5): nenhum bug de compilação/deadlock desta vez (ao contrário de
  4.1.3), mas três horas de geometria de teste mal calculada até acertar cenários que refletissem
  o comportamento real do Jolt** — vale registrar para não repetir o mesmo erro em testes
  futuros de personagem/colisão. (1) Uma caixa larga rotacionada para simular "parede/rampa
  íngreme" não cobre a mesma área XZ que a caixa original antes de rotacionar — um personagem
  posicionado na mesma coordenada X/Z da versão não-rotacionada cai no vazio ao lado dela,
  reportando `InAir` (não `OnSteepGround`), um falso negativo que parece "o Jolt não detectou a
  rampa íngreme" quando na verdade é "o personagem nunca tocou a rampa". Resolvido calculando a
  posição real da face rotacionada antes de posicionar o personagem, não chutando a mesma
  coordenada da caixa não-rotacionada. (2) Mover um personagem horizontalmente contra a QUINA de
  um objeto fino (quando a intenção era "entrar por baixo dele") trava na borda — `WalkStairs`
  só dispara com movimento horizontal desejado e a normal da quina pode ser classificada como
  "rampa íngreme demais" mesmo sem ser uma rampa de verdade, cancelando o avanço via
  `CancelVelocityTowardsSteepSlopes`. Resolvido evitando esse caminho de teste inteiramente:
  entrar num vão baixo tem que ser testado com o personagem JÁ POSICIONADO dentro do vão (mesma
  disciplina documentada na limitação de `AetherPhysics_CreateCharacter` acima), não simulando
  uma aproximação lateral genérica. (3) O extrato de estado do `AetherCharacterGroundState` no
  código de teste inicial media só a posição/estado FINAL depois de várias dezenas de frames —
  para rampas íngremes isso é sempre ambíguo (o personagem pode ter escorregado de volta para
  `InAir`, que parece um resultado "neutro" mas na verdade mascara se a detecção de rampa íngreme
  funcionou). Corrigido medindo o PRIMEIRO frame em que o estado deixa de ser `InAir`, não o
  estado final — o padrão usado em `character_sobre_rampa_ingreme_demais_reporta_steep_ground`/
  `Character_ContraRampaIngremeDemais_PrimeiroContatoNaoEhOnGround`.
- **Física 2D (4.1.6): o próprio benchmark de carga alta (1000 corpos empilhados) pegou um bug
  real de dimensionamento pré-existente em `AetherPhysicsWorld`** — `maxBodyPairs`/
  `maxContactConstraints` (jolt_bridge.cpp, construtor de `AetherPhysicsWorld`) são dimensionados
  como `max(1024, maxBodies)`, ou seja, um valor por CORPO. Mas o Jolt limita PARES/CONSTRAINTS
  de contato simultâneos, não corpos — um cenário denso (muitos corpos empilhados tocando vários
  vizinhos ao mesmo tempo) gera mais pares de contato que corpos, e o teto insuficiente faz o
  Jolt descartar contatos silenciosamente em produção (`JPH::EPhysicsUpdateError::
  BodyPairCacheFull`/`ContactConstraintsFull`) ou abortar com o assert de debug instalado em
  4.1.1 (`AssertFailedImpl`). Não chamamos isso de "corrigido na fronteira" — não é um bug de
  4.1.6 em si, é uma limitação de dimensionamento que já existia desde 4.1.1 e nunca tinha sido
  exercitada por nenhum teste anterior (o maior cenário de física antes deste item envolvia
  poucos corpos por vez). O benchmark contorna isso passando um `maxBodies` maior que o número
  de corpos reais (4x de folga) ao criar o mundo — não alterei o dimensionamento interno do
  `AetherPhysicsWorld`, porque a correção de verdade (separar `maxBodies` de
  `maxBodyPairs`/`maxContactConstraints` como parâmetros independentes na API pública) é uma
  mudança de ABI que merece ser feita deliberadamente, não como efeito colateral de um
  benchmark — fica registrado aqui como candidato a próxima limitação a resolver, não como algo
  já corrigido.
- **Determinismo cross-platform (4.1.8): decisão de escopo tomada ANTES de escrever código, não
  uma correção de bug.** O plano pedia "determinismo em ponto fixo (modo opcional)" — investigação
  prévia (leitura do solver do Jolt) confirmou que não existe modo fixed-point no Jolt: ele usa
  `float`/`Vec4`/SIMD em toda a matemática interna, e trocar isso por ponto fixo seria reescrever
  o solver de terceiros vendorizado por inteiro, não configurar uma flag. O item 4.1.7
  (decomposição convexa) foi avaliado junto e descartado desta rodada pelo mesmo motivo de
  disciplina: o Jolt só tem `ConvexHullShape` de hull ÚNICO (quickhull), decomposição convexa real
  precisa de uma lib externa (V-HACD/CoACD) nunca vendorizada, e não há importador de malha no
  repositório para alimentar nenhum dos dois — implementar "decomposição" que decompõe em UM hull
  só seria a gambiarra que este projeto se recusa a fazer. Optou-se por implementar só 4.1.8, via
  o que o Jolt OFERECE nativamente para o mesmo objetivo prático (`CROSS_PLATFORM_DETERMINISTIC`,
  que desliga FMA e trava SIMD em SSE4 — ver `Build/CMakeLists.txt` do próprio Jolt), como opção de
  build CMake (`AETHER_PHYSICS_DETERMINISTIC`), no mesmo padrão já estabelecido por
  `DOUBLE_PRECISION`/`USE_ASSERTS`. Nenhum bug de compilação ou runtime nesta fatia — a flag
  compilou e todos os 43 testes de física pré-existentes passaram de primeira na build
  determinística. Confirmado experimentalmente (fora da suíte de teste, via binário de comparação
  descartável) que a flag realmente altera o código de máquina gerado: a mesma simulação produz
  posições finais numericamente diferentes entre build normal e determinística — descartando a
  hipótese de que `CROSS_PLATFORM_DETERMINISTIC` fosse um no-op silencioso nesta configuração de
  compilador/plataforma. Nenhuma mudança foi necessária no lado C# (`managed/Aether.Core/Physics/`):
  é uma opção de tempo de build da `.dll` nativa, e o P/Invoke `[LibraryImport("aether_physics")]`
  já existente funciona identicamente com qualquer uma das duas variantes compiladas.
