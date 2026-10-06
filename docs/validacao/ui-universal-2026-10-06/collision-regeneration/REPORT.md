# Receita persistente e regeneração de colisão

Bloco implementado no checkout de `https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`. A receita guarda fontes, slots, orçamento, identidade das partes e a base usada para separar geometria herdada de personalizações. Regenerar produz uma revisão; a cena só muda após revisar correspondências e aplicar. Colliders publicados continuam sendo a autoridade da física.

Edições de propriedades, recurso manual, estado e remoções locais são preservados. Uma remoção fica registrada e não recria silenciosamente o componente. Correspondência espacial exige confirmação; vínculos manuais são exclusivos. Fontes ausentes, resultado obsoleto, referências incompatíveis, geometria inválida e falha de publicação bloqueiam a transação. Recursos derivados são imutáveis e permanecem disponíveis para Undo/Redo.

Duplicação e instanciação remapeiam as fontes. Regenerar uma instância preserva seu vínculo e não altera a fonte. O Apply de prefab da receita declara e publica a revisão física inteira — receita e Colliders relacionados — para evitar metadados novos com colisão antiga. Campos independentes mantêm suas próprias regras de override e conflito.

A ferramenta usa a rota contextual do objeto, prévia no viewport, paginação por parte, escolha de fontes, confirmação e um Apply/Undo. O ícone novo está no atlas executável. Não há componente decorativo no menu de criação nem fachada de runtime para executar receitas de editor.

## Aceite

Cinco cenários direcionados verificam geração/regeneração, overrides e tombstones, identidade/Jolt, prefab/Apply/novas instâncias, arquivos e reabertura, reimportação real de GLB, mapeamento, staleness, publicação recusada e interação por ponteiro em interfaces compacta e completa. A regressão GUI passou em 105/105 cenários. Logs e hashes estão em `acceptance.json`.

No POCO F7, mover a fonte para X=-2,5, manter centro local X=0,3 e desativar a segunda parte → regenerar → revisar ambas as páginas → confirmar → Apply → salvar. A prévia não mudou o arquivo; um Undo restaurou o arquivo anterior byte por byte e um Redo restaurou exatamente o aplicado. Reabrir a atividade manteve fontes, orçamento e estados. O projeto retirado do aparelho foi reaberto pelo pipeline nativo com hashes dos GLBs e consultado no Jolt do host; a identidade e a parte desativada foram verificadas. Isso é aceite físico de autoria/persistência e aceite do solver no host; não é um novo aceite de gameplay/Play Android.

Capturas foram inspecionadas individualmente. Este bloco não gravou vídeo. A captura revelou a antiga paginação que escondia candidatos na altura intermediária; a correção foi recompilada, testada por ponteiro e conferida no dispositivo antes do aceite. Os rótulos compactos do orçamento também foram ajustados após a inspeção.

Os 210 arquivos de renderer/shaders da referência de desempenho permanecem idênticos. Não houve medição nova de FPS/temperatura. O roadmap original permaneceu idêntico; seus requisitos não foram substituídos por esta entrega. Não houve commit/push neste bloco.

## Limites e continuidade

Este recorte fecha receita/regeneração explícita com preservação e publicação coerente; **U05, U09 e a engine inteira continuam abertos**. Não adiciona bake de skin/pose, orçamento irrestrito ou regeneração automática durante reimportação. Seleções de slots de um mesmo objeto compõem o mesmo sólido; não anunciam identidade independente por slot. A prévia limita contornos desenhados acima de 3.200 triângulos e informa amostragem; a geometria publicada e o solver usam os dados completos.

Os blocos seguintes continuam no [plano integrado](../../../planos/ui-universal/BLOCOS-INTEGRADOS-2026-10-06.md): concluir autoria física; locomoção/arbitragem/animação; UI componível integral; modelagem visual completa; SDK/reprodução/rede/orçamento. U11 anterior conserva seu aceite próprio; ele não substitui a ampliação de modelagem visual pedida pelo usuário.

Referências: [Godot 4.5 — import process](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/import_process.html), [source 4.5-stable — scene importer](https://github.com/godotengine/godot/blob/4.5-stable/editor/import/3d/resource_importer_scene.cpp), [Unity 6000.0 — prefab instance overrides](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html). Princípios adotados: fonte separada do derivado, parâmetros persistentes, base/edição local distintas e publicação explícita. Foram integrados aos sistemas reais da engine.
