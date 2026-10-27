# G4d — Volumetric Clouds no modelo do HDRP, comparadas com a Unity

Validação no POCO F7 (Vila Astra), prancha a prancha. Cada `prancha-depois-vN.png`
põe lado a lado um frame de vídeo oficial da Unity e a captura do aparelho no
mesmo tipo de céu; o título traz o custo de GPU medido por A/B (com e sem nuvens).

## Referências da Unity (frames em `referencia-unity/`)

| Arquivo | Vídeo | Tempo | Uso |
|---|---|---|---|
| `..._0LTnOaeDbuQ_t13s.jpg` | Unity, "HDRP Volumetric Clouds" — https://www.youtube.com/watch?v=0LTnOaeDbuQ | 0:13 | meio-dia, cúmulos largos |
| `..._95OOpQPJKU0_t03s.jpg` | "Volumetric Clouds vs Cloud Layer Test" — https://www.youtube.com/watch?v=95OOpQPJKU0 | 0:03 | cúmulos pequenos até o horizonte |
| `..._0LTnOaeDbuQ_t52s.jpg` | "HDRP Volumetric Clouds", trecho Physically Based Sky interaction | 0:52 | hora dourada (sol a poucos graus) |
| `..._95OOpQPJKU0_t33s.jpg` / `..._0LTnOaeDbuQ_t54s.jpg` | os dois vídeos acima | 0:33 / 0:54 | pôr do sol, sol no horizonte |
| `..._0LTnOaeDbuQ_t46s.jpg` | "HDRP Volumetric Clouds" | 0:46 | cúmulos grandes (cobertura 0,6–0,7) |

## De onde veio cada parte

- **Horizon Zero Dawn / HDRP** (Schneider, "The Real-time Volumetric Cloudscapes
  of Horizon Zero Dawn", SIGGRAPH 2015; Volumetric Clouds do HDRP 17): casca
  esférica entre base e topo, mapa de clima 2D para a cobertura, ruído 3D
  Perlin-Worley para a forma e Worley para a erosão, perfil de cúmulo com base
  plana e topo arredondado, Beer + powder, oitavas de espalhamento múltiplo
  (Wrenninge), 1/16 dos pixels com acúmulo temporal. Código escrito do zero em
  `native/rhi/shaders/cloud_volume.frag`; ruídos gerados por `native/renderer/cloud_noise.cpp`.
- **Hillaire 2020** ("A Scalable and Production Ready Sky and Atmosphere
  Rendering Technique", padrão do Physically Based Sky do HDRP): absorção de
  ozônio (0,650 / 1,881 / 0,085)·10⁻³/km, em `dirt_road_sky.frag` e
  `renderer/physical_atmosphere.cpp`.
- **Enviro 3.2.1a** (Hendrik Haupt; licença company do dono), estudado, nada
  copiado: `Scripts/Runtime/Modules/Lighting/EnviroLightingModule.cs` e o
  `Default Lighting Preset.asset`. A luz do dia vem de curvas por hora solar; a
  exposição do HDRP é fixa e sai de `sceneExposure` (EV100 12,7 ao meio-dia,
  ~11,2 com o sol a 8°, ~10 no horizonte): a cena fica 1,5 EV mais escura na
  hora dourada e 2,7 EV no pôr do sol, em vez de a exposição automática levar
  tudo ao cinza médio. Na Astra isso entrou como limite mínimo da exposição
  automática das variantes (0 na hora dourada, −1,2 no pôr do sol, na escala
  EV100 da Astra, que mede ~1,5 ao meio-dia). A temperatura do sol do Enviro
  (2850 K no horizonte → 5500 K ao meio-dia) não foi usada: na Astra a
  atmosfera física já tinge o sol.
- **UniStorm 5.3.1** (Black Horizon Studios; licença company do dono):
  - v3: perfil de altura de `cloudMapBase`/`cloudGradient` (substituído na v4 pelo do HZD);
  - ambiente topo/base de `renderCloudsInternal` (`dirt_road_sky.frag`);
  - gradientes `CloudLightColor`/`CloudBaseColor` do prefab do sistema: na hora
    dourada (18h12) o lado iluminado vai a (1,0; 0,70; 0,55) e a base continua
    (0,28; 0,34; 0,39). A base das nossas nuvens passou a ser dessaturada e
    fria, sem herdar o amarelo do céu (`dirt_road_sky.frag`).
- **Clima (g4c)**: tom cinza-azulado (~7000 K) do céu fechado e luz espalhada
  pela sombra das nuvens, trazidos com o merge da base (`instanced_renderer.cpp`).

## Ajustes de cena só nas variantes de captura

A Vila foi feita para fim de tarde. Nas variantes (não no projeto): sol a 6000 K
(era 4600 K), exposição automática por histograma (era fixa em −0,41), neblina em
"Cor" (em "Acompanhar o céu" ela cobria o próprio céu e deixava o zênite
verde-acinzentado) e camada de 1200 m.

## Custo de GPU (quadro inteiro, A/B com e sem nuvens)

| Versão | Sem nuvens | Com nuvens | Custo |
|---|---|---|---|
| v1 (1/16 dos pixels) | 3,7 ms | 5,0 ms | +1,3 ms |
| v3 (meia resolução, 1 de cada 2x2) | 3,7 ms | 12,8 ms | +9,1 ms (o quad inteiro paga a marcha) |
| v4 (1/16 dos pixels) | 3,9 ms | 5,6 ms | +1,7 ms |
| v5 (luz mais barata, temporal com rejeição) | 3,87 ms | 5,33 ms | +1,46 ms |
| v7 (reconstrução em meia resolução, forma do emulador) | 4,36 ms | 5,83 ms | +1,48 ms |

## v5 no POCO (APK 0.3.0-dev.261025999-nuvens-0356, 25/10)

`prancha-depois-v5.png`: câmera levantada para o céu no Play (arrasto na área de
olhar), quadros em `poco-v5/` (`-1` olhando o céu; `meio-dia-2` e `grandes-3`
girando: o acúmulo temporal não deixa rastro visível).

- Meio-dia, horizonte e cúmulos grandes: base plana, topos arredondados, zênite azul.
- Hora dourada e poente: céu laranja-avermelhado, zênite quase preto (40, 0, 0).
  Causa: a Hora do dia já tinge e atenua o sol (2000 K no horizonte, 0,35x) e o
  céu físico aplica a própria transmitância por cima, então o ar avermelha duas
  vezes. A correção (sol fora da atmosfera só para o céu físico e a irradiância
  do chão) está escrita e ainda não foi compilada nem capturada.
- O limite de exposição das variantes estava na escala errada (o meio-dia da Astra
  mede -0,8 EV100, não +1,5): as capturas da v5 com limite 0 e -1,2 saíram pretas.
  As de agora usam -3,3 (hora dourada) e -4,5 (poente).

## v6 no POCO (APK 0.3.0-dev.261025999-nuvens-2043, 25/10)

`prancha-depois-v6.png`, quadros em `poco-v6/`.

- Hora dourada e poente: o céu físico recebe o sol de fora do ar (a Hora do dia
  já tinge e atenua o sol da cena). Zênite da hora dourada (40, 71, 91), antes
  (40, 0, 0); poente com zênite cinza quente e a faixa laranja no horizonte. As
  nuvens ficam pêssego do lado do sol, como a Unity em 0:52.
- Nitidez: marcha em 1/16 dos pixels com subpixel por quadro e reconstrução em
  meia resolução (cloud_resolve.frag). A borda arredondada ficou mais definida,
  mas a nuvem grande segue lisa por dentro: o borrão vem mais da forma (ruído
  de baixa frequência, erosão fraca no miolo visível) do que da resolução.
- Horizonte: ruído limitado a 2 mips e corte do céu a ~0,7°. Na captura ainda
  há poucos cúmulos pequenos perto do horizonte; não melhorou de forma visível.
- Custo: o A/B não rodou (o adb caiu no fim da rodada).

## v7 no POCO (APK 0.3.0-dev.261025999-nuvens-2154, 25/10)

Forma ajustada no emulador de CPU do shader (`emulador-v7-unity.png`, variante J)
antes de gastar o aparelho; `prancha-depois-v7.png`, quadros em `poco-v7/`.

- Causa do aspecto de blob: o mip da erosão estava 2 níveis borrado demais
  (+4,58 em vez de +2,58; o texel da erosão é 6x menor que o da forma).
- Meio-dia: cúmulos com topo em couve-flor e sombra interna.
- Horizonte: dezenas de cúmulos pequenos até a linha do horizonte.
- Cobertura 0,68: nuvens separadas com céu azul entre elas.
- Regressão: na hora dourada e no poente a nuvem inteira fica laranja
  saturado (na v6, aprovada, era pêssego). A base mais escura (0,45x) e o od
  menor deixaram a luz do sol dominar a cor.
- Custo: +1,48 ms (meta 1,5 ms).

## v8 no POCO (APK 0.3.0-dev.261026999-nuvens-0915, 26/10, com a base a06d4e30)

`prancha-depois-v8.png` (pares 3 a 5), quadros em `poco-v8/`. O Play abriu nas
três cenas sem crash (buffer de crash vazio, sem [Exit]).

- Riscos: com o histórico contido pelos 4 texels da marcha e a rejeição por
  movimento, as estrias na borda das nuvens grandes ao girar sumiram
  (`poco-v8/riscos-v7-x-v8.jpg`, v7 à esquerda).
- Hora dourada e poente: a marcha de luz esticada e o ambiente cinza-arroxeado
  não aparecem o bastante no aparelho; a nuvem segue laranja quase uniforme.
  No emulador o lado da sombra ficava cinza-arroxeado; a diferença vem da
  exposição do aparelho, que o emulador não reproduz.
- Cúmulos grandes: a cena saiu mais exposta que na v7 (céu azul-marinho, nuvem
  branca estourada e chapada). A cor nova só atua com o sol baixo; a suspeita é
  a exposição/teto de qualidade trazidos com a base. Não confirmado.
- Custo: não medido nesta rodada (as mudanças só valem com sol baixo e no
  histórico; a v7 mediu +1,48 ms).

## v9 no POCO (APK 0.3.0-dev.261026999-nuvens-0941, 26/10) — fechamento

`prancha-depois-v9.png` (cinco pares; o horizonte é da rodada da v7, anterior à
colisão abaixo), quadros em `poco-v9/`. Play aberto nas quatro cenas sem crash.

- Causa das nuvens chapadas da v8: o Cached Shadows do G1b gravava o flag do
  atlas dinâmico em `cloudShadow.y`, que nesta branch é a espessura da camada
  (Ambiente v20). Com o flag em 0/1 a camada caía para 200 m: sem volume, sem
  couve-flor, sem autossombra. Na base sozinha o campo estava livre; a colisão
  só existe no merge. O flag passou para `shadowDynamicParameters.y` (do próprio
  G1b), lido em `environment_lighting.glsl`; teste de host
  `cached_shadow_flag_keeps_the_cloud_thickness`.
- Hora dourada: lado do sol dourado, lado oposto e base cinza-arroxeados,
  com volume (Unity 0:52). Poente: lado iluminado amarelo-dourado, sombra
  malva-acinzentada (0:54). Exposição medida: −2,63 EV100 com limite −2,3;
  poente −4,48 com limite −3,5; meio-dia −0,65 (limites livres).
- Cúmulos grandes e meio-dia: couve-flor com sombreamento interno, como a v7.
- Estrias na borda ao girar: resolvidas na v8 (`poco-v8/riscos-v7-x-v8.jpg`).
- Custo: o da v7, +1,48 ms; a v8/v9 não mudou a marcha ao meio-dia (a marcha
  esticada só vale com o sol baixo) e acrescentou só leituras ao histórico.

Pendência registrada: a sombra das nuvens no chão usa o alfa da camada 2D na
escala dela, não a mancha volumétrica; não casa exatamente com as nuvens.

## v10 no POCO (26/10): trepidação e lua

APK de controle 0.3.0-dev.261026999-nuvens-1136 (o binário novo com os SPIR-V
das nuvens de cad5805c, as mesmas nuvens do 0941) × 0.3.0-dev.261026999-nuvens-1121.
Quadros em `poco-v10/`.

- Trepidação, câmera parada olhando o céu, vento zero, quadros seguidos (níveis
  de 8 bits, desvio padrão por pixel na faixa do céu):
  - antes (51 quadros válidos; do 52º em diante a imagem inteira mudou, a
    câmera saiu do lugar, e esses foram descartados): média 0,23, p95 0,81,
    p99 3,29; só nas nuvens 0,47 (p99 2,81);
  - depois (60 quadros): média 0,14, p95 0,48, p99 1,85; só nas nuvens 0,25
    (p99 1,84).
  Queda de ~40-50%. A screencap pega ~1 quadro por segundo: mede o ruído de
  quadro a quadro, mas não reproduz o tremor de 30 fps visto ao olho.
- Lua (mesma noite de lua cheia, dia 168/169, com uma direcional Lua ligada ao
  Sol como na receita Dia/Noite): o disco aparece às 4 h, baixo no oeste
  (`poco-v10/lua-4h-disco.jpg`). Às 20 h e às 0 h o giro da câmera (olhar
  erguido ~20°) não enquadrou o disco: à meia-noite a lua cheia passa a ~8°
  do zênite e às 20 h está baixa no leste, atrás das casas e das nuvens. A
  luz quase não muda entre as três horas: a exposição automática leva a noite
  a ~0,6 EV100 e a Lua (0,35) pesa pouco perto do céu noturno. O movimento da
  lua está provado no host (teste da lua cheia de 19 h a 5 h e ~50 min de
  atraso por dia), não ainda numa prancha do aparelho.

## v11 no POCO (26/10) — prancha final e fechamento

`prancha-final.png`: pares de cor Unity × POCO (hora dourada e poente, APK
0.3.0-dev.261026999-nuvens-1121), a noite no aparelho e o mapa do tremor.
Quadros e vídeos em `poco-v11/`. Vila devolvida com md5 2e2e9968 ao fim de
cada rodada.

- Tremor, agora no vídeo (5 s a 30 fps, `screenrecord`, câmera parada, vento
  zero), controle 1136 × 1121: diferença média entre quadros seguidos nas
  nuvens 0,16 → 0,07 níveis de 8 bits (−56%), p99 1,64 → 1,16, desvio por
  pixel 0,35 → 0,24 (`tremor-controle.mp4`, `tremor-1121.mp4`, `tremor.png`).
  Técnica: jitter da marcha por ruído de gradiente intercalado somado à razão
  áurea × contador real de quadros; histórico com 85% de peso com a câmera
  parada; caixa de contenção a 25% da faixa parada e 5% girando; ruído das
  nuvens em coordenadas de mundo.
- Lua por efeméride: disco visto com céu limpo à 0 h (perto do zênite) e às 4 h
  (baixo no oeste). Às 20 h, baixo no leste, ficou atrás das casas no giro
  usado; o movimento das 19 h às 5 h está no teste de host.
- Nuvens à noite (APK 1444): a marcha de luz usa a direcional dominante (o sol
  de dia, a lua à noite, cruzando no crepúsculo) com a cor e a intensidade
  dela, e o ambiente do céu noturno da V-2 entra na base. Com lua cheia as
  nuvens ficam cinza-arroxeadas contra o céu azul-luar, com a borda do lado da
  lua mais clara (0 h). Exposição 2,05 EV100.
- Lua nova (APK 1550): antes o piso das estrelas era uma fração do ambiente da
  meia-noite, que já chega quase zerado; o céu ficava 11 EV abaixo da lua
  cheia, a exposição ia ao piso (−8,3) e as luminárias e telhados estouravam
  em branco (`v20`, fora da prancha). Agora o brilho do ar tem piso absoluto
  (`NightStarlightFloor`, ~1/20 do céu sob a Lua padrão de 0,5 lux): céu
  noturno 0,056/0,075/0,124 contra 1,62/2,17/3,61 com lua cheia (5,3 EV
  abaixo), exposição −2,85 EV100, nuvens escuras recortadas contra o céu.
  A exposição automática devolve boa parte da diferença; para uma lua nova
  visivelmente mais escura, o limite mínimo de exposição do Volume decide,
  como no HDRP.
- Suíte: 1725/1725 nativos, 105/105 do ambiente, contratos sem mudança.

Pendência registrada: sombra das nuvens no chão ainda pela camada 2D (v9).

## v12 no POCO (27/10) — a noite volta a ser noite

`prancha-noite.png`, quadros e medidas em `poco-v12/` (APK
0.3.0-dev.261026999-nuvens-0846; medidas com `tools/validation/measure-night-frames.py`,
viewport sem a barra do editor). Pedido do dono: a noite da prancha final parecia
dia nublado claro, a queixa original da V-2.

Causa, nas variantes de captura do G4 (não no motor):
- a Lua era um clone do Sol com 0,8 na unidade Engine do Sol, ~1100× a Lua da
  receita (0,5 lux = 7,3e-4 na escala do shader);
- o Volume mantinha o Limite mínimo −8,26 EV100 herdado da Vila, e a exposição
  automática levava a noite ao cinza médio (o defeito 01 da V-2);
- o conversor das variantes gravava a neblina em Cor fixa do dia; a Vila migrada
  pelo app fica em Acompanhar o céu, que apaga à noite. A cor do dia vezes a
  exposição da noite virava telhados, copas e o horizonte brancos.
- O `NightStarlightFloor` do 5f332696 tinha sido calibrado nessa escala errada
  (1,5e-3, acima do próprio luar); agora é 1/20 do céu sob a Lua de 0,5 lux na
  escala do shader (`0,5/683 · 0,2/π / 20`), com o teste ajustado.

Variantes refeitas como a receita Sol com dia e noite e a V-2: Lua de 0,5 lux,
8500 K; Exposição automática com Limite mínimo do Volume da noite; neblina
Acompanhar o céu. Exposição medida −7,7 EV100 com lua cheia e −10,4 na lua nova,
segura no limite.

| Limite mínimo | Lua cheia: quadro / céu / chão (sRGB) | abaixo do meio-dia | Lua nova: quadro | abaixo do meio-dia |
|---|---|---|---|---|
| −6 (padrão da Astra) | 56 / 79 / 32 | 2,5 EV | 6,2 | 6,9 EV |
| **−5,5 (final)** | **37 / 55 / 12** | **3,6 EV** | **3,9** | **7,6 EV** |
| −5 | 21 / 34 / 4 | 5,0 EV | 2,7 | 8,1 EV |

Meio-dia de referência: `poco-v9/meio-dia-1.jpg`, luminância média 0,227. A
recusada (`poco-v11/noite-lua-20-praca-1444.jpg`) media 0,252, o mesmo que o dia.
Com −5,5: céu azul-escuro com estrelas, nuvens cinza recortadas, chão legível e
fraco, postes e janelas acesos; lua nova quase preta, só postes e estrelas.
A V-2 aprovada (sem nuvens, −6) mede quadro 66: as nuvens cobrem parte do céu
e o limite −5,5 escurece mais.

Para o ciclo de dia e noite numa cena do usuário vale o mesmo que na Unity: a
Vila do dono continua com o limite −8,26 herdado; no Volume da noite, Limite
mínimo perto de −5,5 EV100.
