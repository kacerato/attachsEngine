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
