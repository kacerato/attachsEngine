# P02 — presets de componentes do projeto

## Entrega

Percurso persistente ligado ao menu do componente e ao Add. Salvar atual solicita nome pelo teclado; a biblioteca lista os presets do mesmo tipo quando aberta pelo componente, ou todos quando aberta pelo Add. Selecionar prepara a prévia sem alterar a cena. Aplicar valores conserva a instância; Adicionar cria outra instância e resolve requisitos transitivos em uma única operação de histórico. Renomear e excluir são operações de biblioteca; exclusão exige segundo toque.

O contrato usa os descritores registrados da Astra, não tabelas de campos de outra engine. Atualmente abrange os oito tipos nativos de câmera, visual e física registrados no schema. Comportamento C# é explicitamente recusado porque referências serializadas de campos de script ainda precisam de um contrato portátil próprio. Tipos indisponíveis carregados do arquivo permanecem preservados, mas não executáveis.

## Integridade

- Arquivo: `.astra/component-presets.astra`, formato `ASTRA_COMPONENT_PRESETS_1`, versão do tipo e payload individual. Não modifica o formato das cenas.
- Identidades de preset separadas das instâncias dos objetos. Nome limitado a 96 bytes, 256 entradas, payload individual de até 1 MiB e biblioteca até 8 MiB.
- Publicação usa substituição atômica existente. Compara os bytes originais antes de gravar e recusa conflito externo. O contrato continua sendo um único escritor do editor; não é controle multiprocesso com lock.
- Parsing completo antes de publicar em memória; entradas desconhecidas preservadas. Instanciação usa o leitor/migração do componente e recusa versão incompatível.
- Captura zera referências de objetos da cena. Aplicar valores preserva as referências do destino. Adicionar começa com referências vazias, que podem exigir configuração antes do Play, como na Junta.
- Malhas capturam GUID persistente e descartam índice efêmero. Aplicação resolve índice atual e confere malhas/materiais/texturas usados. Recusa recurso ausente ou textura indecodificável antes de alterar autoria.
- Aplicação verifica epoch/revisão da cena, tipo e validade; recusa durante Play ou transação aberta. Dependências e valores são preparados em cópia e publicados por `EditorHistory::applyValues`, com Undo/Redo da cena.

## Prévia e interface

Campos numéricos, booleanos e enumerações apresentam quantidade de diferenças e até três exemplos. A prévia de malha informa substituição integral de geometria/materiais/slots; ainda não é diff por slot. Ao adicionar, mostra componentes que serão criados ou o conflito que impede a operação. Painel paginado no inspetor, quatro entradas por página. Não há popup de informações técnicas.

Nome usa a barra de texto da plataforma e teclado Android; fallback de teclado interno também integrado. A conferência inicial no aparelho encontrou ausência da barra de texto: integração corrigida em `platformFieldActive` e título específico incluído.

## Evidência e limites

Build host e Android Debug aprovados. P02 14/14 e composição 6/6 antes do ajuste da barra de nome; caso completo de presets reaprovado após o ajuste. Caso de presets cobre UI de salvar, reabertura do arquivo, ausência de IDs de cena no payload, prévia não mutante, aplicação, preservação de referência, Undo, nova instância, epoch/revisão obsoleta, Olhar+Câmera adicionados e desfeitos juntos, conflito externo, rename e exclusão preservando outro registro.

Ainda pendentes: favoritos/busca/filtros avançados, presets compostos autorais de vários tipos, aplicação seletiva por campo, migração de referências C#, importação/exportação entre projetos com remapeamento de recursos, diff completo de slots, Undo da biblioteca de presets, teste com biblioteca desconhecida/corrompida e interrupção física durante gravação. A biblioteca local não é recurso AssetRegistry: não anuncia dependências reversas ou movimentação universal de arquivo. Publicação GPU segue posterior à autoria, com mensagem de pendência se falhar.

O pacote P02 não é declarado completo. As condições de física entregues nas continuações anteriores e este percurso de presets serão considerados juntos na reestimativa, sem contabilizar novamente P01/P08.

### Conferência final no aparelho

APK final instalado em 25053PC47G. Projeto P01Camera0915i: Malha de EsferaDraco → Presets → Salvar atual → nome EsferaDraco_Preset com barra visível acima do teclado. Biblioteca sobreviveu à reinstalação/reabertura. No Cubo, seleção do preset manteve a geometria durante a prévia; Aplicar valores mudou de cubo para esfera azul. Um Undo restaurou o cubo e a cena foi salva restaurada. O preset de demonstração permanece na biblioteca desse projeto.

A conferência encontrou e corrigiu dois defeitos antes do fechamento: barra de nome ausente e conversão incorreta de índice da malha (o slot MeshRenderer é 1-based; assetGuid recebe índice 0-based). O teste de presets passou a verificar explicitamente GUID e slot 1, incluindo captura de malha legada sem GUID. P02 final 14/14. Evidências em docs/validacao/evidencias/p01-p04-20260915: presets-keyboard.png, presets-preview.png, presets-applied.png, presets-undo.png. Renomear/excluir, proteção de referência e adição Olhar+Câmera possuem evidência de host; não foram todos repetidos fisicamente. Sem commit/push nesta entrega.
