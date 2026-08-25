# Estado da execução — o que existe de verdade

Instantâneo do repositório contra o roadmap de `PLANO-ENGINE-MOBILE.md`.
Regra: só entra nesta tabela o que **compila e passa em teste**.

## Resumo

| | |
|---|---|
| Testes C# | **180 passando**, 0 falhando |
| Testes C++ | **52 passando**, 0 falhando (41 do núcleo + 4 de lifecycle + 7 de física) |
| Linhas C# | ~8.800 |
| Linhas C++ (próprias, sem código vendorizado) | ~3.400 |
| Dependências externas | **nenhuma** — build e testes rodam offline, de propósito (Jolt Physics é vendorizado em `native/third_party/`, não baixado no build — ver `VENDORED_COMMIT.txt`) |

## Por fase

| Fase | Item | Estado |
|---|---|---|
| **0.1** | Monorepo, build C# + CMake/Ninja, runner de testes próprio | ✅ |
| **0.1.3** | Shell Android ARM64: NativeActivity, Gradle/NDK, landscape imersivo, lifecycle e surface Vulkan | ⚠️ APK debug/release compilam e lint passa; execução em aparelho físico pendente |
| **0.3.2** | Protótipo de interação: câmera, gizmos, menu radial, inspector | ✅ `prototype/editor.html` |
| **1.5** | Matemática: float2/3/4, quaternion, float4x4, Transform, Bounds, Ray, Plane, Frustum | ✅ 25 testes |
| **1.2** | Memória: FrameArena, PoolAllocator, NativeList/Array, MemoryBudget | ✅ 22 testes |
| **1.2** | Job system com work-stealing, dependências, política big.LITTLE | ✅ 10 testes |
| **3.4** | PowerGovernor com histerese | ✅ 6 testes |
| **1.3** | ECS por arquétipos: chunks de 16 KB, consultas por chunk sem alocação, command buffer | ✅ 24 testes |
| **1.3.4** | Hierarquia (lista encadeada de irmãos) + propagação de transform | ✅ 10 testes |
| **1.4.1–1.4.2** | Registro de metadados de componente (runtime, não source generator — ver `ComponentDescriptor`) + serializador binário bit-exato e texto determinístico, migração de esquema em cadeia | ✅ 14 testes |
| **1.4.3** | Sistema de recursos: `ResourceId`, `ResourceRef`/`WeakResourceRef` com posse linear, `ResourceHandleTable` com contagem de uso, carregamento assíncrono via JobSystem | ✅ 18 testes |
| **1.4.4** | Undo/redo (`UndoStack`) + WAL de recuperação com fsync, checksum FNV-1a por registro, recuperação parcial ante corrupção | ✅ 16 testes |
| **5.1–5.6** | AetherFlow: AST, validador, gerador de C#, parser de volta, interpretador, serializador | ✅ 34 testes |
| **2.1** | RHI Vulkan: cache de descritores, perfis de dispositivo S/A/B/C | ✅ lógica testada |
| **2.3** | **Render graph**: topológico, poda, aliasing, barreiras, load/store ops, memoryless, fusão de subpasses | ✅ 41 testes |
| **4.1.1** | Física: Jolt Physics vendorizado (`native/third_party/JoltPhysics`), fronteira C ABI blittable (`jolt_bridge.h/.cpp`) — mundo, corpos estático/cinemático/dinâmico, formas caixa/esfera, `Step`, leitura de transform/velocidade, `IsActive`, raycast contra o mundo | ✅ 7 testes (queda livre bate cinemática, corpo assenta sobre piso na altura esperada, corpo estático não se move, raycast acerta/erra pelo alcance, round-trip de velocidade, handles inválidos não crasham) |

## O que ainda não existe

- **Nada ainda foi validado numa GPU física.** O shell Android cria instance, device, fila de apresentação e `VkSurfaceKHR`, mas não havia aparelho conectado durante a validação. A lógica portátil de lifecycle tem testes; a criação Vulkan precisa de teste de dispositivo.
- Existe app Android empacotável, mas ainda não existe app iOS, swapchain concreto, command loop de renderização ou shaders compilados.
- O protótipo do editor é HTML/Canvas, não a engine — valida **interação**, não desempenho gráfico. Usa as mesmas convenções de espaço do núcleo em C# de propósito, para que o que se aprende ali transfira.
- Animação, áudio, assets, build: Fases 6 e 8, não iniciadas.
- **Física (Fase 4): só o item 4.1.1 (fatia vertical mundo/corpo/forma/step/raycast) está pronto.** Deliberadamente ainda não implementados, para não fingir que um item complexo foi "riscado" com uma versão capenga: 4.1.2 (facade C# de `RigidBody`/`Collider` como componentes de ECS com sincronização bidirecional Jolt↔ECS — hoje só existe a API C++/P-Invoke crua), 4.1.3 (juntas e motores), 4.1.5 (character controller), 4.1.6 (física 2D dedicada/benchmark), 4.1.7 (decomposição convexa de malhas), 4.1.8 (determinismo em ponto fixo — a fatia atual usa o float padrão do Jolt, que não é bit-determinístico entre plataformas), 4.1.9 (sub-stepping adaptativo ligado ao PowerGovernor térmico).
- **1.4.5 (índice de dependências em SQLite) deliberadamente não implementado ainda.** Não existe um SQLite de verdade vendorizado no repositório; implementar uma versão simplificada só para "riscar o item" seria a gambiarra que este projeto se recusa a fazer. Quando entrar, será o amálgama C `sqlite3.c` vendorizado em `native/` (não um pacote NuGet — mantém a política de zero dependência externa) exposto por P/Invoke.

## Shell Android — validação atual

| Verificação | Resultado |
|---|---|
| APK debug ARM64 | ✅ gerado e assinado com certificado de desenvolvimento |
| APK release ARM64 | ✅ gerado sem assinatura de publicação |
| Android Lint debug/release | ✅ sem erros |
| Lifecycle portátil | ✅ 4 testes: ativação, pausa/retomada, perda de janela e encerramento |
| Instalação e abertura em aparelho físico | ⏳ nenhum aparelho ADB conectado |
| Criação real de surface Vulkan | ⏳ depende da execução em aparelho físico |

O APK é um **shell de fundação**, não um editor demonstrativo: por decisão de
escopo ele não desenha triângulo, não cria swapchain e bloqueia o loop quando
não há evento, evitando consumo térmico artificial.

## O que é protótipo ou prova de conceito

| Artefato | Natureza | O que prova | O que não prova |
|---|---|---|---|
| `prototype/editor.html` | Protótipo interativo HTML/Canvas | UX de câmera, seleção, gizmos, menu radial e Inspector em landscape | Renderer, desempenho mobile ou integração com a engine |
| `Aether.Flow` | PoC arquitetural funcional | AST única, validação, interpretação, serialização e round-trip parcial com C# | Catálogo completo de nós, CFG geral, integração com Inspector/runtime |
| `native/rhi/device.cpp` + shell Android | Vertical slice técnico ainda sem validação física | Build NDK, fronteira de plataforma e criação prevista de instance/device/surface | Compatibilidade real de drivers, swapchain, apresentação ou frame renderizado |
| Device profiles e render graph | Implementação headless testada | Regras de capability, ordenação, barreiras, aliasing e fusão | Execução dessas decisões numa GPU real |

## Limitações conhecidas que um leitor precisa saber

| Onde | Limitação | Consequência |
|---|---|---|
| `World.GetComponent<T>` | Incrementa a versão do componente em toda chamada, porque um retorno `ref` não distingue leitura de escrita | Change detection fica conservador (nunca perde uma escrita, mas reporta falsos positivos). Um acessor com wrapper resolve |
| `Aether.Flow` | Biblioteca de nós mínima (~12 nós), não o catálogo da Parte 9.5 | Prova a tese, não entrega o produto |
| `Aether.Flow` | `return`/`break` dentro de um ramo de `if` não são reconhecidos como controle de fluxo — caem no escape hatch `code.raw`, que sempre encadeia para a instrução seguinte | Um C# de entrada com `if (cond) { return; } maisCodigo();` gera um grafo que roda `maisCodigo()` mesmo assim, divergindo da semântica real. Ortogonal ao pino "depois" (ver correções abaixo) — precisa de suporte a `return`/`break` como nós de controle de verdade |
| `JobSystem` | Detecção de ciclo cobre só auto-espera na mesma thread; outros travamentos caem num timeout de 3 s | Um timeout como rede de proteção é remendo, não solução. Precisa de grafo de dependências explícito |
| `prototype/editor.html` | Rasterização por painter's algorithm em Canvas 2D | Artefatos de ordenação entre objetos grandes que se interpenetram. Irrelevante para o que o protótipo testa |
| `TextSerializer` | Migração de esquema por nome/valor de campo lido no esquema ATUAL, não por bytes crus como o binário | Um campo renomeado entre versões perde o valor num arquivo texto antigo; a migração de verdade só é garantida no formato binário. Intencional (o plano só pede migração para o binário), documentado no cabeçalho de `TextSerializer.cs` |

## Correções desta revisão (não são limitações — já resolvidas)

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
