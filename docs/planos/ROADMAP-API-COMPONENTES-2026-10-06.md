# Roadmap — API, componentes e funcionalidades derivadas

Branch `claude/api-componentes`, criada em 06/10/2026 a partir de `d12c2ea2` (mesmo commit de `main` e `codex/gameplay-runtime`), no worktree `Downloads/atchengine-api`. O checkout `Downloads/atchengine` continua com a frente do usuário (UI/shell) e não é tocado por esta branch.

Este documento ordena o trabalho. Ele não declara capacidades novas: os números abaixo vêm de leitura do checkout em `d12c2ea2`, sem build ou execução nesta etapa.

## 1. Planos que de fato fazem essas melhorias

| Plano | Papel neste roadmap | Situação |
|---|---|---|
| [EXPANSAO-OBJETOS-COMPONENTES-API-2026-09-26](EXPANSAO-OBJETOS-COMPONENTES-API-2026-09-26.md) | Catálogo-alvo por família (211 tipos, 254 entradas no Add), API por onda e definição de pronto por tipo | Referência de escopo. Onda 0 em grande parte executada; ondas 1–5 parciais |
| [ampliacao-2026-09-23/FAMILIAS-ESTADO](ampliacao-2026-09-23/FAMILIAS-ESTADO.md) + [ROADMAP P00–P20](ampliacao-2026-09-23/ROADMAP.md) | Saldo oficial: 91 capacidades (87 obrigatórias + 4 condicionais), dependências entre pacotes | Saldo: 10 concluídas, 5 parciais, 72 a auditar |
| [PLANO-PRODUCAO-EM-LOTE-ATTACHSENGINE](PLANO-PRODUCAO-EM-LOTE-ATTACHSENGINE.md) | Método: contratos JSON → geração de descritores, campos, enums e fachadas C# | Entrega 1 aplicada (45 tipos, 374 propriedades geradas). Métodos/eventos pendentes |
| [FECHAMENTO-DE-CAPACIDADES-2026-10-02](FECHAMENTO-DE-CAPACIDADES-2026-10-02.md) | Contrato de método/evento e cadeia de evidência por cenário; bibliotecas candidatas | Rejeitado como plano de execução; o desenho de métodos/eventos e evidência continua válido |
| [API-RUNTIME-EXPANSAO-2026-09-30](API-RUNTIME-EXPANSAO-2026-09-30.md), [EXECUCAO-AUDITORIA-UNITY-ASTRA-2026-09-30](EXECUCAO-AUDITORIA-UNITY-ASTRA-2026-09-30.md), PACOTE-50*, MECANISMOS-FISICOS, CHARACTER-*, INPUT-*, TIMER-*, TWEEN-*, AUDIO-* | Registros do que já foi entregue e seus limites | Histórico; usados para não reimplementar |

Os planos de UI ([UI-ROADMAP-COMPLETO](UI-ROADMAP-COMPLETO-2026-10-03.md), [GUI-AUTORIA-E-IMGUI](GUI-AUTORIA-E-IMGUI-2026-10-02.md)) e de renderização (`codex/render-rebuild`) pertencem a outras frentes. Este roadmap consome o que elas publicarem, sem editar os mesmos sistemas.

## 2. Estado de partida (leitura estática em `d12c2ea2`)

| Área | Evidência | Número/estado |
|---|---|---|
| Schemas de componente | `docs/componentes/MATRIZ-PROPRIEDADES.md`, `tools/component_contracts/` | 45 schemas, 44 no Add, 44 fachadas geradas |
| Receitas de criação | `native/editor/editor_creation_catalog.h:195` | 77 entradas |
| ABI de scripts | `native/scene/script_runtime.h:232` `ScriptSceneAccess` | versão 41; `available()` exige **todos** os ponteiros |
| Contrato de tipo | `native/scene/components.h:231` `ComponentType` | números, booleanos, enums, referências, triplas, recursos, slots, coleções. **Sem métodos nem eventos** |
| Callbacks de Behavior | `managed/Astra.Scripting/Behavior.cs:634-662` | Awake, Enable, Disable, Start, Update, LateUpdate, FixedUpdate, ApplicationPause/Focus, TimerElapsed, Trigger/Collision Enter/Stay/Exit, Destroy, Stop |
| Conexões autoráveis | `native/runtime/object_activation_connection.h` | Três emissores fixos (Timer, TransformTween, conexões de física 3D/2D) e uma única ação: ativar/desativar/alternar objeto |
| Serviços C# presentes | `managed/Astra.Scripting/` | GameObject, Transform, Time, Mathf, RandomStream, Awaitable/Coroutine, Physics/Physics2D, Input, Save, Audio, Paths, Timer, Tween/NumberTween, Gui, Materials, Animation |
| Serviços ausentes | busca sem resultado no escopo `managed/Astra.Scripting` e `native/runtime` | carregamento de cena, Screen/Application, Debug.DrawLine/DrawRay, Haptics, ScreenPointToRay/WorldToScreen |
| Material físico | `native/scene/physics_body.h:59` | atrito/restituição ficam no corpo, não por colisor; não existe recurso PhysicsMaterial |
| Tween composto | `managed/Astra.Scripting/Tween.cs` | sem sequência/paralelo (F010 parcial) |

Famílias sem nenhum tipo no catálogo: navegação, partículas, sprite/tiles, decal, linha/trail, sondas de reflexão/luz, Animator, Timeline, câmera virtual com prioridade/blend, efeitos de áudio, vídeo e localização.

## 3. O que este roadmap acrescenta aos planos existentes

Os planos anteriores tratavam cada pacote como mais callbacks na ABI e mais emissores especiais. Quatro problemas estruturais aparecem no código e passam a ser trabalho explícito:

1. **ABI tudo-ou-nada.** Cada função nova incrementa `ScriptSceneAccess.version` e entra em `available()`. A frente de UI chegou à v41 adicionando oito callbacks de GUI; esta branch também precisaria somar callbacks. As duas frentes passariam a disputar o mesmo número de versão e o mesmo `available()`, e um host sem uma única função recusa o runtime inteiro. O plano de 26/09 previa `familyCommand` versionado por tamanho; ele nunca foi implementado.
2. **O contrato não descreve comportamento.** `ComponentType` não tem métodos nem eventos. Cada operação (Play de áudio, Restart de timer, Stop de follower) virou um callback específico com códigos inteiros (`timerCommand` 0..4, `audioCommand` 0..4, `pathRuntimeCommand` 0..5), sem descritor comum para API, ajuda e editor.
3. **Não existe evento genérico (F008).** As conexões atuais ligam três emissores codificados a uma ação. Não é possível ligar “sensor entrou” a “tocar áudio”, “iniciar timer” ou “alterar propriedade” sem script.
4. **A evidência de contrato verifica nomes.** `auditComponentContracts()` confirma presença de consumidor declarado, não o efeito de cada propriedade. A associação propriedade → cenário que observa o efeito continua pendente.

## 4. Blocos, em ordem de dependência

Cada bloco fecha com a trilha completa da AGENTS §6: dado → editor/API → validação → persistência → sincronização → consumidor real, mais Undo, Play e save/reopen quando aplicáveis. Referências versionadas: Unity 6000.0 e Godot 4.5.

### Bloco A — Fundação da API (sem tipos novos)

**Estado (06/10):** A1 e A2 implementados — ver [contrato e evidência](API-METODOS-EVENTOS-ABI42-2026-10-06.md).

**A1. Tabela de capacidades na ABI.** `ScriptSceneAccess` passa a ter um núcleo obrigatório congelado e uma tabela de famílias opcionais, cada uma com id, versão e tamanho próprios. O lado C# consulta a família antes de usar e recusa com `NotSupported` explícito apenas aquela operação. Decisão na implementação: as funções existentes (GUI, áudio, caminhos, timer, tween, input, física 2D) ficam no núcleo congelado v42, para não reescrever a superfície que a frente de UI está editando; só funções novas entram como família.
- Aceite: host antigo sem a família de áudio continua executando scripts que não usam áudio; script que usa áudio recebe falha explícita; frentes diferentes adicionam famílias sem colidir no número de versão.
- Referência: Godot 4.5 [GDExtension interface](https://docs.godotengine.org/en/4.5/tutorials/scripting/gdextension/gdextension_cpp_example.html) (resolução de funções por nome/versão) como princípio; a implementação é Astra.

**A2. Métodos e eventos no contrato.** `ComponentType` ganha descritores de método (argumentos, retorno, erros, fase, mutabilidade em Play) e de evento (payload, emissor, ordem, comportamento após remoção/Stop). O gerador de `tools/component_contracts` produz fachadas C# e entradas de ajuda a partir deles. Os comandos inteiros atuais (timer, audio, tween, path) são reexpostos como métodos descritos, ligados às mesmas funções reais.
- Aceite: `AudioSource.Play()`, `Timer.Restart()` e `PathFollow.Stop()` gerados a partir do descritor produzem o mesmo efeito que os comandos atuais; método sem implementação falha na geração, nunca em silêncio.
- Referência: Godot 4.5 [ClassDB](https://github.com/godotengine/godot/blob/4.5/core/object/class_db.h) (métodos e sinais registrados junto das propriedades).

### Bloco B — Eventos e conexões gerais (F008)

**Estado (06/10):** implementado — ver [Conexão de evento](CONEXOES-DE-EVENTO-2026-10-06.md). As conexões especializadas existentes permanecem até a fila transportar Stay; condição de remoção registrada no documento.

Componente repetível “Conexões de evento”: emissor (qualquer evento descrito em A2) → filtro opcional → lista ordenada de ações. Ações iniciais, todas com consumidor existente: ativar/desativar objeto, alterar propriedade refletida, chamar método descrito (A2), enviar mensagem a Behavior (`SendMessage` já existe), iniciar Timer/Tween, tocar áudio. As conexões de Timer, Tween e física migram para o modelo comum com migração de arquivo; o formato antigo continua legível.
- Aceite: “sensor entrou → tocar som e ativar porta” montado só no Inspector, salvo, reaberto, executado em Play; remover o receptor encerra a conexão com diagnóstico; reentrância limitada e documentada.
- Referência: Unity 6000.0 [UnityEvent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html); Godot 4.5 [signals](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html).

### Bloco C — Serviços de runtime ausentes

**Estado (06/10):** Screen/vista, conversões de câmera, Debug.DrawLine/DrawRay, vibração e ParentChanged/ChildrenChanged implementados — ver [serviços de runtime](SERVICOS-RUNTIME-2026-10-06.md). Realocados com motivo: carregamento de cena virou o bloco C2 (F004), implementado — ver [cenas em Play](CENAS-EM-PLAY-2026-10-06.md); JointBreak exige limite de quebra nas juntas e ControllerColliderHit exige contatos do CharacterVirtual, ambos entram no bloco F; BecameVisible depende de retorno de visibilidade do renderer (coordenar com `codex/render-rebuild`); área segura real exige `WindowInsets` no shell Java (coordenar com a frente de UI).

- **Cena (F004):** carregar cena única/aditiva/assíncrona, descarregar, cena ativa, eventos de carga; handles da cena descarregada invalidados por geração. Referência: Unity 6000.0 [SceneManager](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.html).
- **Application/Screen:** tamanho, DPI, área segura, orientação, taxa alvo, plataforma; pausa/foco já chegam como callbacks. Referência: [Screen](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Screen.html), [Application](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Application.html).
- **Câmera:** ScreenPointToRay, WorldToScreen/ViewportPoint, usando a câmera autorada de jogo. Referência: [Camera.ScreenPointToRay](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Camera.ScreenPointToRay.html).
- **Debug em Play:** DrawLine/DrawRay com duração, desenhados pelo overlay de gizmos existente; não persistem. Referência: [Debug.DrawLine](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Debug.DrawLine.html).
- **Haptics:** vibração por duração/amplitude via Vibrator Android; host recusa explicitamente.
- **Callbacks faltantes:** ParentChanged, ChildrenChanged, BecameVisible/Invisible (depende do teste de visibilidade do renderer), JointBreak e ControllerColliderHit. Ordem comparada com [U9](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html).

### Bloco D — Composição de tempo (F010)

**Estado (06/10):** implementado — ver [Sequência de tweens](SEQUENCIA-DE-TWEENS-2026-10-06.md). Etapas sobre Transform Tween; NumberTween em etapa autoral fica pendente (trilha só de script).

Sequência e paralelo persistentes sobre TransformTween e NumberTween existentes: inserção, intervalos, callbacks, loop do grupo, cancelamento único. Scheduler atual, sem segundo serviço. Referência: Godot 4.5 [Tween](https://docs.godotengine.org/en/4.5/classes/class_tween.html) (`chain`/`parallel`).

### Bloco E — Reconciliação do saldo

**Estado (06/10):** reconciliação feita em [FAMILIAS-ESTADO](ampliacao-2026-09-23/FAMILIAS-ESTADO.md): das 87 obrigatórias, 10 concluídas, 47 parciais e 30 ausentes (novo estado: conferido sem tipo ou consumidor). Leitura estática de código e testes existentes; nenhum aceite novo foi executado. A associação propriedade → cenário de efeito continua pendente.

Das 72 linhas “auditar”, várias têm implementação no checkout (Mesh, MeshRenderer, SkinnedMesh, LOD, Camera, Light, corpo/colisores/juntas/personagem/queries Jolt, Physics2D, ActionMap). Conferir cada requisito do catálogo contra o código, fechar com cenário host quando o efeito estiver comprovado e registrar a lacuna real quando não estiver. Ferramenta: `tools/report-family-progress.py`. Resultado esperado: contagem real de concluídas/parciais publicada, e a lista de lacunas que alimenta os blocos F–L.

Junto: associar propriedades a cenários de efeito (problema 4 da seção 3), começando pelas famílias reconciliadas.

### Bloco F — Física 3D sobre Jolt (P07)

**Estado (06/10):** PhysicsMaterial implementado — ver [Material físico](MATERIAL-FISICO-2026-10-06.md) (no corpo, como no Godot; por colisor depende de F038). Restante do bloco pendente.

- **PhysicsMaterial (F039):** recurso com atrito, restituição e modo de combinação, atribuído por colisor; migração do atrito/restituição atuais do corpo. Referência: [PhysicsMaterial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PhysicMaterial.html).
- **Composição de colisores (F038):** vários colisores em filhos formando um corpo composto.
- **Componentes de consulta:** Raio e Varredura de forma persistentes (Godot [RayCast3D](https://docs.godotengine.org/en/4.5/classes/class_raycast3d.html)/[ShapeCast3D](https://docs.godotengine.org/en/4.5/classes/class_shapecast3d.html)), Braço de mola ([SpringArm3D](https://docs.godotengine.org/en/4.5/classes/class_springarm3d.html)) para câmera.
- **Ragdoll (F044)** com as juntas já existentes, e **veículo** sobre `VehicleConstraint` do Jolt (F091, condicional).

### Bloco G — Câmera virtual (F023)

Câmera virtual com prioridade, cérebro com blend, órbita, colisão/desoclusão e ruído, compondo `astra.camera`, `camera.follow` e `camera.look` existentes. Referência: [Cinemachine 3](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/index.html).

### Bloco H — Áudio completo (F058, F059, F061)

Streaming com pontos de loop, prioridade/preempção de vozes, mistura espacial contínua, sends e efeitos selecionados no mixer, snapshots. miniaudio 0.11.23 já integrado. Referências: [AudioSource](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html), Godot 4.5 [Audio buses](https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html).

### Bloco I — Animação (F062–F066)

Animator com parâmetros, estados, transições, blend 1D/2D, camadas/máscaras e eventos; depois Timeline. Avaliar ozz-animation para sampling/blending antes de escrever avaliador próprio. Exige superfície de grafo no editor. Referência: [Animator Controller](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AnimatorController.html).

### Bloco J — Navegação (F067–F070)

Recast/Detour: recurso de navmesh versionado, bake cancelável, agente que propõe velocidade ao personagem/corpo, obstáculo e link. **Dependência nova: exige aprovação do usuário antes de entrar no build.** Referência: [Recast 1.6.0](https://github.com/recastnavigation/recastnavigation/tree/v1.6.0).

### Bloco K — Partículas e linhas (F025, F075)

Emissor com módulos e curvas/gradientes (F076 já existe), Line/Trail. Depende do renderer: coordenar com `codex/render-rebuild` antes de começar.

### Bloco L — Mundo 2D (F071–F074)

Sprite/AnimatedSprite, TileSet/TileMapLayer com pintura, Camera2D/Parallax, Light2D. Física 2D Box2D já existe como base.

## 5. Ordem de execução proposta

| Ordem | Bloco | Por quê nesta posição |
|---|---|---|
| 1 | A1 + A2 | Destrava todo o resto e elimina a disputa de versão da ABI com a frente de UI |
| 2 | B | Maior ganho de autoria sem script; usa A2 |
| 3 | C | Serviços básicos que todo jogo usa; independentes entre si |
| 4 | E | Barato e corrige a contagem; define as lacunas de F–L com dados |
| 5 | D, F, G | Ampliam backends existentes (tween, Jolt, câmera) |
| 6 | H | Áudio já tem backend; completa as três linhas parciais |
| 7 | I, J | Sistemas grandes; J depende de aprovação |
| 8 | K, L | Dependem de coordenação com renderer e com a frente 2D |

## 6. Regras de convivência com as outras frentes

- Esta branch só recebe commits do worktree `atchengine-api`. Não integrar `codex/gameplay-runtime` nem `codex/render-rebuild` sem pedido.
- Atlas de ícones (`assets/astra-visual/ui/astra-ui-icons.png`, `catalog.json`, `astra-ui-icons.aeui`) é regenerado pelas duas frentes. Ícones novos entram como SVG; o atlas é regenerado de novo no momento do merge, em vez de resolver conflito no PNG.
- Até A1 existir, qualquer callback novo na ABI desta branch conflita com a frente de UI. Por isso A1 vem primeiro.
- O worktree não tem `build/`. Configurar `build/api-host` próprio (mesmo gerador e `-DAETHER_VULKAN_HEADERS` do host) em vez de compartilhar o diretório de build do outro checkout.

## 7. Aceite por bloco

1. Referência Unity 6000.0 ou Godot 4.5 com link no schema ou no documento do bloco.
2. Nenhum controle, método ou ação sem consumidor real; capacidade indisponível recusa explicitamente.
3. Persistência com migração dos formatos anteriores; payload desconhecido preservado.
4. Inspector e ícones conforme `AGENTS.md` do editor: captura real, conceito gerado só como hipótese, captura posterior.
5. Testes host focados no fluxo (criar → editar → salvar/reabrir → Undo/Redo → Play com efeito observável); build Android do alvo arm64 antes de commit que toque `native/`.
6. Contagens publicadas separando implementado, parcial e pesquisado. Validação no aparelho relatada à parte.
