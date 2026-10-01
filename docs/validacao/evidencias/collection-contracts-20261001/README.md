# Contratos de coleções e reversão de prefabs em lote

Implementação: [contrato e limites](../../../planos/CONTRATOS-COLECOES-2026-10-01.md).

Validação host executada em 01/10/2026:

- Filtro `prefab_`: 20/20. Inclui identidade/ordem/fronteira da coleção, Apply recusado sem mutação, reversão completa com Undo, lote removendo componente e referência em um passo e fluxo real de toque no Inspector.
- Filtro `component_`: 45/45. Inclui recusa atômica do preset misto e os contratos/API gerada. Os filtros se sobrepõem; as contagens não devem ser somadas como testes distintos.
- O primeiro filtro de componentes passou 43/44. O teste de catálogo ainda procurava paginação antiga; foi corrigido para navegar por família e rolagem como o editor atual. O log inicial foi preservado.
- A linkedição Debug inicial falhou com COMDAT inconsistente e depois `file truncated`. A primeira recuperação reconstruiu o objeto do teste de sessão e usou `-Wl,--strip-debug`. Uma linkedição posterior voltou a falhar e a configuração compartilhada havia mudado. A validação final foi deslocada para `build/collection-contracts-host`, com Debug, asserts e `-g0` para C/C++; não foi alterada configuração de produto no repositório.

`batch-portrait.png`, `batch-phone.png` e `collection-portrait.png` vêm do executável de preview, usando recurso prefab, edição de instância e comparação reais. Inspeção em 480×900 e 360×800. A captura é da UI rasterizada no host, não de Vulkan nem de Android. O texto da recusa foi encurtado após inspeção para não cortar a orientação ao autor.

ADB retornou lista vazia nesta execução. Não houve instalação, teste físico nem declaração de aceite Android. A expansão completa do roadmap, Apply estrutural, merge por elemento, edição de fonte, aninhamento e variantes continuam pendentes.
