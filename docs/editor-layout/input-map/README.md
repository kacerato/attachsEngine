# Mapa de entrada: referência visual e tela executável

NÃO IREI SER SIMPLISTA NO DESIGN.

O [conceito gerado](proposta-v2.png) partiu de uma captura ADB da tela anterior. O [resultado no Android](captura-android.png) mostra a implementação real; o conceito não é prova de suporte. A geração usou a ferramenta de imagem integrada. Os dois ícones novos foram gerados separadamente com os ícones tridimensionais existentes como referência e integrados ao atlas nativo.

## Decisão de fluxo

A [Unity Input System](https://docs.unity3d.com/ja/Packages/com.unity.inputsystem%401.4/manual/ActionAssets.html) organiza ações e vínculos para edição direta; o [InputMap da Godot](https://docs.godotengine.org/en/stable/classes/class_inputmap.html) associa várias fontes a uma ação nomeada. A adaptação Astra usa uma superfície dedicada em paisagem: lista de ações à esquerda, vínculos no centro e propriedades da seleção à direita. Hierarquia, Arquivos e Inspector da cena cedem espaço enquanto o mapa está aberto.

Somente dados consumidos pelo runtime aparecem: tipo, papel Mover/Olhar/Saltar compatível, contexto, zona morta e sensibilidade quando aplicáveis, fonte e ajustes do vínculo. Não foram copiados os diagnósticos ao vivo, esquemas de controle ou grupos fictícios da proposta exploratória. A lista usa paginação para até 64 ações e quatro vínculos visíveis por página, com seleção direta. Em largura estreita, a edição troca para uma superfície de ação/vínculo em vez de espremer três colunas.

## Prompts finais

- Interface: “Use the actual Astra Android screenshot as visual reference. Create a high fidelity landscape Input Map workspace with charcoal surfaces and acid-lime identity, a visible action list, selected action, multiple bindings and contextual properties. Give the viewport area to input authoring. Preserve realistic touch targets and Portuguese labels. Remove invented diagnostics, live event displays, description, repeat delay and any control without model or runtime.”
- Ícone de ação: “Match the existing Astra tactile 3D icon: ivory molded body, graphite mechanism and acid-lime detail. Make a compact action trigger switch, transparent background, readable at 24 dp.”
- Ícone de vínculo: “Match the same 3D material and scale. Make a connector plug joining two sockets with a luminous lime connection, transparent background, distinct from the action trigger.”

O código da tela está em `native/editor/editor_screen.cpp`; os arquivos de origem dos ícones estão em `assets/astra-visual/icons/named/input/`. O atlas e o enum são gerados por `tools/pack-icon-atlas.py`.
