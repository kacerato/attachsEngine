# Bloco B — Conexão de evento (F008)

Branch `claude/api-componentes`. Segundo bloco do [roadmap de API e componentes](ROADMAP-API-COMPONENTES-2026-10-06.md); usa os métodos e eventos do [bloco A](API-METODOS-EVENTOS-ABI42-2026-10-06.md).

## O que passou a ser possível

Montar comportamento sem script no Inspector: **quando este objeto emitir um evento, faça uma ação**. Exemplos que agora funcionam do editor ao Play:

- Sensor 3D ou 2D: alguém entrou → tocar o próprio som (receita **Gatilho sonoro**).
- Sensor: alguém entrou → iniciar o Timer da porta com intervalo 0,5 s.
- Timer disparou → ativar, desativar ou alternar outro objeto.
- Tween concluiu → reiniciar outro tween, parar um percurso ou pausar um áudio.

A conexão é um componente repetível (`astra.logic.event_connection`, família Lógica › Eventos). Ela mora no objeto emissor, como o UnityEvent mora no componente que o dispara e as conexões Godot ficam salvas no nó de origem.

## Contrato

| Propriedade | Tipo | Significado |
|---|---|---|
| `enabled` | booleano | Desligada não reage; a configuração é preservada |
| `event` | enumeração persistente | Evento declarado por um componente do próprio objeto (10 eventos hoje) |
| `other_filter` | referência | Só contatos com este outro objeto; visível só para eventos de contato |
| `action` | enumeração | Desconectado, ativar, desativar, alternar objeto ou chamar método |
| `method` | enumeração persistente | Método declarado (15 hoje); visível só em "Chamar método" |
| `argument` | número, 0–3600 | Intervalo do Timer (zero usa o autorado) ou posição do áudio; visível só para esses métodos |
| `receiver` | referência | Objeto ativado ou dono do componente chamado; vazio usa o próprio emissor |
| `once` | booleano | Reage só ao primeiro evento de cada execução de Play |

Os valores das enumerações são identidades gravadas na cena, separadas do índice do evento ou método dentro do tipo. `auditEventConnectionCatalog()` exige que todo evento declarado e todo método acionável (sem retorno, com zero ou um argumento numérico) tenham identidade na conexão, e que toda identidade aponte para um descritor com assinatura que a conexão sabe montar. Um tipo que ganhar evento ou comando sem entrar no catálogo quebra o teste.

## Cadeia

```text
Inspector / receita → EventConnection (arquivo v1, histórico, clone com referências)
Play: emissor real → ComponentEventQueue → cursor Connections
  → SceneEventConnections (no ponto seguro de drainCommands, antes do flush estrutural)
  → GameWorld.setActive  |  invokeComponentMethod (mesma porta dos scripts)
  → diagnóstico no console do editor quando o receptor ou o componente falta
```

A fila é processada em todo ponto seguro do quadro, inclusive depois dos tweens, de modo que um evento emitido em qualquer fase é atendido no mesmo quadro ou no início do seguinte. As ações executadas durante a entrega podem emitir novos eventos; esses ficam para a próxima passagem, o que limita cascatas.

## Editor

- Add Component › Lógica › Eventos, com busca por "UnityEvent", "Signal", "Gatilho".
- Grupos **Quando** (evento, filtro) e **Então** (ação, método, valor, receptor). Os campos condicionais usam os mesmos predicados da validação: método, valor e filtro só aparecem quando têm consumidor.
- No viewport, a instância em edição mostra a linha até o receptor, como as conexões físicas.
- Duas receitas no menu Criar › Gameplay: **Conexão de evento** (liga o objeto selecionado como receptor) e **Gatilho sonoro**.
- Ícones novos `component/event-connection` e `event/sound-trigger`, gerados por `tools/generate-component-icons.py` e empacotados no atlas real.

## Relação com as conexões especializadas existentes

Timer (`elapsed_action`), TransformTween (`finished_action`) e as Conexões físicas 3D/2D continuam funcionando e lendo os mesmos arquivos. A Conexão de evento cobre o que elas não cobrem: chamar métodos e ligar qualquer evento declarado. As conexões físicas também tratam **Stay**, que a fila de eventos deliberadamente não transporta.

Condição para remover a duplicação: quando a fila transportar Stay com custo medido, as quatro formas especializadas migram para Conexão de evento por migração de arquivo transacional. Até lá, as duas coexistem com responsabilidades distintas.

## Referências

- Unity 6000.0 [UnityEvent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html) e [Manual: UnityEvents](https://docs.unity3d.com/6000.0/Documentation/Manual/unity-events.html): chamadas persistentes configuradas no Inspector, alvo + método + argumento.
- Godot 4.5 [Using signals](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html): conexão salva na cena, a partir do nó emissor.

| Aspecto | Astra | Classificação |
|---|---|---|
| Lista de chamadas por evento | Uma ação por componente; várias conexões no mesmo objeto | Adaptação explícita: cada ação tem Inspector, histórico e diagnóstico próprios |
| Argumento | Um número quando o método o declara | Adaptação explícita: tipos de valor fechados no contrato do bloco A |
| Execução | No ponto seguro, não dentro do passo físico | Adaptação explícita |
| Chamada a Behavior C# por nome | Não oferecida | Pendente: exige campo texto autoral e validação contra o catálogo de scripts |
| Alterar propriedade arbitrária | Não oferecida | Pendente: exige seletor de propriedade e valor tipado no Inspector |

## Validação executada (06/10/2026)

- Host C++: `event_connection_*` **5/5** — catálogo auditado contra os descritores, arquivo/validação/clone, Timer → ativação e sensor Jolt → `Timer.start(0,5)` com filtro e diagnóstico de componente ausente, `once` por sessão de Play, receitas em um passo de Undo. Suíte completa sem falhas novas em relação à base.
- UI executável (`aether_ui_preview`), capturas em `docs/validacao/evidencias/event-connection-20261006/`: Quando, Então (ativação e chamada de método), catálogo Add em 853×394 e Então em 1200×700. Zero glifos ausentes e zero instâncias descartadas; 3 recortes de desenho no viewport em 1200×700, nenhum no Inspector. A primeira captura mostrou abas na ordem Então/Conexão/Quando e "Então" paginada; a ordem foi corrigida e a ativação passou a caber numa página do telefone. Chamar método ainda pagina no telefone (Ação/Receptor e Método/Valor).
- Android: biblioteca arm64 compilada (ver bloco A). Sem execução no aparelho: interação por toque, IME do picker e áudio real do Gatilho sonoro continuam a qualificar quando houver ADB.
- Sem imagem conceitual nesta rodada: a captura inicial apontou só problemas de ordem e densidade, corrigidos na UI real.
