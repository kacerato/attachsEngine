# PathProbe — aceite Android P15a

Copiar PathProbe.cs para a pasta de scripts do projeto P15a-ADB-20260930 e anexar **uma** instância de PathProbe a um objeto de diagnóstico pela UI real. Classe PathProbe, ComponentId acceptance.path-follow. Não anexa automaticamente, não cria Path/Follower e não necessita arquivo de engine.

Criar receita Caminho Bézier e depois Seguidor de caminho com o Path selecionado para associar target. Configurar inicial0, enabled/objeto ativos e Speed>0 (1 é suficiente) ou modo Duration (3–7s recomendado); curva deve ter comprimento mundial>.01. Não colocar física/animação/constraint ou outro writer no seguidor. Pode anexar Behavior em outro objeto: a busca sobe até root e encontra par existente pelo target real.

Em Play o script lê pontos por índice e ID, lista IDs, amostra comprimento/posição/tangente mundial via API22, reinicia o seguidor, observa avanço de progresso E pose; Stop deve conservar ambos por .4s; Restart deve produzir novo avanço. Logcat Astra.Script contém somente marcos PATH WAIT/READY/ADVANCE/STOP/RESTART/PASS/FAIL. WAIT nunca equivale a PASS. Erro de API gera FAIL único; falta de curva/target não cria fallback. O probe termina após PASS/FAIL/timeout; reiniciar Play inicia nova execução.

Só comandos de playback alteram o GameWorld Play; pontos e propriedades authoradas são somente lidos. Não testa point-CRUD por este script nem promete prova visual, persistência ou audibilidade. Root deve capturar UI real e comparar editor.aescene antes/depois/save/reopen separadamente. A validação física foi realizada pelo agente principal; este agente atualizou apenas o relato abaixo.

## Validação Android real — 30/09/2026

No projeto P15a-ADB-20260930, PathProbe foi compilado automaticamente ao abrir o projeto e anexado pela UI real: catálogo → prévia → Adicionar. A fachada correta usada pelo script é Astra.Components.PathComponent.

Foram registrados PATH PASS às 19:32:28.305 e 19:35:03.303, além de PASS após reabertura no arquivo path-probe-reopened.log. Artefatos: [evidências Android P15a](../../../docs/validacao/evidencias/p15a-android-20260930). A reabertura e os logs são evidências distintas das capturas de interface.

Um Cubo filho da receita possuía Body/Collider, e a autoridade física bloqueou o PathFollow. Ambos foram retirados pela UI, e o cenário voltou a registrar PASS. Isso confirma o limite de disputa de autoridade; não demonstra coexistência de controle físico e PathFollow. Mensagens WAIT observadas antes da configuração correta permanecem WAIT e não contam como aceite.
