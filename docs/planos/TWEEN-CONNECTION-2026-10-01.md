# Conclusão de TransformTween — componente3 / ABI29

Referências oficiais: [Godot4.5 Tween.finished](https://docs.godotengine.org/en/4.5/classes/class_tween.html#signals), [source4.5](https://raw.githubusercontent.com/godotengine/godot/4.5/scene/animation/tween.cpp) e [workflow de sinais4.5](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html). Princípio: conclusão ocorre após todos os ciclos finitos; loop infinito e cancelamento não são conclusão. A conexão Astra é um dado persistente com ação nativa de ativação, não uma cópia do sistema completo de Callable/signals da Godot.

## Cadeia

Receita/Inspector → TransformTween3 (finished_action/finished_target) → arquivo/clone/prefab → GameWorld → SceneTweens → escrita da pose final → applyObjectActivationConnection → activeSelf → consumidores, incluindo extração de luzes. O cenário nativo possui receptores Light reais; ativação de duas composições clonadas produz duas luzes extraídas.

Ação0 desconecta;1 ativa;2 desativa;3 alterna. Referência é remapeada pelo mecanismo existente de ComponentObjectReference. Versões1–2 migram desconectadas. Um TransformTween por objeto continua a regra; não aumentou multiplicidade. O evento é aplicado uma única vez por execução finita; Restart rearma, Cancel não dispara, loop infinito não dispara, pausa/autoridade concorrente impedem avanço indevido. A tentativa acontece depois da pose final validada. Falha do receptor não desfaz a pose e não dispara tentativas silenciosas em quadros seguintes.

State mantém connectionInvoked e WorldStatus para diagnóstico nativo/Inspector; Stop limpa ambos. O snapshot ABI29 de tween não passou a expor erro da conexão. Não foi adicionado callback C# de término: C# pode consultar Completed e observar activeSelf na atualização seguinte; a fixture usa essa cadeia. Não há API de método arbitrário ou sequência/paralelo fingida.

## Autoria e UX

NÃO IREI SER SIMPLISTA NO DESIGN.

“Tween conectado” cria uma instância finita e conecta o receptor selecionado, abrindo o componente; uma operação de Undo. Categoria Conexão reúne ação, picker com busca/IME e diagnóstico. Alterações isoladas são reversíveis e persistentes. Em portrait os campos aparecem juntos; landscape usa paginação existente. Referência não preenchida continua diagnosticável pelo contrato de refs, sem fallback fake.

Ícone próprio `event/tween-completion` desenhado em SVG, rasterizado e empacotado no atlas real229; o marcador do objeto conectado usa esse conceito. Ligação ao receptor aparece somente para a instância selecionada e em edição, além da visualização de destino já existente. Não adiciona painel permanente. Estado de sucesso/erro usa resultado do serviço real.

## Validação e contagem

[Evidências e limites](../validacao/evidencias/tween-connection-v3-20261001/README.md). Três cenários de conexão passaram: arquivo/migração/clone/pose/luzes; uma tentativa/restart/cancel/infinito/receptor removido/teardown; receita/enum/picker/IME/Undo/Redo/arquivo. Regressões de controles e tempo, schemas/API gerada e atlas passaram. C#54/54 com doze fixtures.

34 schemas,33 fachadas,57 receitas,229 ícones. Componente TransformTween3; ABI29/cena16/prefab3/Timer4. Contagem de receitas/ícones não significa capacidade nova independente para cada item. P06 e P00–P20 continuam parciais; tween genérico por PropertyId, sequência/paralelo, eventos gerais e prova física Android permanecem no atlas.
