# PK-9 — Material como na Unity 6/URP (T-01..T-05) — validação de 20/10/2026

Pacote da auditoria full Unity (`docs/planos/auditoria-full-unity-2026-10-19.md`, seção "Materiais"), branch
`claude/beautiful-kalam-695ced`, base `9fbbbe57`.

Referências (Unity 6000.0, URP 17):
[Lit shader](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html),
[Complex Lit](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/shader-complex-lit.html),
[Simple Lit](https://docs.unity3d.com/6000.2/Documentation/Manual/urp/simple-lit-shader.html),
[Blending modes](https://docs.unity3d.com/6000.1/Documentation/Manual/urp/blending-modes.html),
[Particles Lit](https://docs.unity3d.com/6000.1/Documentation/Manual/urp/particles-lit-shader.html).

## Como cada item funciona na prática na Unity (e o que a Astra faz)

| Item | Unity 6/URP, no Inspector do Material | Astra |
|---|---|---|
| T-01 | O campo **Shader** no topo troca o Inspector inteiro: Lit, Complex Lit (Lit + Clear Coat), Simple Lit (Blinn-Phong), Unlit e Particles Lit/Unlit. Cada um mostra só as propriedades que lê. | `MaterialAsset` v8 guarda o shader; o Inspector do material do projeto (cartão "Shader", lista com os 6) mostra só os campos que o shader lê (tabela única `editor/editor_material_fields.h`). |
| T-02 | **Smoothness** 0..1 com **Source** Metallic Alpha / Albedo Alpha (no Specular: Specular Alpha). | O arquivo continua em rugosidade (sem migração de dado); Inspector e API C# mostram Suavidade = 1 − rugosidade e a Origem (alfa do mapa, alfa da cor base, ou rugosidade glTF para modelos glTF). |
| T-03 | Height Map (paralaxe, G, 0,005..0,08), Detail Inputs (Mask no A, Base Map ×2 com visibilidade, Normal Map com força, Tiling/Offset), Clear Coat (Mask, Smoothness, mapa R/G) e **Workflow Mode** Metallic/Specular. | Altura por um passo de paralaxe (URP `ParallaxOffset1Step`), detalhe ×2 no espaço da Unity (cinza sRGB 0,5 neutro), normal de detalhe reorientado, verniz dielétrico F0 0,04 sobre a camada de baixo (sol + ambiente), workflow especular com o RGB do mapa como cor especular. |
| T-04 | Surface Type Opaque/Transparent; Blending Mode Alpha/Premultiply/Additive/Multiply; Preserve Specular; Render Face Front/Back/Both; Alpha Clipping + Threshold; Sorting Priority (fila 3000 + prioridade). | Pipelines de mistura próprias (ONE/1−SA, ONE/ONE, DST_COLOR/ZERO); Render Face Back descarta a frente; recorte também no transparente; prioridade ordena os transparentes antes da distância. |
| T-05 | O `.mat` do Standard/URP traz `_PARALLAXMAP`, `_DETAIL_MULX2`, `_ALPHAPREMULTIPLY_ON`. | O importador aceita os três e lê também URP Lit/Simple Lit/Unlit/Particles e o Standard (Specular setup). |


## Testes de host

- `aether_unity_material_tests` 35/35 (novo `test_material_shading_pk9.cpp`, inclui o teste que exige highp em constante inteira acima de 16 bits nos shaders com mediump int: v8 e migração v7, flags e linhas
  13..20 da extensão, campos visíveis por shader, Smoothness, importador Standard/URP, Inspector por toque com
  slider que só grava ao soltar, e o gerador do projeto de validação).
- `aether_tests` 1635/1635 (com G2, PK-6 e PK-7 mesclados); suíte gerenciada 523/523; `generate-component-contracts.py --sync-api`: API atual.
- Três testes antigos do editor foram reescritos para os campos novos (a lista paginada do material do projeto saiu).

## Aparelho (POCO F7, Astra Dev 186/187, Play com câmera fixa)

Projeto `PK9 Materiais` gerado no host (`ASTRA_PK9_PROJECT`) com o Cenário de referência (sol físico, exposição),
texturas procedurais próprias e materiais `.mat` URP convertidos pelo importador; variantes só mudam a câmera.

| Captura | O que mostra |
|---|---|
| 01–04 | Projeto aberto; Inspector do material (URP/Lit, Transparente, Mistura, Faces, Mapa base); lista dos 6 shaders; troca para Simple Lit ao vivo (Fluxo some, Mapa/Cor especular aparecem, rev. 2, desfeito) |
| 05 | Grade Metálico 0→1 × Suavidade 0→1 em esferas e cubos |
| 06 | Tijolo: mapa normal, altura (paralaxe) e detalhe ×2 |
| 07 | Transparentes vistos por trás: vidro com Preserve Specular, aditivo; **cubo Multiplicar errado** |
| 08 | Sem verniz × verniz (Complex Lit) do lado do sol |
| 09 | Antes da correção: GGX com suavidade 1 estourava o fp16 e virava um ponto preto |
| host-* | Captura do Inspector no host (Lit e Simple Lit) |

`prancha-pk9.png`: frames da Unity (pasta `referencia-unity/`, com vídeo e tempo no nome) × capturas do POCO.

### Julgamento contra a Unity (APK 0.3.0-dev.261022999-pk9-1245, base com G2, PK-6 e PK-7)

`prancha-pk9.png`: os mesmos frames da Unity à esquerda (vídeo e tempo no nome, ou a página da documentação),
o POCO à direita, e uma linha por item com o que ainda difere.

| Item | Resultado no POCO | O que ainda difere |
|---|---|---|
| Metálico × Suavidade | Sob HDRI de campo (Hausdorf Clear Sky, Poly Haven, CC0; captura 10) o metal liso reflete céu e chão com o mesmo contraste das esferas URP Lit do LlamAcademy em campo aberto. Sob o céu procedural claro do cenário (11) a reflexão é de um céu quase uniforme. | Nada de material visível. O realce do sol na suavidade 1 mostra um padrão quadriculado de poucos pixels (12): pendente. |
| Verniz (Complex Lit) | Sob o HDRI (13) o verniz reflete céu e horizonte numa camada lisa, com realce nítido por cima da base; sem verniz fica fosco, como no Clear Coat da Unity. | Luzes adicionais não acendem o verniz (declarado na capacidade). |
| Tijolo: normal, altura, detalhe ×2, máscara, AO | Chanfros e relevo do sol, grão do detalhe só fora do círculo da máscara, argamassa funda pela AO (14). | A cor da argamassa é a da textura de teste, mais clara que a foto do vídeo. |
| Multiplicar | O plano azul tinge céu, vidro e tijolo e escurece o que está atrás (15), como a imagem oficial de Blending Mode Multiply. | Nada visível. |
| Vidro, aditivo | Preserve Specular e aditivo com as saídas próprias (16). | O vidro do vídeo reflete um estúdio escuro; no cenário claro o reflexo é suave (ambiente). |

### O que a comparação encontrou e mudou

- **Constantes relaxadas na Adreno (causa do Multiplicar claro e de todo o PK-9 desligado no aparelho).**
  `dirt_road_shading.glsl` declara `precision mediump int`; as constantes declaradas depois saíam com
  `RelaxedPrecision`, e a Adreno as guarda em 16 bits. `MATERIAL_SHADING_EXTENSION` (1u<<20) virava 0: o shader do
  Material, a mistura própria, a altura, o detalhe e o verniz nunca rodavam no aparelho; o Multiplicar recebia a
  cor HDR e clareava (17). Diagnóstico com shader de teste: a CPU enviava `0x10a001`, a GPU via o bit 20 apagado
  (18, ciano = R zero). Correção: constantes em highp (`5b1b938c`), conferida no SPIR-V de todos os shaders. O mesmo
  defeito desligava `DRAW_NO_SHADOWS`/`DRAW_NO_SUN` (bits 17 e 18) no aparelho.
- **Verniz com aro escuro**: o reflexo do verniz vinha da LUT de split-sum e a atenuação da base, de Schlick puro;
  na silhueta a base sumia sem ganhar reflexo. Agora segue a URP (`EnvironmentBRDFClearCoat`, mesmo fresnelTerm).
- **Brilho de suavidade 1 virava ponto preto** (09): GGX em highp e limitado antes do fp16.
- **Projeto de validação**: AO procedural no tijolo e opção `ASTRA_PK9_HDRI` no gerador para a comparação justa do metal.

## Custo no aparelho

POCO F7, Astra Dev 232 (difere do código entregue só no ramo de teste do Multiplicar e num log de CPU), Play com
câmera fixa, resolução dinâmica e qualidade travadas, `aether.profile_frames`, 3 janelas de 10 s por variante,
bateria 36,9→37,7 °C. Variantes do mesmo projeto, só trocando o shader dos 32 materiais da grade:

| Variante | gpu_frame_ms (mediana das janelas) | p90 | fps |
|---|---|---|---|
| Lit | 13,20 | 13,62–13,95 | 60 |
| Complex Lit (verniz ligado) | 13,20 | 13,63–16,66 | 60 |
| Simple Lit | 13,20 | 13,67–13,88 | 60 |
| Lit (repetição) | 13,20 | 13,66–14,01 | 60 |

Diferença abaixo de 0,02 ms: a grade cobre pouco da tela e o quadro é dominado pelos outros passes. Isto mostra que
o PK-9 não pesa nesta cena; não mede o pior caso (um material de verniz cobrindo a tela inteira), que fica pendente.
Uma rodada anterior foi descartada porque outra frente instalou o APK 220 durante a medição.

## Mudanças anteriores (186/187)

- Brilho especular calculado em highp e limitado antes do fp16 (ponto preto da captura 09).
- Multiplicar passou a usar a tinta da superfície em vez da cor iluminada (a cena é HDR antes da exposição).
- Verniz e transparentes recapturados do lado do sol.
