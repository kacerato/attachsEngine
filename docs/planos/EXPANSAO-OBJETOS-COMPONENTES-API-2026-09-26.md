# Expansão de objetos, componentes e API — meta: igual ou maior que Unity

Data: 26/09/2026. Checkout: `codex/gameplay-runtime`, `769fd65f`. Tipo de entrega: **plano**, baseado em leitura de código e no inventário Unity já versionado no repositório. Nada foi compilado nem executado para produzir este documento.

Referências: Unity 6000.0 e pacotes fixados em [resumo-cobertura.json](../componentes/pesquisa-2026-09-15/resumo-cobertura.json); Godot 4.5-stable em [godot-manifesto.json](ampliacao-2026-09-23/godot-manifesto.json). O link oficial de cada tipo Unity está no campo `apiDocumentation` de [catalogo-unity.json](../componentes/pesquisa-2026-09-15/catalogo-unity.json). Cada entrega deve copiar esse link para o registro da capacidade.

Este plano **substitui a ordem de execução** do [ROADMAP de 23/09](ampliacao-2026-09-23/ROADMAP.md). O atlas e as fichas daquele diretório continuam valendo como material de pesquisa.

---

## 1. Resumo

| Contador | Hoje (medido) | Unity comparável | Meta final Astra |
|---|---:|---:|---:|
| **Tipos nativos anexáveis** (schema com consumidor real) | 15 | 243 tipos, com duplicações (ver §3) | **211** |
| **Entradas no Add Component** (tipos + variantes nomeadas) | 14 | 243 | **≥ 254** |
| **Cobertura dos 243 tipos Unity** (equivalente ou adaptação explícita) | ≈ 20 parciais | — | **243/243, zero pendentes** |
| **Receitas de criação de objeto** (menu +) | 25 | ≈ 95–110 (estimativa, §3) | **≥ 140** |
| **Callbacks de comportamento** | 10 | ≈ 60 mensagens MonoBehaviour | **≥ 40** (as de runtime) |
| **Fachada C# tipada por componente** | 5 de 15 | 1 por tipo | **211 de 211, gerada** |

A distância é de 15 para 211 tipos. Seguindo o modelo da rodada anterior, isso significaria cerca de 200 enums `EditorWidget`, 200 blocos `switch`, 200 abas ou painéis e 200 ícones repetidos. Por isso, antes de acrescentar tipos, é preciso fazer uma **Onda 0**: registro por família, receitas descritas como dados, tipos de propriedade ricos, Add pesquisável e API C# gerada. Depois dela, um componente simples custa **um arquivo de tipo, uma linha de registro, um ícone e o consumidor**. Não exige workspace, widget nem `switch` novos.

---

## 2. Estado real e o que deu errado na rodada anterior

### 2.1 Inventário medido

| Área | Evidência | Número |
|---|---|---|
| Schemas nativos | `native/scene/component_schema.h:105`, `std::array<ComponentSchema, 15>` | 15: Corpo físico, Personagem, Olhar, Acompanhar alvo, Colisor 3D, Junta, Câmera, Malha, Luz, Ambiente, LOD Group, Malha deformável, Animação, Timer, Comportamento |
| Add Component | `native/editor/editor_component_catalog.h:49`, `std::array<…,14>` | 14 (Comportamento entra pela área de código) |
| Receitas de criação | `native/editor/editor_creation_catalog.h:30`, `std::array<…,27>` | 27 entradas, 25 criam objeto |
| Workspaces | `native/editor/editor_screen.h:493` | 9: Scene, Assets, Lighting, Play, Settings, Code, **Timers, Physics, Input** |
| Tipos de propriedade | `native/scene/components.h`: número, booleano, enum, referência de objeto, tripla (vetor/cor), recurso, número/enum por slot | Faltam inteiro, string, vec2/vec4, quaternion, cor RGBA/HDR, máscara, curva, gradiente, lista de estruturas e conexão de evento |
| API C# | `managed/Astra.Scripting/*.cs`: 356 linhas `public` | `GameObject` com nome, ativo, pai/filhos, find, criar filho, destruir e componentes por string; física (raycast/shape/overlap); input por ação; materiais; animação. **Não existem** `Instantiate`, tempo/escala de tempo, carregamento de cena, `Random`, salvamento, áudio nem UI |
| Callbacks | `managed/Astra.Scripting/Behavior.cs:254-271` | Start, Update, FixedUpdate, Stop, TimerElapsed e Trigger/Collision Enter/Stay/Exit |
| ABI | `native/scene/script_runtime.h:155`, `ScriptSceneAccess` | cerca de 71 ponteiros de função |
| Backends disponíveis | `native/third_party/` | **Jolt** (juntas, veículos, soft body, ragdoll). **Box2D v3.1.1** está vendorizado, mas só entra no benchmark (`native/CMakeLists.txt:75`). Não há áudio, partículas, navegação, UI de jogo, fonte de texto de runtime nem vídeo |

### 2.2 Defeitos estruturais que impedem escalar

1. **Receitas escritas à mão.** Cada receita é um valor de `EditorWidget` e um ramo de ternários em `native/editor/editor_session.cpp:3125-3212`. Doze das receitas físicas diferem apenas por três enums de `PhysicsBody` e `Collider`. Chegar a 140 receitas desse jeito é inviável.
2. **Grupo de propriedades errado.** Luz, Ambiente, LOD, Malha deformável, Animação e Timer usam `EditorPropertyGroup::Material`, e Acompanhar alvo usa `CameraLook` (`editor_component_catalog.h:55-63`). O Inspector já reflete propriedades de forma genérica (`editor_screen.cpp:786`), então esse enum de grupos é resíduo e confunde.
3. **Ícones reaproveitados.** Timer usa o ícone de código, Malha deformável usa o de junta, e Luz e Ambiente dividem o sol (`editor_component_catalog.h:58-63`). Com 200 tipos, isso deixa o Add ilegível.
4. **Add paginado de 6 em 6** (`editor_screen.cpp:2500`, captura `docs/capturas/g5-add-component.png`, “1 / 2”). Com 211 tipos, seriam 36 páginas.
5. **Três workspaces de topo criadas para funções pontuais.** “Timers da cena” mostra um único tipo de componente. “Camadas físicas” e “Mapa de entrada” são configurações do projeto; na Unity ficam em Project Settings. A captura `docs/editor-layout/input-map/captura-android.png` mostra cerca de ⅔ da tela vazia, “Remover ação” e “Remover vínculo” disputando o rodapé e o título “Mapa de entrada” solto na barra superior.
6. **O crescimento foi medido em receitas, não em capacidade.** O menu passou de 10 para 27 entradas enquanto os schemas foram de 13 para 15. O próprio ROADMAP registra isso, mas o marco continuou sendo apresentado como “expansão”.

### 2.3 O que reverter ou realocar (Onda 0)

| Item atual | Destino |
|---|---|
| `EditorWorkspace::Timers` | Remover. O Timer é editado no Inspector, e o estado em Play (contagem, disparos) aparece no cabeçalho do componente durante Play |
| `EditorWorkspace::Physics` (matriz de camadas) | Mover para **Configurações › Projeto › Camadas e colisão**, reaproveitando a mesma operação de histórico |
| `EditorWorkspace::Input` (mapa de ações) | Mover para **Configurações › Projeto › Entrada**, reaproveitando a mesma operação de histórico e as mesmas regras de `InputService` |
| 25 `EditorWidget::Create*` | Converter em linhas de dados (§4.3) e manter os testes existentes de Undo, arquivo e Play |
| `EditorPropertyGroup` nos componentes | Remover. As seções saem de `PropertyPresentation.group` |
| Ícones repetidos | Ícone exclusivo por tipo (§6.5) |

As operações de dados por trás das três workspaces (matriz de camadas, ações de entrada, Timer) estão corretas e continuam. Muda apenas **onde** aparecem.

---

## 3. A régua Unity usada nas metas

### 3.1 Componentes: 243

Fonte: `catalogo-unity.json` (2.476 tipos, 575 Component). Regra reproduzível:

- Entram tipos Component **não abstratos, não obsoletos e não ocultos**.
- Saem as bases e os tipos adicionados automaticamente: Behaviour, Component, Collider, Collider2D, Effector2D, GridLayout, Joint, Joint2D, AnchoredJoint2D, Renderer, MonoBehaviour, PhysicsUpdateBehaviour2D, Transform, RectTransform, CanvasRenderer, ParticleSystemRenderer, UIRenderer, VFXRenderer, Selectable, BaseInput, Panel*, TMP internos, RigConstraint, RigTransform, Universal*AdditionalData, `DebugUIHandler*`, UIFoldout e SceneRenderPipeline.
- Saem os pacotes XR/AR (106), Netcode (14) e HDRP (18). Ficam na Onda 6, condicional.

Resultado: **243** = UnityEngine 97 + Cinemachine 48 + uGUI 34 + Animation Rigging 15 + Localization 9 + 2D Animation 7 + Input System 7 + RP Core 7 + URP 6 + AI Navigation 4 + Splines 4 + SpriteShape 2 + Tilemap Extras 1 + Timeline 1 + VFX 1.

Esse número tem **duplicações internas da Unity**: Text legado e TextMeshPro, Dropdown e TMP_Dropdown, InputField e TMP_InputField, `Multi*Constraint` do Rigging repetindo as seis constraints do núcleo, Rig e RigBuilder, três componentes `*Events` do Cinemachine, 5 colisores 2D que diferem só na forma e 9 juntas 2D. A Astra agrupa esses casos em um tipo com variantes nomeadas. Por isso, a meta de **tipos** (211) fica abaixo de 243, a meta de **entradas no Add** (254) fica acima, e a **cobertura** é de 243/243.

### 3.2 Menu de criação: 95–110

O arquivo `menus-gameobject.json` tem 78 caminhos literais. Ele não inclui registros nativos (sprites 2D, tilemaps, volumes, Cinemachine) e inclui 20 itens XR. A contagem do menu de um projeto Unity 6 URP sem XR é uma **estimativa**: Create Empty 3, 3D Object 11, Effects 5, Light 10 (com 2D), Audio 2, Video 1, UI 17, Volume 4, 2D Object ≈ 17, Camera 1, Cinemachine ≈ 13, AI 3, Splines ≈ 6. Total ≈ 95–110. A meta de 140 supera esse intervalo com receitas úteis ao mobile: joystick virtual, área segura, veículos, ragdoll e água.

### 3.3 API

Seguem a Unity como referência: as mensagens de MonoBehaviour (≈ 60, das quais cerca de 40 são de runtime) e as classes de serviço do núcleo (GameObject/Object, Transform, Time, Physics, Physics2D, Input, Random, Mathf, Debug, Application, Screen, SceneManager, Resources/Addressables, PlayerPrefs, Awaitable/Coroutine e Handheld). Os nomes Astra continuam Astra: `Behavior`, `Start`, `Stop` etc.

---

## 4. Onda 0 — fundação para escalar, sem tipos novos

Nenhum tipo novo entra nesta onda. Ao final dela, os 15 tipos atuais funcionam sobre a nova fundação e a UI anterior foi corrigida.

### 4.1 Registro por família (substitui os dois `std::array` fixos)

- Cada família declara seu próprio `constexpr` de `ComponentSchema`, por exemplo `scene/physics3d_schemas.h`. `componentSchemas` passa a ser a concatenação dessas tabelas, e a **lista continua única**.
- O schema ganha `icon`, `searchTerms`, `family` e `subfamily`. `editorComponentCatalog` deixa de existir como segunda lista, e o Inspector e o Add leem o schema.
- `ComponentCategory` (5 valores) é substituído por família e subfamília: Física › 3D › Juntas, UI › Layout etc.
- `findComponentSchema` passa a buscar por índice (`unordered_map` estático ou busca binária) porque serão mais de 200 tipos.
- **Aceite:** adicionar um tipo de teste exige apenas o arquivo do tipo e uma linha na tabela da família. Os testes de composição existentes continuam passando.

### 4.2 Tipos de propriedade que os próximos componentes exigem

O descritor ganha: `Integer`, `String` (com localização opcional), `Vector2`, `Vector4`, `Quaternion` (editado como Euler), `ColorRGBA` e `ColorHDR`, `LayerMask`, `Curve` (recurso inline), `Gradient`, `List<Struct>` com identidade por elemento, `ResourceList` e `EventConnection` (alvo + método/ação).

Justificativa por consumidor: partículas usam curvas, gradientes e módulos; UI usa strings, vec2, âncoras e cores RGBA; áudio usa curva de atenuação; Animator usa listas de estados e parâmetros; Cinemachine usa vec2 de composição; eventos usam conexões. Sem esses tipos, cada família improvisaria o próprio formato.

A serialização é por id persistente, com migração e preservação de payload desconhecido. `PropertyKind` (`component_reflection.h:31`) e `FieldKind` (`component_preset.h:28`) são ampliados, e **não duplicados**.

### 4.3 Receitas de criação como dados

```
CreationRecipe { id; family; name; description; icon;
                 components: [{ typeId, overrides: [{propertyId, value}] }];
                 children: [recipe-id | inline];      // ex.: Tela → Botão → Texto
                 requiresCapability; placement(ViewCenter|Selection|Root) }
```

- Uma única ação, `CreateFromRecipe(index)`, passa pelo `planComponentAddition` existente e grava **um único Undo**. Os 25 `EditorWidget::Create*` e o bloco `editor_session.cpp:3125-3212` saem.
- A disponibilidade é calculada pela capability de cada receita. `creationAlwaysAvailable` e o adaptador de água deixam de ser listas de exceção.
- Água, cubo e chão continuam com o recurso de biblioteca atual, declarado na receita.
- **Aceite:** as 25 receitas atuais produzem documentos idênticos aos de hoje (teste de igualdade no arquivo salvo), e uma receita nova é apenas uma linha de dados.

### 4.4 Add Component e menu de criação para 200+ itens

A especificação visual está em §6. Aqui fica apenas o contrato: busca incremental sobre nome, termos e família; lista virtualizada; seções “Sugeridos” (regras simples, por exemplo objeto com Malha e sem Colisor sugere Colisor) e “Recentes”; prévia da composição (“adiciona também: Câmera”); motivo de indisponibilidade sempre visível. **Não há mais paginação de 6 itens.**

### 4.5 Fachada C# gerada a partir do schema

- Uma ferramenta de build (`tools/`) lê as tabelas de schema, via exportação JSON do próprio binário de testes, e gera `managed/Astra.Scripting/Generated/Components.g.cs`. O resultado é um `readonly struct` por tipo, com propriedades tipadas que usam o acesso genérico por id já existente (`Component.GetFloat/SetFloat…`) e `GetComponent<T>()`.
- Os wrappers escritos à mão (Animation, Timer, CameraFollowRig, DeformableMesh, Materials) são mantidos onde houver operações que não são propriedades (tocar clipe, reiniciar timer).
- A ABI **não** ganha um ponteiro por tipo. Operações de família entram como `familyCommand(familyId, commandId, payload, size)` versionado por tamanho, seguindo o padrão de `ScriptRenderingSettings.size/schemaVersion`.
- **Aceite:** um teste C# lê e escreve pelo menos uma propriedade tipada de cada um dos 15 tipos pela fachada gerada, e a fachada é regenerada no build sem edição manual.

### 4.6 Ciclo de vida completo

Os callbacks passam a ser: `Awake`, `Enable`, `Start`, `FixedUpdate`, `Update`, `LateUpdate`, `Disable`, `Stop`/`Destroy`, `ApplicationPause(bool)`, `ApplicationFocus(bool)`, `ParentChanged`, `ChildrenChanged` e `BecameVisible/Invisible` (esses dois dependem do Notificador da Onda 1). A ordem é documentada e comparada com [U9](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html). Uma falha em um callback isola aquele comportamento, sem derrubar o Play.

### 4.7 Configurações do projeto

A workspace **Settings** existente ganha uma lista de seções à esquerda: Gráficos (atual), Camadas e colisão, Entrada, Tempo (passo fixo, escala), Física (gravidade, iterações), Tags. As duas últimas nascem como dados do projeto. A matriz e o mapa de entrada migram para cá (§2.3).

**Aceite da Onda 0:** 15 tipos e 25 receitas funcionando como hoje, com 6 workspaces de topo, Add sem paginação, fachada gerada, callbacks novos testados (ordem e isolamento) e captura no aparelho do Add, do menu de criação e de Configurações.

---

## 5. Catálogo alvo por família

Legenda: ✔ existe (pode precisar de ampliação) · **N** novo · *var* = variante nomeada no Add, dentro do mesmo tipo. As colunas “Cobre” trazem os tipos Unity cobertos, com os tipos Godot entre colchetes.

### A. Lógica e objeto — 8 tipos (Onda 1)

| Tipo Astra | Cobre |
|---|---|
| ✔ Comportamento | MonoBehaviour |
| ✔ Timer | [Timer] |
| N Conexões de evento | UnityEvent, EventTrigger (parte lógica), CinemachineBrainEvents/CameraEvents/CameraManagerEvents, SignalReceiver (parcial) [signals] |
| N Tween | [Tween] |
| N Marcador | [Marker3D] |
| N Notificador de visibilidade | OnBecameVisible [VisibleOnScreenNotifier3D] |
| N Ativador por visibilidade | [VisibleOnScreenEnabler3D] |
| N Transformação remota | [RemoteTransform3D] |

No objeto, e não em componentes: **tag, layer e flags estáticas** (GameObject da Unity).

### B. Restrições e rig — 16 (Onda 1: 6 · Onda 3: 10)

Mirar, Olhar para, Pai, Posição, Rotação e Escala (Aim/LookAt/Parent/Position/Rotation/ScaleConstraint; o suporte a várias fontes com peso também cobre `Multi*Constraint`, BlendConstraint e MultiReferential). Na Onda 3: Rig (Rig + RigBuilder + BoneRenderer como gizmo), IK de dois ossos, IK de cadeia, Transformação amortecida, Cadeia de torção, Correção de torção, Sobrescrever transformação, Referencial múltiplo, Anexo a osso [BoneAttachment3D] e Mola de ossos [SpringBoneSimulator3D].

### C. Renderização 3D — 20 (4 ✔ · Onda 1: 11 · Onda 2: 1 · Onda 5: 4)

✔ Malha (MeshFilter + MeshRenderer), ✔ Malha deformável, ✔ LOD Group, ✔ Ambiente (Volume, Skybox global).
Onda 1: Billboard, Linha (LineRenderer), Rastro (TrailRenderer), Decal (DecalProjector, Projector), Malha instanciada [MultiMeshInstance3D], Lens flare (LensFlare, LensFlareComponentSRP, FlareLayer), Skybox da câmera (Skybox), Área de oclusão (OcclusionArea), Portal de oclusão (OcclusionPortal), Grupo de ordenação (SortingGroup), Controle de streaming (StreamingController, sobre o texture streaming existente).
Onda 2: Texto 3D (TextMesh, TextMeshPro) [Label3D], que depende da fonte de runtime.
Onda 5: Volume de neblina (LocalVolumetricFog) [FogVolume], Forma CSG com *var* caixa, esfera, cilindro, toro, polígono e malha [CSG*], Grade 3D [GridMap], Sprite 3D [Sprite3D].

### D. Câmera — 34 (3 ✔ · Onda 3: 31)

✔ Câmera, ✔ Olhar (≈ PanTilt + InputAxisController), ✔ Acompanhar alvo (≈ CinemachineFollow).
Onda 3: Câmera virtual (CinemachineCamera, prioridade), Cérebro (Brain: blends), Órbita (OrbitalFollow e FreeLookModifier), Terceira pessoa, Mira em 3ª pessoa, Compositor de rotação, Compositor de posição, Mira rígida (HardLookAt), Travamento rígido, Dolly em spline, Carrinho em spline, Rolagem na spline (SplineRoll e SplineSmoother), Grupo de alvos, Enquadramento de grupo, Confinador 3D, Confinador 2D, Desoclusor, Descolisor, Fonte de impulso, Impulso por colisão, Ouvinte de impulso (inclui o External), Ruído (BasicMultiChannelPerlin), Câmera por estado, Tomada limpa (ClearShot e ShotQualityEvaluator), Sequenciador, Mistura, Zoom de seguimento, Deslocamento, Foco automático (depende de DoF no pós), Pixel perfect (PixelPerfectCamera, CinemachinePixelPerfect), Câmera livre de depuração (FreeCamera, CameraSwitcher).
Adaptação explícita: Storyboard, Recomposer, TriggerAction, VolumeSettings e PostProcessing viram propriedades da Câmera virtual ou ações do componente de conexões.

### E. Luz e GI — 7 (1 ✔ · Onda 1: 3 · Onda 5: 3)

✔ Luz, com *var* direcional, pontual, spot e **área** (Area Light). Onda 1: Sonda de reflexão (ReflectionProbe), Grupo de sondas de luz (LightProbeGroup), Volume de sondas (LightProbeProxyVolume, ProbeVolume). Onda 5: Ajuste de sondas (ProbeAdjustmentVolume), Âncora de luz (LightAnchor), Bake de GI [LightmapGI]. Sombras e controles aparecem **somente** onde há suporte efetivo no renderer (AGENTS §11).

### F. Física 3D (Jolt) — 17 (4 ✔ · Onda 1: 13)

✔ Corpo físico, ✔ Personagem (CharacterController), ✔ Colisor 3D, com *var* caixa, esfera, cápsula, malha, **cilindro, convexo, campo de altura** (TerrainCollider) e **plano**. ✔ Junta, com *var* ponto, dobradiça, deslizante e distância/mola (os 4 kinds de `joint.h:5`), mais **fixa, cone, swing-twist (CharacterJoint), 6DOF (ConfigurableJoint), caminho, engrenagem, cremalheira e polia**, todas com constraints nativas do Jolt.
Novos: Força constante (ConstantForce), Área de efeito (gravidade/vento/arrasto por volume) [Area3D], Raio [RayCast3D], Varredura de forma [ShapeCast3D], Braço de mola [SpringArm3D], Veículo e Roda (WheelCollider) [VehicleBody3D/VehicleWheel3D], Veículo de esteira (TrackedVehicle do Jolt), Corpo macio (Cloth) [SoftBody3D], Ragdoll (assistente Ragdoll) [PhysicalBoneSimulator3D], Osso físico [PhysicalBone3D], Superfície transportadora (esteira) [constant_linear_velocity], Flutuação (sobre `physics/water_buoyancy`).
**Adaptação explícita:** ArticulationBody vira cadeia de Juntas 6DOF com motores + Ragdoll, porque o Jolt não tem solver reduzido de Featherstone. Isso deve aparecer na ajuda do tipo.
Recurso, e não componente: Material físico (atrito, restituição, combinação), atribuído por Colisor.

### G. Física 2D (Box2D v3.1.1, já vendorizado) — 14 (Onda 4)

Corpo 2D (Rigidbody2D), Colisor 2D com *var* caixa, círculo, cápsula, polígono e borda/cadeia (Box/Circle/Capsule/Polygon/EdgeCollider2D, CustomCollider2D), Colisor composto 2D, Colisor de tilemap 2D, Junta 2D com *var* distância, fixa, atrito, dobradiça, relativa, deslizante, mola, alvo e roda (9 Joint2D), Efetor de área, Flutuação 2D, Plataforma 2D, Efetor de ponto, Efetor de superfície, Força constante 2D, Personagem 2D [CharacterBody2D], Raio 2D, Varredura 2D.
**Decisão:** Box2D e não Jolt restrito a um plano. É o backend da própria Unity e já está no repositório; o benchmark do item 4.1.6 deve ser citado ao abrir a onda.

### H. Mundo 2D — 16 (Onda 4)

Sprite (SpriteRenderer), Sprite animado [AnimatedSprite2D], Máscara de sprite (SpriteMask), Grade (Grid, GridInformation), Camada de tiles (Tilemap + TilemapRenderer) [TileMapLayer], Forma de sprite (SpriteShapeController, SpriteShapeRenderer, ObjectPlacement), Pele de sprite (SpriteSkin), Osso 2D [Bone2D], IK 2D com *var* CCD, FABRIK e membro (CCD/Fabrik/LimbSolver2D, IKManager2D), Biblioteca de sprites (SpriteLibrary, SpriteResolver), Luz 2D com *var* global, forma livre, sprite e spot (Light2D), Projetor de sombra 2D (ShadowCaster2D), Sombra composta 2D, Parallax [Parallax2D], Polígono 2D [Polygon2D], Linha 2D [Line2D].

### I. Animação — 4 (1 ✔ · Onda 3: 3)

✔ Animação (Animation). Novos: Animador (Animator: máquina de estados, blend trees, camadas, parâmetros, root motion, IK pass), Diretor de timeline (PlayableDirector), Receptor de sinais (SignalReceiver).

### J. Áudio — 9 (Onda 2)

Fonte de áudio (AudioSource, 2D/3D por mistura espacial) [AudioStreamPlayer2D/3D], Ouvinte (AudioListener), Zona de reverberação (AudioReverbZone), Passa-baixa, Passa-alta, Eco, Distorção, Chorus e Reverb (os 6 Audio*Filter). Mixer e buses são **recurso de projeto**, editado em Configurações › Áudio.

### K. Efeitos — 4 (Onda 5)

Partículas (ParticleSystem com módulos: emissão, forma, velocidade, cor/tamanho ao longo da vida, colisão, sub-emissores, texture sheet e trilhas), Partículas GPU (VisualEffect, VFXPropertyBinder) [GPUParticles3D], Campo de força de partículas (ParticleSystemForceField) [GPUParticlesAttractor], Colisor de partículas [GPUParticlesCollision].

### L. Terreno, vegetação e água — 7 (Onda 5)

Terreno (Terrain), Árvore (Tree), Zona de vento (WindZone), Espalhador de vegetação (detalhes e grama do Terrain) [MultiMesh]. **Água vira componente:** Superfície de água, Oceano e Rio, que hoje são tipos especiais de entidade criados por `createWaterSurface`. Os recursos e o renderer de água atuais são mantidos, e o que muda é o contrato (AGENTS §7: composição).

### M. Navegação — 6 (Onda 5)

Superfície de navegação (NavMeshSurface) [NavigationRegion3D], Agente (NavMeshAgent), Obstáculo (NavMeshObstacle), Link (NavMeshLink), Modificador (NavMeshModifier), Volume modificador (NavMeshModifierVolume). Backend candidato: Recast/Detour (zlib). É **dependência nova e exige sua aprovação**.

### N. Splines — 4 (Onda 5)

Spline (SplineContainer) [Path3D], Animar ao longo (SplineAnimate) [PathFollow3D], Extrusão (SplineExtrude) [CSGPolygon path], Instanciar ao longo (SplineInstantiate). O Rio passa a poder usar Spline como traçado.

### O. UI de jogo — 39 (Onda 2)

Tela (Canvas + CanvasScaler + GraphicRaycaster), Grupo de UI (CanvasGroup), Imagem (Image: simples, fatiada, preenchida radial/linear, lado a lado), Imagem bruta (RawImage), Texto (Text, TextMeshProUGUI, texto rico) [RichTextLabel], Botão, Alternador, Grupo de alternadores, Deslizante (Slider), Barra de rolagem, Área de rolagem (ScrollRect), Lista suspensa (Dropdown, TMP_Dropdown), Campo de texto (InputField, TMP_InputField, IME Android), Máscara, Máscara retangular (RectMask2D), Layout horizontal, vertical e em grade, Layout fluido [FlowContainer], Margem [MarginContainer], Centralizar [CenterContainer], Ajuste ao conteúdo (ContentSizeFitter), Proporção (AspectRatioFitter), Elemento de layout, Sistema de eventos (EventSystem, StandaloneInputModule, InputSystemUIInputModule, MultiplayerEventSystem), Gatilho de eventos (EventTrigger), Contorno (Outline), Sombra de UI (Shadow, PositionAsUV1), Barra de progresso [ProgressBar/TextureProgressBar], **Área segura** (notch/barra de gestos do Android), **Joystick virtual** (OnScreenStick), **Botão virtual** (OnScreenButton), Abas [TabContainer], Lista de itens virtualizada [ItemList], Seletor numérico [SpinBox], Painel modal [Popup], Visão de câmera (RenderTexture em UI) [SubViewport], Raycaster físico (PhysicsRaycaster: toque em objeto 3D), Raycaster 2D (Physics2DRaycaster).
O RectTransform é criado automaticamente pela Tela e pelos filhos, e não aparece no Add. **O runtime de UI não depende de `editor_screen`** (AGENTS §7); ele reaproveita apenas o desenho e o layout de baixo nível.

### P. Entrada — 3 (Onda 1: 1 · Onda 2: 2)

Entrada do jogador (PlayerInput: mapa e contexto ativos por objeto) na Onda 1. Gerenciador de jogadores (PlayerInputManager, multijogador local) e Cursor virtual (VirtualMouseInput) na Onda 2.

### Q. Vídeo — 1 (Onda 2)

Reprodutor de vídeo (VideoPlayer) sobre `AMediaCodec` do NDK, com saída para textura (material, UI ou Visão de câmera).

### R. Localização — 2 (Onda 2)

Texto localizado (LocalizeStringEvent, LocalizeStringListEvent, GameObjectLocalizer) e Recurso localizado (LocalizeAsset/Sprite/Texture/AudioClip/GameObjectEvent, LocalizedMonoBehaviour). As tabelas são recurso de projeto, editado em Configurações › Idiomas.

### Totais

| Família | Tipos | Variantes extras no Add | Onda |
|---|---:|---:|---|
| A Lógica | 8 | – | 1 |
| B Restrições/rig | 16 | – | 1, 3 |
| C Render 3D | 20 | +5 (CSG) | 1, 2, 5 |
| D Câmera | 34 | – | 3 |
| E Luz/GI | 7 | +3 (direcional, pontual, spot, área) | 1, 5 |
| F Física 3D | 17 | +4 colisor, +7 junta | 1 |
| G Física 2D | 14 | +4 colisor, +8 junta | 4 |
| H Mundo 2D | 16 | +2 IK, +3 luz 2D | 4 |
| I Animação | 4 | – | 3 |
| J Áudio | 9 | – | 2 |
| K Efeitos | 4 | – | 5 |
| L Terreno/água | 7 | – | 5 |
| M Navegação | 6 | – | 5 |
| N Splines | 4 | – | 5 |
| O UI | 39 | – | 2 |
| P Entrada | 3 | – | 1, 2 |
| Q Vídeo | 1 | – | 2 |
| R Localização | 2 | – | 2 |
| **Total** | **211** | **+43** (8 delas já existem nos dados) | |

Entradas no Add: 211 tipos + 43 variantes nomeadas = **254**. Oito dessas variantes já existem nos dados (3 tipos de luz, 4 formas de colisor e 4 kinds de junta) e hoje aparecem como uma entrada só.

---

## 6. Interface: como 211 tipos cabem no editor mobile sem virar lista de texto

Segue o `AGENTS.md` do editor: captura real → proposta visual gerada → ícones → implementação → nova captura. **A imagem gerada é hipótese e não comprova nada.**

### 6.1 Regra de workspaces

Ficam **6 workspaces de topo**: Cena, Recursos, Iluminação, Código, Play e Configurações. Uma workspace nova só é aceita quando edita um **recurso com tela própria**: grafo do Animador e Timeline, na Onda 3, dentro de uma workspace “Animação”. Tipo de componente, lista de configurações ou visualizador de estado **nunca** viram workspace.

### 6.2 Add Component (painel do Inspector)

- **Tablet/paisagem:** campo de busca no topo com foco automático; à esquerda, um trilho de famílias (ícone, nome e contagem); à direita, uma lista virtualizada de cartões. Cada cartão tem ícone exclusivo de 40 dp, nome, uma linha de descrição, selo “+ Câmera” quando traz dependência e motivo de bloqueio em tom de aviso. No topo da lista ficam “Sugeridos para este objeto” e “Recentes”.
- **Celular/retrato:** as famílias viram chips roláveis na horizontal, e a lista ocupa a altura toda.
- Tocar no cartão abre uma prévia da composição quando há dependências; caso contrário, adiciona direto e rola o Inspector até o componente novo.

### 6.3 Menu de criação (+ da Hierarquia)

Usa o mesmo componente visual do Add com dados de `CreationRecipe`: famílias à esquerda e **grade** de cartões grandes (ícone da receita + nome), porque aqui a pessoa escolhe “o que colocar na cena”. Pressionar e segurar mostra a composição (“Corpo dinâmico + Colisor esfera”). A receita é criada no centro da vista ou como filha da seleção, conforme `placement`.

### 6.4 Inspector

- Cabeçalho por componente: ícone exclusivo, nome, interruptor de ativo, menu com Redefinir, Copiar/Colar valores, Preset, Documentação (link Unity/Godot registrado) e Remover.
- Seções vêm de `PropertyPresentation.group`, são recolhíveis e têm “Avançado” fechado por padrão. Componentes com módulos (Partículas, Animador, Tela) usam seções com interruptor no título, como os módulos de ParticleSystem.
- Editores novos de propriedade: curva, gradiente, lista de estruturas reordenável, máscara de camadas, cor HDR e seletor de conexão de evento. São *pop-overs* ancorados ao campo, e não telas cheias.
- Em Play, o cabeçalho exibe o estado vivo de leitura (timer restante, velocidade do corpo, clipe tocando). Isso substitui a workspace Timers.

### 6.5 Ícones

- Um ícone exclusivo por tipo e por receita, **≈ 211 + ≈ 115 novos**, desenhados em SVG por família em `assets/astra-visual/icons/named/<família>/`.
- Os scripts por família (como `tools/generate-physics-recipe-icons.py`) são unificados em **um gerador** que rasteriza para o atlas `astra-ui-icons` e publica `catalog.json`.
- Linguagem visual única: silhueta do conceito em tom neutro e acento verde Astra apenas no elemento que diferencia a variante, como o raio na esfera sensora ou a seta na cinemática. Uma galeria de conferência lado a lado é gerada a cada família.

### 6.6 Gizmos no viewport

Todo tipo com extensão espacial precisa de gizmo, e isso entra no aceite do tipo: colisores em arame, alcance e cone de luz, frustum de câmera, raio de áudio, raio do agente de navegação, alças de spline, eixos e limites de junta, volume de efeito e contorno da área segura. Um tipo invisível no viewport não é “utilizável”.

### 6.7 Ferramentas de viewport, sem workspace

Pintura de tilemap, escultura e pintura de terreno, espalhamento de vegetação, edição de spline e edição de forma CSG são **modos da barra de ferramentas do viewport** com paleta no dock inferior. A edição da Tela de UI acontece no próprio viewport em modo 2D.

---

## 7. Ondas, acumulados e aceite

| Onda | Conteúdo | Tipos novos | Acumulado | Add acumulado | Receitas acumuladas |
|---|---|---:|---:|---:|---:|
| 0 | Fundação (§4) e correção da UI anterior | 0 | 15 | 23 (variantes existentes nomeadas) | 25 |
| 1 | Jogo 3D: física 3D, render 3D, sondas, luz de área, constraints, lógica, prefab e API de objetos | 40 | 55 | 76 | ≈ 62 |
| 2 | Áudio, UI de jogo, texto 3D, vídeo, localização e entrada local | 54 | 109 | 130 | ≈ 90 |
| 3 | Câmeras, Animador, Timeline e rig | 44 | 153 | 174 | ≈ 105 |
| 4 | 2D completo: Box2D, sprites, tiles, luz 2D e animação 2D | 30 | 183 | 221 | ≈ 128 |
| 5 | Efeitos, terreno/vegetação, água como componente, navegação, splines, CSG, neblina e GI | 28 | 211 | 254 | ≥ 140 |
| 6 | Condicional: rede, XR/AR, HDRP-like | – | – | – | – |

**Por que essa ordem:** a Onda 1 aproveita backends que já existem (Jolt, renderer, streaming), o que dá o maior ganho por esforço. A Onda 2 destrava o “jogo completo” no celular, que precisa de HUD, som e toque. A Onda 3 depende de splines para o dolly; a fatia mínima de Spline (container e amostragem) é antecipada para ela. A Onda 4 é isolada, com backend e renderer de sprites próprios. A Onda 5 reúne os sistemas pesados de conteúdo.

### Onda 1: itens de API que acompanham

- `Instantiate(prefab | objeto, pai, pose)` e `Destroy(delay)`. Exige **prefab básico**: recurso com identidade, instância com override por propriedade e apply/revert simples. Variantes e aninhamento ficam para depois.
- `Time` (delta, fixed, escala, sem escala, frame), `Random` (com semente), utilitários matemáticos (Lerp, SmoothDamp, MoveTowards), `Transform` (LookAt, Rotate, Translate, TransformPoint/Direction, local/mundo), tag/layer/`FindWithTag`/`FindObjectsByType`, `GetComponentInChildren/InParent`, `GetComponent<T>()`, `Debug` (Log, Warn, Error, DrawLine/DrawRay em Play), `Application` (pausa, foco, plataforma, FPS alvo), `Screen` (tamanho, área segura, orientação, DPI), `Scene` (carregar, aditivo, assíncrono), `Save` (chave-valor + arquivo), `Awaitable` (NextFrame, Seconds, FixedUpdate) no lugar de corrotinas e `Haptics.Vibrate`.
- Callbacks novos da Onda 1: `JointBreak`, `ControllerColliderHit`, `BecameVisible/Invisible` e os eventos da Área de efeito.

### Onda 2: API

`Audio.PlayOneShot`, controle de mixer, eventos de UI (clique, arraste, foco, submit), `Localization.Language` e o evento de troca de idioma.

### Onda 3: API

Parâmetros e estados do Animador, `AnimatorMove`/`AnimatorIK`, Timeline (tocar, pausar, avançar, sinais) e Câmera (`ScreenPointToRay`, `WorldToScreen`, prioridade e impulso).

### Onda 4: API

`Physics2D` (raycast, overlap, casts, camadas) e os callbacks `Collision2D`/`Trigger2D`/`JointBreak2D`.

### Onda 5: API

Partículas (Emit, Play/Stop, `ParticleCollision`/`ParticleTrigger`), navegação (`SetDestination`, caminho, `Warp`, áreas) e spline (avaliar posição e tangente).

### Definição de pronto por TIPO (aplicada em todas as ondas)

1. Referência Unity 6000.0 ou Godot 4.5 com link, registrada no schema (`helpUrl`).
2. Schema na tabela da família, com consumidor declarado e real: nenhum campo sem efeito, e controle sem suporte fica oculto ou recusado.
3. Propriedades persistidas por id, com default, unidade e faixa; migração quando alterar formato existente.
4. Inspector gerado, ícone exclusivo e gizmo quando espacial.
5. Pelo menos uma receita de criação quando o tipo faz sentido sozinho.
6. Fachada C# gerada e operações de família na ABI quando houver.
7. Mutabilidade em Play declarada e aplicada no ponto seguro.
8. Teste host do cenário mínimo: criar pela receita → editar → salvar/reabrir → Undo/Redo → Play com efeito observável no consumidor.

### Aceite por onda

- Contadores publicados (tipos, Add, receitas, cobertura Unity) com a lista do que é **implementado**, **parcial** e **pendente**, separados.
- Uma cena de validação da onda que combine pelo menos 5 tipos novos, sem código especial no núcleo.
- Captura no aparelho do Add, da criação e do Inspector dos tipos novos, com a validação no aparelho relatada separadamente da validação host.
- Build Android e testes host dos alvos afetados, sem ritual de rebuild total.

---

## 8. Riscos e limites

- **Volume de ícones e gizmos:** ≈ 330 ícones e ≈ 90 gizmos. O gerador único (§6.5) e gizmos por primitiva (caixa, esfera, cone, linha, arco) compostos por dados evitam 90 implementações à mão.
- **Tamanho de `editor_screen.cpp` (6.160 linhas) e `editor_session.cpp` (7.511):** o Add, o menu de criação e os editores de propriedade novos devem nascer em arquivos próprios do editor (`editor_add_component.cpp`, `editor_property_widgets.cpp`) por responsabilidade real, sem inchar esses dois arquivos.
- **Custo por frame:** com 200 tipos, o runtime não pode percorrer todos os schemas por objeto em cada frame. Cada família mantém a própria lista de instâncias vivas, atualizada por invalidação, como já fazem luzes e LOD.
- **`Components::MaximumCount = 64` por objeto** continua suficiente: uma Tela com 39 tipos distribui componentes entre filhos, não em um só objeto.
- **Paridade:** este plano **não** promete comportamento idêntico ao da Unity. Promete cobertura de capacidade com diferenças declaradas (ArticulationBody, eventos agrupados, TMP/legado unificados).

---

## 9. Decisões que dependem de você

| Decisão | Recomendação | Por quê |
|---|---|---|
| Backend de áudio (Onda 2) | **miniaudio** (domínio público/MIT-0) sobre AAudio | Arquivo único com decodificação, mixer e efeitos básicos; evita escrever DSP e decoders |
| Física 2D (Onda 4) | **Box2D v3.1.1**, já no repositório | Mesmo backend da Unity; o Jolt em plano não oferece juntas 2D nem efetores |
| Navegação (Onda 5) | **Recast/Detour** (zlib) | Padrão da indústria (Unity, Godot e Unreal derivam dele) |
| Texto de runtime (Onda 2) | FreeType + HarfBuzz com atlas SDF, ou ampliar o `ui_text` do editor se já fizer shaping | Precisa de IME, RTL e fallback; decidir após medir o `ui_text` existente |
| Vídeo (Onda 2) | `AMediaCodec` do NDK | Nativo, sem dependência |
| Onda 6 (rede, XR) | Fora da meta até as Ondas 1–5 fecharem | Não é pré-requisito dos outros pacotes |
