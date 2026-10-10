# N01 — seleção de poses e conjuntos

Estado: implementação local em revisão e validação. Este documento não fecha
N01 nem substitui o aceite visual e de gestos no Android.

## Referências e adaptação

- [Godot 4.5, EditorSelection](https://docs.godotengine.org/en/4.5/classes/class_editorselection.html):
  separa a lista selecionada da edição do Inspector; `get_top_selected_nodes`
  exclui filhos que já recebem a transformação do pai selecionado. Na Astra,
  a seleção guarda IDs de objetos, o alvo principal escolhe o canal e somente
  as raízes selecionadas recebem o delta autoral.
- [Unity 2018.4, atalhos oficiais](https://docs.unity3d.com/2018.4/Documentation/uploads/Main/Unity_HotKeys_Win.pdf):
  conjuntos de seleção podem ser guardados e recuperados. A adaptação usa nomes
  e caminhos relativos ao dono do clipe, com criação por toque e persistência
  por projeto, sem exigir teclado numérico.
- O manual local de seleção do UMotion descreve sincronização entre árvore,
  viewport e Clip Editor, relações de parentesco e foco. Conjuntos salvos são
  uma capacidade da Astra; não são atribuídos ao UMotion.

## Dados, ownership e persistência

`EditorScreenState.clipPoseSelection` contém até 256 IDs transitórios. O principal
deve pertencer à seleção. Quando é descendente de outro objeto selecionado,
uma raiz da seleção assume o papel principal, evitando edição duplicada.
Abrir um clipe inicia sua seleção; fechar libera seleção e isolamento. IDs
de objetos removidos são descartados na reconciliação do editor.

Conjuntos são identificados por GUID do clipe e nome UTF-8 de até 96 bytes.
Cada alvo usa o mesmo caminho canônico de binding, relativo ao dono do clipe:
nomes com `/` e segmentos especiais são codificados, sem heurística por nome.
Não há pointers de componentes persistidos. Um caminho ausente ou ambíguo recusa
a recuperação inteira e preserva a seleção atual.

O arquivo `.astra/animation-selection.astra`, versão 1, contém no máximo 32
conjuntos por clipe, 1024 no projeto e 1 MiB. Parser e publicação são limitados e atômicos. Salvar, substituir
e excluir possuem Undo/Redo de recurso; cada reprodução verifica o projeto,
estado esperado e conteúdo atual do arquivo. Alteração externa exige reabrir o
projeto, em vez de sobrescrever dados desconhecidos. Os conjuntos são preferências
de autoria: não introduzem estado de UI no runtime nem alteram AECLIP 4.

## Edição real do grupo

O canal principal determina propriedade, camada, representação e dimensão.
Tradução, Euler e morph aplicam diferença local; escala aplica razão por eixo;
quaternion aplica a mesma rotação relativa local. Escala inicialmente zero não
admite uma razão que a transforme em valor diferente de zero. Representações
ou dimensões diferentes recusam a operação inteira.

Um canal ausente só é criado quando a política permite; Base e camada usam as
primitivas reais do clipe. Todos os canais entram no mesmo candidato, sampler,
preview e publicação. Uma falha preserva o rascunho anterior; Gravar publica uma
única revisão e uma operação de Undo. Auto-key e intervalo armado permanecem
os mesmos do gesto individual.

Travas são aplicadas a cada canal. Reset identidade e referência da cena usam
o valor próprio de cada raiz selecionada. Referência da cena não é bind pose;
rig/rest pose será tratado por N03. Copiar/colar canal local aplica delta ao grupo.
Colar TRS mundial exige uma única raiz selecionada e recusa grupos explicitamente,
em vez de alterar só parte da seleção.

## Fluxo de toque e estados

NÃO IREI SER SIMPLISTA NO DESIGN.

Ferramentas de pose compartilham uma folha contextual com tarefas Gravação,
Reutilizar e Seleção. Seleção oferece Somar, Limpar, Todos, Pais, Filhos, Irmãos,
Salvar, recuperar conjuntos e Isolar. A árvore substitui a folha para escolher
juntas sobrepostas; Somar alterna membros pelo mesmo backend usado no viewport.
Timeline e canal principal reconciliam a mesma seleção.

Foco usa o centro dos limites das origens selecionadas, preservando a distância
individual e ampliando-a quando necessário para um grupo. Isolamento de seleção
é independente de inspeção da camada isolada: filtra desenhos e picking, mantém
descendentes e malhas cujos `SkinnedMesh.bones` são afetados, e termina ao fechar
o preview. Luzes e ambiente continuam compondo a cena. O ícone
`animation/selection-set` entra no atlas e enum de produção.

## SDK e cenário de aceite

ABI 9 acrescenta seleção de objetos, relações, foco/isolamento e conjuntos
nomeados. As versões 1–8 conservam tamanho e offsets de seus prefixos. O SDK C#
rejeita chamadas indisponíveis em versões anteriores e IDs fora do contrato da
cena; não depende de manter a folha aberta.

O cenário integrado cobre duas juntas e um descendente também selecionado,
recusa atômica de canal ausente, delta repetido sem acumulação duplicada,
publicação agrupada, Undo/Redo, reset por referência individual, foco/skin,
criação de conjunto por toque, controles 853×394 e 655×300, árvore, reabertura,
binding ambíguo, conflito externo e consumo em Play. Resultados e capturas devem
ser registrados no ledger de validação somente depois da execução.

Capturas raster host verificam os controles e não provam imagem PBR/Vulkan,
toque físico, desempenho térmico ou aceitação no aparelho. ADB continua reservado
para o final do plano, conforme decisão do proprietário.
