# Identidade de coleções nos contratos de componentes

Avanço de P01/P03 e da segurança de presets e overrides. Não encerra esses pacotes nem o bloco de prefabs.

`ComponentType.collections` declara nome da coleção, quantidade, identidade persistente por elemento e próxima identidade. Path declara pontos; Animation declara clipes. O contrato reutiliza os IDs e os contadores já serializados: não altera a ABI, o arquivo de cena ou a versão desses componentes.

O comparador de prefabs usa esse contrato no lugar da exceção específica de Path. Diferença de identidade, ordem ou contador é apresentada como alteração do componente completo. A reversão completa continua disponível; Apply seletivo recusa a coleção incompatível antes de alterar a fonte ou as instâncias. Isso também cobre clipes diferentes que usam o mesmo recurso: o recurso não substitui a identidade do elemento.

A aplicação comum de campos de presets recusa atomicamente uma seleção que contenha endereços posicionais incompatíveis. Não publica primeiro os campos simples da seleção. Uma operação separada sobre campo escalar independente continua permitida.

A matriz gerada publica as coleções declaradas. A auditoria recusa descritores de coleção incompletos ou nomes repetidos. Descritores sem coleções mantêm o comportamento existente.

## Reversão de diferenças em lote

A rota de comparação oferece "Reverter N diferenças deste objeto" quando há mais de uma diferença. O escopo é o objeto comparado, não todos os objetos da instância. A operação prepara o candidato completo antes de conferir referências: retirar um componente e retirar a referência local a ele podem ocorrer juntos. Fonte/revisão obsoleta, endereço inválido ou referência restante recusam o lote sem alterar cena ou histórico. Undo/Redo restaura o lote e sua base em um passo. A reversão individual usa a mesma implementação.

## Referência e decisão

[Unity 6000.0, Override prefab instances](https://docs.unity.com/en-us/engine/6000.0/manual/working-with-gameobjects/prefabs/override/prefab-instance-overrides) distingue alterações de propriedades e de estrutura. A Astra mantém essa distinção e usa suas identidades persistentes para impedir que um endereço por posição troque silenciosamente de destinatário.

## Aceite e limites

Os cenários dirigidos verificam reordenação e fronteira de alocação, recusa sem mudança de cena/histórico, reversão completa com Undo e recusa atômica de preset misturando escalar e coleção. Logs de build e execução devem acompanhar a entrega; inclusão de teste no código não prova que passou.

Execução host: `prefab_` 20/20 e `component_` 45/45, incluindo abrir a comparação por toque, reverter o lote e desfazer. Capturas reais do preview em 480×900 e 360×800 foram inspecionadas; a mensagem de recusa foi encurtada para preservar sua legibilidade. A matriz consolidada registra 34 schemas, 33 tipos no Add e 33 fachadas geradas; não houve acréscimo de tipos neste pacote. [Logs e limites da evidência](../validacao/evidencias/collection-contracts-20261001/README.md).

Merge por ElementId, aplicação estrutural de componentes/objetos, edição contextual da fonte, aninhamento e variantes continuam pendentes. Este pacote não cria novo componente ou receita. A ação de lote reutiliza a rota existente de comparação. Não há declaração de validação Android.
