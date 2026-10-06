# Desempenho estrutural do editor — POCO F7 / Sponza

## Objetivo e ponto de partida

Executar o roadmap `Astra_Roadmap_Desempenho_Graficos.md` fornecido em
05/10/2026, usando-o como referência de engenharia. Melhorar custo por frame e
execução sustentada preservando a imagem e os dados do projeto. Não contabilizar
redução de resolução, preset, efeitos ou meta de FPS como otimização estrutural.

O Release instalado corrigiu custo de desenvolvimento: CPU ativa observada de
13,09 para 2,16 ms; GPU de 21,75 para 12,37 ms. A comparação anterior não fixou
rigorosamente a câmera. O aparelho chegou a 44,3 °C na bateria e teve pressão
térmica leve. Portanto aquecimento e desempenho sustentado continuam abertos.
Evidências em `docs/validacao/performance-poco-f7-2026-10-05/diagnostico.md`.

## Ordem de execução e dependências

```text
M0: identidade + câmera efetiva + resolução/cadência controladas
  ├─ M4/M5: composição final sem STORE/LOAD intermediário desnecessário
  ├─ M2: cena persistente, invalidação e uploads por alteração
  ├─ M3: visibilidade conservadora na geometria real do viewport
  └─ M7: views invisíveis, trabalho em repouso e consumo por frame
          ↓
M8: A/B/B/A no aparelho + imagem + caudas + execução aquecida
```

| Bloco | Hipótese / alteração | Prova exigida | Estado após reconexão |
|---|---|---|---|
| M0 | Contexto emitido antes da Sponza; launcher não encaminha pose; câmera de diagnóstico não dirige o editor | Contexto após publicação com pose idêntica, counts reais e janela reiniciada | Implementado; pose, pressão, memória e energia medidos |
| M4/M5 | Pós e UI da saída de um anexo fazem passes separados e recarregam a imagem final | Compor UI no mesmo passe compatível; A/B no mesmo APK; imagem equivalente | Implementado; ganho de GPU 2,81%, insuficiente isoladamente |
| M2 | Procurar reconstrução, alocação e upload recorrentes nos caminhos realmente quentes | Contadores por alteração e CPU ativa em repouso/movimento; corrigir apenas desperdício demonstrado | Divisão espacial medida: ganho 4,41%, custo de memória, opt-in |
| M3 | HZB está bloqueado para viewport de editor; habilitar cegamente viola projeção | Corrigir viewport/rotação/histórico, fail-open em movimento; medir economia líquida e ausência de sumiço | Corrigido; sem eficácia nesta vista, opt-in |
| M7 | Editor continua produzindo carga ao atingir o limite térmico | Medir repouso/navegação, views ativas, potência quando disponível e frames exibidos | Reuso estático implementado: potência -41,75%; câmera/iluminação/Play verificados |
| M8 | Mais FPS pode elevar potência mesmo com frame mais barato | Comparar a mesma cadência e qualidade; repetir em estado térmico comparável; endurance de 20 min | A/B/B/A e térmica de 20 min fechados; continuidade apresentada longa parcial |

A primeira composição mantém o caminho anterior para TAA (segundo anexo de
histórico), FSR1 (saída intermediária), HUD e ausência de pós/UI. Uma chave de
diagnóstico permite A/B no mesmo APK; não vira preset nem propriedade autoral.
Nenhum histórico temporal deve conter pixels de interface.

## Protocolo de ganho e rejeição

1. Mesmo projeto, pose explícita, tamanho do viewport, AA, sombras e resolução.
   Resolução fixa primeiro; adaptação térmica/DRS analisada separadamente.
2. APK Release único, mesma assinatura, atualização sem apagar dados. Registrar
   SHA-256, aparelho, temperatura inicial/final, target e caminho executado.
3. A/B/B/A: caminho anterior / fusão / fusão / anterior. Separar carga inicial,
   aquecimento de cache e janelas de 600 frames. Registrar logs brutos.
4. CPU ativa, esperas, GPU total e regiões, p95/p99, draws/triângulos, RSS,
   Vulkan memory e FPS de apresentação. Timestamps em tilers podem colapsar;
   GPU total prevalece sobre uma atribuição de passes inválida.
5. Capturas na mesma pose; verificar materiais, gizmos, transparência e bordas.
   Depois verificar movimento, troca de projeto, Play/Stop e recuperação.
6. Comparar consumo à mesma cadência. Temperatura da bateria é um indicador
   lento, não temperatura do SoC nem prova isolada de potência. Não declarar
   aquecimento resolvido sem execução sustentada e sinal de consumo válido.
7. Investigar regressão acima de 5% que supere a variabilidade. Rejeitar ganho
   de média acompanhado de imagem incorreta, p99 pior ou falha de lifecycle.

## Referências concretas

- [Godot 4.5: GPU optimization](https://docs.godotengine.org/en/4.5/tutorials/performance/gpu_optimization.html): distinguir geometria, pixels e banda antes de escolher técnica. Adaptar ao renderer existente da Astra.
- [Khronos: Tile Based Rendering Best Practices](https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html): evitar tráfego de anexos entre passes e preservar dados no tile quando os consumidores são compatíveis. A fusão da composição final aplica esse princípio sem reduzir qualidade.
- [Android: power optimization](https://developer.android.com/games/optimize/power): eficiência e cadência sustentável precisam de evidência própria; não inferir potência apenas de FPS.

Este plano não declara os marcos concluídos. Cada bloco recebe resultado e
limitações após implementação e medição, sem transformar o roadmap inteiro
num catálogo de recursos já existentes.

## Execução e decisões desta sessão

- **M0 implementado e medido:** pose explícita encaminhada pelo launcher e
  aplicada à câmera real do editor; contexto reiniciado após publicação da
  cena; counts de fonte e instâncias reais; coletor SurfaceFlinger dedicado,
  sem lacunas nas quatro rodadas do HZB corrigido. A última rodada do recorte
  de composição ainda teve lacuna e não vale como FPS de apresentação.
  Fingerprint interno de pacote continua zero
  em projetos independentes: usar SHA-256 do arquivo da cena e das configurações
  para identidade autoral; não interpretar zero como hash de conteúdo.
- **M4/M5 composição implementada e comparada:** fusão isolada não apresentou
  ganho. Recorte do pós ao viewport e clear correto tiveram diferença observada
  de 2,81% em GPU e 3,04% em FPS; abaixo do critério de eficácia de 5%. A imagem
  variou em apenas um pixel, com diferença máxima de dois níveis por canal.
  Portanto não registrar este bloco como solução suficiente do problema.
- **M3 HZB corrigido, mas rejeitado como otimização padrão nesta vista:** produtor
  reconstruído após importação; viewport físico/rotação; invalidação por cena,
  projeção e viewport; cache estático; fail-open em câmera em movimento.
  A comparação detectou culling incorreto, levando a corrigir inversão Y e
  projeção das duas faces dos limites em CPU/compute/SPIR-V. Depois a imagem
  ficou idêntica, mas os 21 desenhos descartados somavam apenas 48 triângulos.
  A GPU média passou de 10,706 para 11,022 ms (2,95% pior), e o FPS interno
  passou de 78,20 para 74,79. Não houve ganho; HZB permanece opt-in.
- **M2/M3 divisão interna de geometria implementada como candidata:** reutiliza
  `SpatialRenderChunks` nos opacos importados, com blocos de até 2048 triângulos.
  Mantém objeto, matriz, material e índices de instância da autoria; preserva
  integralmente os triângulos; submete blocos visíveis por lote indireto do mesmo
  objeto. Transparências, água, impostores, geometrias deformadas e LODs distintos
  mantêm o caminho anterior. Buffers participam do allocator/lifecycle real.
  Há custo adicional de índices, CPU de culling e carga inicial: precisa de A/B.
  A candidata fica **desativada no uso normal** até aceite no aparelho.
- **M8 parcial:** 32 testes direcionados de HZB/culling/chunks passaram no host;
  o build do target host completo falhou no assembler por `test_bulk50.cpp.obj`
  grande demais, sem relação com estes casos. Builds Android Release passaram.
  O POCO desapareceu do ADB antes da medição da divisão espacial; reconnect
  e descoberta mDNS não encontraram aparelho. APK e coletor ficam preparados.
- **Estado ao perder o ADB:** M7 e aceite térmico estavam abertos. `powerstats`
  não devolveu dados e os sensores sysfs de corrente/clock recusaram acesso.
  A continuação após reconexão, abaixo, usa a HAL de bateria via Perfetto.

Próxima coleta: `tools/measure-editor-composition.ps1 -SpatialComparison` com
mesmo APK e ordem A/B/B/A; primeiro resolução/cadência fixa, depois navegação e
execução aquecida. Promover o caminho apenas com geometria/imagem corretas,
economia acima do ruído, caudas aceitáveis e custo de memória registrado.

Referência adicional: [Godot 4.5: occlusion culling](https://docs.godotengine.org/en/4.5/tutorials/3d/occlusion_culling.html)
explica que limites grandes têm menor eficiência de oclusão. Aqui a adaptação
divide unidades internas do renderer, sem exigir que o usuário quebre seus
objetos editáveis para obter granularidade de visibilidade.

## Continuação após reconexão do POCO

- Cena/configuração conferidas pelo SHA-256 anterior; APK candidato instalado
  sem apagar dados. Primeira comparação foi descartada após a superfície
  mudar de 60 para 120 Hz no meio da coleta.
- **M2/M3 candidata espacial medida:** 1872 blocos, menos 502.893 triângulos
  enviados na vista principal (22,43%); GPU 10,460→9,999 ms, FPS interno
  82,07→84,66. Custo extra de buffers de 44.038.144 bytes e cerca de 64 MB
  em RSS. Há diferenças em 40 pixels por reordenação; 14 excedem 3/255.
  Melhora líquida de GPU 4,41%, ainda abaixo de 5%; permanece experimental.
- **M7 implementado no consumidor Vulkan:** reaproveita a cor/profundidade
  intermediárias já existentes quando câmera, projeção, tempo da cena,
  uniformes reais, opções e recursos não mudam. A UI continua sendo composta
  em todos os frames. Não há novo render target nem redução de FPS/qualidade.
  Transformação, material/visibilidade, grade, streaming, HDRI e recriação de
  superfície invalidam o cache. Play, água, deformação, temporal, exposição
  automática e prévia de câmera pendente seguem pelo caminho completo.
- **A/B/B/A de M7 na mesma cadência:** apresentação contínua em quatro rodadas;
  A 59,72–59,83 FPS, B 60,06 FPS. GPU média 14,066→9,554 ms. Potência total
  obtida da HAL de bateria via Perfetto: 4,446→2,590 W (41,75% menor).
  Câmera, resolução, sombras e imagem iguais; capturas B idênticas a A.
  O clock da GPU pode variar: o resultado é custo/consumo efetivo do aparelho,
  não uma alegação de medição isolada da GPU ou de clock fixo.
- **Instrumentação de energia funcional:** sysfs inacessível não implica HAL
  inacessível. `android.power` expõe corrente, tensão e carga neste POCO;
  power rails não estão disponíveis. A análise integra o sinal no tempo,
  registra carga decrescente e ausência de carregador. Sinal é do aparelho
  inteiro, incluindo tela, rádio e coleta, igualmente presentes em A/B.
- A segunda vista detectou adaptação de sombras 25→9 taps e foi rejeitada.
  O diagnóstico passou a poder fixar qualidade autoral sem desligar monitor
  ou proteção térmica do Android; a câmera bloqueada reinicia o contexto
  ao atingir a pose solicitada depois da publicação inicial do projeto.
  Segunda vista passou com imagem idêntica e FPS interno 110,21→120,12;
  apresentação de B teve lacuna. Câmera e iluminação invalidaram corretamente.
  Play/Stop passou em RuntimeGameplay0910; Sponza tem MeshCollider inválido
  anterior à otimização, preservado e diagnosticado.
- **M8 endurance estático medido:** 20 min, GPU média 9,529 ms e FPS interno
  60,056; escala 0,75/qualidade constantes, pressão `none`, memória GPU estável.
  Potência média total 2,925 W e temperatura de bateria 44,0→42,6 °C.
  A coleta de apresentação longa teve lacunas; não afirmar FPS exibido contínuo
  por 20 min. Capturas, trace e gráfico em `editor-scene-reuse-soak/`.
  M7 promovido e instalado no Release final; abertura normal e diagnóstico
  explícito `false` verificados. Dados atuais em
  `docs/validacao/performance-poco-f7-2026-10-05/continuacao.md`.

Referência de atualização por demanda: [Godot 4.5 SubViewport](https://docs.godotengine.org/en/4.5/classes/class_subviewport.html),
`UPDATE_ONCE`/`UPDATE_DISABLED`: atualização do render target separada de sua
apresentação. Adaptação: invalidar pelos consumidores reais da Astra e manter
a UI independente, com fallback completo para conteúdo dinâmico.
Referência de medição: [Perfetto: battery counters](https://perfetto.dev/docs/data-sources/battery-counters).

## Continuação autoral da Sponza e transição térmica

No pedido posterior de player/iluminação, o MeshCollider inválido foi corrigido na autoria e o Play recebeu Character, câmera e controles nativos persistentes. Cena visual original: 3.738.920 triângulos; com player: 3.739.944. Colisão derivada separada da geometria visual. Sol, ambiente e seis fontes pontuais ajustados usando os sistemas existentes.

A validação encontrou um defeito independente da autoria: ao reduzir a escala máxima sob pressão severa, o renderer reportava a extensão ativa como se fosse a extensão do recurso alocado. A amostragem do pós-processamento lia margens não desenhadas. Corrigida a separação entre extensão de alocação e extensão de desenho; Play/movimento/olhar/salto em pressão severa passaram no POCO. APK corrigido instalado, sem mudar a configuração gráfica persistida.

**O caminho dinâmico continua aberto no roadmap:** Play mede aproximadamente 50–56 FPS internos nas vistas verificadas e não usa M7. Editor aquecido/severo foi limitado a cerca de 60 FPS; o aceite anterior de 120 FPS em repouso não é promessa de 120 FPS sob essa pressão. Esta revisão fecha autoria e corrupção visual da transição, não eficiência do Play ou endurance em navegação. Evidências: `docs/validacao/sponza-player-2026-10-05/README.md`.

## Pacote seguinte: Play preservando a imagem

Objetivo explícito: reduzir custo do frame em movimento, mantendo resolução, materiais, iluminação, sombras, efeitos e geometria visual. DRS, preset inferior, LOD mais agressivo, redução de cadência ou menor densidade não contam como ganho deste pacote.

Baseline atual da Sponza: GPU média 17,8 ms e CPU ativa de aproximadamente 3,8 ms numa vista inicial; cerca de 2,43 milhões de triângulos visíveis. São dados do aceite anterior, não uma atribuição definitiva do custo a geometria ou pixels. O reaproveitamento de M7 é exclusivo do editor estático e não é solução para Play.

A rodada A da composição controlada atribui aproximadamente 15,0 ms ao opaco, 3,24 ms ao pós e 0,30 ms à UI, com atribuição de regiões resolvida. O principal alvo é o opaco. A atribuição ao passe não separa ainda binning/vértices de shading/texturas: a próxima análise de M3/M5 deve fazer essa separação antes de selecionar mudanças em shaders ou buffers. Composição é uma candidata pequena, não a promessa de resolver os aproximadamente 50 FPS.

1. **M0/M4/M5: composição do Play.** O pós e a GUI autoral do Play ainda usam passes separados sobre a imagem final. Primeira candidata: compor a GUI no mesmo passe final, preservando ordem, tamanho do viewport, cor e interação. A interface não entra em históricos temporais. TAA, FSR1 e o HUD separado do controlador global mantêm o caminho anterior. Candidata opt-in até comparação de GPU total e imagem no POCO; não presumir que o ganho do editor se transfere para Play.
2. **M3/M5: custo de geometria e visibilidade.** Medir mais de uma vista e navegação, distinguindo geometria, fragmentos e banda. Reavaliar granularidade e oclusão conservadora apenas em vistas em que o ganho líquido supere seu custo. HZB/divisão espacial não são promovidos só porque descartam triângulos; as candidatas anteriores ficaram abaixo do critério ou pioraram GPU.
3. **M2/M4: preparação/submissão persistentes.** Medir bytes de upload, mudanças de recursos, lotes e sincronização com player em movimento. Corrigir reconstrução/upload de dados estáticos apenas se observados. Manter o relógio de física, input, scripts e animação; não esconder custo reduzindo simulação.
4. **M8: aceite dinâmico e térmico.** Mesma cena/pose/rota, configuração e cadência entre A/B/B/A; imagens e funcionamento do player; FPS apresentado, GPU total, p95/p99, memória e potência à mesma cadência. Teste aquecido separado e carregador registrado. Meta de 120 FPS exige frame sustentável abaixo de 8,33 ms; é objetivo, não garantia antes das medições.

Princípio da primeira candidata: [Khronos Vulkan Guide, Tile Based Rendering](https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html), evitar gravação/leitura externa de anexos entre passes compatíveis. Referência de diagnóstico: [Godot 4.5, GPU optimization](https://docs.godotengine.org/en/4.5/tutorials/performance/gpu_optimization.html), separar gargalos de geometria, pixels e banda antes de escolher a técnica.

Resultado da primeira candidata: imagens idênticas, diferença observada de GPU de apenas 0,2234% (18,6120→18,5704 ms), sem aumento de FPS. Não promovida; permanece desativada por padrão. A medição revelou e levou a corrigir identidade de Play/Stop e o snapshot de modo/câmera no autostart. No APK final, o contexto de Play usa a câmera do player e zero frames reutilizados; isso é correção de medição/transição, não ganho de FPS. Evidências e limitações: `docs/validacao/play-performance-2026-10-05/README.md`.

**Próxima implementação prioritária: M3/M5 no passe opaco.** Separar vértices/binning, overdraw e material/sombras por captura/isolações diagnósticas, sem promover os modos de isolamento como configuração do jogo. Depois corrigir o trabalho redundante dominante: especialização/early depth quando seus contratos permitirem, tráfego de geometria ou visibilidade conservadora na câmera do player. Cada técnica continua sendo hipótese até medir ganho líquido e imagem equivalente; não combinar várias mudanças sem atribuição. M2/M4 de CPU fica atrás desse alvo enquanto a GPU dominar a vista.

## M3/M5: família sólida de shader, pacote validado em Play

Atribuição diagnóstica na câmera do player: Full / sem normal / sem IBL / base color / Full = GPU 18,34 / 16,41 / 18,46 / 9,60 / 18,50 ms. Modos diagnósticos não foram promovidos como configuração. O custo remanescente de base color inclui geometria, cobertura e pós; não é uma medida pura de vértices.

Implementada especialização de sólidos sem recorte no renderer real, com materiais/overrides e dither de instância como condições de seleção. Alpha mask/blend, água, impostores e transições de LOD mantêm o shader original. Reutiliza os recursos e a ordem de submissão; há lifecycle de pipelines e fallback conservador. O shader original mantém os três OpKill no SPIR-V, enquanto a especialização sólida elimina todos.

A/B/B/A no POCO F7, mesma pose e extensão interna 0,75, sem frames M7: GPU 18,5769→16,0166 ms (-13,7823%); FPS apresentado de 47,98/48,24 para 56,97/56,48; zero pixels diferentes. Ganho do pacote aceito e ativado por padrão. Isso não conclui M3/M5, não prova 120 FPS nem endurance térmico. Potência à mesma cadência continua pendente; aparelho conectado ao carregador.

A remoção do sample MR de peso zero também foi implementada/medida separadamente, com consumidor por especialização, mas a diferença de 1,29% e p95 pior não satisfizeram o aceite. Permanece opt-in e não integra o padrão. Evidências, hashes, limites e referências: `docs/validacao/play-opaque-2026-10-05/README.md`.

Confirmação adicional no APK final, em resolução nativa: A/B/B/A com GPU 25,3725→21,0754 ms (-16,9361%), FPS apresentado 35,88/36,25→42,84/43,02 e zero pixels diferentes. A família nova executou todos os 2.400 frames B; em A, nenhum. Mantidos os mesmos gráficos e pós, sem reutilização do editor. Ainda não satisfaz 60/120 FPS nesta condição; o próximo bloco deve atuar no custo remanescente de material/iluminação/sombras e pós, com atribuição separada, além de fechar rota dinâmica e potência à mesma cadência. Os números da escala 0,75 não são números de resolução nativa.

Sessão normal no APK final: 55,54 FPS apresentados em 30 s contínuos. Como a política adaptativa existente foi para escala 0,75/filtro 9, o número não é aceito como atribuição isolada da otimização. Joystick/olhar/Saltar/Stop foram conferidos, a cena e as configurações permaneceram com os mesmos hashes e o editor foi reaberto sem extras de bancada. Endurance térmico e 120 FPS continuam pendentes.

## M3/M5: especializacao de iluminacao e superficies solidas

Segundo bloco de shader aceito no Play: selecionar uma familia compilada para pontos sem sombra local somente quando todos os registros de luz efetivamente publicados comprovam essa condicao. O contrato solido anterior tambem permite eliminar os programas de agua/impostor. Qualquer spot, tile local, material com recorte ou dither retorna ao caminho geral, com reavaliacao por frame; falha da pipeline otimizada preserva o caminho geral e registra aviso. Sol, cascatas, luzes pontuais, materiais, geometria e pos permanecem iguais.

A primeira candidata, apenas cone/sombra local, ficou em -4,8806% de GPU com p95 pior e foi rejeitada isoladamente. A segunda alcancou GPU 21,5681 -> 19,3661 ms (-10,2099%) no POCO F7 em resolucao nativa, A/B/B/A, com zero pixels diferentes. FPS apresentado A 42,05/42,18; B 46,30/46,91. Nova familia em todos os 2.400 frames B; nenhum frame M7 reutilizado no Play. Evidencias: `docs/validacao/play-lighting-2026-10-05/README.md`.

Esse ganho e incremental sobre a familia opaca anterior, nao uma nova reducao de graficos. M3/M5 ainda nao estao concluidos: falta fechar rota com movimento e ampliar a atribuicao em mais vistas. Proximos alvos medidos sao o shader opaco restante (aproximadamente 15,6 ms nesta vista) e o pos (3,5 ms), separando geometria, trafego e shading antes de alterar filtros ou visibilidade. 120 FPS exige menos de 8,33 ms sustentaveis; este bloco ainda nao cumpre isso. M8: reducao de potencia/aquecimento a mesma cadencia permanece pendente, pois o usuario escolheu continuar com carregador conectado. Nao confundir temperatura de bateria nem FPS adaptativo com aceite de eficiencia energetica.

APK final deste bloco instalado com hash conferido. Sessao normal: 61,9649 FPS apresentados / minimo por segundo 59, politica existente em escala 0,75/filtro 9. Este dado e aceite de integracao, nao medicao de ganho em resolucao nativa. Nova familia em todos os 1.800 frames das tres janelas, nenhum frame reutilizado no Play. Joystick/olhar/Saltar/Stop e hashes da cena/configuracao conferidos; editor reaberto sem extras. Evidencia e limites no ledger do bloco.
