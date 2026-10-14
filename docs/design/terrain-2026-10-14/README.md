# Terreno — proposta e evidência

`concept.png` é hipótese gerada com image_gen, baseada na captura nativa anterior `build/terrain-evidence/ui-1280-1.png`. Não é captura da engine. A paisagem ilustrativa, árvores e vegetação não representam funcionalidades implementadas.

Direção aplicada: manter o shell Astra em grafite, preto e verde; separar Gerar, Esculpir, Camadas e Gerir; agrupar ferramentas, parâmetros de pincel e coordenadas; oferecer geração com destaque e estados reais. A organização de ferramentas/propriedades foi estudada na ITsMagic. Não é réplica visual.

Prompt de direção utilizado: criar hipótese de UI para gerador de terreno/voxel, usando a captura do editor Astra como referência; manter shell existente e identidade preto/grafite/lima; painel lateral com quatro abas, oito ferramentas, raio/força, pincel no viewport, XYZ, aplicar/remover operação e gerar; somente controles com comportamento implementável; identificar PROPOSTA VISUAL. A paisagem serve apenas como contexto ilustrativo.

Confronto com implementação: controles correspondem à receita persistida, operações nativas e publicação de malhas. Não foram adicionados controles de vegetação, erosão, streaming ou shaders sem consumidor.

Capturas posteriores reproduzíveis: `aether_terrain_tests`, cenário `terrain_native_ui_controls_and_capture`, gera `build/terrain-evidence/ui-1280-{0..3}.ppm` e `ui-655-{0..3}.ppm`, com o draw list, fonte, atlas e roteador reais. Rasterização de software no Windows; não é evidência Vulkan/Android. Layout em 1280×720 e 655×300, sem glyphs ausentes ou comandos descartados. O teste também toca o botão real de gerar e verifica a criação assíncrona na cena.

Quatro ícones novos: terreno, voxel, esculpir e camadas, em `assets/astra-visual/icons/vector/terrain` e `named/terrain`, catalogados e empacotados no atlas real. O acesso à ferramenta usa o novo ícone de terreno.
