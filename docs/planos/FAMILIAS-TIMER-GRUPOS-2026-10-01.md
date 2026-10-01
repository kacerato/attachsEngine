# Encerramento de F009 e F011

Esta revisão reconcilia os requisitos do catálogo com os sistemas existentes; não cria novos tipos nem converte testes antigos em execução nova. O usuário autorizou encerrar esta rodada com evidência host/APK, mantendo qualificação física integrada separada.

## F009 — Timer

O componente `scene::Timer` v4 conserva intervalo, repetição, enabled, autostart e escolha de relógio. `runtime::SceneTimers` é o scheduler real da sessão: identidade objeto/instância, início, parada, pausa/retomada, restante, agregado de disparos, reconciliação de criação/remoção e reset ao sair de Play. `EditorSession` usa esse mesmo scheduler nos controles da Inspeção; não modifica o documento para executar Start/Stop. O Bridge entrega `Behavior.TimerElapsed` e comandos/snapshots pela ABI. `GameTimer` e `GameTimerRuntime` são os consumidores C# publicados.

Os [controles e limites de relógio](TIMER-CONTROLS-2026-10-01.md) e a [conexão persistente](TIMER-CONNECTION-2026-10-01.md) registram diferenças intencionais de default frente à Godot. [Aceite host registrado](../validacao/evidencias/timer-controls-abi28-20261001/README.md): fluxo de autoria, arquivo/legado, Play, comandos reais, restante, callback, remoção e teardown. As capturas executáveis também estão nesse pacote. Não houve nova execução desses cenários nesta reconciliação.

Referências concretas: [Godot 4.5 Timer](https://docs.godotengine.org/en/4.5/classes/class_timer.html) e [fonte 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/main/timer.cpp). F009 exige o timer completo com esses contratos Astra; F008 permanece responsável por conexões gerais, listas de receptores e assinaturas arbitrárias. Fechar F009 não fecha F008 nem P06.

## F011 — Grupos, tags e consultas

`runtime::ObjectGroups` mantém associações múltiplas persistentes, nomes únicos e limites explícitos. `GameWorld` modifica sua própria cópia em Play e consulta snapshots no mundo corrente, com filtro de ativos e exclusão de destruições pendentes. `World.cs` valida o handle antes de `Groups`, `IsInGroup`, `AddToGroup`, `RemoveFromGroup` e `FindGameObjectsInGroup`. As tags e consultas de tag usam o mesmo mundo. `Layer` é um campo separado, validado em 0–31 pela ABI34; não é derivado de membership.

[Contrato de grupos](GROUPS-2026-10-01.md) e [aceite host registrado](../validacao/evidencias/groups-abi25-20261001/README.md): IME/rota real de associação, rename/remove, Undo, arquivo, prefab/clone, snapshots, ramo inativo, destruição e reimportação. [Base ABI34](FAMILIAS-BASE-2026-10-01.md) cobre objeto/layer e escopo dos handles. Não houve nova execução dos cenários de grupos nesta reconciliação. O despacho por nome de método `call_group` e catálogo global descritivo não fazem parte dos requisitos F011.

Referências: [Godot 4.5 Groups](https://docs.godotengine.org/en/4.5/tutorials/scripting/groups.html), [Node 4.5](https://docs.godotengine.org/en/4.5/classes/class_node.html#class-node-method-add-to-group) e [fonte 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/main/node.cpp).

## Pacote e limite físico

Os contratos de Timer e grupos foram acrescentados antes da ABI34; os callbacks continuam obrigatórios em `NativeBehaviorRuntime.SceneAccess`, com SDK e nativo versionados juntos. O [manifest ABI34](../validacao/evidencias/families-prefab-20261001/package-manifest.json) comprova o build Android e igualdade dos assemblies/atlas gerados e empacotados. Isso é evidência de pacote, não execução dos consumidores no CLR Android. Nenhuma instalação, execução ou interação física nova foi realizada para este encerramento.
