# Plano mestre de reconstrução da Astra

**Editor, código, componentes, importação estrutural e runtime mobile**

Base: `codex/gameplay-runtime` · `c321b01f6a4b` · 11 de setembro de 2026

# 1. Missão, contrato de entrega e leitura

Este plano une a refundação do editor, o atlas de componentes e o atlas gráfico em uma sequência executável para a branch `codex/gameplay-runtime`. A prioridade é construir uma ferramenta autoral confiável: abrir um projeto vazio, importar conteúdo preservando sua estrutura, combinar componentes, escrever comportamentos, executar e continuar trabalhando depois de interrupções. A Astra continua sendo uma engine no celular para produzir jogos destinados ao celular. Computador e serviços remotos podem ser ferramentas auxiliares, não pré-requisitos ocultos do fluxo de criação.

**A unidade de capacidade é uma composição reutilizável, não uma demonstração.** Não haverá um comando fundamental “criar carro” que esconda modelo, física, controles e câmera. Haverá objetos, hierarquia, recursos de malha/material, corpos, formas de colisão, juntas, input, comportamento e cenas reutilizáveis. O usuário poderá construir um veículo, elevador, ferramenta, personagem ou máquina com essas peças. Um asset de veículo criado pelo usuário é legítimo; transformá-lo em requisito estrutural do núcleo não é.

Este documento é um plano técnico fundamentado em leitura de código e documentação. Nenhuma alteração foi aplicada ao repositório e nenhum APK foi compilado, instalado ou testado para esta entrega. Os testes do capítulo de aceitação são requisitos a executar. As contagens citadas pelo autor do commit não foram reproduzidas aqui. Uma ocorrência no código pode sustentar uma hipótese forte sem demonstrar que ela é a única causa de um sintoma observado no aparelho.

## 1.1 Snapshot e regras de verdade

| Campo | Base desta análise |
|---|---|
| Repositório | kacerato/attachsEngine |
| Branch solicitada | codex/gameplay-runtime |
| Commit fixado | c321b01f6a4bb83f715cf1bb26f4c18617e4e592 |
| Data do commit | 11/09/2026, 16:36:54 UTC |
| Evidência consultada | Código selecionado da branch, planos anteriores, conceito anterior de IDE e documentação primária |
| Não observado | Execução do APK, trace do defeito de retomada e GLB específico que perdeu a porta |
| Imagem ainda esperada | Novo layout de menu por toque prolongado mencionado na mensagem; não foi recebido nesta etapa |

As referências `[GHxx]` apontam para arquivos no commit fixado. `[Rxx]` identifica documentação externa; `[ARxx]`, planos anteriores; `[IMG01]`, uma referência visual anterior. Símbolos marcados como **propostos** não devem ser confundidos com classes existentes. Antes de implementar, comparar o HEAD local com o snapshot e registrar diferenças. Não repetir conclusões sobre a antiga `main`: nesta branch já existem `runtime::SceneGraph`, componentes tipados, picking de malha e infraestrutura de código. [GH01] [GH07] [GH10] [GH11]

## 1.2 Três níveis de escopo

**Correção imediata:** evitar componente invisível, escrita em modal, sumiço visual após retomada, grade sobre a geometria e destruição da estrutura importada. Esses problemas interrompem a autoria e vêm antes de efeitos adicionais.

**Fundação de produto:** IDE integrado, console, projetos/arquivos, importação transacional, hierarquia, inspetor universal, composição e execução previsíveis. Cada entrega precisa atravessar dados, UI, runtime e persistência.

**Expansão de capacidade:** materiais/shaders autoráveis, compute, animação, ferramentas, iluminação e desempenho avançados. O atlas gráfico continua valendo como catálogo de técnicas, mas os recursos são integrados sobre os contratos da fundação. Não se exige implementar tudo do HDRP para fechar o primeiro editor confiável. [AR01] [AR02]

## 1.3 O que não será aceito como conclusão

Trocar “Editar” por um lápis que continua abrindo o mesmo diálogo não entrega edição direta. Ocultar um erro mantendo um script antigo em execução não entrega aplicação automática. Mostrar nomes dos nós como uma lista plana não entrega hierarquia importada. Fazer a grade sumir inteiramente não corrige profundidade. Reiniciar automaticamente o aplicativo depois de toda pausa não corrige o ciclo Android. Menos draw calls à custa de perder portas selecionáveis não entrega otimização compatível com uma engine autoral.

O fechamento deve demonstrar três composições diferentes, criadas pelo mesmo conjunto de ferramentas, com arquivos externos, scripts próprios, gravação/reabertura e execução fora do editor. Não são presets do produto nem botões específicos: são provas de generalidade.

# 2. Diagnóstico da branch: observado, provável e pendente

## 2.1 O editor de texto separado é comportamento implementado

`EditorTextInput.java` cria um `AlertDialog` com `EditText` para código, números e outros textos. O caso de código expande o diálogo e usa confirmação explícita; há polling de pedidos a cada 100 ms. Portanto, o problema não é só o tema visual: o fluxo atual foi construído como uma edição externa temporária. A ponte precisa mudar para uma sessão de edição embutida, preservando composição, seleção e clipboard do Android. [GH03]

Não remover as proteções úteis: tokens, limites, rejeição de edição obsoleta e preservação do conteúdo em falhas continuam necessários. O que desaparece é o modal e o passo obrigatório de concluir uma cópia separada do texto. A edição deve nascer do próprio campo ou superfície de código.

## 2.2 O catálogo de scripts pode desaparecer sem o código antigo deixar de executar

Em `EditorCodeWorkspace`, `replace`, `undo`, `redo` e a criação de script limpam `scriptTypes_`. Em `applyBuildReport`, uma compilação sem sucesso também limpa esse catálogo, embora a mensagem informe que a versão aplicada anterior foi preservada. O inspetor procura o schema nessa mesma lista. O catálogo usado para descoberta/propriedades fica assim atrelado à validade instantânea do rascunho. [GH02] [GH09]

**Hipótese forte:** essa combinação explica parte do sintoma “não aparece para adicionar, mas funciona em Play”. Não prova que a instância foi excluída do documento. A UI inspecionada ainda enumera componentes anexados e registros não resolvidos. Há um segundo fator: ao focar/expandir um componente, um caminho de `buildComponents` reduz os cards a um único item com `cards.assign(1, selected)`. Paginação e foco podem reforçar a sensação de desaparecimento. [GH09]

A correção deve tratar os três níveis separadamente: instância autorada, schema conhecido e versão em execução. Não basta chamar “Aplicar” novamente nem reanexar automaticamente o mesmo comportamento, o que poderia duplicar execução.

## 2.3 A importação percorre a árvore, mas a saída é orientada a desenhos

`gltf_import.cpp` acumula matrizes durante a travessia de nós e produz `MapDrawRecord` por primitiva. A estrutura de saída exposta não inclui uma árvore autoral completa com pais e transforms locais. `EditorMapScene::import` inspecionado cria as entidades diretamente na raiz e usa o centro de bounds como posição editorial, compondo depois a geometria relativa ao centro. [GH05] [GH06] [GH07]

Isso diferencia duas capacidades: **renderizar o resultado da hierarquia** e **preservar a hierarquia para edição**. A primeira pode parecer correta numa imagem, enquanto a segunda é necessária para selecionar apenas a porta, manter o pivô da dobradiça e movimentar a carroceria sem perder os filhos. A implementação deve publicar uma representação de cena antes de extrair draws.

O importer já traz trabalhos úteis: cancelamento/progresso, limites, fatores de material, reaproveitamento de geometria e diagnósticos. O registro e a retomada de fontes do projeto também avançaram no commit. Reutilizar esses mecanismos mediante testes; não perpetuar a lista de draws como fonte da árvore. [GH01] [GH05]

## 2.4 O escopo GLB está explicitamente limitado

O contrato atual exclui imagens/texturas, skins, animações, câmeras/luzes e extensões; o código rejeita qualquer lista não vazia de `extensionsRequired`. Isso explica incompatibilidade com muitos arquivos sem significar que o modelo em si seja inválido. A expansão exige decodificadores, semântica de materiais, animação e negociação de extensões — não remover o `if` de rejeição e importar incorretamente. [GH05] [GH06]

A justificativa documentada para ausência de texturas inclui disponibilidade do decodificador de plataforma em versões de Android. Essa é uma decisão de implementação atual, não uma impossibilidade de carregar PNG/JPEG em aparelhos mais antigos. O plano exige um backend compatível com o mínimo real do aplicativo e um orçamento de memória decodificada.

## 2.5 Grade e eixos são tratados como overlay

`editor_grid.h` declara o caráter de overlay, gera até 258 linhas, define cobertura finita e alterna o espaçamento por potências de dez. Isso sustenta a revisão do caminho de profundidade e da transição de escala. Não basta alterar as cores. A causa exata da oscilação percebida ao fazer zoom precisa de captura/trace; cobertura, transição de nível, projeção e clipping devem ser isolados nos testes. [GH08]

## 2.6 Retomada com hierarquia presente: não presumir perda de cena

O relato é compatível com documento CPU preservado e projeção gráfica incompleta, recursos não republicados, visão inválida ou estado de visibilidade antigo. São hipóteses, não um diagnóstico fechado. O commit relata correções anteriores envolvendo pausa do seletor e descritores/buffers após importação, o que reforça a necessidade de testes cruzados de lifecycle e assets, sem demonstrar a mesma causa. [GH01]

Instrumentar a cadeia documento → assets resolvidos → instâncias extraídas → buffers residentes → draws submetidos → apresentação. Não substituir o documento por uma cena vazia quando o renderer retorna zero draws. Reconstruir uma swapchain e seus dependentes não exige apagar dados autorais. [R14]

## 2.7 Bases aproveitáveis e limites da inspeção

A branch já possui `EditorDocument` sobre `runtime::SceneGraph`, componentes com dados próprios, histórico, referência a assets, construção de malhas para picking e contrato de host managed. A arquitetura proposta deve completar esse movimento, não recriar um segundo SceneGraph, um segundo registry ou outro sistema de scripts isolado. [GH07] [GH10] [GH11]

O nome de um host não comprova qual VM está realmente carregada. O header de `DotNetHost` contém observações específicas sobre ABI de hospedagem e VM vendorizada. O relatório de baseline deve registrar bibliotecas, versão e caminhos efetivamente usados pelo APK; não repetir simplesmente “CoreCLR” ou “Mono” por inferência de nomes. [GH11]

# 3. Referências das grandes engines e decisões para a Astra

Unity separa console, importação de modelos e diferenças aplicadas a instâncias de recursos reutilizáveis. Godot expõe edição integrada de scripts e configuração de importação por nó/recurso. Essas referências orientam a jornada do criador, não uma cópia de arquitetura interna nem de todos os botões. [R01] [R02] [R03] [R04] [R05] [R06]

| Referência | Aprendizado aplicável | Decisão proposta |
|---|---|---|
| Console Unity | Eventos filtráveis e localização do problema | Console próprio, estruturado, relacionado a objeto, arquivo e geração |
| Script editor Godot | O código é editado na área de trabalho | Editor embutido com caret/seleção/IME, não um diálogo auxiliar |
| Importação avançada Godot | Inspeção do asset e de seus nós | Tela de importação com preview, árvore, recursos e relatório |
| Instâncias/overrides Unity | Reutilização não elimina customização local | SceneAsset + instância + overrides tipados, com reimportação reconciliada |
| Hierarquia das engines | Objetos autorais não são unidades de draw | Árvore baseada em nós/objetos; batching é uma projeção reversível por ID |
| Separação editor/jogo | Navegação editorial não é gameplay | Câmera editorial independente e mundo Play isolado |

A documentação de ModelImporter usada como referência é de fluxo de modelos e não estabelece, por si, que a Unity importe GLB nativamente pelo mesmo caminho. O alvo é reproduzir o comportamento autoral esperado, com um adaptador glTF explícito na Astra. [R02]

## 3.1 Decisões arquiteturais obrigatórias

**ADR-01 — Fonte autoral única.** Objetos, componentes anexados e referências duráveis pertencem ao documento. O renderer, o catálogo de compilação e os painéis não criam verdades paralelas sobre sua existência.

**ADR-02 — UI orientada a comandos e propriedades.** Um comando tem ID estável, predicado de disponibilidade, contexto, transação e diagnóstico. Uma propriedade tem tipo, unidade, validação e editor apropriado. Botões e atalhos chamam a mesma operação.

**ADR-03 — Layout substituível, modelo preservado.** A nova casca do editor substitui o workspace antigo progressivamente. Não reescrever física, parser, RHI e linguagem ao mesmo tempo só para redesenhar painéis.

**ADR-04 — Linguagem por provedor.** C# é o primeiro provedor a consolidar na branch. Java/Kotlin continuam úteis na integração Android; Lua pode entrar depois por decisão explícita. O seletor não promete linguagens sem compilação/execução/exportação conectadas.

**ADR-05 — Recursos versionados, identidade estável.** O conteúdo importado pode mudar sem transformar todo arquivo em um recurso novo. Hash de bytes identifica revisão, não substitui AssetId. A identidade de instância autorada também não é o índice de um draw.

**ADR-06 — Aplicação automática com publicação atômica.** Colar código atualiza o buffer e agenda a compilação; somente uma geração consistente e aprovada pode substituir a versão válida. Erros não apagam instâncias nem schemas anteriores.

**ADR-07 — Grade é parte da vista 3D editorial.** Usa convenção de profundidade do renderer; gizmo de orientação e controles 2D permanecem HUD. “Sempre na frente” deve ser política explícita para ferramentas, não efeito colateral.

**ADR-08 — Orientação pertence ao host.** O workspace de código pode solicitar retrato e o workspace de cena retornar à paisagem. A transição conserva documento/seleção/buffers e reconstrói somente projeções dependentes da janela.

**ADR-09 — Extensões gerais.** Água, veículos e personagens usam recursos, componentes e APIs comuns. Não existe ramificação do editor pelo nome de uma cena. Primitivas e cenas reutilizáveis podem ser atalhos declarativos para composição, sem privilégios de runtime.

**ADR-10 — Toda capacidade tem teste de autoria.** Expor um parâmetro no inspetor, compilar um shader ou renderizar uma demo não basta. É obrigatório criar, modificar, desfazer quando aplicável, salvar, reabrir e executar pelo fluxo de projeto.

# 4. Modelo universal: identidade, autoridade e transações

## 4.1 Camadas e responsabilidades

| Camada proposta | Dono de quê | Não deve depender de |
|---|---|---|
| ProjectSession | Projeto aberto, documentos, registry, tarefas e recuperação | Instância de Activity ou swapchain específica |
| AuthoringDocument | Objetos, componentes, hierarchy, valores e referências | Resultado transitório de compilação ou visibilidade GPU |
| CommandBus/History | Transações de edição e operações reversíveis | Widgets, strings traduzidas ou coordenadas de tela |
| AssetDatabase | Fontes, subassets, derivados, dependências e versões | Slots temporários de vertex/index buffer |
| CodeWorkspace | Documentos de texto, seleção, revisão e diagnósticos | Existência de um diálogo Android |
| ScriptBuildService | Snapshot, compilação, schemas e publicação | Texto mutável durante a compilação |
| RuntimeWorld | Instâncias de execução, física, scripts, eventos | Estado de painéis e modo de paginação |
| RenderWorld/RenderView | Projeção visível, recursos e história por vista | Nome do projeto ou tipo de demonstração |
| AndroidHost | Janela, IME, SAF, lifecycle e apresentação | Propriedade permanente do documento |

Os nomes indicam responsabilidades: mapear classes existentes antes de criar novas. Uma única classe pode inicialmente implementar duas responsabilidades com limites claros; isso é preferível a dez serviços vazios. A dependência proibida é semântica, não apenas a inclusão de um header.

## 4.2 Identificadores que não podem ser confundidos

`ProjectId` identifica o projeto; `DocumentId` e `ObjectId` identificam autoria. `ComponentInstanceId` identifica cada componente anexado, inclusive dois comportamentos do mesmo tipo quando permitido. `TypeId` e `PropertyId` identificam contratos persistentes. `AssetId` identifica o recurso; `SubassetId`, uma parte; `ContentRevision`, uma revisão do conteúdo.

`DocumentRevision`, `TextRevision` e `BuildGeneration` são versões, não identidades. `PlaySessionId` separa uma execução da seguinte. `DeviceEpoch` e `SurfaceEpoch` impedem recursos de uma geração gráfica de serem confundidos com a atual. Os tipos podem usar formatos compactos adequados à implementação; a regra é não reciclar identificadores de forma que eventos antigos atinjam objetos diferentes.

Um ponteiro de reflexão managed, um índice de vetor, um nome de classe ou um caminho de arquivo nunca deve funcionar sozinho como identidade persistente de componente. Ao migrar a implementação, preservar os IDs já gravados; gerar um mapa de migração quando o formato anterior não os possuía.

## 4.3 Registro de capacidades e propriedades

Uma definição de componente reúne ID, schema version, nome, categoria, ícone, multiplicidade, dependências, incompatibilidades e descritores de propriedades. Os descritores informam tipo, unidade, default, limites, editor especializado, referência aceita e versão de migração. Não é necessário repetir no painel um `switch` para cada componente novo.

Dependências podem ser locais, de recurso, de serviço ou de fase. Um script que altera FOV depende de câmera; um script que segue um alvo depende apenas da pose dos objetos. Um colisor descreve uma forma, não uma espécie de modelo. Não anexar automaticamente corpo dinâmico a qualquer mesh importada, pois isso mudaria a intenção da cena.

O registro alimenta adicionar componente, inspetor, serialização, diagnósticos, autocomplete, API e integração NoCode. Dados desconhecidos permanecem preservados como registros não resolvidos. Ausência do plugin não equivale à permissão de descartar suas propriedades.

## 4.4 Autoridade de transformação

No modo Edit, comandos autorais são os escritores. Em Play, corpos dinâmicos recebem pose da física; objetos cinemáticos recebem alvos de movimento integrados à física; animação e constraints possuem ordem explícita; a câmera segue a pose final no estágio definido. O sistema deve impedir dois escritores silenciosos disputando a mesma transformação.

A matriz mundial segue `world = parentWorld × local`, segundo a convenção escolhida. Reparentear oferece preservar local ou mundial. Com escala não uniforme e rotação, preservar o mundial pode exigir shear incompatível com TRS puro. Nesse caso, manter uma representação matricial autorizada ou rejeitar a transformação com explicação; nunca arredondar silenciosamente e afirmar preservação exata.

Pivô do nó importado não é centro do bounds. Um modo de gizmo “centro da seleção” é uma conveniência editorial e não deve reescrever automaticamente a origem da malha. Essa distinção é indispensável para uma porta, um braço mecânico ou qualquer parte articulada.

## 4.5 Comandos e concorrência

Comandos autorais levam alvo, revisão esperada, operação e payload tipado. A UI emite intenção; o documento valida e publica a nova revisão. Workers de importação e compilação leem snapshots imutáveis e devolvem resultados com token, geração e dependências. A conclusão é rejeitada se seu projeto foi fechado, sua fonte mudou ou sua janela já foi substituída.

Um arraste de transform ou uma sessão de edição numérica constitui uma transação, não centenas de passos de Undo. O histórico de texto é independente do histórico da cena; os ícones globais de desfazer/refazer agem sobre o foco atual, com indicação acessível. Navegação da câmera, mudança de painel e filtro do console não tornam a cena dirty.

Publicar dados CPU, registry e recursos GPU exige uma barreira lógica de commit. Em caso de falha, manter o conjunto anterior íntegro. O gerenciamento deve permitir modelos de double-buffer e filas de descarte por fence, sem obrigar `vkDeviceWaitIdle` a cada mudança de propriedade.

# 5. Scripts que nunca desaparecem silenciosamente

## 5.1 Quatro estados independentes

O sistema deve manter: **documentos-fonte em edição**, **catálogo válido conhecido**, **instâncias anexadas** e **instâncias realmente executadas**. São relacionados, mas não intercambiáveis. A implementação atual limpa o catálogo em operações comuns de texto; a nova versão não o fará como efeito de `replace`, `undo` ou `redo`. [GH02]

Uma definição de script proposta contém `ScriptAssetId`, `TypeId`, `SchemaVersion`, `BuildGeneration` e descriptors. Uma instância autorada contém `ComponentInstanceId`, referência ao tipo/recurso, `enabled` e valores serializados. Um vínculo de execução acrescenta `PlaySessionId` e a geração carregada, sem substituir a instância autoral.

| Estado apresentado | Significado | Comportamento do editor |
|---|---|---|
| Atual | Fonte e última publicação correspondem | Campos e anexação disponíveis |
| Alterado | Rascunho mais novo, ainda não compilado | Instâncias e último schema permanecem; status discreto |
| Compilando | Snapshot está em processamento | Edição continua; resultados obsoletos não são publicados |
| Erro na fonte atual | Falhou a geração nova | Console mostra erro; catálogo anterior e dados não desaparecem |
| Tipo não resolvido | Referência não encontrada na geração pertinente | Card continua visível, dados preservados, diagnóstico e relink |
| Desabilitado | Componente presente, execução desativada | Não tratar como removido |
| Apenas runtime | Componente criado durante Play | Exibir na inspeção de execução, sem persistir como autoria por acidente |

## 5.2 Lista de componentes pertence ao documento

O inspetor enumera os `ComponentInstanceId` do objeto. O schema enriquece a apresentação; não decide se a instância existe. A lista de “Adicionar componente” usa um registry versionado conhecido, não uma lista temporária limpa pelo teclado. Um tipo inválido ou temporariamente indisponível pode ser mostrado com estado e motivo, sem prometer que a versão nova executa.

No fluxo explícito “Criar e anexar”, o destino é registrado por ObjectId e a nova instância pode permanecer visível como pendente até o primeiro build válido. Colar código em um arquivo não anexa esse tipo a todos os objetos nem muda o alvo selecionado depois; publicação de tipo e anexação são operações separadas. A formatação automática, caso exista, é opcional e não reescreve o clipboard ou desloca o caret silenciosamente.

Substituir a redução implícita dos cards e a paginação por uma lista contínua recolhível, virtualizada quando necessário. Abrir um componente não oculta seus irmãos. Um modo de foco, caso exista, deve ser optativo, claramente identificado e reversível; não é o comportamento padrão. [GH09]

Referências de UI devem usar IDs, não o índice de um componente que muda depois de inserção/ordenação. Cliques atrasados precisam conferir a identidade do alvo. Adicionar o mesmo tipo várias vezes só é permitido quando seu contrato autoriza; a falta de schema não cria duplicatas automaticamente.

## 5.3 Compilar e aplicar ao colar

O caminho normal será: inserir texto no documento → finalizar a transação de colagem/composição → salvar o rascunho em armazenamento seguro → capturar snapshot → compilar/analisar em worker → validar schema e dependências → publicar atomicamente → atualizar diagnósticos e bindings pertinentes.

A compilação automática pode esperar um período curto sem edição, inicialmente testado na faixa proposta de 400–800 ms. Esse número é uma hipótese de UX, não medição ou requisito fixo. Colagem concluída agenda uma análise imediatamente; múltiplos eventos de uma única composição não geram dezenas de builds. A UI indica a geração que está compilando.

O botão habitual “Aplicar” deixa de ser necessário. Uma ação de recompilar no menu de diagnóstico pode existir para investigação, mas não será a correção escondida para scripts desaparecendo. Conteúdo incompleto é texto válido para edição, não necessariamente código executável: erros ficam no arquivo e no console sem apagar a versão anterior.

**Política inicial segura:** durante Edit, publicação de tipos/metadados é automática. Durante Play, uma geração nova pode ficar pendente até a próxima fronteira segura ou até Stop/Play, conforme o nível de recarga implementado. O aplicativo informa isso. Não reiniciar silenciosamente a simulação nem executar arbitrariamente o rascunho assim que o clipboard muda.

Quando o usuário inicia um novo Play com erro na fonte atual, o padrão é bloquear e abrir o diagnóstico. A opção explícita de executar a última versão válida deve dizer qual versão será usada. Preservar uma execução já iniciada é diferente de iniciar outra fingindo estar atualizada.

## 5.4 Publicação, migração e recarga

Uma saída de build leva hashes de fontes, versão da API, IDs de tipos e schema. Antes do commit, conferir que o snapshot ainda é atual. Resultados atrasados são arquivados como diagnósticos de geração antiga ou descartados; nunca sobrescrevem a geração mais recente. Trocar arquivo-fonte não reanexa componentes por nome de classe.

Renomear campos requer `PropertyId` estável ou migração explícita. Alterar tipo de campo exige conversor validado ou preservação do valor órfão. Remover um tipo mantém a instância como não resolvida; remover uma instância é outro comando. Duplicidade de `TypeId` gera erro localizável nos dois arquivos.

Recarga em execução é uma entrega posterior: preservar somente o estado serializável previsto, desmontar callbacks/subscriptions da geração antiga e recriar bindings em ponto seguro. Não prometer preservar pilhas, coroutines arbitrárias ou tarefas nativas em andamento. Testar referências que impedem descarregamento e diagnosticar quando é necessário reiniciar a sessão Play, não a engine inteira.

## 5.5 Linguagens e API universal

Ao criar script, apresentar uma escolha compacta com **C# e seu símbolo**, além de nome, pasta e natureza do arquivo. “Comportamento” cria uma classe anexável; “classe auxiliar” não aparece como componente; um recurso de dados ou ferramenta editorial só aparece quando houver suporte. O caminho começa com arquivo vazio funcional, não com seleção obrigatória de demonstração.

O registro de linguagens proposto declara extensões, ícone, validador, serviço semântico, compilador/VM, descoberta de tipos, serialização, debug e exportação. Uma linguagem só pode ser selecionada como utilizável se a trilha correspondente estiver pronta. Nenhum papel da Java/Kotlin na Activity justifica limitar gameplay a essa camada. [GH11]

A API deve permitir transformação/hierarquia, consulta e criação de componentes, input por ações, câmera, física/consultas, eventos, animação, áudio, materiais e recursos. Sistemas de câmera, inventário e interação devem ser código do projeto. Render targets, shaders e compute entram no contrato gráfico subsequente, sem serem reduzidos a um script “efeito de água”.

# 6. IDE embutido, campos inline e orientação

## 6.1 A edição acontece onde o conteúdo está desenhado

Remover a dependência de `AlertDialog` para editar código, nome, valor numérico e texto comum. O usuário toca na linha do IDE ou no valor do inspetor; ali aparecem caret, seleção e composição. O teclado do sistema pode surgir, mas não uma segunda superfície de entrada copiando o conteúdo. [GH03]

Proposta de integração: um `CodeEditorHost` Android embutido no retângulo do workspace e um `InlinePropertyEditorHost` ancorado ao retângulo do campo ativo. São Views reais da interface, com clipping, foco, geometria e estilo coordenados com o shell nativo. Não são uma janela modal nem uma textura rotacionada por cima da cena.

O contrato Android para um editor customizado passa por `InputConnection`; `EditorInfo` define comportamento esperado de IME. A biblioteca/widget escolhido deve respeitar esse contrato em vez de implementar seu próprio teclado improvisado. A preferência é reaproveitar a edição nativa ou um widget especializado auditado atrás do host. A escolha final exige prova com C#, JNI, Unicode, seleção e lifecycle antes de substituir o fluxo atual. [R09] [R10]

## 6.2 Autoridade de texto e protocolo de edição

O documento textual é único. A representação Java usada pelo IME e a representação nativa/managed usada pelo serviço de código são projeções sincronizadas por deltas e revisões, não dois buffers inteiros que se sobrescrevem alternadamente. O protocolo proposto inclui `beginBatchEdit`, inserção/substituição de intervalo, composição, seleção, commit e cancelamento da sessão.

Registrar `TextDocumentId`, revisão base, intervalo, texto novo, faixa de composição e seleção resultante. Traduzir índices UTF-16 do Android para o formato interno escolhido sem cortar pares substitutos; navegar/excluir caracteres combinados corretamente. Para arquivo, declarar UTF-8 e preservar política de quebras de linha. Testar acentos, emoji, caracteres fora do BMP, colagem multiline e arquivos externos.

A thread de UI não espera compilação, importação ou fila GPU. A análise recebe snapshot imutável. Atualizações programáticas do documento não podem disparar um ciclo de callback que reinsere o mesmo texto. Acordar o render só quando houver mudança de conteúdo/caret/seleção ou animação necessária, evitando reconstruir toda a cena por cada tecla.

O histórico textual agrupa colagem como uma operação, digitação segundo fronteiras naturais e substituição de autocomplete como outra. Não guardar dezenas de cópias completas de um arquivo grande por cada tecla como solução final. Escolher piece table, rope ou armazenamento segmentado conforme benchmark; não trocar a estrutura só por prestígio técnico.

## 6.3 Campos numéricos e transformações

O campo tem uma sessão local de edição com valor original e rascunho. Sequências intermediárias como `-`, `1,` ou `2e` permanecem editáveis, sem virar zero nem alterar o documento para um valor inválido. O parser final usa unidade/tipo do descriptor, valida finitude e limites, e trata separador decimal de forma explícita.

Para vetores, cada eixo pode ser editado inline; tabulação/Next move para o próximo campo. Um recurso opcional de colar vetor precisa validar dimensão e convenção decimal, sem interpretar separadores ambíguos silenciosamente. O label continua visível durante erro; o erro aparece próximo do próprio campo.

Prévia de valores válidos pode ocorrer dentro de uma transação, com uma única entrada no histórico ao confirmar. Done ou perda de foco confirmam apenas um valor válido; Escape/Cancelar gesto restaura o original. Uma mudança concorrente externa exige resolver conflito, não confirmar um rascunho sobre um alvo diferente. Ao abrir teclado, scroll leva o campo ativo para área visível; a câmera não recebe os mesmos toques.

Copiar/colar texto continua sendo parte normal do sistema operacional e do editor. Isso não justifica uma linha permanente de “Copiar valores / Colar valores / Restaurar” em cada componente. Essas ações especializadas saem do cabeçalho e só permanecem em menu contextual quando tiverem finalidade comprovada.

## 6.4 IDE em retrato: resposta técnica

**É viável planejar o workspace de código em retrato, mantendo edição de cena em paisagem.** A transição deve alterar a política da janela/Activity, não apenas girar a subview de código. O teclado é gerido pelo sistema e acompanha a configuração de sua janela; rotacionar a textura do IDE não controla sua orientação independentemente.

A branch fixa as Activities em `sensorLandscape`. Propor `WorkspaceOrientationPolicy` com preferências “seguir dispositivo”, “retrato no código” e “manter paisagem”, conforme o suporte validado. Ao sair do IDE, restaurar a política anterior da cena. Antes da mudança, conservar documento, objeto selecionado, câmera editorial, tabs, revisões, seleção de texto, scroll e sessão de compilação. [GH04]

Configurações Android podem recriar a Activity. `configChanges` não substitui uma estratégia de recuperação de estado, recursos e tamanho; a implementação deve tolerar ambos os caminhos. Regras de orientação em telas grandes variam por target e categoria; a documentação Android 16 lista exceção para games e o manifest atual declara essa categoria. Ainda assim, nenhuma solicitação de orientação deve ser tratada como garantia universal em multiwindow, dispositivos e versões futuras. [R12] [R13]

**Escopo do compromisso:** tornar o IDE responsivo e manter os dados íntegros com mudança de configuração. Não “girar apenas o teclado” dentro de uma janela permanentemente landscape. Também não alterar o aspect ratio autorado da câmera do jogo só porque o usuário abriu código em retrato.

## 6.5 Teclado, área útil e conforto

Usar insets reais de IME/sistema, não uma altura fixa de teclado. Solicitar ausência de extração em tela cheia quando apropriado; não assumir que todo IME respeitará exatamente o mesmo layout. Manter a linha do caret visível, sem dois ajustes cumulativos de resize/scroll. Ao voltar do seletor de arquivos ou da troca de orientação, restaurar foco somente quando isso corresponder à intenção do usuário. [R10] [R11]

No modo retrato, arquivos tornam-se drawer; console ocupa painel inferior recolhível e o código usa a largura principal. Uma pequena faixa de símbolos de programação pode existir como acessório do IDE, mas não deve substituir seleção/clipboard do sistema. Testar teclado físico, Gboard e pelo menos outro IME disponível, sem dizer que um emulador representa todo comportamento real.

# 7. Console e diagnóstico como infraestrutura

## 7.1 Um console único, várias origens

Criar um `DiagnosticBus` proposto com produtores nativos, managed, compilador, importador, assets, renderização e lifecycle. Logcat permanece um destino de diagnóstico de plataforma, mas não o único lugar onde o criador vê os logs. O console editorial apresenta eventos e permite abrir o objeto, arquivo ou tarefa relacionados. A referência de Console Unity orienta filtragem e localização, não define a implementação interna da Astra. [R01]

Cada evento registra ID, relógio monotônico, horário opcional, severidade, categoria, mensagem, origem, thread, ProjectId, PlaySessionId, BuildGeneration, ObjectId/ComponentInstanceId quando pertinentes, arquivo/linha/coluna e correlação de tarefa. Stack trace pertence a um detalhe expandível; não é necessário imprimir toda pilha em cada linha da lista.

Eventos de importação também levam AssetId e ImportTransactionId; eventos gráficos, DeviceEpoch/SurfaceEpoch e passe. Dados ausentes são indicados como indisponíveis, não preenchidos com zero parecendo medição. Um salto para objeto verifica o mundo e a geração; nunca seleciona outro objeto cujo ID foi reutilizado.

## 7.2 Retenção, performance e fluxo

Produtores não podem bloquear o frame para desenhar linhas. Usar fila bounded com política explícita de pressão, limite por bytes e número de eventos, e buffer de histórico. Mensagens repetidas podem ser agrupadas por assinatura preservando contador, primeira/última ocorrência e contexto suficiente. Perdas geram um resumo de descarte, não um silêncio enganoso.

O orçamento inicial será medido no aparelho com rajadas controladas, incluindo um script defeituoso que escreve a cada Update. Não salvar síncrona e ilimitadamente a mesma mensagem em disco. Eventos de compilação importantes devem permanecer no registro de diagnósticos da geração, mesmo quando a janela de logs é limpa.

**Limpar console não corrige erros.** A ação limpa o histórico visível de logs conforme seu escopo, mas erros ativos do projeto continuam no painel Problemas. Separar store de diagnósticos de build e store de eventos. Pausar rolagem não pausa produção nem execução do jogo.

## 7.3 Layout e ações

A área do console tem severidade, categoria/origem, busca, agrupamento e seguir saída. A lista virtualizada exibe mensagem curta, localização e contexto; selecionar abre detalhe abaixo ou ao lado conforme largura. Long-press permite copiar mensagem ou stack, exportar trecho e revelar origem, sem uma dúzia de botões em cada linha.

Rolagem automática acompanha o final enquanto o usuário não navega para trás. Ao examinar um erro, o console não o arrasta continuamente para baixo. Tocar no diagnóstico de C# leva ao arquivo e destaca intervalo; tocar no objeto revela a seleção pertinente. Compilação com erro pode expandir o painel, mas não rouba foco a cada tecla ou mensagem de informação.

Exportar logs requer um pacote explícito de sessão, com contexto de build, hardware, renderer, versão e filtros. Não exportar automaticamente arquivos de projeto ou dados pessoais. O usuário escolhe o pacote e o destino. Um indicador compacto de erros/avisos pode existir na barra, sempre com ação real de abrir o painel filtrado.

## 7.4 Critério de utilidade

Um caso de defeito deve ser narrável pelo console: “o componente X da geração A permaneceu executando; a fonte B falhou nesta linha”; ou “o documento tem 127 renderers, mas 0 instâncias foram publicadas para a superfície 8 porque a biblioteca de assets está pendente”. Mensagens como “erro desconhecido” ou “cena carregada” sem contexto não fecham o contrato.

# 8. Viewport: profundidade, grade e estabilidade de seleção

## 8.1 Separar três tipos de desenho editorial

**Elementos no mundo:** grade de piso, eixos mundiais, volumes e wireframes que representam posições 3D. Devem ter política de profundidade definida.

**Ferramentas de manipulação:** gizmos de mover/girar/escalar. Podem usar alças parcialmente sempre visíveis, desde que esse comportamento seja intencional e reconhecível.

**Interface da vista:** widget de orientação no canto, botões e labels. São HUD e não fingem ser geometria atrás de objetos.

Hoje a grade foi concebida como overlay. A proposta é um `EditorGridPass` separado que receba câmera, retângulo, plano e depth apropriado. Grade/eixos mundiais normalmente testam profundidade e não escrevem depth. O widget de orientação continua legível no HUD. Não associar automaticamente todos os eixos coloridos à mesma regra. [GH08]

## 8.2 Reconstrução da grade

Escolher uma implementação de plano analítico ou de linhas 3D com antialiasing e cobertura suficiente. A versão analítica calcula a interseção do raio da câmera com o plano editorial e usa coordenadas mundiais para a malha de linhas. O espaçamento é orientado por tamanho em pixels; dois níveis de escala coexistem com blend contínuo.

Planejar tratamento do denominador próximo de zero na interseção, pontos atrás da câmera, near/far, câmera abaixo do plano, perspectiva e ortográfica. Em vista lateral paralela ao piso, a regra deve ser intencional: grade do plano escolhido, grade alinhada à vista ou piso visto de perfil, não uma falha arbitrária. Expor essa política como configuração editorial, sem transformar a grade em componente exportado.

Para afastamento/aproximação, as linhas menores perdem opacidade enquanto as maiores assumem a leitura. A origem não se desloca com a câmera. A cobertura pode acompanhar a vista, mas sua fase mundial permanece estável. Evitar o salto de bordas finitas e a troca instantânea de todas as linhas por mudança de década presente na estratégia atual. O fade no horizonte pode ser correto; a grade piscar com zoom comum não é. [GH08]

## 8.3 Integração com o renderer

Definir explicitamente convenção de depth, transformações de superfície, retângulo lógico/físico, resolução interna e antialiasing. Se houver reverse-Z, a comparação deve acompanhar sua convenção; não copiar um teste `LESS` por hábito. Evitar ler depth de outra vista, outro tamanho ou outra geração.

Com MSAA, decidir como comparar a cobertura e profundidade resolvida; com resolução dinâmica, mapear pixels editoriais para depth da cena. Não reutilizar amostras temporais antigas para os eixos do editor. Linhas e UI não devem herdar ghosting do pós-processamento do jogo.

Sobre um piso coplanar, aplicar política limitada de tolerância/offset ou composição editorial deliberada, sem empurrar todas as linhas para a frente de qualquer objeto. Transparência exige decisão própria: um vidro que não escreve depth não ocultará a grade da mesma forma que uma parede opaca. Documentar esse comportamento em vez de tratá-lo como regressão aleatória.

## 8.4 Picking, seleção e transforms

A branch já tem construção de malhas para picking. Preservar e validar esse avanço antes de propor outra implementação. O ID selecionado deve apontar ao objeto autorado e, se necessário, ao subasset atingido; o batching não pode impedir distinguir dois filhos que compartilham a mesma malha. [GH07]

Testar o retângulo real após abrir painéis, teclado, mudar orientação e resolução. A transformação toque → coordenada lógica → viewport → raio deve ocorrer uma vez, com inversa coerente da projeção usada para gizmos. Seleção por triângulo/BVH ou ID buffer deve manter política de alpha/oclusão documentada.

Bounds precisam acompanhar edição, skinning e novos assets. Um objeto invisível por referência ausente não deve virar invisível na hierarquia. Frame Selection usa o bounds apropriado do objeto/subárvore, não a origem absoluta de todo GLB. Distinguir enquadrar recurso, instância e seleção múltipla.

## 8.5 Ensaios mínimos de aprovação

Uma caixa opaca colocada sobre a origem oculta os eixos de piso atrás dela. Ao mover a caixa, as linhas aparecem no lugar correto. Zoom repetido atravessa níveis de escala sem apagões súbitos. Orbitar perto do horizonte não produz uma diagonal gigante. Redimensionar painéis não desloca o acerto do toque. Importar um novo modelo não altera a identidade de objetos selecionados anteriormente.

Esses são critérios a testar com vídeo e captura de buffers. Não foram validados por esta análise e não devem ser marcados como corrigidos apenas porque uma nova função de grade compila.

# 9. Android: retorno do segundo plano sem perder a cena

## 9.1 Vidas úteis independentes

Projeto/documento têm vida útil autoral. RuntimeWorld tem vida útil da sessão Play. Recursos GPU têm vida útil de device. Cor/depth/history/framebuffers têm dependências de vista e superfície. Activity e teclado têm vida útil de host. Colocar todos esses objetos no mesmo bloco de destruição na pausa é uma fonte de acoplamento que o plano deve eliminar.

Modelo de estados proposto: `Active → Suspended → RecreatingSurface → RehydratingView → Active`. Um device perdido segue uma rota adicional de recriação de recursos. Uma Activity recriada sem perda de processo reanexa o host à sessão; morte do processo recupera o último checkpoint íntegro do projeto, não ponteiros de memória.

Essa máquina de estados não pressupõe que toda pausa destrua a superfície. Ela reage aos eventos efetivos e ao resultado das APIs. Evitar reconstrução desnecessária de todas as texturas ou do runtime quando somente a extensão da swapchain mudou. A documentação Vulkan distingue a swapchain e objetos dependentes que precisam ser recriados. [R14]

## 9.2 Instrumentação antes de correção

No evento de retomada, registrar: estado do documento e revisão, objetos ativos, renderers autorados, assets resolvidos/pendentes/ausentes, biblioteca CPU, DeviceEpoch, SurfaceEpoch, resolução, câmera/near/far, candidatos de culling, instâncias publicadas, draws e resultado de acquire/present.

Adicionar um diagnóstico de reconciliação: se há objetos renderizáveis resolvidos e o renderer publica zero por vários frames sem motivo, emitir evento com a fase responsável. Não ocultar a falha com um reset arbitrário. Um modo de inspeção pode desativar temporariamente oclusão para isolar HZB stale, mas isso é diagnóstico, não correção final.

O sintoma do usuário — hierarquia preservada, viewport vazio e recuperação ao reiniciar — permanece classificado como **causa não fechada sem reprodução**. Câmera, zero extent, recursos, cache de culling e biblioteca não republicada precisam ser descartados ou confirmados por evidência.

## 9.3 Sequência de retomada proposta

Invalidar referências ao host/surface antiga; cancelar somente gestos e operações dependentes daquela janela. Recriar recursos de apresentação quando necessário; atualizar extensão, viewport e projeção. Garantir residência dos recursos de cena necessários e reconciliar a extração contra a revisão atual do documento.

Forçar a publicação inicial integral dos dados dependentes da vista quando o epoch muda. Reconstruir descritores e buffers que realmente mudaram, limpar histórico temporal/HZB inválido e agendar frame. A imagem pode mostrar estado de carregamento enquanto recursos são reidratados, mas não apresentar uma cena vazia como se estivesse carregada corretamente.

Somente depois habilitar interações que dependem do picking e da nova geometria de tela. O editor continua preservando objetos, componentes e fontes durante essa passagem. Inputs atrasados de uma janela antiga são rejeitados por token/epoch.

## 9.4 SAF, importação, código e interrupção

Abrir o seletor de arquivos provoca mudanças de foco/pausa. Não cancelar o próprio pedido só porque a Activity perdeu foco; o commit já descreve uma correção desse tipo e exige regressão específica. Importação e edição têm tokens próprios, não reutilizam “está pausado” como sinal universal de cancelamento. [GH01]

Durante rotação ou retorno, uma tarefa de importação concluída pode publicar no projeto apenas se ProjectId, transação e fonte ainda correspondem. Se o projeto foi fechado, descartar o resultado ou mantê-lo em cache isolado; nunca inserir objetos no próximo projeto aberto. O mesmo vale para compilação C#.

O texto ainda não salvo precisa de journal/checkpoint periódico com revisão, em transação atômica. Não confiar exclusivamente em `onPause` ou `onDestroy` para salvar: a aplicação pode ser encerrada antes do callback desejado. O checkpoint não altera por si o estado autoral confirmado nem substitui silenciosamente um arquivo externo modificado. [R12]

## 9.5 Testes de duração e falha

Definir ensaios separados para Home/voltar, bloquear/desbloquear, abrir picker/cancelar, alternar IDE/cena, mudar orientação, pressão de memória e morte de processo. Repetir com cena vazia, GLB importado, script ativo e importação/compilação em andamento.

A meta de laboratório proposta inclui 100 ciclos rápidos e uma sessão prolongada com medições de memória. É uma carga de teste, não promessa de que esse número já passou. O checkpoint restaurado deve indicar eventual rascunho recuperado e jamais sobrescrever silenciosamente uma cena íntegra por um snapshot parcial.

# 10. Importação estrutural: do GLB à cena editável

## 10.1 Importar, registrar e instanciar são operações diferentes

**Importar** lê fonte e produz recursos derivados. **Registrar** publica esses recursos e dependências no projeto. **Instanciar** cria objetos na cena a partir de um recurso. **Reimportar** atualiza derivados e reconcilia referências; não duplica automaticamente a cena.

O caminho principal proposto é Projeto/Arquivos → Importar. “Adicionar objeto” cria objetos ou instancia recursos já presentes. A operação de importação não deve ficar escondida em Geometria. Um atalho de arrastar um GLB pode conduzir à importação e depois posicionar uma instância, mas os dois atos continuam distinguíveis, com diagnóstico e Undo apropriados.

A Godot oferece inspeção por nó e opções de recursos no fluxo de importação avançada; o plano usa essa referência para organizar preview, árvore e configuração. Não pretende copiar as mesmas opções se seus consumidores não existirem na Astra. [R05] [R06]

## 10.2 Representação intermediária obrigatória

Introduzir ou adaptar uma `ImportedSceneIR` proposta antes de gerar qualquer pacote de draws. Ela representa cenas/raízes, nós com pais e filhos ordenados, transform local, metadados, meshes compartilhadas, primitivas/material slots, materiais, imagens/samplers, câmeras, luzes suportadas, skins, animações e morph targets do perfil implementado.

Um nó sem malha precisa sobreviver: pode ser grupo, pivô, joint, câmera ou alvo de animação. Duas instâncias da mesma malha compartilham dados geométricos, mas mantêm identidades e transforms distintos. Uma malha com três primitivas não precisa virar três objetos irmãos: pode ser um renderer com três submeshes/material slots.

A especificação glTF descreve a transformação da malha por um nó e separa `children`, meshes e primitives. Usar essa distinção como contrato de entrada. A lista de draw packets é uma saída para renderização, nunca a representação autoral principal. [R07]

## 10.3 Porta de veículo como teste, não como classe especial

A hierarquia esperada pode ser `Veiculo → Carroceria / PortaEsquerda / PortaDireita / Rodas`. Importar o recurso inteiro preserva essa árvore quando ela existe na fonte. Selecionar `PortaEsquerda` permite editar apenas seu transform local; mover `Veiculo` transforma todos os filhos. O pivô importado permanece como origem de rotação, inclusive quando não coincide com o centro geométrico.

Esse comportamento é idêntico para armário com gavetas, robô com articulações, prédio com portas ou qualquer assembly. Não existe um `VehicleDoorImporter` fundamental. O teste usa nomes reconhecíveis apenas para facilitar a verificação.

**Limite real:** se o autor exportou carroceria e porta como uma única malha fundida sem separação útil, o importador não pode inferir semanticamente a porta com certeza. Ferramentas futuras de seleção/split de geometria podem ajudar, mas são ferramentas de modelagem separadas. O diagnóstico deve informar o que a fonte contém e não inventar uma árvore inexistente.

## 10.4 Identidade persistente e reimportação

Nomes glTF são opcionais e podem repetir; o container também pode manter referências externas. Não tratar nome ou índice de array como um UUID universal. A estratégia atual de chaves por nomes/índices de primitivas precisa de uma evolução explícita, sobretudo para renomear e reordenar a fonte. [GH05] [GH06] [R07]

Criar identidade de fonte estável no projeto e mapa de subassets persistente. Preferir UID fornecido por metadado de exportação quando disponível; caso contrário, reconciliar usando estrutura, assinatura de recurso e mapa anterior. Ambiguidade gera conflito para confirmação, não escolha silenciosa da primeira porta com mesmo nome. O hash do conteúdo identifica a revisão, sem trocar o AssetId a cada edição.

A instância na cena referencia o SceneAsset e mantém overrides por identidade estável: transform, nome local, material, habilitação e componentes adicionados. Reimportação compara base anterior, base nova e alterações locais. Um filho removido da fonte deve apresentar referência ausente/conflito e opções controladas de preservação ou remoção; não apagar scripts e dados órfãos sem explicação.

## 10.5 Preservar origem e permitir personalização

Separar recurso importado read-only, instância autorada editável e derivados GPU. O usuário pode editar propriedades de filhos como overrides, criar uma cena derivada reutilizável ou desvincular a instância por comando explícito. Não esconder “desempacotar tudo” como condição obrigatória para apenas mover uma porta.

A distinção entre um recurso reutilizável e diferenças aplicadas às instâncias é uma referência útil da Unity. Na Astra, precisa funcionar também para meshes/materials compartilhados e reimportação, com alcance da edição visível ao usuário. Alterar um material comum não deve afetar todas as instâncias sem que a UI deixe claro seu compartilhamento. [R03]

## 10.6 Transação de importação

Pipeline proposto: adquirir fonte → copiar/stage → validar container e dependências → parse para IR → resolver/decodificar → gerar derivados → preview/relatório → commit do registry → publicação gráfica → instanciação opcional. Cada etapa tem progresso real, orçamento, cancelamento e erro correlacionado no console.

A publicação deve ter uma fronteira atômica: o projeto vê o conjunto antigo completo ou o novo completo. Se houver falha de upload, manter recursos anteriores e não reescrever referências para slots ainda indisponíveis. Bibliotecas gráficas completas podem ser mantidas inicialmente por segurança, mas migrar para uploads/versionamento incremental após testes, sem invalidar draws existentes. [GH01]

Não prometer que toda exclusão de arquivo seja reversível sem lixeira/journal apropriado. Undo de instanciação remove a instância, não necessariamente apaga a fonte importada do projeto; o escopo de cada operação fica explícito.

# 11. Importador de alto nível: cobertura, material e segurança

## 11.1 Perfil de compatibilidade, não promessa “aceita qualquer coisa”

Um importador maduro define o que suporta, valida o arquivo e dá diagnóstico específico. Ampliar suporte não significa aceitar dados inválidos, extensões obrigatórias desconhecidas ou arquivos cujo conteúdo excede os recursos do aparelho. O relatório deve diferenciar erro de formato, dependência ausente, recurso não implementado, capacidade do renderer e orçamento de memória.

| Domínio | Entrega necessária | Dependência que precisa funcionar |
|---|---|---|
| Estrutura básica | GLB/glTF, cenas, nós, locais/matrizes e malhas compartilhadas | IR estrutural, IDs, serialização, instancing e picking |
| Dados geométricos | Índices suportados, stride, sparse accessors, normal/tangent, UVs, cores | Validação numérica, conversão e formatos de vértice |
| Topologias | Triângulos e conversão documentada de strips/fans; linhas/pontos por backend | Política clara de conversão e material/raster compatível |
| PBR básico | Fatores e texturas de base, metallic/roughness, normal, occlusion e emissive | Decoder, samplers, cor linear/sRGB, shader e mipmaps |
| Transparência | OPAQUE/MASK/BLEND, cutoff e dupla face | Filas de desenho, depth e política de ordenação |
| Câmeras/luzes | Câmeras glTF e luzes da extensão suportada | Componentes reais e convenções de unidade/projeção |
| Animação | Tracks de transform, interpolação, clips e alvos | Identidade de nó, relógio, mixer e autoridade de pose |
| Skinning/morphs | Joints, inverse bind, pesos, targets e bounds | Deformação, animação, seleção e materiais compatíveis |
| Compressão | Extensões de geometria/textura escolhidas e testadas | Decodificador, versão, CPU/memória e capacidade GPU |
| Materiais avançados | Extensões escolhidas de clearcoat, transmission, volume etc. | Renderer que reproduza o efeito, não só parser |

Cada linha tem status por versão: ausente, parcial, validada ou condicional. Registrar o que é preservado para futura reimportação e o que pode ser exibido com aproximação declarada. Não marcar uma extensão como suportada apenas porque seus campos foram lidos.

## 11.2 Texturas e formação de imagem

Separar bytes-fonte, imagem decodificada, mip chain e representação GPU. Canais de cor e de dados precisam da semântica adequada, com normal/tangent e orientação consistentes. A tabela de materiais do projeto deve mostrar os recursos reais, não só “material 4”. Texturas ausentes aparecem com diagnóstico e visualização de erro reconhecível, nunca confundidas com um material final simples.

O primeiro percurso completo será um GLB texturizado PBR, incluindo normal map, metal/roughness, emissão, alpha e múltiplos materiais. Testar também a mesma fonte após reabrir projeto e após perder superfície. Mudanças de fator de material não devem recompilar todos os shaders nem reimportar todas as imagens.

Escolher decodificação compatível com a API mínima do aplicativo. O backend pode combinar bibliotecas nativas auditadas e integração Android apropriada; não depender exclusivamente de uma função inexistente nos dispositivos prometidos. Aplicar limites sobre dimensões, bytes descomprimidos, mipmaps e alocação total antes de aceitar imagens grandes.

Compressão de GPU deve ser negociada por capacidade. Uma fonte comprimida por extensão precisa do decoder/transcoder correto; a política de fallback preserva semântica e informa custo. Textura pequena na origem comprimida pode ficar muito maior em RAM: o limite do tamanho do GLB não é suficiente para proteger o aparelho.

## 11.3 GLTF externo e Storage Access Framework

Implementar um `AssetSourceProvider` que resolva URIs autorizados, diretórios escolhidos e pacotes importados. `content://` não vira caminho POSIX por concatenação. O caminho já implementado de copiar a fonte para `Fontes/` é uma base útil; expandi-lo para dependências com resolver, manifesto e staging. [GH01]

Uma entrada pode ser GLB autocontido, GLB com dependências, glTF com `.bin`/imagens, ou bundle explicitamente escolhido. O sistema deve solicitar acesso às dependências que não foram entregues, não pesquisar arbitrariamente armazenamento privado. Copiar as dependências necessárias para o projeto e verificar que ele continua abrindo offline após a permissão original não estar disponível. GLB, por especificação, ainda pode referenciar dados externos. [R07]

Normalizar URIs e caminhos relativos, bloquear travessia de diretórios fora do escopo autorizado e ciclos de resolução, impor orçamento de quantidade/tamanho e não abrir rede automaticamente. Imports remotos, se adicionados, têm consentimento e política própria.

## 11.4 Parser, decoder e consumidor são contratos diferentes

Avaliar substituir o parser ad hoc por um adaptador mantido, como `cgltf`, ou conservar partes atuais sob testes de conformidade. O candidato fornece parsing e utilitários, não entrega automaticamente texturas, GPU, árvore autoral, persistência, física ou UX. A decisão deve comparar correção, acesso a extensões, custos Android e capacidade de diagnóstico. [R15] [R16]

Antes de adotar, fixar commit/versão, revisar licença e correções relevantes, executar corpus malformado e testes de sanitizers/fuzzing no host. A validação de uma biblioteca não elimina validação de orçamento e integração. Não declarar segurança apenas porque a biblioteca é popular ou single-header.

Integrar Khronos glTF Validator à validação de corpus/CI e, quando viável, a uma ferramenta de diagnóstico. A pipeline ainda precisa retornar mensagens legíveis no aparelho mesmo sem distribuir a ferramenta completa de validação. [R08]

## 11.5 Matriz de extensões e dependências

Registrar separadamente `extensionsUsed` e `extensionsRequired`. Quando a extensão é obrigatória e não há suporte semântico, recusar com o nome exato e orientação de exportação alternativa. Quando é opcional e existe representação-base válida, indicar a perda/aproximação. Nunca ignorar silenciosamente compressão que muda a forma de ler geometria.

Priorizar extensões comuns compatíveis com a estratégia do renderer: transformação de textura, quantização, compressão de malha escolhida, texturas BasisU/KTX2 quando integradas, luzes pontuais e materiais do escopo. A ordem é resultado de corpus e necessidades dos projetos, não uma lista de marketing.

Importar animação, morphs e skins exige também editar, reproduzir, salvar e renderizar seu resultado. Estimar esse trabalho antes de anunciar “importador completo”. Um asset pode ser válido, mas exigir uma capacidade futura; o plano torna essa lacuna visível sem bloquear a expansão.

# 12. Layout universal do editor e dos fluxos de criação

## 12.1 Referência visual recuperada e adaptação necessária

O conceito anterior de IDE Astra em retrato foi recuperado e inspecionado. Ele traz identidade escura, acento azul/ciano, tabs, árvore de arquivos, numeração de linhas e barra de status. É uma referência de composição, não uma captura comprovada da branch. O código desenhado na imagem não define a API da engine. [IMG01]

Preservar identidade e coerência de ícones, mas não reproduzir os problemas de escala do conceito: duas linhas de comandos grandes, controles de Play repetidos e árvore lateral larga consomem espaço especialmente com teclado aberto. A proposta deve ser validada na dimensão lógica real do telefone, não apenas num mockup alto sem IME.

O novo layout de long-press anunciado pelo usuário ainda não foi enviado. As alternativas abaixo são propostas de comportamento e espaço, prontas para ajustar quando essa referência chegar; não uma alegação de reprodução fiel de imagem inexistente.

## 12.2 Alternativa A — docks adaptativos, recomendada

**Cena em paisagem:** faixa superior compacta para contexto/projeto e execução; hierarquia à esquerda; arquivos em dock recolhível; viewport central; inspetor à direita. Painéis consomem espaço real no layout, não cobrem permanentemente metade do enquadramento. Ferramentas de transformação ficam próximas do viewport.

**Código:** o mesmo workspace muda de conteúdo, com uma barra contextual única, tabs, editor e status. Em retrato, arquivos viram drawer e o console pode ocupar a parte inferior; em paisagem larga, arquivos podem ficar lado a lado. A seleção de objeto e a câmera editorial são conservadas enquanto o código está aberto.

**Vantagem:** mantém familiaridade de uma engine e boa relação entre objeto, componente e recurso. **Custo:** requer layout responsivo e política clara de foco/insets. É a configuração padrão proposta, não uma soma de todas as opções visuais possíveis.

## 12.3 Alternativa B — foco central para telefones menores

O viewport ou código ocupa a região principal. Hierarquia, arquivos e inspetor abrem como painéis alternáveis nas bordas, mantendo as mesmas funções e identidades. O botão de um painel apresenta seu estado; abrir outro não altera seleção nem perde rascunho.

**Vantagem:** mais área útil e alvos de toque legíveis. **Custo:** menos comparação simultânea entre hierarquia e propriedades. Usar automaticamente quando a largura útil não sustenta os painéis mínimos, e permitir preferência do usuário. Não reduzir o catálogo de componentes por causa do formato da tela.

## 12.4 Alternativa C — código e preview dividido

Destinada a tablet, dobrável aberto ou configuração com teclado físico e área suficiente. Código e uma vista de preview podem ficar lado a lado, com console compartilhado. Cada RenderView possui sua câmera e história; não duplicar o mundo autoral nem criar uma simulação escondida só para preencher o painel.

**Vantagem:** feedback simultâneo. **Custo:** mais renderização e memória. Não é o padrão do telefone com teclado virtual. Desligar a atualização de um preview oculto e medir o custo de multivista antes de apresentá-la como melhoria gratuita.

## 12.5 Geometria de layout e estados a prototipar

Especificar limites em dp e áreas úteis após barras/insets. A arte do ícone pode ser menor que seu alvo de toque, mas regiões expandidas não podem se sobrepor tornando o resultado do toque ambíguo. Campos densos podem utilizar linhas dedicadas e rolagem, em vez de comprimir textos até ficarem ilegíveis.

Os wireframes de aprovação devem cobrir: cena vazia; vários objetos selecionados; componente longo; IDE com teclado; IDE sem teclado; erro com console; importador com árvore; menu de long-press junto à borda; rotação no meio de edição; arquivo externo ausente. A mesma coleção de ícones e tokens de espaço deve ser usada em todos.

Nenhuma aba principal é adicionada só porque existe um módulo. Iluminação pertence a componentes/recursos e ferramentas apropriadas; água não recupera uma aba global obrigatória. Configurações de projeto existem para políticas globais reais, como input, física, compilação e qualidade.

## 12.6 Adicionar objeto, componente e recurso

**Adicionar objeto** abre um catálogo pesquisável de composições básicas: vazio/grupo, malhas primitivas, câmera, luz e outras capacidades implementadas. Criar um cubo é um atalho declarativo para objeto + transform + referência de malha + renderer; não um tipo de entidade com privilégios especiais.

**Adicionar componente** atua sobre a seleção e usa o registro de tipos. Mostra requisitos e incompatibilidades antes de alterar o objeto. **Criar recurso** atua no projeto: material, script, cena reutilizável, shader e dados suportados. **Importar** recebe fontes externas. Essas ações podem compartilhar componentes de UI, mas não uma lista confusa em “Geometria”.

O catálogo tem busca, categorias concisas, descrição curta e contexto de destino. Pode haver favoritos/recentes sem destacar “carro” como função nuclear. Uma cena reutilizável de veículo criada pelo usuário aparece como um asset dele, assim como qualquer outra composição.

## 12.7 Long-press e menu contextual da hierarquia

Toque seleciona; arraste pode rolar ou reparentear conforme região/estado; long-press abre ações do alvo selecionado. Usar limiar/slop consistente com plataforma e testes, com cancelamento quando o gesto vira rolagem. Não disparar delete, arrastar e menu na mesma sequência.

Ações propostas: enquadrar seleção, renomear inline, duplicar, criar filho/grupo, mudar pai, instanciar recurso, adicionar componente, anexar/abrir script, salvar como cena reutilizável, ocultar/bloquear e excluir. Mostrar apenas as que são válidas para a seleção e para o modo Edit/Play.

Em área suficiente, menu ancorado com ícone e rótulo curto; perto de bordas ou com muitas ações, sheet contextual compacto. Menus podem usar texto para desambiguar: “menos botões com texto” não significa esconder o significado de operações destrutivas. Acesso alternativo por `…` e teclado/mouse evita depender exclusivamente do long-press.

Duplicar preserva subárvore e remapeia referências internas, deixando referências externas legítimas intactas. Excluir exige política de dependentes e histórico; mover não muda identidade. Em Play, ações sobre runtime-only são claramente separadas de mutações autorais.

## 12.8 Importador: organização visual proposta

A tela de importação possui contexto do arquivo/destino, preview, árvore e área de propriedades do item selecionado. Em espaço largo, três regiões; no telefone, tabs de conteúdo e seleção persistente: Visão geral, Cena, Materiais/Texturas, Animações e Relatório, somente quando o conteúdo/suporte justificar.

Configurações incluem selecionar cena de origem, preservar hierarchy, política de normais/tangentes, gerar derivados compatíveis, opções de animação e destino. “Preservar hierarquia” é padrão autoral; uma otimização que combine geometria deve indicar consequências e manter mapeamento de IDs. Selecionar submesh no preview não transforma material slot em objeto indevidamente.

O relatório exibe contagens da fonte, recursos aceitos, recursos omitidos, extensão incompatível, bytes estimados e warnings. Uma prévia de material neutra não deve mascarar ausência de texturas. A operação primária importa recursos; instanciar na cena é uma decisão explícita, preservada como preferência somente se for clara.

# 13. Ícones: contrato para produção e integração

O manifesto no pacote contém nomes de arquivos, finalidade, local de uso, prioridade e natureza de cada ícone. Ele é uma lista de produção para o usuário gerar as imagens; **nenhum novo PNG de ícone é apresentado como criado nesta entrega**. Os mesmos IDs devem alimentar os comandos e o atlas visual para evitar nomear sprites apenas por posição numa folha.

## 13.1 Sistema visual proposto

Usar silhuetas reconhecíveis, espessura consistente e detalhes que sobrevivam à redução. A identidade Astra pode permanecer no acento e nas formas, mas o efeito luminoso de uma ilustração grande não pode prejudicar leitura em 18–24 dp. As referências antigas servem para coerência de família; não obrigam glow, bevel ou múltiplas cores em toda ação. [IMG01]

Entregar preferencialmente fonte vetorial autorizada e PNG transparente em resolução suficiente para os tamanhos-alvo, sem texto miúdo embutido. A variação de cor/estado deve ser aplicada pela UI quando possível. Não criar arquivos diferentes para cada tom de cinza se um único glyph resolve; reservar variante de estado quando a forma muda, como olho aberto/fechado.

Para C#, usar símbolo reconhecível e rótulo `C#` na opção de linguagem. Conferir origem/licença do símbolo utilizado. Não deixar um gerador inventar caracteres semelhantes a C++ ou “CS” sem validação. Um fallback tipográfico `C#` é preferível a uma marca deformada.

## 13.2 Comandos versus estados

Ícone de ação deve ter comando, contexto, nome acessível e estado habilitado. Um indicador de compilação em progresso não finge ser botão de executar. Um erro abre diagnóstico; um status “atual” pode abrir detalhe de geração. Teclado, orientação e modo de painel refletem a preferência real.

Não repetir Undo/Redo, Play e Search em duas barras do mesmo workspace. Não criar ícone “Editar” para abrir o velho diálogo. Não manter “Aplicar código” como requisito rotineiro escondido atrás de um símbolo. Não promover copiar/colar/restaurar valores a três botões permanentes por componente.

**Ícones não substituem informação essencial.** Campos mantêm rótulos, categorias do importador mantêm nomes quando necessário e operações destrutivas têm descrição. Tooltips/descrições acessíveis são parte da implementação, não uma legenda externa que o usuário precisa memorizar.

## 13.3 Validação da produção

Cada arquivo do manifesto deve existir, ter transparência correta, margem óptica compatível e leitura nos tamanhos usados. Conferir fundo claro/escuro, estado ativo/desabilitado, contraste e regiões de toque. O atlas deve ter mapeamento estável por nome e não depender da ordem de extração de uma imagem gerada.

Há ícones marcados para expansão posterior. Eles podem ser produzidos antecipadamente, mas só entram no produto quando o comando ou componente correspondente existir. A lista detalhada aparece no anexo e também em CSV/JSON; não é uma promessa de funcionalidades já disponíveis.

# 14. Runtime, componentes e gráficos: manter a ambição sem novos atalhos

## 14.1 Extensão por capacidade

A nova casca deve consumir os mesmos contratos do runtime: criar/consultar componentes, propriedades, recursos, hierarquia e eventos. Um sistema de câmera do usuário pode controlar o próprio transform ou um alvo referenciado; um sistema de interação consulta capacidades, não nome de objeto. Um colisor de caixa aproxima qualquer objeto adequado, não só uma malha chamada Cube.

O catálogo fundamental inclui transform, renderização, câmera, luz/ambiente, collider, corpo físico, joints, comportamento, input e recursos reutilizáveis. Animação, áudio, UI, navegação, partículas e demais famílias entram por seus marcos do atlas. Não colocar toda a lista de componentes em um único menu antes de conectar seus consumidores. [AR01]

Definir o contrato de “adicionar por código”: alterações dentro de Play afetam o RuntimeWorld e aparecem na inspeção de execução. Elas não reescrevem automaticamente a cena autorada. Ferramentas de editor podem criar componentes autorais por CommandBus, com Undo e persistência. A distinção evita confundir criação runtime com “componente sumiu quando saí do Play”.

## 14.2 Exemplo de generalidade para câmeras

Um comportamento `FollowTarget` proposto lê um alvo e escreve um transform. Funciona numa câmera, luz ou marcador. Um comportamento separado controla parâmetros ópticos de câmera quando esse componente existe. Colisão da câmera usa consultas físicas; controle de jogador usa input por ações. Assim, uma câmera em terceira pessoa, uma câmera de inspeção e um drone podem compartilhar partes sem um componente rígido “câmera do carro”.

A API deve preservar vida útil e segurança de referências: objetos removidos retornam referência inválida diagnosticável, não ponteiro antigo. Operações permitidas por fase são documentadas. Uma chamada que muda corpo dinâmico precisa passar pela autoridade física adequada, não disputar transform diretamente com o solver.

## 14.3 Gráficos autoráveis, não presets de cena

A proposta mantém recursos de material, shader, textura, RenderView e custom passes como contratos gerais. Água pode consumir malha/domínio, material, solver compute e consultas; removê-la não quebra a criação de uma cena seca. Um efeito de pós-processamento do usuário declara entradas/saídas, estágio e capacidades necessárias, em vez de ser adicionado por nome de mapa.

Não substituir Vulkan/renderer atual por URP, HDRP, Filament ou um viewer web sem decisão de arquitetura. As grandes engines fornecem padrões de autoria e técnicas; os atlas anteriores já separam pipeline, PBR, compute e fronteira experimental. Esta entrega integra sua ordem com os defeitos concretos da branch. [AR02]

## 14.4 Qualidade e desempenho em sequência

Antes de efeitos sofisticados, validar cor, materiais, texturas, profundidade, câmera, transforms e retomada. Depois, medições por passe, visibilidade/instancing preservando IDs, sombras estáveis, motion vectors e história temporal. Só então avançar para efeitos de tela, bake mobile, materiais/shaders autoráveis, compute e extensões mais caras conforme as dependências G0–G15. [AR02]

Uma edição de uniforme deve atualizar o recurso necessário, não reconstruir todo pacote geométrico. Uma mudança de layout não deve recompilar shader. Uma nova instância da mesma malha não deve duplicar a fonte. Otimização autoral mede latência de interação, importação, compilação e memória, além do frame time do jogo.

Culling e batching não eliminam relações parent/child. É possível agrupar submissões mantendo ObjectId e offsets por instância. Se uma otimização opcional fundir geometria e remover editabilidade, ela pertence a um derivado de distribuição claramente separado, com mapeamento para diagnóstico e sem adulterar a cena-fonte.

## 14.5 Contratos de recursos gráficos criados pelo usuário

Um material/shader precisa ser editável no aparelho, compilável com erro localizado, salvável, reabrível e empacotável. Um kernel compute declara buffers/imagens, acesso, dimensão de trabalho, requisitos e limites. O frame graph pode validar dependências de recursos, mas não torna código GPU arbitrário seguro ou barato automaticamente.

Registrar versão de compilador/backend, reflection, variantes, cache e última publicação válida como no sistema C#. Diagnóstico gráfico deve ir ao mesmo console, com origem distinta. A prévia pode ter capacidades inferiores ao alvo artístico preservando o recurso, mas deve informar claramente qual aproximação está em uso.

## 14.6 Desempenho sustentado e tarefas editoriais

Estabelecer budgets por classe de dispositivo e cena. CPU, GPU, memória, uploads, shader compilation e temperatura competem com UI e teclado. Tarefas longas têm limite de concorrência, progresso, cancelamento e preferência pela responsividade da edição. Não usar um target de FPS universal como substituto para medição.

Comparar p50/p95/p99 de frame time, latência de input, tempo de primeira imagem após retomada, memória antes/depois de ciclos e custo de cada etapa de importação. Incluir resolução, refresh, build, dispositivo, driver e duração. Valores numéricos deste plano são metas de teste propostas, nunca resultados medidos na Astra.

## 14.7 Distribuição começa cedo

Criar cedo um projeto mínimo que execute sem o workspace de editor e sem diretórios temporários do desenvolvimento. Repetir depois com GLB texturizado, scripts, animação e recursos gráficos. Uma biblioteca que só existe na Activity do editor não pode ser pré-requisito oculto do jogo exportado.

O empacotamento registra fontes necessárias, derivados, assemblies/scripts, shaders, capacidades e versões. Testar a instalação em outro diretório/dispositivo disponível, reimportação ausente e arquivos offline. O fechamento de distribuição não pode descobrir pela primeira vez que o jogo dependia da imagem de preview ou do servidor usado pelo desenvolvedor.

# 15. Migração, arquivos e estratégia de execução

## 15.1 Não fazer uma reescrita simultânea de todos os sistemas

Criar uma trilha de substituição do workspace com critérios de corte. Primeiro acrescentar observabilidade e preservar estado; depois trocar um consumidor por vez: catálogo/schema, texto embutido, grade, importação estrutural e painéis. O código antigo não permanece indefinidamente como segunda fonte de verdade, mas pode existir temporariamente atrás de uma chave de desenvolvimento para comparação.

Antes de deletar um caminho, provar que o novo conserva arquivos, scripts, referências, componentes e recursos existentes. Nenhuma limpeza de demos autoriza apagar projetos pessoais. Criar backup ou formato de migração reversível e documentar o que não pode ser inferido automaticamente do formato antigo.

## 15.2 Mapa inicial de arquivos reais

| Responsabilidade | Entradas existentes para modificar/auditar |
|---|---|
| IDE, buffers e build report | native/editor/editor_code_workspace.cpp/.h; editor_screen.cpp/.h; editor_session.cpp/.h |
| Entrada Android | android/app/src/main/java/dev/aether/editor/EditorTextInput.java; native/platform/android/android_editor_text_input.cpp/.h |
| Orientação/host | android/app/src/main/AndroidManifest.xml; host real da AetherActivity a localizar na source set ativa; android_window e android_main |
| Componentes e UI | native/editor/editor_component_catalog.h; editor_properties.h; editor_document.h; native/scene e native/runtime |
| Grade, câmera e picking | native/editor/editor_grid.h; editor_view.*; editor_camera.*; editor_pick_mesh.h; editor_gizmo.* |
| GLB e identidade | native/resources/gltf_import.cpp/.h; asset_registry.cpp/.h; editor_map_scene.cpp/.h |
| Entrada de arquivo | ModelPicker.java; android_model_picker.cpp/.h; editor_filesystem.cpp/.h |
| Publicação GPU | native/platform/android/dirt_road_resources.*; instanced_renderer.*; renderer/RHI |
| Retomada | native/platform/android/android_main.cpp; android_window.*; android_vulkan_surface.*; lifecycle_trace.h |
| Código managed | native/platform/android/dotnet_host.*; módulos managed realmente chamados pelo host, a mapear no baseline |

Essa tabela identifica pontos de entrada, não autoriza substituir todos. A antiga localização de `AetherActivity.java` em `src/main` não foi encontrada no snapshot; localizar source sets/geração/build real antes de editar um caminho presumido. Não criar outra classe duplicada para contornar a descoberta.

## 15.3 Arquivos novos propostos e fronteiras

Propor, somente quando não houver equivalentes: `TextEditSession`, `InlineEditBridge`, `ScriptTypeCatalog`, `BuildPublication`, `DiagnosticEvent/Store`, `ImportedSceneIR`, `SceneAssetInstantiator`, `SubassetReconciler`, `RenderViewRecovery`, `EditorGridPass`, `WorkspaceLayoutModel` e `CommandRegistry`.

Esses nomes descrevem módulos, não uma obrigação de exatamente doze arquivos. Todos precisam ter consumidor e teste. Manter cabeçalhos de UI fora de importação/runtime, parser fora de widgets e referências de Activity fora de fontes autorais. O código de cola Android é um adaptador, não o local onde se decide o que é “porta” ou “veículo”.

## 15.4 Migração de cenas achatadas

Dados antigos que só armazenam draws com transforms globais podem não conter a árvore original suficiente para reconstrução exata. Quando a fonte GLB estiver disponível, reimportar para a IR e reconciliar instâncias existentes; usar mapa de migração com confirmação quando houver ambiguidade. Não inventar pais pelo nome nem mover o pivô silenciosamente.

Preservar cena anterior intacta e produzir relatório: quais objetos foram mapeados, quais overrides foram mantidos, quais referências exigem decisão e quais subassets são novos/ausentes. A migração não termina enquanto scripts apontarem para filhos diferentes dos anteriores sem aviso.

## 15.5 Critério de conclusão de uma tarefa

Cada PR deve conter objetivo observável, base/commit, causa/evidência, arquivos e símbolos, contrato de dados, migração, testes host, roteiro Android, limitações e rollback. Teste não executado é marcado como pendente. A tarefa passa somente quando seu fluxo pelo editor funciona, não quando a implementação isolada compila.

Ao finalizar uma etapa, atualizar este plano de status e não renumerar IDs que já estejam usados por testes, tarefas ou ícones. Dependências novas devem ser incorporadas ao grafo antes de começar a etapa dependente. Evitar tarefas como “melhorar importer” sem uma saída e um teste concretos.

# 16. Roadmap mestre e ordem de implementação

O grafo abaixo substitui uma leitura linear e conflitante dos planos anteriores. Os IDs P e G continuam sendo referências de capacidades; M00–M15 é a trilha consolidada desta revisão. Etapas sem dependência direta podem evoluir em paralelo após seus contratos, mas nenhuma pode alegar conclusão sem seus pré-requisitos.

**Caminho crítico imediato:** M00 → M01 → correções M02/M03 → M04/M05 → M06/M07 → M08/M10. M09 amplia o perfil de importação; M11 adiciona retrato depois da recuperação e do input confiáveis. M12–M15 consolidam composição, gráficos, desempenho e distribuição. Uma prova mínima de exportação pertence ao baseline e volta a ser executada em cada ampliação; não é descoberta apenas em M15.

Os IDs de testes referem-se ao anexo. Todos os itens começam como planejados. A inclusão no documento não significa tarefa executada.

## M00 — Baseline reproduzível e contratos existentes

**Pré-requisitos:** baseline acessível. **Relação com os atlas:** P0 · G0.

Fixar o commit de trabalho, reproduzir a entrada do projeto e mapear o host, registry, build e source sets efetivos.

**M00.1 — Baseline e inventário.** Registrar APK, toolchain, backend, VM/bibliotecas e versões. Capturar os sintomas sem atualizar silenciosamente o alvo. Entradas de código: docs de estado; build Android; host managed; native/editor.

**M00.2 — Harness e fixtures.** Criar fixtures pequenas de hierarchy, material, script e retomada. Mantê-las em testes, não em modos de demonstração. Entradas de código: tests nativos/managed/Android; ferramentas de diagnóstico.

**M00.3 — Trace de operação.** Registrar geração e correlação para edição, build, import, Play e surface. Guardar baseline de imagem e frame time. Entradas de código: editor_session; android_main; infraestrutura de logs.

**Critério de passagem:** Um roteiro de reprodução, log correlacionado, inventário e limites conhecidos existem. Nada é marcado validado somente por estar no commit.

**Validação vinculada:** OBS01, OBS02, QA01. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M01 — Identidade e observabilidade comuns

**Pré-requisitos:** M00. **Relação com os atlas:** P1 · G1.

Consolidar identidade de documento, componentes, assets e gerações antes de trocar consumidores.

**M01.1 — Contrato de identidades.** Mapear IDs atuais e introduzir somente o que faltar: component instance, build generation e resource/surface epochs. Entradas de código: native/runtime; native/scene; editor_document; asset_registry.

**M01.2 — Eventos estruturados.** Implementar schema do DiagnosticEvent, producers não bloqueantes e stores separados para logs e problemas. Entradas de código: diagnostic bus proposto; código nativo/managed.

**M01.3 — Persistência e revisão.** Garantir snapshots, escrita atômica e rejeição de resultados obsoletos. Testar referências e arquivos anteriores. Entradas de código: editor_archive; history; registry; code workspace.

**Critério de passagem:** Mesma instância é reconhecida por UI, persistência e runtime; traces distinguem gerações; dados desconhecidos não se perdem.

**Validação vinculada:** CMP01, CMP07, OBS03, OBS04. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M02 — Coerência de scripts e componentes

**Pré-requisitos:** M01. **Relação com os atlas:** P1/P5 · G1.

Eliminar o acoplamento que limpa tipos durante a edição sem apagar instâncias ou esconder versão executada.

**M02.1 — Catálogo válido independente.** Separar catálogo publicado de diagnostics/draft. replace/undo/redo não invalidam a existência dos schemas conhecidos. Entradas de código: editor_code_workspace.cpp/.h.

**M02.2 — Inspetor estável.** Enumerar por ComponentInstanceId; manter irmãos visíveis; remover foco implícito cards.assign e dependência de índice transitório. Entradas de código: editor_screen.cpp/.h; editor_session; component catalog.

**M02.3 — Bindings e estados.** Mostrar Atual/Alterado/Erro/Não resolvido/Runtime-only com geração. Remover e anexar são comandos explícitos idempotentes. Entradas de código: native/scene/script_behavior; runtime; UI de componentes.

**Critério de passagem:** Introduzir erro num script não faz seu componente desaparecer; Play e inspeção informam a versão real; nenhum reapply duplica execução.

**Validação vinculada:** CMP01–CMP09, IDE04, IDE05. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M03 — Recuperação de Activity, surface e projeto

**Pré-requisitos:** M01. **Relação com os atlas:** P2/P12 · G1.

Fechar o defeito de retomada pela cadeia CPU/GPU, sem reset do documento.

**M03.1 — Máquina de lifecycle.** Separar suspender, recriar superfície, reidratar vista e recuperar processo. Invalidar somente o que depende do epoch. Entradas de código: android_main; android_window; android_vulkan_surface.

**M03.2 — Reidratação gráfica.** Reconciliar biblioteca, draws, descritores, câmera e history. Publicar frame inicial completo da nova surface. Entradas de código: dirt_road_resources; instanced_renderer; RHI.

**M03.3 — Checkpoint e tarefas.** Preservar texto/cena; picker e build usam tokens próprios. Rejeitar completions de projeto fechado. Entradas de código: EditorTextInput; ModelPicker; editor_session; stores.

**Critério de passagem:** Retorno do segundo plano restaura imagem coerente com a hierarquia, incluindo projeto importado; teste repetido e processo morto têm evidência.

**Validação vinculada:** LIF01–LIF09, IMP09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M04 — Viewport e grade em profundidade

**Pré-requisitos:** M01, M03. **Relação com os atlas:** P2 · G1/G2.

Corrigir overlay da grade, transições de escala, projeção e seleção depois de mudanças de layout.

**M04.1 — Contrato de RenderView.** Centralizar retângulo, coordenadas, depth e epochs. Distinguir HUD, gizmos e desenho no mundo. Entradas de código: editor_view; camera; renderer view.

**M04.2 — Grade estável.** Implementar passe 3D/analítico depth-tested e blend de níveis. Manter eixos de piso coerentes e orientação HUD separada. Entradas de código: editor_grid.h; novo passe no renderer.

**M04.3 — Picking e gizmos.** Validar malha/BVH existente, IDs, projeção e pivot local/global sob resize e hierarchy. Entradas de código: editor_pick_mesh; map_scene; gizmo; session.

**Critério de passagem:** Eixos não atravessam objetos opacos por defeito; zoom/órbita não produzem apagões; picking coincide com o pixel após resize.

**Validação vinculada:** VIE01–VIE09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M05 — Entrada de texto realmente inline

**Pré-requisitos:** M01, M03. **Relação com os atlas:** P5/P7 · G1.

Trocar o modal por sessões ancoradas de edição com IME, Unicode, transações e focus routing.

**M05.1 — Ponte embutida.** Criar host View para IDE/campo, InputConnection e deltas revisionados. Retirar AlertDialog desses fluxos. Entradas de código: EditorTextInput.java; android_editor_text_input; UI host.

**M05.2 — Transações de campo.** Implementar rascunho local, validação por tipo/unidade, commit/cancel e live preview válido. Entradas de código: property descriptors; inline editor; history.

**M05.3 — Foco e geometria.** Reconciliar IME insets, clipping, scroll, clipboard e undo de foco sem repassar input à câmera. Entradas de código: AndroidHost; layout; input router; code workspace.

**Critério de passagem:** Digitação/colagem ocorre na superfície real do conteúdo; campos não abrem formulário e preservam valores ao cancelar.

**Validação vinculada:** TXT01–TXT09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M06 — IDE, compilação automática e console visível

**Pré-requisitos:** M02, M05. **Relação com os atlas:** P5/P7 · G3.

Concluir a jornada escrever → compilar → publicar → diagnosticar sem botão habitual de Apply.

**M06.1 — Pipeline de edição/build.** Debounce e snapshot; salvar seguro; publicação compare-generation. Colagem é transação única; IME composto não dispara build incompleto a cada evento. Entradas de código: editor_code_workspace; serviço de build existente; editor_session.

**M06.2 — IDE e linguagem.** Toolbar única de ícones, tabs, source navigation, autocomplete compatível e criação C# pelo provedor. Entradas de código: editor_screen; CodeEditorHost; registry de linguagem.

**M06.3 — Console editorial.** Filtros, lista virtualizada, collapse, detalhe e salto para fonte/objeto; separar logs e erros ativos. Entradas de código: DiagnosticStore; ConsoleView; integração runtime/compilador.

**Critério de passagem:** Criar, colar, corrigir e executar script novo pelo aparelho sem editor externo e sem reanexação; console mostra todo o percurso.

**Validação vinculada:** IDE01–IDE09, OBS01–OBS09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M07 — Importação estrutural e pivôs

**Pré-requisitos:** M01, M04. **Relação com os atlas:** P1/P2 · G2/G3.

Substituir a fonte plana de draws por uma IR de cena com árvores e recursos.

**M07.1 — IR e adaptador.** Definir nós, roots, local transforms, meshes/slots compartilhados. Adaptar parser com validação e contrato de erros. Entradas de código: gltf_import.*; ImportedSceneIR proposta.

**M07.2 — Instanciação da árvore.** Criar objetos/filhos incluindo nós vazios; preservar pivôs e identidade; extrair draws a partir da cena. Entradas de código: editor_map_scene.*; runtime scene_graph; SceneAssetInstantiator.

**M07.3 — Migração e regressão.** Converter com fonte original quando possível; relatar conflitos de formatos achatados; validar portas/assemblies genéricos. Entradas de código: archive migrations; tests importer; asset_registry.

**Critério de passagem:** Parte articulada importada é editável isoladamente sem alterar irmãos; árvore e pivôs persistem e picking segue o objeto correto.

**Validação vinculada:** IMP01–IMP08, UNI01. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M08 — Projeto, assets e reimportação

**Pré-requisitos:** M06, M07. **Relação com os atlas:** P1/P7/P12 · G3.

Mover importação ao projeto e consolidar instâncias/overrides, dependências e reimportação.

**M08.1 — Browser autoral.** Importar, nomes reais, miniaturas, pastas, seleção e destinos; separar recurso da instância e do material slot. Entradas de código: editor_filesystem; ModelPicker; asset browser; creation catalog.

**M08.2 — Reconciliador.** Subasset IDs, mapas persistentes e three-way overrides. Renomeações ambíguas pedem decisão sem trocar alvo silenciosamente. Entradas de código: asset_registry; SubassetReconciler; SceneAsset.

**M08.3 — Commit/cancel e dependências.** Staging seguro, registry transacional, relatório e cancelamento. Operações mover/excluir têm dependentes e escopo declarado. Entradas de código: import service; filesystem; archive; GPU publication.

**Critério de passagem:** Reimportar não duplica objetos, não perde scripts nos filhos nem troca portas por nome; projeto reabre sem URI temporário.

**Validação vinculada:** IMP05–IMP09, AST01–AST09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M09 — Cobertura GLTF e aparência fiel

**Pré-requisitos:** M08. **Relação com os atlas:** P3/P6 · G2/G3/G6.

Expandir o perfil funcional por dados, recursos e consumidores, sem anunciar suporte que só existe no parser.

**M09.1 — Resolver e imagens.** GLTF externo/GLB com dependências, decode compatível, mipmaps e samplers; limite de memória descomprimida. Entradas de código: AssetSourceProvider; texture loader; material resources.

**M09.2 — PBR e geometria.** Semântica de canais/UV/normal/alpha, sparse accessors/topologias do perfil, materiais e extensões prioritárias. Entradas de código: gltf import adapter; renderer; shader/material system.

**M09.3 — Animação e skinning.** Nodes/joints/clips/morphs, interpolação e bounds; preview e execução; declarar extensões condicionais. Entradas de código: animation runtime; mesh/skinning; importer; inspector.

**Critério de passagem:** Matriz de cobertura publicada e corpus testado; modelos texturizados e animados do perfil podem ser importados/editados/reabertos no telefone.

**Validação vinculada:** AST01–AST09, GFX01, GFX02, UNI05. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M10 — Workspace e menus universais

**Pré-requisitos:** M04, M05, M06, M08. **Relação com os atlas:** P2/P7 · G2.

Substituir layout por docks adaptativos, criação por capacidades e menus contextuais coerentes.

**M10.1 — Comandos e catálogo.** Unificar ações por ID, disponibilidade e transação; separar criar objeto, adicionar componente, recurso e importação. Entradas de código: editor_commands; creation_catalog; component_catalog.

**M10.2 — Painéis e interação.** Implementar alternativa A com fallback B, inspector accordion e long-press sem conflito de scroll/drag. Entradas de código: WorkspaceLayoutModel; editor_screen; input router.

**M10.3 — Ícones e acessibilidade.** Integrar manifesto sem atalhos fictícios, validar alvos/tamanhos e contraste; remover controles textuais redundantes. Entradas de código: ui_icon_id/atlas; design tokens; a11y labels.

**Critério de passagem:** Todos os fluxos principais cabem na tela real sem sobreposição; não existe comando nuclear especializado em carro/porta/demo.

**Validação vinculada:** UX01–UX09, UNI02, UNI03. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M11 — Retrato opcional exclusivo do IDE

**Pré-requisitos:** M03, M05, M06, M10. **Relação com os atlas:** P7/P12 · G1.

Permitir código em retrato pelo host com retorno íntegro à cena em paisagem.

**M11.1 — Política de orientação.** Solicitar preferência por workspace respeitando sistema/dispositivo; registrar fallback e restaurar política anterior. Entradas de código: AndroidManifest; Activity real; WorkspaceOrientationPolicy.

**M11.2 — Reflow e teclado.** Drawer de arquivos, tabs e console responsivo; insets reais; nenhuma rotação manual da textura para simular IME. Entradas de código: CodeEditorHost; layout; IME bridge.

**M11.3 — Recuperação de sessão.** Preservar caret, composição possível, buffers, seleção e tasks; reanexar host a ProjectSession. Entradas de código: session persistence; lifecycle; runtime owner.

**Critério de passagem:** Alternar código retrato/cena paisagem não perde fonte/cena nem muda câmera de jogo; limitações do host ficam explícitas.

**Validação vinculada:** TXT06, TXT08, LIF04, LIF05, UX05. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M12 — Composição e gameplay reutilizáveis

**Pré-requisitos:** M02, M07, M08, M09, M10. **Relação com os atlas:** P4–P9 · G6.

Validar os sistemas existentes/novos em composições diferentes e ampliar a API sem casos por nome.

**M12.1 — Cena e scripts reutilizáveis.** Múltiplos behaviors, interfaces/dados, prefabs/cenas aninhadas e referências remapeadas. Entradas de código: runtime; managed API; resource registry; inspector.

**M12.2 — Física e câmera por capacidade.** Corpos/formas/joints compartilhados, input por ações e câmera independente. Corrigir autoridade de transform. Entradas de código: physics facade; character/camera components; script API.

**M12.3 — Animação, áudio e UI.** Integrar consumidores necessários à experiência de teste; gerenciadores lógicos sem malha e runtime-only explícito. Entradas de código: animation/audio/UI runtime; lifecycle; exporter.

**Critério de passagem:** FollowTarget controla câmera, luz e marcador; joint controla objetos distintos; cenas são montadas sem editar código da engine.

**Validação vinculada:** UNI01–UNI09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M13 — Autoria gráfica extensível

**Pré-requisitos:** M09, M12. **Relação com os atlas:** P10 · G3–G11.

Trazer materiais/shaders/compute e efeitos gerais preservando o caminho de autoria mobile.

**M13.1 — Contrato de material/shader.** Recurso versionado, reflection, propriedades no inspector, erro localizado e última publicação válida. Entradas de código: material/shader registry; compiler; code workspace; console.

**M13.2 — Passes e compute.** APIs de buffers/imagens/dispatch, frame graph, dependências e negociação de capacidades; sem função especial por demo. Entradas de código: renderer/RHI; rendergraph; managed graphic API.

**M13.3 — Qualidade incremental.** Executar a ordem do atlas: PBR/cor, sombras, visibilidade, temporal, efeitos/bake e extensões opcionais. Entradas de código: sistemas G2–G11; testes visuais e dispositivos.

**Critério de passagem:** Usuário cria material, efeito e compute do escopo no projeto, salva/reabre/exporta; sem substituir motor nem exigir workstation oculta.

**Validação vinculada:** GFX01–GFX09, UNI08. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M14 — Desempenho e escalabilidade sustentados

**Pré-requisitos:** M12, M13. **Relação com os atlas:** P12 · G4/G12–G14.

Medir e otimizar a combinação de editor, assets, scripting e renderer mantendo imagem e identidade.

**M14.1 — Budgets e profiling.** Métricas CPU/GPU quando disponíveis, memória e temperatura; editor/Play medidos separados. Entradas de código: profiler; Android performance; diagnostic exporter.

**M14.2 — Incremental e residência.** Reduzir rebuild de geometria, uploads e UI redundante; streaming/LOD/instancing preservam objetos. Entradas de código: resource registry; extraction; renderer; tasks.

**M14.3 — Experimentos condicionais.** Avaliar caminhos avançados com A/B equivalente. Rejeitar experimento sem ganho ou sem autoria/debug sustentáveis. Entradas de código: backends gráficos experimentais; atlas G13/G14.

**Critério de passagem:** Relatório traz métricas absolutas e metadados; nenhuma perda de editabilidade/qualidade escondida para aumentar FPS.

**Validação vinculada:** QA02–QA06, GFX07–GFX09. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

## M15 — Migração, distribuição e fechamento

**Pré-requisitos:** M11, M12, M13, M14. **Relação com os atlas:** P12 · G15.

Fechar o conjunto declarado de funcionalidades como produto e remover dependências legadas de demos/UI.

**M15.1 — Migrações e regressões.** Rodar projetos antigos, imports, scripts, formats e recovery; preservar o que não é convertido automaticamente. Entradas de código: archive; registry migrations; suite completa.

**M15.2 — Pacote independente.** Validar jogo fora do editor com código, assets e shaders corretos; caminho local/offline documentado. Entradas de código: exporter; build; runtime app.

**M15.3 — Corte do legado.** Remover modal de edição e UI substituída do build normal; documentar capacidades estáveis/condicionais/ausentes. Entradas de código: editor legacy paths; docs; release checks.

**Critério de passagem:** Três composições autoradas passam do projeto vazio ao jogo independente; checklist registra executado, falhou e pendente sem maquiar status.

**Validação vinculada:** QA01–QA09, UNI09, suite completa. **Rollback:** manter a revisão anterior e os dados-fonte; reverter a implementação sem apagar documentos novos. Mudanças de formato requerem migrador e backup antes do corte.

# 17. Plano de aceitação e evidências

As fichas abaixo são roteiros de teste, não resultados. O pacote JSON preserva `status: nao_executado`. O implementador registra ambiente, passos, observado, evidência e falhas. Um teste que depende de teclado/driver/lifecycle real não pode ser aprovado somente com mocks de host.

Os testes podem usar fixtures automatizadas de assets e geração de documentos no harness. Isso não substitui as sessões manuais de autoria pelo editor. A prova final precisa de um usuário construindo as composições, e não um método interno que já abre tudo pronto.

## TXT — Texto e edição inline

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| TXT01 · Código no lugar | Tocar em uma linha, inserir e selecionar texto no IDE. | Caret e texto permanecem no próprio editor; nenhuma caixa modal ou cópia separada é aberta. |
| TXT02 · Transform no campo | Editar X/Y/Z com sinal, decimal e valor intermediário incompleto. | Rascunho inválido não vira zero; confirmar aplica um valor válido e uma transação. |
| TXT03 · Composição Unicode | Usar acentos, combinação de caracteres, emoji e exclusão em sequência. | Nenhum byte/caractere parcial; seleção e offsets da API coincidem com a apresentação. |
| TXT04 · Clipboard multiline | Colar um arquivo de múltiplas linhas e desfazer/refazer. | Uma colagem forma uma operação, preserva quebras de linha e não duplica conteúdo. |
| TXT05 · Cancelamento numérico | Alterar um valor com preview e cancelar a sessão. | Valor original e estado do documento são restaurados sem nova alteração oculta. |
| TXT06 · Teclado e viewport | Abrir/fechar IME em diferentes campos e alternar painel. | Campo/caret fica visível; o mesmo toque não movimenta a câmera; não há resize duplo. |
| TXT07 · Conflito de revisão | Modificar a fonte externamente enquanto um buffer está dirty. | Conflito é apresentado; nenhuma versão é sobrescrita silenciosamente. |
| TXT08 · Orientação editando | Mudar configuração com seleção e texto ainda não salvo. | Buffer, seleção e revisão persistem; recuperação de composição tem política explícita. |
| TXT09 · Histórico com foco | Executar Undo no código e depois num transform. | Históricos independentes seguem o foco; Undo de texto não move objetos. |

## CMP — Componentes e schemas

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| CMP01 · Erro sem sumiço | Anexar dois scripts válidos e introduzir erro em um. | Ambas as instâncias permanecem visíveis com IDs/valores e status correto. |
| CMP02 · Catálogo após edição | Editar, desfazer e refazer fonte válida. | Lista de adicionar conserva os tipos conhecidos; não depende de Apply manual para reaparecer. |
| CMP03 · Expandir não esconder | Abrir/recolher um componente com vários irmãos. | Outros componentes continuam alcançáveis e identificados na mesma lista. |
| CMP04 · IDs estáveis de campo | Renomear campo preservando PropertyId e recompilar. | Valor salvo é preservado; mudança incompatível produz diagnóstico/migração. |
| CMP05 · ID duplicado | Criar dois tipos com mesmo TypeId. | Erro mostra os arquivos envolvidos e nenhuma escolha arbitrária é publicada. |
| CMP06 · Excluir fonte | Remover arquivo de um script anexado. | Componente fica não resolvido com dados preservados; não some nem executa como atual. |
| CMP07 · Duplicar subárvore | Duplicar objeto com scripts e referências internas/externas. | Novos IDs internos; referências internas remapeadas e externas válidas mantidas. |
| CMP08 · Remover durante Play | Remover/desabilitar comportamento no mundo de execução permitido. | Binding e callbacks encerram corretamente; autoria não muda sem comando explícito. |
| CMP09 · Runtime-only | Criar componente por script em Play e parar. | Inspeção de execução o identifica; retorno ao Edit preserva a cena autorada. |

## IDE — Compilação e workspace

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| IDE01 · Novo C# | Criar comportamento pela opção C# com ícone e destino. | Arquivo abre no IDE; o tipo só é anunciado utilizável conforme resultado do provedor. |
| IDE02 · Colar e publicar | Colar código válido no modo Edit e aguardar análise automática. | Compila/publica sem botão obrigatório; status informa geração e alvo. |
| IDE03 · Colar com erro | Colar código inválido e corrigi-lo inline. | Erro localizável; última versão válida é identificada; nova correção publica automaticamente. |
| IDE04 · Resultado atrasado | Atrasar build A e concluir build B mais novo antes de A. | A nunca substitui B; geração de diagnósticos não é confundida. |
| IDE05 · Play e fonte divergente | Manter Play A; editar fonte B com erro; pedir novo Play. | Execução atual e fonte são diferenciadas; novo Play não usa A fingindo executar B. |
| IDE06 · Múltiplos behaviors | Anexar duas instâncias permitidas de comportamento com parâmetros diferentes. | Instâncias independentes, IDs distintos e nenhum overwrite por nome de classe. |
| IDE07 · Navegação de erro | Tocar no diagnóstico de um arquivo não aberto. | Arquivo/tab abre na posição exata sem descartar outro buffer dirty. |
| IDE08 · Projeto fechado | Fechar projeto durante compilação e abrir outro. | Resultado não contamina o novo projeto; fonte e tarefa antiga ficam isoladas. |
| IDE09 · Jornada offline | Criar, editar, compilar, salvar e executar novo código sem rede. | Trilha declarada mobile funciona sem dependência remota oculta. |

## OBS — Console e observabilidade

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| OBS01 · Fontes diversas | Emitir informação, aviso e erro nativos/managed/importador. | Console unifica eventos e permite filtrar origem/severidade. |
| OBS02 · Correlação | Executar importação, build e retomada em sequência. | Cada evento possui tarefa/geração pertinente e a narrativa pode ser reconstruída. |
| OBS03 · Pressão de logs | Gerar rajada acima do limite configurado. | Fila e memória continuam bounded; UI responsiva; descarte é contabilizado. |
| OBS04 · Problemas separados | Limpar logs durante erro de compilação ativo. | Erro continua no store de problemas; limpar não muda status do projeto. |
| OBS05 · Rolagem controlada | Rolar histórico para trás enquanto novos logs chegam. | Console não arrasta o usuário; seguir saída pode ser retomado explicitamente. |
| OBS06 · Agrupamento | Emitir mensagens iguais em contextos/objetos diferentes. | Agrupamento preserva contexto ou distingue assinaturas; contagem e timestamps corretos. |
| OBS07 · Alvo obsoleto | Abrir log de objeto de uma sessão Play encerrada. | Não seleciona objeto diferente; indica alvo indisponível ou sessão anterior. |
| OBS08 · Exportação | Exportar um trecho de sessão com metadados. | Inclui somente escopo escolhido; conteúdo do projeto não é enviado automaticamente. |
| OBS09 · Custo | Comparar baseline com console aberto, fechado e em rajada. | Tempos e memória registrados; não inventar custo zero nem resultado de GPU indisponível. |

## VIE — Viewport e grade

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| VIE01 · Ocultação de eixos | Colocar objeto opaco cobrindo origem/eixos no piso. | Parte atrás do objeto é ocultada; widget de orientação HUD continua visível. |
| VIE02 · Zoom contínuo | Aproximar/afastar atravessando várias escalas. | Grade troca densidade com transição estável, sem apagão abrupto. |
| VIE03 · Horizonte | Orbitar quase paralelo ao plano e atravessar sua altura. | Sem linhas explosivas, NaN ou flips inesperados; fade/política de plano definidos. |
| VIE04 · Ortográfica | Alternar vistas superior/frontal/lateral e perspectiva. | Projeção, picking e escolha do plano editorial seguem a regra documentada. |
| VIE05 · Resize e insets | Abrir painéis/teclado e mudar tamanho de janela. | Gizmo e picking coincidem com o pixel real sem offsets duplicados. |
| VIE06 · Escalas hierárquicas | Manipular filho de pai rotacionado com escala não uniforme. | Transform mundial/local é consistente; shear incompatível não é descartado silenciosamente. |
| VIE07 · Pivô versus centro | Alternar modo de gizmo numa peça articulada. | Centro visual não reescreve a origem do asset; rotação local preserva pivô. |
| VIE08 · Instâncias compartilhadas | Selecionar duas instâncias da mesma malha em posições distintas. | Cada toque resolve o ObjectId correto, mesmo com instancing. |
| VIE09 · Depth e temporal | Ativar caminhos AA/DR do perfil e trocar surface. | Grade não usa depth/history errado; HUD não recebe ghosting da cena. |

## LIF — Retomada e recuperação

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| LIF01 · Home e retorno | Enviar app ao fundo e voltar com projeto GLB aberto. | Hierarquia e viewport coerentes; nada exige reinício manual. |
| LIF02 · Bloqueio de tela | Bloquear/desbloquear durante Edit e Play. | Estado retomado segundo política de relógio; sem grande salto físico por delta acumulado. |
| LIF03 · Seletor de arquivos | Abrir picker, cancelar e abrir novamente. | Pausa não cancela indevidamente o pedido; tokens não são reutilizados para outra ação. |
| LIF04 · Ciclos de orientação | Alternar IDE retrato e cena paisagem repetidamente. | Documento, tabs, seleção e runtime não duplicam nem desaparecem. |
| LIF05 · Recriação de Activity | Forçar recriação permitida pelo ambiente de teste. | Host reanexa sessão ou recupera checkpoint; sem ponteiros de janela antiga. |
| LIF06 · Morte de processo | Encerrar processo depois de checkpoint conhecido. | Reabertura recupera cena e rascunhos íntegros, indicando recuperação. |
| LIF07 · Surface epoch | Recriar superfície e verificar uploads/culling/history. | Recursos da geração antiga não são usados; primeiro frame válido repopula a vista. |
| LIF08 · Pressão de memória | Simular pressão e falha de alocação em import/recovery. | Cena anterior permanece; erro claro e nenhum commit parcial. |
| LIF09 · Duração | Executar 100 ciclos definidos e sessão prolongada. | Relatório inclui memória/recursos antes/depois e falhas reais, sem marcar teste não executado como aprovado. |

## IMP — IR, hierarchy e reimportação

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| IMP01 · Assembly articulado | Importar recurso com raiz, pivôs vazios e filhos de malha. | Estrutura e ordem de filhos correspondem à fonte; pais vazios não desaparecem. |
| IMP02 · Parte isolada | Selecionar/mover uma porta ou braço filho. | Somente o filho muda; mover raiz transporta toda a composição. |
| IMP03 · Submeshes | Importar nó com vários material slots/primitives. | Um nó autoral mantém slots; não são inventados objetos por draw sem intenção. |
| IMP04 · Instancing | Importar nós que referenciam mesma malha. | Geometria compartilhada, transforms e identidades independentes. |
| IMP05 · Reimportar edição | Editar material/transform/script local e reimportar fonte. | Overrides persistem e novos dados reconciliam; não duplica instâncias. |
| IMP06 · Nomes ambíguos | Usar nomes duplicados/vazios e depois reordenar a fonte. | Nenhuma troca silenciosa de subasset; mapa ou conflito explicita ambiguidade. |
| IMP07 · Matrizes/pivôs | Importar nó com matriz e origem fora do centro do mesh. | Transform e pivot preservados; limites TRS declarados. |
| IMP08 · Fonte achatada | Importar malha única com aparência de assembly. | Não inventa partes sem evidência; mostra estrutura real e limitações de separação. |
| IMP09 · Cancel/commit | Cancelar antes do commit e provocar falha durante publicação. | Registro/objetos antigos intactos; resultado incompleto não vira versão corrente. |

## AST — Assets e compatibilidade

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| AST01 · PBR texturizado | Importar caso com mapas base/normal/MR/AO/emissive e UVs. | Materiais e samplers do perfil funcionam em Edit/Play e após reabrir. |
| AST02 · Dependências externas | Importar glTF e GLB com arquivos externos via acesso autorizado. | Resolver encontra/copia dependências e reporta ausentes; funciona offline depois. |
| AST03 · Extensão exigida | Importar arquivo com extensão obrigatória não implementada. | Recusa com nome e causa; não aceita geometria ou material incorreto. |
| AST04 · Arquivo malformado | Exercitar offsets, counts, sparse e dados inválidos. | Sem crash/out-of-bounds; erro específico; corpus e limites documentados. |
| AST05 · Memória decodificada | Usar imagem grande/arquivo comprimido com expansão alta. | Budget é checado antes da alocação excessiva; cena anterior permanece. |
| AST06 · Renomear/mover | Mover recurso referenciado dentro do projeto. | Referências por identidade sobrevivem; caminhos e metadados são atualizados. |
| AST07 · Excluir dependente | Excluir material/textura em uso. | Dependentes são informados e política de remoção/placeholder é consistente. |
| AST08 · Cor e alpha | Comparar casos OPAQUE/MASK/BLEND e normal map. | Ordenação/depth/cutoff/dupla face e canais do perfil preservam semântica. |
| AST09 · Skin/morph/clip | Importar personagem com joints, morphs e animações do perfil. | Alvos, interpolação e bounds corretos; omissões são declaradas, não ocultas. |

## UX — Layout e interação

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| UX01 · Menus distintos | Buscar importar modelo, criar objeto e adicionar componente. | Entradas têm contextos próprios; import não está escondido em geometria. |
| UX02 · Long-press | Segurar item, rolar antes do limiar e arrastar para novo pai. | Gestos não disparam várias ações; target e cancelamento coerentes. |
| UX03 · Bordas | Abrir menu contextual nas quatro bordas com IME visível. | Conteúdo permanece acessível, sem clipping ou botões fora da tela. |
| UX04 · Inspeção extensa | Abrir componente com muitas propriedades e vários irmãos. | Scroll/accordion é contínuo; nenhum componente some por paginação/foco implícito. |
| UX05 · Retrato real | Usar IDE com teclado real e arquivos/console. | Código tem área útil, caret visível; não há toolbar duplicada ou árvore fixa larga. |
| UX06 · Ícones | Comparar manifest com arquivos e comandos disponíveis. | Nome/estado/action corretos; nenhuma opção sem implementação aparece como funcional. |
| UX07 · Alvos e foco | Tocar regiões adjacentes e usar teclado/mouse conectado. | Sem sobreposição ambígua; foco acessível e seleção previsíveis. |
| UX08 · Dados compartilhados | Editar recurso compartilhado versus override de instância. | Alcance da mudança é explícito e não há duplicação silenciosa de materiais. |
| UX09 · Texto secundário | Revisar barras/cabeçalhos e ações destrutivas. | Sem botões permanentes Editar/Aplicar/Copiar-Colar-Restaurar; rótulos necessários continuam claros. |

## UNI — Universalidade e autoria

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| UNI01 · Além do veículo | Repetir fixture de importação com armário e robô. | Mesma IR, hierarquia e APIs funcionam sem código de identificação por nome. |
| UNI02 · Primitiva declarativa | Criar cubo e trocar malha/material depois. | É composição de capacidades; física/scripts não dependem do tipo visual Cube. |
| UNI03 · Objeto lógico | Criar gerenciador de missão sem renderer/collider. | Scripts e dados funcionam sem objeto visual obrigatório. |
| UNI04 · FollowTarget | Usar mesmo comportamento em câmera, luz e marcador. | Comportamento depende do transform, não de objeto especial. |
| UNI05 · Física independente | Usar diferentes colliders em modelos distintos. | Forma física e malha são independentes; combinações inválidas têm diagnóstico. |
| UNI06 · Referências e cenas | Instanciar cena reutilizável duas vezes e editar somente uma. | IDs e overrides independentes; alteração comum tem alcance declarado. |
| UNI07 · Código novo | Criar comportamento não incluído nos exemplos. | Propriedades, eventos, logs e execução funcionam sem alterar engine. |
| UNI08 · Extensão sem água | Executar projeto com módulo de água ausente. | Editor básico, importação, scripts e PBR continuam disponíveis. |
| UNI09 · Três composições | Autor criar interior, assembly e pátio a partir de projetos vazios. | Todos passam por salvar/reabrir/Play e distribuição sem gerador interno de demo. |

## GFX — Gráficos autoráveis e integrados

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| GFX01 · Imagem base | Comparar material PBR e iluminação com referência controlada. | Canais/cor/normal corretos antes de adicionar pós-processamento. |
| GFX02 · Material persistente | Alterar parâmetros e texturas e reabrir projeto. | Mesma aparência e referências; update simples não reconstrói geometria inteira. |
| GFX03 · Shader no aparelho | Criar/editar shader do perfil e provocar erro. | Compila localmente, diagnóstico localizável, última versão válida identificada. |
| GFX04 · Compute geral | Criar kernel do escopo em projeto e modificar parâmetros. | Recursos/dispatch/barreiras do contrato funcionam sem função específica de água. |
| GFX05 · Passe personalizado | Adicionar efeito que declare dependências gráficas. | Validação de recursos/ordem e fallback explícitos; exportação usa mesmo recurso. |
| GFX06 · História por vista | Abrir preview/câmera secundária e fazer corte/resize. | Histórias temporais e câmeras não se contaminam. |
| GFX07 · Batching com autoria | Ativar instancing/culling e selecionar filhos importados. | Melhoria de submissão não elimina IDs, transforms ou editabilidade. |
| GFX08 · Capacidades | Abrir projeto avançado em perfil sem determinado recurso. | Dados autorais preservados, aproximação/indisponibilidade informada. |
| GFX09 · Sustentado | Executar cena combinando recursos em sessão longa. | Registrar frame time, memória e thermal quando disponíveis, sem FPS fabricado. |

## QA — Fechamento e distribuição

| Teste | Procedimento | Resultado exigido |
|---|---|---|
| QA01 · Exportação mínima cedo | Executar jogo mínimo sem workspace logo no baseline. | Não depende de diretórios/estado transitório do editor. |
| QA02 · Metadados | Gravar relatório de teste de desempenho/visual. | Build, aparelho, driver, resolução, cena e duração permitem comparação. |
| QA03 · Import concorrente | Navegar/editar enquanto asset grande é preparado. | UI permanece responsiva dentro da meta definida; cancelamento funciona. |
| QA04 · Métricas comparáveis | Comparar duas versões com mesma cena/resolução/qualidade. | Resultados absolutos e distribuição publicados; sem reduzir conteúdo escondido. |
| QA05 · Recursos após ciclos | Repetir abrir/fechar projetos, Play e recompilação. | Memória/handles não crescem sem limite; resíduos são explicados. |
| QA06 · Sinais indisponíveis | Executar em GPU sem métricas desejadas. | Ausência é declarada; não reportar zero como tempo medido. |
| QA07 · Migração de formato | Abrir projetos anteriores com recursos/IDs legados. | Backup e relatório; incompatibilidades explícitas; nada é descartado silenciosamente. |
| QA08 · Distribuição completa | Empacotar fonte/código/shaders/GLB do cenário-alvo. | Jogo roda no fluxo mobile declarado, offline depois dos assets instalados. |
| QA09 · Corte de legado | Auditar build normal e documentação final. | Sem modal antigo, demos obrigatórias, botões fictícios ou alegações de testes pendentes como aprovados. |

# 18. Rastreabilidade dos pedidos e riscos

A matriz fecha o vínculo entre a mensagem do usuário e os marcos/testes. Não substitui a especificação detalhada dos capítulos anteriores. Ao atualizar o plano, manter os IDs REQ para que uma mudança visual não elimine inadvertidamente um requisito de dados ou runtime.

| Pedido | Resposta técnica | Marcos / testes |
|---|---|---|
| REQ01 · Universalidade sem botões de veículo | Objetos/componentes/recursos e comandos reutilizáveis; exemplos viram fixtures ou assets do usuário. | M01,M10,M12 · UNI01,UNI02,UNI04 |
| REQ02 · Refazer layout pela referência | Identidade Astra preservada; alternativas A/B/C avaliadas com teclado e dimensões reais. | M10,M11 · UX03,UX05,UX09 |
| REQ03 · Mais ícones e menos texto repetido | Manifesto com função/contexto; toolbar sem duplicações e acessibilidade mantida. | M06,M10 · UX06,UX07,UX09 |
| REQ04 · Código digitado no próprio IDE | CodeEditorHost embutido; retirar AlertDialog do fluxo normal. | M05,M06 · TXT01,TXT03,TXT04 |
| REQ05 · Valores editados no campo | Sessão inline transacional e rascunho de valor incompleto sem corromper documento. | M05 · TXT02,TXT05,TXT09 |
| REQ06 · IDE em retrato | Orientação opcional do host por workspace, reflow e retorno à cena. | M03,M11 · LIF04,LIF05,UX05 |
| REQ07 · Teclado vertical adequado | Usar configuração da janela e insets; não prometer rotação isolada de IME. | M05,M11 · TXT06,TXT08,UX05 |
| REQ08 · Colagem com aplicação automática | Atualização de texto e build/publicação por geração, sem Apply habitual. | M02,M06 · IDE02,IDE03,IDE04 |
| REQ09 · Console elaborado | Eventos estruturados, filtros, retenção bounded, stack e navegação para fonte/objeto. | M01,M06 · OBS01–OBS09 |
| REQ10 · Logs dentro do editor | Runtime/compilador/importador/lifecycle conectados ao mesmo bus; Logcat como sink complementar. | M01,M06 · OBS01,OBS02,OBS04 |
| REQ11 · Linhas sobre objetos | Separar grade/eixos de piso depth-tested de HUD e gizmos intencionais. | M04 · VIE01,VIE09 |
| REQ12 · Grade piscando ao zoom | Escala contínua, blend, cobertura e projeção/horizonte testados. | M04 · VIE02,VIE03,VIE04 |
| REQ13 · Escolha de linguagem e logo C# | Provider registry; primeiro C# validado; não anunciar outras linguagens como prontas. | M06 · IDE01,IDE09,UX06 |
| REQ14 · Componente some mas executa | Instância, schema e build independentes; lista por ID com status honesto. | M02 · CMP01,CMP02,CMP03,IDE05 |
| REQ15 · Reaplicar não restaura | Eliminar invalidação de catálogo e duplicação por reanexação; rastrear gerações. | M02,M06 · CMP02,IDE04,IDE06 |
| REQ16 · Importação fora de Geometria | Projeto/assets como local de entrada; criar/registrar/instanciar separados. | M08,M10 · UX01,AST06 |
| REQ17 · Hierarchy de veículo e filhos | ImportedSceneIR preserva todos os nós úteis, pais, ordem e transforms locais. | M07 · IMP01,IMP02,IMP03 |
| REQ18 · Mover só uma porta | Seleção por objeto filho, pivot da fonte, override local e render derivado. | M04,M07,M08 · VIE07,IMP02,IMP05 |
| REQ19 · Importador de cobertura ampla | Perfil core, imagens, externo, skins/animações e extensões com consumidores e limites. | M09 · AST01–AST09 |
| REQ20 · Menu de adicionar objetos | Catálogo por capacidade/composto, search, contexto e favoritos opcionais. | M10 · UX01,UNI02,UNI03 |
| REQ21 · Long-press e opções | Menu ancorado/sheet com regras de toque/drag/scroll e acesso alternativo. | M10 · UX02,UX03,UX07 |
| REQ22 · Retomada com viewport vazio | Diagnóstico CPU→GPU, epochs, reidratação e checkpoint sem apagar documento. | M03 · LIF01–LIF09 |
| REQ23 · Lista de novos ícones | Nomes estáveis, função, colocação, prioridade e status; usuário produz as artes. | M10 · UX06,UX09 |
| REQ24 · Código cria possibilidades gerais | Behavior múltiplo, classes auxiliares, APIs de cena, física, câmera e recursos. | M06,M12 · UNI03–UNI07 |
| REQ25 · Qualidade gráfica máxima planejada | Manter G0–G15, shader/material/compute autoráveis e recursos condicionais por hardware. | M13,M14 · GFX01–GFX09 |
| REQ26 · Desempenho sem perder autoria | Instancing/culling/streaming por projeção com IDs, medição sustentada e budgets. | M14 · GFX07,GFX09,QA02–QA06 |
| REQ27 · Mobile para mobile | Editor, import, scripts e shaders no aparelho; distribuição sem dependência oculta de PC. | M06,M09,M13,M15 · IDE09,QA01,QA08 |
| REQ28 · Não pular etapas | Grafo único de pré-requisitos e gates com evidências; primeiro sessão estável. | M00–M15 · QA09 |
| REQ29 · Não perder arquivos atuais | Backup, journal, migração, formato versionado e conflito explícito. | M01,M03,M08,M15 · TXT07,LIF06,IMP09,QA07 |
| REQ30 · Sem copiar/colar/restaurar em cada card | Retirar controles permanentes redundantes; ações úteis em contexto, clipboard textual normal. | M05,M10 · UX09,TXT04 |

## 18.1 Riscos que precisam permanecer visíveis

**Dados e migração:** a árvore original pode ter sido perdida em cenas antigas achatadas. Preservar backup e fonte; não reconstruir por adivinhação. IDs e overrides são pré-requisitos para reimportar sem perda.

**Edição e compilação:** aplicar ao colar não deve implicar execução automática de código arbitrário fora da política definida. Um script do projeto pode consumir CPU/memória ou bloquear execução; um timeout de tarefa managed não encerra com segurança qualquer código preso numa thread. Definir confiança do projeto, execução optativa de ferramentas de editor e estratégia de interrupção/processo quando necessária. Não anunciar sandbox absoluto.

**Shader/compute externo:** reflexão e validação de recursos não impedem todo hang de GPU ou custo excessivo. Limites de dispatch, capabilities, cancelamento entre tarefas e recuperação de device precisam de diagnóstico; conteúdo não confiável exige política de execução explícita.

**UI nativa embutida:** a coexistência de Views Android e superfície Vulkan precisa de testes de z-order, clipping, teclado, foco e sincronização. O objetivo é uma área de edição real integrada; não é trocar o renderer por um WebView disfarçado. A prova do host de texto precede o polimento do IDE.

**Orientação:** o host pode receber decisões do sistema diferentes da preferência solicitada. A interface deve continuar utilizável; dados não podem depender de conseguir travar a orientação. O modo retrato só é ativado como opção pronta depois de passar na matriz de dispositivos/layouts alvo.

**Cobertura do importador:** fontes podem ser válidas mas fora do perfil, exigir extensões ou exceder memória. Diagnosticar não é fracasso de universalidade; falsa importação sem estrutura, textura ou animação é que compromete o contrato.

**Performance:** async import/build não é desempenho gratuito; workers competem por CPU/RAM e temperatura. Respeitar prioridades e budgets de edição. Reconstrução total de bibliotecas pode ser transição válida, mas não solução permanente para qualquer mudança pequena.

## 18.2 Perguntas que a implementação deve resolver por evidência

Qual a classe/source set real da Activity em cada variante? Qual VM e compilador estão empacotados? Quais gerações o runtime e o inspetor usam no momento exato do sumiço? O defeito de retomada ocorre sem importação recente? O GLB problemático tem de fato nós separados para a porta? Quais extensões aparecem no corpus real? Qual a menor tela útil e a API Android mínima prometidas?

Essas perguntas não impedem o plano. São entregas objetivas do baseline e dos testes, evitando que o agente escolha uma resposta conveniente sem ler o build, o trace ou o arquivo-fonte. A nova imagem de long-press será incorporada ao design sem alterar os contratos autorais.

# 19. Manifesto de ícones para geração

Arquivos propostos em PNG transparente: o basename também é a chave lógica do atlas. Os grupos abaixo não obrigam a exibir todos os ícones ao mesmo tempo. `agora` significa necessário na refundação correspondente, não já implementado. Marcos posteriores mantêm o ícone fora da UI até o recurso estar pronto. Ícones de estado devem ser visualmente distintos de ações quando não houver interação.

O JSON/CSV do pacote acrescenta categoria, tipo de uso e marco. Não há ícones obrigatórios de carro, barco ou porta: esses itens são assets/composições criados pelo usuário.

## Workspace

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `workspace_scene.png` | **Vista da cena.** Alterna para o workspace 3D; restaura seleção/câmera. Local: Barra contextual do IDE. | agora |
| `workspace_code.png` | **Código.** Abre o workspace de código sem diálogo. Local: Navegação contextual. | agora |
| `panel_files.png` | **Arquivos.** Abre/recolhe browser ou drawer. Local: Workspace/IDE. | agora |
| `panel_hierarchy.png` | **Hierarquia.** Abre/recolhe árvore de objetos. Local: Workspace de cena. | agora |
| `panel_inspector.png` | **Inspetor.** Abre/recolhe propriedades da seleção. Local: Workspace de cena. | agora |
| `panel_console.png` | **Console.** Abre logs e problemas; pode levar badge numérico. Local: Barra/status. | agora |
| `search.png` | **Buscar.** Busca no contexto ativo; sem duplicação de barras. Local: IDE/browser/catálogos. | agora |
| `menu_more.png` | **Mais ações.** Abre menu contextual de comandos menos frequentes. Local: Cabeçalhos. | agora |
| `navigate_back.png` | **Voltar.** Retorna um nível de navegação sem apagar estado. Local: Importador/drawers. | agora |
| `settings.png` | **Configurações.** Abre opções reais do contexto/projeto. Local: Menu principal/contextual. | agora |

## IDE e execução

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `script_new.png` | **Novo script.** Cria fonte no projeto pelo seletor de linguagem. Local: Toolbar do IDE. | agora |
| `language_csharp.png` | **C#.** Identifica a linguagem e seu provedor disponível. Local: Seletor de linguagem e tipo de arquivo. | agora |
| `script_open.png` | **Abrir código.** Abre a fonte do comportamento selecionado. Local: Card do script/menu do objeto. | agora |
| `file_save.png` | **Salvar arquivo.** Força gravação do buffer; auto-save não elimina controle explícito. Local: Toolbar/overflow do IDE. | agora |
| `file_save_all.png` | **Salvar arquivos.** Grava buffers modificados do workspace. Local: Overflow do IDE. | opcional |
| `history_undo.png` | **Desfazer.** Desfaz no histórico do foco atual. Local: Barra contextual única. | agora |
| `history_redo.png` | **Refazer.** Refaz no histórico do foco atual. Local: Barra contextual única. | agora |
| `find_replace.png` | **Buscar e substituir.** Abre ferramentas inline de busca/substituição. Local: Área contextual do IDE. | agora |
| `build_busy.png` | **Compilando.** Indica build da geração atual; abre detalhe de tarefa. Local: Status do código. | agora |
| `build_current.png` | **Código atualizado.** Indica última publicação coerente com a fonte. Local: Status do código. | agora |
| `build_stale.png` | **Código alterado.** Distingue rascunho mais novo de versão válida/rodando. Local: Status/card. | agora |
| `play.png` | **Executar.** Inicia Play com política de versão explícita. Local: Barra de execução. | agora |
| `pause.png` | **Pausar.** Pausa a sessão Play sem destruir autoria. Local: Barra de execução. | agora |
| `stop.png` | **Parar.** Encerra Play e devolve a autoria. Local: Barra de execução. | agora |
| `step_frame.png` | **Avançar passo.** Avança simulação pausada quando suportado. Local: Barra/overflow de execução. | agora |
| `keyboard.png` | **Teclado.** Controla foco/visibilidade solicitada do IME. Local: IDE mobile. | agora |
| `orientation_portrait.png` | **Código em retrato.** Ativa preferência de orientação do workspace. Local: Menu de layout do IDE. | M11 |
| `orientation_landscape.png` | **Manter paisagem.** Indica/seleciona preferência landscape. Local: Menu de layout do IDE. | M11 |

## Console e estado

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `severity_info.png` | **Informação.** Filtra ou identifica eventos informativos. Local: Console. | agora |
| `severity_warning.png` | **Aviso.** Filtra warnings e sinaliza aproximações/limites. Local: Console/importador. | agora |
| `severity_error.png` | **Erro.** Filtra erros e abre diagnóstico pertinente. Local: Console/status. | agora |
| `filter.png` | **Filtrar.** Abre filtros de categoria/origem/contexto. Local: Console e browser. | agora |
| `collapse_repeats.png` | **Agrupar repetidos.** Agrupa mensagens por assinatura preservando contadores. Local: Console. | agora |
| `follow_output.png` | **Seguir saída.** Retoma rolagem automática até o último evento. Local: Console. | agora |
| `clear_log.png` | **Limpar logs.** Limpa histórico no escopo, sem apagar problemas ativos. Local: Console. | agora |
| `export_log.png` | **Exportar diagnóstico.** Exporta trecho e metadados escolhidos. Local: Console/overflow. | agora |

## Hierarquia e objetos

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `object_empty.png` | **Objeto vazio.** Identifica/cria entidade sem capacidade visual obrigatória. Local: Catálogo/hierarquia. | agora |
| `object_group.png` | **Grupo.** Organiza filhos por transform sem assumir geometria. Local: Catálogo/hierarquia. | agora |
| `object_add_child.png` | **Adicionar filho.** Abre catálogo com pai de destino explícito. Local: Menu contextual. | agora |
| `component_add.png` | **Adicionar componente.** Abre registry filtrado pela seleção. Local: Inspetor/menu contextual. | agora |
| `script_attach.png` | **Anexar script.** Escolhe comportamento existente e cria instância no alvo. Local: Inspetor/menu contextual. | agora |
| `rename.png` | **Renomear.** Inicia edição do nome no próprio campo/linha. Local: Menu contextual. | agora |
| `duplicate.png` | **Duplicar.** Duplica seleção/subárvore com remapeamento. Local: Menu contextual. | agora |
| `reparent.png` | **Mudar pai.** Move sob outro objeto com política de transform. Local: Menu contextual/drag feedback. | agora |
| `visibility_on.png` | **Visível.** Estado visível; alterna conforme escopo editorial. Local: Linha da hierarquia. | agora |
| `visibility_off.png` | **Oculto.** Estado oculto distinto de exclusão. Local: Linha da hierarquia. | agora |
| `selection_lock.png` | **Bloquear seleção.** Evita picking do objeto sem removê-lo. Local: Hierarquia/menu. | agora |
| `delete.png` | **Excluir.** Remove por comando com dependências/Undo. Local: Menu contextual separado. | agora |
| `scene_instance.png` | **Instância de cena.** Identifica recurso reutilizável instanciado. Local: Hierarquia/browser. | M08 |

## Viewport

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `tool_select.png` | **Selecionar.** Seleciona objeto por ID. Local: Rail de ferramentas. | agora |
| `tool_move.png` | **Mover.** Manipula translação da seleção. Local: Rail de ferramentas. | agora |
| `tool_rotate.png` | **Girar.** Manipula rotação preservando pivot e autoridade. Local: Rail de ferramentas. | agora |
| `tool_scale.png` | **Escalar.** Manipula escala com validação. Local: Rail de ferramentas. | agora |
| `frame_selection.png` | **Enquadrar seleção.** Enquadra bounds da seleção/subárvore. Local: Viewport/menu. | agora |
| `frame_all.png` | **Enquadrar cena.** Enquadra conteúdo autoral relevante. Local: Menu do viewport. | agora |
| `grid.png` | **Grade.** Mostra/oculta grade editorial do plano escolhido. Local: Menu do viewport. | agora |
| `snap.png` | **Encaixe.** Ativa snapping e abre seus valores. Local: Opções da ferramenta. | agora |
| `space_local.png` | **Espaço local.** Identifica/seleciona eixos locais. Local: Opções da ferramenta. | agora |
| `space_world.png` | **Espaço mundial.** Identifica/seleciona eixos mundiais. Local: Opções da ferramenta. | agora |
| `pivot_origin.png` | **Pivô do objeto.** Gizmo usa origem sem alterar asset. Local: Opções da ferramenta. | agora |
| `pivot_center.png` | **Centro da seleção.** Gizmo usa centro temporário da seleção. Local: Opções da ferramenta. | agora |

## Assets e importação

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `asset_import.png` | **Importar recurso.** Recebe fonte externa no contexto do projeto. Local: Browser/Projeto. | agora |
| `asset_reimport.png` | **Reimportar.** Atualiza recurso preservando identidade/overrides. Local: Inspetor do asset/menu. | M08 |
| `asset_source.png` | **Fonte original.** Revela a fonte/dados do importador. Local: Inspetor de recurso. | agora |
| `asset_instantiate.png` | **Instanciar.** Cria uma instância na cena a partir do recurso. Local: Browser/contexto. | agora |
| `folder.png` | **Pasta.** Identifica diretório do projeto. Local: Browser. | agora |
| `folder_new.png` | **Nova pasta.** Cria pasta com nome inline e destino explícito. Local: Browser/menu. | agora |
| `asset_model.png` | **Modelo 3D.** Identifica SceneAsset/modelo importado. Local: Browser/importador. | agora |
| `asset_mesh.png` | **Malha.** Identifica geometria reutilizável/subasset. Local: Browser/campo de referência. | agora |
| `asset_material.png` | **Material.** Identifica recurso de aparência compartilhado. Local: Browser/slots. | agora |
| `asset_texture.png` | **Textura.** Identifica imagem/textura com semântica. Local: Browser/importador. | M09 |
| `asset_animation.png` | **Animação.** Identifica clip/coleção de tracks. Local: Browser/importador. | M09 |
| `reference_missing.png` | **Referência ausente.** Indica alvo não resolvido sem esconder dados. Local: Campos/browser/hierarquia. | agora |
| `override_changed.png` | **Alteração local.** Indica override da instância em relação à origem. Local: Inspetor/hierarquia. | M08 |
| `import_report.png` | **Relatório de importação.** Abre diagnóstico e cobertura da fonte. Local: Importador. | agora |

## Capacidades e expansão

| Arquivo | Função e local | Disponibilidade planejada |
|---|---|---|
| `component_camera.png` | **Câmera.** Identifica câmera anexável ou objeto que a contém. Local: Catálogo/inspetor/hierarquia. | agora |
| `component_light.png` | **Luz.** Identifica capacidade de iluminação, não preset Sol obrigatório. Local: Catálogo/inspetor. | agora |
| `component_body.png` | **Corpo físico.** Identifica body com modo definido por propriedade. Local: Catálogo/inspetor. | agora |
| `component_collider.png` | **Colisor.** Identifica forma de colisão independente da malha. Local: Catálogo/inspetor. | agora |
| `component_joint.png` | **Junta.** Identifica restrição física entre corpos. Local: Catálogo/inspetor. | agora |
| `component_sensor.png` | **Sensor.** Identifica detecção/trigger por capacidade. Local: Catálogo/inspetor. | agora |
| `component_behavior.png` | **Comportamento.** Identifica instância de script, independente de linguagem. Local: Inspetor/hierarquia. | agora |
| `component_audio.png` | **Áudio.** Identifica fonte/consumidor de áudio implementado. Local: Catálogo/inspetor. | M12 |
| `component_skeleton.png` | **Esqueleto.** Identifica rig/joints sem confundir com body físico. Local: Importador/hierarquia. | M09 |
| `asset_shader.png` | **Shader.** Identifica código/recurso gráfico autorável. Local: Browser/IDE. | M13 |
| `asset_compute.png` | **Compute.** Identifica kernel e contrato de recursos. Local: Browser/IDE. | M13 |
| `component_environment.png` | **Ambiente.** Identifica recurso/componente de céu/ambiente. Local: Catálogo/inspetor. | M13 |

# 20. Protocolo para o agente de implementação

Use este capítulo como contrato de execução no repositório. Ele não autoriza implementar todas as etapas de uma só vez sem validar suas dependências.

## 20.1 Primeira entrega de código

Fixe o HEAD local e compare com `c321b01f6a4bb83f715cf1bb26f4c18617e4e592`. Leia os arquivos reais da branch e identifique o host usado pelo APK. Atualize o diagnóstico com símbolos atuais. Preserve mudanças do usuário e não faça reset destrutivo para adequar o repositório ao snapshot deste documento.

Comece por M00/M01 e prepare testes de regressão dos scripts e lifecycle. Em seguida, execute a correção M02 de forma isolada: o catálogo válido deve sobreviver às edições e falhas de compilação; as instâncias persistentes devem continuar visíveis; expandir uma não pode ocultar as outras como comportamento padrão. Não reanexe componentes automaticamente para mascarar perda de schema.

A demonstração mínima do PR deve anexar dois scripts, editar um até gerar erro, verificar seus IDs/valores e catálogo, corrigir e compilar de novo, iniciar/parar Play e reabrir o projeto. Declare o que foi testado no host e o que foi confirmado no aparelho. Nenhuma screenshot sozinha substitui essa sequência.

## 20.2 Unidade de trabalho por etapa

Para cada tarefa: descreva a invariante quebrada, mostre a evidência, proponha o contrato e altere somente os consumidores necessários. Acrescente teste que falhe no comportamento anterior e passe na nova implementação. Registre migrações, riscos e rollback. Não invente testes de hardware executados quando não houver aparelho conectado.

Caso uma etapa dependa de uma capacidade ausente, implemente o pré-requisito ou volte ao roadmap; não crie um caminho exclusivo para o modelo de demonstração. O teste de generalidade sempre deve incluir outro objeto/cena com a mesma capacidade. O usuário pode construir um veículo; a engine não deve precisar saber que ele é um veículo para importar nós, girar uma peça ou anexar um script.

## 20.3 Regras de UI durante a execução

Edição de código, nomes e números acontece no controle real. Nenhum popup de texto substituto. O preview gráfico, o console e a criação de recursos podem ter painéis próprios, mas não servem de desculpa para copiar o texto para um formulário externo.

Use a referência visual anterior da Astra como direção, sem copiar código ilustrativo nem espaços inadequados para um telefone com IME. Redesenhe barras por finalidade e use o manifesto. O novo menu long-press será ajustado à referência específica quando recebida, mantendo a arbitragem de gestos e os comandos universais.

Importação começa no projeto. A árvore do asset e da instância deve preservar nós úteis, pais e pivôs. Não aceitar o resultado “visual igual” quando portas/filhos deixarem de ser individualmente editáveis. Uma fonte fundida deve ser descrita honestamente; não inventar separação sem metadados/ação do usuário.

## 20.4 Relatório de fechamento de PR

O relatório contém: commit-base e novo commit; arquivos alterados; invariantes atendidas; testes executados com resultados; testes pendentes; roteiro manual; evidências; impacto de memória/performance quando medido; migração e limitações. Atualize JSONs de status somente com evidência, mantendo os IDs.

Evite conclusões genéricas como “IDE modernizado”, “GLB completo” ou “suporte universal”. Diga qual fluxo o criador consegue realizar e com qual perfil de dados/dispositivo. O projeto deve progredir para ferramentas amplas e confiáveis, não para uma lista crescente de demonstrações especiais.

# 21. Referências e fontes

A versão documental consultada é registrada pelo endereço. `stable` ou uma branch pública pode mudar; pin de bibliotecas, APIs e toolchain deve ser fixado no baseline. Os links do repositório Astra estão fixados no SHA auditado. Planos anteriores e imagem são fontes da conversa e não documentação oficial externa.

**[GH01] Snapshot da branch codex/gameplay-runtime** — Commit inspecionado. Commit de 11/09/2026, 16:36:54 UTC. Resultados de testes descritos pelo autor não foram reexecutados nesta análise.

https://github.com/kacerato/attachsEngine/commit/c321b01f6a4bb83f715cf1bb26f4c18617e4e592

**[GH02] native/editor/editor_code_workspace.cpp** — Código inspecionado. Inspecionados replace, undo, redo, createScript, saveBuffer e applyBuildReport; catálogo scriptTypes é limpo em alterações e em falha de build.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/editor/editor_code_workspace.cpp

**[GH03] android/app/src/main/java/dev/aether/editor/EditorTextInput.java** — Código inspecionado. Entrada por AlertDialog com EditText, polling de 100 ms e confirmação explícita; código e números usam esse caminho.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/android/app/src/main/java/dev/aether/editor/EditorTextInput.java

**[GH04] android/app/src/main/AndroidManifest.xml** — Código inspecionado. Activities em sensorLandscape; appCategory game; configChanges declarados. Não comprova recuperação correta de superfície.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/android/app/src/main/AndroidManifest.xml

**[GH05] native/resources/gltf_import.h** — Código inspecionado. Contrato de GLB estático, limites, recursos omitidos, chaves e estruturas de saída.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/resources/gltf_import.h

**[GH06] native/resources/gltf_import.cpp** — Código inspecionado. Trecho inspecionado aproximadamente 320–580: leitura de primitivas, extensões exigidas e travessia de nós gerando draws com matriz mundial.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/resources/gltf_import.cpp

**[GH07] native/editor/editor_map_scene.cpp** — Código inspecionado. Trecho 1–205: import, adoptPackage, reconcileAssets, buildPickMeshes, localGeometry e pickGeometry.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/editor/editor_map_scene.cpp

**[GH08] native/editor/editor_grid.h** — Código inspecionado. Grade com 258 linhas, espaçamento em décadas e desenho concebido como overlay editorial.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/editor/editor_grid.h

**[GH09] native/editor/editor_screen.cpp** — Código inspecionado. Trechos 1–180, 550–1080: toolbar, componentes, scriptSchema, cards.assign(1,selected), campos e workspace de código.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/editor/editor_screen.cpp

**[GH10] native/editor/editor_document.h** — Código inspecionado. EditorDocument deriva de runtime::SceneGraph; aliases para SceneObject/Transform e componentes tipados. A branch já avançou em relação à main anterior.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/editor/editor_document.h

**[GH11] native/platform/android/dotnet_host.h** — Código inspecionado. Contrato de hospedagem managed, NativeRegion e ciclo de vida. Comentários distinguem ABI de host e VM vendorizada; confirmar binários em execução.

https://github.com/kacerato/attachsEngine/blob/c321b01f6a4bb83f715cf1bb26f4c18617e4e592/native/platform/android/dotnet_host.h

**[AR01] Atlas dos componentes: roadmap P0–P12** — Plano anterior da conversa. Capítulos 8–12; consultado nas seções de arquitetura, dependências e roadmap. Não é documentação oficial da Unity.

Atlas_Componentes_Unity_Astra.md

**[AR02] Atlas gráfico mobile: roadmap G0–G15** — Plano anterior da conversa. Capítulo 26 e contratos adjacentes. Propostas e testes planejados, não benchmarks da engine.

Atlas_Grafico_Mobile_Astra.md

**[IMG01] Conceito anterior de IDE Astra em retrato** — Imagem anterior do usuário. Recuperada da Library e inspecionada. É um conceito gerado; o código desenhado nele não é evidência da API nem do APK atual.

image-gen-1(20260911-023218).png

**[R01] Unity: Console** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.unity3d.com/6000.0/Documentation/Manual/Console.html

**[R02] Unity: Model Import Settings** — Referência primária. Referência de fluxo de importação/modelos. Não implica suporte GLB nativo no núcleo da Unity.

https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html

**[R03] Unity: Prefab instance overrides** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.unity3d.com/6000.5/Documentation/Manual/PrefabInstanceOverrides.html

**[R04] Godot: Script editor** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.godotengine.org/en/stable/tutorials/editor/script_editor.html

**[R05] Godot: Advanced Import Settings** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html

**[R06] Godot: Import configuration** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/import_configuration.html

**[R07] Khronos: glTF 2.0 Specification** — Especificação primária. Seções de nós, transformações, malhas, materiais, animação, skins, extensões e GLB. Nomes não são IDs únicos; GLB também pode referenciar recursos externos.

https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

**[R08] Khronos: glTF Validator** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://github.com/KhronosGroup/glTF-Validator

**[R09] Android: InputConnection** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://developer.android.com/reference/android/view/inputmethod/InputConnection

**[R10] Android: EditorInfo** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://developer.android.com/reference/android/view/inputmethod/EditorInfo

**[R11] Android: Keyboard visibility** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/visibility

**[R12] Android: Configuration changes** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://developer.android.com/guide/topics/resources/runtime-changes

**[R13] Android 16: Behavior changes for targeted apps** — Referência primária. Seção de orientação/resizability/aspect ratio e exceções. A exceção de games usa appCategory; não generalizar política para todos os dispositivos/targets.

https://developer.android.com/about/versions/16/behavior-changes-16

**[R14] Khronos Vulkan: Swapchain recreation** — Referência primária. Consultada em 11/09/2026; referência de comportamento, não prova de implementação da Astra.

https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html

**[R15] cgltf: parser e APIs de acesso** — Referência primária. Candidato a adaptador de parsing, não motor de importação completo. Versão, correções de segurança, licença e fuzzing devem ser verificados antes da adoção.

https://github.com/jkuhlmann/cgltf

**[R16] cgltf: API de accessors e transforms** — Referência primária. Separar parsing, carregamento de buffers, validação, accessors e transformação. Pin de dependência obrigatório.

https://github.com/jkuhlmann/cgltf/blob/master/cgltf.h


## Adendo — aba Jogo e recarga durante execução (17/09/2026)

Solicitação do usuário registrada em [Aba Jogo do viewport e recarga](ABA-JOGO-RECARGA-2026-09-17.md). A mini prévia atual é sob demanda; não equivale a execução contínua ou hot reload. O adendo define a futura aba Cena/Jogo, mundo Play único, RenderView independente, input/foco, atualização segura de propriedades/assets/código, versões, rollback e limites de preservação de estado. É arquitetura futura, não implementação nem ampliação automática do percentual; a sequência P01/P02 permanece ativa.
