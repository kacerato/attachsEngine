# Colisão decomposta: U04 / U05 / U09 / U11 / U14

Repositório alvo: https://github.com/kacerato/attachsEngine. Esta entrega amplia a cadeia universal existente; não substitui nem encerra o roadmap UI original. O usuário ativou o ADB durante a rodada; evidência física, host e publicação são registradas separadamente em `docs/validacao/ui-universal-2026-10-05/convex-parts.json`.

## Referências e decisões

- Godot **4.5**, [MeshConvexDecompositionSettings](https://docs.godotengine.org/en/4.5/classes/class_meshconvexdecompositionsettings.html) e [MeshInstance3D](https://docs.godotengine.org/en/4.5/classes/class_meshinstance3d.html): separar geração de cascos, seus orçamentos e os recursos de shape. Os parâmetros dessa versão da Godot não são aliases dos parâmetros V-HACD 4; em particular, concavidade/distância não equivale ao erro percentual de volume.
- Godot **4.5-stable**, [editor do MeshInstance3D](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/mesh_instance_3d_editor_plugin.cpp): ação contextual sobre a malha, criação de formas físicas distintas do recurso visual e histórico da alteração.
- [V-HACD 4](https://github.com/kmammou/v-hacd), commit `f900e42361491f525262d4825e758845e4969897`, BSD-3-Clause: biblioteca real, header original sem alterações. `maximumParts`, resolução voxel, limite de vértices e erro de volume são encaminhados ao algoritmo. O solver continua Jolt **5.6.1**; V-HACD executa somente na autoria.
- Uso real pesquisado: [projeto e tutorial Collision 3D Modular do Xogot](https://github.com/xogot-projects/Xogot-Collision). É referência de organização de malha/colisão e descoberta contextual em editor mobile. O vídeo foi localizado; não há alegação de análise quadro a quadro nem de teste do Xogot.

NÃO VOU IMPLEMENTAR DEPENDÊNCIAS DE FORMA CENOGRÁFICA. A engine já possui AssetRegistry, importação GLB com mapa de identidades, publicação transacional, múltiplos Collider por Body, Inspector por instância, solver e Undo. A expansão usa essas dependências reais.

## Cadeia implementada

`Malhas de todos os slots do objeto → coordenadas locais reais → validação de sólido → worker V-HACD → prévia das partes → candidata autoral → preflight Jolt → GLB separado + mapa de GUIDs + registro no journal → Collider por parte + Body/Motor → Undo → arquivo de cena → reabertura de recursos → Play`.

O objeto conserva identidade, malha, materiais, transformação e filhos. Cada casco usa um GUID de Mesh próprio dentro de um GLB dedicado em `Collision/bake-<SHA256>.glb`. Metadados do GLB guardam revisão do algoritmo, parâmetros, hash da geometria e GUIDs de origem. O registro guarda dependências das fontes importadas; reabrir/reimportar conserva essas dependências. O mapa de importação existente persiste identidade de cada parte. O bake é uma revisão imutável, não uma receita que destrói edições automaticamente.

Aplicar altera somente o objeto em uma entrada de Undo. Desfazer restaura os componentes anteriores; o recurso registrado permanece disponível para Redo, compartilhamento e outras cenas. Remoção de recurso continua sendo uma ação explícita com verificação de uso. Falha da publicação do consumidor gráfico/disco restaura registro, biblioteca e cena pelo journal existente.

O worker possui apenas seus buffers, resultado e estado de progresso; não toca documento, UI, registro, solver ou GPU. Um worker por sessão, subparalelismo V-HACD desligado; destruição pede cancelamento e faz join. Cancelamento e prazo são cooperativos nos callbacks da biblioteca, não uma promessa de interrupção instantânea. Seleção, revisão, epoch, projeto e hash da geometria são conferidos para impedir aplicação de resultados antigos.

A preparação da candidata é separada do preflight físico. **Gerar** não cozinha primeiro um casco único nem inicia a física da cena inteira: isso bloquearia a interface e poderia recusar uma malha justamente antes de decompô-la. **Aplicar** valida a cena física completa com as partes reais antes de publicar ou alterar o objeto. Preservar/Ajustar/Convexo continuam validando suas prévias pelo backend existente.

## Propriedades e compatibilidade

`Collider` formato **v8** acrescenta `mesh_local_pose`. Desligado preserva o comportamento legado de v1–v7: centros/rotações guardados, mas inativos para Mesh. Ligado aplica pose independente ao shape. Os cascos gerados vêm com a opção ligada e pose zero.

Centro, rotação, ativo, recurso, Convexo e tolerância são propriedades reais de cada instância, persistidas e consumidas pelo solver. A transformação de Mesh inclui a matriz afim completa, inclusive rotação local sob escala não uniforme; a geometria física recebe essa matriz, sem decomposição TRS que eliminaria shear. Primitivas mantêm suas restrições geométricas existentes. O outline do editor aplica a mesma pose autoral; consultas identificam a instância Collider atingida. SDK é derivado do contrato nativo.

## Workflow

NÃO IREI SER SIMPLISTA NO DESIGN.

Selecionar objeto → ações do Inspector → Configurar locomoção → Decompor → orçamento → Gerar prévia. O viewport permanece visível e mostra os cascos com cores distintas; desligar uma parte torna seu contorno cinza. A lista paginada muda o estado das partes antes da aplicação. Depois de aplicar, cada parte é um Collider editável pelo Inspector existente, com seu outline ao selecionar a instância.

A geração ocupa a superfície contextual de ações, não um painel permanente. O novo conceito possui ícone próprio `component/convex-parts`, produzido e integrado ao atlas real por `generate-convex-parts-icon.py` + `pack-icon-atlas.py`. Proposta estrutural adotada: revisão física separada do visual, com prévia no viewport e edição por instância; não substituir o objeto por um jogador de forma fixa.

Em superfícies abaixo de 600 px de altura lógica, a prévia pronta prioriza uma parte por página; todas permanecem acessíveis. Os orçamentos ficam na etapa de geração, acessível por Voltar. Em superfícies maiores, são três partes por página. A navegação é reservada antes do conteúdo para não desaparecer. Etapas da biblioteca são traduzidas para ações legíveis na interface. O exemplo autorado está em `examples/ui/ConvexParts`.

GUI: Leve (até 8 partes, 50 mil voxels, 32 vértices/casco), Equilibrado (16 / 100 mil / 32), Detalhado (32 / 400 mil / 64). Erro de volume 1%; prazo cooperativo 60 s nos dois primeiros e 120 s no detalhado. API `ConvexBakeSettings` permite valores intermediários e erro de volume de 0,1 a 10%. O erro de volume não garante preservar cada cavidade: inspecionar a prévia é obrigatório.

## Limites reais / trabalho restante

- Entrada até 100 mil triângulos; máximo 32 partes para respeitar 64 componentes por objeto. O limite de 256 partes por Body do solver continua válido para compostos com descendentes. Não há alegação de suporte a geometria arbitrariamente grande.
- Exige malha fechada e orientada depois de soldar coordenadas iguais. Malhas abertas, não manifold, degeneradas ou com recurso ausente recebem erro; não viram uma caixa silenciosa.
- A ampliação [U05](U05-FONTES-HIERARQUIA-2026-10-05.md) adiciona fontes explícitas por objeto/slot/descendente, inclusive destino sem renderer. Edição de vértices/cortes, picking de parte diretamente no viewport e geração a partir de skin deformada continuam pendentes.
- Composto autorado existente é preservado e sua substituição automática é recusada. Regeneração com correspondência entre partes e manutenção de overrides/prefab continua U05/U09 aberto.
- Campos numéricos avançados de bake na GUI ainda não existem; presets funcionais e API tipada são a cobertura atual.
- Centro de massa/inércia customizados, novos motores/modos, apoio por manifold, rede e reprodução continuam nos pacotes correspondentes. Os novos cascos não encerram esses sistemas.
- Medição de pico de memória/thermal e cancelamento de entradas grandes em mobile continuam pendentes. O cenário físico desta rodada cobre geração, prévia, aplicação, Undo/Redo, reabertura e controles sequenciais; não cobre toda malha, escala, gestos simultâneos ou orçamento.

VOU TESTAR O QUE PROTEGE COMPORTAMENTO REAL. Evidência final em `docs/validacao/ui-universal-2026-10-05/convex-parts.json`: 76/76 cenários host, SDK sem erros/avisos, UI nativa em 800×400 com todas as páginas acessíveis, build e hash da instalação Android. No Xiaomi 25053PC47G, a revisão final gerou oito partes, alternou estado, aplicou 11 componentes no ator, desfez/refez e reabriu a cena salva com bytes idênticos. Play partiu de X=0/Y=0,49; joystick levou X a 1,97, salto elevou Y a 1,94, e impulso levou X a 2,24 com seu contador atualizado. São observações sequenciais do cenário U, não aceitação universal.

`examples/ui/ConvexPartsReady` entrega essa cena jogável, seus GLBs, registro e mapas de identidade produzidos pelo editor; `examples/ui/ConvexParts` conserva o ponto de partida sem colisão do ator. Alterações desta entrega estão locais, ainda sem commit/push. Os pacotes universais seguem parciais.
# Atualização posterior — 2026-10-06

Os estados abaixo pertencem à entrega original. O [bloco de autoria física durável](BLOCO-AUTORIA-FISICA-2026-10-06.md) acrescenta skin/morph/clipe reais, parâmetros completos, cache e budgets medidos e está aceito dentro dos contratos publicados. O Jolt incluído foi confirmado como **5.6.0**, corrigindo eventual indicação anterior de 5.6.1. Pose de colisão é snapshot estático, erro de volume não garante cavidades arbitrárias e deadline é cooperativo. [Relatório/limites/aceites](../../validacao/ui-universal-2026-10-06/physical-authoring/REPORT.md). U14 thermal/custo amplo permanece próprio.
