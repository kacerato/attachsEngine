# U11 — seleção de superfícies das partes de colisão

Repositório confirmado: [kacerato/attachsEngine](https://github.com/kacerato/attachsEngine). Continuação da seleção por contornos; U11 e o roadmap universal permanecem parciais. Nesta rodada o usuário informou que não há ADB: nenhum comando ADB, instalação ou captura física integra este aceite.

## Problema e referência

Uma forma aberta no Inspector só podia ser escolhida perto das linhas do overlay. Tocar no centro de uma face voltava à seleção da malha visual, mesmo que a colisão estivesse numa raiz sem renderer. O novo comportamento permite escolher a superfície do Collider sem converter sua aparência ou trocar o proprietário.

Godot **4.5**, [EditorNode3DGizmo](https://docs.godotengine.org/en/4.5/classes/class_editornode3dgizmo.html#class-editornode3dgizmo-method-add-collision-triangles), distingue linhas desenhadas e triângulos de seleção; o mesmo documento descreve seleção por raio com identidade de subgizmo e histórico de edição. Godot **4.5-stable**, [CollisionShape3DGizmoPlugin](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/physics/collision_shape_3d_gizmo_plugin.cpp), usa a forma do recurso para fornecer contornos/handles, tratando cápsula e cilindro separadamente. Princípio extraído: selecionar a geometria autorada e conservar a identidade da parte. Não foi copiado código da Godot.

Pesquisa de uso visual: [análise anterior de 100 quadros consecutivos](ANALISE-VIDEO-COLLIDERS-2026-10-05.md) mostra a separação entre seleção de peça e raiz. Isso não demonstra este novo algoritmo de superfícies. Não foram analisados novos quadros nesta rodada; o restante do vídeo e o vídeo de World Space continuam pendentes.

## Caminho dos dados e implementação

`toque curto elegível → raio da câmera/intervalo próximo-distante → pose mundial + pose local do Collider → superfície de cada parte → profundidade mínima + ID persistente → Inspector existente → edição/histórico/arquivo/solver existentes`.

- `EditorPickMesh::localRay` compartilha a inversa afim já usada pelo BVH. Não normaliza a direção local, preservando comparação de profundidade em unidades mundiais sob escala não uniforme e shear. Transformações singulares/não finitas são rejeitadas.
- `intersectColliderPrimitive` intersecta caixa, esfera, cilindro finito com tampas planas e cápsula com hemisférios. As junções internas da cápsula não viram faces selecionáveis. Raios iniciados dentro da forma ou com entrada cortada pelo plano próximo podem encontrar a superfície de saída dentro do intervalo visível.
- `EditorMapScene::intersectColliderMesh` usa os triângulos completos da fonte para Mesh não convexa; para convexa usa os triângulos do casco efetivamente produzido pelo cooking de prévia existente. A cavidade da malha em U continua vazia no modo triangular e é preenchida pelo casco único no modo convexo.
- O BVH de triângulos reutiliza o cache de recursos. O BVH do casco nasce sob demanda e pertence à entrada do cache de cooking existente, limitada a 32 entradas. Troca de biblioteca/pacote invalida essas entradas; tolerância e slots fazem parte da chave. Não existe cooking/simulação nova no tick de gameplay.
- Malha física explícita ausente não consulta a visual como substituta. Fonte secundária ausente recusa a consulta inteira, sem selecionar uma forma incompleta. A consulta é do editor; diagnósticos autorais de recurso continuam no Inspector existente.
- `pickColliderSurface` consulta somente a coleção do objeto ativo. Componentes indisponíveis são rejeitados pelo descriptor executável real. O Collider desativado continua selecionável para edição; isso não o reativa no solver.
- A menor profundidade ganha; coincidência exata desempata pelo menor ID persistente, independente da ordem da coleção. Planos próximo/distante e retângulo do viewport são respeitados. O BVH foi ampliado para considerar o intervalo durante a busca, podendo ignorar uma face cortada e encontrar outra visível.
- O roteamento tenta superfície primeiro, depois o contorno com tolerância existente de 8 pixels lógicos quando não há superfície atingida. Gizmos, navegação, Cancel, multi-touch, seleção múltipla, GUI, camadas e objetos ocultos/bloqueados mantêm as condições existentes. Sem Collider aberto ou com visualizações desligadas, o fluxo normal de seleção de objeto permanece.
- Escolher uma parte altera somente o foco de componente/status e limpa filtros antigos. Nenhum dado de cena, recurso, histórico ou formato é modificado pelo toque. Alterar propriedades depois usa os consumidores, histórico e serialização reais do Collider; não foi criada API de gameplay/ABI nova.

## UX e captura executável

**NÃO IREI SER SIMPLISTA NO DESIGN.** Mantemos o viewport como superfície contextual de escolha, com foco branco, demais partes cinza e o Inspector existente. A instrução agora diz “toque na forma para trocar”, e o status distingue superfície de contorno. Não acrescenta painel, modo ou ícone de conceito novo; o Collider já possui ícone no atlas executável.

As capturas de 1280×720 e 800×400 vêm do executável nativo com assets reais da UI, após um toque no interior da segunda forma que abre seu Collider sem alterar o arquivo/Undo. A primeira captura compacta revelou que a instrução passava sob a barra vertical; a revisão desloca a instrução em viewports curtos, mantendo os alvos de toque livres. Capturas finais: [1280×720](../../validacao/ui-universal-2026-10-05/surface-picking/host-1280.png) e [800×400](../../validacao/ui-universal-2026-10-05/surface-picking/host-800.png).

O rasterizador de software desenha a UI e os contornos projetados reais, mas não renderiza a cena Vulkan. Estas imagens demonstram legibilidade, layout e estado do Inspector no host; não demonstram oclusão da cena, desempenho mobile nem aceitação física Android. Não foi necessária imagem conceitual para uma nova superfície genérica: a correção foi aplicada e recapturada na UI existente.

## Aceite e limites

O registro em [collider-surfaces.json](../../validacao/ui-universal-2026-10-05/surface-picking/collider-surfaces.json) separa build host, cenários direcionados, regressão de UI/autoria, capturas, pacote Android e aceite físico pendente. Os cenários incluem cápsula/cilindro distintos, profundidade, recorte, escala/shear, cavidade, pose local, fonte ausente, troca real de pacote, toque/Inspector, ausência de mutação ao selecionar, Undo, serialização e efeito da edição no Jolt.

Resultado registrado: build host concluído, **4/4 cenários direcionados** (2 de superfícies e 2 de contornos/histórico/solver) e **82/82 cenários de UI/autoria**. Capturas executáveis nas duas resoluções passaram pelo toque real. Android Release/RelWithDebInfo compilado: SHA-256 `aed8cb8b3c32ffce290c87786e911f8df614f241622657bb504066cdf6bc5fd0`. Esse é um pacote candidato, sem hash instalado ou aceite físico nesta rodada. A próxima conferência deve instalar esse hash e percorrer o roteiro registrado no JSON. Fontes de desempenho protegidas: **268, zero alterações**; sem comparação nova de FPS/memória/thermal. Alterações locais, sem commit/push.

É seleção da **superfície autorada** entre Colliders do objeto ativo, seguida de contorno do overlay. Não é query de contato do solver e não altera as restrições de escala/cooking/autoridade do Jolt. Não considera oclusão por outros objetos visuais, seleciona vértices/faces individuais ou escolhe componentes de outro descendente. O fallback de contorno conserva o comportamento sem oclusão do overlay. Partes coincidentes continuam acessíveis pelas setas do Inspector.

Continuam abertos: ownership entre descendentes, centro de massa/apoio, skin, regeneração com overrides/prefab/reimport, reconstrução dinâmica, orçamento medido para malhas muito grandes, gestos e aceite prolongado no aparelho. O roadmap original permanece integral, com 140 planejados, 20 parciais e 4 completos. Não há declaração de paridade nem fechamento da engine.

## Aceite físico posterior — 2026-10-06

O APK acima foi instalado no Xiaomi 25053PC47G/onyx e seu hash instalado corresponde ao pacote. Toque na superfície abriu o Collider 12; desativação pelo Inspector alterou o arquivo; Undo + Save e reabertura fria restauraram o SHA original da cena. Com overlays desligados, o mesmo toque selecionou a malha SecondInstance normalmente. Capturas e hashes: [acceptance.json](../../validacao/ui-universal-2026-10-06/surface-device/acceptance.json). Este aceite substitui a pendência física dessa revisão, sem atribuir ao aparelho os testes de solver realizados no host ou declarar gestos simultâneos/soak validados.

A ampliação posterior do contexto para Body/filhos tem implementação e evidências próprias: [U11 — corpos e filhos](U11-CORPOS-E-FILHOS-2026-10-06.md).
