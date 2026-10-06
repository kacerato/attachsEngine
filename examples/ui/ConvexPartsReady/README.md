# Malha côncava pronta para Play

Projeto autorado e salvo no Android com a revisão validada da attachsEngine.
Copie **a pasta inteira**, incluindo `.astra/`, para a área de projetos do editor
e abra **UI Convex Parts Ready**. Entre em Play: **Mover**, **Saltar** e
**Impulso** controlam o objeto em U, com sua malha visual original.

O ator possui oito Collider Mesh independentes, PhysicsBody e DynamicBodyMotor.
As partes vêm de V-HACD no orçamento Leve, não de uma forma de jogador fixa.
O Canvas guarda o receptor por identidade. Piso, câmera, controles e script
também são editáveis.

Selecione **ConcavePlayer** no Inspector para editar cada Collider: recurso,
estado, centro e rotação local alteram a forma física. O GLB de colisão e os
mapas de identidade em `.astra/imports/` são recursos reais. A cena, o registro
e esses mapas foram copiados sem alterar o formato serializado pelo editor.

Para experimentar a geração desde o visual original, use o projeto vizinho
`../ConvexParts/`. Esta revisão preserva compostos existentes e recusa sua
substituição automática; regeneração com correspondência de overrides ainda
não está implementada. O histórico Undo de uma sessão não é um recurso
persistido neste exemplo; a sequência Apply/Undo/Redo foi conferida na sessão
de autoria no aparelho.

Limites desta revisão: sólido fechado/orientado, 100 mil triângulos de entrada,
até 32 cascos conforme orçamento, 8–64 vértices por casco. Decomposição aproxima
a geometria; confira as cavidades na prévia. Hierarquia de fontes, skin deformada,
picking de partes e correspondência de overrides ao regenerar seguem pendentes.

Referência: Godot 4.5 [MeshConvexDecompositionSettings](https://docs.godotengine.org/en/4.5/classes/class_meshconvexdecompositionsettings.html).
Implementação: V-HACD 4, commit `f900e42361491f525262d4825e758845e4969897`, e Jolt 5.6.1.
Evidência: `docs/validacao/ui-universal-2026-10-05/convex-parts.json`.
