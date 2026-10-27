# Sombra do sol em dois mapas — Vila S, POCO F7 (24/10/2026)

> **Não vale como resultado (retificado em 25/10).** O commit mudou `environment_lighting.glsl` sem
> regenerar os `*_spirv.h`: o aparelho não lia o mapa dinâmico. Medida real em
> `../2026-10-25-sombra-moradores` (sombra 2,64 → 1,37 ms).

APK cortes-2100, mesmo binário; atlas único = `aether.shadow_dirty_all=true`. Rodadas intercaladas, 42–43 °C.

| Rodada | Sombra GPU (ms) | Opaco | GPU quadro | FPS interno | Thread | Processo |
|---|---|---|---|---|---|---|
| 1 atlas único | 2,77 | 13,26 | 31,32 | 28,6 | 13,45 | 15,49 |
| 2 dois mapas | **0,66** | 13,68 | **29,85** | 30,3 | 12,98 | 15,42 |
| 3 atlas único | 2,73 | 13,44 | 31,43 | 28,8 | 12,19 | 14,38 |
| 4 dois mapas | **0,59** | 13,59 | **29,70** | 30,6 | 12,43 | 14,75 |

Sombra −2,1 ms (−77%), opaco +0,2–0,4 ms (segunda leitura), GPU do quadro −1,6 ms; CPU neutra.
`[ShadowCache]`: 240 cascatas em 120 quadros = só as 2 cascatas perto do mapa dinâmico (30 desenhos
de moradores por quadro); o atlas das cascatas fica inteiro em cache.

Imagem (`prancha-sombra-dois-mapas.jpg`): sombra estática igual; as sombras dos moradores perto ficam
mais fracas e menos marcadas (dinâmico de 1024 px, uma busca filtrada). Próximo ajuste: busca 2×2 no
dinâmico ou 2048 no S; os moradores longe deixam de projetar (cascatas 2 e 3), como o Shadow Casting
Distance da URP.
