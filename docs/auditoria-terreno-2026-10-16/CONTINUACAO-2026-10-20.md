# Continuação dos plugins — 20/10/2026

Etapa posterior: [pintura independente na GPU](PINTURA-GPU-2026-10-20.md). Este registro conserva o estágio de contratos/mapas anterior à integração do consumidor por fragmento.

Universo de trabalho: os 71 itens de `plano-implementacao.csv`, com todas as propriedades comprometidas. Esta revisão permanece na branch `codex/terrain-voxel-generator`. ADB suspenso pelo proprietário até concluir a implementação e a conferência local; nenhuma validação Android desta revisão pode ser inferida do APK 110.

## P11 — mapa de pintura independente

`TerrainRecipe.controlCells`: zero conserva a avaliação legada; 8–512, em passos de oito, gera um campo de pesos separado da triangulação. A receita original é reexecutada ao mudar a resolução, preservando traços e caminhos em vez de reamostrar destrutivamente o mapa anterior. A receita AETERRAIN7 persiste esse parâmetro; leitores das versões 1–6 continuam disponíveis.

O editor expõe uma barra e entrada precisa em Forma → Detalhes. O mapa é amostrado nos vértices pelo mesher atual. **O shader ainda interpola pesos de vértice; pintura fina entre vértices exige mapas amostrados por fragmento.** P11 permanece parcial, incluindo resolução independente de detalhe/vegetação e consumo final na GPU. Volume conserva pintura 3D de paredes e teto.

Chunks sem influência de texels alterados podem ser reutilizados. A influência bilinear inclui um halo convertido da resolução do mapa para a malha. Os pincéis são organizados em blocos espaciais de 16×16 texels sem alterar a ordem de aplicação; o cancelamento é consultado durante as linhas. Resolução máxima não é reduzida automaticamente. A referência geométrica do material é reexecutada em até 128 células; o mapa de controle pode ter até 513×513 amostras.

Referência: [Unity 6000.0 TerrainData](https://docs.unity.com/en-us/engine/6000.0/script-reference/unityengine/terraindata), que distingue heightmapResolution, alphamapResolution e detailResolution.

## D09 — checkpoints e deltas da receita

O armazenamento `.astra/terrain-history/objects` guarda checkpoints e deltas binários exatos por SHA-256, com limite de profundidade e retorno periódico a checkpoint completo. Referências por hash completo do GLB ficam em `revisions`. Gravações usam o publicador atômico com sincronização de arquivo existente na engine. Cada revisão verifica integridade, limites, bases da cadeia e caminhos antes do uso.

A publicação conserva uma cópia autoral antes de modificar a cena. Ao reabrir um GLB ausente, truncado ou com hash divergente, o editor pode carregar a receita íntegra do histórico e oferece reconstrução explícita ao aplicar. A cópia autoral não substitui silenciosamente a cena por outra revisão. Um caminho registrado com hash diferente não pode ser sobrescrito pela reconstrução.

O coletor isolado recebe raízes vivas e protegidas e preserva as bases transitivas. Ele **não é executado automaticamente pelo editor**, não apaga GLBs e ainda precisa da integração com referências de todas as cenas e Undo/Redo. Os deltas atuais são da receita; deltas de geometria e GC completo continuam pendentes. Portanto D09 permanece parcial.

Referência: [Unity 6000.0 Undo.RegisterCompleteObjectUndo](https://docs.unity.com/en-us/engine/6000.0/script-reference/unityeditor/undo/registercompleteobjectundo). A Astra utiliza seu próprio formato de armazenamento e recuperação.

## P10 — backend de vizinhança

Novo módulo valida membros e grupos estáveis, coordenadas mundiais e arestas completas de heightfields. Rejeita sobreposição, resolução incompatível, IDs duplicados, pesos inválidos e ambiguidades. A reconciliação usa identidade de camada, nunca coincidência do índice, e resolve cantos de quatro terrenos numa única média em altura mundial. Metadados têm formato versionado e limites de entrada.

Este backend ainda não constitui a função completa do editor: faltam integração autoral de grupos, publicação conjunta, persistência das restrições de borda no terreno e correspondência com o renderer extensível. Vizinhança voxel não é suportada pelo módulo. P10 permanece parcial.

Referências: [Unity 6000.0 Terrain.SetNeighbors](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Terrain.SetNeighbors.html) e [TerrainLayer](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainLayer.html).

## M01–M08 — contratos e catálogo original

O contrato `AETERRAINSURFACE1` conserva camadas ordenadas/ativação/identidade, sete canais PBR, inversão e remapeamentos escalares/RGBA, transformações, tint/saturação, triplanar, height blend, ruídos globais/por camada, três normal noises, array anti-tile, resampling e clusters de duas/três variantes distintas. Possui resolução explícita de referências, limites e rejeição atômica de dados inválidos.

O adaptador lê o catálogo efetivo de 82 materiais/146 texturas. Cada textura conserva o GUID de importação, hashes de origem/runtime/pacote, caminhos e dimensões. Materiais recebem IDs determinísticos a partir da identidade do catálogo. A seleção preserva a ordem escolhida e devolve os metadados das texturas requeridas sem copiar, diminuir ou recomprimir pixels. Canais, espaços de cor, smoothness e remapeamentos vêm do contrato resolvido; nomes de arquivo não decidem sua interpretação.

As propriedades JSON originais, inclusive campos ainda não suportados, sobrevivem à seleção, serialização e reabertura do material. Referências ausentes e papéis ainda não consumidos produzem diagnóstico. Não se presume ativação ou escala de ruído apenas por existir uma imagem. Nenhum tipo novo foi registrado no menu do editor por existir somente um contrato.

**Pendente:** importação real dessas texturas no projeto, empacotamento/upload, controle por fragmento, consumo PBR/ruídos/clusters no shader e a UI completa de camadas. O material renderizado ainda usa quatro albedos legados. Modelos e parsers não concluem M01–M08.

Referências: [TerrainLayer, Unity 6000.0](https://docs.unity.com/en-us/engine/6000.0/script-reference/unityengine/terrainlayer) e [Sample Texture 2D Array, Shader Graph 17.0.4](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Sample-Texture-2D-Array-Node.html). A distribuição fornecida chama-se MicroSplat 3.9.41; o package.json interno informa 3.9.0, distinção preservada na auditoria.

## Verificação local

- **90/90 cenários integrados passaram**, log `build/terrain-evidence/terrain-20261020-complete-check.log`. Incluem publicação/reabertura/Undo, slider e entrada precisa, recuperação e reconstrução exata de GLB, mapas e chunks regionais, casos de vizinhos e todos os 82 materiais do catálogo.
- A execução isolada anterior passou em 48/48 casos; a suíte integrada acima inclui também os testes de proveniência acrescentados depois. Não se somam as contagens como testes únicos.
- **10/10 unidades** passaram na compilação sintática Android ARM64 com as opções do target atual; log final `android-syntax-20261020-final.log`. Isso não prova ligação do APK, instalação ou runtime Android.
- Dois cenários condicionais de symlink registraram indisponibilidade do privilégio no Windows e não executaram suas asserções específicas. Os cenários de corrupção, hash, caminho relativo e interrupção executaram normalmente.
- Capturas da UI de produção, por executável host/rasterização: `maps-ui-1280.png`, `maps-ui-655-controls.png`, `maps-ui-655-detail.png`. Conferidos legibilidade, alvos, barras, rolagem e ação fixa em 1280×720 e 655×300. Essa captura isolada de painel não é evidência do shader nem de uma cena Android.
- A auditoria de aceite recebeu uma coluna datada; todas as propriedades comprometidas e os 71 itens permanecem no plano. Nenhum item integral dos plugins é considerado concluído por existir somente modelo, parser ou teste de backend.

Código funcional publicado: `23fe8dde`, branch `codex/terrain-voxel-generator`. Sem novo APK nesta etapa; ADB permanece suspenso.

## Publicação das notas

AstraDocs `f35821b` publicado na main do site, preservando a nota de nuvens `d7a9596` publicada por outra sessão. Nova rota `/pt-br/desenvolvimento/2026-10-20/terreno-mapas-recuperacao/` e nota `2026-10-20-terreno-mapas-recuperacao-catalogo`. Build: 2435 páginas; exportação: 2434 Markdown, JSON e dez exemplos. Verificação de 594461 links sem erros. Conferência visual desktop/mobile realizada; viewport temporário restaurado e servidor local encerrado.

Vercel `dpl_79SCY9gX8BAdmHpFe8fq5tYTRdvn` READY em produção. Domínio `astraengine.com.br`: HTTP 200 na aba Atualizações, no guia e no Markdown; nota, contagem 90/90, AETERRAIN7 e escopo de 71 itens confirmados no conteúdo servido. A meta completa dos plugins permanece ativa; este checkpoint não conclui o bloco 1 nem autoriza afirmar paridade visual/runtime.
