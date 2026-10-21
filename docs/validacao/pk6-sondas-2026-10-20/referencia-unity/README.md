# Referência Unity em vídeo — PK-6 (Reflection Probe e Light Probe Group)

Frames extraídos com `ffmpeg -ss <t>` dos vídeos abaixo (720p), usados para julgar reflexo,
paralaxe da caixa e transição de luz antes de dar o bloco por pronto. São capturas de vídeos
públicos do YouTube, citadas como referência visual; não fazem parte do produto.

| Arquivo | Vídeo | Timestamp | O que mostra |
|---|---|---|---|
| R1 | [Brackeys — REFLECTIONS in Unity](https://www.youtube.com/watch?v=lhELeLnynI8&t=120s) | 2:00 | Esfera metálica numa sala sem sonda: só o céu (preto aqui) reflete. |
| R2 | [Brackeys — REFLECTIONS in Unity](https://www.youtube.com/watch?v=lhELeLnynI8&t=220s) | 3:40 | Gizmo da caixa de influência (Box Size) cobrindo o ambiente; Inspector com Type, Importance, Box Projection, Blend Distance, Resolution, HDR, Clipping Planes. |
| R3 | [Brackeys — REFLECTIONS in Unity](https://www.youtube.com/watch?v=lhELeLnynI8&t=340s) | 5:40 | Depois do Bake: a esfera cromada reflete a sala (painéis brancos, chão). |
| R4 | [Brackeys — REFLECTIONS in Unity](https://www.youtube.com/watch?v=lhELeLnynI8&t=460s) | 7:40 | Sonda Realtime: o cubo laranja movido aparece no reflexo da esfera. |
| R5 | [Gamified — How to use Reflection Probes in Unity](https://www.youtube.com/watch?v=9-S1SYadqzE&t=160s) | 2:40 | Caixa amarela da sonda ajustada ao interior de um quarto. |
| R6 | [Gamified — How to use Reflection Probes in Unity](https://www.youtube.com/watch?v=9-S1SYadqzE&t=350s) | 5:50 | Box Projection ligado: a esfera reflete paredes, janela e quadro na posição certa da sala (paralaxe), não um céu no infinito. |
| L1 | [GameDev Blueprint — LIGHT PROBES in Unity (URP)](https://www.youtube.com/watch?v=3J9FpXqRoos&t=200s) | 3:20 | Light Probe Group em edição: esferas amarelas (sondas) e azuis (selecionadas) espalhadas na sala, com sol e sombra. |
| L2 | [GameDev Blueprint — LIGHT PROBES in Unity (URP)](https://www.youtube.com/watch?v=3J9FpXqRoos&t=260s) | 4:20 | Cápsula dinâmica no lado iluminado: corpo claro e tingido pelo rebate do chão vermelho. |
| L3 | [GameDev Blueprint — LIGHT PROBES in Unity (URP)](https://www.youtube.com/watch?v=3J9FpXqRoos&t=280s) | 4:40 | A mesma cápsula na sombra da parede: escurece pelas sondas; linhas amarelas ligam o objeto às 4 sondas do tetraedro. |
| L4 | [Brackeys — HIGH QUALITY LIGHTING using Light Probes](https://www.youtube.com/watch?v=_E0JXOZDTKA&t=400s) | 6:40 | Grupo com as arestas da tetraedralização visíveis. |
| L5 | [Brackeys — HIGH QUALITY LIGHTING using Light Probes](https://www.youtube.com/watch?v=_E0JXOZDTKA&t=450s) | 7:30 | Esfera branca no sol com as 4 linhas de interpolação (Mesh Renderer › Light Probes: Blend Probes). |
| L6 | [Brackeys — HIGH QUALITY LIGHTING using Light Probes](https://www.youtube.com/watch?v=_E0JXOZDTKA&t=470s) | 7:50 | A mesma esfera na sombra: o tom muda de forma contínua, sem salto. |

## O que se tira dos frames

- Reflexo sem sonda é o céu; com a sonda assada é a sala (R1 × R3). A troca do tom é forte em metal liso.
- A caixa é o objeto de edição principal da sonda: desenhada em arame amarelo/laranja, ajustada às paredes (R2, R5).
- Box Projection corrige a posição das paredes no reflexo conforme o ponto da superfície (R6).
- Light Probe Group: muitas sondas pequenas e esféricas; a seleção muda de cor; arestas finas ligam as vizinhas (L1, L4).
- O objeto dinâmico escurece de forma contínua ao entrar na sombra e pega cor do rebate (L2/L3, L5/L6), e o
  editor mostra as 4 sondas que ele mistura.
