# M06 — linguagem C#, edição e contexto de execução

Continuação de 12/09/2026, sobre `8064704` em `codex/gameplay-runtime`, com alterações anteriores preservadas. O usuário aprovou a revisão visual aplicada e autorizou prosseguir com o pacote funcional. A autorização ADB foi mantida. Este documento complementa o [plano mestre](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md) e o [estado anterior](ESTADO-M06-IDE.md); não declara os demais marcos concluídos.

## Entrega ligada ao produto

| Área | Comportamento implementado | Caminho de uso |
|---|---|---|
| Sugestões C# | Símbolos Roslyn acessíveis no contexto, tipos dos buffers abertos, assinatura e indicação de sobrecargas; substituição do identificador inteiro, incluindo sufixo após o caret | Pausa na digitação ou menu da barra acessória → Sugestões C# |
| Definição | Resolve símbolo e navega à declaração da fonte do projeto; símbolos de assemblies mostram a assinatura sem inventar um arquivo | Posicionar caret → menu → Ir à definição |
| Buscar/substituir | Área inline acima do código, anterior/próxima com retorno ao início, contagem, substituição individual ou de todas as ocorrências | Lupa do IDE ou menu → Buscar e substituir |
| Busca de projeto | Pesquisa textual nas fontes C#, incluindo rascunhos abertos; resultado mostra arquivo, linha e trecho e abre a ocorrência | Área de busca → No projeto, ou menu → Buscar nas fontes C# |
| Recuo | Preferência persistente de 2 espaços, 4 espaços ou tabulação; recuo/recuo inverso por seleção | Menu da barra acessória → Recuo; Tab e Diminuir recuo |
| Histórico | Digitação separada por pausa de um segundo, mudança de direção, quebra de linha ou intervalo descontínuo; substituição completa permanece uma transação | Desfazer/refazer existente |
| Console | Geração de build, sessão de Play e projeto no detalhe; snippet imutável do diagnóstico; altura arrastável pelo puxador superior | Console → evento; arrastar puxador |
| Componentes | Aviso explícito para tipo não resolvido, campos removidos preservados e alteração incompatível de tipo; não remove instâncias para corrigir schema | Publicação de código → console/inspetor |
| Play | Início recusado durante build, com fonte alterada ou build falho; Stop continua disponível durante build | Play da cena ou do IDE |

A busca no arquivo é literal e distingue maiúsculas/minúsculas. A busca nas fontes C# é literal e não distingue caixa. Não há regex nem substituição em massa no projeto nesta entrega. Não são anunciados provedores de linguagens que a Astra não implementou.

## Arquitetura e identidade

`EditorCodeBuffer` continua dono do documento e do histórico. `EditorCodeInput` permanece o campo Android visível. A consulta não salva arquivos, não emite assemblies, não publica catálogo e não instancia comportamentos.

1. O campo envia operação, posição UTF-16 e termo com token, buffer e revisão.
2. A thread da sessão tira um snapshot da raiz, caminho e buffers C# abertos, acrescentando a geração do workspace. A UI Java não lê nem altera a cena.
3. `android_main` mantém um worker assíncrono de linguagem separado do worker de compilação. Uma nova solicitação substitui a pendente e pede cancelamento da anterior.
4. `NativeLanguage` usa `ProjectCompiler.ReadSources` e as mesmas referências de metadata, com overlays dos buffers. A análise C# 12 usa `SemanticModel.LookupSymbols`, `GetSymbolInfo`, `GetTypeInfo` e `GetDeclaredSymbol`.
5. Antes de entregar, o nativo confronta buffer, revisão e geração. O campo confronta novamente token, documento, revisão e caret antes de aceitar uma sugestão. Respostas antigas não substituem texto novo.
6. Resultado de busca/definição entra na navegação normal do workspace, com linha/coluna e revisão de visualização. Abertura e edição continuam sujeitas aos mesmos limites do documento.

O worker tem cancelamento cooperativo e prazo de oito segundos. O lock managed protege somente o ownership da consulta e a resposta; não fica tomado durante a análise. O desligamento aguarda o worker antes de destruir o host .NET. A consulta usa buffers abertos sem exigir que a compilação termine primeiro.

### Redimensionamento após rotação

A rodada integrada encontrou uma corrida na volta da cena ao código: o Android
entregava `CONFIG_CHANGED` com a dimensão anterior; depois enviava
`WINDOW_RESIZED`, que era ignorado. A swapchain intermediária podia continuar
apresentando com sucesso e deixar todo o IDE esticado, enquanto o campo Android
aguardava um aspect ratio compatível. O shell agora agenda a conciliação 120 ms
após o último resize e depois de terminar a inicialização assíncrona do renderer.
Compara a janela com a extensão visível transformada da swapchain, respeitando
preTransform; se divergirem, recria a superfície e reidrata os consumidores
existentes. Não altera câmera, grade, projeção ou dados autorais. Ainda pode
existir um frame transitório durante a rotação; não foi feita uma matriz de
dispositivos ou um redesenho da transição.

Limites explícitos: 16 buffers, 512 KiB por fonte, 1.024 fontes e 32 MiB por projeto; pedido até 9 MiB; resposta nativa até 1 MiB; 80 sugestões, 64 destinos de definição e 200 ocorrências de busca. A UI informa resultados limitados. As referências de metadata são reutilizadas, mas as árvores/compilação de consulta são reconstruídas: **não é ainda um workspace Roslyn incremental**.

O completion é uma consulta semântica de símbolos, não o `CompletionService` completo do Visual Studio. Não inclui automaticamente `using`, snippets, refatorações, rename de símbolo ou cobertura exaustiva de todos os contextos C#. A coloração continua lexical. Consultas não constituem prova de que uma API externa exista ou funcione na Astra: só refletem os símbolos disponíveis no projeto e nas referências efetivamente empacotadas.

## Compilação, diagnóstico e execução

O relatório passa de `ASTRA_CODE1` para `ASTRA_CODE2`, acrescentando linha inicial e trecho da fonte a cada diagnóstico. O parser nativo aceita as duas versões. O trecho é capturado dos inputs efetivamente compilados, com até três linhas e 300 caracteres UTF-16 por linha, sem cortar um surrogate pair. Abrir outra aba ou editar o arquivo não muda esse trecho histórico.

Conclusão de build obsoleto não substitui os diagnósticos atuais nem reabre o console de erros de outra geração. O estado de falha precede catálogo vazio: um primeiro build com erro também bloqueia o início normal de Play.

Entradas do console levam `buildGeneration`, `playSession`, `project` e, quando a origem fornece, `component`. A deduplicação inclui esse contexto. Logs de script capturam o mundo e a geração efetivamente escolhidos no início de Play. A ABI atual do log de script informa objeto, mas não instância de componente; esse ID não é inferido. Mensagens de conciliação de schema conhecem a instância e a registram.

O console rejeita navegação para outro projeto ou para objeto de uma sessão Play encerrada. Quando o build do diagnóstico não corresponde ao publicado, abre a fonte atual sem fingir que aquela linha ainda é o ponto exato; o trecho original permanece disponível no detalhe. Geração de build é contextual ao workspace, não uma identidade histórica global de conteúdo entre reinícios.

Campos que deixaram o schema ficam serializados, com aviso, para permitir correção futura. Mudança incompatível de tipo não converte nem apaga o valor automaticamente; o runtime existente recusa aplicação incompatível. Não há hot reload de objetos em execução nesta entrega. O caminho normal exige fonte atual publicada antes de iniciar Play; não foi criada opção de executar deliberadamente o último assembly válido com fontes em erro.

## Validação e pendências reais

Resultados, APK e capturas estão no [registro funcional](../validacao/2026-09-12-m06-funcional.md). O fluxo com duas instâncias reutiliza `BehaviorWorld` e a serialização existentes; esses sistemas não foram reimplementados nem considerados novos por terem sido exercitados nesta rodada.

Permanecem abertos: matriz IME/graphemes/lifecycle/falha de armazenamento; build fora de ordem no aparelho; edição e compilação de projetos grandes; workspace semântico incremental; registro extensível de provedores; histórico por deltas em vez de snapshots; campos numéricos realmente inline; identificação da instância na ABI de logs; produtores de importação/render/lifecycle no barramento M01. O console arrastável guarda a altura na sessão, sem preferência persistida entre processos.

O gate focal de autoria e execução não aprova automaticamente todas as fichas IDE/OBS/TXT, M11 ou as entregas A–G. M08 (assets e reimportação), M10 (comandos/workspace) e os demais componentes continuam conforme suas dependências, sem presumir implementação por comparação com outra engine.

## Referências primárias

- [Roslyn SemanticModel](https://learn.microsoft.com/en-us/dotnet/api/microsoft.codeanalysis.semanticmodel?view=roslyn-dotnet-4.14.0): resolução semântica e símbolos acessíveis.
- [Roslyn FAQ](https://github.com/dotnet/roslyn/blob/main/docs/wiki/FAQ.md): árvores, compilação e análise do código.

São referências técnicas do serviço de linguagem. As evidências da Astra são o código ligado ao produto, a compilação e os resultados observados descritos no registro separado.
