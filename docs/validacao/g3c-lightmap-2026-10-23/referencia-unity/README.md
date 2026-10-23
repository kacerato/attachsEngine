# Referência Unity em vídeo — G3c (Progressive Lightmapper e janela Lighting)

Frames extraídos com `ffmpeg -ss <t>` dos vídeos abaixo (720p), usados para julgar o fluxo e o resultado
antes de dar o bloco por pronto. São capturas de vídeos públicos do YouTube, citadas como referência visual;
não fazem parte do produto.

| Arquivo | Vídeo | Timestamp | O que mostra |
|---|---|---|---|
| U1 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=60s) | 1:00 | Interior sci-fi só com luz de tempo real: cantos e teto pretos, o ambiente do céu não entra na sala. |
| U2 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=120s) | 2:00 | Mesh Renderer › Lighting: Contribute Global Illumination e Receive Global Illumination (Lightmaps / Light Probes). |
| U3 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=195s) | 3:15 | Luzes passadas para Mode Baked na lista de luzes; a sala ainda sem bake. |
| U4 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=225s) | 3:45 | Lighting window › Scene: Lightmapper, Direct/Indirect/Environment Samples, Bounces, Lightmap Resolution, Padding, Max Lightmap Size, Compress, Directional Mode; a sala depois do Generate Lighting, com o rebote rosa nas paredes. |
| U5 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=270s) | 4:30 | Vista de resolução do lightmap (xadrez de texels por objeto). |
| U6 | [Unity — How to build Lightmaps in Unity 2020.1](https://www.youtube.com/watch?v=KJ4fl-KBDR8&t=0s) | 0:00 | Resultado final assado: luz indireta colorida nas paredes, sombra suave nos cantos. |
| U7 | [GameDev Blueprint — LIGHT BAKING in Unity (URP Version)](https://www.youtube.com/watch?v=XjRNWH8Eazo&t=375s) | 6:15 | Diagrama do rebote: luz que bate no chão laranja e tinge a estátua. |
| U8 | [GameDev Blueprint — LIGHT BAKING in Unity (URP Version)](https://www.youtube.com/watch?v=XjRNWH8Eazo&t=510s) | 8:30 | Configurações que só mudam o tempo de bake × as que mudam o desempenho do jogo. |
| U9 | [Mythmatic — Unity 6 Lightmapping Tutorial: Modular Room Workflow](https://www.youtube.com/watch?v=-nqZfFUzAL8&t=465s) | 7:45 | Parede com janela e beiral, por fora: o interior escuro até o bake. |
| U10 | [Mythmatic — Unity 6 Lightmapping Tutorial: Modular Room Workflow](https://www.youtube.com/watch?v=-nqZfFUzAL8&t=645s) | 10:45 | Quarto assado na Unity 6: a luz que entra pela janela rebate no chão e clareia paredes e móveis. |

## O que se tira dos frames

- Antes do bake (U1, U9) o interior fica com o ambiente de fora ou preto; depois (U4, U6, U10) a luz rebate e
  as paredes ganham a cor do que está em volta. O canto e o teto continuam mais escuros que a parede aberta.
- A Unity organiza o bake na aba Scene da Lighting window (U4): primeiro o que muda o tempo de bake (amostras,
  rebotes, filtro), depois o que muda o jogo (resolução, padding, tamanho máximo, compressão, modo direcional).
- O renderer decide por objeto (U2): só o que contribui e recebe lightmap entra nele; o resto vai para as sondas.
- A luz em modo Baked (U3) some do tempo real nos objetos assados; o resultado mostra a luz e o rebote juntos.
