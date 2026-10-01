# P15a — Caminhos e Curve3D

Referências versionadas: [Godot 4.5 Curve3D](https://docs.godotengine.org/en/4.5/classes/class_curve3d.html), [Path3D](https://docs.godotengine.org/en/4.5/classes/class_path3d.html), [source 4.5-stable curve.h](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/curve.h) e [curve.cpp](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/curve.cpp). Princípio extraído: separar pontos Bézier authorados do cache de comprimento/amostragem; posições dos handles são relativas ao ponto; Path organiza a curva em espaço local e o consumidor resolve espaço mundial. O pacote adapta isso ao limite mobile de128pontos e IDs persistentes, sem copiar o Inspector ou expor up-vector/tilt sem consumidor.

## Contrato executável de dados

`resources::Curve3D` é um valor tipado independente de ComponentValue: points, closed. Cada CurvePoint3D tem id u64 e position/in/out float3. IDs devem ser não zero e únicos. Valores finitos e magnitude máxima100000;128pontos. Handles in/out são offsets locais. `scene::Path` possui esse valor inline, id astra.path, payload1 e nextPointId persistente. A curva inline acompanha clone, prefab e save da cena; não existe GUID externo de Curve3D neste recorte. IDs internos não são referências de objeto e não devem ser remapeados no clone.

Path aceita curvas vazias ou de um ponto como rascunho autoral. O consumidor deve diagnosticar incapacidade de seguir curva de comprimento zero. closed fecha também dois pontos na Astra, permitindo ida/volta, uma diferença explícita da condição mais de dois da referência Godot. Sem tilt/roll/up-vector transportado nesta etapa.

`BakedCurve3D` mantém samples/length separados e nunca é serializado. bake(curve,tolerance=.01) usa subdivisão de Casteljau, excesso de comprimento do polígono de controle sobre corda, máximo depth12 e32768samples; isso detecta também loops e reversões collineares que um teste apenas no meio perderia. Depth limita a aproximação; tolerância não promete erro global garantido. Exceder orçamento ou entrada inválida preserva cache anterior e retorna false. Comprimentos usam double. sampleDistance usa busca binária e interpolação linear dos segmentos baked, tangent normalizada de segmento, clamp por default ou wrap explícito; curva degenerada retorna posição e direção zero. A orientação não promete frame suave nem transporte paralelo. O PathFollow transforma pontos e handles para mundo antes do bake para medir distância mundial, conforme contrato com seu consumidor.

## Edição e reversão

Path expõe closed e nove ComponentSlotNumber reais: point_position_x/y/z, point_in_x/y/z, point_out_x/y/z. Slots são índices temporários; operações duráveis usam pointID. Não foi inventado descriptor genérico de collection. Root conecta API tipada por ID; a UI lê IDs e mantém seleção estável após reorder.

Path oferece insertPoint(beforeID,point,outID), removePoint(ID), movePoint(ID,beforeID), editPoint(ID,point), point(ID). beforeID0 significa append. Insert ignora ID fornecido e aloca monotonicamente; refuse overflow/limite/handle inválido antes de mutar. Edit conserva identidade. Remove permite rascunho vazio. EditorSession expõe insertPathPoint/removePathPoint/movePathPoint/editPathPoint(object,instance,...), copia o objeto, altera candidato e publica pela EditorHistory::applyValues existente. Cada operação tem Undo/Redo e invalidação do documento; Play e gesto aberto recusam edição autoral para evitar misturar documentos. Comandos runtime próprios são responsabilidade do GameWorld/ABI real, não destas funções editor-only.

## Validação executada na integração

Arquivo test_curve3d.cpp contém dois cenários que passaram na integração: Bézier com handles, comprimento, tangente normalizada, cache preservado na falha, fechamento/wrap/degeneração; edição real EditorSession com insert/IDs/reorder/handles/remove/Undo/Redo/save/reopen e inserção inválida atômica. Schema, consumer, CMake, API, criação, superfície de pontos e gizmos estão integrados. PathFollow e comandos ABI operam sobre o GameWorld real e têm cenários próprios. Build host passou; nenhum ADB foi executado. Resultados completos e limites estão na [auditoria consolidada](EXECUCAO-AUDITORIA-UNITY-ASTRA-2026-09-30.md); estes dados não encerram o pacote P15 inteiro.

## Superfície de pontos e input

A rota contextual usa objeto real do Inspector (inclui locked/focused), instanceID e pointID; navegação/reorder preservam IDs e seleção de lista resolve índice em ID imediatamente. Posição e tangentes abrem o teclado numérico existente. EditorTextEdit transporta elementId, versão da cena e instance; commit resolve novamente esse ID na coleção atual e rejeita ponto removido, revisão alterada ou contexto trocado. A ponte Android compara elementId ao renovar solicitação. Edição de curva é individual, não replica identidade de pontos em multi-selection.

Mutação da curva pelo UI permanece Edit-only, inclusive quando o Inspector Play usa documento espelho com workspace Scene temporário. Reiniciar/Parar PathFollow entram diretamente no mundo Play, preservam pausa até próximo Step e mostram progress/diagnóstico reais. A curva de receitas é preenchida no candidato antes de publicar histórico; erro recusa criação inteira. Rota de pontos abre na receita concluída. Cache de estatísticas compara a curva tipada e não refaz bake porque outro objeto mudou de transformação. Gizmo é integrado pelo design ao selectedinstance/pointID; ausência de captura no aparelho continua explícita.

A tangente nos extremos de curva fechada é compartilhada e normalizada, derivada de out−in do primeiro ponto; sem handles válidos usa direção central de vizinhos. Isso evita salto numérico do frame na costura suave, sem prometer suavizar cantos autorados.

## Continuação Android autorizada

O cenário P15a passou no Xiaomi 25053PC47G/API36: criação pelo catálogo, edição numérica de posição/tangente, Undo/Redo, save/reopen, associação de Behavior C# compilado no aparelho, amostragem mundial pela ABI22, movimento de cubo filho e Stop/Restart pela API e pela UI. Dados autorais permaneceram idênticos depois de sair de Play. [Relatório e evidências reais](../validacao/evidencias/p15a-android-20260930/README.md).

Também foram corrigidos o cache negativo de bake em ScenePaths, o diagnóstico de autoridade física de descendentes, a identificação objeto/instância/ponto no destaque e a seleção dos gizmos em objetos/layers ocultos ou bloqueados. Não há promessa de desempenho de muitas curvas ou adaptação portrait por esse aceite. Não foram adicionadas capacidades fictícias de Curve3D externo, tilt ou extrusão.
