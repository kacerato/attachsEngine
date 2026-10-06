# U05 — fontes físicas escolhidas na hierarquia

Alvo: [attachsEngine](https://github.com/kacerato/attachsEngine), continuidade da decomposição U04. Uma malha não deve determinar onde ficam Body, motor ou controles. Este pacote permite usar um objeto sem renderer como corpo principal e escolher os slots visuais de seus descendentes como fontes estáticas do bake.

## Referências e adaptação

- Godot **4.5**, [MeshInstance3D](https://docs.godotengine.org/en/4.5/classes/class_meshinstance3d.html) e [editor 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/mesh_instance_3d_editor_plugin.cpp): geração contextual de múltiplas formas, recursos físicos distintos do visual e alteração com histórico. Nesta engine, fontes e resultado são etapas separadas dentro da ação existente; o destino é o Body escolhido pelo autor.
- Unity **6000.6**, [compound colliders](https://docs.unity.com/en-us/engine/6000.6/manual/physics-section/physics-overview/collision-section/collider-shapes/compound-colliders/introduction): várias formas participam do mesmo corpo e podem ter transformações próprias. Aqui o bake expressa cada instância visual no referencial local do corpo, sem reparentar os visuais ou assumir que todo descendente pertence fisicamente a ele.
- Caso real de correção: [issue oficial de MeshCollider com shear](https://issuetracker-mig.prd.it.unity3d.com/issues/mesh-collider-does-not-skew-slash-shear-correctly-when-nested-inside-another-gameobject). É uma regressão relevante para escolher matrizes completas, em vez de reduzir uma hierarquia a posição/rotação/escala separadas.
- Pesquisa de vídeo: [Colliders — Unity Official Tutorials](https://www.youtube.com/watch?v=bh9ArKrPY8w), publicado em 2013. A entrega inicial apenas o localizou. A continuação agora registra [100 quadros consecutivos analisados e amostras de contexto](ANALISE-VIDEO-COLLIDERS-2026-10-05.md), com timestamps e capturas, sem declarar análise integral dos 4 minutos. A referência atual de API/composição é a documentação versionada acima, e o aceite visual usa a UI executável da attachsEngine.

As otimizações de desempenho informadas pelo usuário foram preservadas. O pacote atua em autoria, worker e Inspector. Os hashes antes/depois de 268 arquivos dos caminhos de Android, renderer, shaders, ponte Jolt e teste HZB permaneceram iguais. Isso confirma integridade das fontes protegidas; não substitui um A/B de desempenho no aparelho.

## Cadeia de dados

`Body selecionado → inventário de objetos/slots → seleção tipada → elegibilidade/recursos → matrizes relativas completas → geometria imutável → worker → prévia das partes → preflight da cena → GLB + dependências + origem → Collider no Body → Undo → reabertura → Play`.

`EditorSession::MotorBakeSource` guarda identidade do objeto e índice do slot. `motorDecompositionSources` informa cada fonte e seu diagnóstico; `selectMotorDecompositionSources` altera a escolha atomicamente. Uma seleção inválida não substitui a anterior. `beginMotorDecomposition` consome esse rascunho; não há tomada automática de todos os filhos quando o destino não tem malha.

A escolha padrão acompanha todos os slots do próprio objeto, inclusive recursos ausentes: a geração diagnostica a ausência em vez de ignorá-la. Acima de 128 slots, exige um conjunto explícito. Depois de uma escolha manual/API, mudanças no documento preservam esse rascunho e invalidam a prévia; não acrescentam fontes novas silenciosamente. Troca de objeto/projeto/epoch reinicia a escolha padrão.

A matriz de cada fonte é composta de suas transformações locais até o Body, mais a matriz do recurso importado. Não é necessário inverter a matriz mundial nem decompor o resultado em TRS. Rotação sob escala não uniforme conserva shear nos vértices; a extração corrige winding quando a matriz intrínseca do recurso possui reflexão. A transformação do próprio Body continua sendo aplicada pelo backend físico existente. Instâncias da mesma malha em objetos diferentes são fontes distintas; slots repetidos da mesma malha no mesmo objeto não duplicam triângulos. **Transform autoral continua recusando escala negativa**; espelhamento completo de objetos exige um pacote próprio de renderização, normais/culling, física e persistência. A correção matemática da extração não declara esse pacote implementado.

Fontes fora da hierarquia, slots ausentes, matrizes singulares/não finitas e recursos não carregados causam erro. Um Body, Character ou corpo 2D no caminho de um descendente marca uma fronteira de autoridade, que não é absorvida. Skin não vira pose base silenciosamente. A autoridade do destino e compostos existentes continuam sujeitos às regras de U04.

O GLB persiste `asset.extras.collisionBake.authoringSources`: IDs dos objetos como strings, slots, GUIDs de Mesh quando disponíveis e matrizes relativas. O registro conserva dependências dos arquivos importados. Fontes de primitivas intrínsecas podem não ter GUID; nesse caso o objeto/slot e a geometria imutável continuam registrados. Esses metadados documentam a origem de uma revisão; **não são uma receita de regeneração automática**.

Aplicar confere revisão/epoch/projeto/seleção, hash da geometria e origem completa novamente. Transformação, troca de slot ou identidade de recurso invalidam o resultado. A cena, os filhos, seus renderers e materiais não mudam durante a geração. Apply altera somente os componentes do destino, com uma entrada de Undo.

## Workflow e limites

NÃO IREI SER SIMPLISTA NO DESIGN.

Objeto → Configurar locomoção → Decompor → Fontes. A escolha ocupa temporariamente a mesma superfície contextual. O viewport permanece visível; **Só este objeto** e **Disponíveis** servem como atalhos de seleção, enquanto cada linha permite incluir/remover um slot. Fontes impedidas permanecem visíveis com diagnóstico; uma fonte já escolhida pode ser retirada explicitamente mesmo quando seu recurso está ausente. **Só este objeto** conserva todos os slots do destino; **Disponíveis** inclui apenas fontes disponíveis na árvore e não transfere corpos filhos. Depois de concluir, o autor gera e revisa os cascos. Uma revisão pronta permite voltar à escolha; mudar fontes invalida a prévia, sem alterar a cena.

A lista reaproveita a identidade visual e as primitivas do editor; fontes são instâncias de malha já existentes, não um novo componente cenográfico. Superfícies compactas mostram uma fonte por página, maiores mostram três. O trabalho de inventário ocorre ao entrar/editar a autoria ou quando a revisão muda, não em todo frame ocioso.

Limites: 128 fontes selecionadas e inventário GUI até 256 slots, com aviso explícito se truncado; seleção tipada pode escolher um conjunto de até 128 fontes além desse inventário. O atalho global recusa inventário truncado, sem selecionar um subconjunto silenciosamente. Mantêm-se os limites de 100 mil triângulos e 32 partes. Cada objeto fonte deve formar um sólido fechado/orientado depois de soldar os vértices de seus slots. Objetos diferentes podem encostar ou se sobrepor: são decompostos separadamente no mesmo worker e reunidos no Body. O orçamento total de partes é dividido entre objetos, com pelo menos uma parte por objeto; o tempo é compartilhado, não reiniciado por fonte. O GLB registra intervalos de triângulos por origem e `sourceObjectId` por casco. Faces duplicadas/não manifold dentro do mesmo objeto recebem diagnóstico. Não é uma união booleana; a decomposição continua aproximada.

## O que este pacote não encerra

Movimento de filhos após o bake não modifica automaticamente os cascos. Skin/pose de animação, seleção por clique da fonte no viewport, edição de vértices/cortes, receitas de regeneração, correspondência de overrides/prefab e reconstrução dinâmica com conservação de momentum permanecem abertos. Não declarar U05, U09 ou o roadmap universal completos por esta ampliação.

## Aceite

Cenário executado: Body em objeto sem renderer; malhas filhas em ramo com escala não uniforme/rotação e segunda instância rotacionada/escalada do mesmo GUID; escolher/excluir cada uma; gerar sem alterar cena; verificar parede e cavidade no Jolt; Apply/Undo/Redo; salvar/reabrir; controles autorados ligados ao objeto principal. Cinco testes direcionados passaram, incluindo objetos encostados/sobrepostos; regressão de UI/autoria: **78/78**. Capturas nativas de UI em **800×400** e **1280×720** percorreram fontes e todas as 16 partes com ponteiro real; são rasterização de UI em software, não renderização Vulkan do cenário.

No Xiaomi 25053PC47G, o Release instalado foi conferido por hash: `57dc28f0ccd363172913d11d1910c6b6a47500773405b0e79050f9a20fa5f2a2`. A autoria gerou 16 cascos no Body principal sem renderer, mantendo os visuais. Undo restaurou zero componentes; Redo, salvar e reabrir conservaram a cena (`e7e6b94906ef0cc61fbfa53d3507775a310a8033cc27fca5cb91cfdf13813cd5`). Joystick: X 0→1,93; salto: Y 0,53→1,85; impulso: contador 0→1 e X 1,93→2,19. São gestos sequenciais; não encerram multi-touch simultâneo, thermal, consumo ou pico de memória.

Projeto inicial: `examples/ui/HierarchySources`; montagem pronta salva pelo aparelho: `examples/ui/HierarchySourcesReady`. O GLB real contém duas origens do mesmo GUID, matrizes completas e oito cascos por objeto. Cena/registro/import maps não foram montados manualmente. Os limites, logs, hashes e capturas estão em [hierarchy-sources.json](../../validacao/ui-universal-2026-10-05/hierarchy-sources.json). O roadmap original segue integral: **140 planejados, 20 parciais, 4 completos**; U05/U09 e a engine permanecem parciais.
