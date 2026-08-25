# Estado da execução — o que existe de verdade

Instantâneo do repositório contra o roadmap de `PLANO-ENGINE-MOBILE.md`.
Regra: só entra nesta tabela o que **compila e passa em teste**.

## Resumo

| | |
|---|---|
| Testes C# | **123 passando**, 0 falhando |
| Testes C++ | **45 passando**, 0 falhando (41 do núcleo + 4 de lifecycle) |
| Linhas C# | ~6.900 |
| Linhas C++ | ~2.300 |
| Dependências externas | **nenhuma** — build e testes rodam offline, de propósito |

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
| **5.1–5.6** | AetherFlow: AST, validador, gerador de C#, parser de volta, interpretador, serializador | ✅ 26 testes |
| **2.1** | RHI Vulkan: cache de descritores, perfis de dispositivo S/A/B/C | ✅ lógica testada |
| **2.3** | **Render graph**: topológico, poda, aliasing, barreiras, load/store ops, memoryless, fusão de subpasses | ✅ 41 testes |

## O que ainda não existe

- **Nada ainda foi validado numa GPU física.** O shell Android cria instance, device, fila de apresentação e `VkSurfaceKHR`, mas não havia aparelho conectado durante a validação. A lógica portátil de lifecycle tem testes; a criação Vulkan precisa de teste de dispositivo.
- Existe app Android empacotável, mas ainda não existe app iOS, swapchain concreto, command loop de renderização ou shaders compilados.
- O protótipo do editor é HTML/Canvas, não a engine — valida **interação**, não desempenho gráfico. Usa as mesmas convenções de espaço do núcleo em C# de propósito, para que o que se aprende ali transfira.
- Física, animação, áudio, assets, build: Fases 4, 6 e 8, não iniciadas.

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
| `Aether.Flow` | Um `if` precisa ser a última instrução do bloco, no gerador e no parser | Evita a ambiguidade de dois ramos reconvergindo. É a limitação mais séria contra a tese de round-trip do plano e merece uma passada com CFG de verdade |
| `Aether.Flow` | Biblioteca de nós mínima (~12 nós), não o catálogo da Parte 9.5 | Prova a tese, não entrega o produto |
| `JobSystem` | Detecção de ciclo cobre só auto-espera na mesma thread; outros travamentos caem num timeout de 3 s | Um timeout como rede de proteção é remendo, não solução. Precisa de grafo de dependências explícito |
| `prototype/editor.html` | Rasterização por painter's algorithm em Canvas 2D | Artefatos de ordenação entre objetos grandes que se interpenetram. Irrelevante para o que o protótipo testa |
