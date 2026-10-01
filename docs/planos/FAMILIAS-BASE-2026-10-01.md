# Fechamento da base de objetos e transformações

Recorte: F001 e F002 do catálogo versionado. Implementação e aceite host/APK registrados em 2026-10-01. A revisão no aparelho desta revisão ABI34 permanece pendente; as capturas aqui são do executável de UI host, com o documento real e rasterização por software.

## Contratos e comportamento

Objeto conserva identidade de mundo/geração, metadados, ordem, grupos e referências internas ao duplicar. A destruição invalida handles imediatamente; a remoção de armazenamento ocorre no safepoint. `GameObject.Layer` agora lê/escreve a camada real 0–31, com invalidação dos consumidores. O cenário nativo confirma que a mudança altera a participação em um campo Box2D.

Transformações locais usam TRS, graus no documento e quaternion no SDK. O pivô é a origem local do objeto; a revisão não introduz pivô independente. Ponto inclui translação; vetor usa a matriz linear completa; direção usa a orientação quaternion da cadeia e ignora escala. O forward dos utilitários de objeto é +Z. Essas convenções não mudam a orientação específica de recursos de câmera.

Uma hierarquia de TRS pode gerar shear. `LocalToWorldMatrix` e conversões de ponto/vetor preservam a transformação afim completa. Posição mundial e orientação autorada mundial podem ser editadas sem exigir decomposição dessa matriz. `RotateAround` escreve posição e rotação em uma única alteração local, preservando escala. `WorldTransform` continua sendo um contrato TRS exato e recusa uma matriz mundial que não pode representar; não aplaina shear silenciosamente. Escala local negativa é recusada pelo contrato nativo existente. Escala zero é permitida, mas operações que exigem inversa recusam matriz singular. Valores não finitos ou resultados que excedem a representação são recusados.

Reparent distingue preservar local e preservar mundo. Preservar mundo é recusado atomicamente se a pose resultante não é representável em TRS local. A autoridade física da subárvore é verificada também no safepoint: se um body Jolt, Box2D ou Character adquirir a pose entre a fila e o flush, a operação falha com `TransformOwnedByPhysics`, sem mudar o pai.

## Referências concretas

- [Unity 6000.0 — Transform.SetParent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Transform.SetParent.html): explicitar a política de preservar espaço local/mundial.
- [Unity 6000.0 — Transform](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transform.html): hierarquias com escala não uniforme podem produzir shear; não tratar a matriz como TRS simples universal.
- [Unity 6000.0 — GameObject.layer](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject-layer.html): camada com consumidor real, distinta de grupos.
- [Godot 4.5 — Node3D](https://docs.godotengine.org/en/4.5/classes/class_node3d.html) e [fonte Node3D 4.5](https://github.com/godotengine/godot/blob/4.5/scene/3d/node_3d.cpp): separar transformação afim de orientação e declarar limitações de inversão/decomposição.
- [Godot 4.5 — Node](https://docs.godotengine.org/en/4.5/classes/class_node.html): identidade, grupos e reparent são contratos de objeto, não estado de UI.

Os princípios foram adaptados aos dados TRS e à autoridade de pose existentes da Astra; não se afirma paridade de API ou convenção de eixo.

## Evidência e limites

Pasta: `docs/validacao/evidencias/families-base-20261001/`.

- 26 cenários nativos focados na base, incluindo quatro cenários novos de integração e 22 regressões existentes.
- 8 cenários de regressão dos campos 3D/2D.
- Um cenário C# integrado de matemática/transporte com hierarquia não comutativa, shear, escrita atômica e erros; um teste de layout ABI.
- Build gerenciado final: zero warnings/erros. `:app:assembleDebug`: sucesso. Manifesto confirma bytes do SDK, rendering e atlas e bibliotecas arm64 no APK.
- ABI34 acrescenta somente o callback `ObjectLayer` ao final da tabela; layouts dos campos e callbacks anteriores permanecem preservados. Native e SDK devem ser publicados juntos.
- Captura host permite inspecionar seleção e campos reais. Não prova toque, Vulkan, execução C# no Android ou comportamento físico no aparelho.

F001 e F002 encerram a implementação no recorte host/APK com a captura executável. Por instrução do usuário de finalizar com as evidências obtidas e não retomar o aparelho nesta rodada, a qualificação física integrada P20 permanece separada. O registro conserva esse limite explícito, em vez de contar APIs/propriedades como famílias ou inferir execução Android.
