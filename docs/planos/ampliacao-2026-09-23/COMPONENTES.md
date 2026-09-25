# Catálogo de capacidades e composição

Esta é a especificação de escopo da Astra; nomes novos abaixo são **alvos propostos**, não APIs disponíveis. As fichas de todos os tipos externos do recorte estão no [atlas](ATLAS.html), incluindo propriedades herdadas, métodos, sinais e origem. O inventário detalhado Unity anterior continua em [CATALOGO-UNITY.md](../../componentes/pesquisa-2026-09-15/CATALOGO-UNITY.md). Os valores atuais Astra pertencem à [matriz gerada](../../componentes/MATRIZ-PROPRIEDADES.md) e aos descritores, não a uma tabela manual duplicada.

## Regra de leitura

Cada linha define uma capacidade a entregar, o conjunto de propriedades/operações que deve receber contrato e suas relações. Quando uma linha agrupa shapes/widgets, o mecanismo comum é compartilhado, mas as propriedades específicas de cada subtipo são obrigatórias. A ficha externa individual no atlas complementa estas linhas; copiar a lista inteira sem escolher a semântica Astra não constitui implementação.

**C** = componente de objeto; **R** = recurso; **S** = sistema/serviço de mundo/projeto; **E** = ferramenta de autoria; **B** = base de identidade/hierarquia. “Ampliar” significa base localizada; “planejar integração” significa caminho universal não demonstrado no escopo inspecionado. Não equivale a afirmar que nenhum código semelhante existe em outro módulo.

Uma referência a recurso pode ser vazia enquanto o objeto está sendo autorado. O editor distingue “válido como documento incompleto” de “pronto para executar/renderizar”. Sem recurso necessário, informa ausência e não simula sucesso. Dependência de renderização não implica dependência de física, e vice-versa.

## Identidade, cenas, composição e execução — P01–P06

| Capacidade Astra / referência | Propriedades e operações a cobrir | Relações e restrições | Trabalho |
|---|---|---|---|
| B Objeto / Unity GameObject, Godot Node | ID, nome, ativo próprio/efetivo, pai, ordem, grupos, layer; criar/destruir/duplicar/consultar | Mundo+geração; nome não é identidade; ciclo rejeitado; filhos e referências remapeados em duplicação | Ampliar `GameObject`/SceneGraph |
| B Transform 3D / Transform, Node3D | Posição/rotação/escala local, pose mundial, pivô/convenções, reparent com preservar local ou mundo, conversão de ponto/vetor/direção | Um escritor de pose por fase; shear e escala negativa têm política; invalidar descendentes/bounds | Consolidar e documentar |
| B Transform 2D / RectTransform/Transform, Node2D | Posição, ângulo, escala, skew/pivô se suportado, z/sorting, local/mundo | Não misturar ângulo em radianos/graus; 2D não herda automaticamente física 3D | P12 |
| R/S Cena / Scene, PackedScene/SceneTree | Recurso, raiz, cenas aditivas, load/unload, progresso, cancelamento, cena ativa | Dependências de assets; publication atômica; IDs persistentes versus handles de execução | P05 |
| R Prefab/subcena / Prefab, PackedScene | Raiz e filhos, overrides por PropertyId, elementos adicionados/removidos, variantes, apply/revert, instanciar | Grafo acíclico de dependências; contexto de edição; propagação não apaga overrides | P05 |
| R Receita/preset | Componentes iniciais, defaults, requisitos, referências externas, remapeamento | Cria composição via resolvedor; não acrescenta classes especiais por demo | Ampliar presets/recipes |
| C Comportamento / MonoBehaviour, script em Node | Fonte/tipo/instância, ativação, propriedades expostas, ordem, callbacks, erros | Compilação + mundo; múltiplas instâncias por contrato; referência desconhecida preservada | Ampliar `Astra.Behavior` |
| S Eventos/conexões / UnityEvent, signals | Assinatura, emissor/receptor, método/ação, payload, prioridade, conexão/desconexão, sinal único | Tipagem, lifetime, reentrância, ownership e despacho em thread definida | P06 |
| C/S Timer / timers de gameplay, Timer | Intervalo, repetição, autostart, pause, tempo escalado, remaining read-only | Relógio de mundo; cancelamento em Stop/destruição | P06 |
| R/S Tween / animação de propriedades, Tween | Alvo, PropertyId, origem/destino, duração, easing, sequência/paralelo, loop | Propriedade elegível; cancelamento e concorrência com física/animação definidos | P06 |
| S Grupos/tags/consultas | Membership múltipla, consulta, escopo de mundo, layers separados | Grupos de organização não viram máscara física automaticamente | P06 |

**Adaptação explícita:** um Node Godot pode reunir função e posição na árvore; na Astra essas funções podem ser componentes no mesmo objeto ou objetos filhos, conforme pose/ownership. O autor poderá criar uma composição pronta sem que a engine passe a executar SceneTree Godot. `Transform` já faz parte do objeto; não é preciso adicioná-lo como dependência redundante na UI.

## Recursos, geometria, materiais e câmeras — P05/P08

| Capacidade | Propriedades/operações | Dependências | Trabalho |
|---|---|---|---|
| R Mesh / Mesh, ArrayMesh | Vértices/índices, atributos, topologia, submeshes, bounds, derivados, flags de leitura | Importador/cooker, buffers RHI; preservação de fonte e GUID | Ampliar caminho atual |
| C MeshRenderer / MeshFilter+MeshRenderer, MeshInstance3D | Mesh, slots de material, enabled, shadows, visibility/layers, sorting, bounds override | Mesh renderizável, material compatível; colisor independente | Ampliar `astra.render.mesh` |
| C SkinnedMesh / SkinnedMeshRenderer, MeshInstance3D+Skin+Skeleton3D | Skin, root/bones, bind poses, pesos, quality, bounds, morph weights | Mesh/skin, alvos válidos e backend de deformação; limite reportado | Base localizada; P11 |
| C InstancedRenderer / instancing Unity, MultiMeshInstance3D | Recurso mesh, transforms/cores por instância, custom data, bounds, culling | Buffers/instâncias; compartilhamento sem alias indevido | Integrar autoria e API |
| C LODGroup / LODGroup, visibility ranges | Níveis, renderers, limiares, fade, referência de tamanho, culling | Objetos descendentes/alvos válidos; recursos e política por vista | Ampliar existente |
| R Shader | Parâmetros tipados, passes, variantes, keywords/perfil, tipos de textura e bindings | Compilação/reflexão, pipeline RHI; mensagens de erro apontam fonte | P08a |
| R Material / Material, ShaderMaterial | Shader, parâmetros escalares/vetores/cores/texturas, blend/cull/depth conforme shader | Shader + recursos; material compartilhado versus override de instância | Ampliar sem limitar ao PBR fixo |
| R Texture / Texture, Texture2D | Fonte, espaço de cor, formato, dimensões, mipmaps, compressão, streaming | Decode/cooker/residência; importação distinta de edição de sampler | Ampliar contratos existentes |
| R Sampler/binding | Wrap U/V/W, filtro min/mag/mip, anisotropia, UV set/transform por binding | Imagem/view/sampler separados; capacidade de aparelho explícita | Consolidar |
| R RenderTarget / RenderTexture, ViewportTexture | Tamanho/escala, cor/depth, formato, MSAA, lifetime e resize | View/render graph; não pode ler/escrever recurso em conflito | P08b |
| C Camera / Camera, Camera3D | Perspective/ortho, FOV/tamanho, near/far, aspect/viewport, priority/main, mask, target, clear, exposure | Transform completo, view; câmera editor separada; roll preservado | Ampliar `astra.camera` |
| C CameraRig / Cinemachine, composição Camera3D+SpringArm3D | Follow/look-at, offset, damping, limites, composição, colisão, blend | Camera + targets; queries opcionais para obstruction | P08b/P15 |
| C Decal / URP DecalProjector, Decal | Material, box size, normal/angle fade, distância, mask, ordem | Passe/backend decal, render layers; sem equivalência a mesh decal automática | P08c |
| C Line/Trail / LineRenderer/TrailRenderer, Line2D | Pontos, largura por curva, material, cor/gradient, duração, caps/joins, espaço | Recurso curva/material, renderer de linha; dimensionalidade explícita | P14 |

**Adaptação explícita:** Unity separa MeshFilter/MeshRenderer; Astra já concentra referência de geometria no seu componente de malha. Godot associa Skin ao MeshInstance e esqueleto por relação de nodes; Astra deve preservar alvos de ossos por identidade sem impor a mesma árvore de classes.

## Luz, ambiente e volumes — P08/P16

| Capacidade | Propriedades/operações | Dependências | Trabalho |
|---|---|---|---|
| C Light / Light; DirectionalLight3D, OmniLight3D, SpotLight3D | Tipo, cor/temperatura, unidade/intensidade, alcance, cone, layers, enable | Renderer/light cluster, pose; unidade por tipo e conversões documentadas | Ampliar existente |
| Configuração de sombra | Cast, bias/normal bias, resolution/budget, distance, cascades/faces, filtering | Shadow passes reais, atlas, backend/aparelho | Manter capacidade por tipo |
| C/R Ambiente / Volume, WorldEnvironment | Sky, exposure, tone map, ambient, fog, effects/profile, prioridade, blend | Perfis tipados, consumidor de render; global versus local | Ampliar existente |
| R Céu/atmosfera | HDRI, orientação, sol, intensidade, turbidez/parâmetros físicos pertinentes | Atmosfera/env map existentes; iluminação e fundo separados | Consolidar |
| C ReflectionProbe | Shape/extents, offset, capture/baked data, resolution, layers, refresh, blend | Captura, cubemap/atlas e seleção por instância | P08d, capability hoje planejada |
| C Irradiance/LightProbe volume | Distribuição, bake data, bounds, interpolation, atualização | Bake e amostragem indireta real | P08d |
| R Lightmap/bake settings | UV set, texel density, charts, atlas, output, quality, cancel/progress | Geometria/material/luz estáticos, bake job e renderer | P08d |
| C Fog volume | Forma, densidade, albedo/emissão, anisotropia se implementada, falloff | Integração volumétrica; neblina simples não equivale a volume | Fatia própria em P08 |
| C/R WaterBody/WaterWorld | Forma/limites, material, ondas, corrente, profundidade, buoyancy/queries | Sistema de água já existente, renderer e física sincronizados | Consolidar contratos universais em P16 |

**Pendente:** o registro atual marca cookies/luz de área/probes/lightmaps como planejados. Suas telas só entram como controles operantes junto aos consumidores. Configuração preservada mas indisponível recebe motivo verificável. URP 17.0.4, HDRP 17.0.4 e Built-in são referências diferentes no inventário Unity, não um pipeline único da Astra.

## Física e personagem — P07/P12

| Capacidade | Propriedades/operações | Dependências e conflitos | Trabalho |
|---|---|---|---|
| C Corpo 3D / Rigidbody, RigidBody3D/StaticBody3D/AnimatableBody3D | Tipo de movimento, massa/densidade se suportada, gravidade, damping, COM/inércia, velocities, forças/impulsos/torques, sleep, CCD/interpolation, locks | Mundo Jolt, shapes pertinentes; modo determina escritor da pose | Ampliar corpo existente |
| C Collider box/sphere/capsule | Tamanho/raio/altura, centro/rotação local, enabled, sensor, material, layers/masks | Forma independente da mesh; corpo estático implícito ou relação explícita definida | Ampliar Collider |
| C Collider convex/mesh/heightfield | Fonte geométrica própria, convex/decomposition, cooking, scale, margin | Cooked data; restrições de corpo dinâmico do backend; sem fallback silencioso | Consolidar |
| C Composição de colisores | Lista/filhos shapes, IDs, pose e material por shape | Agregação em corpo proprietário; remover filho invalida composição | P07a |
| R PhysicsMaterial | Friction, restitution, combine policy, surface data | Propriedade suportada no backend; compartilhado/override explícito | P07a |
| C Sensor/Area | Shape, monitoring, monitorable, layers, enter/stay/exit | Consulta de contatos; referência ao objeto/shape; sem obrigar renderer | P07c |
| C Joint fixo/hinge/slider/spring/6DOF | Corpos A/B, frames/anchors, eixos, limits, motor, força/torque, damping, break threshold | Pelo menos corpo proprietário; segundo corpo ou world anchor; validação de frames | Ampliar Junta, sem flags fictícias |
| C CharacterMotor / CharacterController, CharacterBody3D | Shape, speed, slope, step, skin margin, gravity, floor snap, platform velocity, up direction, jump | Backend CharacterVirtual; conflicts atuais com Body/Collider devem evoluir com migração | Ampliar Character |
| S Physics queries | Ray/shape casts, overlaps, filters, triggers, max distance, initial overlap | Mundo, alocação/buffer e truncamento explícitos; hits com IDs válidos | Ampliar APIs atuais |
| C/R Ragdoll | Corpos/joints por osso, blend, limites, transição animated/dynamic | Skeleton + physics; uma autoridade final por osso | P07d/P11 |
| C/S Physics2D | Bodies, circle/box/capsule/polygon/edge, sensors, joints, character e queries | Backend escolhido, mundo 2D, unidade; não colidir implicitamente com 3D | P12 |

No Godot, `CollisionShape3D` deve servir a um `CollisionObject3D`; na Astra um shape pode ser dado do componente Collider ou parte de uma composição. Essa relação não é herança e não deve virar um `RequireComponent` cego que acrescente Rigidbody a todo sensor/colisor. Camadas de detecção, resposta física e visibilidade são domínios distintos.

## Input, UI de jogo, texto e áudio — P06/P09/P10

| Capacidade | Propriedades/operações | Dependências | Trabalho |
|---|---|---|---|
| R/S ActionMap/InputContext | Actions, bindings, interactions, deadzones, axes, device groups, enabled, rebind | Entrada plataforma + foco; contexto de editor/Play separado | Ampliar input existente |
| C/R Canvas/root UI | Screen/world space, escala, viewport/target, sorting, safe area | Runtime UI, transform/layout, renderer | P09 |
| C Rect/layout | Anchors, offsets, pivot, min/preferred/max, stretch, alignment, aspect | Parent layout, política de medida/arranjo; não expor valores sem efeito | P09 |
| C Containers row/column/grid/flow | Gap, padding, alignment, wrap, sizing | Filhos UI; layout controla posição, não script concorrente | P09 |
| C Image/NineSlice | Texture/region, tint, borders, fill/mode, aspect | Texture e sampler; atlas/pivô | P09 |
| C Text/RichText | String/localization key, font, size, wrap, alignment, spans, ellipsis, selection | Font/shaping/fallback; rich text com marcação definida | P09 |
| C Button/Toggle/Radio | Text/icon, interactable, pressed/checked, group, transitions, click/change | Foco e input, tema; Radio precisa de grupo explícito | P09 |
| C Slider/Range/Progress | Min/max/value/step, orientation, format, change event | Validação de domínio; Progress pode ser read-only na entrada | P09 |
| C Scroll/List/Tree/Dropdown | Scroll, selection, item source, virtualization, expansion, clipping | Layout, input/capture e item IDs; dado separado do widget | P09 |
| C TextInput | Value, placeholder, multiline, selection/caret, validation, keyboard type, submit | IME/clipboard/foco Android; dado sensível não entra em logs | P09 |
| R Theme/Style | Tokens, estados, fonts, colors, sizes, variants | Herança de recurso; alteração invalida layout/draw conforme efeito | P09 |
| S Accessibility/Localization | Nome/role/state, foco/ordem, locale, fontes substitutas, RTL, contraste | Semântica de UI e plataforma; leitura de tela não depende só de cor | P09/P18 |
| R AudioClip/Stream | Fonte, formato, sample rate/channels, duração, import/streaming, loop points | Decode, cache e threads de áudio; GUID durável | P10 |
| C AudioSource 2D/3D | Clip/stream, bus, volume, pitch, loop, autoplay, priority, spatial blend/range/cone | Audio world; Transform para 3D; não exigir renderer | P10 |
| C AudioListener | Enabled, priority, orientação, seleção de listener de jogo | Audio world; câmera é relação opcional | P10 |
| R/S Mixer/buses/effects | Gain, mute/solo, sends, effects/params, snapshots | Grafo sem ciclos não suportados; DSP real; lifecycle da plataforma | P10 |

Cada widget recebe estados normal/hover/focus/pressed/disabled quando pertinentes, navegação, nome acessível e eventos tipados. Isso integra a união de `Control`/containers do Godot com autoria de componentes; não obriga reproduzir uGUI e UI Toolkit como dois sistemas Astra distintos.

## Animação, navegação, 2D, efeitos e mundo — P11–P16

| Capacidade | Propriedades/operações | Dependências | Trabalho |
|---|---|---|---|
| R AnimationClip | Tracks, target IDs, keyframes, interpolation, duration, loop, events | Recursos importados/autorais; curvas e propriedades animáveis | Ampliar atual |
| C AnimationPlayer | Clips, default, autoplay, speed, play/stop/seek/blend e estado transitório | Clips/alvos; não persistir tempo de Play por acidente | Ampliar atual |
| R Animator graph | States, transitions, conditions, parameters, blend spaces, layers, masks | Clip evaluator; árvore/grafo validado; máquina de estados salva em resource | P11b |
| C/R Rig/IK/constraints | Targets, chains, poles, limits, weights, solver order | Skeleton; targets válidos; controle de pose por fase | P11c |
| R/C Timeline | Tracks, bindings, clips, markers, ranges, preview/record | Tempo, animation/audio/camera/events; update determinístico do editor | P11d |
| C NavigationRegion/Surface | Geometry source, bounds, layers, cell size, radius/height/step/slope, bake data | Bake/cooker + recurso navmesh | P13 |
| C NavigationAgent | Destination, speed/accel, radius, avoidance, path state, stop distance, masks | Nav world + motor de movimento; não exigir Rigidbody automaticamente | P13 |
| C NavigationObstacle/Link | Shape/radius/velocity; start/end, cost, direction, enabled | Nav world; dynamic obstacle não equivale a rebake global | P13 |
| C Perception/state logic | Sensors/queries, memory, state/action graph, timers, events | Capacidade composta de gameplay; sem `EnemyAI` especial no núcleo | P13/P17 |
| C Sprite/AnimatedSprite | Texture/frames, region, pivot, tint, flip, sorting, fps/playback | Atlas/material 2D; frame IDs duráveis | P12 |
| R TileSet / C TileMapLayer | Tiles, atlas, cells, terrain/autotile, metadata, collisions, nav, animation | Painting, chunk bounds, física/nav 2D opcionais | P12 |
| C Camera2D/Parallax | Zoom, offset, limits, smoothing, follow, repeats | Transform2D e view; game camera independente do editor | P12 |
| C Light2D/Occluder2D | Type, color/energy, texture, masks, shape, shadow params | Renderer 2D e occlusion; não reusar sombras 3D semanticamente | P12 |
| C/R ParticleEmitter | Rate/bursts, shape, lifetime, seed, local/world, velocity, color/size curves, collision | Simulation CPU/GPU declarada, material e bounds | P14 |
| R Curve/Gradient | Keys/stops, interpolation, tangent/mode, extrapolation, colorspace | Identidade de key se referenciada; edição agrupada em Undo | P01/P15 |
| R Spline / C Path | Points, tangents, closed, up/roll, sampling, length | Geometry resource e ferramentas de edição | P15 |
| C PathFollower | Path ref, offset/distance, speed, orientation, loop | Path + Transform; física tem autoridade se usada | P15 |
| C Terrain | Heightfield/tiles, size/resolution, material layers, holes, normals, collision | Streaming/cooking, mesh/render e física; brush transacional | P16 |
| C Foliage/scatter | Source meshes, seed, density, masks, slope/height, scale/rotation range, LOD | Instancing e terreno/surface; instâncias derivadas versus autorais | P16 |
| S World streaming | Regions, budgets, load radius/priorities, async progress, rebasing policy | Scenes/assets, handle invalidation, GPU/CPU lifetime | P05/P16/P18 |

## Projeto, ferramentas e módulos condicionais — P17–P20

| Capacidade | Propriedades/operações | Dependências | Resultado esperado |
|---|---|---|---|
| S SaveGame | Schema/version, slot, timestamp, data/ref mapping, migration | Storage e dados de gameplay, não cópia do EditorDocument | Salvar/carregar com erros explícitos |
| E Import profiles | Scale/axes, geometry, material/textures, animation, collisions, exclusions | Importador, fonte, reconciliação e jobs | Reimportação previsível sem perder edição |
| E Inspector extensions | Widget por tipo, group, conditions, custom renderer validado | Schema/commands/Undo, não mutação direta | Novas propriedades sem painel monolítico |
| E Code/graph workspace | Diagnostics, help, completion, graph nodes/ports, debug | Compiler/Flow/API/metadata reais | C# e visual convergem no mesmo contrato |
| E Profiler/debug overlays | Frame/sample, CPU/GPU/mem, draw/physics/navigation overlays | Profiler e medições reais, ativação explícita | Encontrar custo e relações, sem logs por frame |
| S Build/export profiles | Startup scene, target, assets, quality/input/physics, modules | Cooker/runtime/bootstrap; assinatura separada | Executável de jogo independente |
| C/S Network identity/replication | Authority, ID, ownership, properties, RPC policy, spawn, interpolation | Transport + serialization + scenes + time | Dois peers e reconexão verificados |
| C XR origin/controller/AR data | Tracking origin, poses/actions, anchors/planes opcionais | Runtime e hardware específicos | Capacidade condicional honesta |
| C VideoPlayer | Source, loop, speed, seek, audio/target, aspect | Decode/plataforma, texture e áudio | Reprodução e lifecycle reais |
| C Vehicle/Cloth/SoftBody | Parâmetros de solver e autoria pertinentes à família | Física, mesh/skin, orçamento e backend | Extensões completas, sem falsa paridade |

## Exemplos de composição que o desenho deve suportar

| Objetivo de autor | Composição | Dependência que NÃO deve ser imposta |
|---|---|---|
| Zona invisível que inicia música | Objeto + sensor + conexão/evento; áudio em objeto independente | MeshRenderer |
| Porta articulada importada | Prefab com pivô correto + mesh + shapes + body/joint + comportamento | Nome `Door`, mesh primitiva específica ou lógica no renderer |
| Personagem com câmera e HUD | Root/motor; filho visual skin/animator; rig/camera; Canvas e actions | HUD dentro do corpo físico ou câmera substituindo navegação do editor |
| Plataforma 2D animada | Sprite + body cinemático 2D + shape + animation/tween sob política de pose | Shader 3D ou rigidbody 3D |
| Placa de vídeo no cenário | Mesh/material com RenderTarget + player de vídeo | Nova classe de renderer exclusiva para placa |
| Estrada, trilho ou trajetória de câmera | Mesmo Spline resource; extrusão, follower ou camera rig diferentes | Spline embutida em `RoadManager` |

## Ficha obrigatória de implementação por membro

Para transformar qualquer item deste catálogo em trabalho implementável: fixar TypeId/PropertyId e versão, tipo/padrão/unidade/faixa, enum completo, read/write, referências/cardinalidade, serialização, mutabilidade em edição/Play, backend/capability, invalidação, erro, editor, API e aceitação observável. Valores ainda não escolhidos são decisão do pacote, nunca zero presumido. O atlas conserva o default externo quando presente; ele **não** determina o default Astra automaticamente.

O inventário de XML não detecta todos os avisos de configuração Godot; a extração Unity não resolve toda condição de compilação. Requisitos de pai, recursos obrigatórios e restrições por plataforma devem ser conferidos na ficha oficial de cada tipo antes de promover a capacidade. O plano inclui esse trabalho na entrega de cada pacote, sem vender a extração automática como análise semântica completa.
