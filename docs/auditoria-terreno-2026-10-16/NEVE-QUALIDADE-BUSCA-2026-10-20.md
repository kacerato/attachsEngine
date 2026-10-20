# M11–M12 — neve, qualidade autoral e busca do catálogo

Revisão local de20/10/2026, posterior a03b8f10a/48233d11, na branch `codex/terrain-voxel-generator`. Implementação e conferência local aceitas para este incremento; **publicação confirmada, sem APK/ADB**. M11/M12 e o plano integral não são declarados completos: autoridades globais, produtor de poças, tracks, capacidades adjacentes e validação Android real continuam pendentes.

## Implementado e com consumidor

- Neve Off/Simple/Rich com oito papéis de mapas, direção, altura/dot, quantidade, máscara R máximo/G mínimo com prioridade G e sampler Clamp real, participação por camada, normal noise, resampling, stochastic/triplanar, idade/cristais, erosão/clareamento e borda molhada/porosidade. Preset original permanece GA; normal também aceita RGB/RG explicitamente, conservando interpretação/fatores ao trocar fonte. Formatos têm vistas e mips próprios, separados dos escalares de smoothness/AO.
- Rim, glitter/emissão e transmissão fina usam os consumidores reais de iluminação. A adaptação não implementa blur em tela/multiscatter HDRP. Controladores globais importados conservam dados e diagnóstico; falta de autoridade impede publicação, sem toggle funcional fictício.
- Superfície AETERRAINSURFACE3 conserva corpo legado, extensão V2 e acrescenta cauda versionada Snow/residência. Fixtures V1/V2 são saídas de escritores históricos reais. GLB exige domínio explícito nas superfícies V2/V3. Cache derivado revisão11 inclui packing, teto e Clamp; leitores antigos não devem editar o formato novo. Receita permanece AETERRAIN8.
- Original por padrão; teto4096/2048/1024/512/256 por terreno e Inherit/Original/override por mapa. Upload/restream, pedidos manuais/câmera, streaming worker, cache, orçamento e compressão conservam o teto. Orçamento conta a cauda permitida, protege as escolhas dos terrenos contra redução automática e recusa o que não cabe. Arquivo original/dimensões/cadeia completa continuam disponíveis; isso não promete reduzir pico CPU de decode.
- Busca independente por contexto, filtros reais de papel/pacote, índices comuns para desenho/paginação/seleção e recusa de toque obsoleto. Material, PBR, variantes, noise, Geo/Global e Snow usam o fluxo. Normal inicia pelo filtro Normal; a escolha Todos permanece disponível e salva. Miniaturas reais aparecem em todos os seletores.
- UI com sliders e entrada precisa, fontes/Off/ausência de fonte explícitos e ação de ajustar altura ±1m baseada em geometria correspondente ou novo plano comprovável. Defaults fonte50..140m ficam intactos até ação do autor. Quantidade e aviso/ação ficam visíveis no scroll0 de655×300; modos estão no início de Fontes. Papéis têm nomes neutros, e Canal mostra o formato real.

## Assets preservados

Catálogo82 materiais/152 texturas:148 registros anteriores preservados, dois Geo continuam nas posições146/147 e quatro TGA originais Snow/Core entram com PNG RGBA integral lossless e proveniência/hash/GUID. Nenhum original foi reduzido. Contratos completos: [Snow](PROXIMO-M11-M12-2026-10-20.md) e [qualidade/busca](PROXIMO-QUALIDADE-CATALOGO-2026-10-20.md).

Três novos ícones vetoriais de cobertura, luz e residência têm masters384px/atlas96px, IDs482–484. As152 miniaturas de texturas entram no pipeline real. Atlas2048×3236,636 entradas; os481 IDs publicados anteriores permanecem estáveis. O atlas cresceu para conservar resolução. Geradores idênticos preservam bytes e timestamps.

## Evidência local final

- **158/158 cenários integrados do editor**: toque roteado no host, fontes reais, barras/precisão, modos, formato/fatores, aplicar, undo/histórico, reabrir e scene roundtrip. RGB/RG substituem uma fonte real diferente e conservam interpretação ao aplicar/reabrir. V3 sem domínio é recusado por regressão específica.
- **72/72 Core**, com migração V1/V2, roundtrip V3, tabela/descritores, RGB/RG/GA e cobertura CPU. UpVector não unitário normaliza igual à GPU; faixa menor que0.001m conserva divisor mínimo fonte. Puddle não zero permanece Estimate enquanto não houver produtor visual compartilhado.
- **238 modos GPU** na NVIDIA RTX3050 com shader, tabela e serialização reais, incluindo pixels Geo originais, Clamp em UV0/1/fora/mip1 e Snow RGB/RG. Fixture32×32 não prova aparência natural, desempenho ou orçamento no Android.
- **36/36 cenários integrados de publicação de texturas** e **11/11 focados de residência/streaming**: limites manual/câmera/desligado, leitura assíncrona, duplicatas, Original protegido, recusa sem degradação escondida, compressão e preview rebased.
- **130 capturas executáveis base**, depois **28 capturas novas/revisadas** em655×300 e1280×720: oito famílias de pickers normais e seis estados Snow de formatos/fontes/máscara. Visibilidade, canais, controles alcançáveis e footer conferidos. São capturas da UI de produção rasterizada pelo host, sem cena3D ou aparelho.
- **21/21 unidades Android ARM64 sintáticas**, com repetição das **4/4** afetadas pelo último refinamento;44 shaders embutidos validados. Sem link Android/APK/toque ou telemetria de memória/FPS no dispositivo.
- **4/4 testes Python Geo/atlas/miniaturas** e **1/1 assets Snow**: bytes/pixels/dimensões/IDs/no-op/mtime e conflito sem publicação parcial. Fontes integrais e atlas foram conferidos.

Logs/capturas ficam em `build/terrain-evidence/terrain-snow-*`, `terrain-normal-picker-*` e `snow-ui-*` (evidência interna ignorada). A primeira suíte153 teve uma falha no teste: procurava Qualidade dentro de Snow em vez de retornar ao root. O percurso foi corrigido mantendo Apply/reabertura e asserção512. A inspeção visual adicional encontrou a trava GA; ela foi removida, com novas provas Core/GPU/UI. Uma suspeita de domínio V3 foi retirada após reabrir a fonte atual, já correta; a regressão explícita foi acrescentada. Não houve alteração de expectativa de pixel na extensão RGB/RG.

## Limites e próxima integração

Quatro canais pintados continuam o limite visual atual, embora32 camadas sejam autoradas. Globais/poças/tracks/tesselação, vegetação, grafos, integrações altura/voxel e demais linhas abertas continuam no plano. O consumidor Snow CPU só anuncia correspondência às entradas confirmadas; nenhum dado aritmético vira prova de cena real. Naturalidade/iluminação em uma cena grande e desempenho/toque precisam do aceite próprio.

[M13–M15](PROXIMO-M13-M15-2026-10-20.md), [arquitetura](M13-M15-ARQUITETURA-2026-10-20.md) e [UI](M13-M15-INTERFACE-CONTRATO-2026-10-20.md) conservam todo o inventário de wetness, poças, Streams e Lava. Quatro originais foram preparados em staging lossless, com1 teste Python. Fundação FX independente de PBR e modelos SurfaceV4 passaram respectivamente 9/9 e4/4 cenários isolados no host, somente em staging. Recipe9/persistência e integração visual continuam pendentes; esses testes não representam capacidades prontas do editor.

## Referências versionadas

Fonte fornecida MicroSplat Snow3.9.0/Unity2019.4, distribuição Terrain Collection3.9.41. Unity6000.0: [TextureImporter.maxTextureSize](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TextureImporter-maxTextureSize.html), [TextureMipmapLimitSettings](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TextureMipmapLimitSettings.html), [TerrainPaintTool](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintTool_1.html) e [CalculateBrushTransform](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintUtility.CalculateBrushTransform.html). Shader Graph17.0.4: Dot Product, Sample Texture2D, Normal Blend e Fresnel, links integrais no contrato Snow. HDRP17.0.4: [transmissão/espalhamento](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/skin-and-diffusive-surfaces-subsurface-scattering.html). Referências orientam capacidades, não equivalência de visual, BRDF ou desempenho.

## Publicação confirmada

Engine `a3d27687` publicada na branch de terreno; AstraDocs `4b5264a9` publicada em main. Vercel READY (`dpl_Dmy7pPfq4MNXuKDXuVNturMkGE3n`), alias `astraengine.com.br`, guia HTML/Markdown, feed e Atualizações com HTTP200 e conteúdo novo conferidos. A nota de iluminação recebida de outra sessão foi preservada. Build final:2440 páginas HTML,2439 Markdown,595629 links e zero erros. Revisão executável do site em desktop1265px e mobile390px sem overflow. Sem APK/ADB.
