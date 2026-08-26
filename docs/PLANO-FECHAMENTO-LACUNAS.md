# Plano de fechamento de lacunas e convergência dos marcos

> **Estado vivo em 26/08/2026:** 5 das 18 lacunas registradas estão integralmente
> fechadas (`GAP-FLOW-01`, `GAP-PHY-01`, `GAP-PHY-02`, `GAP-JOB-01` e
> `GAP-ECS-01`). Os gates M0–M9 permanecem 0/10;
> avanços parciais de hardware/shell não encerram M0. Evidências e contagens
> detalhadas ficam em `ESTADO.md`.

## 1. Propósito e autoridade

Este documento é o plano de execução para transformar o estado atual da Aether nos marcos demonstráveis definidos em `PLANO-ENGINE-MOBILE.md`. Ele não substitui o plano principal. O plano principal continua sendo a fonte de verdade para produto, arquitetura e critérios M0–M9; este documento define ordem, dependências, correções, testes e regras de aceite para fechar o que está parcial ou ausente em `ESTADO.md`.

As decisões deste plano seguem cinco regras:

1. Um item só fica concluído quando o fluxo vertical real funciona e seu critério pode ser reproduzido.
2. Teste headless não substitui validação em dispositivo quando a hipótese envolve GPU, driver, toque, térmica, lifecycle ou armazenamento Android.
3. Uma fatia de fase posterior não encerra uma fase anterior. Física e AetherFlow podem existir antes de M1, mas M0 e M1 continuam abertos até seus próprios critérios serem satisfeitos.
4. Correção silenciosa tem prioridade sobre novas funcionalidades. Divergência semântica, contatos descartados ou estado não sincronizado bloqueiam expansão do subsistema afetado.
5. Limitações aceitas precisam de contrato, fallback e documentação. Workaround não é correção definitiva.

## 2. Estado de partida e classificação

### 2.1 Síntese

O repositório possui fundações testadas de matemática, memória, jobs, ECS, serialização, recursos, undo/WAL, Render Graph headless, física Jolt e uma PoC do AetherFlow. Também possui um shell Android empacotável e um protótipo HTML/Canvas do editor.

Ainda não há um marco de produto encerrado. O caminho crítico é:

```text
verdade de estado e CI
        ↓
M0: hardware + Vulkan + .NET + toque + térmica
        ↓
M1: núcleo completo e medido em aparelho
        ↓
M2: renderer real
        ↓
M3: editor integrado e testado com usuários
        ↓
M4: simulação e jogo vertical completo
        ↓
M5: Flow/C# com equivalência semântica
        ↓
M6–M9: conteúdo, gráficos, publicação e ecossistema
```

### 2.2 Classes de lacuna

| Classe | Significado | Regra de tratamento |
|---|---|---|
| **B0 — Existencial** | Pode invalidar o produto ou a arquitetura | Provar antes de ampliar o investimento |
| **B1 — Correção** | Pode produzir estado ou execução incorreta sem falha evidente | Corrigir e adicionar regressão antes de novas features no subsistema |
| **B2 — Fundação** | API ou infraestrutura necessária a várias fases | Resolver antes da primeira consumidora de produto |
| **B3 — Integração** | Peça existe isolada, mas não participa do fluxo editor/runtime | Fechar como vertical slice |
| **B4 — Escala/qualidade** | Funciona no caso comum, mas não atende orçamento, volume ou UX | Medir, definir limite e otimizar |
| **B5 — Escopo futuro** | Item de fase ainda não iniciada | Executar somente após o gate anterior |

### 2.3 Lacunas registradas

| ID | Classe | Lacuna | Critério objetivo de fechamento |
|---|---|---|---|
| GAP-HW-01 | B0 | Vulkan e lifecycle não validados em GPU física | Matriz mínima de aparelhos verde, incluindo perda/recriação de surface e background/foreground |
| GAP-M0-02 | B0 | PoCs A–E e critério M0 incompletos | Cinco relatórios reproduzíveis e demonstração integral de M0 |
| GAP-FLOW-01 | B1 | `return`/`break` em ramos podem divergir do C# | Testes diferenciais provam equivalência entre C#, AST, interpretador e C# regenerado |
| GAP-PHY-01 | B1 | Capacidade de pares/contatos derivada incorretamente de `maxBodies` | ABI versionada com limites separados e nenhum descarte silencioso |
| GAP-PHY-02 | B1 | Cinemáticos não resincronizam ECS→Jolt | Plataforma movida pelo ECS atualiza o Jolt e transporta personagem/corpos corretamente |
| GAP-JOB-01 | B1 | Ciclos gerais de jobs só terminam por timeout | Grafo detecta ciclo antes da espera; timeout permanece apenas como proteção externa |
| GAP-ECS-01 | B2 | Leitura de componente marca escrita | APIs de leitura/escrita distintas e change detection exata |
| GAP-SER-01 | B2 | Rename no formato texto perde valor antigo | IDs/aliases estáveis e migração testada entre versões |
| GAP-PHY-03 | B2 | Não há Trigger/sensor persistente nem eventos | Sensor, Enter/Stay/Exit, filtros e serialização funcionando no ECS |
| GAP-PHY-04 | B4 | Criação/destruição P/Invoke individual | API em lote, contador de crossings e teste de spawn massivo |
| GAP-JOINT-01 | B3 | Juntas só em world space e fora do ECS | Espaço local/world, componentes declarativos e sincronização por entidades |
| GAP-FLOW-02 | B3 | Flow não acessa serviços externos, inclusive física | `ExecutionServices` explícito, testável e sem singleton global |
| GAP-CHAR-01 | B3 | Gravidade/composição de movimento depende de disciplina do chamador | `CharacterMotorSystem` com contrato e modos testados |
| GAP-CHAR-02 | B5 | Criar agachado, escalar e nadar ausentes | Stance inicial e capacidades independentes de escalada/natação |
| GAP-2D-01 | B0/B4 | Não houve comparação real Jolt restrito × Box2D v3 | Benchmark equivalente em hardware e ADR com decisão mensurável |
| GAP-CORE-01 | B2 | Source generator, SQLite, Node, tempo/eventos/configuração e benchmark M1 incompletos | Todos os itens 1.1–1.5 integrados e gate M1 verde |
| GAP-RHI-01 | B0/B3 | Render Graph não executa em GPU; não há swapchain/present | Frame real apresentado, capturável e validado nos perfis |
| GAP-EDITOR-01 | B0/B3 | Editor é protótipo HTML, não produto integrado | Editor nativo usa renderer/runtime reais e cumpre teste M3 |

## 3. Modelo de execução

### 3.1 Trilhas permanentes

Cada onda terá quatro trilhas. Nenhuma pode ser deixada para o fim:

| Trilha | Responsabilidade |
|---|---|
| **Runtime** | Core, ECS, render, física, áudio, animação e serviços |
| **Editor** | UX touch, Inspector, comandos, serialização, recovery e ferramentas |
| **Plataforma/Build** | Android/iOS, CI, dispositivos, ABI, empacotamento e profiling |
| **Qualidade** | Testes, benchmarks, fuzzing, compatibilidade, documentação e telemetria |

### 3.2 Gates obrigatórios

Um gate só muda para verde quando:

- o código compila em clean build;
- testes relacionados passam sem skip silencioso;
- o artefato é gerado pelo CI;
- evidências de hardware são anexadas quando aplicável;
- métricas permanecem dentro do orçamento;
- erros têm diagnóstico e contexto;
- formato/ABI alterado possui versão e teste de compatibilidade;
- `ESTADO.md` e documentação pública são atualizados na mesma mudança;
- não há stub ou `NotImplementedException` em caminho alcançável;
- o critério demonstrável do marco foi gravado e pode ser repetido.

### 3.3 Política para alterações de ABI e formatos

Toda evolução da fronteira C#↔C++ deve usar descritores com `structSize` e `apiVersion`. Campos novos entram no final; o nativo aceita versões anteriores quando houver fallback seguro. Handles continuam índice+geração. Uma ABI incompatível exige símbolo de versão novo e janela de migração, nunca alteração silenciosa do layout.

Arquivos persistentes usam `formatVersion`, IDs estáveis de tipo/campo e migradores em cadeia. Testes guardam fixtures de pelo menos duas versões anteriores. Cache pode ser descartado; projeto, cena e assets importados não.

## 4. Onda 0 — verdade operacional e correções de segurança

**Objetivo:** impedir que o projeto expanda sobre estado incorreto ou documentação ambígua.

### 4.1 Inventário executável

1. Criar `docs/MATRIZ-MARCOS.md` ou gerar uma tabela equivalente contendo todos os itens 0.1.1–9.6.7.
2. Para cada item registrar: `não iniciado`, `PoC`, `parcial`, `implementado`, `validado em hardware` ou `aceito`.
3. Associar evidência: testes, benchmark, vídeo, relatório de dispositivo ou ADR.
4. Proibir que um cabeçalho amplo como “5.1–5.6” fique verde quando só a infraestrutura de AST foi concluída.
5. Alinhar README e `ESTADO.md`: “fase corrente” representa o gate em fechamento; fatias adiantadas aparecem como experimentais, não como avanço de marco.

**Aceite:** não existe item verde sem evidência; README, estado e matriz não se contradizem.

### 4.2 CI confiável

**Estado: implementação concluída.** A execução hospedada aguarda a publicação do
repositório; `android-device` e `soak` também aguardam um runner físico rotulado
`android-device-lab`. Essas pendências são de evidência operacional, não stubs
silenciosos na pipeline.

1. ✅ Suites `unit`, `native`, `interop`, `android-host`, `benchmark` e
   `clean-build-nightly` são jobs independentes. `android-device`/`soak` vivem num
   workflow manual próprio porque exigem ADB e hardware reais.
2. ✅ `interop` e o clean build usam `AETHER_REQUIRE_NATIVE=1`; ausência da DLL
   transforma os testes dependentes em skips contabilizados e falha o job. Somente
   `unit` permite esses skips explicitamente.
3. ✅ APK, logs/resultados, executáveis, budgets e versões do NDK/JDK são publicados
   como artefatos dos jobs aplicáveis.
4. ✅ O clean build diário usa árvore nova, compila o nativo antes do gerenciado,
   instala headers Vulkan isolados do NDK e executa 242 testes C# + 96 nativos.
5. ✅ `metrics/budgets.v1.json` versiona budgets de P/Invoke, alocação, CPU, GPU,
   memória e energia. `tools/validate-metrics-budget.ps1` valida o contrato e quebra
   o gate ao ultrapassar limites; coletores que exigem dispositivo permanecem
   declarados como `device-required`, sem fabricar medições em runner hospedado.

**Aceite:** o caminho limpo foi reproduzido localmente com 242/242 testes C# e
96/96 nativos; a fixture negativa de 301 chamadas nativas/frame quebra o gate.
Falhas de dependência não se transformam em falsos verdes. A primeira execução no
GitHub e as séries CPU/GPU/memória/energia em hardware continuam como evidências
necessárias para M0/M1, sem reabrir a implementação desta seção.

### 4.3 Correção imediata do AetherFlow

1. Modelar `return`, `break` e `continue` como nós de controle explícitos na AST.
2. Validar escopo: `break`/`continue` apenas dentro de loop; `return` respeita tipo de retorno.
3. Atualizar parser C#→Flow, gerador Flow→C#, interpretador, validador e serializador de forma simétrica.
4. Criar testes diferenciais que executam o C# de referência, o interpretador e o C# regenerado e comparam efeitos sobre um mundo determinístico.
5. Incluir casos aninhados, `if` sem `else`, retornos antecipados, loops e `code.raw`.

**Aceite:** nenhuma construção suportada muda a sequência de efeitos no round-trip; construções não suportadas são rejeitadas ou isoladas, nunca reinterpretadas silenciosamente.

### 4.4 Segurança imediata da física

1. Criar `AetherPhysicsWorldDescV2` com `maxBodies`, `maxBodyPairs`, `maxContactConstraints`, `maxBroadPhasePairs` e política de overflow.
2. Fazer `Step` retornar flags do `EPhysicsUpdateError` e logar categoria, capacidade e contadores.
3. Em debug/teste, overflow falha imediatamente com diagnóstico. Em release, nunca é silencioso: contador e warning rate-limited.
4. Adicionar cenários densos com 500, 1.000 e 5.000 corpos, incluindo pilhas e múltiplos vizinhos.
5. Manter o símbolo V1 durante a migração e mapeá-lo para defaults conservadores.

**Aceite:** nenhum teste usa `maxBodies × 4` como workaround; o limite necessário é medido e configurado pelo conceito correto.

## 5. Onda 1 — fechar M0 em hardware real

**Objetivo:** responder os riscos fatais antes de continuar o produto.

### 5.1 Laboratório de dispositivos

1. Adquirir ou disponibilizar inicialmente seis aparelhos: ao menos dois Adreno, dois Mali, um aparelho fraco perfil C e um perfil S/A recente. Expandir para os 12 previstos antes de M2.
2. Registrar modelo, SoC, GPU, RAM, Android, driver Vulkan, extensões, taxa de atualização, página de 16 KB e estado térmico.
3. Criar runner ADB que instala, limpa dados, abre, coleta Logcat, alterna background/foreground, gira/configura e encerra.
4. Executar perda/recriação da janela, screen off/on, interrupção por Activity, memória baixa simulada e retomada.
5. Definir quarentena documentada para defeito específico de driver, com fallback e prazo; nunca ignorar aparelho silenciosamente.

**Aceite:** `Surface Vulkan pronta` e lifecycle completo verdes na matriz mínima; crashes incluem stack e capabilities.

### 5.2 Shell gráfico mínimo

1. Implementar swapchain com seleção de formato, present mode e extensão, sem API Android vazando para o RHI.
2. Criar command pool/buffers, sincronização de frames, acquire/present e recriação após `OUT_OF_DATE`/`SUBOPTIMAL`.
3. Renderizar primeiro triângulo e depois cubo texturizado com depth.
4. Suspender submit quando não houver surface ou quando o app estiver inativo.
5. Garantir destruição ordenada e recuperação após background/foreground repetido 100 vezes.

**Aceite:** nenhuma validation error; captura AGI/RenderDoc legível; 1.000 ciclos de frame e 100 retomadas sem leak ou crash.

### 5.3 PoC-A — Vulkan + .NET

1. Finalizar IDL gerado para lotes blittable, IDs e spans; proibir strings/objetos gerenciados no caminho quente.
2. Montar lista de render em C# e enviar em poucos lotes ao nativo.
3. Instrumentar quantidade e tempo de crossings por frame.
4. Renderizar 5.000 objetos com culling mínimo e material simples.
5. Testar comparação A/B entre composição em C#, composição nativa e tamanhos de lote.

**Aceite:** 60 fps e < 3 ms de CPU no aparelho de referência, sem GC/frame e dentro do orçamento de crossings.

### 5.4 PoC-B — gestos

1. Converter o protótipo existente em especificação de comportamento: coordenadas, conflitos de gesto, tolerâncias, inércia, snap e tamanhos de alvo.
2. Testar mover, girar e escalar com 10 usuários reais; registrar erros, tempo e tentativas.
3. Testar telas pequenas, tablet, mão dominante, caneta e acessibilidade motora básica.
4. Ajustar gesto até a mediana ficar abaixo de 30 s sem ocultar taxa de erro.

**Aceite:** tarefa concluída por todos os participantes dentro do critério e sem regressão grave de precisão.

### 5.5 PoC-C — térmica

1. Integrar leitura Android ao `PowerGovernor`, com estados normal, aquecendo, limitado e crítico.
2. Registrar CPU/GPU frame, watts estimados, temperatura, clocks e decisão de qualidade.
3. Fazer o viewport renderizar sob demanda quando ocioso e reduzir resolução/efeitos com histerese.
4. Rodar 30 minutos em pelo menos três perfis e em condição carregando/não carregando.

**Aceite:** < 4 W sustentados e sem queda abaixo de 55 fps no perfil-alvo; mudanças de qualidade não oscilam.

### 5.6 PoC-D — hot reload C#

1. Carregar runtime .NET no processo com lifecycle explícito e fronteira isolada.
2. Compilar uma alteração incremental, validar assembly e aplicar em contexto recarregável.
3. Migrar estado serializável ou informar incompatibilidade com diagnóstico.
4. Medir do último toque de edição até a mudança visível no frame.
5. Para iOS, realizar spike jurídico/técnico separado e registrar ADR; não prometer execução proibida pela plataforma.

**Aceite:** < 2 s no Android de referência; decisão de iOS registrada com protótipo permitido ou escopo Android-first formalizado.

### 5.7 PoC-E — ASTC em GPU

1. Implementar caminho de compressão isolado e cancelável, fora do frame.
2. Validar qualidade, alinhamento de blocos, mipmaps e fallback ETC2/Basis.
3. Medir textura 4096×4096 em aparelhos A/S e memória de pico.

**Aceite:** < 300 ms no perfil definido pelo plano ou mitigação formal aprovada antes de M0.

### 5.8 Gate M0

M0 fecha somente com cubo texturizado controlado por toque, shader C# atualizado em menos de dois segundos, soak térmico de 30 minutos e cinco PoCs verdes ou com mitigação aprovada. A demonstração usa APK produzido pelo CI em aparelho catalogado.

## 6. Onda 2 — fechar M1 e estabilizar o núcleo

### 6.1 Plataforma 1.1

1. Criar interfaces de filesystem, display, input, clipboard, file picker, lifecycle e device capabilities no módulo de plataforma.
2. Implementar Android Storage Access Framework sem expor paths desktop ao Core.
3. Modelar toque com ID, histórico, pressão, previsão e cancelamento; mapear mouse, teclado, gamepad e sensores para ações.
4. Tratar safe area, notch, taxa variável, display externo, multi-window e memória baixa.
5. Mover a leitura térmica para serviço de plataforma consumido pelo `PowerGovernor`.

**Aceite:** testes portáteis das máquinas de estado e testes de dispositivo para cada callback Android; Core não depende de Android.

### 6.2 Memória e concorrência 1.2

**Estado parcial:** `GAP-JOB-01` fechado; filas SPSC/MPMC, profiling de
alocação, analisador NoAlloc e sanitizers continuam abertos.

1. Finalizar primitivas SPSC/MPMC e contadores com testes de stress e sanitizers.
2. ✅ O JobSystem registra pré-requisitos e esperas runtime em grafo explícito e
   detecta ciclos antes do bloqueio, incluindo arestas publicadas concorrentemente.
3. ✅ O timeout permanece apenas como watchdog de trabalho externo não observável
   pelo grafo, não como mecanismo primário de detecção de ciclo.
4. Implementar rastreamento de alocações por subsistema e visualizador de jobs.
5. Criar analisador `[NoAlloc]` e permitir exceções somente com justificativa versionada.
6. Rodar TSAN/ASAN/UBSAN onde a toolchain suportar, além de stress de shutdown e cancelamento.

**Aceite parcial:** ciclos de 2–N jobs são rejeitados com caminho do ciclo
(`GAP-JOB-01` verde). A parte de alocação depende do analisador `[NoAlloc]` ainda
aberto nesta seção.

### 6.3 ECS 1.3

**Estado parcial:** `GAP-ECS-01` fechado; fachada `Node`, filtros avançados e
benchmark hierárquico em aparelho ainda estão abertos.

1. ✅ `World.Read<T>`/`Write<T>` e
   `Chunk.GetReadOnlySpan<T>`/`GetWritableSpan<T>` separam intenção. Somente o
   acesso mutável incrementa a versão, uma vez por referência/span obtido.
2. ✅ Hierarquia, propagação de transforms e sincronização física usam os novos
   acessores. `GetComponent<T>`/`GetSpan<T>` permanecem temporariamente como APIs
   deprecated conservadoras para compatibilidade.
3. ✅ `Node` é uma fachada `readonly struct` sobre `World`+`EntityId`, sem estado
   ou ownership duplicado. Expõe componentes, parent/children sem alocação e
   invalida com a geração da entidade; reparent entre mundos é recusado.
4. Completar filtros/cache de consultas e benchmark de mudanças estruturais.
5. Medir 100 mil entidades hierarquizadas com profundidades realistas e casos patológicos.

**Aceite parcial:** duas regressões provam que leitura não marca mudança e escrita
marca exatamente o chunk/componente correto (`GAP-ECS-01` verde). O teste flat de
100 mil entidades mantém zero GC/frame e mediu 4 ms nesta máquina; o aceite M1
continua aberto até medir hierarquia real a < 6 ms em aparelho classe A.

### 6.4 Reflexão, serialização e recursos 1.4

1. Criar source generator de descriptors, propriedades, métodos, IDs estáveis e bindings básicos.
2. Preservar registro runtime como fallback para plugin/diagnóstico, não como fonte duplicada.
3. Adicionar `FormerlySerializedAs`/aliases ou IDs de campo explícitos ao serializer texto.
4. Testar rename, remoção, adição, mudança compatível/incompatível e cadeia v1→v2→v3 nos formatos texto/binário.
5. Vendorizar o amálgama SQLite com licença registrada, wrapper C mínimo e P/Invoke versionado.
6. Implementar índice de dependências transacional, rebuildable e nunca fonte de verdade.
7. Integrar ResourceId, dependências, import metadata e invalidation sem path espalhado.

**Aceite:** fixture antiga sobrevive a rename; índice SQLite pode ser apagado e reconstruído; Inspector/Flow/serializer consomem o mesmo metadata descriptor.

### 6.5 Matemática, tempo e serviços 1.5

1. Validar SIMD NEON contra implementação escalar e tolerâncias por operação.
2. Implementar relógios monotônicos, fixed step, acumulador limitado, interpolação, time scale e pausa.
3. Criar sinais/eventos tipados com ownership de inscrição e remoção segura.
4. Criar configuração versionada por categorias, com defaults e override por projeto/dispositivo.
5. Adicionar testes de precision drift, spiral of death, pausa/retomada e serialização de settings.

### 6.6 Gate M1

Salvar/recarregar precisa ser bit-exato. O benchmark de 100 mil entidades deve rodar em aparelho classe A a 60 fps, < 6 ms de CPU e zero GC/frame. O relatório inclui modelo, build, temperatura, distribuição p50/p95/p99 e memória, não apenas média.

## 7. Onda 3 — fechar M2: renderer Vulkan real

### 7.1 RHI 2.1

1. Evoluir o shell M0 para RHI completo: buffers, imagens, samplers, pipelines e handles opacos.
2. Integrar VMA ou alternativa aprovada por ADR, com budget e telemetria por categoria.
3. Implementar bindless com detecção de feature e fallback não-bindless.
4. Gravar command buffers em jobs e sincronizar com timeline semaphores quando suportadas.
5. Integrar validation layers, nomes de objeto, captura e device fault quando disponível.

### 7.2 Shaders 2.2

1. Definir linguagem fonte e toolchain reprodutível para SPIR-V.
2. Gerar reflexão de bindings e validar contra metadata de material.
3. Controlar variantes por orçamento; persistir cache por GPU/driver.
4. Compilar em background e usar material rosa de fallback com erro legível.
5. Implementar hot reload usando o caminho validado em M0.

### 7.3 Render Graph 2.3

1. Conectar a implementação headless ao RHI sem introduzir dependência reversa.
2. Materializar recursos transitórios, barreiras e load/store reais.
3. Validar aliasing com camadas de validação e testes de vida útil.
4. Medir fusão de subpasses/memoryless em tile-based GPUs.
5. Criar visualizador consumível pelo editor posterior.

### 7.4 Pipeline, culling e 2D 2.4–2.6

1. Entregar depth prepass, Forward+, PBR, sombras, IBL, transparência e pós em slices completos.
2. Implementar BVH incremental, frustum/HZB, instancing, LOD e ordenação por PSO/profundidade.
3. Criar renderer 2D com sprite batcher, atlas, tilemap e luzes 2D.
4. Cada efeito possui feature detection, preset e fallback antes de entrar no perfil C.
5. Capturas douradas usam tolerância perceptual e aparelhos de fabricantes diferentes.

### 7.5 Gate M2

Executar cena equivalente a Sponza com 500 mil triângulos, 30 luzes, PBR e sombras. Perfil A: 60 fps estáveis e < 3,5 W. Perfil C: 30 fps com degradação automática. O Render Graph precisa provar, por captura e métricas, que suas decisões são executadas na GPU.

## 8. Onda 4 — fechar M3: editor mobile integrado

### 8.1 Substituição controlada do protótipo

O HTML/Canvas permanece como referência de UX até os testes M0, mas não entra no runtime. Cada comportamento validado vira especificação e teste de interação. Painter's algorithm não será “corrigido” no protótipo; a solução definitiva é o viewport Vulkan do M2.

### 8.2 UI e gestos 3.1–3.2

1. Implementar UI retida com layout, listas virtualizadas, texto/ícones SDF e tokens existentes.
2. Criar resolver de conflitos de gestos com captura, prioridade, cancelamento e visualização de estado.
3. Garantir alvos mínimos, hápticos, escalabilidade, leitor de tela e alto contraste.
4. Implementar bottom sheets/drawers sem reduzir o viewport abaixo do limite do plano.
5. Automatizar testes de gesto por sequência de ponteiros e validar em touchscreen real.

### 8.3 Viewport e ferramentas 3.3

1. Integrar câmera, picking, gizmos, snap, overlays e debug draw ao renderer real.
2. Usar Command para toda manipulação e agrupar um gesto inteiro em uma operação undoável.
3. Separar gizmos/editor overlays do Game Runtime.
4. Instrumentar precisão, latência de toque e custo de picking.

### 8.4 Painéis e fluxo de projeto 3.4–3.5

1. Gerar Inspector pelo source metadata, com factories extensíveis e edição múltipla.
2. Hierarquia opera sobre EntityId/Node e usa comandos para reparent/reorder/delete.
3. Asset Browser usa banco de assets, previews, filtros e operações transacionais.
4. Console inclui origem, categoria, severidade, stack e agrupamento.
5. Implementar prefabs, autosave temporário→validação→replace atômico e recovery por WAL.

### 8.5 Play Mode e onboarding 3.6–3.7

1. Separar Edit World e Play World; nunca executar diretamente sobre o estado autoral.
2. Isolar processo quando o compartilhamento AHardwareBuffer estiver validado; usar fallback seguro até lá.
3. Permitir pausa, step, inspeção e retorno sem corromper a cena.
4. Criar onboarding interativo e níveis Essencial/Padrão/Completo.

### 8.6 Gate M3

Usuário novo monta cena de 20 objetos com luz, material e prefab em menos de 15 minutos usando apenas os dedos e entra em Play. Quinze usuários externos fazem o teste; SUS precisa ser ≥ 72. Abaixo disso, UX volta para projeto e M4 não recebe prioridade de produto.

## 9. Onda 5 — fechar M4 e eliminar as lacunas da física

### 9.1 Física 4.1 — estabilização da base existente

#### 9.1.1 Cinemáticos e sincronização

**Estado: fechado (`GAP-PHY-02`).**

1. `AetherPhysics_MoveKinematicV2` expõe o movimento pela ABI versionada.
2. Um cache exato de alvo por body sincroniza somente mudanças reais e envia uma
   única parada no frame seguinte. O ECS agora possui versões exatas por
   chunk/coluna (`GAP-ECS-01` fechado), mas o cache por body continua necessário:
   a versão de chunk identifica a coluna alterada, não qual corpo mudou nem o alvo
   anterior usado para emitir a parada final.
3. Autoridade definida: estático=autoria, dinâmico=Jolt e
   cinemático=ECS/animação.
4. Somente corpos dinâmicos percorrem Jolt→ECS, eliminando o loop de feedback.
5. Regressões cobrem plataforma transladando e girando com character e corpo
   dinâmico apoiado, além de validar que frames estáveis não cruzam a ABI.

#### 9.1.2 Triggers e eventos

1. Adicionar shape/body sensor na fronteira nativa.
2. Coletar contatos em buffer lock-free do callback de física e publicar após o step.
3. Emitir Enter/Stay/Exit com IDs estáveis, camada, ponto/normal quando disponível.
4. Criar `TriggerComponent`, metadata, serialização, Inspector, script e Flow.
5. Definir comportamento em destroy, disable, reparent e scene unload.

#### 9.1.3 Batching e orçamento de interop

1. Criar `CreateBodies(ReadOnlySpan<BodyDesc>, Span<Handle>)` e `DestroyBodies(ReadOnlySpan<Handle>)`.
2. Manter chamadas unitárias como wrappers de conveniência fora do caminho massivo.
3. Instrumentar crossings/frame e bytes transferidos.
4. Testar spawn/despawn de 100, 1.000 e 10.000 corpos sem alocação por corpo no frame.

#### 9.1.4 Juntas completas e ECS

1. Adicionar espaço `World`, `LocalToBody1`, `LocalToBody2` com conversões testadas.
2. Criar componentes de junta com referências por EntityId/UUID, resolução tardia e handles geracionais.
3. Implementar `JointSyncSystem` com lifecycle idempotente e recriação somente quando descriptor muda.
4. Validar Hinge na API pública: rotação contínua é normalizada para `-pi/+pi`; valores fora do contrato do Jolt retornam erro, não são truncados silenciosamente.
5. Entregar SixDOF como recurso separado; veículo, gear, pulley e path entram conforme consumidor real e cada um recebe slice/testes próprios.
6. Serializar limites/motor e expor via Inspector/Flow.

#### 9.1.5 Contexto de física no Flow

1. Introduzir `FlowExecutionContext` com `World`, `PhysicsWorld`, tempo, input, logger e serviços permitidos.
2. Nós declaram capability necessária; validador rejeita grafo usado em contexto incompatível.
3. Gerador C# recebe serviços por parâmetro/Behavior, nunca por `PhysicsContext.Current` estático.
4. Implementar raycast, raycast all, shapecast, overlap, trigger events e operações seguras.
5. Testar interpretador e C# gerado contra o mesmo mundo físico determinístico.

#### 9.1.6 Character Controller

1. Criar `CharacterMotorComponent/System` que compõe gravidade, velocidade desejada, plataforma, step e stick-to-floor.
2. Separar estados `Grounded`, `Falling`, `Rising`, `Sliding`, `Climbing`, `Swimming` e stance.
3. Permitir descriptor inicial em pé/agachado; validar forma antes da criação e retornar erro contextual.
4. Implementar escalada como capability: detecção de superfície, ângulo/tag/layer, aquisição/perda, velocidade e transição.
5. Implementar natação sobre `WaterVolume/FluidVolume`: overlap, profundidade, buoyancy/drag e controle configurável.
6. Não colocar gameplay específico no Core; motor e capabilities são componentes reutilizáveis.
7. Testar quinas, tetos finos, rampas, degraus, plataformas e mudança de stance com folgas válidas.

#### 9.1.7 Física 2D

1. Vendorizar Box2D v3 em módulo isolado após revisão de licença/tamanho.
2. Criar harness equivalente: mesmas formas, contagem, densidade, steps, warm-up e dispositivos.
3. Medir CPU p50/p95, memória, tamanho binário, estabilidade e recursos necessários.
4. Decisão recomendada: escolher Box2D como backend 2D somente se entregar ganho sustentado relevante (meta inicial ≥ 30% de CPU ou memória no perfil B/C) que compense segunda biblioteca e manutenção. Caso contrário, manter Jolt restrito e registrar que a escolha é por simplicidade, não por desempenho.
5. Não criar API pública amarrada ao backend; componentes 2D usam interface própria e handles opacos.

#### 9.1.8 Itens 4.1.7–4.1.9

1. Decomposição convexa entra no import pipeline, assíncrona, cacheada e com preview/limite de custo.
2. Determinismo em ponto fixo começa por spike e ADR. Como Jolt float não é bit-determinístico, a solução pode ser backend/modo restrito separado; não prometer determinismo total sem prova multiplataforma.
3. Sub-stepping adaptativo recebe limites configuráveis, métricas de erro e integração com PowerGovernor. Redução térmica não pode alterar gameplay de forma imprevisível sem modo declarado.

### 9.2 Animação, áudio, VFX, IA e UI 4.2–4.6

1. Executar as etapas na ordem de dependência do plano: skeleton/clip→grafo→IK/retarget; backend áudio→grafo→efeitos; pools de partículas→VFX nodes; navmesh→agent→behavior; canvas UI→widgets→binding.
2. Usar o canvas comum somente após existir uma infraestrutura estável de nós; editores de animação/VFX não duplicam pan, zoom, pins e undo.
3. Toda propriedade exposta usa metadata comum para Inspector, serialização, Flow e animação.
4. Criar vertical slices: personagem animado; fonte espacial; efeito de partícula; agente navegando; HUD ligado a variável.

### 9.3 Gate M4

Construir um platformer 3D inteiramente no editor com animação, física, inimigos/navmesh, áudio, partículas, HUD e menu. Rodar a 60 fps no perfil A e 30 fps no C, incluindo soak e recovery.

## 10. Onda 6 — fechar M5: AetherFlow e scripting C# de produto

### 10.1 Linguagem e round-trip 5.1–5.2

1. Congelar a AST versionada somente após corrigir controle de fluxo e definir tipos/coerções.
2. Preservar comentários, nomes e regiões não representáveis via nós `code.raw` com contrato de efeitos.
3. Usar testes property-based e corpus de C# real; comparar AST normalizada e comportamento, não apenas texto.
4. Definir claramente o subconjunto suportado; rejeitar ambiguidade com diagnóstico acionável.

### 10.2 Canvas e representações 5.3–5.4

1. Construir o canvas sobre o framework M3 e o Command/Undo existentes.
2. Virtualizar 1.000+ nós; medir pan/zoom e hit-testing em aparelhos B/C.
3. Fazer Grafo, Lista e Blocos serem views da mesma AST, sem conversão destrutiva.
4. Testar transformação entre views com grafos aleatórios e assets reais.

### 10.3 Biblioteca, reflexão e depuração 5.5–5.6

1. Gerar nós a partir do source metadata usado pelo Inspector e binding C#.
2. Organizar o catálogo por capability e módulos; carregamento sob demanda evita 400 tipos sempre residentes.
3. Cada nó possui testes, documentação, erro, custo e disponibilidade editor/runtime.
4. Implementar breakpoints, step, watch e mapa de custo antes de time-travel completo.
5. Time-travel usa buffer circular com orçamento e snapshots/deltas do ECS; nunca cresce sem limite.

### 10.4 C# e IA 5.7–5.8

1. Integrar compilação incremental, hot reload e migração de estado provados em M0.
2. Implementar API em três níveis com capabilities e sandbox.
3. Editor de código touch compartilha diagnósticos e símbolos do compilador.
4. IA é opcional, opt-in e só produz diff revisável; o produto funciona integralmente offline sem ela.

### 10.5 Gate M5

Usuário sem programação cria jogo completo em duas horas apenas com Flow. Desenvolvedor converte grafo para C#, otimiza e volta sem perda semântica. O teste inclui retorno antecipado, loops, eventos de física e persistência.

## 11. Ondas 7–10 — fases 6 a 9

Estas ondas seguem os itens do plano principal, mas só iniciam após o marco precedente. A ordem interna abaixo evita construir ferramentas sobre assets, renderer ou metadata instáveis.

### 11.1 M6 — criação de conteúdo

1. **6.1 Assets primeiro:** Asset Database, importadores, cache por hash, dependências SQLite, compressão e streaming.
2. **6.2–6.4 Geometria/autoria:** half-edge e histórico antes de modelagem, escultura e UV; operações sempre canceláveis e undoáveis.
3. **6.5 Materiais:** Shader Graph usa toolchain M2 e canvas M5.
4. **6.6–6.7 Mundo/Geometry Nodes:** streaming e jobs precisam respeitar budgets M1/M2.
5. **6.8 Captura:** fotogrametria/LiDAR/ML são opcionais por capability, com fallback e consentimento.

**Gate:** personagem criado integralmente no celular e objeto escaneado convertido em asset PBR em menos de cinco minutos.

### 11.2 M7 — gráficos avançados

1. GPU-driven/MicroMesh vem antes de GI e sombras virtuais porque define representação e culling.
2. Cada recurso possui fallback completo B/C e limite térmico.
3. GI, sombras, volumétricos e upscaling entram individualmente com captura dourada, orçamento e feature flag.
4. Integrar perfis, banco de dispositivos, calibração e PowerGovernor antes de declarar beta.

**Gate:** cena-alvo a 60 fps no perfil A, < 4,5 W, com comparador visual e fallbacks comprovados.

### 11.3 M8 — build e publicação

1. Aether Player e `.aetherpack` versionado precedem serviços de nuvem.
2. Pipeline validate→collect→strip→compile→package→sign deve ser reproduzível e produzir SBOM/licenças.
3. Chaves nunca saem do hardware seguro; logs não contêm segredo.
4. Multiplayer e serviços usam interfaces locais/fakes para desenvolvimento offline.
5. Profiler fecha CPU/GPU/memória/térmica antes da publicação real.

**Gate:** jogo publicado na Google Play somente pelo celular e projeto compartilhado para 100 execuções via Player.

### 11.4 M9 — colaboração e lançamento

1. Git visual/diff de cena precede CRDT; formatos e UUIDs precisam estar maduros.
2. Plugins usam API versionada, capabilities e sandbox; nunca acessam internals por conveniência.
3. Asset Store exige scanning, moderação, licença e rollback antes de pagamentos.
4. Matriz de 100+ dispositivos, auditoria LGPD/GDPR, localização e crash-free rate são gates, não ações pós-lançamento.

**Gate 1.0:** métricas do plano principal atendidas e três jogos comerciais publicados com a Aether.

## 12. Estratégia de testes e evidências

| Nível | Finalidade | Exemplos |
|---|---|---|
| Unitário | Invariantes locais | math, handles, migração, parser, grafo de jobs |
| Integração | Fronteiras reais | C#↔C++, ECS↔Jolt, Render Graph↔RHI, Flow↔Physics |
| Differential/property | Equivalência | Flow interpretado vs C#; SIMD vs escalar; serializers |
| Stress/fuzz | Corrupção e concorrência | WAL parcial, scene fuzz, jobs, handles reciclados |
| Hardware | Driver, toque, energia e lifecycle | Vulkan, input, thermal, storage, áudio |
| Visual | Render e UI | golden images perceptuais, layouts e perfis |
| Usabilidade | Produto | PoC-B, SUS M3, criação M5 |
| Soak | Estabilidade | 30 min M0, sessões longas de editor/play/build |

Toda métrica deve registrar build, commit, aparelho, temperatura inicial/final e distribuição p50/p95/p99. Benchmarks não rodam misturados a testes de correção e não podem usar workaround oculto.

## 13. Riscos e decisões recomendadas

| Risco | Decisão recomendada | Sinal de reavaliação |
|---|---|---|
| Interop C#↔nativo caro | Lotes blittable e contador obrigatório | > 3 ms CPU ou crossings acima do orçamento |
| Vulkan inconsistente | Capabilities + fallback e device lab cedo | Crash/artefato em mais de um driver por perfil |
| iOS restringe hot reload | Android-first e ADR jurídico/técnico | Impossibilidade de cumprir PoC-D dentro das regras |
| Jolt 2D caro | Benchmark real contra Box2D | Ganho ≥ 30% que justifique backend adicional |
| Determinismo impossível no Jolt float | Modo/backend restrito separado | Divergência entre ARM64/OS nos testes de replay |
| Editor touch não atinge SUS | Reprojetar UX antes de M4 | SUS < 72 ou tarefas acima do tempo-alvo |
| Escopo excede capacidade | Gates rígidos e itens opcionais atrás de flag | Marco falha duas revisões consecutivas sem reduzir risco |
| Abstração precoce | Vertical slices e metadata única | Interface sem dois consumidores reais |

## 14. Sequência inicial recomendada

### Sprint 1 — estado e riscos de correção

- matriz de marcos e alinhamento documental;
- suites CI sem skip silencioso;
- Flow `return`/`break`/`continue`;
- descriptor V2 do mundo físico e retorno de erros do `Step`.

### Sprint 2 — primeiro hardware

- runner ADB e inventário de aparelhos;
- instalar/abrir APK e validar lifecycle;
- logs/telemetria categorizados;
- swapchain e triângulo.

### Sprint 3 — M0 gráfico

- cubo texturizado;
- batching C#↔Vulkan e contador de interop;
- shader reload;
- captura e testes de surface loss.

### Sprint 4 — M0 humano e térmico

- testes PoC-B;
- soak PoC-C;
- compressão PoC-E;
- relatório e gate M0.

### Sprints 5–7 — núcleo M1

- Job graph, acesso ECS read/write, Node;
- source generator e serializer aliases;
- SQLite e Asset Dependency Index;
- tempo/eventos/configuração e benchmark 100k.

### Sprints seguintes

M2 passa a ser o caminho crítico. Correções de cinemáticos, triggers e batching de física podem ocorrer em paralelo, mas nenhuma fase de produto avança para M3 antes de M2 verde.

## 15. Definition of Done por item

Cada subetapa só pode ser marcada como concluída quando contém, conforme aplicável:

- API pública e ownership definidos;
- lifecycle completo, inclusive destroy/disable/reload;
- metadata/reflexão;
- serialização e migração;
- Inspector;
- Command/undo/redo para mutação editorial;
- acesso por script;
- acesso por Flow ou justificativa de dependência ainda fechada;
- tratamento de erro e logging categorizado;
- capability/fallback mobile;
- testes de regressão e integração;
- benchmark/orçamento para caminho quente;
- documentação curta de threading, limitações e compatibilidade;
- clean build C#, C++, Android debug/release e lint quando afetados;
- validação em aparelho quando a hipótese não for puramente portátil.

## 16. Resultado esperado

Este plano evita dois erros: declarar fases concluídas por causa de PoCs isoladas e reescrever fundações que já têm testes. A execução preserva as peças válidas, corrige primeiro as divergências silenciosas, fecha os marcos na ordem de dependência e exige evidência objetiva para cada avanço.

O próximo estado correto do projeto não é “mais features”. É **M0 comprovado em hardware, correções B1 encerradas e M1 mensurável**. Depois disso, renderer, editor, simulação, Flow e ferramentas podem crescer sobre uma base cuja arquitetura e limites já foram testados no ambiente real do produto.
