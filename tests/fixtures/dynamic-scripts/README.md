# Scripts dinâmicos e mensagens

Cenário de aceite das etapas 2 e 3 do bloco ampliado, executado pelo teste
`DynamicScripts_MessagesRemovalAndDelayedDestruction_RunTheAcceptanceScenario`
e pelo editor Android. Copie `DynamicProbe.cs` para a raiz de um projeto
isolado e os dois arquivos de `scenes/` para a pasta de cenas. Aguarde a
compilação e entre em Play.

O único objeto autoral é Driver. Durante Play o script cria uma hierarquia,
anexa comportamentos (inclusive dentro de Awake), verifica busca por tipo e
interface, mensagens locais/descendentes/ancestrais, payload zero e booleano,
ordem, inatividade, nomes duplicados, ausência/ambiguidade e remoção durante
despacho. Uma exceção **intencional** `expected receiver failure` comprova que
o receptor seguinte continua. Um objeto recebe três pedidos de destruição;
vence o primeiro prazo, em 0,15 segundo de simulação.

Aceite: Console emite `DYNAMIC READY` e depois `DYNAMIC PASS`. Em Inspect,
selecione Nested: dois DynamicReceiver devem aparecer. Stop deve deixar
somente Driver. Salve, reabra o projeto e repita Play: a nova sessão deve passar
sem herdar objetos, filas ou estado estático da anterior.

A cena não contém geometria: o viewport cinza é esperado. Não substitua cenas
de trabalho por esta fixture. O relatório da entrega está em
`docs/planos/O1B-SCRIPTS-MENSAGENS-2026-09-28.md`.
