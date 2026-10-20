# M05–M08: ruído, anti-tile, resampling e variantes

Este registro sucede o checkpoint PBR `ebd31aec`. Não encerra o plano de 71 itens, nem anuncia APK. ADB permanece suspenso pelo proprietário até a conferência local. As propriedades dos plugins continuam no contrato original; não houve redução dos assets.

## Comportamento

- Detail noise modula a cor perto da câmera e desaparece entre fadeStart/end; distance noise aparece nessa faixa. Configurações globais e locais são independentes. Cada camada decide sua participação nos ruídos globais.
- Frequência é repetições por metro, não tamanho de tile. Projeção UV/local/mundial, textura original, canal R/G/B/A/RGB, espaço linear/sRGB, fator, inversão e remapeamentos têm consumidores. O valor 0,5 é neutro. A curva é smoothstep e a distância usa a câmera real no mundo.
- Três fontes de normal globais e três locais usam canais RGB/RG, reconstrução de Z, força, frequência, fade, projeção e participação independentes. A mistura reorientada usa uma base geométrica comum e trata normais opostas sem divisão singular.
- Anti-tile conserva três fontes por entrada: normal, detalhe e distância. A camada referencia uma identidade estável, resolvida na CPU; reordenar entradas não troca a seleção. Desativar o array conserva a autoria.
- Pintar → Ruído reúne seleção do contexto, fontes reais do catálogo e barras deslizantes com entrada precisa. O limite da barra de fade mantém fim estritamente maior que início. O ícone TerrainNoise existente mantém a identidade visual.

## Transporte e arquitetura

O contrato base continua com dois cabeçalhos e 40 linhas por camada. O terceiro campo do cabeçalho aponta para um bloco avançado opcional versionado. M05 usa blocos de 37 linhas globais e por camada, reservando três registros de normal. M06 preenche esses registros e acrescenta as participações e as referências de anti-tile. A publicação valida resolução de texturas, papéis de cor/dados, referências, versões e orçamento de 4 MiB antes da mutação.

As tabelas independentes são deduplicadas em forma canônica. Somente a cópia inserida é relocada para a tabela final, incluindo todos os ponteiros internos. Compartilhamento não mistura parâmetros entre terrenos.

O contrato AETERRAIN8/superfície GLB v3/cache9 já conserva esses dados e imagens. Editar parâmetros sem trocar referências reutiliza os pixels imutáveis. Não há redução de resolução implícita. O atlas passa a conservar arquivos e timestamps quando o resultado gerado é idêntico, evitando recompilações sem mudança.

## Referências e limites

Unity Shader Graph **17.0.4**: [Triplanar](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Triplanar-Node.html), [Normal Blend](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Blend-Node.html) e [Normal Strength](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Strength-Node.html). Os fontes fornecidos MicroSplat internos 3.9.0/Unity2019.4 orientam a capacidade. Os fades, unidades e remapeamentos são escolhas explícitas da Astra; não é paridade numérica de presets.

Base PBR pode usar 21 amostras por camada em triplanar; M05 soma até 12 e M06 até 27: 60 por camada, 240 para quatro camadas no pior caso. UV usa uma projeção. Efeitos desligados, força ou participação zero evitam amostragem; fade espacial mantém derivadas corretas. Timestamps do harness offscreen medem somente essa cena pequena na GPU do PC, sem extrapolação para Android.

Controle de pintura continua com quatro canais, embora a autoria conserve até 32 camadas. M09 e módulos seguintes continuam pendentes.

## M07–M08 integrados

Resampling separa albedo/normal/altura, multiplicador de frequência distante, início/fim e modos Constant/Noise/Crossfade. Ruído modifica o interior da faixa, preservando extremos. A opção de altura normaliza as contribuições próximas/distantes; multiplicador 1 preserva o resultado base. Fontes antigas de altura com ruído desligado mantêm Crossfade equivalente.

Clusters usam duas/três variantes reais com sete mapas, tamanho absoluto do tile, deslocamento e rotação. Com cluster ligado, tamanho vem da variante e escala X/Y da camada fica inativa/preservada; offset e rotação da camada continuam somando. Catálogo já contém 18 materiais com referências completas de clusters; um cluster original foi aplicado e reimportado no teste integrado, conservando seus canais.

O plano de contribuições é compartilhado entre todos os canais e o resampling. Gradientes e bases são calculados antes das escolhas por célula/variante; nenhuma derivada implícita ocorre dentro das ramificações por contribuição. O modo estocástico tem seleção espacial determinística. Qualidade Baixa usa uma contribuição e variante fixa com warp no estocástico; Média usa duas vizinhas contínuas em uma distribuição escalar; Alta usa três vértices triangulares compartilhados. No modo sem estocástico, Baixa admite fronteiras duras, Média mistura vizinhas e Alta usa todas as variantes. Não há pré-processamento de histogramas nem alegação de equivalência ao protótipo UnityLabs.

A referência histórica adicional é https://github.com/UnityLabs/procedural-stochastic-texturing e seu SampleProceduralTexture2DNode.cs, com captura de gradientes antes da seleção. Não confundir pesquisa oficial com funcionalidade pronta de Shader Graph17.

Com três contribuições, triplanar, sete canais, resampling e todos os ruídos, o teto teórico pode atingir 540 amostras para quatro camadas; a filtragem de altura Alta pode elevar a 756. UV, papéis ausentes/desligados e orçamentos explícitos de contribuições reduzem trabalho. Não extrapolar timings de um alvo32×32 da RTX3050 para Android.

## UI e assets

Global/Local, papéis e links relacionados ocupam132px; a faixa de quatro camadas caiu de72 para48px. Título/subtítulo fixos identificam contexto. Frequência, força, contraste e reforço usam largura inteira para evitar rótulos sobrepostos. Variantes mostram nomes e miniaturas reais, com GUIDs preservados internamente. Limites de fade não se cruzam e valores precisos continuam disponíveis. Abrir os ajustes inicializa a superfície legada sem exigir uma seleção prévia no catálogo.

O conceito gerado `terrain-noise-proposal.png` é apenas hipótese visual. Os PNGs `variation-*.png` são capturas da UI executável, sobre viewport neutro de fixture. Um ícone de clusters foi gerado com transparência, usando a linguagem do ícone existente; `terrain/cluster-generated.png` entra pelo atlas no ID TerrainLayers existente, preservando enum479 e fontes vetoriais/originais. O atlas e ambos os geradores foram verificados; saídas idênticas e headers SPIR-V idênticos conservam timestamps.

## Aceite local

- **113/113 cenários integrados**: `terrain-variation-host-final.log`. Incluem toque, fonte real, rolagem/barras, entrada precisa, independência de papéis, limites, variantes distintas e sete mapas, aplicação de cluster original e GLB reaberto. Duas asserções condicionais de symlink seguem indisponíveis por privilégio Windows.
- **116 modos GPU**: `terrain-cluster-gpu.log`, NVIDIA RTX3050. Código de produção/tabela/serialização reais, leitura de pixels e timestamps. Casos incluem extremos/meio da transição, papéis independentes, RGB/RG, RNM, inclinações/normais de face, altura, quatro compatibilidades, variantes/metragem, continuidade de células, determinismo e seleção consistente perto/longe. Descritores fixos do harness não provam alocador completo, iluminação final ou Android.
- **16/16 unidades Android ARM64 sintáticas**: `terrain-variation-android-final.log`; não é link/APK/runtime.
- Todos os shaders embutidos regenerados/validados: `terrain-variation-shaders.log`; no-op do header dirt_road em `terrain-shader-noop.log` preservou timestamp.
- 16 capturas UI executáveis1280×720/655×300 e alcance dos campos contínuos conferidos. Cenários noise/normal/anti-tile/resample/cluster/variant/variant-maps/variant-picker.
- Nove testes Python passaram: seis contratos de materiais, três publicação de arquivos gerados. Duas execuções do gerador conservaram os18 arquivos acompanhados e os58 thumbnails originais usados pelo atlas. A integração do novo ícone foi posterior e intencional; IDs permanecem inalterados.
- Revisão adicional de tabela/relocação/papéis/gradientes/rotas sem achados acionáveis. `git diff --check` passou.

Nenhum ADB ou APK novo. APK110 permanece anterior. Publicação desta continuação ainda será confirmada na branch/site. O plano integral de71 itens e os compromissos originais permanecem abertos.
