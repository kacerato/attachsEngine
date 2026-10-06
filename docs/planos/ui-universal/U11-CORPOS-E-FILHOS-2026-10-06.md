# U11 — seleção de formas vinculadas a um corpo

NÃO IREI SER SIMPLISTA NO DESIGN.

## Capacidade e dependências

Um Body pode estar na raiz e seus Colliders em objetos descendentes. O runtime já monta esse compound usando o campo persistido `Collider.owner`; o editor selecionava formas apenas na coleção do objeto atualmente inspecionado. A expansão usa esse contrato existente: Body → vínculo explícito → objeto autoral → UID local do Collider → superfície/contorno → Inspector real → histórico/arquivo → compound Jolt.

Não há migração de arquivo, novo componente, conversão da malha ou nova convenção de propriedade. `owner=0` continua significando corpo no próprio objeto. Um Body intermediário impede a referência ao ancestral; descendência sozinha não atribui propriedade física.

## Referências e decisão

- [Godot 4.5 CollisionObject3D](https://docs.godotengine.org/en/4.5/classes/class_collisionobject3d.html): múltiplas formas e identificação de quem possui cada forma. Extração: corpo físico, forma e objeto de autoria precisam manter identidades distintas.
- [Godot 4.5-stable CollisionShape3D, código oficial](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/physics/collision_shape_3d.cpp): associação com `create_shape_owner(this)`, atualização de transformação/disabled e remoção no unparent. A engine conserva seu vínculo explícito já serializado, sem copiar a regra de parent direto da Godot.
- [Godot 4.5-stable gizmo de CollisionShape3D](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/gizmos/physics/collision_shape_3d_gizmo_plugin.cpp): edição sobre a geometria da forma. Reutilizamos projeção e interseção já existentes, sem bounds da malha como substituição.
- Uso observado no vídeo oficial Unity sobre Colliders: [análise com 100 frames consecutivos](ANALISE-VIDEO-COLLIDERS-2026-10-05.md). A cobertura é somente o trecho 90,033–93,333 s; esta rodada não analisou novos frames e não declara análise integral do vídeo.

## Interação implementada

Abrir o Body no Inspector mostra seus Colliders vinculados, inclusive através de uma pasta intermediária. Tocar uma superfície abre o Collider no objeto que contém o componente. Ao abrir esse filho, as formas dos irmãos vinculados continuam disponíveis. A forma em foco é branca; as outras formas são cinza. A instrução contextual identifica o Body, permitindo entender por que formas em objetos diferentes participam da mesma edição.

Esta navegação substitui a necessidade de voltar repetidamente à Hierarquia, sem acrescentar uma janela fixa ao viewport. Reutiliza ícones reais de Body/Collider. A mudança é isolada no picking/visualização do editor e mantém a navegação existente pela Hierarquia e pelo Inspector.

A captura física encontrou corte da última letra da instrução. A causa era medir com métricas Regular e renderizar a legenda Medium. O texto contextual agora mede, encurta e desenha no mesmo peso Regular, com margem para o glifo; a revisão exige novas capturas e hash de APK próprio, sem aproveitar a captura rejeitada como aceite final.

O hit contém `(objeto, UID do componente)`: dois filhos podem possuir Collider 1 sem serem confundidos. Superfície mais próxima vence globalmente; contornos são fallback. Empates usam objeto/UID persistentes. Cada objeto candidato respeita visibilidade, atividade, camadas e bloqueios de picking. Colliders desativados continuam editáveis. Corpos independentes e vínculos inválidos não entram no contexto compartilhado; um Collider sem Body continua editável no seu próprio objeto.

## Aceite e limites

Cenário: Body sem Collider próprio → dois filhos com UID 1, um dentro de pasta → tocar ambos → desativar apenas o segundo pelo Inspector → salvar/reabrir → Jolt deixa de atingir somente essa forma → Undo restaura o compound. Selecionar uma forma não modifica arquivo, transformações, proprietário físico ou histórico.

Resultado: build host concluído, **6/6 cenários direcionados** na revisão final e **84/84 cenários de UI/autoria** antes do refinamento tipográfico final. Após esse refinamento, os seis cenários e as capturas nativas de 1280×720/800×400 foram repetidos; a regressão ampla não foi repetida para uma mudança localizada de peso de legenda. O projeto de aceite foi gerado pelo serializer nativo e conferido em um compound Jolt real antes da gravação.

Android Release/RelWithDebInfo compilado e instalado: SHA-256 **f7c52a1c2de972d11d3f581ac6dd17ee09cd905ecf6e627f6c92700182e942f9**, confirmado no `base.apk` do Xiaomi 25053PC47G/onyx. Aceite autoral físico: Body sem Collider próprio → caixa em pasta descendente → cilindro irmão, ambos com UID 1 → vínculo SharedBody no Inspector → desativar apenas RightCylinder/Save → Undo/Save → reabrir a frio → selecionar a forma novamente, ativa. Comparação do arquivo físico mostrou alteração apenas na linha de RightCylinder; Undo e reabertura conservaram o SHA original **febaf0db75ac85187527872edc9e05b5367472794b1f2f876c4d0da929193d85**. Overlays OFF voltou à seleção normal de LeftBox. A captura final mostra a instrução completa, sem corte ou sobreposição com controles.

Evidências, logs, hashes e cenários: [acceptance.json](../../validacao/ui-universal-2026-10-06/body-owners/acceptance.json). [Captura do vínculo real](../../validacao/ui-universal-2026-10-06/body-owners/device-accepted-link.png). As capturas anteriores `device-initial/root/body/left/right.png` usam outro APK e foram rejeitadas como aceite visual final. O arquivo original do roadmap mantém seu SHA, e **268 fontes de desempenho protegidas têm zero alterações**. Isso não é um benchmark de FPS/thermal. Alterações locais, sem commit/push nesta rodada.

U11 permanece parcial: ainda faltam edição direta de alças de Collider no viewport, distribuição/limites avançados de formas e perfil de custo em grande escala. No encerramento da entrega de seleção descrita acima, a consulta de runtime devolvia Body + UID local, sem o objeto autoral da forma. Esse limite foi tratado na continuação [U11 — identidade das queries](U11-IDENTIDADE-QUERIES-2026-10-06.md), com ABI44, Collider tipado, compound Jolt e execução de script no Android. Os hashes/capturas desta entrega de seleção permanecem próprios da revisão anterior.
