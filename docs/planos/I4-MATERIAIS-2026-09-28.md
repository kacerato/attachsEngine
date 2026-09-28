# I4 — edição de vários materiais · 28/09/2026

Implementação: `bd67ebce`. Continuação do item 87 do inventário, após `df0c1538`. O recorte desta entrega é **MaterialAsset**; mapas HDRI e perfis de ambiente continuam pendentes. A contagem do inventário não muda: o item 87 permanece parcial.

## Comportamento

Em Arquivos, selecionar vários materiais abre o Inspector existente com a quantidade e o material ativo. Cada propriedade divergente mostra `—` e “Valores diferentes”. Um gesto aplica somente a propriedade editada ao conjunto, preservando as outras diferenças. Isso inclui atribuir o valor que o ativo já possui e zerar uma transformação de UV já zerada nele.

O toque longo em um campo misto oferece os valores dos materiais selecionados. Textura, modo de alfa, faces, corte, canais, números e amostragem seguem os dados e consumidores existentes. A diferença de UV é identificada separadamente da diferença da textura. A janela focada continua editando apenas seu próprio material.

## Persistência e histórico

Uma publicação prepara todos os alvos e verifica os arquivos antes de escrever. Os materiais adicionais são companheiros da transação de importação já existente: arquivos e registro compartilham o journal e sua recuperação. A biblioteca de materiais é publicada após o sucesso da gravação.

O histórico registra um único comando de recurso com os estados anteriores e posteriores. Desfazer/Refazer verifica o conjunto antes de publicar. Conflito externo recusa a operação sem modificar os demais alvos. Não se usa `mergeLast`: ele não aceita transações com `resourceReplay`.

## Referência e adaptação

- [Unity 6.0 — Inspector options](https://docs.unity3d.com/6000.0/Documentation/Manual/InspectorOptions.html): valores comuns, indicação de diferenças e edição de várias seleções.
- [UnityCsReference 6000.0 — MaterialProperty](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/MaterialProperty.cs): propriedade com múltiplos alvos e identificação de valor misto.

A Astra mantém seus descritores, biblioteca de materiais, journal e Inspector. No toque, a seleção vem de Arquivos e o gesto longo oferece a cópia do valor de um alvo. Não foi criado outro sistema de material ou um Inspector paralelo.

## Validação e limites

Build host e compilação nativa Android passaram. A suíte nativa passou: **1.237/1.237**.

O teste de integração `multiple_material_assets_edit_one_property_atomically_and_keep_focused_scope` cobre valor igual ao ativo, preservação de outros campos, gravação/reabertura dos arquivos, consumidor de render, Desfazer/Refazer, conflito externo, janela focada, cópia de valor misto e reset de UV.

O seletor de textura existente omite os controles completos de transformação UV quando falta altura (por exemplo, superfície 853 × 394). O cenário de reset usa retrato 400 × 740. Esta entrega não resolve essa limitação de navegação.

APK Debug gerado e instalado no aparelho 25053PC47G. O cenário Android usou `I4Materiais0928`, cópia isolada de `M08Recursos0913k`, com um segundo material de validação registrado. O original foi preservado.

No aparelho, selecionar os dois materiais mostrou `—` em Cor R (1 e 0,35). O toque longo abriu os valores individuais; copiar o valor do ativo atualizou o outro material. Após desfazer, digitar 0,6 gravou ambos os arquivos. Um único Desfazer restaurou 1 e 0,35; Refazer gravou 0,6 nos dois novamente. Fechar e reabrir o aplicativo e selecionar os dois materiais manteve 0,6 no Inspector. Os demais campos permaneceram iguais. A conferência incluiu a UI e a leitura dos arquivos persistidos.

Capturas reais: [valores mistos](../capturas/i4-materiais/valores-mistos.png), [copiar valor](../capturas/i4-materiais/copiar-valor.png), [edição aplicada](../capturas/i4-materiais/valor-aplicado.png) e [Desfazer](../capturas/i4-materiais/desfazer.png). A lateral identifica quantidade e ativo, mantém o viewport visível e oferece campos legíveis em paisagem. São capturas da implementação, sem imagem conceitual. Foram reutilizados os ícones de material e multisseleção existentes.

A janela focada, conflitos externos, publicação no consumidor de render e UV foram cobertos no host; não repetir essa evidência como se todos esses cenários tivessem sido executados no aparelho.
