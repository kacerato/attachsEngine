# Malha côncava com colisão independente e controles autorados

Projeto gerado pelo serializer e importador nativos da attachsEngine. Copie esta
pasta para a área de projetos do editor e abra **UI Convex Parts v1**.

1. Selecione **ConcavePlayer** na Hierarquia.
2. Abra as ações do Inspector → **Configurar locomoção…** → **Decompor**.
3. Escolha **Leve · 8** e **Gerar prévia**. A malha em U continua sendo o visual;
   as linhas coloridas mostram as partes físicas. Alterne uma parte para conferir
   seu contorno cinza; reative-a antes de aplicar.
4. **Aplicar · 1 Undo** cria Body, Motor e um Collider por parte. Cada Collider
   possui recurso, estado e pose próprios. Salve e reabra o projeto.
5. Entre em Play. O joystick **Mover**, **Saltar** e **Impulso** controlam esse
   mesmo objeto. O Canvas guarda o receptor por identidade; o script não procura
   um cilindro ou um jogador por nome.

O projeto de origem contém apenas o visual importado do ator. A colisão deve ser
gerada pelo workflow acima antes de Play; não há forma substituta silenciosa.
O piso, a câmera e a UI são objetos editáveis. O recurso de colisão gerado fica
em `Collision/`; Undo preserva esse recurso para Redo e outros usuários.

Limites desta revisão: sólido fechado/orientado, 100 mil triângulos de entrada,
até 32 cascos conforme orçamento, 8–64 vértices por casco. Decomposição aproxima
a geometria; confira as cavidades na prévia. Hierarquia de fontes, skin deformada,
picking de partes e correspondência de overrides ao regenerar seguem pendentes.

Referência: Godot 4.5 [MeshConvexDecompositionSettings](https://docs.godotengine.org/en/4.5/classes/class_meshconvexdecompositionsettings.html).
Implementação: V-HACD 4, commit `f900e42361491f525262d4825e758845e4969897`, e Jolt.
