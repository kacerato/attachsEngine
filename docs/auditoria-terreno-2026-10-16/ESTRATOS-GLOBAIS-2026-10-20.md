# M09–M10 — estratos e mapas globais

Revisão de desenvolvimento de 20/10/2026, após `2750745c`, validada localmente conforme evidências abaixo. Não representa aceite completo do módulo fornecido ou APK.

## Contratos preservados

O corpo posicional de AETERRAINSURFACE1 permanece intacto. AETERRAINSURFACE2 acrescenta extensão versionada com estratos, quatro mapas globais e influências por GUID de camada. A leitura antiga recebe valores neutros/desligados; versões desconhecidas, GUIDs duplicados/órfãos e valores inválidos são recusados. As propriedades M01–M08 permanecem no mesmo corpo, inclusive fontes desativadas e proveniência.

O domínio global é origem X/Z e extensão completa em metros locais, derivado do tamanho real da receita, independente de textureScale, das variantes e da repetição da camada. GLB escreve esse domínio; o importador o exige em superfícies v2, inclui-o na chave de deduplicação e valida números/extensões. Cache de superfície extensão 2 conserva os quatro valores; o leitor também aceita extensão 1 com domínio neutro. Revisão do importador10 força regeneração dos derivados anteriores. A fonte e o histórico devem ser preservados.

## Estratos

Modo desligado/albedo/albedo+normal, mapas e remapeamentos próprios; mistura Multiply2x/LighterColor, intensidade, força normal, metros por repetição vertical e offset em metros. Range de quatro alturas mundiais ordenadas com rampas lineares e extremos coincidentes definidos pelo consumidor. Cada camada conserva participação e filtro de altura assinado/força. O filtro lê a altura material após variantes/resampling.

A curva conserva todas as chaves, IDs, modos de tangente, inclinações, pesos e vínculos usando AnimationCurve real. Seu eixo é horizontal rotacionado, com escala/offset em metros e rotação em graus; a saída soma UV de estrato. A LUT de 256 amostras é um derivado aproximado, sem truncar as chaves de autoria. O orçamento de metadados recusa dados grandes demais com diagnóstico. A extensão assinada conserva tempos negativos e externos a 0..1, sem alterar a política de tempo não negativo das animações existentes. A LUT continua amostrando 0..1 considerando todas as chaves. Curva vazia tem valor zero; uma chave tem valor constante. Gráfico e seleção abrangem os tempos autorados e destacam o intervalo consumido pelo material. Autoria, serialização e LUT foram verificadas no host; reabertura integrada e limites do aparelho são registrados no aceite abaixo.

## Mapas globais

Tint, normal, SAOM e emissão são independentes, com fonte, intensidade, faixa de distância, intensidade perto/longe, transformação UV e participação de cada camada. UV=(domínioNormalizado+offset)×escala, preservando a ordem do fonte. Fades lineares. Tint oferece Multiply2x/Overlay/LighterColor/Crossfade; normal usa RNM/Crossfade e formatos RGB/RG. SAOM e emissão usam Crossfade.

SAOM conserva seleção de canais/remapeamento/fator por smoothness/AO/metallic/máscara, com defaults R/G/B/A; roughness=1−smoothness. Referências compatíveis podem compartilhar amostra. Não interpretar normal GA como RG. Recursos adjacentes ainda sem consumidor (specular workflow, slope curves, ruído UV e políticas adicionais de amostragem) continuam registrados em `M09-M10-SOURCE-CONTRACT.md`; não foram apagados do compromisso integral.

## Assets e interface

Dois PNGs originais Geo 1×256 foram acrescentados ao catálogo, sem alterar os 82 materiais e 146 texturas anteriores. O catálogo passa a 148 texturas. Hashes, GUIDs e origem são verificáveis; thumbnails 1×160 são derivados separados. Os originais não são reduzidos. Mapas globais ausentes nos exemplos do pacote exigem escolha explícita; não são substituídos por branco.

Ícones novos de estratos e domínio global compartilham a geometria vetorial Astra, masters 384px e atlas real 96px. IDs antigos 1–479, inclusive marca 478/479, permanecem; novos conceitos 480/481. O empacotador mantém nomes e retângulos em conjunto ao acrescentar conceitos após a marca. A proposta visual e as capturas executáveis serão avaliadas separadamente; gráfico conceitual não comprova interface pronta.

## Revisão visual antes do aceite

A primeira captura executável 655×300 mostrou uma falha estrutural: barras de navegação repetidas ocupavam toda a área útil, deixando nenhum controle editável visível em scroll0. A mesma captura mostrou Ruído selecionado ao editar Geo. Esses resultados impedem aceitar a primeira UI mesmo com os 128 testes integrados verdes.

Uma proposta foi gerada a partir dessas capturas reais e está em `build/terrain-evidence/terrain-global-layout-proposal.png`. É hipótese de design, sem prova de implementação. O mapeamento aprovado mantém Back→Pintura, seletor da camada com miniatura real e setas, navegação do módulo e propriedades existentes. Fontes/mode/blend ganham página própria; números e gráfico ganham prioridade. A barra de progresso decorativa da imagem e miniaturas de fontes sem atlas real não serão implementadas. Não há propriedade nova sem modelo/serialização/consumidor.

A revisão executável produziu **82 capturas**, com 41 estados em 655×300 e 1280×720. A inspeção confirmou sliders de Geo/Global/Ruído na abertura, gráfico e seleção de chave visíveis, ação de primeira chave em curva vazia e fonte/estado global visíveis. Ruído e mapas PBR antigos usam o mesmo contexto compacto, preservando seus controles. O título explicita Geo/curva desligados; sem fonte, o mapa global só ativa após selecionar uma textura real.

O teste integrado encontrou vazamento da página de texturas ao voltar à biblioteca de materiais; a entrada em biblioteca agora reinicia a página quando muda de contexto. **132/132 cenários integrados passaram** após a correção. O fixture de captura respeita a inatividade real de escala por camada quando cluster controla escala e filtro de altura desligado. As capturas são do layout executável de produção, sem cena 3D iluminada; não demonstram qualidade visual final do terreno nem ergonomia no aparelho.

## Referências concretas

Fonte fornecido: MicroSplat Global Texture versão interna 3.9.0, Unity 2019.4. Conversões e limites estão no contrato de fonte, sem alegar execução Unity ou paridade numérica.

Capacidades estudadas na Unity 6000.0: [AnimationCurve](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationCurve.html), [Keyframe](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Keyframe.html). Shader Graph 17.0.4: [Blend](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Blend-Node.html), [Tiling and Offset](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Tiling-And-Offset-Node.html), [Sample Texture2D](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Sample-Texture-2D-Node.html), [Normal Blend](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Blend-Node.html). Verificadas em 20/10/2026. Astra adapta os fluxos ao toque e conserva sua identidade.

## Validação e publicação

- Core: **64/64 cenários** passaram, incluindo curvas assinadas/vazias/uma chave, fixture v1 produzida pelo escritor legado real e tabela mista M05–M10 com relocação.
- GPU NVIDIA RTX 3050: **199 modos** passaram no shader real, incluindo todos os pixels de referência do par Geo original. A cena offscreen 32×32 não mede desempenho Android.
- Curvas de animação existentes: **33/33 cenários** passaram, conservando a política NonNegative das APIs anteriores.
- Assets/atlas: **3 testes Python** passaram, com bytes/mtime estáveis em execução idêntica e recusa de conflito sem publicação parcial. Preparação de neve: **1 teste** contra os quatro TGA reais passou; continua staging, fora do catálogo ativo.
- Revisão focada de código: nenhum erro funcional grave concreto encontrado em curvas assinadas, domínio/cache, tabela/relocação ou shader; não substitui testes no aparelho.
- Os 44 shaders embutidos foram regenerados e validados. Sem erro novo; os avisos preexistentes de precisão em água permanecem.
- Editor integrado: **132/132**; **82 capturas executáveis**. **17/17 unidades Android ARM64** passaram em compilação sintática; não é link ou execução Android. Publicação ainda em andamento.

Sem ADB e sem APK novo; APK 110 é anterior a estas mudanças. Os 71 itens e o primeiro bloco continuam abertos.
