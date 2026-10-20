# M13–M15 — proposta estrutural executável da UI

Preparação isolada em20/10/2026. Este documento não altera fontes ativas, não executa build e não aceita M13–M15. M11/M12 permanece congelado na rodada de aceite do pai. Os IDs abaixo são reservas propostas, ainda sem widgets executáveis; nenhuma reserva autoriza expor um botão sem modelo, serialização, consumidor e teste. Nenhum valor é rotulado como simulação física ou vazão volumétrica por ser animado no shader.

Foram lidos integralmente:

- docs/auditoria-terreno-2026-10-16/PROXIMO-M13-M15-2026-10-20.md.
- build/terrain-flows-next/ARCHITECTURE.md, inclusive fechamento de assets, packing, manager e adjacentes.
- Contratos ativos de pintura/estado/numéricos e catálogo, sem modificá-los. UI-BASELINE-SHA256.json registra os arquivos consultados para futura aplicação coordenada; não constitui cópia pronta para substituir o root.

## 1. Separar editar, pintar e transformar

A referência de fluxo é Unity6000.0: [TerrainPaintTool](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintTool_1.html) separa entrada/saída do modo, preview e OnPaint; [CalculateBrushTransform](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintUtility.CalculateBrushTransform.html) distingue UV do terreno, espaço do terreno e transformação do pincel; [Transforms](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transform.html) mantém Move/Rotate/Scale como edição do objeto. Referências primárias abertas nesta preparação em20/10/2026. São princípios de interação; a UI conserva identidade Astra e não copia o Inspector.

| Modo Astra proposto | Ação real | O que um toque no viewport pode fazer |
| --- | --- | --- |
| Editar FX | Selecionar settings, fontes, autoridade, resolução e políticas; aplicar transação autoral. | Selecionar/navegar; nunca pintar automaticamente. |
| Pintar FX | Armar ferramenta tipada de máscara por canal, cursor de domínio e transação de gesto. | Convergir somente o canal FX selecionado; nunca escrever controlWeights, vertex weights ou TerrainStroke.layer. |
| Transformar | Encerrar/cancelar gesto FX e usar os ToolMove/ToolRotate/ToolScale existentes no objeto selecionado. | Mover/rotacionar/escalar objeto; máscara permanece em coordenadas locais e não é repintada. |

Câmera continua com a navegação existente. UI e gestos com múltiplos dedos têm prioridade sobre pintura. Trocar terreno, ferramenta ou modo encerra a transação vigente por regra explícita; Cancel restaura bitmap e receita do início do gesto. Não converter drag de slider, scroll lateral ou segundo dedo em amostra FX. O estado armado é visível; entrar para editar um material não arma o pincel silenciosamente.

## 2. Estrutura compacta e prioridade em655×300

Preservar o painel profundo existente, com96px de contexto e aproximadamente108px úteis de conteúdo: título32px, modo32px, grupos32px. Footer real de aplicação permanece; progresso só aparece enquanto um job real existe. Nenhuma linha de thumbnails de camadas PBR no contexto FX, pois não seleciona o canal editado.

1. Título32: voltar ao contexto de Pintura, nome do terreno/grupo, estado Pintar/Editar/Transformar e fechamento existentes.
2. Modo32: Editar / Pintar / Mover. Transformação detalhada usa os controles existentes da cena; o botão só existe com dispatch real de ferramenta.
3. Grupos32: Úmido / Poças / Streams / Lava. Seleção define TerrainFxChannel e grupo de settings, não terrainBrush.layer. Um grupo sem consumidor implementado não é um tab ativo fictício.
4. Pintar, scroll0: alvo e raio em duas colunas de64px, rótulos curtos e valores precisos; linha seguinte32px com Prévia e Pincel. Total96px. Modo armado e canal já estão fixos no contexto.
5. Pincel, scroll0: fluxo e falloff em duas colunas64px; abaixo32px Desenhar / Apagar. Formato/hardness/opacidade/aspect/rotação ficam no restante da página rolável.
6. Editar, scroll0: primeiro controle com efeito do grupo (máximo de wetness, máximo de poça, máximo de stream ou máximo de lava), seguido de estado ligado/fonte/autoridade. Fonte ausente tem CTA real e mensagem; sliders preparados não escondem que não há consumidor ativo.
7. Prévia, scroll0: alternar Material / Canal selecionado / RGBA composto, com imagem/cursor vindos do bitmap realmente publicado. Texto informa resolução, revisão e estado preparado/publicado. Sem heatmap ou miniatura inventada.

Desktop mantém os mesmos grupos e ordenação. Usa espaço adicional para inspeção, não insere cinco barras anteriores aos campos. Sliders contínuos sempre têm chip de entrada exata; enums, canais e resoluções são escolhas discretas, não sliders que fingem continuidade. Nenhum rótulo longo deve disputar a largura de um chip64px.

## 3. Modelo de máscara e fluxo autoral

Aguardar os nomes públicos finais do contrato puro em staged/. Interfaces esperadas por semântica, não aliases a tipos atuais:

- TerrainFxChannel: Wetness R, Puddle G, Stream B, Lava A; quatro valores independentes0..1. Soma pode exceder1.
- TerrainFxAsset (nome recebido no staged; integração na recipe ainda pendente): enabled, resolução, domínio, fonte importada opcional, versão de algoritmo e lista de strokes com IDs. Bitmap editável e source asset são entidades distintas.
- TerrainFxStroke: canal, posição local, shape2D, raio em metros Astra, aspect, rotação, hardness, opacity, alvo, flow, duração por amostra, pressão, gesture e filtros efetivamente consumidos; erase é estado transitório que grava alvo efetivo0.
- TerrainFxEditorState transitório: modo, canal, brush preparado, página/contexto, preview, gesto/revision/token e seleção de controller/emitter/collider. Não reutilizar terrainBrush, terrainProfilePoint ou queries de outras ferramentas.
- TerrainFxField: valores float de autoria e dirty rect inteiro; quantizeTerrainFx produz RGBA8 linear Clamp, e a consulta CPU usa os mesmos bytes quantizados publicados.

Criar máscara: escolher resolução explícita antes de alocar, mostrar bytes calculados reais, validar orçamento e criar preto neutro. Importar: selecionar fonte RGBA real no catálogo, preservar GUID/hash/original e explicar que o bitmap autoral será derivado. Sem fonte fornecida, criar preto é uma operação raster real, não uma textura branca substituta.

A pintura fonte converge ao alvo por lerp clamped com falloff espacial, sampleSeconds, flow e pressure. Não implementar soma irrestrita. Flow8/falloff1/target1/pressure1 são defaults fonte; raio em metros requer adaptação documentada. Opacity/hardness/shape/aspect/rotation só entram na UI se cada um participar da função raster compartilhada; a fórmula final de composição desses termos deve congelar com o runtime e ter testes sentinela.

Apagar usa alvo efetivo0 somente no canal selecionado e conserva o alvo positivo preparado para voltar a desenhar. Campo Alvo fica desativado/explicado durante erase, para não aceitar valor sem efeito. Restaurar a fonte é outro comando: só expor quando houver consumidor de restauração por área/canal; não chamar erase de Restore. Trocar canal preserva os outros três bytes e a seleção PBR.

Size e resolução independem de mesh cells e controlCells. Alteração destrutiva do domínio/resolução prepara resultado e regra de reamostragem/reprojeção antes da confirmação, com orçamento e cancelamento; nunca reduzir silenciosamente. Primeira capacidade heightfield; voxel/overhang recebe diagnóstico explícito até haver domínio adequado, sem fingir pintura por face via mapaXZ.

## 4. Páginas propostas, ainda não expostas

| View | Contexto | Prioridade e navegação |
| --- | --- | --- |
|300|Pintar FX|Alvo/raio; Prévia e Pincel; máscara pronta/ausente e canal.|
|301|Pincel FX|Fluxo/falloff; desenhar/apagar; shape, hardness, opacity, aspect, rotação.|
|302|Filtros do pincel|Slope mínimo/máximo na convenção1-clamp(dot(normal,up),0,1) e estado do filtro; up é informação do produtor, sem ediçãoXYZ até existir consumidor tipado.|
|303|Máscara, domínio e resolução|Criar preto/importar fonte real; resolução explícita; domínio derivado; orçamento e política.|
|304|Prévia da máscara|Material, canal, RGBA composto; revisão real e contagem de dirty texels; sem estatística fabricada.|
|305|Exposição/raycast|Job de cobertura real: parâmetros suportados, recalcular/cancelar e resultado; ausente até physics/job completo.|
|310|Wetness|Enabled e min/max; altura/distância/porosidade/chuva/fontes como links reais.|
|311|Altura Wetness|Nível mundial, contraste1/m, frequência temporal rad/s e amplitude em metros.|
|312|Distância Wetness|Início, fim=start+length, força perto/longe, sem confundir length com end.|
|313|Porosidade/camada|Global ou por camada conforme consumidor; preservar snowPorosity e evitar dois campos concorrentes.|
|314|Chuva/ripples|Intensidade/frequência, mapa e packing R shape, GB normal, A fase; flags/mode somente consumidos.|
|320|Poças|Blend por altura material e máximo; não volume geométrico.|
|321|Foam/camada|Override por camada0..2 e porosidade compartilhada, com precedência real.|
|322|Specular fades|Reserva: não expor antes de consumidor de resposta especular confirmado.|
|330|Streams|Enabled, máximo e blend; links Flow, Normal/Foam, Fades, UV e autoridade.|
|331|Stream Flow|Refração, frequência de fase, amplitude UV do ciclo e depth damp; nomes/unidades reais.|
|332|Stream Normal/Foam|NormalRG separado de foamB, forças e fonte original.|
|333|Alturas Stream|Quatro alturas mundiais ordenadas de entrada/saída, não fade de distância.|
|334|Mapeamento Stream|RepetiçõesXY e domínio real; direção derivada de slope/gravidade, sem direção pintadaRG.|
|340|Lava|Enabled, máximo, blend e emissãoHDR; fontes, cores, flow, crosta, fades e UV.|
|341|Lava Flow|Amplitude/ciclo e frequência de fase respeitando argumentos reais de Flow.|
|342|Distorção/crosta|Tamanho, taxa temporal, escala UV e escurecimento/crust.|
|343|Cores visuais Lava|Low, highlight e edgeRGB; não grausC nem resfriamento físico.|
|344|Alturas Lava|Quatro alturas mundiais, com guardas de ordem e rampas lineares.|
|345|UV/stochastic Lava|RepetiçõesXY e opções dependentes de cluster/consumidor; custo real.|
|350|Autoridade|Local ou referência persistente a controller real; resolução/missing e precedência.|
|351|Manager dinâmico|Buffers/tick/tempo e parâmetros reais; só com solver/lifecycle completo.|
|352/353|Emitters/Colliders|Listas reais porGUID; criar/selecionar/remover com capacidade e lifecycle.|
|354/355|Emitter/Collider selecionado|Tipo, força/raio quando consumidos e Transform existente; referência ao terreno/controller.|
|360..375|Mapas lógicos do módulo|Reserva de16 slots, alocar somente os papéis tipados congelados.|
|380..395|Pickers de fonte correspondentes|Busca, papel/pacote, thumbs reais e seleção estável do helper existente.|

Não adicionar estas views ao predicado de páginas ativas antes de implementar consumidores e testes de cada grupo. Novas views não podem cair por intervalos amplos em helpers M05–M12.

## 5. Inventário de material e controles com consumidor

| Grupo | Controles reais a preservar | Consumidor/gate |
| --- | --- | --- |
| Wetness | min/max; globalPorosity; heightWetness nível/contraste/frequência/amplitude; distance início/perto/fim/longe | FX.R e BRDF molhada; altura mundial+tempo e distância. Ordenação min/max/fim e comprimento>0. |
| Puddle | heightBlend6, máximo1 | saturate((FX.G*max-materialHeight)*blend). Rótulo altura material. |
| Streams | refração.4, fase.5, cicloUV.3, depthDamp.2; blend40/max1; normal.35/foam12; repeatsXY70; tintRGB.5; quatro alturas0/10/1000/1500 | Duas fases de Flow, refração da superfície, normal/smoothness/AO/foam; tempo e derivadas reais. |
| Rain | intensidade0, UV150; raw R shape/lifetime, GB normal e A fase | Chuva/ripples com mapa, normal e quatro fases; não usar normalRGB/RG/GA no lugar deGB. |
| Lava | blend2.7/max1/amplitude.5/fase.275; distSize.05/rate.017/UV.15/crust2; UV70; RGB edge/low/high; emissive2; stochasticContrast.5/size1 | HeatB, crustA e normal/distortionRG; emissãoHDR e hot/cold visual; sem promessa térmica. |
| Camada | porosidade0..1 e foam0..2 | Lookup por GUID e flags por-layer; não conflitar com slots Snow/Geo ou criar porosidade duplicada. |
| Snow clearing | wetnessMaskSnow e estado FX combinado | Mesmo produtor/bytes/tick/materialHeight/normal/autoridade da consulta CPU e shader; Estimate quando incompleto. |

Fonte streamfoam RG normal e B foam exige duas visões/mips, mantendo bruto. Rain GB normal é packing próprio novo, não alias de RG/GA. Lava RG normal/distortion e B heat/A crust são visões independentes do mesmo original. Alfas RGB não consumidos continuam preservados sem campos editáveis falsos. Map factors/remaps só aparecem por canal efetivamente usado no consumidor.

Flags a inventariar integralmente: wetness, puddles, streams, lava, perTexPorosity, perTexFoam, dynamicFlows, wetnessMaskSnow, heightWetness, rainDrops, globalWetness/globalRain/globalStreams/globalPuddles, heightDampStreams/heightDampLava, specularFade, distanceWetness, lavaStochastic. RainMode só vira escolha executável quando o trecho efetivo do runtime demonstrar seu consumidor; enum no pacote não basta.

TraxFXThresholds, tessLavaDislacementScale (grafia fonte), integração mesh/gravity e opções adjacentes sem consumidor permanecem em proveniência e diagnóstico de importação. Não ficam em sliders desativados que insinuem capacidade entregue. Opção importada ativa sem suporte recebe motivo e caminho de desativação explícito quando possível, nunca descarte silencioso.

## 6. Autoridades, animação e dinâmica

Local/global é resolução de autoridade, não um toggle de uniform sem dono. Selecionar controller existente usa GUID estável, criação real e referência persistida. A UI só oferece Criar controller se o tipo tiver modelo, cena, componente, serializer, registro e consumidor. Referência quebrada impede publicação atômica dos efeitos dependentes, explica o motivo e permite voltar a Local. Valores locais preparados continuam salvos quando a autoridade global vence.

Controller efetivo fornece limites wetness, máximo puddle/stream, rain intensity e relógio. Lava máximo permanece local segundo fonte; não inventar GlobalLavaMax. Tempo/fase vem do frame/scene runtime. Pausa ou scrub de prévia só são botões com API real que estabiliza tempo/tick; não animar com wall clock do serializer. Velocidade de fase e amplitude UV não transportam a quantidade pintada.

Manager: evaporation(.01,.01), strength(1,1), speed(1,1), resistance(.1,.1), wetnessEvaporation.01, burnEvaporation.01; precedem defaults do shader isolado. Exibir resoluções/bytes dos dois buffers e heightmap efetivos, tick real, limite e estado runtime somente quando medidos. Nenhum progresso se não há job.

Emitter Water/Lava, strength0..1 e Transform/raio seguindo semântica fonte; força participa do raio efetivo, não é litros/s. Collider Water/Lava/Both com domínio real. Capacidade64 de cada deve ser explícita; tentativa65 falha antes de criar parcialmente. Remover, duplicar, trocar terreno, Play/Stop/load e destruir atualiza registro, sem entrada órfã. Buffers dinâmicos não são salvos por acidente; checkpoint exige comando real e dados tempo/tick/seed/revision, com undo/restore coerente.

## 7. IDs e dispatch propostos

Não há ocorrência ativa do prefixo EC002 no inventário consultado. Reservar em conjunto com o runtime e revisar novamente antes de integrar:

- Action base0xEC002000, offsets0..255.
- Number base0xEC002100, índices locais0..255; entrada exata reconhecida por namespace antes de handlers antigos.
- Slider base0xEC002300, índice local0..255 com coluna0/1/2 em+0x100; ocupa2300..25FF sem alias de números/actions.
- Preview/image/widget de gesto somente após existir binding real; não reservar UiImageId como promessa de imagem disponível.
- Guard privado do terreno deve reconhecer este namespace explicitamente ou ampliar a faixa até0xEC003000 de forma coordenada. Helpers antigos continuam com intervalos exatos.

| Offsets Action | Ações propostas |
| --- | --- |
|0|Abrir FX com validação do terreno.|
|1/2/3|Editar/Pintar/Transformar; transação e armamento reais.|
|10..13|Selecionar Wetness/Puddle/Stream/Lava.|
|14..16|Preview Material/Canal/RGBA; só com textura publicada.|
|20/21|Desenhar/Apagar; restauração de fonte usa outra ação apenas se implementada.|
|22..27|Pincel/Filtros/Máscara/Prévia/Exposição/Revisão de gesto.|
|30..35|Criar máscara/importar; preparar/aplicar/cancelar resize; publicar snapshot estático.|
|40..63|Flags implementados dos grupos; cada offset mapeado a um bool concreto, nunca enum incremento de todas as flags desconhecidas.|
|64..95|Links para páginas concretas de wet/puddle/streams/lava/fontes/autoridade.|
|96..111|Selecionar mapa lógico; usar somente a contagem congelada.|
|112..119|Voltar/paginar fonte e ações de canal/packing suportadas.|
|120..125|Seis resultados de catálogo, resolvidos pela identidade desenhada.|
|128..135|Autoridade Local/selecionar/criar/desvincular, somente com respectivos consumidores.|
|136..143|Manager/pause/resume/checkpoint se APIs reais congeladas.|
|144..151|Criar/remover emitter/collider, tipos consumidos e navegação real.|
|160..165/168..173|Seis itens reais emitter/collider; paginação/seleção porGUID.|
|176..191|Reservado a operações de lifecycle comprovadas.|
|192..255|Não alocado; não aceitar como controle válido.|

| Índices numéricos locais | Grupo |
| --- | --- |
|0..23|Pincel/filtros: alvo, raio, fluxo, falloff, hardness, opacity, aspect, rotação, slopeMin/Max e parâmetros de job suportados. UpXYZ permanece não alocado enquanto não existir campo/consumidor.|
|24..47|Wetness/altura/distância/chuva/porosidade.|
|48..63|Poças, foam por camada, fades especulares apenas quando consumidos.|
|64..95|Streams/Flow/normal/foam/fades/UV/tintRGB.|
|96..143|Lava/Flow/distortion/crust/coresRGB/emission/fades/stochastic.|
|144..175|Controller/Manager e relógio efetivamente autorais.|
|176..199|Emitter/collider selecionados; Transform reutiliza pipeline existente.|
|200..223|Factor/remaps do mapa selecionado, reutilizados por contexto com gate exato de canais.|
|224..239|Inteiros/custos/configurações efetivamente autorais; resolução terá enum ou entrada inteira validada.|
|240..255|Sem alocação inicial.|

A tabela final deve listar getter, setter, range prático, limite exato, view, consumidor e serializer de cada índice antes de codificar. Campos invisíveis/sem efeito rejeitam entrada numérica forjada. Zero/sinal de escala e fluxo devem respeitar o contrato do runtime, sem clamp autoral escondido. Não congelar ranges físicos a partir de sliders Unity quando unidade fonte não é metro/grau.

## 8. Persistência, histórico e arquivos de integração

Serializers propostos da arquitetura: AETERRAIN9 com cauda FX version1 e AETERRAINSURFACE4 com cauda Flows version1, mantendo corpos legados e extensõesV2/V3. Reader1..8/1..3 cria FX neutro/desabilitado; fixtures históricas vêm de writers antigos reais. Não fabricar arquivo legado trocando somente o cabeçalho do serializer novo.

GLB carrega recipeFX editável e snapshot linear independente, domínio/origem/extensão, sourceGUID, revision/hash/dimensões/semântica. Não ocupar TerrainControlBinding.textureIndex de pesos PBR. Cache/import revision inclui algoritmo FX, bitmap/source, cap/packing e autoridade resolvida. Política Original/Cap é reaproveitada; máscara viva e ping-pong precisam orçamento/limite próprio, sem downres automático nem perfil thumbnail como fonte.

Um gesto é um undo. Down captura estado, Move acumula amostras com duração/pressão gravadas, Up confirma receita+bytes, Cancel restaura. Redo reproduz hash, não usa deltaTime atual. Workers carregam GUID do terreno+authorRevision+generationToken; stale job não troca outro terreno ou gesto. DirtyRect e ancestralidade de mips são separados de dirty chunks. FX-only não remesheia, não altera colisão, plantas ou pesos PBR.

Arquivos futuros, sujeitos ao contrato tipado: editor_terrain_fx_controls.h, editor_terrain_fx_screen.h/.cpp ou helper privado equivalente, editor_terrain.cpp para dispatch/gesto, editor_session.cpp para input exato/undo/publicação, editor_screen_state.h para estado transitório mínimo, editor_terrain_catalog_search.h para novas rotas reais, tests/native/test_terrain_ui.cpp e preview fixture. Runtime owns terrain_fx_field/asset/import, surface/recipe serializers, bindings/tabela/shader e autoridade/lifecycle. Uma única edição coordenada dos headers, sem reconstruções sucessivas por campo.

Busca reutiliza o helper de índices/identidade/thumbnails existente e contextos separados. Novos source_roles e packing de mapas FX devem vir de metadados reais; não tratar wet/puddle/stream/lava como quatro novos papéis PBR nem inferir normalGB pelo nome. Tipagem exata dos mapas lógicos fica pendente do runtime. Os quatro assets reais do contrato (streamfoam512, rain256, lava01/021024) só entram no botão de originais após importação/catálogo/hashes/roles/pipeline reais.

## 9. Matriz de testes e aceite

### Testes puros e persistência

- Canais sentinela independentes; soma>1 válida; pintar/apagarR não alteraGBA nem pesos PBR.
- Lerp clamped, alvo0/1, flow/tempo/pressão, shape/hardness/falloff/opacity/aspect/rotation e slope no espaço documentado.
- Stroke/frame invariance conforme política de amostragem; undo/redo/cancel reproduzem receita e hash dos bytes publicados.
- Dirty rect bordas0/1, halo/mips, budgets antes de alocação, cancel/stale tokens/outro terreno e troca de resolução/domain.
- Source/global precedência, controller ausente, duplicação/remove/load/Play e limites64+overflow atômico.
- Serializers antigos reais, disabled refs/proveniência completa, GLB/cache/reabertura fria e cap sem trocar originais/UV.
- Snow clearing usa o mesmo produtor e bytes/tick; sem inputs dinâmicos/texturas/material completos retorna Estimate com motivo, não Exact.

### Testes roteados de UI

- Abrir FX diretamente de terreno novo/legado; create preto real com resolução válida; material PBR continua inalterado.
- Modo Editar não pinta, Pintar arma explicitamente e Transformar usa a ferramenta existente sem continuar stroke.
- Em655×300, alvo/raio e Prévia/Pincel totalmente visíveis a scroll0; em Pincel, flow/falloff e erase visíveis. Chips numéricos reais sem colisões.
- Trocar grupo troca somente canalFX; alvos/canais anteriores e material layer permanecem. Erase afeta só escolhido e volta ao alvo preparado.
- Slider/entrada exata de cada campo têm mesmo limite/consumidor; fades start/end não cruzam, dots não viram graus, globais não habilitam sem autoridade.
- Busca/pacote/papel/thumb/seleção por GUID nos mapas FX; packingsRG/GB/heat/crust mantidos, factor/remap sem efeito rejeitado.
- Stroke por toque, pressionado sem pressão usa1, segundo dedo/UI scroll/cancel não grava amostra errada; uma entrada undo por gesto.
- Apply/reopen/histórico/cena fria preservam FX e settings; hashes de malha/colisão/controlWeights idênticos após pintura FX-only.
- Jobs reais mostram progresso/cancel e publicam uma revisão coerente; sem job, sem barra fake. Mudança destrutiva apresenta resultado concreto antes da decisão.

### GPU/runtime e visual

Parent valida seco/neutro, wet BRDF, poça por materialHeight, rain/ripples, stream phase/normal/foam/refract e lava quente/fria/crust/emission, todos com assets originais e luz real. Comparar CPU/GPU nos mesmos bytes/tick, fases0/.5/fronteiras, pause/reverse quando suportados, slopezero seguro, interações com Snow/Geo/Globals/cluster e autoridade.

Capturar UI executável655×300 e1280×720: máscara ausente/criada/importada, cada canal, Paint/Edit/Move, erase, filtros, source picker, autoridade ausente/ativa, emitter/collider/budget, job/cancel e scene reopen. Julgar hierarquia, área útil e estado. Se superficial, usar captura como referência para proposta de imagem, confrontar controles com consumidores, implementar e recapturar. Conceito não prova UI e fixture sintético não prova aparelho.

## 10. Coordenação pendente

Runtime foi contatado para o contrato puro em build/terrain-flows-next/staged/. Foram recebidas propostas puras de tipos, detalhadas abaixo; ainda não são contrato de publicação/scene/shader congelado. A proposta mantém semântica e reserva IDs, sem criar aliases ou structs ativos. Antes da implementação: congelar rasters/brush/filtros, campos completos de material, packingGB, porosidade compartilhada, controller/lifecycle e gates; depois mapear cada índice/ação ao tipo real. Nenhuma entrega pública de M13–M15 antes de capacidade completa por pacote e atualização do site pelo fluxo do projeto.


## 11. Tipagem recebida e alinhamento com o staged puro

Leitura coordenada, sem editar os arquivos do runtime/review:

- staged/native/resources/terrain_fx_asset.h: TerrainFxChannel, TerrainFxBrushShape, TerrainFxLimits, TerrainFxDomain, TerrainFxStroke, TerrainFxAsset; validators/serialize/deserialize próprios.
- staged/native/resources/terrain_fx_field.h/.cpp: TerrainFxField, TerrainFxDirtyRect, rasterTerrainFx, quantizeTerrainFx, checkpointTerrainFx, sampleTerrainFxBytes e terrainFxMipDirtyRect.
- staged/terrain_flow_settings.fragment.h: proposta Surface.flow e Layer.flow, settings/controlador/simulação/emitter/collider completos. Fragmento não é header integrado nem prova de consumidor visual.
- staged/terrain_flow_coverage.h: helper escalar de estágios, sem textura/BRDF/refração/normal/solver/GPU; correspondência Off/EstimateStaticInputs/EstimateDynamicInputs, sem enum Exact.

| UI proposta | Tipo/campo staged real | Adaptação/gate |
| --- | --- | --- |
| Canal FX | TerrainFxChannel | Wetness/Puddle/Stream/Lava; enum separado de camadaPBR. |
| Criar bitmap | TerrainFxAsset.enabled,width,height,domain,seedPixels | Domain vem da recipe real; seed vazio significa zero. Defaults256 e limites não autorizam alocação antes de validar. |
| Fonte importada | TerrainFxAsset.sourceImage | Asset original e bitmap editável distintos; integração com flow.streamControl ainda precisa dono único. |
| Alvo/raio/flow/falloff | TerrainFxStroke.target,radiusMeters,flow,falloff | Double; validação e limites do rasterizador, sem reutilizar float TerrainStroke de sculpt. |
| Shape/aspect/rotation | shape Circle/Box,aspect,rotationDegrees | Só esses shapes2D no primeiro consumidor; sem hemisphere/cone falsos. |
| Hardness/opacity | hardness,opacity | Raster atual calcula platô de hardness e taper elevado ao falloff; amount=clamp(falloffEspacial*sampleSeconds*flow*pressure*opacity,0,1). |
| Pressão/tempo | pressure,sampleSeconds | Dados capturados da entrada/tempo do gesto, não sliders que alteram o replay depois; ausência de pressão usa1. |
| Apagar | Não existe bool erase no Stroke recebido | Estado transitório da ferramenta; gravar target0 na amostra efetiva. Preservar alvo preparado para voltar a desenhar. |
| Slope | slopeFilter,slopeMin,slopeMax + span slope do raster | Produtor fornece campo normalizado da mesma resolução. Não existe upXYZ editável no tipo recebido. |
| Identidade/histórico | identity u64,gesture u32,strokes,bakedStrokeCount | IDs/gesture estáveis; prefixo baked está em seedPixels e o log inteiro permanece. Não reaplicar filtro antigo após sculpt. |
| Bytes da prévia | quantizeTerrainFx + sampleTerrainFxBytes | Campo autoral mantém float; inspeção CPU fiel usa os bytes publicados e Clamp pixel-center. |
| Wetness/poça/streams/lava | TerrainFlowSettings | Todos os vetores/flags/proveniências existem no fragmento; não expor até runtime visual e serializerV4. |
| Porosidade/foam camada | TerrainSurfaceLayer.snowPorosity / TerrainFlowLayerSettings.foam | Fragmento inicial tinha porosity; foi superado: snowPorosity é autoridade canônicaV3, e Layer.flow contém apenas foam. |
| Controller | TerrainFlowControllerState.identity e quatro availability flags | Estado resolvido puro não implementa criação, persistência ou lifecycle da entidade. Botões de cena continuam gated. |
| Manager/emitter/collider | TerrainFlowSimulationSettings/TerrainFlowEmitterSettings/TerrainFlowColliderSettings | Tipos de settings não provam registro, buffers, update ou Transform; não criar nomes de menu ainda. |

Limites puros recebidos: maximumDimension2048, maximumPixels4Mi, maximumWorkingBytes96MiB, maximumStrokes65536, maximumMetadataBytes64MiB, maximumRasterSamples128Mi. São limites atuais da proposta, não estimativas de memória medida no aparelho. A UI deve calcular orçamento da operação concreta e usar a validação antes de confirmar; nenhuma promessa de que todas as configurações máximas cabem simultaneamente.

### Adaptação início/fim versus source length

O fragmento recebido conserva distanceWetnessParams=(100,1,200,0), com Z sendo comprimento. Na página312, mostrar início100/fim300/perto1/longe0. Getter de fim=X+Z; editar fim grava Z=fim-X. Editar início mantendo o fim visual grava X=novoInicio e Z=oldEnd-novoInicio; ambos mudam atomicamente e exigem fim>início. Não serializar um fim dentro de Z nem migrar valores fonte implicitamente. Testar round-trip sem edição e ambos os setters, inclusive intervalos pequenos. StreamFades/LavaFades continuam quatro alturas independentes ordenadas, sem essa conversão.

### Decisões ainda pendentes de publicação

1. TerrainFxAsset.sourceImage/seedPixels/snapshot e TerrainFlowSettings.streamControl devem ter relação explícita, com um único dono da máscara estática. Não desenhar dois pickers concorrentes para o mesmo dado. dynamicStreamControl no fragmento não autoriza picker para um buffer que pertence ao manager.
2. Cinco mapas packed do fragmento (streamControl, dynamicStreamControl, streamNormal, lavaDiffuse, rainDropTexture) não são16 mapas autorais já implementados. Reservas360..375 são para views lógicas necessárias; número e compartilhamento de sourceGUID dependem dos bindings/decoders finais. RG normal/foamB, rainGB/shapeR/phaseA e lavaRG/heatB/crustA precisam views de mip independentes, sem duplicar seletor de fonte sem razão.
3. Tipo atual de TerrainSurfaceChannel não fornece GB específico para rain; não renomear RG paraGB. Congelar packing/codec real antes de expor canal editável.
4. Decisão congelada pelo pai/review: Layer.snowPorosity permanece autoridade canônica compartilhada na caudaV3; Layer.flow só contém foam. Não migrar, renomear serializer nem copiar default. UI usa o rótulo Porosidade do material.
5. Helper de cobertura puro não devolve Exact. O review registrou uma particularidade de ordem fonte quando lava substitui acumulador prévio de poça; parent/runtime deve escolher e documentar equivalência/adaptação. UI consome result.snowClearing/correspondence do produtor tipado, sem recalcular max próprio ou remover proteção Estimate do Snow.
6. Controller, simulação e emissor/collider ainda não possuem consumidor/lifecycle no staged puro. A lista de futuras páginas é inventário, não habilitação.

## 12. Patch de finalidade Normal separado desta proposta

Por pedido do pai após capturas M11/M12, existe patch SOMENTE staged em normal-picker-staged/. Ele não implementa M13–M15: novo picker de finalidade inequívoca Normal começa com filtro Normal, enquanto contexto salvo e escolha explícita Todos prevalecem. Abrange PBR/variante normal, Geo/Global normal, SnowGA/normal-noise e ruídos normais tipados. Sem inferir nomes. MANIFEST.json tem hashes de baseline/staged e comprovação de que ativos permaneceram iguais; aplicar só após liberação do pai e verificação de drift. Dois testes novos cobrem abrir/trocar/voltar/Todos e fonte SnowGA real. Nenhum build executado.


### Acordo proposto ao pai com o agente runtime

Após leitura dos staged, runtime concordou com: FxAsset como dono único da máscara estática; flow.streamControl apenas referência de origem/importação sincronizada a FxAsset.sourceImage ou derivado runtime, sem segundo editor. Importar decodifica seed e cria checkpoint; strokes produzem snapshot próprio e preservam original. Divergência das referências é diagnóstico de integração. A proposta inicial de migrar porosidade foi superada pelo congelamento do pai: snowPorosity continua canônicaV3 e compartilhada, layer.flow só contém foam; nenhuma migração/renomeação/cópia de default. DistanceWetness mantém source length e adapter início/fim. Checkpoints deverão validar hashes de prefixo/base/domínio/pixels além de bakedStrokeCount. Esses acordos ainda aguardam congelamento do pai e não alteram arquivos ativos.

NormalGB futuro também exige metadado/decoder próprio no catálogo/import: o filtro Normal pode descobrir o asset pelo papel explícito, mas isso não autoriza interpretar GB como RGB/RG/GA. Se a finalidade consumidora não suportar o packing escolhido, mostrar diagnóstico/recusar publicação antes de expor uma seleção aparentemente pronta.


Atualização final de tipagem confirmada pelo review e runtime: consultar staged/surface-v4/native/resources/terrain_surface_asset.h para o modeloV4 mais recente. O fragmento inicial fora dessa pasta é proposta antiga e não deve ser aplicado como modelo congelado. snowPorosity mantém dadosV3 como autoridade única; duas páginas podem editar o mesmo campo com o rótulo Porosidade do material, sem criar outra propriedade persistente.
