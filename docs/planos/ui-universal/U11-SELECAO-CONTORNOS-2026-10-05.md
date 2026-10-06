# U11 — escolher Collider pelo contorno no viewport

Alvo confirmado: [kacerato/attachsEngine](https://github.com/kacerato/attachsEngine), continuidade do recurso de colisão composto de U04/U05. U11 permanece parcial.

Este documento registra o primeiro pacote de contornos e seu aceite físico. A continuação [U11 — superfícies](U11-SUPERFICIES-2026-10-05.md) amplia o toque para o interior da forma e ordenação por profundidade; seu aceite no Android fica separado e pendente. As limitações abaixo são históricas desse primeiro pacote.

## Problema e referências

Depois de gerar várias partes, a coleção e as setas do Inspector permitiam editar cada Collider, mas faltava reconhecer a parte tocada na geometria. Selecionar a malha visual devolvia o renderer, inclusive quando Body/Colliders estavam numa raiz sem malha.

A [análise visual](ANALISE-VIDEO-COLLIDERS-2026-10-05.md) registra 100 quadros consecutivos de seleção de peças/raiz no vídeo oficial da Unity. O vídeo não prova APIs internas. A referência atual é Unity **6000.0**, [compound colliders](https://docs.unity3d.com/6000.0/Documentation/Manual/compound-colliders-introduction.html), e Godot **4.5-stable**, [CollisionShape3DGizmoPlugin](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/physics/collision_shape_3d_gizmo_plugin.cpp): a forma física fornece as linhas e os segmentos usados para seleção. Usamos esse princípio com os providers de contorno já existentes, sem copiar código ou introduzir outro sistema físico.

## Arquitetura e interação

**NÃO IREI SER SIMPLISTA NO DESIGN.** O viewport funciona como superfície contextual para escolher a parte; o Inspector existente continua sendo a superfície de propriedades. O foco de componente muda, a seleção do objeto permanece. Isso evita uma segunda árvore de partes, mantém viewport dominante e é reversível dentro de três arquivos de editor.

`Collider aberto → toque curto no viewport → elegibilidade de objeto/camada → linhas da forma/recurso → mesma projeção/recorte do desenho → ID persistente da parte → Inspector existente → propriedade → histórico/persistência/runtime existentes`.

- `editor_component_visuals.h::pickColliderContour` consulta apenas os Colliders do objeto selecionado. Primitivas usam centro/rotação e a hierarquia completa; Mesh usa o recurso próprio/slots e a prévia real de hull já existente. Não seleciona uma esfera/AABB substituta do renderer.
- Tolerância de toque: 8 pixels lógicos ao redor da linha projetada. Segmentos passam pelo recorte de câmera/viewport existente. Empate exato é resolvido pelo menor ID persistente, independente da ordem da coleção.
- A identidade do descriptor deve ser a do Collider executável. Um registro indisponível preservado com o mesmo nome não é convertido/cast para Collider nem ganha contorno falso.
- `EditorSession::handleViewportPointer` intercepta apenas o toque curto com um Collider aberto, visualizações ativas e seleção única. Pan/orbit/pinch, Cancel, gizmos, seleção múltipla, GUI, câmera inspecionada, camadas ocultas/bloqueadas e objeto desativado mantêm seus contratos.
- O toque troca `expandedNative`, limpa filtros/página/grupo antigos e identifica a instância no status. Não altera cena, Transform, recursos, histórico ou formato de arquivo.
- O contorno em foco usa branco e as demais partes ficam em cinza atenuado, pelo ID usado no Inspector. Capturas reais revelaram que o verde de seleção anterior era quase igual ao verde dos demais contornos. Uma instrução discreta aparece acima dos controles de navegação somente nesse contexto, com fundo para legibilidade; foi reposicionada após capturas mostrarem sobreposição com status e navegação. O ícone existente de Collider é reutilizado: não surgiu um novo tipo/conceito de recurso.
- A edição posterior usa os controles reais do componente, com Undo/Redo e serialização existentes. Foco de Inspector é estado temporário do editor, não dado de gameplay.

## Limites explícitos

É seleção de **contorno do overlay**, com o mesmo comportamento sem teste de oclusão das linhas já desenhadas. Não é raycast físico, seleção por superfície ou escolha da parte mais próxima em profundidade. Não captura o interior sem linhas. Malhas grandes continuam usando os segmentos limitados da prévia existente; partes exatamente coincidentes continuam acessíveis pelas setas do Inspector.

O pacote opera nos Colliders do objeto selecionado, incluindo partes U04/U05 armazenadas na raiz do Body. Escolher Collider de outro descendente e apresentar owner/centro de massa/apoio no contexto exige a próxima ampliação de U11. Não encerra skin, reimportação/regeneração com overrides, medidas térmicas ou o roadmap original.

## Validação

Registro final: [collider-contours.json](../../validacao/ui-universal-2026-10-06/collider-contours.json). Host: **2/2 cenários direcionados** e **80/80 cenários de UI/autoria**, incluindo seleção real via ponteiro, arquivo inalterado ao trocar foco, desativação de uma única parte, reabertura, Undo e participação da parte no Jolt. Não foi executada a suíte inteira da engine.

Android Release/RelWithDebInfo: build e instalação concluídos no Xiaomi 25053PC47G; SHA-256 instalado igual ao APK produzido, `391b7f8df7c04c5eeb6bb57d38b9e4871e20bc401527af230a858c7008fe05e6`. Toque selecionou Collider 18 mantendo CompoundPlayer; desativação e Undo foram conferidos, assim como Save e reabertura fria. A cena voltou ao hash original `e7e6b94906ef0cc61fbfa53d3507775a310a8033cc27fca5cb91cfdf13813cd5`. Na vista reaberta, outro toque abriu Collider 17. Com contornos desligados, o mesmo toque voltou a selecionar a malha SecondInstance. O aparelho terminou em Edit com contornos ativos. Não foi repetido o aceite físico de Play nesta rodada; o consumo pelo solver foi verificado no host.

**NÃO IREI SER SIMPLISTA NO DESIGN.** A [captura final após reabrir](../../validacao/ui-universal-2026-10-06/device-accepted-reopened.png) confirma foco branco, partes em cinza e instrução acima da navegação/status. A captura `device-final-picked.png` pertence a uma revisão intermediária rejeitada por sobreposição, apesar do nome; somente os arquivos `device-accepted-*` sustentam o aceite do APK final. O vídeo flutuante externo foi minimizado pelo usuário antes dessas capturas de aceite.

Fontes de desempenho protegidas: **268 verificadas, zero alterações**; sem novo A/B de FPS, memória ou thermal. A análise de vídeo é evidência de pesquisa; os testes e capturas da attachsEngine são evidência separada de implementação. Os 100 quadros analisados não abrangem todo o vídeo e a análise visual do vídeo de World Space continua pendente. U11 permanece parcial e a contagem original 140/20/4 não mudou. Alterações locais, sem commit/push nesta rodada.
