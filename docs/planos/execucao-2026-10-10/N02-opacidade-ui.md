# N02 — Opacidade base da interface

Estado em 10/10/2026: cadeia de implementação presente, sintaxe com Werror
aprovada. Inspector executável em 655×300: toque altera 1 para 0,51,
salvar/reabrir e Undo/Redo aprovados, com captura posterior inspecionada.
Rodada nativa de Play e publicação desta novidade ainda pendentes.
Esta dependência não representa fechamento do N02.

## Objetivo e referências

Animar opacidade requer um valor base que funcione mesmo sem uma transição
ativa. Alterar somente a pose de uma animação desativada não atende ao contrato.

A [Godot 4.5 CanvasItem](https://docs.godotengine.org/en/4.5/classes/class_canvasitem.html)
separa modulação herdada e modulação somente do elemento. A
[Unity 6000.0 CanvasGroup](https://docs.unity.com/ja-jp/engine/6000.0/script-reference/unityengine/canvasgroup)
separa alpha, interação e raycasts. Extraímos o princípio de composição visual
herdada, mantendo as regras de entrada já existentes da Astra.

## Modelo, composição e autoridade

`GuiNode.opacity` é um número finito em `[0,1]`, padrão `1`. A opacidade final
é o produto da opacidade final do pai, do valor base, da pose de movimento e
da pose do estado visual. Cores, alfa das imagens e tintas continuam compondo
por seus contratos próprios.

Opacidade final zero elimina o desenho e impede novos toques, preservando o
contrato anterior de hierarquias apagadas por movimento/transição. `visible` e
`enabled` continuam controlando seus estados. Uma região invisível interativa
usa o fluxo já existente de controle sem fundo, com opacidade base positiva.

O documento de autoria é independente da cópia de Play. Cada lease de
`SceneGui` mantém sua própria cópia; o roteamento existente do SDK também
encaminha os novos comandos para a instância selecionada.

## Persistência, edição e desempenho

AEUI 6 acrescenta a opacidade ao fim de cada registro de nó. O leitor aceita
AEUI 1–5 com opacidade `1`; valores fora do domínio e documentos truncados são
recusados antes de substituir o documento anterior. Duplicação, snapshots e
Undo/Redo preservam o valor pelo mesmo caminho dos demais dados autorais.

O editor abre Aparência primeiro e mantém uma seção de propriedades aberta por
vez. **Opacidade / grupo** usa slider numérico e explicação da herança. Cabeçalhos
e campos deste Inspector têm altura mínima de 44 px. Conteúdo, interação,
movimento e layout permanecem acessíveis nas outras seções, com rolagem.
O ícone `ui/opacity` é produzido no pipeline
vetorial existente e integrado ao atlas real, sem depender de uma imagem de
conceito.

A captura inicial mostrou que as linhas permanentes de dono, recurso e ações
escondiam o slider em 300 px de altura. O Inspector contextual agora usa uma
faixa de 44 px: voltar, elemento selecionado, salvar, criar e ações. Tocar no
nome abre contexto da fonte, caminho, alteração pendente e quantidade de Canvas
que compartilham o recurso. Informações permanecem acessíveis sem ocupar metade
da tela durante cada edição. Menus de criação e ações também usam linhas de
44 px. A UI independente preserva sua navegação existente.

`N02-opacidade-conceito.png` é uma hipótese gerada a partir da captura inicial.
O seletor de recurso “Fundo / Nenhum” sugerido pela imagem não foi implementado:
Fundo continua usando o editor de cor real. Os controles executáveis seguem a
paleta do produto, incluindo o acento azul existente e o ícone angular novo.

Evidência: `host/gui-opacity-touch.log`, `host/capturas/gui-opacity-after.png` e
`host/capturas/gui-opacity-authored.aeui` sob a validação N01/N07. O utilitário
`tools/validation/gui-opacity-inspector.cpp` instancia o GuiWorkbench real,
encaminha ponteiro e captura instâncias de desenho. É raster host; não comprova
GPU Vulkan nem aparelho.

`GuiDocument::setOpacity` valida o escalar e encontra o nó pelo índice estável.
`GuiRuntime::setOpacity` preserva o layout medido quando a única mudança é a
opacidade, depois recompõe a apresentação. Uma alteração geométrica pendente
impede esse atalho. Não há cópia do nó, validação de toda a árvore ou reflow de
irmãos para cada escrita de opacidade.

## API

`GuiElement.Opacity` lê e escreve o valor base. Comandos 15/16 são acrescentados
à ABI existente; o payload escalar da resposta transporta a opacidade apenas
nesses comandos. Snapshots comuns continuam retornando o valor do controle.
Nenhum tamanho ou deslocamento de estrutura ABI mudou.

Na API nativa, valores inválidos são recusados atomicamente. Na API C#, NaN,
infinito e valores fora de `[0,1]` geram `ArgumentOutOfRangeException` antes da
chamada. Handles expirados continuam seguindo o diagnóstico do mundo/lease.

## Cenário de aceite

Criar painel e botão filho → definir opacidades base → combinar com movimento
e estado visual → salvar/reabrir → comparar opacidade resultante → Undo/Redo →
alterar pelo SDK em Play → verificar isolamento do recurso → definir zero e
verificar recusa de novo toque → restaurar opacidade e clicar → recusar domínio
inválido → migrar AEUI 5.

O cenário protege composição, persistência, autoridade de entrada e lifecycle;
não substitui a futura trilha animada, seu preview restaurável, remap de prefab
ou a inspeção visual no executável e no aparelho. ADB permanece reservado ao
fim do plano completo, conforme solicitado.
