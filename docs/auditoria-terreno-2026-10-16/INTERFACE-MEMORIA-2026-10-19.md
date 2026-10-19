# Interface e memória — 19/10/2026

Estado: código publicado e APK 110 testado no POCO F7 nos cenários abaixo; não é conclusão do primeiro bloco nem dos 71 itens.

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

64/64 cenários de terreno e 18/18 de publicação de texturas passaram na revisão final. Incluem sampler, vida útil, reabertura por cache, políticas de streaming, sliders de uma/duas colunas, rolagem e limites de curvas/resolução. O auxiliar da suíte de texturas mantinha um ponteiro para métricas temporárias de fonte; sua vida útil foi corrigida, preservando as asserções. Capturas host: `build/terrain-evidence/sliders-final-ui-1280-1.png`, `sliders-final-ui-655-1.png` e `sliders-final-ui-1280-4-s1.png`. Proposta imagegen é hipótese visual, não implementação. A validação posterior do APK 110 está registrada abaixo.

## APK 110 — validação Android

Código funcional `9fc2f719`, publicado na branch. APK `0.3.1-dev.terrain.20261019.5`, code 110, SHA256 `7c03d93805560e0c34e6147d2a4a58adccc13ef955b261d9ee087039157cc654`, instalado por atualização no POCO F7. Auditorias de assinatura, identidade, ZIP/ARM64, atlas e igualdade do binário passaram. Limitação ELF16K anterior permanece.

- Mesmo projeto `TerrenoQA_1510`, 795 operações: biblioteca contabilizada 127,27 MiB no 109 e 43,25 MiB no 110 (84,02 MiB / 66,0% menos). Capturas `device109-history-budget.png` e `device110-history-budget.png`; reedição aberta sem modificar o terreno. Não confundir orçamento da biblioteca com memória total do processo ou desempenho de todos os projetos.
- Novo `TerrainQA110`: terreno plano sólido criado com 96 células pelo slider (partiu de 64). Raio/força visíveis antes das ferramentas. Intensidade ajustada por arraste e entrada exata 0,5; rolagem vertical não deslocou o valor horizontal.
- Prévia capturada durante DOWN/MOVE: `device110-live-sculpt.png`; UP concluiu nove amostras. Undo voltou a zero e Redo restaurou nove. Gizmo de objeto ausente durante escultura.
- LOD por duas colunas: 1→3 habilitou erro; erro ajustado a 1,9891%. Aplicação, salvar, reiniciar processo e reabrir preservaram nove operações e LOD/erro (`device110-reopened-lod.png`). Isso não certifica visualmente todas as transições por distância.
- Logs `device110-history.log`, `device110-qa.log`, `device110-reopen.log`: sem FATAL EXCEPTION/Fatal signal no roteiro. Há mensagens do framework/vendor, não se afirma log inteiramente limpo.
- `TerrainQA108` do proprietário permaneceu sem gestos novos. Capturas/evidências são locais, não entram nas docs públicas.

O primeiro bloco e os 71 itens continuam abertos. Este incremento não conclui materiais extensíveis, checkpoints/deltas/GC, vegetação, grafos nem todos os testes de voxel/caminhos no aparelho.

## Publicação das docs

AstraDocs `36bd0b7` publicado na main do site, com nota nova `2026-10-19-terreno-sliders-texturas-compartilhadas` e guia da revisão. Build: 2434 páginas; exportação: 2433 Markdown/JSON e dez exemplos; verificação de 594226 links sem erros. Revisão visual local desktop/mobile realizada. Vercel `dpl_6GFgCmEFqkVAGjWCQDB1zmKdP1zs` READY em produção; domínio `astraengine.com.br` confirmado por HTTP 200, nota/code 110 e seções de sliders/compartilhamento presentes. APK público permanece inalterado; 110 é teste privado. Código da engine continua exclusivamente na branch `codex/terrain-voxel-generator`.
