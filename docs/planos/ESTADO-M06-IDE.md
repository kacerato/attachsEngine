# Continuidade do plano mestre — M06 / Entrega F

Data: 12/09/2026. Base consultada: `8064704`, branch `codex/gameplay-runtime`.
Este registro descreve alterações de trabalho posteriores à base, sem commit ou push nesta etapa.

## Continuação M06/M08 — 13/09/2026

[Recuperação e recursos transacionais](M06-M08-RECUPERACAO-RECURSOS.md) acrescenta análise semântica incremental, checkpoint de rascunhos, importação pelo projeto com revisão/cancelamento, registro separado de instanciação e journal de publicação. [Validação própria](../validacao/2026-09-13-m06-m08-recursos.md). M06 e M08 permanecem abertos; a seção histórica abaixo não deve ser lida como estado atualizado dessas capacidades.

## Continuação funcional após aprovação visual — 12/09/2026

O usuário confirmou “ok isso foi bem aplicado” e autorizou o próximo bloco. A
continuação está em [M06 — pacote funcional](M06-PACOTE-FUNCIONAL.md), com
[evidências separadas](../validacao/2026-09-12-m06-funcional.md). Esta atualização
prevalece sobre as pendências históricas de linguagem, busca/substituição,
agrupamento por pausa, contexto de console e puxador arrastável listadas abaixo.

- Implementados e ligados ao IDE: sugestões semânticas Roslyn, definição,
  busca/substituição inline, pesquisa nas fontes C#, preferências de recuo,
  histórico agrupado por pausa e descarte de consultas antigas.
- Implementados no console: contexto de build/Play/projeto, snippet da revisão
  compilada, altura arrastável e navegação protegida contra contextos antigos.
- Integridade: Play bloqueia fontes em erro/desatualizadas; publicação registra
  mudanças incompatíveis de schema e preserva instâncias/valores órfãos.
- ADB: correção/publicação automática, assinatura real de `ISceneAccess.Log`,
  substituição com undo, busca, duas instâncias `Alpha`/`Beta`, Play/Stop e
  reabertura com nova execução observados. O gate focal de autoria/execução
  avançou; **M06 completo continua aberto** pelas matrizes de robustez e pelos
  limites de linguagem/observabilidade descritos no pacote.

As seções seguintes mantêm o histórico da primeira implementação. Não usar as
frases “serviço Roslyn pendente” ou “conexão caiu antes de Play” como estado atual.

## Referências lidas e autoridade

- [Plano mestre completo](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md): 1.454 linhas, lidas integralmente, incluindo contratos, M00–M15, fichas de aceitação e restrições de autoria/recursos. Cópia integral do arquivo enviado `Plano_Mestre_Astra_Editor_Runtime_Assets (1).md`; SHA-256 `ee4e78053c73a12618ff8eaa71295b14fff2cb36a7eba69f337fe9c1386c2b88`.
- [Sete entregas A–G](../PROXIMO-PACOTE-GAMEPLAY.md): lidas integralmente; o estado de 11/09 e os commits posteriores distinguem trabalhos entregues de propostas antigas.
- [Registro do runtime](../runtime-gameplay.md), [refundação](../REFUNDACAO-ASTRA.md) e [decisão Android](../adr/ADR-REFUNDACAO-ANDROID-TEXT.md): histórico e contratos do caminho atual.
- Duas imagens enviadas nesta solicitação: breadcrumb/cabeçalho, abas, linhas/guias, rodapé e árvore lateral. **Somente layout**. As cores vêm de `editorTheme()`, incluindo o acento lima existente.

O arquivo mestre descreve um snapshot anterior (`c321b01`). Sua lista de defeitos não prova que todos continuavam presentes em `8064704`. O usuário declarou o viewport resolvido: esta etapa não altera projeção, grade, câmera ou shaders. A implementação inicial não usou ADB. A mensagem posterior **“adb on. continue”** autorizou a rodada de aparelho de 12/09, descrita abaixo; não é aprovação integral do plano.

## O que já existia, antes deste bloco

| Base | Implementação encontrada | Consequência para a continuação |
|---|---|---|
| M02 / Entrega F | Catálogo publicado separado do texto; relatório encenado e publicação aceita pelo hospedeiro | Reutilizar `EditorCodeWorkspace`; não criar outro sistema de componentes |
| `2a16b81` | Console de compilador, script e editor, filtros, repetição consecutiva, salto e virtualização | Completar as lacunas do console existente |
| `1a04f82` | Build automático após pausa de 1,25 s, salvamento anterior ao pedido e colagem agrupada | Acrescentar o estado real de composição/batch; pausa não prova término do IME |
| `8064704` | Barra de ícones e menu de ações menos frequentes | Evoluir a disposição conforme as imagens, preservando a identidade |
| M05 parcial | InputConnection em View invisível; texto desenhado pela engine, toque coloca caret no fim da linha | Introduzir uma projeção visível com seleção real, mantendo documento/histórico nativos |

## Implementação deste bloco

### M05 / M06.2 — documento e campo visível

`EditorCodeInput.CodeField` é um `EditText` visível, posicionado exatamente na região de código calculada pela UI nativa. Não é diálogo, editor externo, WebView ou outro documento autoral. Android fornece caret, handles de seleção, clipboard e rolagem; `EditorCodeBuffer` continua dono do texto, revisão e histórico.

- Cada buffer guarda seleção, rolagem horizontal/vertical e revisão de navegação. Abrir outra aba não reaproveita o caret global da anterior.
- Edições atravessam JNI como intervalo UTF-8 + remoção + inserção + revisão de origem. Não se manda o arquivo inteiro a cada tecla. Snapshots completos são usados ao abrir, desfazer/refazer e recuperar divergência.
- A fila JNI tem teto de 256 mensagens e 1 MiB de inserções. A thread da sessão aplica as mutações; a UI Android não toca no mundo da cena.
- Revisão antiga não sobrescreve o buffer novo. O campo aguarda reconhecimento e oferece o rascunho divergente à recuperação em `.astra/recovery/Codigo-recuperado-N.txt`, com mensagem navegável no console. A extensão `.txt` evita compilar uma segunda cópia da classe. Falha de armazenamento deve ser exercitada na aceitação; não há garantia contra processo morto antes do reconhecimento.
- Undo/redo, incluindo Ctrl+Z/Ctrl+Y e ações de contexto Android, usam o histórico nativo. Colagem e ações de recuo são transações explícitas. A digitação continua agrupada por sessão; agrupamento por pausas naturais e histórico por deltas ainda são refinamentos pendentes.
- Conversão UTF-16 ↔ UTF-8 protege limites de code points. A seleção visual de graphemes é delegada ao Android; não foi aprovada uma matriz de teclados/idiomas.
- Numeração de linhas com gutter fixo, linha atual, guias de recuo, fonte monoespaçada e scroll horizontal. O arquivo não é truncado a 512 caracteres por linha como no desenho anterior.
- Coloração lexical C# calcula em worker único, com atraso de 280 ms e descarte de resultado de outro buffer/revisão. Teto de 16 mil spans. **Não é análise semântica nem promessa de cobertura de toda a gramática C#**, particularmente strings raw/interpolação complexa.
- Barra acessória: desfazer/refazer, recuar/avançar indentação, chaves/parênteses, ponto e vírgula, selecionar/copiar/recortar/colar e recolher teclado. Undo/redo reutilizam PNGs `mark-v1` já usados pela Astra; sinais de código permanecem sinais de código.

### Input e teclado

O primeiro host usou a hierarquia da NativeActivity e preservou o gesto completo como não tratado pelo consumidor nativo. No aparelho isso revelou um problema adicional: o Vulkan sobrescrevia os pixels Android da mesma janela. O host foi corrigido para uma superfície `TYPE_APPLICATION_PANEL` anexada à Activity, limitada ao código, sem modalidade. O campo recebe seus toques nessa janela; toolbar, abas e console continuam no roteador Astra. A guarda de gesto nativa protege transições; os controladores da cena não recebem o gesto de código.

As mensagens de texto são drenadas antes de comandos nativos de aba/undo. `SOFT_INPUT_ADJUST_NOTHING` deixa a reserva de teclado a cargo do editor. Em API 30+, os insets vêm das métricas da janela completa da Activity, pois a subjanela já recortada pode informar zero. O observador do campo curto inativo não sobrescreve mais essa medida. A captura ADB confirmou status e console acima do IME no aparelho da rodada. Outros IMEs, API 26–29 e ciclo de vida adverso continuam pendentes.

### M06.1 — compilar e publicar

O debounce anterior de 1,25 s foi mantido. Agora a composição e o batch Android bloqueiam o disparo mesmo que o texto fique parado durante uma palavra composta. Ao terminar, a pausa volta a contar. A criação, a edição, o salvamento seguro e a publicação com confronto de geração continuam usando a implementação existente.

O comando ocasional chama-se **Recompilar projeto** no menu; não reaparece um Apply obrigatório. O status distingue rascunho, composição, compilação, erro e publicação. Falha com diagnósticos abre o console sem transferir o foco ao painel.

Na rodada ADB, o compilador passou a aceitar projeto sem fontes C# como assembly
vazio válido, inclusive para permitir remover o último tipo publicado. O erro
ASTRA002 era incorreto para a criação de um projeto vazio. Console registra início
automático, resultado e confirmação/recusa de publicação, com revisão nos eventos
de início/publicação. Isso não substitui o futuro contexto estruturado de EventId/Play.

### M06.2 — layout e navegação

- Cabeçalho compacto com voltar à cena, arquivos, caminho atual, salvar, busca e menu.
- Abas com indicador ativo e edição não salva; controles para alcançar abas excedentes. As regiões de toque são registradas apenas para abas visíveis.
- Arquivos abrem em gaveta, inclusive em telas onde o painel anterior sumia por largura. A árvore reutiliza o filesystem existente, com recuo, chevrons, seleção e ações de renomear/apagar já presentes.
- Nova pasta pelo painel; criação de componente C# e auxiliar C# na pasta selecionada, ou `Scripts` como destino inicial. O auxiliar não recebe `Behavior`/`ComponentId`; modelos continuam opcionais.
- Busca no buffer com ocorrência anterior/próxima e wrap; “Ir para linha”; diagnóstico usa linha e **coluna UTF-16 do compilador**, não o fim da linha.
- Rodapé com linha/coluna, C#/Texto, UTF-8, LF/CRLF, rascunho e estado do build. Não há TSX fictício por causa das imagens.

### M06.3 — console

- Limpar registros preserva o bloco de diagnósticos do último build. Uma compilação nova substitui esse bloco.
- O teto de 512 entradas passa a incluir teto de 1 MiB de conteúdo, limite por mensagem e aviso de descartes. Logs não expulsam diagnósticos do compilador para abrir espaço. Diagnósticos além do teto de exibição continuam no relatório do workspace.
- O agrupamento também distingue coluna. Filtros continuam apenas ocultando entradas, sem apagar dados.
- Console inicia recolhido, com contadores visíveis; expande em falha de compilação. Cabeçalho distribui filtros conforme a largura disponível.

## O que impede declarar M06 concluído

| Item do plano completo | Estado após este bloco | Continuação concreta |
|---|---|---|
| M06.1, IDE04/IDE08 | Confronto de geração existente e composição integrada; compilado, não exercitado nesta rodada | Rodada autorizada de builds fora de ordem, fechamento/troca de projeto, IME segurando composição, falha de disco e recuperação |
| M06.2, linguagem | Criação C#, coloração lexical e navegação implementadas | Serviço Roslyn de autocomplete/source navigation semântica, com cancelamento, contexto e revisão; registro de provedor sem listar linguagens inexistentes |
| M06.2, edição | Campo/seleção/clipboard/abas/busca/recuo implementados | Busca/substituição, busca de arquivos do projeto, agrupamento natural do histórico, preferências de recuo e tabulação visual uniforme |
| M06.3, OBS | Problemas/registros, detalhe integral paginado, copiar/exportar recorte, filtro por origem/texto e EventId implementados nesta entrega; validação focal registrada em 2026-09-12-ide-v2.md | Contexto estruturado de geração/Play e cobertura de todos os produtores M01; snippet associado à revisão; painel arrastável |
| M05 / TXT | Campo de código visível integrado | Campos numéricos ainda usam o adaptador curto anterior; validar cancelamento, teclado real, graphemes, seleção longa e rolagem após troca de foco |
| M11 / retrato | Antecipado por pedido e aprovação explícita do usuário: orientação real por workspace, escala e IME; validação focal registrada em 2026-09-12-ide-v2.md | Matriz completa de pausa, retomada, picker externo, processo morto, tablets/foldables e builds em trânsito |
| Gate de M06 | **Pendente** | Criar → colar → corrigir → compilar/publicar → executar pelo aparelho, com duas instâncias preservadas e console mostrando o percurso |

## Relação com as sete entregas

A (runtime/schema), B (recursos/importação), C (aparência), D (física) e E (input/gameplay) mantêm os estados e limitações documentados na tabela A–G. Não foram reclassificados como completos por esta alteração. F recebeu o bloco descrito acima. G continua dependente da rodada integrada autorizada. M06 do plano mestre não equivale ao antigo M6 de aparência da primeira refundação; os identificadores dos planos não devem ser misturados.

Depois da conclusão de M06, os próximos consumidores são M08 (assets/reimportação) e M10 (workspace/comandos), considerando M07 já parcial. MaterialAsset/slots/texturas, reimportação com conciliação, sensor por colisor/CharacterVirtual, exportação e os demais componentes do catálogo continuam pendentes onde assim registrados. Não há implementação presumida por existir na Unity ou ItsMagic.

## Evidência desta etapa

Compilação, integração de tipos e link são evidência técnica limitada. A rodada
ADB autorizada encontrou e corrigiu composição de superfícies, escala e reserva
de teclado. Confirmou criação pela interface, edição, compilação/publicação
automática, erro/correção, salto ao diagnóstico e colagem com undo/redo. As
capturas e limites estão no [registro de validação](../validacao/2026-09-12-m06-ide.md).

A conexão caiu antes de concluir anexação/Play do novo script; a reconexão ao
endpoint mDNS foi recusada. **O gate M06 continua pendente**, assim como os itens
de linguagem, console e robustez da tabela. A existência de uma captura correta
não aprova todas as fichas IDE/OBS/TXT ou a retomada de processo.

## Fontes técnicas consultadas

- [Android EditText](https://developer.android.com/reference/android/widget/EditText): campo visível e edição/seleção.
- [Android InputConnection](https://developer.android.com/reference/android/view/inputmethod/InputConnection): batch e composição do IME.
- [Android WindowInsets.Type](https://developer.android.com/reference/android/view/WindowInsets.Type): insets do teclado.
- [AOSP ViewRootImpl](https://android.googlesource.com/platform/frameworks/base/+/android12-release/core/java/android/view/ViewRootImpl.java): `NativePostImeInputStage`, encaminhamento quando o consumidor nativo não trata o evento.

Essas fontes justificam o contrato de integração; não constituem evidência de execução da Astra.

### Direção visual aprovada — 12/09/2026

Usuário aprovou a proposta M06/M11, excluindo a barra de status Android desenhada no mockup. Aplicação rastreada em [design](../design/m06-ide-v2/README.md). A autorização ADB permanece válida. A entrega não fecha automaticamente todos os gates M06/M11 ou as outras etapas A–G.

A aplicação aprovada foi instalada e recebeu validação focal de retrato/IME, edição/publicação, console/detalhe/exportação/busca e retorno à cena. Ver [evidências e limites](../validacao/2026-09-12-ide-v2.md). O frame transitório de rotação, clipboard externo, matriz completa de lifecycle e gate de execução permanecem explicitados.
