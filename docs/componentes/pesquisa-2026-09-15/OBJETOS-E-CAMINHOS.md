# Objetos, composições e caminhos de uso

**Referência: Unity 6000.0 e versões fixadas no atlas · 15/09/2026.**  
Este documento distingue objeto, componente, recurso, serviço e representação editorial. Os nomes da coluna Unity pertencem à referência. As receitas Astra são propostas: não afirmam que seus componentes/consumidores já estejam disponíveis.

## 1. Como ler as receitas

Um GameObject é o recipiente da composição; Transform ou RectTransform define sua relação espacial. Mesh é recurso de geometria; MeshRenderer consome geometria/materiais; Material referencia shader e dados; Texture é outro recurso. Nenhum desses nomes é intercambiável.

As tabelas abaixo descrevem **composição funcional**, não reproduzem todos os componentes adicionados automaticamente por cada menu/pacote/configuração da Unity. O comportamento literal de um menu deve ser confirmado na fonte fixada. O inventário `menus-gameobject.json` registra **78 comandos literais** encontrados, incluindo comandos que operam em objetos existentes. Não é inventário integral de todos os menus nativos, menus montados por constantes ou extensões instaláveis.

O caminho comum na Astra será: **Add → Objeto / Componente / Recurso / Importar → configurar referências → editar propriedades → resolver requisitos → salvar → executar**. Uma receita pode criar uma subárvore, mas todos os itens criados ficam visíveis e editáveis. Não haverá objeto privilegiado “Carro” na arquitetura; um veículo será uma composição do usuário.

Fontes gerais: [GameObject](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.html), [primitivas](https://docs.unity3d.com/6000.0/Documentation/Manual/PrimitiveObjects.html), fontes de cada classe no [atlas pesquisável](ATLAS-UNITY.html), [registros de menus](menus-gameobject.json).

## 2. Entidades e geometria de base

| Objeto / receita | Composição e recursos | Propriedades / passos | Consumidores e dependentes |
|---|---|---|---|
| Vazio | GameObject + Transform | Nome, ativo, camada/tag quando implementadas, pai, local/world | Scripts, organização, pontos, qualquer composição futura |
| Filho vazio | Vazio sob a seleção | Criar ID; escolher pose local; referências preservadas | Hierarquia, animação e composição |
| Pai vazio / grupo | Vazio + reparent transacional | Preservar world/local explicitamente; tratar shear | Todos os filhos e seus usuários |
| Cubo | MeshFilter/MeshRenderer + malha cubo; collider apropriado se a receita o incluir | Dimensões/escala, material, forma física independente | Renderização, picking, física opcional |
| Esfera | Renderer + malha esfera; SphereCollider quando desejado | Raio geométrico/físico, material, tesselação do derivado | Visual, física, volumes |
| Cápsula | Renderer + malha cápsula; CapsuleCollider quando desejado | Altura, raio, eixo, material | Placeholder, física, personagem por composição |
| Cilindro | Renderer + malha cilindro | Altura, raio e material; colisão exige decisão separada | Visual; Unity não possui collider cilíndrico primitivo no padrão descrito |
| Plano | Renderer + malha no plano XZ | Extensão, subdivisão quando gerada, UV, normal | Chão, superfície, física opcional |
| Quad | Renderer + malha no plano XY | Tamanho, material, dupla face quando necessária | Tela, sprite 3D, vídeo, billboard |
| Modelo importado | Fonte → IR → objetos/nós → subassets mesh/material/texture | Selecionar cena/nós, perfil, pivôs e overrides | Instâncias, reimportação, renderer e animação |
| Instância reutilizável | Asset de cena/prefab + overrides | Instanciar, editar local, aplicar/reverter por propriedade | Outros prefabs, referências externas e exportação |
| LOD composto | LODGroup + conjuntos de Renderer | Referências dos níveis, thresholds/transição, bounds | Culling por vista e budget |
| Malha deformável | SkinnedMeshRenderer + Mesh + ossos/bind poses | Root bone, bones, pesos, blendshapes, bounds | Animator, rig, sombras e picking deformado |
| Ponto/socket | Transform identificado ou subasset de conexão | Posição/rotação local, vínculo ao recurso/nó, nome estável | Ferramentas, equipamentos, emissores, câmera |

A Astra deve permitir a mesma malha em vários renderers e vários objetos compartilhando material. “Criar variante” duplica a identidade de recurso intencionalmente; editar um usuário não deve duplicar tudo automaticamente.

## 3. Câmeras, iluminação e volumes

| Objeto / receita | Composição funcional | Configuração | Dependentes |
|---|---|---|---|
| Câmera | Transform + Camera; AudioListener é capacidade adicional, não requisito para imagem | Projeção, near/far, pose completa, saída, prioridade | RenderView, preview, picking espacial e efeitos |
| Câmera com alvo | Camera + comportamento de pose + referência de alvo | Offset, damping, eixo de mira, política de colisão opcional | Input/física se usados; preview e saída |
| Câmera em caminho | Camera + consumidor de spline | Percurso, orientação, velocidade/curva | Spline, relógio e sequenciador |
| Câmera procedural Cinemachine | Componentes do pacote + câmera de saída/controlador | Alvos, prioridades, composição e transições | Brain/controlador e estágios de atualização; não importar pacote como se fosse runtime Astra |
| Luz direcional | Transform + Light em modo direcional | Direção, cor/intensidade; sombras conforme backend | Iluminação, sombras, ambiente |
| Luz pontual | Transform + Light pontual | Alcance, cor/intensidade; gizmo de volume | Culling de luzes, materiais, sombra pontual futura |
| Luz spot | Transform + Light spot | Ângulos, alcance, direção; cone editorial | Iluminação, shadow map e orçamento |
| Luz de área | Light/tipo/extensão apropriada ao pipeline | Forma, dimensões, unidade e modo | Backend de área/bake; não equiparar automaticamente a uma luz pontual |
| Reflection Probe | Transform + ReflectionProbe | Volume, resolução, captura, refresh, cubemap | Materiais/reflexão, scheduler e recursos |
| Light Probe Group | Grupo de posições de amostragem | Editar pontos e bake conforme sistema | Iluminação de objetos móveis, interpolação |
| Light Probe Proxy Volume | Componente e região de amostragem | Bounds, resolução e vínculo de renderer | Sistema GI compatível; não substitui toda tecnologia de probes |
| Skybox | Componente/recurso de céu conforme pipeline | Material/ambiente por câmera ou projeto | Fundo, iluminação de ambiente e reflexão |
| Volume global | Volume + VolumeProfile | Peso/prioridade e parâmetros sobrescritos | Avaliação por câmera e passes suportados |
| Volume local | Volume + perfil + região adequada | Shape, blend distance e prioridades | Sistema de volumes, localização da câmera |
| Decal | Projetor/decal do pipeline | Material, volume, máscaras e fade | Pass de decals e dados de superfície |
| Lens flare | Componente/recurso de flare do pipeline | Elementos, escala, cor, oclusão | Projeção por câmera e efeitos |
| Região de oclusão | OcclusionArea/Portal ou contrato equivalente | Volumes/estado e bake | Culling, cena e dados derivados |

Cada um desses objetos precisa de representação editorial própria: símbolo, contorno/alcance, handles e seleção. A região visível no editor é uma projeção do contrato; não precisa criar meshes físicas ou sombras no jogo.

## 4. Física, mecanismos e navegação

| Objeto / receita | Composição | Passos | Dependentes |
|---|---|---|---|
| Obstáculo estático | Transform + Collider(s) | Escolher forma, dimensões, material e filtro | Mundo físico, queries e contato; Rigidbody dinâmico não é obrigatório |
| Corpo dinâmico | Transform + Rigidbody + forma adequada | Massa, centro de massa, damping, gravidade, CCD/interpolação conforme backend | Solver, contatos, joints e autoridade da pose |
| Corpo cinemático | Corpo configurado para controle externo + shapes | Movimento por comandos/fase correta | Contatos e scripts; não competir com integração dinâmica |
| Trigger | Collider em modo sensor com composição de corpo conforme regra do solver | Máscaras, callbacks e estado | Eventos de entrada/saída; regras variam por backend |
| Personagem | CharacterController ou composição de corpo/shape/input | Altura, raio, degrau, inclinação, movimento | Queries, solver, input e câmera |
| Articulação | Hierarquia de ArticulationBody | Tipo de junta, anchors, eixos, drives e limites | Solver de articulações; não simples sequência de transforms |
| Dobradiça | Corpos + HingeJoint | Connected body, anchor, eixo, limites/motor | Solver e poses dos corpos |
| Junta fixa | Corpos + FixedJoint | Vínculos, anchors, ruptura quando suportada | Solver; remover corpo afeta junta |
| Junta configurável | Corpos + ConfigurableJoint | Graus de liberdade, limites, drives e referência | Solver com capacidades correspondentes |
| Mola | Corpos + SpringJoint | Distâncias, spring, damper e anchors | Solver, relógio físico |
| Roda física | WheelCollider ou solução equivalente composta | Raio, suspensão, atrito, steer, torque, freio | Corpo, solo e atualização física; renderer da roda separado |
| Ragdoll | Ossos + shapes + corpos + juntas | Assistente gera composição editável; transição com animação explícita | Skinning e autoridade de pose |
| Tecido | Cloth + renderer deformável/mesh e restrições | Partículas/vértices, colisão, parâmetros e solver | Deformação, física e bounds |
| Superfície de navegação | NavMeshSurface + recursos/coleta | Área, agente, geometria, bake e atualização | NavMeshData e consultas |
| Agente | NavMeshAgent + transform controlado | Destino, raio/altura, velocidade, avoidance | Dados de navegação e política de movimento |
| Obstáculo de navegação | NavMeshObstacle | Forma e carving quando habilitado | Navmesh/avoidance; não implica collider físico |
| Link de navegação | NavMeshLink/OffMeshLink conforme pacote | Extremos, áreas, custo, direção e largura | Agentes e dados de navegação |
| Modificador | NavMeshModifier / Volume | Região, área e regra de coleta | Bake/superfícies afetadas |

O contrato de dependência deve saber apontar o outro corpo, o recurso de navegação ou o mundo físico. Não modelar tudo como “precisa de outro componente no mesmo objeto”.

## 5. Áudio, vídeo, efeitos e ambiente

| Objeto / receita | Composição | Configuração | Dependentes |
|---|---|---|---|
| Fonte sonora | AudioSource + AudioClip ou streaming | Gain, pitch, loop, mixer, espacialização, distâncias/curvas | Backend de áudio, Listener e recursos |
| Ouvinte | AudioListener | Pose e política de ouvinte ativo | Mix espacial; resolver múltiplos ouvintes explicitamente |
| Filtro de áudio | Filtro específico na cadeia suportada | Frequências, ganho, atraso/feedback conforme tipo | Fonte/listener/backend; ordem da cadeia importa |
| Zona de reverb | AudioReverbZone | Distâncias, parâmetros e prioridade | Ouvinte e processamento acústico |
| Vídeo | VideoPlayer + fonte + target | Decoder, tempo, loop, aspecto e áudio | Texture/render target, material/UI, relógio |
| Partículas | ParticleSystem + ParticleSystemRenderer | Emissão, lifetime, módulos, material e bounds | Simulador, renderer, eventos e colisão opcional |
| Força de partículas | ParticleSystemForceField | Região, direção e campos | Sistemas que consomem força externa |
| Linha | LineRenderer | Lista de posições, largura/curva, gradiente, material | Geometria dinâmica e renderer |
| Rastro | TrailRenderer | Tempo, largura, distância mínima e material | Movimento do objeto, buffers e renderer |
| VFX graph | VisualEffect + VisualEffectAsset | Parâmetros expostos, eventos e bounds | Compilador de grafo, compute/render, recursos |
| Terreno | Terrain + TerrainData; TerrainCollider quando necessário | Alturas, camadas, detalhes, árvores, LOD e vizinhos | Renderização, física, navegação e streaming |
| Árvore | Recurso/renderer com geometria/material/LOD | Instanciamento, vento e variação | Terrain/vegetação, sombras e culling |
| Zona de vento | WindZone | Tipo, direção, intensidade/turbulência conforme consumidores | Vegetação/efeitos que explicitamente leem vento |
| Spline | SplineContainer | Nós, tangentes, modo fechado, referências | Movimento, extrusão e instanciadores |
| Extrusão por spline | SplineExtrude + spline + material | Perfil, amostragem, espessura e fechamento | Mesh gerada, renderer, collider opcional |
| Instanciamento por spline | SplineInstantiate + fontes | Espaçamento, orientação, variação e seed | Prefabs/objetos e curva |

Uma zona ou um campo de força não influencia automaticamente todo subsistema: cada consumidor deve registrar o contrato e suportá-lo.

## 6. UI de jogo e texto

Estas receitas referenciam uGUI/TMP. A Astra pode mapear múltiplos tipos Unity para uma implementação própria, desde que registre equivalência, propriedades cobertas e diferenças. Não misturar o layout do editor com o runtime de UI de jogo.

| Objeto / receita | Composição funcional | Passos e referências | Dependentes |
|---|---|---|---|
| Canvas | Canvas + RectTransform; scaler/raycaster conforme uso | Modo overlay/câmera/world, câmera quando exigida, escala | Widgets, rendererUI e input espacial |
| Event System | EventSystem + módulo de input escolhido | Seleção inicial, ações e dispositivos | Selectable, navegação e eventos |
| Panel | RectTransform + visual de fundo | Anchors, offsets, cor/material e raycast | Container de filhos; bloquear input só quando intencional |
| Image | RectTransform + Image + CanvasRenderer | Sprite, tipo, fill, cor, material | Canvas, atlas e clipping |
| Raw Image | RectTransform + RawImage + CanvasRenderer | Texture/RenderTexture, UV rect e cor | Vídeo, preview e UI |
| Texto legado | RectTransform + Text + renderer UI | Fonte, texto, layout e material | Canvas; legado deve ser rotulado |
| Texto TMP UI | RectTransform + TextMeshProUGUI + recursos | Font asset, fallback, material, conteúdo e wrapping | Canvas, atlas, shaping/layout |
| Texto TMP 3D | TextMeshPro + recursos/renderer | Fonte, material, tamanho, alinhamento | Cena 3D e texto |
| TextMesh legado | Transform + TextMesh + renderer | Fonte, texto, espaçamento e orientação | Cena; alternativa legada explícita |
| Button | Visual + Button/Selectable e label opcional | Target graphic, estados, navigation, onClick | EventSystem, módulo e raycaster para interação |
| Toggle | Visual + Toggle, check graphic e label opcionais | isOn, grupo, transições e evento | Input; ToggleGroup regula exclusividade |
| Slider | Slider + referências a fill/handle | Min/max, wholeNumbers, direção, valor e evento | Input/navegação e layout |
| Scrollbar | Scrollbar + handle | Valor, size, passos/direção e evento | Input e ScrollRect quando associado |
| Dropdown | Dropdown/TMP_Dropdown + template/lista | Options, caption, item, valor e evento | Toggle/lista, Canvas e EventSystem |
| Input Field | InputField/TMP_InputField + texto/caret/placeholder | Tipo de conteúdo, limite, seleção, validação, eventos | IME, clipboard, foco e tipografia |
| Scroll View | ScrollRect + viewport/mask + content; barras opcionais | Eixos, inertia, bounds, elasticidade e vínculos | Layout dos filhos, input e clipping |
| Grupo horizontal | HorizontalLayoutGroup + filhos | Padding, spacing, alinhamento, controle/expansão de tamanho | Layout dos filhos |
| Grupo vertical | VerticalLayoutGroup + filhos | Mesmos contratos em outro eixo | Menus/listas |
| Grade | GridLayoutGroup + filhos | Cell size, spacing, constraint, eixo/canto | Layout dos filhos |
| Elemento de layout | LayoutElement | Min/preferred/flexible, prioridade, ignore | Grupo/solver de layout |
| Ajuste de tamanho | ContentSizeFitter / AspectRatioFitter | Modo/eixo/aspecto e autoridade | Layout; evitar ciclos de dependência de tamanho |
| Máscara | Mask ou RectMask2D | Região e apresentação | Renderers UI filhos; algoritmos diferentes |
| Grupo de interação | CanvasGroup | Alpha, interactable, blocksRaycasts | Visuais e input na subárvore |
| Documento UI Toolkit | UIDocument + PanelSettings + VisualTreeAsset/estilos | Documento/árvore/bindings; elementos visuais próprios | Painel, eventos, estilo e renderização UI Toolkit |

A lista pesquisável separa os 150 tipos VisualElement dos componentes GameObject. Seus campos e bases estão disponíveis sem sugerir que cada botão UI Toolkit seja anexado pelo menu Add Component.

## 7. 2D, organização e sequenciamento

| Objeto / receita | Composição | Caminho | Dependentes |
|---|---|---|---|
| Sprite | SpriteRenderer + Sprite | Importar sprite, escolher material/cor/sorting | Renderer2D, animação e máscara |
| Sprite mask | SpriteMask + recurso | Definir shape/sprite e faixa de sorting | SpriteRenderers que participam |
| Grid | Grid | Layout de célula, tamanho/gap/swizzle conforme contrato | Tilemaps e posicionamento |
| Tilemap retangular | Grid + Tilemap + TilemapRenderer | Tile assets/paleta, pintura, ordem e material | Colisão quando adicionada |
| Tilemap isométrica/hexagonal | Grid configurado + Tilemap/renderer | Layout/transform de célula e sorting coerentes | Picking, pintura e colisão |
| Tilemap física | TilemapCollider2D; composição com corpo/composite conforme necessidade | Rebuild, extrusão/composição e material | Mundo físico2D |
| SpriteShape aberto/fechado | SpriteShapeController/Renderer + perfil/curva | Spline, pontos, perfis e geração | Mesh2D e collider opcional |
| Sprite com esqueleto | SpriteSkin + recursos/pesos + ossos | Vincular sprite e skeleton, deformar | SpriteRenderer e animação2D |
| Biblioteca de sprites | SpriteLibrary + SpriteResolver | Categorias/labels e biblioteca | Animação por troca de imagem |
| Corpo 2D | Rigidbody2D + Collider2D(s) | Tipo, massa, gravidade, restrições e filtros | Solver2D, joints e contatos |
| Effector 2D | Componente effector + região/composição exigida | Direção/força/superfície conforme tipo | Corpos2D participantes |
| Timeline | PlayableDirector + TimelineAsset | Tracks, clips, bindings, relógio e signals | Animação, áudio e câmera |
| Rig | RigBuilder/Animator + hierarquia de Rig/constraints | Ossos, alvos, pesos e ordem | Avaliação da pose |
| Gerenciador lógico | Vazio + behaviors | Referências a serviços/objetos e lifecycle | Gameplay; não exige malha |

## 8. Extensões condicionais e seus pré-requisitos

| Receita | Composição / recursos | Infraestrutura indispensável |
|---|---|---|
| Sessão de rede | NetworkManager + transporte + configurações | Transporte real, autoridade, IDs, catálogo de prefabs e diagnóstico |
| Objeto de rede | NetworkObject + behaviors/sincronizadores | Sessão, spawn/despawn, ownership e serialização |
| XR Origin | Origem + câmera rastreada + controlador/input | Provedor XR instalado, runtime e espaços de rastreio |
| Interactor | Direct/Ray/Gaze/Near-Far/Socket conforme tipo | InteractionManager, input, queries e alvo compatível |
| Interactable | Grab/teleport/interação conforme tipo | Colliders/poses/gerenciadores exigidos pelo contrato; não igualar todos os tipos |
| UI XR | Canvas/elementos + raycaster e módulo próprios | Input rastreado, interação e UI |
| Sessão AR | ARSession + origem + managers | Provedor AR Android, permissões e capacidade de hardware |
| Plano/face/nuvem AR | Trackable + visualizador/prefab | Manager correspondente e dados fornecidos pelo provedor |
| Conteúdo localizado | Componentes de Localize + tabelas/eventos | Locale, tabelas, carregamento e fallback |
| Conteúdo endereçável | AssetReference/handle e catálogo | Build de catálogo, resolução e lifecycle de recursos |

Essas receitas entram no roadmap como capacidades condicionais. Registrar o componente na lista sem transporte, provedor, decoder ou renderer não entrega sua função.

## 9. O que o usuário deve conseguir fazer com qualquer composição

1. Encontrar a capacidade por nome, finalidade, categoria, favoritos ou contexto.
2. Ver pré-requisitos e impactos antes da mudança; receber uma composição válida sem inserir duplicatas silenciosas.
3. Identificar cada componente na hierarquia/inspector e, quando espacial, na cena.
4. Editar valores e referências no formato apropriado, com entrada livre no domínio permitido.
5. Navegar ao recurso e voltar ao objeto, preservando contexto.
6. Ver quem depende do item e quem o utiliza; reparar referência sem perder identidade.
7. Criar presets e variantes, ordenar visualmente grupos e reorganizar painéis.
8. Editar por UI, gizmo, script ou ferramenta usando os mesmos comandos e regras de fase.
9. Desfazer/refazer a operação inteira, salvar, reabrir e exportar o comportamento do escopo suportado.
10. Receber diagnóstico claro quando uma capacidade, referência ou propriedade não está implementada; dados desconhecidos são preservados.

## 10. Fechamento do inventário de objetos

O conjunto de objetos possíveis é aberto: qualquer subárvore/composição pode virar um recurso reutilizável. “Listar todos os objetos” deve significar inventariar receitas oficiais do recorte e permitir composições arbitrárias. Não transformar uma lista de receitas em enum fechado de entidades da Astra.

Ainda é necessário fechar menus registrados por constantes e pelo código nativo da Unity, menus de Graphics Editor não coletados, customização por packages adicionais e seus defaults automáticos. Essa pendência é separada das 78 declarações literais extraídas. O arquivo de menus contém pacote, método, caminho, linha e fonte para o próximo agente continuar sem presumir equivalência.
