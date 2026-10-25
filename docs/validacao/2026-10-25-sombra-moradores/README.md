# Sombra dos moradores no mapa dinâmico — Vila S, POCO F7 (25/10/2026)

## Resultado final (cortes-1426, 14:57–15:04)

**Retificação.** A tabela e a prancha de 03:47 abaixo não valem: os commits 48f2cda3 e
1db82cd9 mudaram `environment_lighting.glsl` sem regenerar os `*_spirv.h`
(`tools/generate-embedded-shaders.ps1 -All`). O aparelho nunca leu o atlas dinâmico; os
moradores ficavam sem sombra e o "custo do dinâmico" medido não incluía a leitura no shader.
O par congelado de 04:12 (`par-congelado/`) mostrou a falta; `par-congelado-diag/` provou que
redesenhar o mapa a cada quadro não mudava nada.

Duas causas, duas correções:

1. SPIR-V regenerada (todas as famílias passam em `-All -Check`).
2. Mapa dinâmico nas **4 cascatas**, cada uma no retângulo justo dos moradores. Com só as
   cascatas 0–1, o morador a 15–30 m saía do atlas estático e não entrava no dinâmico. A
   leitura dinâmica é uma busca só, na cascata do fragmento (fora da mistura entre cascatas);
   a linha de baixo do atlas só é limpa e gravada quando há morador nas cascatas 2–3.

As quedas no Play de 05:03–07:00 (`crash-*`) eram o estouro de pilha da base
(`AndroidShell` na pilha da thread do jogo; o `InstancedRenderer` cresceu), corrigido na base
(09756295, trazida em d4d39678). Os tombstones de compilador Adreno lidos ali eram de 23/10
(clima), não destas quedas. A mesclagem com o G3c exigiu +1 combined image sampler no pool
do conjunto de ambiente (c83be2c0).

**Par congelado** (`par-congelado-final/`, Play pausado, só `debug.astra.shadow_dirty_all`
trocando): 1×3 e 2×4 idênticos; dinâmico × atlas único difere em 0,027% dos pixels
(>16 níveis), só em fios de borda das sombras dos moradores (texel do retângulo justo é mais
fino que o da cascata). Nenhuma sombra falta; a sombra longa do Viking aparece nos dois.
Prancha: `prancha-sombra-moradores-final.jpg`. As capturas repetidas (3 e 4 de cada par, idênticas
pixel a pixel às 1 e 2) e as telas das rodadas que caíram (`par-congelado-4cascatas`,
`par-congelado-spirv`) não foram guardadas; os `log.txt` ficam.

**Custo** (`final/vila-s`, A/B/A/B intercalado, mesmo binário):

| Configuração | Sombra GPU (ms) | GPU quadro (ms) | FPS interno | CPU processo (ms) |
|---|---|---|---|---|
| Atlas único | 2,63 / 2,65 | 39,38 / 39,50 | 23,3 / 23,1 | 19,53 / 19,80 |
| **Dinâmico, 4 cascatas (padrão)** | **1,37 / 1,37** | **38,59 / 38,64** | 23,9 / 24,0 | 19,59 / 18,58 |

Sombra −1,27 ms, GPU do quadro −0,8 ms. O GPU do quadro está ~8 ms acima das rodadas da
madrugada: a base agora traz clima e lightmap (G3c/G4c), e a temperatura não foi controlada
entre as duas sessões; o A/B intercalado é o que vale.

---

## Histórico (não vale como resultado — shader não embarcado)


APK cortes-0332, mesmo binário, rodadas intercaladas (35,0 → 40,4 °C). Atlas único =
`aether.shadow_dirty_all=true`; dinâmico = fit justo aos moradores + o mesmo PCF do estático,
lado forçado por `aether.shadow_dynamic_resolution`.

| Configuração | Sombra GPU (ms) | Opaco | GPU quadro | FPS interno |
|---|---|---|---|---|
| Atlas único | 2,86 / 2,67 | 13,59 / 13,25 | 31,80 / 30,76 | 28,3 / 28,8 |
| Dinâmico 1024 | 0,67 / 0,72 | 13,72 / 13,17 | 29,60 / 28,79 | 30,6 / 31,2 |
| **Dinâmico 2048 (padrão no S)** | **0,92 / 0,94** | 13,15 / 13,20 | **29,04 / 29,06** | 31,8 / 31,4 |

Prancha `prancha-sombra-moradores.jpg`: com 2048 e fit as sombras dos moradores voltam com o contraste
do atlas único (recorte ampliado dos pés e das sombras longas); a sombra estática é idêntica. A causa da
sombra fraca de 24/10 era o desvio de profundidade ×4 e a busca única; o fit mantém a profundidade e o
desvio da cascata. Sombra ≤ 1 ms, opaco sem custo extra visível, GPU do quadro −1,7 a −2,8 ms.
