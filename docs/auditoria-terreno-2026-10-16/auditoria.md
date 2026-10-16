# Auditoria e escopo de implementação — cinco plugins

**16/10/2026 · Astra · branch `codex/terrain-voxel-generator`.**

**A implementação atual cobre uma parte pequena dos pacotes. Nenhum dos cinco está integralmente implementado.** Há um gerador finito, edição básica de altura/densidade, receita persistida, prévia, quatro camadas e um shader de albedo com variação. Isso não equivale aos sistemas completos de Digger, Path Painter, MicroSplat, Advanced Terrain Grass ou MapMagic.

Esta entrega é a auditoria e o compromisso de escopo solicitado, não uma nova entrega de APK. O ADB não foi usado nesta auditoria. Os testes de aparelho foram interrompidos conforme solicitado. As correções locais iniciadas anteriormente continuam separadas desta análise; a compilação/testes interrompidos da revisão 87 não são tratados como aprovados.

## Entrega direta

| Pacote | O que será incorporado à Astra | Situação real hoje |
|---|---|---|
| **Digger 6.0** | Escultura volumétrica completa do recorte fornecido: formas e dimensões, escavação/adição/pintura/restauração, buracos, acabamento, chunks afetados, LOD, persistência e API. | Parcial: volume finito, quatro formas aproximadas, ferramentas e receita. Falta gestão incremental, vários parâmetros e API runtime. |
| **Path Painter II 2.1.10** | Caminhos editáveis com pista e acostamento separados, perfis/curvas, rampas, seguir terreno, cruzamentos, pintura inteligente e limpeza de vegetação. | Parcial: nivelamento e pintura por amostras. Não há sistema completo de caminhos editáveis. |
| **MicroSplat Collection** | Camadas PBR, transições por altura, triplanar configurável, anti-repetição real, clusters, mapas globais, neve, umidade/poças/fluxos/lava, integração mesh-terreno e efeitos dos 11 módulos recebidos. | Parcial: quatro albedos e pesos por vértice, triplanar de albedo e duas escalas da mesma textura. A maioria dos módulos está ausente. |
| **Advanced Terrain Grass 1.4** | Vegetação instanciada, protótipos, mapas de densidade, vento, iluminação/translucência, sombras/fades, culling/cache, interação e origem flutuante. | Ausente como sistema integrado ao terreno. Não contar renderização genérica de meshes como implementação de ATG. |
| **MapMagic 2 Bundle 2.1.11** | Grafos executáveis, nós de mapas/objetos/splines, erosão, biomas, funções, presets de pincel, tiles/vizinhança e geração em runtime. | Ausente como sistema MapMagic. Noise/Terrace da escultura atual não equivalem aos nós, grafo e execução do pacote. |

**O escopo inclui as famílias abaixo e seus parâmetros de autoria.** Opções custosas terão perfis de qualidade e detecção de capacidade; não serão silenciosamente removidas do plano. Adaptadores específicos de Unity serão substituídos pelos consumidores da Astra. Exemplos desativados, placeholders e duplicações entre versões não virarão itens falsos no menu.

## Inventário recebido e como conferir

| Distribuição | Conteúdo no pacote principal | Observações verificadas |
|---|---:|---|
| Digger 6.0 | 151 arquivos | 73 C#, 14 shaders, 11 HLSL, 17 PNG, 15 prefabs. |
| Path Painter II 2.1.10 | 85 arquivos | Quatro DLLs em duas variantes, dois XML de API, manual identificado como 2.1.2, 42 PNG, quatro PSD e 16 TerrainLayers. |
| MicroSplat Collection 3.9.41 no nome da distribuição | 517 arquivos | Os 11 `package.json` internos declaram **3.9.0 / Unity 2019.4**. 53 C#, 95 TXT de suporte/snippets, 34 shaders e 39 PDFs. |
| Advanced Terrain Grass 1.4 | 173 arquivos | Manual 1.36; 14 C#, 13 FBX, 33 PSD, 23 materiais, 12 prefabs, cinco cenas e **oito Unity packages internos**. |
| MapMagic 2 Bundle 2.1.11 | 1.220 arquivos | 225 C#, 26 FBX, 28 prefabs, 76 `.asset`, 685 PNG, 36 TIF, 11 RAW, 15 cenas e cinco PDFs. Muitos PNG são ícones de editor, não texturas de terreno. |
| **Total principal** | **2.146** | Exclui `.meta`, diretórios e previews separados. Inclui os oito arquivos de pacote interno como arquivos. |
| **Conteúdo dos oito pacotes internos ATG** | **314 adicionais** | Variantes URP 7.1.7/7.2/10/11/12.1, HDRP 10.1/12.1 e Grass Displacement. Há conteúdo repetido entre versões; não são 314 capacidades únicas. |

O catálogo soma **2.460 ocorrências de arquivos**, com caminho, hash, tamanho e dimensões das imagens reconhecidas. Há **11.281 registros extraídos de declarações, controles, propriedades de shader e API XML**. Esse número inclui métodos, portas, infraestrutura e repetições; **não é uma contagem de funcionalidades**.

- [Índice pesquisável](indice.html).
- [Matriz dos 71 itens de implementação](plano-implementacao.csv), com estado atual, propriedades e lacuna/aceite.
- [Todos os arquivos e assets](arquivos.csv).
- [Declarações, controles, shaders e API com arquivo/linha](declaracoes-controles.csv).
- [Nós e parâmetros extraídos do MapMagic](mapmagic-nos.md).
- [Manifesto, hashes, subpacotes e contagens](inventario.json).
- [Destinação dos assets e arquivos](destinacao-assets.csv).

Os anexos são de pesquisa local. O catálogo de declarações é uma extração estática, não uma compilação/reflexão de C# nem comprovação de execução Unity. O catálogo de nós remove comentários, mas mantém variantes condicionais/legadas e placeholders identificáveis por caminho e atributos. Campos herdados e controles desenhados por código devem ser conferidos pela referência de declaração e pelo Inspector antes de fechar cada item. Não se deve chamar esse catálogo de “todas as propriedades já portadas”.

## 1. Digger — escultura e volume

**Fonte:** `Assets/Digger/Modules/Core/Sources/ModificationParameters.cs`, `BrushType.cs`, `ActionType.cs`, `Operations/`, `Jobs/`, `ChunkLODGroup.cs` e `Editor/Operations/ABasicOperationEditor.cs`.

| ID | Implementação comprometida e parâmetros | Lacuna atual / aceite necessário |
|---|---|---|
| D01 | Dig/Add com posição XYZ, força/opacidade, raio e dimensões locais. Separar força acumulativa de opacidade-alvo. | A força atual não representa todos os modos do original. Medir efeito e independência da frequência de eventos. |
| D02 | Esfera/elipsoide, meia esfera, **cubo arredondado**, estalagmite e inversão; X/Y/Z independentes, vínculo de dimensões e orientação. | Caixa não é cubo arredondado; cone atual não tem altura independente/inversão. Comparar silhueta, influência e volume resultante. |
| D03 | Profundidade do alvo em relação à superfície, manter altura durante o gesto, alvo acima do dedo e carimbo XYZ interno. | Falta o contrato completo de profundidade/trava. Cursor deve representar exatamente o volume editado. |
| D04 | Pintura volumétrica: camada selecionada, opacidade, opacidade-alvo, preservação/normalização dos demais pesos. | Quatro pesos existem; biblioteca extensível e alvo de opacidade faltam. Testar paredes e teto da caverna. |
| D05 | Reset/Restore, Smooth e Sharpen com raio, força e vizinhança volumétrica; restaurar geometria e pesos com regra explícita. | Existem operadores próprios; falta confrontar seus resultados com os casos do pacote, inclusive bordas. |
| D06 | Buracos na interface altura/voxel, abertura e fechamento, remoção de faces internas e colisão correspondente. | Buraco fechado no relevo não substitui integração altura↔volume. Testar entrada de caverna sem fresta visual ou física. |
| D07 | Atualização **apenas dos chunks afetados**, borda de dependência de normais/filtros e costura entre chunks. | Hoje regenera o terreno inteiro. Aceite exige registrar quais chunks realmente foram refeitos. |
| D08 | LOD de geometria, distâncias, transição e continuidade de bordas; colisão com política própria. | LOD ausente no terreno. Não confundir chunking com LOD. |
| D09 | Histórico por gesto, deltas/checkpoints, salvar/carregar e recuperação; revisão e descarte seguros. | GLBs antigos se acumulam. Implementar coleta respeitando cenas, referências e Undo. |
| D10 | API runtime de modificação, callback/conclusão, cancelamento, fila, limites e consulta do terreno. | C++ de editor não é API pública de gameplay. Integrar fachada da geração atual da Astra. |
| D11 | Notificar consumidores de colisão e navegação quando o volume muda; sincronizar materiais da superfície e da caverna. | Não anunciar navmesh dinâmico sem consumidor de navegação implementado e testado. |
| D12 | Prévia durante o gesto, modos exclusivos, desfazer/refazer e controles por forma. | Há prévia e exclusão de gizmo. Corrigir realimentação da deformação no amostrador e testar antes de concluir. |

## 2. Path Painter — caminho é um recurso editável

**Fonte:** manual local, páginas físicas 9–18, 21–32 e 61–70; `Editor/Engine/{000,010}/PathPainter.xml`. As DLLs não serão tratadas como fontes C# disponíveis. O XML fornece contratos públicos; o comportamento precisa de implementação e comparação próprias.

| ID | Implementação comprometida e parâmetros | Lacuna atual / aceite necessário |
|---|---|---|
| P01 | Largura da pista e largura total do acostamento, independentes; manter proporção opcional; limites baseados na resolução real. | Raio e hardness únicos não representam pista+acostamento. |
| P02 | Curva/perfil do acostamento e visualização da seção transversal; transições separadas de forma, textura e vegetação. | Incluir talude, crista, suavidade e efeito real da curva; não reutilizar um único falloff para tudo. |
| P03 | Liga/desliga independentes: modelagem, texturização, limpeza de grama, limpeza de árvores e visualização da vegetação. | O Path atual combina modelagem e pintura. Separar canais e suas alterações. |
| P04 | Elevação relativa positiva/negativa; Auto Ramp; limite de inclinação em graus; direção da rampa e ligação entre alturas. | Y fixo não é Auto Ramp. Testar subida, descida, encontro e leito de rio. |
| P05 | Terrain Follow ↔ rampa uniforme, com mistura contínua entre os extremos e opção de altura definida. | Reaplicar ao editar o caminho sem destruir outras operações. |
| P06 | Textura da pista, textura do acostamento, opacidade-alvo, curvas e Smart Texture Paint; política explícita de sobrepintura e cruzamentos. | Mistura genérica de uma camada não decide cruzamentos nem preserva a pista anterior. |
| P07 | Caminho persistido com pontos/segmentos selecionáveis: criar, selecionar, continuar, mover, inserir/remover ponto, duplicar e apagar. | Reabrir receita de terreno não equivale a editar um caminho. |
| P08 | Segmentos retos, curvos e mistos, extremidades/caps e união de trechos; reconstrução a partir do traço original. | Interpolação linear entre amostras é apenas uma parte. |
| P09 | Limpeza de grama com tipo/escala/seed de ruído, zona totalmente limpa e faixa de rarefação; limite independente para árvores. | Depende de mapas/instâncias reais de vegetação. Desfazer deve repor a distribuição. |
| P10 | Terrenos vizinhos, grupos, coordenadas compartilhadas e propagação/reconciliação de camadas. | Terrenos finitos atuais não têm grupo autoral conectado. Não duplicar camada por diferença de índice. |
| P11 | MapScaler equivalente: resoluções independentes de altura, controle de material e detalhe/vegetação; reamostragem preservando conteúdo. | Alterar `cells` sozinho não resolve esse recurso. |
| P12 | API Paint/Paint3D, linhas, operações em lote e edição de parâmetros; mesma lógica no editor e runtime autorizado. | Implementar contratos Astra, sem empacotar DLL Unity no Android como se ela executasse ali. |

## 3. MicroSplat — superfície, transições e efeitos

**Fonte:** os 11 módulos em `Packages/com.jbooth.microsplat.*`, seus `Scripts/Editor`, shaders/snippets e respectivos PDFs. Versão interna recebida: 3.9.0. Cada propriedade de material deverá ter consumidor, serialização e resultado verificável, não apenas aparecer no Inspector.

| ID | Implementação comprometida e propriedades | O que falta hoje |
|---|---|---|
| M01 | Biblioteca de camadas com identidade estável; mapas de controle independentes da malha; arrays/empacotamento; ordem, ativação e miniaturas. Escala, offset, rotação, tint/saturação por camada. | Quatro texturas fixas e pesos por vértice limitam variedade e resolução da pintura. |
| M02 | Albedo, normal/intensidade, altura/remapeamento, AO/intensidade, smoothness↔roughness, metallic, emissão e força por camada. Regras de canais explícitas. | Albedo não equivale a material completo. Usar os normal maps já fornecidos. |
| M03 | **Height blend** entre materiais: contraste, largura, influência da altura e qualidade de mistura; pesos normalizados e transição coerente de normal/PBR. | Crossfade de cor atual não preserva pedras/saliências da camada. |
| M04 | Triplanar local/mundo; escala/offset; contraste de eixos; projeção por camada; blend por altura; normais de face versus suaves. | Há projeção de albedo com parâmetros fixos. Faltam controles, normal/PBR e blend por altura. |
| M05 | Detail noise e distance noise: textura, escala, força, início/fim de fade; controle global e por camada; projeção triplanar dos ruídos. | Variação macro atual não substitui essas duas frequências/distâncias. |
| M06 | Até três camadas de normal noise, escalas/forças e participação por material; array anti-tile com mapas próprios. | Não implementado. |
| M07 | Distance resampling de albedo e normal: escala, faixa de distância, constante/ruído/crossfade, blend por altura e combinação com clusters. | Não implementado; testar aproximação sem mudança brusca de cor. |
| M08 | Clusters com duas/três texturas distintas; stochastic; máscara/ruído, escala/offset, contraste, noise boost e qualidade por material. | Misturar duas escalas da mesma imagem não é cluster de variantes. |
| M09 | Geo texture/estratos: albedo/normal, mistura, escala/offset em metros, quatro alturas de entrada/saída, curva, rotação e filtro por altura de cada camada. | Não implementado. |
| M10 | Mapas globais tint, normal, SAOM e emissão; mistura Multiply2x/Overlay/LighterColor/Crossfade, intensidades e influência por camada. | Não implementado. O grafo poderá produzir esses mapas. |
| M11 | Neve simples/rica: quantidade, altura/inclinação, direção de acúmulo, máscaras min/max, normal noise, resampling/stochastic, cristais/gelo, derretimento e clareamento por altura/erosão. | Não implementado. Consulta de cobertura CPU deve declarar correspondência com a variante visual. |
| M12 | Neve: translucência, brilho/glitter, rim/emissão e transição molhada na borda; propriedades específicas por camada. | Não reduzir neve a pintar branco. |
| M13 | Wetness/poças: máscaras e pincel FX, escurecimento, normal/smoothness, cobertura, transição seca↔úmida, gotas/ripples e limites por altura/inclinação conforme módulo. | Água genérica da cena não implementa umidade do terreno. |
| M14 | Streams: direção/velocidade de fluxo, normal, máscara, mistura com poças/umidade, animação e controles globais/locais. | Não implementado no material do terreno. |
| M15 | Lava: mapas próprios, fluxo, crosta, transição quente/fria e emissão/temperatura visual. | Não reduzir a uma textura laranja animada. Sem simulação física prometida pelo simples shader. |
| M16 | Mesh↔terreno: captura/amostragem de altura, normal e material; distância/profundidade de mistura e participação por objeto. | Testar pedra, talude e entrada de caverna sem faixa artificial. |
| M17 | Parallax/POM: altura, passos/qualidade e fade por distância; displacement/tessellation com fator, distância e contraste onde houver suporte. | Ausente. Vulkan/Android deve selecionar variante suportada. POM não muda silhueta nem colisão; displacement exige contrato separado. |
| M18 | Alpha-hole com máscara/canal/limiar, sombra e política de colisão; vento e glitter do módulo Wind com mapa, escala, velocidade/intensidade e fade. | Buraco geométrico atual não é Alpha Hole. Vento da superfície não é vento da vegetação. |

**Limite do material recebido:** há PDFs de Procedural Texturing, Scatter, Trax, Decals, Mesh Terrains, Object Shader, Low Poly e 256 Textures no core, mas não módulos de código correspondentes. Eles não são “plugins já recebidos”. O plano cobre os 11 módulos efetivos; funções equivalentes oferecidas pelo MapMagic entram por esse pacote. Não anunciar os módulos adicionais como portados a partir de um PDF.

## 4. Advanced Terrain Grass — vegetação de verdade

**Fonte:** `Scripts/GrassManager.cs`, `GrassTerrainDefinitions.cs`, `Wind/Wind.cs`, `Scripts/HiZBuffer`, shaders ATG e oito pacotes internos. Manual principal recebido: 1.36. O manual descreve um renderer que consome densidades; não é, sozinho, um gerador procedural de vegetação. A integração MapMagic/Path Painter fornecerá e editará esses dados.

| ID | Implementação comprometida e controles | Dependência/aceite |
|---|---|---|
| G01 | Protótipos de grama/folhagem/rochas: mesh, material, densidade, min/max de tamanho, ruído/seed, rotação/alinhamento ao terreno. | Distribuição determinística e persistida; regenerar não pode trocar tudo aleatoriamente. |
| G02 | Mapas de densidade por camada; sincronizar/pintar/apagar/atualizar; dados salvos separados da malha; visualização da distribuição. | Base para MapMagic e limpeza de caminhos. Não armazenar milhões de entidades comuns. |
| G03 | Células/buckets, cache, worker, quantidade de células processadas por frame e preparação em torno da câmera. | `useBurst` no original significa inicialização em lote/raio; não presumir que seja Unity Burst Compiler. |
| G04 | Instancing e draws indiretos quando suportados; seleção de câmera/layer, culling por célula, frustum e Hi-Z/oclusão; modo de diagnóstico. | Adaptar aos recursos Vulkan da Astra e manter alternativa funcional sem compute. |
| G05 | Cull distance/fade length, detail fade start/length, cache distance e densidade global; tamanho de instância usado no culling. | Aproximação e afastamento graduais, sem surgimento brusco ou falha de bounds. |
| G06 | Sombras por protótipo; início/comprimento de fade separados para grama/folhagem; receber sombras, alpha cutoff, normal buffer e depth prepass. | Render colorido, profundidade e sombra precisam concordar no recorte e deformação. |
| G07 | Simple Merge e Soft Merge de camadas, arrays de textura e variante por instância; preservar distribuição e identidade visual. | Não confundir mesclar draw calls com trocar todos os protótipos pelo mesmo mesh. |
| G08 | Cor saudável/seca, variação e escala; normal/bend normal, máscara especular, smoothness, translucência/transmissão e intensidade. | Usar os canais empacotados reais dos assets. Testar contraluz e faces duplas. |
| G09 | Vento: direção/intensidade/turbulência, multiplicadores de grama/folhagem, textura/resolução, tamanho, velocidade e três velocidades de camada. | Movimento compartilhado e coerente entre protótipos. |
| G10 | Rajadas: tiling/speed, jitter de baixa/alta frequência, multiplicador por material, amostragem no pivô e Wind LOD. | Distância deve reduzir custo sem estalo de pose; sombras acompanham a animação. |
| G11 | Displacement/interação recebido no subpacote: mapa ortográfico, cobertura/resolução, deslocamento à frente da câmera, textura RGBA, força/rotação, emissores de mesh/partículas e distância ao solo. | Sobreposição de agentes e recuperação suave; conferir limites quando saem da área de interação. |
| G12 | Origem flutuante/terrain shift, teleporte e troca de câmera; manter instâncias e vento estáveis após deslocamento. | Não presumir mundo infinito apenas por deslocar a origem. |
| G13 | Presets e estatísticas reais: densidade, células visíveis/em cache, draws, memória contabilizada e opções de qualidade. | Testes por perfil. Valores estimados devem ser identificados; não inventar FPS ou memória total. |

As variantes URP/HDRP são referências de implementação, não modos que a Astra passará a executar. O suporte específico VR/single-pass não é prometido como entregue sem um backend XR da Astra; fica como dependência explícita. O manual antigo não certifica suporte Android de todas as variantes. A validação Vulkan precisa acontecer na implementação.

## 5. MapMagic — grafo, biomas e mundo

**Fonte:** `Assets/MapMagic/Generators`, `Nodes`, `Terrains`, `Locks`, `Brush`, `Expose`, `Compatibility` e cinco manuais. Foram extraídas **129 declarações com GeneratorMenu**, após excluir comentários. Incluem classes legadas, placeholders e adaptadores condicionais; não são 129 nós únicos de produto. O [catálogo individual](mapmagic-nos.md) registra classe, atributos, arquivo/linha e declarações dos parâmetros.

| ID | Implementação comprometida | Parâmetros/efeitos que precisam acompanhar |
|---|---|---|
| Q01 | Grafo persistido: criar, conectar/desconectar portas tipadas, mover/duplicar/remover nós, grupos, pesquisa, comentários, presets e Undo. | IDs estáveis, validação de conexão/ciclo, estado desabilitado/bypass, erros por nó e recuperação ao carregar. |
| Q02 | Executor determinístico, dependências, cache/invalidação, jobs canceláveis, progresso e prévia por nó/saída. | Seed, coordenadas mundiais, resolução, margem, tempo de cada nó e dados alterados. |
| Q03 | Map/Initial: Constant, Noise, Voronoi, Simple Form, Import e Spot. | Valores, seed, escala, detalhe/ruído, posição/tamanho/formato e importação dos mapas reais; portas conforme catálogo. |
| Q04 | Map/Modifiers: Curve/Unity Curve, Levels, Contrast, Mask, Blend, Normalize, Blur, Cavity, Slope, Selector, Terrace e Direction. | Curvas/histogramas, intervalos/transições, intensidade, modos de blend e vizinhança; salvar todos os controles de cada nó. |
| Q05 | Relevo avançado: Ledge, Beach, Erosion, Sediment e Parallax de mapas. | Parâmetros/iterações e saídas de erosão/sedimento; este Parallax de mapas não é POM do shader. |
| Q06 | Map Set: criar/adicionar/selecionar/combinar conjuntos; operadores de curvas, níveis, máscara, blur, cavidade, inclinação, seleção, terraço, erosão e deslocamento. | Identidade das camadas e operações em conjunto; não achatar a saída em quatro cores fixas. |
| Q07 | Entradas/saídas de altura, textura/splat, grama, material próprio, texturas/matrizes diretas. | Resoluções independentes, modos de aplicação, margens/bordas e mapeamento de camadas. |
| Q08 | Objetos iniciais: Positions, Scatter, Random e Get by Tag. | Densidade/probabilidade, seed, máscaras, posição e seleção de protótipos/objetos. |
| Q09 | Objetos: Adjust, Randomize, Mask, Lerp, Floor, Split, Rarefy, Combine, Spread, Flatten, Stroke, Stamp, Forest, Slide, Move, Shrink Scale e Safe Borders. | Distribuição/afastamento, transformação, aderência à superfície, restrições, escalas e parâmetros do catálogo. |
| Q10 | Saídas de objetos/árvores, pooling e identidade persistente; portais de objetos. | Culling/LOD e ligação com vegetação; mudanças localizadas não podem recriar todo o mundo sem necessidade. |
| Q11 | Splines: Manual, Interlink, Pathfinding, Stroke, Align, Stamp, Optimize, Relax, Combine, Weld Close, Floor, Avoid, Push, Isoline e Silhouette. | Pontos/tangentes, caminhos sobre mapa de custo, distância/tolerância/iterações, largura e queda; portais e saída editável. |
| Q12 | Biomes Set e Whittaker, subgrafos, máscaras, opacidade e transições; temperatura/umidade e mapas de biomas fornecidos. | Mistura deve afetar altura, material, grama e objetos, sem emenda entre tiles. |
| Q13 | Function, Loop, entradas/saídas tipadas, portais, parâmetros expostos e overrides. | Escopo, tipo, valor padrão, reuso e invalidação corretos; classes “Outdated” só para migração quando necessária. |
| Q14 | Terrenos/tiles: pin/unpin, geração draft/final, distâncias de geração/manutenção, vizinhos, costura, cache, regenerar e geração em runtime. | Tamanho/resolução/margens e política de memória; carregar/descarregar preservando alterações autorais. |
| Q15 | Locks de regiões/dados e pincéis baseados em grafo: presets rápidos, raio, hardness, spacing, leitura/escrita de altura/textura/grama/árvores/objetos e ajuste do último traço. | Proteger edição manual da regeneração; prévia, aplicar/reverter e histórico dos dados afetados. |
| Q16 | Adaptar saídas MicroSplat/vegetação ao mesmo backend da Astra; importar presets e reconstruir cenas de demonstração úteis. | CTS/RTP/MegaSplat/VS Pro condicionais não viram dependências obrigatórias nem itens sem consumidor. O núcleo funcional terá saída nativa. |

Detalhes que também entram no contrato de Q11/Q15: Interlink com iterações, máximo de conexões, Within Tile e Clamp; Optimize com Split/Deviation; Pathfinding com resolução, fatores de distância/elevação/curvatura e inclinação máxima. Presets de pincel guardam cópia de grafo/raio/hardness/spacing e parâmetros expostos, com **Salvar** e **Salvar como** distintos. Auto-valores do gesto incluem posição atual/anterior/capturada, altura do terreno e modo alternativo; no toque, esse modo terá controle explícito em vez de depender de Shift/Ctrl. Pintura em múltiplos terrenos deve diagnosticar dimensões/resoluções incompatíveis.

Não vou colocar todos os nomes no menu antes de seus executores funcionarem. Também não vou chamar um preset “bioma” se ele só mudar a cor do terreno.

## Transições — critérios visuais separados

| Transição | O que precisa ser implementado e demonstrado |
|---|---|
| Material A → B | Controle de largura/contraste, height blend e mistura de normal/PBR; pedra não desaparece como tinta borrada. |
| Projeção topo → parede | Triplanar com controle de eixos, normais corretas e opção de transição por altura, sem esticamento ou costura. |
| Textura próxima → distante | Faixa de fade/resample/noise configurável, sem troca súbita de padrão ou luminosidade. |
| Variante A/B/C | Máscara/contraste e distribuição; variação de imagens reais, não apenas rotação de uma textura. |
| Pista → acostamento → terreno | Três regiões visíveis na prévia, curva e largura com efeito distinto na geometria e no material. |
| Caminho → cruzamento | Regra de sobreposição/continuação e pista preservada; opção de sobrepintura explícita. |
| Vegetação cheia → caminho | Zona limpa e faixa gradual com ruído; árvores têm limite próprio. |
| Bioma → bioma | Pesos compartilhados entre materiais, alturas, objetos e densidades; sem borda de tile. |
| Grama próxima → distante | Fade de instâncias/detalhes/sombras e redução do vento; não desaparecer em blocos perceptíveis. |
| LOD/chunk → vizinho | Costura geométrica/normais e transição coerente, com colisão contínua. |
| Mesh/rocha/caverna → terreno | Mistura de altura, material e normal que acompanha o encontro real. |
| Seco → molhado → poça | Cor, roughness/normal e cobertura correspondentes, com máscaras editáveis. |
| Terreno → neve/gelo/derretimento | Máscara, altura, inclinação, relevo da camada e borda molhada; não uma cobertura branca uniforme. |
| Grama → personagem/objeto | Curvar, combinar influências e recuperar sem salto; vento e interação coexistem. |

## Assets — o que será aproveitado

**Hoje foram incorporados apenas quatro albedos do Path Painter, reduzidos a 256×256.** O manifesto atual `assets/terrain/manifest.json` confirma isso. Não foram incorporados todos os materiais, mapas de normal, modelos e presets dos cinco pacotes.

| Origem | Aproveitamento previsto |
|---|---|
| Path Painter | Camadas de grama, terra, areia, forest ground, caminhos, rocha e rocha com musgo; variações de caminho e combinações; reconstruir presets a partir dos 16 TerrainLayers. Preservar originais 1K/2K/4K e gerar versões por perfil. |
| Digger | Conjuntos de terreno da demo, materiais de caverna e meshes de retículo como referências de forma; reconstruir demonstrações de escavação e borda. Não substituir ícones Astra por branding do fornecedor. |
| MicroSplat | Albedos **e normais** de rock/cliff/moss/wasteland/sand/ground, variantes de clusters, ruídos Perlin/detail, mapas de neve, gotas, lava e glitter. Reconstituir presets de material e os canais empacotados. |
| ATG | Grama, grama com margaridas/pontas marrons, meadow grass, dry grass, nettle, pineappleweed, fern/fishbone, broadleaf shrub e rochas. Importar meshes, albedo/alpha, normal/translucência/especular/smoothness, vento e mapas de interação. |
| MapMagic | Árvores birch/pine/stumps, sedge/nettle, pedras, terreno de neve/areia/cascalho/cliffs/gramas, heightmap Island4 e mapas RAW de biomas. Recriar grafos/presets/cenas de exemplo com dependências válidas. |

Pipeline obrigatório de reaproveitamento:

1. Resolver GUIDs e dependências de material, mesh, textura, prefab e preset; identificar duplicatas por hash, mantendo proveniência.
2. Preservar fontes originais; converter modelos para o formato suportado, mantendo pivô, bounds, UVs, normais, tangentes e canais de vértice usados pelo vento.
3. Interpretar os canais de cada mapa. Albedo em sRGB; normal/altura/máscaras/densidade em linear; preservar alpha. Não reduzir mapa de normal a “textura colorida”.
4. Gerar mipmaps/compressão e resoluções por perfil. **Não reduzir todo o acervo permanentemente a quatro PNG de 256 px.**
5. Criar biblioteca com miniaturas, filtros, presets utilizáveis e indicação das dependências. Um asset importado sem material/consumidor não conta como integrado.
6. Só empacotar no APK os assets necessários ao editor/exemplos selecionados; os demais permanecem disponíveis na biblioteca/importação. Arquivos de documentação, DLLs Unity, crash dumps e variantes duplicadas de pipeline não são conteúdo de jogo.

O arquivo `licença.txt` fornecido acompanha a proveniência. A inclusão técnica de cada asset ainda depende de importação e conferência visual; inventariar um FBX/PSD não comprova que ele já funciona na Astra.

## Arquitetura e UI que vou implementar

**Base de dados compartilhada.** Terreno terá altura/densidade, pesos de material, máscaras de efeitos, densidade de vegetação e objetos com resolução/identidade próprias. Caminhos e grafos serão recursos editáveis que escrevem nessa base. Os cinco sistemas não serão cinco janelas isoladas com cópias incompatíveis do terreno.

**Publicação localizada.** Edição marca regiões/chunks sujos, agenda jobs, atualiza preview e depois confirma deltas e colisão. Undo representa a ação do usuário. Cache, LOD, fontes persistidas e coleta de revisões têm contratos separados. A prévia atual que regenera o pacote inteiro precisa ser substituída; adicionar mais botões não resolve esse custo.

**Fluxo único:** selecionar terreno → escolher tarefa → ajustar parâmetros com prévia → editar → desfazer/refazer → salvar e reabrir no mesmo contexto. Ferramentas: **Esculpir, Caminhos, Materiais, Vegetação, Grafo e Gerir**, com criação/estrutura acessível. Selecionar ferramenta desativa manipulação do objeto; navegar não pinta. Alterações numéricas mostram efeito e estado pendente; aplicar confirma, não revela o resultado pela primeira vez.

**Muitas opções, navegação curta.** Controles frequentes próximos ao viewport; parâmetros avançados por seção, busca, favoritos e presets. Camada/caminho/nó selecionado determina o Inspector. Prévia mostra raio, queda, profundidade, pista/acostamento, máscara ou densidade conforme tarefa. Cores e ícones distintos para conceitos distintos; textos explicam o efeito, não nomes internos do renderer.

**Sem botões decorativos.** Cada controle tem leitura, alteração, serialização, Undo, consumidor e cenário de teste. Em recurso não suportado pelo perfil GPU, mostrar condição e alternativa real; não permitir ativação que apenas muda um booleano.

## Execução em três blocos

1. **Base e autoria:** dados independentes, publicação por região, biblioteca de assets/camadas, completar pincéis/volume, caminhos editáveis e materiais PBR/transições. Fechar D01–D09/D12, P01–P08/P10–P11 e M01–M08 junto de seus consumidores.
2. **Geração e vegetação:** executor/grafo e famílias Q01–Q16, densidade/objetos/árvores, ATG G01–G13, limpeza de caminho P09 e integração com biomas. Não gerar vegetação como decoração desconectada.
3. **Acabamento e runtime:** mapas globais/efeitos M09–M18, APIs D10–D11/P12, qualidade/LOD/cache, migrações, demonstrações e validação integrada. Capacidades GPU condicionais terão documentação específica por perfil.

Esses blocos organizam dependências, **não reduzem o escopo aos primeiros itens**. Não há prazo inventado para um port integral. A auditoria atual não autoriza marcar os três blocos como feitos.

## Aceite: quando um item poderá ser chamado de concluído

- Fonte e versão identificadas; propriedades do pacote confrontadas com os controles equivalentes na Astra, com diferenças registradas.
- Modelo, consumidor, persistência e ciclo de vida reais; criação, seleção, reedição, Undo/Redo e cancelamento sem perda de dados.
- Teste de efeito para cada propriedade importante, incluindo extremos, composição com outras ferramentas e bordas de região.
- Assets convertidos com materiais corretos, escala/canais/pivôs conferidos; cena de exemplo que exercite a função.
- Captura da UI executada antes/depois, estados e feedback durante o gesto; toque no aparelho quando o ADB estiver novamente disponível.
- Medição real de geração, publicação, CPU/GPU/memória por cenário. Nenhuma promessa de desempenho com base apenas no build.
- Branch publicada sem merge na main da engine; nota sanitizada em Atualizações do site com recursos entregues, parciais, migração e validação. Os anexos de fonte/declaração desta pesquisa não serão copiados para o site público.

## Referências oficiais de capacidade

As fontes locais fixam o universo auditado. Estas referências oficiais sustentam os sistemas equivalentes da engine; não comprovam implementação na Astra.

- Unity 6.0: [Raise or Lower Terrain](https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-RaiseLowerTerrain.html) e [Paint Texture](https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-PaintTexture.html).
- Unity Terrain Tools 5.0: [Brushes](https://docs.unity3d.com/Packages/com.unity.terrain-tools@5.0/manual/brushes.html).
- Unity 6.3: [Grass and other details](https://docs.unity.com/en-us/engine/6000.3/manual/creating-environments/script-terrain/terrain-grass).
- Unity 6.7: [TerrainData](https://docs.unity.com/en-us/engine/6000.7/script-reference/unityengine/terraindata), separação de alturas, alpha maps, detalhes e árvores.
- Godot 4.6: [MultiMeshInstance3D](https://docs.godotengine.org/en/4.6/classes/class_multimeshinstance3d.html), [SurfaceTool](https://docs.godotengine.org/en/4.6/classes/class_surfacetool.html) e [FastNoiseLite](https://docs.godotengine.org/en/4.6/classes/class_fastnoiselite.html).

**Conclusão de escopo:** aproveitar os recursos, propriedades, transições, assets e fluxos concretos listados, implementando seus sistemas consumidores. O resultado anterior permanece uma base parcial; a auditoria não o reclassifica como equivalente aos plugins.
