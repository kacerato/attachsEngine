# Execução do plano universal de cenários realistas — registro por bloco

Registro honesto do que foi **implementado** e do que foi **validado**, na
granularidade dos blocos G0–G6 de
[PLANO-UNIVERSAL-CENARIOS-REALISTAS-2026-09-17.md](PLANO-UNIVERSAL-CENARIOS-REALISTAS-2026-09-17.md).

Vocabulário (o mesmo da §9 do plano): **inventariado** → **especificado** →
**implementado não validado** → **validado no escopo indicado**.

**Escopo de validação desta rodada:**

1. Suíte inteira de host (`build/editor-host`, Ninja + g++ com `-Werror`,
   incluindo o Jolt): **1084 de 1084 verdes** após integrar atmosfera física e
   SH9 em 21/09/2026. Essa suíte completa precede a última regressão de ordem de
   desenho; depois dela, os filtros finais `graphics_` (**3/3**) e `screen_`
   (**28/28**) passaram. A suíte completa não foi repetida depois dessa adição. As
   falhas antigas foram corrigidas; a seção "Suíte do host" abaixo diz como.
   A cobertura acumulada deste plano inclui testes como
   `test_component_contracts`, `test_component_preset`,
   `test_component_recipes`, `test_import_geometry_profile`,
   `test_import_scene_impact`, `test_import_format_compat`,
   `test_import_node_exclusion`, todos registrados em `native/CMakeLists.txt`.
2. **Build nativo completo para arm64-v8a pelo Gradle/NDK, com `-Wall -Wextra
   -Wpedantic -Werror`: `BUILD SUCCESSFUL`**. O APK Release foi gerado,
   instalado e aberto no aparelho; isto valida compilação/integração nesse alvo,
   sem substituir medição visual ou de desempenho.
3. **Evidência no aparelho** (Xiaomi 25053PC47G, ARM64): APK instalado, shell
   ASTRA abre, projeto abre, seleção e gizmo funcionam. Duas coisas foram vistas
   funcionando na tela: o agrupamento novo do Corpo físico aparecendo como abas
   **Corpo | Início** assim que o movimento deixa de ser estático
   (`build/astra-g2-body-dynamic.png`), e o **diff campo a campo do preset**, com
   "1 campo diferente", "Marcar tudo / Desmarcar" e a linha `Renderizar: falso →
   verdadeiro` com a marca de levar ou não (`build/astra-preset-diff.png`). Em
    ambos os casos o projeto usado foi devolvido ao estado inicial — componente
    removido, preset excluído, valor restaurado. No G4, o Inspector do componente
    Luz mostrou unidade Lux/Lúmen, intensidade 1000 e o campo condicional de
    temperatura; a edição de 6500 K para 2500 K foi aceita no aparelho sem falha
    (`docs/capturas/g4/luz-fotometrica-6500k.png` e
    `docs/capturas/g4/luz-fotometrica-2500k.png`). O objeto dessa conferência não foi
    salvo no projeto.

**O que NÃO foi validado:** comparação visual A/B controlada com outro renderer,
desempenho do novo céu e equivalência com uma atmosfera completa da HDRP. No aparelho, o GLB oficial `PointLightIntensityTest`
percorreu a leitura, a prévia, a opção **Importar luzes** e a nova preparação do
perfil; a publicação foi cancelada de propósito para preservar o projeto aberto.
O contrato até a criação dos componentes continua coberto no host.

**Compatibilidade Android pendente:** o APK estável permanece em .NET 8.0.27 e
abre projetos no aparelho ARM64 de página 4 KiB. A verificação explícita de 16
KiB recusa esse runtime porque `libhostfxr.so` foi distribuída com `PT_LOAD
0x1000`; portanto, o artefato gerenciado ainda não deve ser anunciado como
compatível com dispositivos que exigem página de 16 KiB. O build comum não
mascara essa limitação: a trava pode ser exigida com
`-Pastra.require16kPages=true`.

## G0 · matriz e corpus — concluído no escopo base

| Entrega | Onde vive | Estado |
|---|---|---|
| Registro de capacidades do motor com estado real (implementada / limitada pelo aparelho / planejada), dono e limite | `native/core/engine_capability.h` | implementado; 32 capacidades declaradas |
| Contrato por propriedade: capacidade exigida, consumidor e invalidação, com herança do esquema do componente | `native/scene/components.h`, `native/scene/component_schema.h` | implementado nos 9 tipos registrados |
| Matriz de propriedades **gerada do código**, com padrão, domínio, unidade, grupo, consumidor, invalidação e condicionalidade | `native/scene/component_reflection.h` → [MATRIZ-PROPRIEDADES.md](../componentes/MATRIZ-PROPRIEDADES.md) | implementado; regenerar com `aether_tests --write-property-matrix docs/componentes/MATRIZ-PROPRIEDADES.md` |
| Auditoria que recusa propriedade sem identidade, identidade repetida, componente sem consumidor, capacidade não registrada e propriedade apoiada em capacidade **planejada** | `auditComponentContracts()` + `tests/native/test_component_contracts.cpp` | validado no host |
| Uma verdade só entre a tabela de luzes do renderer e o registro de capacidades | `static_assert` em `native/renderer/punctual_lights.h` | validado em tempo de compilação |
| Visibilidade condicional declarada no descritor, e não reescrita por índice de widget no inspetor | `native/editor/editor_properties.h` | implementado |
| Grupos de Inspector em Corpo físico (Corpo / Início / Amortecimento), Personagem (Cápsula / Locomoção) e Olhar (Sensibilidade / Limites); triplas de velocidade e giro inicial | `physics_body.h`, `character.h`, `camera_look.h` | implementado |
| `enabled` do Comportamento em C# passa a ter PropertyId — desligável por API, preset e animação, não só pelo dedo | `native/scene/script_behavior.h` | implementado |
| Corpus de três fontes reais, com licença, URL, hash e referência visual no Android: `old_military_crate`, `portable_generator` e `medical_box` | `example-projects/*/Assets/FONTES-ARTE.md`, `docs/capturas/2026-09-17/` | hashes conferidos com os registros; capturas `perimetro-4-alvos-android.png`, `quarentena-play-android.png` e `resgate-play-android.png` |

O corpus não substitui uma comparação A/B de renderer; ele fecha proveniência,
fonte real e referência visual reproduzível pedidas pelo G0.

## G1 · composição e presets — concluído no escopo base

| Entrega | Onde vive | Estado |
|---|---|---|
| Recursos declarados como reflexão do componente (`ComponentResourceBinding`), com slots, herança e ausência declarada distintas | `native/scene/components.h`, `native/scene/mesh_renderer.h` | implementado |
| Grafo de impacto, reparo de recurso e substituição local passam a percorrer os bindings declarados em vez do caminho especial de MeshRenderer | `native/editor/editor_component_impact.h` | implementado |
| Diff campo a campo entre valor atual e candidato, com aplicável / não aplicável e motivo | `native/scene/component_preset.h` | validado no host |
| Aplicação **por campo** em uma transação: nada entra se a combinação deixar o componente inválido | `applyComponentFields()` | validado no host |
| Dependências de recurso de um valor e remapeamento por identidade entre projetos | `componentResourceDependencies()`, `remapComponentResources()`, `missingResourceDependencies()` | validado no host |
| Receita multi componente: captura do objeto inteiro, formato de biblioteca v2 com leitura da v1, aplicação em uma transação com Add resolvendo exigências | `native/editor/editor_component_presets.h`, `EditorSession::applyComponentRecipe` | biblioteca e sessão validadas no host; o teste adiciona Luz, atualiza MeshRenderer e um único Undo restaura o objeto inteiro |
| Painel de presets com diff interativo (marcar campo a campo, marcar tudo, desmarcar), preview de receita com o que é adicionado, atualizado e exigido | `native/editor/editor_screen.cpp`, `editor_session.cpp` | **validado no aparelho** (`build/astra-preset-diff.png`) |

**Lacuna fechada em seguida (ver G3 abaixo):** amostragem, canais, superfície e
fatores por slot eram dados do MeshRenderer **sem PropertyId**, o que deixava a
aplicação seletiva sem alcançá-los. Passaram a ter identidade endereçável por
slot, e o painel deixou de precisar do aviso.

## G2 · importação autorável — concluído no escopo base

| Entrega | Onde vive | Estado |
|---|---|---|
| Perfil de importação v2 com **Normais** (Importar / Calcular), **Modo das normais** (por área / por ângulo) e **Tangentes** (Importar / Calcular), com os nomes do Model Import Settings da Unity | `native/resources/import_profile.h/.cpp` | validado no host |
| Consumidor real: geração de normais quando a fonte não traz NORMAL — o glTF permite e este renderer não — e recálculo quando o perfil pede; geração de tangente forçada por perfil | `native/resources/gltf_import.cpp` | validado no host |
| Campo ausente ou modo desconhecido no arquivo de perfil falha fechado, em vez de assumir padrão | `parseImportProfile` | validado no host |
| Chave do cache de derivados inclui a geometria derivada: um perfil novo não reusa o derivado do antigo | `native/resources/import_cache.cpp` | validado no host |
| Diagnóstico **sem UV com textura declarada**: a fonte declara textura e a primitiva não tem TEXCOORD | `gltf_import.cpp` → relatório de importação | validado no host |
| Diagnóstico **densidade de texel desigual**: razão entre a maior e a menor densidade dentro da primitiva, com a pior razão do arquivo. É a medida que não depende da resolução da textura — e por isso a que explica por que trocar a imagem por uma maior não conserta estiramento | `gltf_import.cpp` → relatório de importação | validado no host |
| **Importar câmeras** (Import Cameras da Unity): a câmera do arquivo vira componente `Camera` no objeto do nó, com lente (perspectiva/ortográfica), planos e a meia volta que converte o −Z do glTF no +Z desta engine | `gltf_import.cpp`, `import_node_map.*`, `editor_import_reconcile.cpp` | validado no host |
| **Importar luzes**: `KHR_lights_punctual` deixa de ser aparência perdida e vira componente `Light` no objeto do nó, com tipo, cor linear, lux/candela, alcance e cones; −Z é convertido para +Z sem girar geometria. Perfil schema 8, cache schema 5, mapa schema 4 e a primeira instanciação usam o mesmo contrato | `gltf_import.*`, `import_profile.*`, `import_cache.*`, `import_node_map.*`, `editor_import_reconcile.*` | validado no host do JSON ao componente e com o GLB CC0 oficial `PointLightIntensityTest`; no Android, prévia, opção ligada e reprocessamento concluídos (`docs/capturas/g2-import-lights-khronos-device.png`); perfil antigo mantém a opção desligada e arquivo truncado falha fechado |
| Câmera em nó com geometria ou filhos é **recusada com motivo**, em vez de girar a geometria do autor para acertar o enquadramento | `gltf_import.cpp` | validado no host |
| **Diff da cena aberta antes de publicar**: quantos objetos desta cena estão presos à fonte, quais saem, quais ficam órfãos (com os nomes) e quantos nós novos entram em cada instância | `importSceneImpact()` em `editor_import_reconcile.*` | validado no host |
| **Perfil por nó — excluir nó da importação**: o nó (e a subárvore) não vem para a cena, mas **continua no mapa com a identidade dele**. Para a cena ele sai pela mesma regra de nó removido (sem edição local sai, com edição fica órfão); reincluir traz de volta o **mesmo** nó, reintroduzido na revisão nova para não ser confundido com "apagado pelo autor". Mudar só a exclusão, com os mesmos bytes, avança a revisão do mapa | `import_node_map.*` (`excluded`, `markExcludedNodes`), `import_profile.*` (schema 4), `editor_import_reconcile.cpp` | validado no host, inclusive a reconciliação que remove e traz de volta |
| Interruptor por nó na aba **Estrutura**, com a herança do pai visível (filho de excluído aparece apagado e sem toque); o impacto na cena é refeito a cada toque sem reler o arquivo, e excluir não pede nova preparação | `editor_screen.cpp`, `editor_session.cpp` | **visto no aparelho** com um GLB real de 443 nós (`docs/capturas/g3/importacao-estrutura-no-excluido-herda.png`) |
| Reabrir o projeto usa a exclusão com que a fonte foi publicada | `EditorSession::reopenSources` | implementado |
| Controles no painel de importação (aba Perfil) e as linhas novas no relatório | `native/editor/editor_screen.cpp`, `editor_session.cpp` | **visto no aparelho** (`importacao-perfil-normais-pendente.png`). O aparelho achou dois defeitos, corrigidos: (1) a tela só considerava escala e textura para "perfil pendente", e Normais/Modo/Tangentes/câmeras deixavam publicar a prévia antiga — agora a tela faz a mesma pergunta que a sessão, com teste por campo; (2) numa tela baixa as linhas de baixo ficavam cortadas sem como alcançá-las — o Perfil é paginado como as outras abas, com as ações fixas no rodapé |

| **Smoothing Angle** (Model Import Settings da Unity): em toda normal gerada, a aresta entre faces com ângulo acima do limite fica dura — o vértice é dividido e cada lado leva a própria normal; a divisão respeita o Modo das normais. Perfil novo nasce com os 60° da Unity; perfil gravado antes (schema < 5) lê 180°, que é o que ele publicou. Entra na chave do cache e na pergunta "perfil pendente"; no painel, passos de 15° | `GltfImportLimits::smoothingAngle`, `splitHardEdges`, `ImportProfile` schema 5 | validado no host (cubo de 8 vértices vira 24 com normais nos eixos; plano não se parte) e **no aparelho**: o mesmo cubo reimportado com 60° passa de degradê a faces planas (`smoothing-angle-60-vs-180.png`) |

**Limites declarados:**

- não há a opção "None" de normais da Unity (aqui ela só produziria superfície
  preta);
- a câmera importada entra **desligada** (`enabled = false`): trazer o
  enquadramento do editor 3D não pode sequestrar a câmera do Play.

## G3 · materiais e malhas — concluído no escopo base

Fecha a lacuna declarada no G1 e atende a §5.4 do plano ("Material: UV set e
transform por binding", "canais", "superfície") na granularidade que ela pede.

| Entrega | Onde vive | Estado |
|---|---|---|
| `ComponentSlotNumber` / `ComponentSlotEnum`: propriedade que existe uma vez **por slot**, com identidade persistente e escrita que nunca cria slot | `native/scene/components.h` | validado no host |
| MeshRenderer declara por slot: tipo de superfície, faces, corte do alfa; canais de rugosidade/metálico/oclusão, origem e força da oclusão, inversão Y da normal, origem do alfa; conjunto de UV, repetição, filtro, deslocamento, escala e rotação; os 11 fatores PBR; e o próprio interruptor de override | `native/scene/mesh_renderer.h` | validado no host |
| Matriz, auditoria, diff e aplicação seletiva passam a endereçar slot; a auditoria recusa propriedade por slot sem escrita | `component_reflection.h`, `component_preset.h` | validado no host |
| Rótulos no vocabulário do URP Lit (Surface Type, Alpha Clipping, Render Face, Tiling, Offset) | `mesh_renderer.h` | implementado |

**Limite declarado:** as propriedades de amostragem escrevem o valor em **todos
os bindings do slot** e leem o binding de cor base. Endereçar (slot × binding)
por propriedade multiplicaria a lista por cinco; a escolha por binding continua
no editor de material, que endereça os dois índices. Também permanece: aplicar
valores nunca cria slot — trocar a quantidade de slots é mudar a geometria do
objeto, e só a substituição completa do componente faz isso. O painel diz isso
quando (e só quando) o preset tem outra quantidade de slots.

### Colisão derivada da malha (Mesh Collider)

| Entrega | Onde vive | Estado |
|---|---|---|
| Forma **Malha** no Colisor, com **Convexo** — o Mesh Collider da Unity. Por padrão a forma herda a malha que o Renderizador do próprio objeto desenha (todos os slots, a mesma malha em dois slots conta uma vez), com a escala do objeto; como na Unity, não tem centro nem rotação próprios | `native/scene/collider.h` (v6, lê v1–v5) | validado no host, inclusive round-trip v6 e leitura das versões anteriores; o overlay também ignora corretamente valores de Pose guardados antes da troca para Malha |
| **Malha de colisão** separada da visual: binding tipado `collision_mesh` por `AssetGuid`, vazio para herdar a visual. O seletor refletido do Inspector, Undo/Redo, preset, remapeamento/reparo e grafo de impacto usam o contrato comum de recursos; o Play resolve o GUID na biblioteca e entrega os triângulos ao Jolt | `Collider::collisionMesh`, `ComponentResourceBinding`, `EditorAction::ComponentResource`, `CollisionGeometrySource` | consumidor físico e Undo validados no host; campo condicional, seletor e restauração por Undo observados no aparelho |
| **Gerar/Regenerar malha física** cria um recurso derivado sem alterar a malha visual. O seletor expõe passos de 5–90% dos triângulos e erro relativo máximo de 0,1–25%, mostra contagens e erro medido. A identidade do derivado fica estável ao mudar os parâmetros; perfis schema 6 preservam seu GUID legado ao migrar para schema 7. Perfil, biblioteca e binding avançam e voltam juntos em Undo/Redo | `resources/mesh_derived.*`, `ImportProfile::collisionMeshes`, `EditorAction::GenerateCollisionMesh`, `EditorHistory::recordResource` | simplificador real, migração, regeneração, identidade e transação integral validados no host; autoria em 50%/5%, regeneração em 75% sem duplicar o recurso e Undo retornando controles e receita a 50%/5% observados no Android |
| **Visual da colisão de malha** usa os triângulos do recurso efetivamente vinculado, ou todos os slots visuais distintos quando está herdando. No modo Convexo, cozinha com o Jolt e desenha as faces do `ConvexHullShape`; o resultado fica em cache por malhas+tolerância. O overlay limita a 2.400 segmentos por colisor | `physics/collision_cooking.*`, `EditorMapScene::collisionHullPreview`, `editor_component_visuals.h` | cubo validado no host com 8 vértices/6 faces finais, teto de segmentos e pose coerente; prévia observada no Android em `malha-colisao-autoria-50-5.png` |
| **Cozimento** no Inspector: `Soldar vértices iguais` e `Otimizar para o jogo` correspondem às intenções de `WeldColocatedVertices` e `CookForFasterSimulation` da Unity 6. A Astra também expõe a tolerância do casco e o ângulo de aresta ativa porque são parâmetros reais do Jolt. Um cartão resume `Casco Jolt · vértices · faces` ou os triângulos exatos e mostra falha de cooking antes do Play | `Collider` v6, grupo `Cozimento`, `AetherMeshCookingV1` | persistência, reflexão, ABI, consumidor Jolt e UI Android validados |
| Convexo ligado: casco convexo, aceito em qualquer corpo. Desligado: os triângulos exatos, **recusados em corpo dinâmico com o motivo** ("ligue Convexo") — a mesma regra da Unity, porque triângulos soltos não têm volume nem massa | `runtime/physics_requirements.h`, `runtime/scene_physics.cpp` | validado no host, inclusive um caixote convexo caindo e parando sobre um piso de malha |
| Ponte com o Jolt: `AetherPhysics_CreateCompoundBodyV3` acrescenta cooking versionado; V2 continua congelada e encaminha os padrões antigos. Inicialização global do Jolt é compartilhada pelo Play e pelo cooker autoral, então a prévia funciona antes de criar um mundo | `native/physics/jolt_bridge.*`, `physics/jolt_init.*` | V3 aceita parâmetros não padrão, recusa contrato inválido e mantém V2 compatível no host |
| A física não conhece o editor: quem monta o mundo entrega a geometria por `CollisionGeometrySource`; o Play usa a MESMA geometria da seleção e do ajuste de colisor | `runtime/scene_physics.h`, `editor/editor_play_scene.h` | implementado |
| O Inspector mostra o motivo antes do Play (sem Renderizador de malha, não convexo em corpo dinâmico) | `editor/editor_component_impact.h` | implementado |
| **Defeito antigo corrigido:** uma consulta física (raio, varredura, sobreposição) num corpo com várias formas sempre respondia com o PRIMEIRO colisor. O índice da parte ia na entrada do composto, e o Jolt devolve o dado da forma folha | `jolt_bridge.cpp` (`ToJoltShape` com dado de usuário) | validado no host |

**Evidência no aparelho** (projeto de teste G3MalhaColisor, `docs/capturas/g3/`):
forma Malha com Convexo e sem a aba Pose (`colisor-forma-malha.png`); Play
recusado com o motivo quando o chão de malha não convexa é dinâmico
(`malha-nao-convexa-dinamico-recusada.png`); um cubo dinâmico com Malha +
Convexo cai e para sobre o chão de malha estática
(`cubo-convexo-repousa-no-chao-de-malha.png`). Com a forma Malha, "Ajustar à
malha" deixa de aparecer: ele trocaria a malha por uma primitiva. O novo campo
condicional **Malha de colisão** abre o seletor tipado, mostra a escolha e volta
à herança da malha visual por Undo (`malha-colisao-separada-seletor.png`).
O mesmo seletor oferece a geração derivada (`malha-colisao-gerar-25.png`) e a
atribuição resultante aparece no campo do componente
(`malha-colisao-derivada-aplicada.png`); a edição temporária foi desfeita após a captura.
Os controles editáveis, a regeneração sem recurso duplicado, o overlay e a
sincronização visual depois de Undo estão registrados em
`malha-colisao-autoria-50-5.png`, `malha-colisao-regenerada-75.png` e
`malha-colisao-undo-sincronizado.png`. Depois da validação, o projeto foi
restaurado ao perfil sem receita e ao objeto original com somente a Malha.

**Limite declarado:** o simplificador pode manter mais triângulos que o alvo
quando o erro necessário ultrapassa o máximo escolhido. O modo Convexo continua
produzindo um único casco; decomposição em vários cascos continua pendente.

### LOD Group (autoral, pela altura na tela)

Fluxo de uso da Unity: **Add Component → LOD Group**, arrastar os objetos para
cada nível, ajustar a transição de cada um em % da altura da tela; ou importar
um modelo cujos nós seguem `Nome_LOD0`, `Nome_LOD1`... e receber o grupo pronto.

| Entrega | Onde vive | Estado |
|---|---|---|
| Componente `LOD Group` (`astra.render.lod_group`): 1 a 4 níveis, objeto de cada nível, transição em % e tamanho; o nível escolhido fica visível com a subárvore, os outros somem, abaixo do último tudo some (Culled). Um objeto comum a dois níveis não some por estar no nível não escolhido | `native/scene/lod_group.h`, `native/runtime/lod_groups.h` | validado no host |
| Métrica da Unity: tamanho do grupo × maior escala global sobre a altura que a vista enxerga àquela distância; ortográfica pela meia altura. Vale a câmera do Play quando há uma, e a do editor no resto | `scene::lodRelativeHeight`, `EditorSession::lodView` | validado no host |
| A troca chega à tela pela publicação de **poses** (esconder desenhos não muda a topologia): o shell republica quando a câmera cruza uma transição, mesmo sem mudança de revisão. A detecção retém no relógio real o início do cross-fade, inclusive no primeiro quadro de fator zero | `EditorSession::lodSelectionChanged`, `android_main.cpp` | regressão focada no host, build Android e transição animada no aparelho validadas |
| Referência **Descendente**: cada nível só aceita objetos abaixo do grupo; o seletor lista só esses e a execução ignora o resto | `ObjectReferenceScope::Descendant` | validado no host |
| Inspector: **Níveis** (contagem, objetos, transições) e **Limites** (tamanho, *Recalcular tamanho*); atribuir o LOD 0 mede o tamanho no mesmo comando (um Desfazer volta os dois); linha "Na vista: LOD n · x% da tela" como a barra de LOD da Unity | `editor_screen.cpp`, `editor_session.cpp`, `editor/editor_lod_group.h` | compilado e observado no aparelho |
| **Importação `_LOD<n>`** (Model Importer da Unity): filhos diretos com o sufixo viram LOD Group no pai, medido pela malha do LOD 0, na mesma transação da instanciação; buraco na sequência ou nível além do quarto é avisado no console | `addImportedLodGroups` | validado no host, pela importação real de um GLB |
| Script: `ComponentIds.LodGroup` com as propriedades pela reflexão comum (`level_count`, `transition_0..3`, `level_0..3`, `size`) | `managed/Astra.Scripting/World.cs` | implementado |

| **Fade Mode = Cross Fade**, como na Unity: os dois níveis são desenhados com cobertura complementar pelo mesmo dither do LOD do pacote (`GpuMeshInstance::normalColumns[7]`, `lod_dither.glsl`). **Fade Transition Width** por nível (proporção do comprimento do nível, 0..1) ou **Animate Cross-fading** (troca por tempo, 0,5 s como `crossFadeAnimationDuration`); o último nível some aos poucos; durante a troca só o nível que sai projeta sombra | `scene::lodGroupFadeFactor`, `runtime::lodObjectStates`, `LodCrossFadeClock` | validado no host |
| **ForceLOD** como estado de execução: `force_level` (0 automático, n = LOD n-1) vence a altura, não aparece no Inspector e nunca é gravado — o Play sempre parte do automático | `LodGroup::forcedLevel` | validado no host |
| **Reimportação que acrescenta níveis**: nós `_LOD<n>` novos entram no grupo que já existe (só em nível vazio, estendendo a sequência a partir do LOD 0), no mesmo Desfazer da reimportação; transições e tamanho do autor ficam | `extendImportedLodGroup`, `ImportReconcileReport::createdObjects` | validado no host, com duas importações reais |

**Evidência no aparelho** (Xiaomi 25053PC47G, projeto de teste G3MalhaColisor,
`docs/capturas/g3/`):

- troca por distância na vista do editor, com a linha "Na vista" acompanhando:
  Culled até 29%, LOD 1 a partir de 37%, LOD 0 a partir de 63%
  (`lod-group-distancia-*.png`);
- Cross Fade com largura 0,2: a 66–63% (faixa do LOD 0, 60–68%) os dois
  níveis aparecem com a trama de Bayer complementar; fora da faixa, inteiros
  (`lod-group-cross-fade-*.png`); o Inspector mostra Animate Cross-fading e a
  largura por nível só com Cross Fade (`lod-group-fade-inspector.png`);
- depois da correção do relógio persistente, **Animate Cross-fading** foi gravado
  cruzando de LOD 1 para LOD 0 em 60%. Com a câmera já parada, a cobertura ainda
  muda nos quadros seguintes durante os 0,5 s
  (`lod-group-animate-cross-fade-pos-correcao.png`);
- importação real de um GLB com `Coluna_LOD0`/`Coluna_LOD1`: o LOD Group nasce
  na Coluna com os dois níveis, e a troca pirâmide → cubo acontece pela
  distância (`lod-group-importado-*.png`); reimportar a fonte com
  `Coluna_LOD2` acrescenta o terceiro nível sem recriar o grupo
  (`lod-group-reimportacao-acrescenta-nivel.png`).

Antes da correção, as capturas de **Animate Cross-fading** ficavam idênticas.
`lodSelectionChanged` iniciava a troca numa cópia de `LodCrossFadeClock`; como o
fator do primeiro quadro é zero, não havia diferença visual e a cópia era
descartada. O relógio persistente agora recebe esse início e mantém a
republicação pelos 0,5 s; a regressão focada e a gravação no aparelho cobrem o
defeito.

O aparelho achou um defeito de uso, corrigido: com a escada 60/30 da Unity, um
grupo importado de dois níveis ficava Culled abaixo de 30% e o modelo sumia no
próprio enquadramento pós-importação. Nos grupos gerados por nome, o último
nível passa a valer até 1% da tela (adaptação da Astra; o autor muda no
Inspector).

**As faces pretas do GLB de teste eram três causas, separadas por experimento
no aparelho** (`culling-*.png`):

1. **Face da frente invertida — defeito real, corrigido.** O pipeline com
   culling por material declarava anti-horário; neste mundo de mão esquerda com
   o Y invertido do Vulkan, a frente glTF chega à tela em sentido horário, e o
   culling descartava a frente de TODO material de uma face: os cubos
   apareciam do avesso. Materiais de face dupla nunca ligam o culling e
   escondiam o defeito.
2. **Metal sem textura:** o `metallicFactor` padrão do glTF é 1, e metal não
   tem difuso — só reflete o ambiente. O cubo do teste era metal; com
   `metallicFactor` 0 as laterais iluminam. Não é defeito; é o material.
3. **Normais médias nas quinas:** resolvido pelo Smoothing Angle (acima).

**Diferenças da Unity:** sem o modo SpeedTree; a largura padrão do fade é 0,2
(a documentação da Unity não fixa uma); um pai existente SEM LOD Group não
ganha um na reimportação — a ausência ali pode ser escolha do autor; grupo
gerado por nome termina em 1% em vez de cortar no último degrau da escada;
Animate Cross-fading tem teste de host, sem captura no aparelho.

## Suíte do host — 1070 de 1070 verdes

Havia cinco falhas **anteriores ao plano** (conferidas numa build de
`a80fcb28`). Foram fechadas assim:

- **Colisão de identidade de widget — defeito real, visto também no aparelho.**
  `ImpactRowBase`/`ImpactClose` tinham os mesmos números de
  `CreationCategoryBase`/`CreationRowBase`, e `ImpactOpenBase` o mesmo de
  `AssetRowBase`. O painel de impacto engolia o toque na categoria do menu
  Adicionar objeto: "Geometria" não abria, e três testes de sessão falhavam por
  isso. O impacto ganhou faixas próprias, e um `static_assert` em
  `editor_screen.h` confere na compilação que nenhuma faixa-base se cruza com
  outra, cada uma com o seu tamanho. Evidência:
  `docs/capturas/g3/menu-geometria-responde.png`.
- **Dois testes atrás do design:** o "voltar para a cena" do IDE saiu da barra
  para a trilha "< Cena" (88a1ca6a), e o raio de seleção passou a começar no
  plano próximo (a80fcb28). Os testes passaram a exigir o comportamento atual,
  em vez de desfazer o design.
- `r4_sampling_…` falhava só com a projeção triplanar ainda fora de commit; o
  teste agora recusa o primeiro valor depois de Mundo, escrito pela constante.

Uma falha **deste** trabalho foi achada e corrigida pela suíte: os campos novos
da aba Perfil empurravam "Preparar com este perfil" para fora de uma tela baixa
(`r3_import_lives_in_properties_and_does_not_block_the_editor`). As ações
passaram para o rodapé fixo, como Revert/Apply no Import Settings da Unity.

## Toque: a expansão até o alvo mínimo roubava a linha vizinha

Achado no aparelho, na aba Estrutura: tocar no meio de uma linha desligava a de
baixo. O roteador expande regiões pequenas até o alvo mínimo de toque e escolhia
a primeira região cuja área **expandida** continha o dedo — numa lista de linhas
mais baixas que o alvo mínimo, a expansão da linha de baixo cobria metade da de
cima. Agora vale a regra do TouchDelegate do Android e dos alvos de toque da
web: a expansão só ajuda o toque que cai **perto** de um alvo, nunca o que cai
**dentro** de outro; um bloqueador por cima continua encerrando a busca
(`native/ui/ui_input.cpp`, teste `input_touch_expansion_never_steals_a_touch_inside_a_neighbour`).
Vale para toda lista do editor. **Validado no aparelho:** o mesmo toque no meio
da linha que antes pegava a vizinha agora desliga o próprio nó, e os filhos
herdam (`docs/capturas/g3/estrutura-toque-no-meio-da-linha-acerta.png`).

## G4 · luz e GI de base — concluído no escopo base

| Entrega | Onde vive | Estado |
|---|---|---|
| Unidade autoral explícita: escala interna para cenas legadas; lux/candela conforme `KHR_lights_punctual`; ou lux/lúmen para fluxo de luz local. A conversão usa uma única ponte de 683 lm/W e, no spot, integra a mesma janela angular linear que o shader desenha | `scene/light_units.h`, `scene/light.h`, `runtime/scene_lights.cpp` | implementado e validado no host; a cena v1 migra para escala interna sem alterar o brilho |
| Filtro por temperatura correlacionada de cor, 1667–25000 K, multiplicado pela cor linear e neutro em D65/6500 K | `scene/light_units.h`, propriedades `use_color_temperature` e `color_temperature` | implementado e validado no host; toggle e edição 6500 K → 2500 K observados no Android |
| Propriedades disponíveis pelo Inspector, preset/reflexão e API comum de componentes; invalidação de seleção de luzes; capacidades `render.light.photometric` e `render.light.temperature` vinculadas ao consumidor | `scene/light.h`, `core/engine_capability.h`, `managed/Astra.Scripting/World.cs` | contrato auditado no host e Inspector observado no Android |
| Luzes pontuais do GLB chegam pelo perfil autoral e usam o mesmo componente/unidade do Inspector e runtime; primeira instanciação e novos nós de reimportação compartilham a aplicação | `resources/gltf_import.cpp` → `ImportNodeMap` → `applyImportedNodeComponents` → `runtime/scene_lights.cpp` | validado no host de ponta a ponta; leitura, perfil e reprocessamento também observados no Android |

Referência de autoria: Unity 6.0 usa **Filter and Temperature**, multiplicando a
temperatura pelo filtro de cor, com D65 em 6500 K. A interoperabilidade segue a
extensão ratificada `KHR_lights_punctual`: direcional em lux, ponto e spot em
candela. A opção por lúmen converte fluxo para intensidade considerando o
ângulo efetivo do spot.

Sombras pontuais/spot, atlas local, lightmap, sondas e bake pertencem aos blocos
avançados do plano e continuam com capacidade **Planned**, sem controles no
Inspector nem setters anunciados como funcionais. O escopo base usa oito luzes
locais, uma direcional e cascatas direcionais; excedentes são reportados.

## G5 · ambiente, HDR e pós de cena — bloco concluído

Referência de autoria: o **Volume/Profile** da URP separa os overrides de cena
da política do pipeline; `RenderSettings` mantém céu e neblina por cena; a HDRP
acrescenta céu físico e atmosfera. A Astra aplica essa intenção pelo componente
universal `Ambiente`, sem copiar classes dos pipelines Unity.

| Entrega | Onde vive | Estado |
|---|---|---|
| Componente `Ambiente` (`astra.render.environment`) com seleção determinística por prioridade e hierarquia; save/reopen, Undo/Redo, preset/reflexão e `ComponentIds.Environment` usam o contrato comum | `scene/environment.h`, `runtime/scene_environment.*`, `component_schema.h` | validado no host; inclusão, remoção por Undo e seis grupos do Inspector observados no Android |
| Autoria agrupada em **Geral / Atmosfera / Pós / Oclusão ambiente / Volume / Neblina**: HDRI ou atmosfera, cores de zênite/horizonte/chão, disco solar, neblina exponencial, exposição EV, ACES/Neutro, bloom, contraste, saturação, vinheta e SSAO | `scene/environment.h`, Inspector refletido | controles condicionais observados no aparelho; propriedades sem efeito oculto não foram adicionadas |
| Céu procedural usa direção do sol real da cena, gradiente atmosférico e disco solar; HDRI continua selecionável | `rhi/shaders/dirt_road_sky.frag` | SPIR-V validado e efeito consumido no Android |
| Scene View no padrão de organização da Unity: botões superiores de Lighting e Effects, com menu Sky / Fog / Post Processing e ícones próprios. São opções editoriais transitórias; Play e câmera autorada continuam usando o ambiente da cena | `editor_screen.*`, `InstancedRenderer::setEditorViewportOptions` | Android: menu observado; desligar Sky removeu visualmente o passe e religar restaurou o céu, sem erro Vulkan |
| Scene View sem `Ambiente` autorado recebe um look editorial transitório com céu, HDR, ACES, bloom, AO e gradação moderada. O fallback não entra no documento nem substitui a câmera de Play; o hemisfério inferior mantém leitura das superfícies e a grade tem contraste reduzido | `renderer::defaultSceneViewEnvironment`, `InstancedRenderer::updateUniformBuffer` e passe da grade | suíte host aprovada; céu, materiais e grade conferidos no Android em cenas distintas |
| Atmosfera do viewport acrescenta espalhamento Rayleigh/Mie analítico, profundidade óptica de horizonte, disco solar antialias e corona, preservando as cores e a direção autoradas | `rhi/shaders/dirt_road_sky.frag` | SPIR-V regenerado de forma reproduzível e executado no Adreno |
| Cena renderiza em `R16G16B16A16_SFLOAT` quando a GPU suporta anexo+amostragem linear; materiais preservam radiância e exposição/tonemapping acontecem uma vez no passe final | `InstancedRenderer::initialize`, `environment_lighting.glsl`, `post_process_common.glsl` | build Android e criação real no Adreno sem erro Vulkan; fallback explícito mantém o formato do display quando RGBA16F não está disponível |
| Neblina reconstrói distância pelo depth para câmera perspectiva e ortográfica. O render graph declara o leitor de pós, obrigando STORE+SAMPLED e proibindo depth memoryless | `renderer/frame_graph.*`, `post_process_common.glsl` | política validada no host e log do aparelho confirmou `store=sim sampled=sim`; ligar Neblina alterou a cena e revelou os controles de cor/densidade/início |
| Pós autoral sobre HDR: bloom antes da curva, exposição em EV, ACES ou Neutro, contraste, saturação e vinheta; o perfil global continua sendo o fallback quando a cena não sobrescreve | `InstancedRenderer::recordPostProcess`, `post_process_common.glsl` | `+2 EV` alterou a imagem no aparelho; valor e componente de teste foram desfeitos depois das capturas |
| Grão de filme sensível à luminância é um override tipado de pós, serializado no componente e no perfil, misturado pelos volumes e aplicado depois da resolução temporal. Intensidade começa sutil e usa o UBO existente, sem aumentar os 128 bytes de push constants | `SceneEnvironment::filmGrain`, `scene/environment.h`, `resources/environment_profile.*`, `post_process_common.glsl` | migração/round-trip aprovados no host; toggle, controle de intensidade e alteração visual observados no Android |
| Oclusão ambiente em espaço de tela usa o depth real, reconstrução perspectiva/ortográfica e 12 amostras rotacionadas; raio em metros, intensidade, potência e viés chegam pelo mesmo UBO do ambiente | `SceneEnvironment`, `DirtRoadFrameUniform::sceneAo*`, `post_process_common.glsl` | serialização/reflexão e resolução de volumes aprovadas no host; controles vistos e ativados no Android, com shader compilado e consumido pelo passe final |
| Instância de ambiente global, caixa orientada ou esfera transformada, com peso, prioridade e faixa externa de mistura | `scene/environment.h`, `runtime/scene_environment.cpp`, `renderer/scene_environment.cpp` | cinco casos focados e suíte host completa aprovados; a posição da câmera resolve a influência a cada quadro, sem republicar a cena durante o deslocamento |
| Overrides independentes de céu, neblina e pós; valores não marcados herdam o resultado anterior/padrão em vez de vazarem do volume | `EnvironmentOverride`, `resolveSceneEnvironment` | teste cobre volume local somente de pós com céu e neblina herdados |
| Oito camadas autorais de ambiente; cada câmera escolhe todas ou uma camada. Perfil e AO são persistidos na versão 4 de Ambiente, máscara na versão 3 de Câmera, com leitura dos arquivos anteriores | `scene/environment.h`, `scene/camera.h`, `editor_scene_camera.h` | save/reopen de componentes e regressões de câmera aprovados no host |
| `EnvironmentProfile` compartilhado separa recurso e instância: arquivo versionado, GUID no registro, seletor tipado, fallback local para referência ausente, atualização comum e Undo/Redo da edição compartilhada | `resources/environment_profile.*`, `EditorSession::createEnvironmentProfile`, `commitEnvironmentProfile` | round-trip, resolução e override local aprovados no host; criação, vínculo, arquivo no projeto e controles de atualização observados no Android |
| A RenderView da câmera renderiza uma fonte HDR própria e executa o mesmo pipeline final da câmera de jogo, inclusive ambiente, neblina, AO, bloom, gradação, FXAA e tonemapping | `instanced_camera_preview.inl`, `InstancedRenderer::createPostResources` | no Android a prévia produziu o cubo; mudar a exposição de 0 para +3 EV alterou também a RenderView, sem erro Vulkan no log |
| Visualização espacial no editor para caixas e esferas, mais cartão **Na vista** com influência percentual e camada | `editor_component_visuals.h`, `editor_screen.cpp` | compilado no host e observado no Android |

Evidência no aparelho (Xiaomi 25053PC47G, `docs/capturas/`):
`g5-atmosfera-editor.png`, `g5-post-editor.png`, `g5-neblina-editor.png`,
`g5-neblina-ativa.png`, `g5-pos-exposicao-2ev.png`,
`g5-perfil-criado.png`, `g5-oclusao-ativa.png`,
`g5-camera-preview-cubo-visivel.png`, `g5-camera-preview-pos-3ev.png`,
`g5-camera-preview-final.png`, `g5-perfil-vazio-final.png` e
`g5-fechamento-apk-final.png`, `g5-scene-effects-menu-device.png`,
`g5-scene-sky-off-device.png`, `g5-scene-fog-off-device.png` e
`g5-fechamento-scene-view-unity.png`, `g5-inspector-grupos-em-duas-linhas.png`,
`g5-grao-filme-ativo.png`, `g5-scene-view-look-padrao.png` e
`g5-viewer-calibrado-sem-grade.png`.
As mudanças autorais usadas na verificação foram desfeitas e o perfil temporário
foi apagado ao terminar.

O contrato foi comparado com o [Volume da URP no Unity 6](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/volume-component-reference.html):
modo global/local, prioridade, peso e distância de mistura têm equivalentes
diretos; o collider separado foi adaptado para caixa/esfera próprias porque a
Astra já possui Transform universal e não deve transformar um volume visual em
contato físico. A filtragem por câmera segue a intenção do
[Volume Mask e Volume Trigger](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/camera-component-reference.html):
a posição real da vista é o trigger e a máscara escolhe as camadas participantes.
No Godot 4, [WorldEnvironment](https://docs.godotengine.org/en/4.5/classes/class_worldenvironment.html)
referencia um recurso Environment reutilizável, e CameraAttributes pode
sobrescrever a câmera. O `EnvironmentProfile` da Astra cumpre a mesma separação
entre recurso compartilhado e instância, enquanto presets de componente
continuam snapshots por cópia. AO e RenderView consomem o ambiente resolvido da
câmera, portanto os controles deste bloco têm consumidor gráfico efetivo.

## G6 · cena real de referência — em execução

### G6-A · enquadramentos salvos (Vistas)

Comparar duas versões de um cenário exige repetir o **mesmo** enquadramento:
recolocar a câmera à mão entre duas medições muda o que está sendo comparado,
porque o resultado passa a incluir a diferença de ângulo. A Unity 6 não traz
isso de fábrica — a Scene view tem *Frame Selected* (F), *Lock View to Selected*
(Shift+F) e a sobreposição **Camera** com Field of View, Clipping Planes e
velocidade, mas guardar um ponto de vista com nome é prática de extensão de
editor ("Set Bookmark" / "Move to Bookmark"). Na Astra a vista é **dado autoral
da cena**: viaja no arquivo, vale para qualquer projeto e tem Desfazer.

| Entrega | Onde vive | Estado |
|---|---|---|
| `SceneView` guarda a câmera de órbita (alvo, distância, yaw, pitch) e a lente (`verticalFov`, o *Field of View* da sobreposição Camera). Posição não é guardada: posição e alvo separados divergiriam e a órbita passaria a girar em torno de um ponto que não está na tela | `native/runtime/scene_views.h` | validado no host |
| `SceneViews` endereça pelo nome: vazio, repetido, com aspas/quebra de linha ou distância não positiva é recusado; teto de 64 vistas e 48 caracteres. `replace` é o "atualizar com a vista atual" | `native/runtime/scene_views.h` | validado no host |
| Seção `VIEWS` no arquivo da cena, versão **13**. Arquivos 1–12 abrem sem nenhuma vista, que é o que sempre tiveram; vista ilegível recusa o arquivo inteiro em vez de perder um enquadramento em silêncio | `native/editor/editor_archive.cpp` | round-trip e leitura de arquivo v6 validados no host |
| Salvar, atualizar, renomear e excluir passam pelo histórico com rótulo próprio **Vistas**, e voltam em Desfazer/Refazer como qualquer edição autoral | `EditorCommandKind::Views`, `EditorHistory::setViews` | validado no host |
| Painel **Vistas** no viewport, ao lado de Enquadrar: lista com a escolhida em destaque, toque na linha leva a câmera até ela, rodapé com Atualizar / Renomear / Excluir e "Salvar vista atual". Nome entra pelo teclado da tela (`EditorTextPurpose::SceneViewName`) | `editor_screen.cpp` (`buildSceneViewsPanel`), `editor_session.cpp` | compilado e coberto no host; falta evidência no aparelho (adb desligado a pedido) |
| Aplicar uma vista antiga sem lente guardada mantém a lente atual do editor, em vez de impor um campo de visão que ela nunca teve | `EditorSession::applySceneView` | validado no host |

### G6-A · medição da fonte (aba Malhas)

Uma cena de referência só serve para comparar mudanças visuais se a FONTE for
conhecida. "A parede ficou borrada" pode ser a iluminação, a textura, o filtro —
ou UV com metade da densidade do resto do prédio, que nenhuma configuração de
engine conserta. A medição separa os dois casos antes da discussão.

O vocabulário é o do [Mesh asset Inspector da Unity 6](https://docs.unity3d.com/6000.3/Documentation/Manual/view-mesh-data-visualizations.html)
(vértices, faces, canais de UV, **Normals**, **Tangents**, **Vertex Color**) e o
do [Model Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html)
(Scale Factor, Tangents: Import/Calculate). A medida que a Unity **não** traz é
a densidade de texel em texels por metro: lá a prática é aplicar uma textura
xadrez (UV Checker) e julgar a olho, e numa tela de seis polegadas isso não
distingue 300 de 600 texels/m — que é justamente a diferença entre uma parede
nítida e uma borrada lado a lado.

| Entrega | Onde vive | Estado |
|---|---|---|
| `MeshDataReport`: contagem, canais presentes, limites em metros, área de superfície e de UV, faixa de UV, densidade de texel por pixel de textura e razão entre decis (esticamento). A densidade acompanha a escala do objeto — o mesmo cubo com o dobro do tamanho tem metade dos texels por metro | `native/renderer/mesh_report.h/.cpp` | validado no host: cubo de 1 m com 1024² dá 1024 texels/m exatos; UV repetida 4× quadruplica; malha desigual sai com razão 4× |
| `ImportSourceReport`: hierarquia (nós, raízes, profundidade), limites do modelo inteiro com a pose de cada nó, e uma linha por malha com material, resolução da textura de cor base, mapa normal e apontamentos em português | `native/resources/import_report.h/.cpp` | validado no host |
| Gravidade honesta: **erro** é só o que nenhuma configuração conserta (sem TEXCOORD com textura no material, UV sem área, mapa normal sem tangente — este com o conserto dito: Tangentes: Calcular). Superfície de cor lisa sem UV é **atenção**, não erro | `import_report.cpp` | corrigido depois de a conferência no corpus oficial acusar erro numa fonte sadia |
| Aba **Malhas** no importador: uma linha por malha com contagem, canais, tamanho na cena e densidade, selo de gravidade, e cartão de detalhe com faixa colorida e o que fazer com cada apontamento. O Resumo diz quantas malhas têm apontamento | `editor_screen.cpp`, `editor_session.cpp` | coberto no host (linhas, abertura e fechamento do cartão); falta evidência no aparelho |
| `aether_tests --source-report <arquivo.glb>`: a mesma medição no terminal, para conferir um arquivo sem abrir o editor | `tests/native/test_mesh_report.cpp` | usado para achar o exagero de gravidade acima |
| Corpus PBR real no teste (Avocado, CC0 do corpus oficial: cor base, mapa normal e metálico/rugosidade em 2048²) | `tests/native/fixtures/gltf/Avocado.glb` | medido de ponta a ponta: 0,081 m de lado, 21.585 texels/m, esticamento 1,32×, nenhum apontamento |

### G6-A · modelo de cena de referência

A Unity 6 tem **Scene Templates** (File > New Scene lista os modelos; Assets >
Create > Scene Template cria outro a partir de uma cena). A Astra usa a mesma
ideia com uma diferença deliberada: o modelo entra na cena **aberta** em vez de
abrir outra — no aparelho, trocar de cena no meio de uma comparação perde
exatamente o que estava sendo comparado.

| Entrega | Onde vive | Estado |
|---|---|---|
| Modelo de cena como DADO: nós com pose, malha, luz fotométrica e cor, mais as vistas salvas que nascem com ele. Nada conhece Vulkan, arquivo ou aparelho | `native/editor/editor_scene_template.h` | validado no host |
| Cenário de referência com escala humana real: corpo de 1,80 m, porta de 2,10 m, pé-direito de 2,60 m. Exterior com sol de 100.000 lux e blocos de 1, 2 e 4 m; interior fechado com vão de porta por onde a luz de fora entra, luz de teto em lúmen e foco de parede; pedestal para o modelo em avaliação nos dois lados | `kReferenceNodes` | validado no host |
| Quatro vistas salvas nascem com o cenário (Exterior, Interior, Interior · porta, Comparação) e o editor abre na primeira | `EditorSession::createSceneTemplate` | validado no host |
| Monta e desfaz em UM passo, vistas inclusive: instanciar malha foi separado da transação para caber dentro de outra (`instantiateAssetInTransaction`) | `editor_session.cpp` | validado no host, com Refazer devolvendo objetos e vistas |
| Sem o cubo autoral na biblioteca o modelo é recusado com motivo, em vez de montar um cenário invisível | `boxAssetSlot`, `createSceneTemplate` | validado no host |
| Entrada "Modelo de cena" no catálogo de criação, com painel de escolha que diz o que cada cenário entrega antes do toque | `editor_creation_catalog.h`, `buildSceneTemplatePanel` | compilado no host e no Android (`assembleDebug` arm64); falta evidência no aparelho |

**Pendente do G6-A com o aparelho desligado:** o passeio no aparelho (aba
Malhas num GLB real escolhido pelo seletor, montagem do cenário de referência e
troca entre as vistas salvas) e a captura das evidências.

### G6-B · sombra de luz pontual e spot (atlas local)

Referência: Light Inspector da Unity 6 — Shadow Type (No Shadows / Hard / Soft),
Realtime Shadows (Strength, Resolution, Bias, Normal Bias, Near Plane) e o atlas
de sombras das luzes adicionais da URP, que escolhe a resolução pelo tamanho da
luz na tela.

| Entrega | Onde vive | Estado |
|---|---|---|
| Política do atlas sem Vulkan: quem recebe sombra (tamanho do alcance na tela), que resolução (potências de dois entre piso e teto, com a escolha do autor por cima) e onde fica (quadtree "buddy", cada tile alinhado ao próprio tamanho). Pontual ocupa seis faces ou nenhuma; o que não cabe é contado por distância ou orçamento | `native/renderer/shadow_atlas.h/.cpp` | 5 casos no host, incluindo projeção de cada face e ausência de sobreposição |
| Luz v3: Sombra (Nenhuma/Dura/Suave), Resolução da sombra (Automática/Baixa/Média/Alta/Muito alta), Força, Desvio, Desvio na normal, Plano próximo. Só em pontual e spot — a direcional segue com as cascatas do sol; ajustes finos só aparecem com a sombra ligada. Luz v2 abre sem sombra, lendo só os oito números da época | `native/scene/light.h`, `runtime/scene_lights.cpp` | host; a leitura v2 foi corrigida porque o teste pegou que ela passaria a exigir os números novos |
| Capacidade `render.shadow.punctual` agora **Implemented**; a tabela do renderer e o registro concordam em tempo de compilação | `core/engine_capability.h`, `renderer/punctual_lights.h` | host |
| Passe Vulkan: atlas local próprio (não divide a imagem das cascatas, para o cache estático do sol continuar valendo), mesmo passe compatível e mesmas pipelines de caster; recorte de casters pela esfera de alcance; desvio autoral escala o do sol (0,05 reproduz o desvio atual). O atlas é limpo todo quadro, com ou sem tile, para o descritor ficar válido | `instanced_renderer.cpp` (`recordLocalShadowPass`), binding 17 | compilado arm64, SPIR-V de todos os shaders regenerado e validado com `spirv-val` |
| Amostragem: pontual escolhe a face pelo eixo dominante, desvio na normal medido em texels do mapa, suave com 4 buscas de compare (16 amostras efetivas), força mistura com o iluminado; fora do mapa conta como iluminado | `rhi/shaders/dirt_road_shading.glsl` | idem |
| Bloco do quadro: 4 vec4 por luz (a quarta aponta o tile), 16 tiles com matriz e retângulo | `DirtRoadFrameUniform` (3104 bytes), `environment_lighting.glsl` | idem |

### G6-A/B · passada no aparelho (Xiaomi 25053PC47G, 21/09/2026)

Projeto de teste próprio (`G6Referencia`); nenhum projeto do usuário foi tocado.

| Achado | Causa | Correção |
|---|---|---|
| Projeto salvo pela build nova não abria ("Formato de cena não reconhecido") | o shell Java aceitava arquivo até v12 e as Vistas subiram para v13. O teste gerenciado `ArquivoDeCenaTests` pega isso, mas não tinha sido rodado | `ProjectSceneSource.LAST_SUPPORTED_ARCHIVE_VERSION = 13`; suíte gerenciada rodada: 456 verdes |
| Botão Vistas invisível | desenhado em +104, embaixo do botão Lighting | movido para depois de Lighting/Effects |
| "Salvar vista atual" cortado; resumo do Modelo de cena cortado | altura do painel não contava o último botão / a segunda linha | alturas corrigidas |
| Cenário de referência estourado em branco | sol em lux (100.000 lux = 146 unidades) sem exposição coerente | o modelo traz volumes de Ambiente: exposição do dia −6,8 EV e caixa do interior +4,5 EV com mistura de 1 m |
| Céu preto e sombra sem preenchimento no exterior | a exposição física escurecia céu e ambiente, que não acompanhavam o sol | **céu, ambiente e reflexo acompanham o sol autorado em lux** (razão sol da cena / sol do recurso), como o céu físico da HDRP; sol em unidade legada não muda nada |
| Interior estourado mesmo com a lâmpada certa | o céu físico entrava inteiro na sala: sem sondas, nada dizia que ali dentro o céu é oclusão | novo override **Luz indireta** no Ambiente v6 (Difuso indireto / Reflexo indireto, o Indirect Lighting Controller do Volume da Unity), misturado por volume; a sala usa 0,1% (2% ainda estourava) |
| Linha clara no pé das paredes | sombra do sol vazando em parede de 10 cm | paredes e teto do modelo com 20 cm, como parede real |
| Luzes do interior sem sombra | o modelo não ligava | o dado do modelo declara `shadows`; luz de teto e foco nascem com sombra suave |

Resultado: exterior legível com sombras do sol; interior com a lâmpada
dominando, a luz do dia entrando pela porta e **a sombra da luz de teto
projetada pela mesa** (o pé da mesa fica na sombra do tampo). A camada de
validação do Vulkan ficou ligada durante a passada e não registrou nenhum VUID.
Capturas em `docs/capturas/g6/`: `modelo-de-cena-painel.png`,
`vistas-painel.png`, `referencia-exterior-exposicao-fisica.png`,
`referencia-interior-estourado-2pct.png`,
`referencia-interior-antes-parede-20cm.png` e
`referencia-interior-sombra-luz-de-teto.png`.

A aba Malhas também foi conferida com um GLB real escolhido pelo seletor do
Android (Avocado, CC0; o arquivo foi colocado em Downloads só para o teste e
apagado em seguida): 406 vértices, 682 triângulos, UV0/Normais/Tangentes, mapa
normal, 21.585 texels/m em 2048², uniformidade 1,3× — os mesmos números do host.
O resumo do arquivo saía cortado numa linha só e passou a quebrar linha.
Capturas: `aba-malhas-avocado.png` e `aba-malhas-cartao-avocado.png`.

Ainda não medido: custo em ms do passe de sombra local (exige bancada limpa,
ver as regras de medição).

### Qualidade gráfica do editor (pedido fora do G6, 21/09/2026)

**Diagnóstico no aparelho:** a escala dinâmica derrubava o viewport do editor
para 50% (640×1384) em um segundo e ele não voltava: o orçamento era de 7,3 ms
(meta de 120 Hz) e a GPU levava ~14 ms mesmo a 50%. Além disso o aparelho cai
no nível automático "Médio" (perfil B, sem ray query/mesh shader/VRS) e não
havia onde mudar nada disso no editor.

Referências: Unity 6 — Project Settings > Quality (níveis) e URP Asset (Render
Scale, Upscaling Filter, Anti-aliasing); Godot 4 — Rendering > Scaling 3D
(Scale/Mode), Anti Aliasing e Max FPS. As duas renderizam o viewport do EDITOR em
resolução nativa.

| Entrega | Onde vive | Estado |
|---|---|---|
| Arquivo de configurações do projeto (`.astra/rendering.astra`): nível, escala de renderização, escala dinâmica, anti-aliasing, nitidez e taxa alvo. Chave futura é ignorada; valor ilegível recusa o arquivo | `native/renderer/rendering_settings_file.*` | host (3 casos) e aparelho (gravar → reabrir) |
| Padrões de editor: sem escala dinâmica e alvo de 60 Hz, salvo pedido explícito do projeto | `withEditorDefaults`, `android_main.cpp` | aparelho: `dinamica=off`, renderizando 1280×2772 nativo |
| Painel **Qualidade** no viewport: Nível (Automático/Baixo/Médio/Alto/Ultra, mostrando o detectado), Escala de renderização (50–100% em passos de 5%), Escala dinâmica, Anti-aliasing (Desligado/FXAA/TAA), Nitidez, Taxa alvo, e rodapé ao vivo "Renderizando L×A · GPU x ms" (GPU medida só com o painel aberto). Aplicar grava o arquivo e refaz o renderer | `buildQualityPanel`, `editor_session.*` | host (sessão) e aparelho |
| Refazer o renderer no ponto seguro do laço, antes do quadro | `qualityRebuildPending` | a primeira versão refazia no meio do quadro e derrubou o processo; corrigido e reconfirmado |

Resultado no aparelho com Alto + TAA + 30% de nitidez: resolução nativa, 3
cascatas de 1536, reflexo especular do ambiente, bloom e TAA, a 18,4 ms de GPU.
Comparação ampliada em `docs/capturas/g6/qualidade-antes-50pct-depois-nativo.png`
e painel em `qualidade-painel.png`.

### Gate 8.1 · estágio gráfico profundo, rodada de 21/09/2026

Esta rodada integra uma parte do gate de doze passos do plano. Ela não fecha o
gate inteiro: registra separadamente o que compilou, o que foi visto no aparelho
e o que ainda depende de autoria, API ou medição.

| Evidência | Resultado |
|---|---|
| Suíte nativa de host | **1084/1084 verdes** antes da regressão final de ordem de desenho; depois, `graphics_` **3/3** e `screen_` **28/28** |
| Ferramentas de ambiente em Python | **3/3 verdes** |
| Propriedades de atmosfera pela ABI de scripts | regressão nativa **1/1**: enum, float e bool atravessam a ponte real, chegam à coleta de ambiente em Play e preservam a autoria após Stop; não equivale a executar CLR no aparelho |
| Shaders alterados | todos os módulos SPIR-V regenerados e aprovados pelo `spirv-val` |
| Alvo Android | rebuild Release incremental em **24 s**; instalação com preservação de dados (`install -r`) concluída com `Success`; APK aberto no Xiaomi/Adreno 825 |
| Projeto de validação | cópia `GraphicsStage0921`; o projeto original permaneceu fora desta rodada |

Após reabrir `GraphicsStage0921`, as quatro abas do painel **Gráficos** ficaram sem
sobreposição. O arquivo puxado do aparelho confirmou 85% de escala,
**Catmull–Rom** e **TAA temporal**. O TAA desta etapa usa movimento de câmera;
motion vectors por objeto e por skinning continuam pendentes.

O novo céu físico foi visto no aparelho. No Inspector do componente Ambiente,
mudar **Intensidade** de 1 para 8 alterou efetivamente o céu renderizado, o que
confirma o caminho propriedade → cena → uniform → consumidor Vulkan nesse caso.
Play e Stop foram executados sem crash, e a Intensidade 8 permaneceu autorada.
O modelo implementado é **single scattering Rayleigh/Mie**. Ele não representa
o conjunto completo da atmosfera HDRP, nem inclui nuvens, fog volumétrico local
ou múltiplo scattering.

O SH9 global está integrado ao recurso de ambiente cozido e ao shader de luz
indireta, coberto pelos testes e pelo build instalado. A autoria HDRI no editor
continua pendente: `Environment`/`EnvironmentProfile` ainda não escolhem uma
fonte HDR por GUID, e o workspace de primitivas continua iniciando com a textura
neutra. Também continuam pendentes a API gráfica global C#, a comparação visual
isolada do SH9, o A/B/custo de Catmull–Rom, a medição em bancada limpa e a
equivalência pixel a pixel entre Scene View e Play.

Evidência desta passada: `build/graphics-stage/graphics-panel-final.png`,
`build/graphics-stage/graphics-shadows-final.png`,
`build/graphics-stage/graphics-post-final.png`,
`build/graphics-stage/atmosphere-view-final.png`,
`build/graphics-stage/play-final.png` e
`build/graphics-stage/device-release.log`.

#### Estabilidade temporal, atmosfera e samplers — continuação de 21/09/2026

O Release seguinte compilou, foi instalado com sucesso no Xiaomi 25053PC47G
(Adreno 825) e reabriu a cópia `GraphicsStage0921`. A causa raiz de o TAA quase
não alterar o vídeo estava em `android_main.cpp`: o laço publicava clip planes e
FOV zero e depois os valores reais em cada quadro. Os setters invalidavam
`temporalHistoryInitialized_` nas duas mudanças, então o pós recebia sempre o
modo de primeiro quadro e não acumulava. A câmera efetiva passou a ser publicada
uma única vez. Um log temporário no quadro 120 confirmou modo 3 e feedback 0,88;
ele foi removido depois da medição.

Foram gravados vídeos de tela de 4 s a 20 Mbps na vista **Exterior**, com 85%,
Catmull–Rom, TAA e 120 Hz solicitados. A análise começou após 1 s e mediu 2 s no
crop `x=850, y=650, w=780, h=430`, aplicando aos dois vídeos a mesma máscara de
7.341 pixels de borda derivada do baseline:

| Captura | Frames | Desvio de aresta | Diferença média entre frames |
|---|---:|---:|---:|
| antes da causa raiz | 120 | 7,6314783 | 10,4111462 |
| Release final | 118 | 2,1526747 | 1,9744731 |

A diferença entre frames caiu aproximadamente 81%. Os números vêm de níveis de
cinza 8-bit de vídeo H.264 e comprovam somente a estabilidade desse recorte
estático; não são tempo de GPU, não avaliam objetos móveis e não substituem as
três cenas nem a bancada limpa. Vídeos, frames e script ficam em
`build/graphics-stability/`, inclusive `taa-exterior-final.mp4`,
`exterior-final.png` e `final-device.log`.

O cache CPU de irradiância do solo físico foi integrado ao CMake e ao frame UBO
de 3328 bytes, no offset 3312. O filtro `physical_atmosphere` passou 5/5; no
aparelho, `exterior-before.png` mostra a porção inferior preta e
`exterior-after.png` mostra o solo iluminado. O modelo continua single scattering
Rayleigh/Mie e não passa a ser Bruneton, Hillaire ou HDRP completo. O efeito
visual foi validado no aparelho; o custo GPU da atmosfera não foi medido.

Os samplers glTF agora preservam independentemente minificação, magnificação,
uso de mip e modo entre mips conforme a
[especificação glTF 2.0 da Khronos](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-sampler),
mantendo flags legados. Cooker e consumidores de autoria/pacote convergem no
mesmo decode. Seis modos e pacote passaram nos testes focados; a regressão de
reabertura do editor e três filtros focados também passaram. A revisão do cache
de importação subiu de 4 para 5 para reconstruir derivados automaticamente na
próxima abertura sem apagar fontes. Pacotes `.aemap` já cozidos com os bits
antigos precisam ser recozidos da fonte; os bits antigos não permitem recuperar
a escolha original. O teste focado do cache passou 1/1 e o Release com a revisão
5 compilou em 24 s, foi instalado com `Success` e reabriu vivo (PID 19040). O
código gráfico medido no vídeo final não mudou nesse rebuild posterior. Esta
rodada não fez A/B visual de cada modo no aparelho.

O fechamento para ausência de histórico também chegou ao Release final: o corte
de câmera é decidido antes de gerar jitter, e frames invalidados usam jitter
zero com FXAA espacial antes de formar uma nova base de histórico. O build
incremental final passou em 13 s, foi instalado após remover o log temporário,
Play/Stop foi exercitado, o céu foi observado nos dois modos e o aplicativo
permaneceu vivo (PID 15010). Não houve manipulação de objeto móvel, portanto
essa observação não valida estabilidade de movimento por objeto.

Motion vectors por objeto/skinning, máscara reativa, autoria HDRI por GUID,
probes locais, pós temporal separado e os demais itens do gate 8.1 continuam
pendentes. `textureQualityHalf`/`residencyMipBias` hoje só chegam ao estado/log e
não alteram residência de textura; permanecem pendentes como consumidor real.
Também não houve equivalência pixel a pixel entre Scene View e Play. Esta
evidência não fecha o estágio gráfico AAA.

#### Recursos HDRI, API gráfica e histórico isolado — 21/09/2026

Esta continuação substitui as pendências de implementação de autoria HDRI,
`textureQualityHalf` e histórico separado citadas no fechamento anterior:

- `EnvironmentMap` agora é recurso do AssetRegistry. Radiance HDR 2:1 é
  importado/reimportado com receita editável, cancelamento, limites e cache por
  conteúdo. Panorama RGBA16F/mips, GGX octaédrico, BRDF e SH9 são derivados da
  mesma fonte. Environment v8 e Profile v4 guardam GUID, rotação e exposição.
- Inspector, perfis e API usam o mesmo binding. Troca de HDRI é discreta entre
  volumes do grupo Céu. OpenEXR e mistura de dois panoramas seguem ausentes.
- `Astra.Graphics` pela ABI 6 permite solicitar os eixos da política gráfica,
  consultar capacidades e distinguir pedido de aplicação confirmada. O estado
  é de Play; Stop restaura autoria. Scripts compilados contra a ABI antiga são
  recusados pela identidade do compilador e precisam ser recompilados.
- Half tem consumidor de residência nos uploads de pacote/autoria. Cadeias
  preservam mips válidos; RGBA8 de um único mip recebe redução com tratamento
  sRGB; comprimidas sem nível reduzido são recusadas. HDRI/LUT ficam fora disso.
- TAA grava histórico em attachment próprio antes de nitidez/grão. O histórico
  usa a grade estável do resolve, não a rasterização bruta anterior. Profundidade
  publica EARLY e LATE fragment tests. Grão tem a mesma posição display-linear
  em sRGB e UNORM. Existe custo de um attachment adicional ainda não medido.

Validação até esta integração: host completo 1100/1100; testes focados de
importação/cache e commit/reopen/binding HDRI; duas fontes HDR reais de 1K/2K
preparadas em cerca de 0,7 s no host; shaders aprovados por spirv-val; C# de
consumo da API compilado; APK Release arm64 compilado. Esses resultados não
equivalem a observar esta versão no aparelho. A passada ADB é feita por último.

**Passada ADB final deste bloco:** o Release incremental final compilou em 18 s
e foi instalado com `Success` no Xiaomi 25053PC47G/Adreno 825. A validação usou
somente a cópia `GraphicsApi0921`. O picker importou a fonte real
`hausdorf_clear_sky_1k.hdr`; a prévia alterou GGX de 128 para 256 amostras,
repreparou os derivados e bloqueou publicação durante o trabalho. O registro
puxado do aparelho contém `AEM_IMPORT 1 1024 256 128 256 512`. A escolha do GUID
alterou céu e iluminação no viewport; exposição 0/7/4 EV e rotação 0/90° tiveram
efeito observado. A cena já usava exposição global -6,8 EV; a exposição HDRI
é uma escala da fonte, não uma substituição da exposição de câmera.

A reabertura registrou uma fonte, um cache reaproveitado, zero falhas e 99 ms
no trecho de reabertura (86 ms preparo, 10 publicação, 3 cena; não é tempo de
GPU nem do cold start inteiro). O script foi compilado pelo editor no Android
depois de corrigir o `ComponentId` ausente no próprio smoke. Em Play, a chamada
C# → ABI → política → rebuild → republicação retornou:
`success=True available=True scale=0.85 aa=Temporal textureBias=1 rotation=45`.
Após Stop, o log confirmou mip bias 0 e anisotropia 8 novamente. O componente
Environment serializado permaneceu idêntico ao anterior, incluindo HDRI em
90°/4 EV; `rendering.astra` permaneceu byte a byte idêntico. O processo continuou
vivo, PID 12142, sem fatal no log coletado. Esta execução não ativou validation
layers, portanto ausência de VUID no log não é aceite de validação Vulkan.

Evidências em `build/graphics-api-validation/`: `hdri-preview.png`,
`hdri-reprepare.png`, `hdri-rotation-90.png`, `reopened-hdri.png`, `api-play.png`,
`api-stopped.png`, fontes/arquivos autorais puxados e `final-device.log`.
A cena de caixas não tem biblioteca de texturas materiais representativa: ela
confirma a aplicação da política Half e a restauração, mas não mede a economia
de residência. O histórico separado apresentou imagem com TAA ativo; não foi
feito novo A/B temporal ou benchmark do attachment adicional. Preview de outra
câmera ainda compartilha a resolução de ambiente da câmera principal.

Permanecem abertos no pedido integral: FSR/ASR, motion vectors por objeto e
skinning, probes locais, exposição automática/LUT/saída HDR, atmosfera
volumétrica/nuvens, decals/materiais avançados, gizmos restantes e os aceites
G6-C/D/E. O bloco não declara esses recursos implementados nem fecha o plano G6.

## 23/09/2026 — texturas, exposição e autoria espacial

- Importação direta PNG/JPEG/KTX2, receita por GUID, gerenciador e prévia com
  canais/mips, Aplicar/Reverter e páginas acessíveis. Perfil v3 acrescenta cobertura
  alfa. Aplicar prepara os dados, valida os bindings publicados e só então grava;
  uma recusa preserva receita/registro/histórico. Undo/Redo recusa fonte externa
  divergente. Materiais em Play expõem UV/sampler por binding via ABI 7.
- FSR 1 EASU/RCAS e AgX foram integrados, com licenças empacotadas. A neblina
  analítica por altura evita cancelamento numérico em grandes diferenças de
  altitude. TAA agora inclui roll na reprojeção da câmera; continua sem vetores
  de movimento por objeto/skinning.
- Exposição automática por histograma GPU: percentis, cinza alvo, limites EV,
  velocidades separadas, compensação e estado por vista. Ambiente v11/perfil v7
  migram documentos antigos com o recurso desligado. A prévia resolve seu próprio
  Ambiente/HDRI e informa recursos indisponíveis; ainda não tem sombras/água próprias.
- Alças de alcance/cones de luz e dimensões/raio/mistura dos volumes de Ambiente
  editam os descritores existentes e agrupam o arraste em um único Undo. Luz usa
  quadro óptico sem escala; dimensões do volume seguem sua transformação e a
  mistura é medida em metros de mundo. Não foram criados probes sem consumidor.
- Medição separa prévia, sombras locais, exposição, EASU e RCAS. O relatório
  Android v8 divide os 15 passes em três registros para evitar truncamento pelo
  Logcat. Os leitores exigem todas as partes e recusam duplicatas; capturas
  antigas mantêm as métricas disponíveis, sem preencher ausências com zero.

Referências da autoria espacial: [Light da Unity 6, Built-in](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html)
para alcance/cone; [gizmos da Godot](https://docs.godotengine.org/en/stable/classes/class_editornode3dgizmoplugin.html)
para edição de handles com confirmação/cancelamento; [volumes da Unreal](https://dev.epicgames.com/documentation/unreal-engine/post-process-effects-in-unreal-engine)
para distância de mistura em unidades de mundo. A Astra mantém os meios-ângulos
já usados pelo próprio schema e o histórico de comandos existente.

Validação host: build incremental `aether_tests` e builds Release da API managed
e do consumidor `GraphicsApiSmoke` concluídos. Passaram os filtros de materiais,
ABI, ambiente/exposição/perfis, textura/cobertura alfa, Qualidade e estatísticas
de GPU. Após o ajuste final de atomicidade e das alças, passaram os oito filtros
focados de gizmos, câmera, bindings, receitas e recusa de publicação/Undo.
O leitor do profiler passou 56 testes PowerShell e os casos de fragmentação do
consumidor Python; o objeto NDK correspondente compilou.
No Xiaomi/Adreno 825, o Release desta primeira passada importou JPEG pelo seletor,
aplicou a textura a uma primitiva e republicou a receita de 512 para 1024 px.
Undo restaurou 512 px e o registro de assets byte a byte; Redo restaurou 1024 px.
O painel Gráficos preservou 90% e TAA ao reabrir o painel e ao reiniciar o aplicativo.
A alça de alcance de luz alterou 10 para 42,593 m; um Undo voltou a 10 m, e o
Undo da mudança de modalidade restaurou o arquivo da cena byte a byte.

A passada encontrou e corrigiu três falhas: escolhas rápidas na receita podiam
perder o último preparo; o Inspector de Malha repetia suas abas; a reconstrução
de qualidade em Play perdia texturas de primitivas. A repetição no Release de
23/09 confirmou a última receita pronta após mudanças rápidas, cancelamento
seguido de nova importação, abas únicas e textura/miniatura preservadas após
reconstrução. A neblina com cor limitada a 0–1 revelou outra lacuna: sob exposição
−6,8 EV, os raios de céu saturados pela densidade aparecem quase pretos. Desligar
somente a neblina recuperou o panorama, confirmando a incompatibilidade entre
sua intensidade limitada e a exposição usada.
O componente v12 e o perfil v8 agora expõem `fog_light_energy` (0–65504),
multiplicando a cor linear antes da exposição. Cenas antigas conservam energia 1.
A autoria segue a separação entre cor e energia da
[Godot 4.4](https://docs.godotengine.org/en/4.4/classes/class_environment.html#class-environment-property-fog-light-energy);
é neblina analítica configurável, sem afirmar dispersão volumétrica.
No Xiaomi, a API aplicou energia 100 mantendo −6,8 EV e a névoa passou a ser
visível sem a faixa preta; o cubo manteve sua textura depois do rebuild de
qualidade. A calibração artística da densidade/altura/energia continua autorável.
A exposição automática foi observada numa transição controlada: mantendo a cena
e 0 EV manual, o script ativou a medição após oito segundos; a imagem superexposta
recuperou os tons. `exposure-manual-zero.png`, `exposure-auto-settled.png` e
`auto-transition.log` em `build/graphics-api-validation/0923` guardam a evidência.
O SHA-256 da cena antes/depois do Play permaneceu
`05C6106929E80BF75FBD161E0E0408D5E4186EABAEC53CF887F1340E2A0D2387`.
O script, o mapa de entrada temporário e a política gráfica do projeto de teste
foram restaurados; o modo de jogo Android voltou ao `performance` original.
O leitor v8 consumiu os 15 passes do log real, incluindo exposição/EASU/RCAS.
Os números dessa execução não são uma bancada de desempenho: a cena estava com
recursos ausentes, `game_mode=2` e GameTurbo não foi verificado como desligado.

O pedido integral segue aberto nas linhas explícitas de §9.1 do plano universal,
incluindo probes locais, reconstrução temporal com movimento por objeto, LUT/HDR
de saída, volumetria/nuvens, materiais avançados e G6-C/D/E. O inventário de G6-C
também identificou ausência de save-game de runtime nesse caminho e nenhum backend
de áudio localizado nos diretórios próprios consultados. Essas são implementações
restantes, não verificações que um APK compilado possa encerrar.

Fatia G6-C, entrada física: `handleInput` traduz `AKeyEvent` e eventos de joystick
para o `InputDeviceState` consumido pelo mesmo `InputActionMap` da cena em Play.
Bindings `Key` e `GamepadButton` usam os key codes Android; `GamepadAxis` 0–7
representam esquerda X/frente, direita X/cima, gatilhos esquerdo/direito e
D-pad X/cima. O último teclado e o último gamepad ativos têm posse do estado;
esta fatia não define jogadores múltiplos. Pausa, perda de foco, Stop, editor
de código visível e desconexão soltam teclas/botões/eixos. A adaptação segue as fontes da
[Unity 6 Input System 1.17](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.17/manual/ActionBindings.html),
os [eventos de controle Android](https://developer.android.com/games/sdk/game-controller/controller-input)
e a [zona morta da Godot](https://docs.godotengine.org/en/stable/tutorials/inputs/controllers_gamepads_joysticks.html).
O teste do helper passou no host. No aparelho, uma tecla E injetada por ADB
atravessou Android → mapa autorado → script C#, registrando `JustPressed` e
`JustReleased`. Isso confirmou a correção do foco: suporte ao IME não significa
campo em edição. Entrar em Play encerra a solicitação de texto autoral e recusa
respostas tardias. Teclado físico e gamepad físico ainda não foram verificados.

ABI 8 acrescenta comandos de Character e CameraLook. O teste
`play_script_character_commands_cover_all_substeps_and_preserve_authorship`
passou com física real no host: mesma distância em 30/60 Hz, expiração da intenção
no quadro seguinte, salto no chão/recusa no ar, limite de pitch, preservação de
roll e documento autoral. O runtime managed nesse teste é um duplo que chama a
ABI; isso não substitui a execução CLR dos comandos no aparelho.

Fechamento desta fatia: `environment` 17/17, `auto_exposure` 2/2 e os filtros
de foco do Play, comando de Character, reidratação, entrada Android e exclusão
de recurso passaram no host. A exclusão de HDRI/perfil não exige republicar
geometria; Mesh/Material/Texture exigem o publicador antes da remoção do arquivo.
O Release final compilou com `:app:assembleRelease` em 29 s. Esses resultados
fecham as regressões encontradas nesta validação, não o restante do plano G6.
