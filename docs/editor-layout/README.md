# Layout aprovado — editor Astra

**Direção revisada (24/09/2026):** [política de abas, listas, popups e doca de diagnósticos](../planos/ampliacao-2026-09-23/UI.md). A ampliação usa o visual Astra atual. A prancha conceitual de 23/09 foi rejeitada como direção visual; consulte a seção inicial do plano para distinguir referência histórica e implementação.

O usuário escolheu em 09/09/2026:

- Cena e Arquivos à esquerda, viewport central e Inspector à direita.
- Painéis redimensionáveis e recolhíveis.
- Densidade de informação da proposta, sem aumentar globalmente os controles.
- Cinza escuro, traços claros e verde Astra nos estados ativos.
- Iconografia própria, sem reutilizar os ícones Godot.

`proposta-aprovada.png` é uma imagem conceitual gerada. Não é uma captura do aplicativo e não comprova recursos, iluminação, água, logs ou propriedades implementados. As dimensões finais e áreas de toque precisam ser verificadas no aparelho.

## Implementado nesta revisão

`EditorSession::dispatch` recebe comandos com versão da cena e IDs, preserva o documento Aether e usa seu histórico. Suporta seleção, enquadramento, renomeação, transformação, propriedade numérica, duplicação, remoção, reparentamento, desfazer e refazer. Recusa versões antigas, IDs ausentes, dados inválidos e alterações durante Play/gestos. Os botões existentes de histórico, duplicar e remover usam essa entrada.

O contrato é C++ e exige a thread da sessão. Ainda não é um transporte JNI/GDExtension e não possui uma fila entre threads. O chamador não pode chamá-lo diretamente da thread de interface Android.

Os 23 ícones originais estão em `assets/astra-visual/icons/hd-v1`: PNG transparente 512×512 e SVG editável. `tools/generate-editor-hd-icons.py` reproduz os arquivos e registra os mesmos IDs no catálogo. `tools/pack-icon-atlas.py` respeita os ícones já preparados para fundo escuro. O atlas continua com células de 96 px, evitando ampliar a memória residente pela resolução dos arquivos fonte.

## Ainda pendente

Não existe equivalência funcional completa com Godot nem adaptação de seus painéis/viewport nesta revisão. Faltam o host visual, o transporte de comandos, o adaptador de nós/recursos/Inspector, a integração do Sistema de Arquivos e os testes completos de edição no aparelho. A aparência aprovada não deve mascarar essas pendências.

Referência técnica fixada: Godot 4.7.2, fontes `editor/docks/scene_tree_dock.*`, `editor/docks/filesystem_dock.*`, `editor/inspector/editor_inspector.*` e `editor/scene/3d/node_3d_editor_plugin.*`. O Aether permanece proprietário da cena, do runtime e do renderer.

## Validação do incremento

- 723/723 testes nativos passaram após a última alteração (build/editor-command-tests.log).
- Debug e Release Android compilados após a última alteração (build/editor-bridge-android-build.log).
- 23 PNGs verificados como RGBA 512×512 com transparência.
- Sem validação desta revisão no aparelho e sem nova instalação. A imagem aprovada continua sendo conceitual; a interface integrada Godot/Aether não está entregue.

## Sistema de Arquivos — incremento seguinte

Implementado no editor Aether: listagem da pasta real do projeto, pastas antes de arquivos, navegação, voltar à raiz, atualização explícita, paginação e painel inferior esquerdo redimensionável/recolhível. Os testes também exercitam os toques nos itens e no recolhimento.

Arquivos `.aescene` podem solicitar abertura pelo caminho validado. O Android salva alterações da cena anterior antes da troca, valida a cena contra o pacote atual e preserva o documento em caso de falha. Reabrir a cena atual é uma operação sem efeito, preservando seu histórico.

A navegação usa caminhos canônicos e recusa saída do projeto. Links simbólicos não são listados. Há um limite explícito de 4096 entradas por pasta; ultrapassá-lo apresenta erro e preserva a listagem anterior. A leitura é síncrona e ocorre apenas ao navegar/atualizar, nunca por frame. Arquivos de origem ainda não são importados por este painel; renomear/mover com atualização de referências ainda está pendente.

Este painel é uma implementação nativa Aether conectada ao seu runtime; não é a adaptação completa do `FileSystemDock` Godot nem equivale a todas as suas funções. O host dos painéis Godot, recursos, Inspector e viewport adaptados continuam pendentes.

## Mudança de direção aprovada: controles originais primeiro

A implementação posterior incorpora a biblioteca do editor original no mesmo APK, mantendo o shell Astra. Cena, seletor pelo +, Inspector e Sistema de Arquivos passam a ser os controles Godot. Consulte `integrations/godot/README.md` para o limite desta etapa: a ligação ao runtime/viewport Aether permanece pendente por decisão explícita do usuário.

O editor nativo anterior também recebeu a remoção da linha Expandir/Recolher/Limpar, navegação por árvore e rolagem em Arquivos e escala própria menor. Ele não é o editor aberto pelo shell nesta revisão. Seus testes passaram 724/724; isso não valida a integração Godot/Aether.
