# Interface e memória — 19/10/2026

Estado: implementação local em validação; não é conclusão do primeiro bloco nem dos 71 itens.

## Escopo deste incremento

- Sliders para ajustes contínuos de estrutura, relevo, pincel, pintura, LOD e caminhos; valor clicável mantém entrada exata.
- Raio e força antes da grade de ferramentas. Sliders em duas colunas têm escalas independentes; rolagem vertical não altera o valor.
- Resolução por passos válidos e teto dependente da resolução volumétrica. LOD inteiro; erro desabilitado com um nível.
- Compartilhamento de pixels/mips idênticos entre importações e caches completos. Não reduz tamanho, resolução, canais nem mipmaps.
- Publicação compacta de texturas na GPU, remapeando materiais e relatório de streaming. Políticas de streaming distintas preservam residências distintas.
- Orçamento CPU do terreno conta apenas uma vez os blocos efetivamente compartilhados. Histórico, GUIDs, GLBs, cenas e geometria permanecem separados.

## Limites

D09 continua parcial: esta revisão não implementa deltas/checkpoints de geometria nem coleta de arquivos. Texturas parcialmente residentes em arquivo não são fundidas por igualdade de sua cauda; isso poderia compartilhar topos diferentes. Os limites de geometria e memória continuam aplicáveis. O catálogo original de materiais não está integrado ao PBR extensível; M01–M08 permanecem abertos. Nenhum controle foi removido para simular conclusão.

## Referências oficiais

- Unity 6000.0, Raise or Lower Terrain: https://docs.unity.com/en-us/engine/6000.0/manual/creating-environments/script-terrain/terrain-tools/terrain-raise-lower-terrain
- Unity 6000.0, Avoiding asset duplication: https://docs.unity3d.com/6000.0/Documentation/Manual/assets-avoid-duplication.html

A referência fundamenta o fluxo tamanho/intensidade e o princípio de dependências compartilhadas. O algoritmo de interning e a publicação da Astra são próprios; não alegam equivalência com AssetBundles.

## Evidência em andamento

64/64 cenários de terreno e 18/18 de publicação de texturas passaram na revisão final. Incluem sampler, vida útil, reabertura por cache, políticas de streaming, sliders de uma/duas colunas, rolagem e limites de curvas/resolução. O auxiliar da suíte de texturas mantinha um ponteiro para métricas temporárias de fonte; sua vida útil foi corrigida, preservando as asserções. Capturas host: `build/terrain-evidence/sliders-final-ui-1280-1.png`, `sliders-final-ui-655-1.png` e `sliders-final-ui-1280-4-s1.png`. Proposta imagegen é hipótese visual, não implementação. APK 110 em compilação; testes Android pendentes.
