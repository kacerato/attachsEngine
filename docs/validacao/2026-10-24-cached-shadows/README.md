# Cached Shadows (split estático/dinâmico) — Vila S, POCO F7 (24/10/2026)

APK cortes-1531, mesmo binário; atlas único = `aether.shadow_dirty_all=true`. Rodadas intercaladas.

| Rodada | Sombra GPU (ms) | GPU quadro | Thread do jogo | Processo | Bateria °C |
|---|---|---|---|---|---|
| 1 atlas único | 2,85 | 31,45 | 12,43 | 14,58 | 40,9 |
| 2 split | 5,77 | 34,67 | 10,28 | 11,97 | 41,8 |
| 3 atlas único | 2,70 | 31,36 | 13,74 | 16,28 | 43,1 |
| 4 split | 5,66 | 34,23 | 14,22 | 16,70 | 44,2 |

Telemetria `[ShadowCache]`: 480 cascatas regravadas em 120 quadros (as 4 a cada quadro), todas por
**poses**: 15 projetores dinâmicos (moradores) espalhados pela Vila tocam as 4 cascatas. Com o split,
cada quadro copia o atlas estático inteiro (4 × 4096² D32 ≈ 256 MB) e desenha 60 dinâmicos; a cópia
custa mais que redesenhar os estáticos. CPU dentro do ruído térmico.

Decisão: split só com cascatas de até 1024 (nível C: 2 × 1024² ≈ 8 MB de cópia); A/B do C pendente.
