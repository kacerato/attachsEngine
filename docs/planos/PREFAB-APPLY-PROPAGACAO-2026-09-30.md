# Apply seletivo de prefab e propagação — 2026-09-30

Revisão posterior: [componentes adicionados/removidos e ordem, 2026-10-01](PREFAB-COMPONENTES-2026-10-01.md). O recorte abaixo registra a entrega de setembro; sua limitação de Apply por composição foi ampliada por essa revisão. Hierarquia de objetos, nested e variantes continuam parciais.

## Capacidade entregue

Apply publica um ou mais endereços escolhidos da instância no `.aeprefab`, atualiza seu registro de assets e reconcilia todas as instâncias desse asset carregadas no documento atual. Não publica outros campos locais da instância escolhida. Nenhum tipo novo de componente foi adicionado.

O contrato é `EditorSession::applyPrefabOverride(view, row, overwriteConflict, error, report)` e sua variante em lote `applyPrefabOverrides`. A inspeção oferece `applyable` e `applyReason` por endereço. A UI deve oferecer Apply somente quando o backend autoriza; substituir uma fonte em conflito exige a escolha explícita `overwriteConflict=true`. Os estados Local, Herdado e Conflito continuam distintos de uma falha de operação.

## Referências e decisões

[Unity 6000.0, ApplyPropertyOverride](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/PrefabUtility.ApplyPropertyOverride.html) separa aplicar uma propriedade de aplicar uma instância inteira e exige identificar o asset de destino. [PrefabInstanceOverrides, Unity 6000.0](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html) descreve Apply/Revert e as transformações de alinhamento da raiz. Astra mantém posição/rotação da raiz como colocação da instância; escala continua comparável. Coleções sem reflexão individual são um endereço de componente inteiro, identificado como tal, sem prometer seleção de elemento.

[PrefabUtility.cs, branch oficial 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Prefabs/PrefabUtility.cs) explicita o mapeamento das referências ao objeto correspondente na fonte. Astra valida a portabilidade antes de remapear: uma referência externa selecionada é recusada mesmo quando seu número coincide com um ID da fonte. Referências externas em campos não escolhidos não impedem aplicar um campo numérico independente.

[Godot 4.5, Creating instances](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/instancing.html) apresenta o fluxo fonte → instâncias independentes, a atualização de defaults e a preservação de overrides locais com Revert junto à propriedade. O [código PackedScene em 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/packed_scene.cpp) mantém estado serializado de cena separado da instanciação. Astra reutiliza seu Prefab, SceneGraph, ObjectCloneMap, PrefabLink e pipeline de hidratação existentes; não introduz outro modelo de prefab.

## Reconciliação B/S/I

B é a baseline serializada em `PrefabLink.base`, S é a nova fonte preparada e I é a instância atual normalizada. Para cada endereço: I=B recebe S; S=B preserva I; I=S converge; quando os três divergem, I é preservado e o endereço permanece Conflito. A baseline avança para S, exceto nos endereços conflitantes, que conservam B. Assim o conflito continua visível após salvar e reabrir, em vez de virar Local silenciosamente.

A propagação remapeia referências separadamente para cada instância e valida o documento preparado inteiro antes da publicação. Não altera o posicionamento da raiz, não copia links de ownership para a fonte e não apaga overrides não selecionados. A fonte publicada alimenta novas instanciações; as instâncias existentes recebem valores pelos mecanismos reais do documento e revisão de cena.

## Transação e reversão

Todos os objetos, dependências, registro e entrada de histórico são preparados antes de escrever. `EditorImportTransaction` verifica SHA256 dos bytes reais do arquivo, publica fonte e registro sob journal e permite rollback em falha. Só depois do commit o documento, registro e histórico preparados substituem seus estados vivos.

Undo/Redo guarda os bytes originais exatos, incluindo espaços não canônicos, o registro do asset e somente os objetos afetados. Antes de repetir, verifica arquivo, registro e valores desses objetos; alterações concorrentes recusam a operação e preservam o histórico. Outros objetos e assets não são restaurados por snapshot global. As dependências são reconstruídas com owners reais de mesh, clip e script registrados.

O Apply marca a cena alterada; a gravação do arquivo de cena continua sendo a ação Save existente. O journal protege fonte/registro em disco, não recupera uma cena ainda não salva após encerramento do processo. O histórico de Undo permanece em memória e não é persistido entre sessões.

## Recorte e custos

São suportadas propriedades de objetos e componentes já existentes, campos de script e componentes atômicos sem reflexão individual. Não há Apply estrutural, nested prefabs ou variantes. Qualquer mudança de estrutura, ownership, identidade ou ordem de componentes em qualquer instância do asset bloqueia o pacote antes da mutação. Play, comparação obsoleta, dependência indisponível e referência externa selecionada também recusam com diagnóstico.

“Todas” significa todas as instâncias carregadas no documento atual. Cenas fechadas e migração de todo o projeto não são percorridas. O preflight e a reconciliação copiam/preparam dados por instância e releem a fonte para verificar consistência: é uma operação de autoria, com custo proporcional aos objetos/endereços carregados, não um caminho executado por quadro.

## Validação host confirmada

Três cenários integrados em `tests/native/test_prefab.cpp`: Apply seletivo em três instâncias com herança, override e conflito → Undo/Redo exatos → Save/reopen; lote com conflito exigindo escolha explícita; recusa sem mutação para estrutura em outra instância, referência externa com colisão numérica de ID e edição externa de bytes. A validação central executou o filtro Prefab: **14/14 passaram**, incluindo esses três cenários novos. A compilação Android final passou; APK e hash estão na auditoria consolidada. Esses resultados não constituem evidência de dispositivo. Nenhum ADB foi utilizado.
