# Sombra dos moradores no mapa dinâmico — Vila S, POCO F7 (25/10/2026)

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
