# Conexão persistente de timeout — Timer v3 / P06

Referências versionadas: [Godot 4.5 Timer](https://docs.godotengine.org/en/4.5/classes/class_timer.html), [código 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/main/timer.cpp), [workflow oficial de signals 4.5](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html) e [Unity 6000.0 UnityEvents](https://docs.unity3d.com/6000.0/Documentation/Manual/unity-events.html). Godot separa tempo de contagem e entrega timeout; Unity permite conservar callbacks autorais na cena. Astra adapta esses princípios a uma conexão tipada e limitada, reutilizando as operações reais de ativação de objetos. Não implementa dispatch arbitrário por nome ou um equivalente completo de UnityEvent/Signal. Pesquisa de vídeo encontrou [Editor vs Code](https://www.youtube.com/watch?v=Qlq8pBB2htg); nenhum frame foi assistido, portanto não se atribui decisão visual a esse vídeo.

## Fluxo e dependências

Inspector do Timer → ação enumerada + referência persistente → histórico e arquivo → grafo de Play → SceneTimers → GameWorld::setActive → invalidação dos consumidores → TimerElapsed em C#. Nenhum segundo scheduler, conexão com ponteiro cru ou EventManager foi introduzido.

Timer v3 adiciona `elapsed_action` (0 desconectado, 1 ativar, 2 desativar, 3 alternar) e `elapsed_target`. Defaults deixam cenas antigas desconectadas. v1 e v2 abrem preservando o intervalo/repetição/relógio; versões desconhecidas ou ações inválidas são recusadas. O alvo usa o descritor de referência comum: picker, validação, remapeamento em clone/prefab e API reflexiva já existente. O componente permanece repetível; cada instância possui sua própria conexão.

A ativação usa activeSelf, inclusive de receptor inativo. Um ancestral inativo continua impedindo participação na hierarquia. Ao disparar, aplica a ação antes da entrega do callback; callbacks continuam sujeitos à elegibilidade do Behavior. Desativar o próprio emissor também interrompe os seus próximos ticks e pode impedir callbacks C# desse objeto; não há reativação automática. Disparos repetidos agregados alternam pela paridade do count, sem fila de milhares de comandos. Ativar/desativar são idempotentes.

Handles são resolvidos no mundo corrente a cada atualização. Receptor ausente ou com destruição pendente não é acionado; o timer conserva sua própria contagem e seus eventos. `SceneTimers::State::connection` expõe Disconnected/Ready/MissingTarget/Rejected para depuração nativa. O Inspector usa os diagnósticos existentes de referência obrigatória quando a ação está conectada. Remover o Timer elimina seu estado na reconciliação estrutural seguinte; parar Play limpa tudo. Não há delegate capturante retido entre mundos.

## Authoring e SDK

NÃO IREI SER SIMPLISTA NO DESIGN.

`Conexão` é uma categoria do Inspector de Timer. “Ao disparar” escolhe a operação; “Receptor” aparece somente quando conectado e abre o seletor real, com pesquisa por IME. Cada confirmação tem um Undo. Nenhum painel fixo ou nova classe de componente foi acrescentado. `GameTimer.ElapsedAction` e `ElapsedTarget` são gerados do mesmo descritor que o editor lê. A ABI continua 25; os callbacks de propriedade existentes já suportam enum e referência.

O ícone `event/timeout-connection` foi desenhado em SVG, rasterizado e integrado ao atlas: objetos cujo primeiro Timer tem conexão usam a variante na hierarquia. Total: 32 schemas, 31 fachadas e 225 ícones. Não se contam as três operações como três componentes.

A captura portrait revelou ferramentas de viewport pintadas sobre o Inspector. O patch limita a navegação Painéis ao viewport estreito, retira dali a fila secundária de opções até voltar ao viewport amplo e separa a navegação inferior dos controles de enquadramento. O cenário de ponteiro verifica que essas opções não interceptam o Inspector e retornam quando o painel fecha. A proposta de imagem foi hipótese; partes dela ainda sobrepunham o cabeçalho e não foram copiadas. A captura executável posterior é a referência do resultado.

## Aceite e limites

Criar Timer → escolher Ativar objeto → pesquisar receptor inativo → escolher → salvar/reabrir → timeout ativa receptor → C# observa receptor ativo antes do callback → parar conserva autoria. Testes adicionais verificam clone/remapeamento, migração, paridade de alternar, destruição pendente e teardown. [Fixture](../../tests/fixtures/events/README.md) e [evidências](../validacao/evidencias/timer-connection-v3-20261001/README.md).

Esta implementação cobre uma conexão persistente por Timer e três operações sobre objetos. Conexões de input/contato/animação, listas de receptores, ações de componentes e callbacks gerais continuam pendentes. P06 permanece parcial; não se declara o plano P00–P20 encerrado.
