# Leitura de campos C# no Inspector em Play

28/09/2026. Correção da lacuna registrada nas entregas O1b e O1c.

O Inspector passa a ler os membros serializados da instância C# viva, em vez de
mostrar somente o snapshot de autoria. Captura apenas o objeto inspecionado
(incluindo Inspector travado), a cada 250 ms e antes de iniciar uma edição.
Durante uma edição textual ou arraste ativo, preserva a transação existente.
Sem inspeção em Play não há captura periódica.

BehaviorWorld resolve os membros pelo PropertyId, incluindo privados herdados,
e produz um snapshot tipado. A API nativa consulta o tamanho e copia o mesmo
snapshot; getters não são chamados duas vezes por essa consulta. ScriptBridge
valida o conjunto antes de atualizar os valores do grafo runtime. O documento
autoral e seu histórico não são escritos pela leitura. Stop descarta o estado
transitório, inclusive o buffer gerenciado da inspeção.

Nulo, valor válido e falha de leitura são estados diferentes. O editor mostra
`Nulo · editar`; tocar uma lista nula cria uma lista vazia pelo caminho real de
edição em Play. Outros tipos usam seus editores existentes. Getter que lança
exceção fica identificado e sem ação de edição, sem desligar o comportamento
ou esconder os demais campos. Uma ponte indisponível não mostra dados antigos
como se fossem atuais.

O protocolo tem limite de 1 MiB; campos e listas respeitam os limites do formato
autoral (1024 elementos e 4096 caracteres para string, com validação nativa
adicional em bytes). Valores fora do formato do editor são diagnosticados:
NaN/infinito, listas além do limite, elementos nulos em listas cujo formato
não representa nulidade individual e curvas/gradientes inválidos. Não há
normalização silenciosa desses valores. Isto é inspeção dos campos publicados
pelo schema, não debugger de todo o heap C#.

## Referência e adaptação

[Godot 4.5, Remote no Scene dock](https://docs.godotengine.org/en/4.5/tutorials/scripting/debug/overview_of_debugging_tools.html#remote-in-scene-dock)
separa a cena local dos parâmetros da execução.
[EditorDebuggerInspector, source 4.5](https://github.com/godotengine/godot/blob/4.5/editor/debugger/editor_debugger_inspector.cpp)
mantém valores inspecionados por identidade e envia edições ao alvo. A Astra
reutiliza a superfície de Inspector com a faixa de contexto Play, o grafo de
execução e o caminho de edição existentes. Não adiciona um segundo painel.

## Validação

- Builds C++, C# e Android debug passaram.
- 34 execuções de testes nativos de Play/Inspector passaram, incluindo snapshot
  incompleto sem publicação parcial e preservação do documento autoral.
- 20 testes C# passaram. O cenário verifica campo herdado, nulo versus lista
  vazia, edição, limite de lista e erro de getter sem falha de lifecycle.
- No aparelho 25053PC47G, landscape 2772×1280, projeto BlockObjects0928:
  referências Child/External/Timer, curva alterada e valor herdado 71 aparecem
  na cópia; lista Optional começa nula, é criada vazia e recebe um elemento
  pelo Inspector. A leitura periódica confirma 1 elemento no C#.
  Após Stop → salvar → encerrar → reabrir → Play, Optional voltou a nulo,
  comprovando que a edição transitória não contaminou a cena nem a nova sessão.
- APK SHA-256: `A5D8029A752537247F0FDA9282DB85A0C76AB748F77CB7793405A29622EBEFFA`.

[Capturas e logs](../validacao/evidencias/inspecao-scripts-play-20260928/README.md).
Sem ícones/conceitos novos: reutiliza controles de campo, lista, contexto e
erro existentes. Não houve benchmark de escala nem aceite em portrait.

Não altera a contagem de tipos ou itens do inventário. Etapas 5–12 do bloco
ampliado permanecem abertas; esta correção não equivale à entrega de prefabs.
