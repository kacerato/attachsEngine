# Plano de fechamento de lacunas e convergência dos marcos

> **Estado vivo em 26/08/2026:** 11 das 13 lacunas reais estão integralmente
> fechadas (`GAP-FLOW-01`, `GAP-PHY-01`, `GAP-PHY-02`, `GAP-PHY-03`,
> `GAP-PHY-04`, `GAP-JOINT-01`, `GAP-JOB-01`, `GAP-ECS-01` e `GAP-SER-01`).
> `GAP-FLOW-02` também foi fechada com contexto de execução injetado e
> capabilities validadas; `GAP-CHAR-01`, com composição de movimento centralizada.
> Cinco registros anteriores foram removidos porque
> eram entregas futuras do roadmap, não lacunas de fases já executadas.
> Os gates M0–M9 permanecem 0/10;
> avanços parciais de hardware/shell não encerram M0. Evidências e contagens
> detalhadas ficam em `ESTADO.md`.

## 1. Propósito e autoridade

Este documento **fecha lacunas deixadas abertas por fases já executadas** do
`PLANO-ENGINE-MOBILE.md`. Ele não é um roadmap e não descreve o que a engine
ainda vai construir — isso é atribuição exclusiva do plano principal, que
detalha todos os ~330 itens de 0.1.1 a 9.6.7 com critérios de sucesso próprios.

**Regra de escopo (o que este documento pode e não pode conter):**

| Pode | Não pode |
|---|---|
| Registrar uma lacuna concreta observada no código que já existe | Descrever trabalho futuro que o plano principal já especifica |
| Definir o critério objetivo que fecha essa lacuna | Reescrever, resumir ou parafrasear itens do plano principal |
| Documentar como uma lacuna foi fechada, com evidência | Definir gates ou marcos — os gates M0–M9 são do plano principal |
| Ordenar as lacunas entre si por dependência e risco | Ordenar o roadmap do produto |

Um item do plano principal que simplesmente **ainda não foi iniciado não é uma
lacuna** — é trabalho futuro normal, e pertence só ao plano principal. Lacuna é
o que ficou *para trás*: divergência silenciosa, ABI incorreta, estado não
sincronizado, teste que mascara ausência de dependência, item marcado como
pronto sem evidência que sustente.

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

### 2.3 Lacunas registradas

| ID | Classe | Lacuna | Critério objetivo de fechamento |
|---|---|---|---|
| GAP-HW-01 | B0 | Vulkan e lifecycle não validados em GPU física | Matriz mínima de aparelhos verde, incluindo perda/recriação de surface e background/foreground |
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
| GAP-2D-01 | B0/B4 | Decisão Jolt restrito × Box2D sem evidência móvel completa | Benchmark equivalente em hardware e ADR com decisão mensurável |

Os registros removidos foram `GAP-M0-02`, `GAP-CORE-01`, `GAP-RHI-01`,
`GAP-EDITOR-01` e `GAP-CHAR-02`. PoCs de M0, conclusão do Core, renderer real,
editor integrado e recursos futuros do character são itens ainda não iniciados
ou parciais do plano principal. Eles continuam visíveis item a item em
`MATRIZ-MARCOS.md`, sem inflar artificialmente este plano temporário.

## 3. Modelo de execução

### 3.1 Trilhas permanentes

Fechar uma lacuna costuma exigir trabalho em mais de uma trilha ao mesmo tempo.
Nenhuma delas pode ser deixada para o fim de um fechamento:

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

## 4. Verdade operacional e correções de segurança

**Objetivo:** impedir que o projeto expanda sobre estado incorreto ou documentação ambígua.

Esta seção é um pré-requisito de todas as lacunas de §5: enquanto o estado
registrado não for confiável e o CI puder reportar verde sem ter rodado, nenhum
fechamento de lacuna pode ser verificado. **Estado: concluída** — §4.1 e §4.3/§4.4
fechados, §4.2 fechado exceto o orçamento de métricas versionadas (ver `ESTADO.md`).

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
   instala headers Vulkan isolados do NDK e executa a suíte corrente (263 testes C# + 108 nativos).
5. ✅ `metrics/budgets.v1.json` versiona budgets de P/Invoke, alocação, CPU, GPU,
   memória e energia. `tools/validate-metrics-budget.ps1` valida o contrato e quebra
   o gate ao ultrapassar limites; coletores que exigem dispositivo permanecem
   declarados como `device-required`, sem fabricar medições em runner hospedado.

**Aceite:** o caminho limpo foi reproduzido localmente; a regressão corrente passa
com 274/274 testes C# e 108/108 nativos, e a fixture negativa de 301 chamadas
nativas/frame quebra o gate.
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

## 5. Sequenciamento das lacunas

As lacunas abertas não são executadas em ordem numérica de `GAP-*`, e sim por
**classe de risco e dependência real** (§2.2). A tabela abaixo é a única ordem
que este documento define. Ela ordena *lacunas*, não o roadmap: o que construir
depois de cada marco continua sendo respondido exclusivamente pelo
`PLANO-ENGINE-MOBILE.md`.

| Ordem | Lacuna | Classe | Bloqueia | Estado |
|---|---|---|---|---|
| 1 | `GAP-FLOW-01` — controle de fluxo divergente | B1 | Qualquer expansão do AetherFlow | ✅ fechada |
| 2 | `GAP-PHY-01` — capacidade derivada de `maxBodies` | B1 | Qualquer cena densa | ✅ fechada |
| 3 | `GAP-PHY-02` — cinemáticos sem resync ECS→Jolt | B1 | Plataformas móveis, animação | ✅ fechada |
| 4 | `GAP-JOB-01` — ciclos de job só por timeout | B1 | Paralelismo confiável | ✅ fechada |
| 5 | `GAP-ECS-01` — leitura marca escrita | B2 | Change detection exata, 1.3.x | ✅ fechada |
| 6 | `GAP-SER-01` — rename perde valor no texto | B2 | Formato de projeto estável, 1.4.x | ✅ fechada |
| 7 | `GAP-PHY-03` — sem Trigger/sensor persistente | B2 | Gameplay orientado a evento | ✅ fechada |
| 8 | `GAP-PHY-04` — P/Invoke individual | B4 | Orçamento de interop, spawn massivo | ✅ fechada |
| 9 | `GAP-JOINT-01` — juntas fora do ECS | B3 | Autoria de juntas no editor | ✅ fechada |
| 10 | `GAP-HW-01` — Vulkan/lifecycle sem matriz de GPU | B0 | **Todo o restante** — risco existencial | ⏳ parcial (1 aparelho) |
| 11 | `GAP-2D-01` — decisão Jolt × Box2D sem B/C | B0/B4 | Decisão já declarada no item 4.1.6 | 🟡 harness/host/A prontos; B/C pendentes |
| 12 | `GAP-FLOW-02` — Flow sem serviços externos | B3 | Nós externos sem singleton | ✅ fechada |
| 13 | `GAP-CHAR-01` — composição de movimento manual | B3 | Character já existente | ✅ fechada |

**Como ler esta tabela:** "Bloqueia" indica o que fica comprometido enquanto a
lacuna existir — não é uma lista de trabalho a fazer. Uma lacuna B0 aberta
significa que expandir o subsistema afetado constrói sobre fundação não
verificada. Itens de escopo futuro não entram mais nesta tabela.

As duas lacunas ainda abertas recebem detalhe apenas quando isso corrige código
ou método já entregue. O restante da engine continua especificado item a item no
plano principal; reescrevê-lo aqui produziria uma segunda fonte de verdade mais
pobre e desatualizada.

## 6. Detalhe de fechamento das lacunas que tocam código existente

Esta seção existe só para lacunas cujo fechamento **altera código já escrito** —
ABI, contrato, autoridade de dados, formato persistido. Nesses casos o "como"
não está no plano principal (que descreve o que construir, não como consertar o
que já existe), então precisa ser registrado aqui.

Itens que se resolvem simplesmente executando trabalho futuro do plano principal
**não são lacunas e não têm seção aqui de propósito**.

### 6.1 Laboratório de dispositivos (`GAP-HW-01`)

O plano principal pede o farm de aparelhos no item 0.1.2, mas não define o
procedimento de validação nem o que constitui evidência aceitável. Isso é
lacuna de método, não de escopo:

1. Disponibilizar inicialmente seis aparelhos: ao menos dois Adreno, dois Mali, um perfil C fraco e um S/A recente. Expandir para os 12 do item 0.1.2 antes de M2.
2. Registrar por aparelho: modelo, SoC, GPU, RAM, versão do Android, driver Vulkan, extensões, taxa de atualização, suporte a página de 16 KB e estado térmico.
3. Runner ADB reproduzível que instala, limpa dados, abre, coleta Logcat, alterna background/foreground, exercita configuração e encerra restaurando o estado do aparelho.
4. Exercitar perda/recriação de janela, screen off/on, interrupção por Activity, memória baixa e retomada.
5. Quarentena documentada para defeito específico de driver, com fallback e prazo; nunca ignorar um aparelho silenciosamente.

**Critério de fechamento:** `Surface Vulkan pronta` e lifecycle completo verdes
na matriz mínima de seis aparelhos; todo crash traz stack e capabilities.

**Estado:** parcial — `tools/validate-android-shell.ps1` implementa os itens 3 e
4 e roda verde em 1 aparelho (Xiaomi SM8735/Adreno). Faltam Mali, perfil C e a
política de quarentena. Evidência em `ESTADO.md`, seção "Shell Android".

### 6.2 Física — estabilização da base existente

Todo o conteúdo abaixo trata de código de física **já escrito** cuja ABI,
autoridade ou contrato precisou mudar. Os itens 4.1.x do plano principal
descrevem o que a física deve fazer; esta seção registra o que estava errado no
que já existia e como foi corrigido.


#### 6.2.1 Cinemáticos e sincronização

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

#### 6.2.2 Triggers e eventos

**Estado: fechado para o critério de `GAP-PHY-03`; extensões de editor/Flow seguem
nos gaps próprios.**

1. `AetherBodyDescV2`/`AetherPhysics_CreateBodyV2` expõem sensor e filtro sem
   alterar `AetherBodyDesc` V1.
2. `TriggerContactListener` agrega callbacks concorrentes por subshape/par sob
   sincronização explícita, deduplica e só publica uma fotografia ordenada após o
   step. A alternativa lock-free permanece otimização mensurável de `GAP-PHY-04`,
   não condição de correção sacrificada antecipadamente.
3. Enter/Stay/Exit usam `JPH::BodyID` estável; filtros Static/Dynamic, buffer do
   chamador e contagem real são cobertos. Corpos sobrepostos são mantidos acordados
   para impedir Exit falso por sleep; destruir encerra o par no step seguinte.
4. `Trigger` é componente ECS, possui metadata/serialização binária e texto, API C#
   em `PhysicsWorld` e resolução handle→`EntityId`. Inspector e nós Flow dependem,
   respectivamente, do editor real (itens 3.x do plano principal) e de `ExecutionServices`
   (`GAP-FLOW-02`), sem singleton temporário.
5. Disable/hot-edit usa contrato explícito destroy→recreate; reparent segue a
   autoridade de transform já definida. Scene unload continua usando a disciplina
   de `PhysicsSyncSystem.DestroyBody` existente para todos os corpos, não um caminho
   especial de trigger.

#### 6.2.3 Batching e orçamento de interop

**Estado: fechado (`GAP-PHY-04`).**

1. `CreateBodies(ReadOnlySpan<PhysicsBodyDescription>, Span<PhysicsBodyHandle>)`
   e `DestroyBodies(ReadOnlySpan<PhysicsBodyHandle>)` atravessam a ABI uma vez.
2. Criação nativa é all-or-none: cria IDs fora da broadphase, desfaz o prefixo
   inteiro em falha e usa `AddBodiesPrepare/Finalize`; destroy usa `RemoveBodies`.
3. Chamadas unitárias C# são wrappers de lote de tamanho 1. `PhysicsSyncSystem`
   agrega todos os corpos novos do Step com buffers de `ArrayPool`, em vez de
   fazer crossing por entidade.
4. `PhysicsBodyInteropStatistics` contabiliza crossings, bytes enviados/recebidos
   e corpos criados/destruídos. A telemetria é deliberadamente deste domínio; o
   contador global multissubsistema continua parte do gate device-required da PoC-A.
5. Regressões 100/1.000/10.000 comprovam 1 crossing create + 1 destroy, handles
   íntegros e zero alocação gerenciada dentro do crossing; testes C++ cobrem
   rollback por capacidade, ordem descriptor→handle e devolução de capacidade.

#### 6.2.4 Juntas completas e ECS

**Estado: fechado para o critério de `GAP-JOINT-01`; extensões de tipos,
Inspector e Flow permanecem nos marcos/gaps consumidores próprios.**

1. `AetherJointDescV2` preserva o símbolo/layout V1 e adiciona `World`,
   `LocalToBody1` e `LocalToBody2`. Pontos usam posição+rotação do corpo de
   referência; eixos usam somente rotação. Testes físicos cobrem ponto local e
   eixo local rotacionado, não apenas conversão matemática isolada.
2. `Joint` é componente declarativo serializável, referencia os dois corpos por
   `EntityId` geracional e mantém o `PhysicsJointHandle` fora da cena. Referências
   ausentes ficam pendentes e resolvem automaticamente quando os corpos surgem.
3. `JointSyncSystem` integra o `PhysicsSyncSystem`: preserva o handle quando
   descriptor/corpos não mudam, usa destroy→recreate em hot-edit e faz sweep de
   componente removido ou entidade destruída. Destruir corpo pelo lifecycle ECS
   remove antes todas as juntas sincronizadas que o referenciam.
4. Validação equivalente existe nos dois lados da ABI: tipo/espaço/motor,
   finitude, eixos não degenerados e contrato Hinge. `-pi/+pi` exatos são
   aceitos como rotação contínua; valor fora do intervalo retorna handle inválido
   antes de tocar nos asserts do Jolt, sem clamp silencioso.
5. Limites, motor, espaços e referências sobrevivem a round-trip binário/texto
   com remapeamento de entidades. A metadata registrada permite ao futuro
   Inspector consumir os campos; UI de produto depende dos itens 3.x e nós
   dependem de `GAP-FLOW-02`, sem criar integração temporária duplicada.
6. SixDOF permanece recurso separado; veículo, gear, pulley e path entram somente
   com consumidor real e slices/testes próprios, portanto não fazem parte do
   critério objetivo de `GAP-JOINT-01`.

#### 6.2.5 Contexto de física no Flow (`GAP-FLOW-02`)

**Estado: fechada.** A lacuna não era "faltam nós de física" (isso é o item 4.1.4
do plano principal) — é que o AetherFlow **já executa** e não tem nenhuma forma
de alcançar um serviço externo, o que forçaria um singleton global se um nó de
física fosse escrito hoje. O critério abaixo existe para impedir esse atalho.

1. ✅ `FlowExecutionContext` injeta `World`, `PhysicsWorld`, tempo, input e logger.
2. ✅ Nós persistem `RequiredCapabilities`; o validador rejeita contexto incompatível e o interpretador falha com diagnóstico do nó/capability.
3. ✅ O gerador recebe o contexto por parâmetro e valida a capability antes do uso; não existe `Current`/singleton.
4. ✅ `log.message` fecha um slice vertical real pelo validador, `.aflow`, interpretador e C# gerado/compilado.
5. Nós concretos de física permanecem exclusivamente nos itens 4.1.4/5.5 do plano principal; sua ausência não reabre esta lacuna arquitetural.

#### 6.2.6 Character Controller (`GAP-CHAR-01`)

**Estado: fechada.** A lacuna era de contrato, não de feature: o character que já
existe funciona, mas exige que o chamador componha gravidade, plataforma e
stick-to-floor manualmente, na ordem certa — disciplina não verificável que já
produziu dois cenários de teste sutilmente errados (documentado em `ESTADO.md`).
Escalar e nadar são escopo futuro do plano principal e não fazem parte deste critério.

1. ✅ `CharacterMotorSystem.UpdateBeforePhysics` compõe velocidade desejada, gravidade limitada, plataforma e o ExtendedUpdate de degraus/stick-to-floor numa ordem única.
2. ✅ `CharacterMotorState` separa `Grounded`, `Falling`, `Rising`, `Sliding` e stance.
3. ✅ `TrySetStance` só altera o estado quando o Jolt aceita a nova forma; nascer agachado continua sendo item futuro, não parte desta correção.
4. ✅ O Core recebe velocidade desejada, não conhece input, câmera, regra de pulo, escalada ou natação; estado/settings são structs sem referência gerenciada.
5. ✅ Os 17 testes de character cobrem piso, teto baixo, rampas, degrau, plataforma, subida/queda, stance, lifecycle e zero GC no fixed step estável.

#### 6.2.7 Física 2D (`GAP-2D-01`)

**Estado: parcial, não fechada.** A lacuna surgiu porque o item 4.1.6 do plano
principal pedia uma *decisão por benchmark* entre Jolt-2D e Box2D v3, mas a
entrega anterior media Jolt restrito contra si mesmo. O segundo termo e o
harness equivalente agora existem; o critério continua incompleto enquanto
faltarem os perfis móveis B/C e a decisão mensurável da ADR-013.

1. ✅ Box2D v3.1.1 foi vendorizado em módulo isolado, com commit e MIT registrados; só os alvos de benchmark o ligam.
2. ✅ O harness usa as mesmas formas, posições, materiais, `dt`, step, warm-up e excitação para impedir que diferenças de sleep falseiem o custo ativo. Três binários separam comparação de CPU e RSS por processo.
3. 🟡 Host e Android perfil A concluídos com p50/p95/p99, RSS, tamanho, temperatura, estabilidade, equivalência geométrica e saída versionada. As execuções integrais ficaram verdes de 50 a 5.000 corpos, mas faltam os mesmos relatórios nos perfis móveis B e C.
4. 🟡 `docs/adr/ADR-013-PHYSICS-2D-BACKEND.md` registra alternativas, evidência host e gate exato. Continua `proposto`; não há decisão definitiva sem B/C.
5. ✅ Nenhuma API pública/runtime foi amarrada ao Box2D. Se o gate móvel aprovar um segundo backend, a interface neutra continua no item 4.1.6 do plano principal.

> **Itens 4.1.7 (decomposição convexa) e 4.1.9 (sub-stepping térmico) não
> aparecem aqui de propósito.** Nunca foram iniciados e o plano principal já os
> especifica — são trabalho futuro normal, não lacuna (§1). O 4.1.8 está
> registrado em `ESTADO.md` como divergência de escopo aceita, também fora deste
> documento.


## 7. Estratégia de testes e evidências

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

## 8. Riscos e decisões recomendadas

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

## 9. Próximo passo

Este documento não define sprints de produto — o que construir e em que ordem
está no `PLANO-ENGINE-MOBILE.md`. O que segue é apenas **qual lacuna atacar em
seguida**, derivado da tabela de §5.

Com onze lacunas fechadas e a verdade operacional (§4) concluída, a próxima
lacuna na ordem de risco é **`GAP-HW-01` (B0)**: a matriz mínima de aparelhos.
Enquanto ela existir, todo trabalho de renderer, editor e simulação é
construído sobre uma fundação verificada em um único driver Adreno.

Fechar `GAP-HW-01` não exige escrever engine nova — exige aparelhos (Mali,
perfil C) e rodar o runner que já existe. Se o hardware não estiver disponível,
lacunas corretivas portáteis podem avançar sem alterar a ordem de risco, desde
que não sejam usadas para declarar M0 verde nem para antecipar entregas do
roadmap. Foi assim que `GAP-FLOW-02` e `GAP-CHAR-01` foram fechadas enquanto a
evidência externa aguarda aparelhos. O harness reproduzível de `GAP-2D-01`
também está pronto e verde no host; seu fechamento exige somente executar os
runners isolados nos perfis B/C e aplicar o gate da ADR-013. Isso não autoriza
implementar um backend de produto antes da decisão nem duplica o roadmap principal.

## 10. Definition of Done por item

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

## 11. Resultado esperado

Este documento evita dois erros: declarar uma fase concluída por causa de uma
fatia isolada, e reescrever fundação que já tem teste. Ele preserva as peças
válidas, corrige primeiro as divergências silenciosas e exige evidência objetiva
para cada lacuna dada como fechada.

Quando as 13 lacunas estiverem fechadas, **este documento deixa de existir** —
não vira um segundo roadmap. O que a engine ainda vai construir está inteiramente
no `PLANO-ENGINE-MOBILE.md`, e o estado real de cada item está em
`ESTADO.md`/`MATRIZ-MARCOS.md`. O papel deste plano é ser temporário: garantir
que o que já foi construído sustenta o que virá depois.
