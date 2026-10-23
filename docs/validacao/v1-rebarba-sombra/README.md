# V-1 — Rebarba branca nos contornos e sombra lavada (22/10/2026)

Pendência: `docs/planos/PENDENCIAS-VISUAIS-2026-10-21.md`, V-1. O dono via uma "rebarba branca" em volta de
personagens, casas e objetos, e lugares de sombra "meio esbranquiçados", sem chegar ao escuro.

## Causa

**O reflexo especular do ambiente.** Sem HDRI na cena, a Vila usa céu procedural. A luz ambiente difusa segue
esse céu, mas o reflexo especular, que todo material lê no binding 3 (`environmentSpecularMap`), continuava sendo
o mapa **empacotado**: uma foto de céu de meio-dia, escalada só por `skyScale × indirectSpecular`. Toda superfície
em sombra e toda borda rasante refletia esse meio-dia:

- nas bordas, com Fresnel alto, aparecia um contorno claro (lanterna, quina das casas, personagens);
- nas faces em sombra, o reflexo somava um véu cinza-azulado, e a sombra não chegava ao escuro.

## Como foi isolado (POCO F7, Vila Astra, Play)

APK de diagnóstico `0.3.0-dev.261022094-v1diag`, com a máscara `aether.post_isolation` ampliada para desligar
um efeito por vez: 1 neblina, 2 AO, 4 bloom (antes declarado, sem efeito), 8 FXAA, 16 nitidez (agora também no
FSR 1 e no ampliador temporal), 32 ambiente difuso, 64 reflexo do ambiente; o TAA foi trocado por
`aether.anti_aliasing`. As capturas e os zooms estão em `isolamento/`.

| Estado | Contorno claro | Véu cinza na sombra |
|---|---|---|
| Base (FXAA, nitidez 0,40) | presente | presente |
| Nitidez desligada / FXAA desligado / TAA | presente | presente |
| Neblina, AO, bloom, ambiente difuso desligados | presente | presente |
| **Reflexo do ambiente desligado** | **some** | **some** |

`isolamento/zoom-lanterna.png`, `zoom-telhado-dir.png` e `zoom-banco.png` mostram os oito estados lado a lado.
O suspeito do `precision mediump int` (achado do PK-9) não explica a rebarba: desligar só o reflexo já a remove
inteira.

## Como a Unity faz

Na janela Lighting da Unity 6, **Environment Reflections › Source: Skybox** gera o reflexo do ambiente a partir
do próprio Skybox da cena (Resolution padrão 128, Intensity Multiplier). Isso vale mesmo com a luz ambiente em
Gradient ou Color. No HDRP, o céu é renderizado num cubemap e filtrado em GGX, e o reflexo do céu é refeito quando
o céu muda. Referências:
[Lighting window](https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html),
[Reflection Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ReflectionProbe.html).

## Correção

- O caminho de captura das sondas do PK-6 ganhou o pedido "reflexo do céu" (`kind 2`): seis faces só com o céu
  (sem objetos), na resolução 128, filtro GGX no octaedro de 256 px × 6 mips do atlas, copiado para uma imagem
  própria (`skyReflection_`).
- Com céu procedural (qualquer céu que não seja HDRI), essa imagem substitui o mapa empacotado no binding 3.
  Isso vale para todo material PBR, para a água e para as sondas de reflexo, que usam o céu no que sobra dos pesos.
  A vista de câmera do editor reflete o mesmo céu.
- A escala passa a ser só `indirectSpecular` (o Intensity Multiplier da Unity), porque a captura já está no brilho
  do céu da cena. Com HDRI, nada muda.
- A captura é refeita quando a chave do céu muda além de um milésimo (relativo acima de 1): sol (direção, cor,
  raio), cores do céu, astros, atmosfera, nuvens (sem o deslocamento do vento) e a exposição do céu
  (`sceneAoDetail.z`). A regra está em `renderer::skyReflectionNeedsCapture`, com teste de host.
- `aether.post_isolation`: 128 desliga o reflexo pelo céu (antes/depois no mesmo APK); 256 força a recaptura a
  cada quadro, para medir o pior caso.

## Resultado no aparelho (APK `0.3.0-dev.261022111-v1ceu`)

Capturas em `correcao/`, prancha em `prancha-unity-x-poco.jpg`.

| Medida (luminância média, sRGB 0–255) | Antes | Depois | Unity |
|---|---|---|---|
| Piso ao sol | 159,2 | 157,4 | — |
| Piso na sombra da casa | 105,7 | 75,8 | — |
| Razão sol/sombra no mesmo piso | **1,49** | **2,08** | 2,22 (Unity 5, praça) · 1,74 (HDRP, nublado) |

- Lanterna: a franja branca virou borda escura, com um leve tom do horizonte.
- Quina da casa: a linha cinza de 1 px desapareceu.
- Face do banco em sombra: de cinza-claro a escuro.
- A casa da direita, que ficava toda preta (o reflexo empacotado nessa direção era chão escuro), agora mostra a
  parede em sombra com detalhe, como as fachadas em sombra dos frames da Unity.

## Custo de GPU (POCO F7, Play, 600 quadros por rodada)

Média de `gpu_frame_ms` do `[FrameProfile]`.

| Rodada | Reflexo pelo céu | Mapa empacotado (128) |
|---|---|---|
| 1 | 25,07 ms | 25,22 ms |
| 2 | 24,97 ms | 24,78 ms |

Em regime não há custo: o material lê uma textura do mesmo tamanho que antes. A captura só acontece quando o céu
muda. O pior caso (recaptura a cada quadro, bit 256) está na seção seguinte.

### Pior caso e APK final (`0.3.0-dev.261022999-v1final-1234`, commit 04f41b39)

| Rodada | Em regime | Recaptura a cada quadro (bit 256) | Mapa empacotado (bit 128) |
|---|---|---|---|
| 1 | 24,87 ms | 25,41 ms | 25,09 ms |
| 2 | 24,80 ms | 25,31 ms | — |

O pior caso, um céu que muda em todo quadro (ciclo de dia acelerado), custa cerca de **+0,5 ms** de GPU por quadro:
seis faces só de céu a 128 px e seis mips de filtro GGX. Um ciclo de dia real muda a chave muito menos que isso.
O APK final repete o resultado: sol/sombra 2,08 com o reflexo pelo céu e 1,51 com o mapa empacotado.

## Referência em vídeo da Unity (frames em `referencia-unity/`)

| Frame | Vídeo e instante | Para que serve |
|---|---|---|
| `hdrp-vila-hg6EAWG2cs0-00m05s-casa-contraluz.jpg` | *Medieval Village HDRP*, [0:05](https://www.youtube.com/watch?v=hg6EAWG2cs0&t=5s) | casa em contraluz: silhueta sem contorno claro |
| `hdrp-vila-hg6EAWG2cs0-00m32s-varanda-sombra.jpg` | idem, [0:32](https://www.youtube.com/watch?v=hg6EAWG2cs0&t=32s) | varanda em sombra com detalhe |
| `hdrp-vila-hg6EAWG2cs0-00m37s-beiral-sombra.jpg` | idem, [0:37](https://www.youtube.com/watch?v=hg6EAWG2cs0&t=37s) | sombra sob a varanda: razão sol/sombra 1,74 (dia nublado) |
| `vila-medieval-Sc0kVg_qyIE-00m44s-praca-sombra.jpg` | *Medieval Village — Unity Asset Store* (Unity 5), [0:44](https://www.youtube.com/watch?v=Sc0kVg_qyIE&t=44s) | praça com sombra no calçamento: razão 2,22 |
| `vila-medieval-Sc0kVg_qyIE-01m10s-telhado-ceu.jpg` | idem, [1:10](https://www.youtube.com/watch?v=Sc0kVg_qyIE&t=70s) | telhados contra o céu |
| `urp-aa-vkdOkAmnA54-00m25s-sem-aa-borda.jpg` | *SMAA vs MSAA vs TAA vs Colibri in Unity URP*, [0:25](https://www.youtube.com/watch?v=vkdOkAmnA54&t=25s) | bordas de vila URP sem AA, em zoom |
| `urp-aa-vkdOkAmnA54-00m59s-msaa-borda.jpg` | idem, [0:59](https://www.youtube.com/watch?v=vkdOkAmnA54&t=59s) | MSAA + SMAA, em zoom |
| `urp-taa-WGgHc-0ym3Q-00m59s-rgb-clamp-zoom.jpg` | *Temporal Anti-Aliasing by Unity URP 2022.3*, [0:59](https://www.youtube.com/watch?v=WGgHc-0ym3Q&t=59s) | TAA com clamp RGB: sangramento de cor na borda |
| `urp-taa-WGgHc-0ym3Q-01m02s-ycocg-zoom.jpg` | idem, [1:02](https://www.youtube.com/watch?v=WGgHc-0ym3Q&t=62s) | TAA com clamp YCoCg: borda limpa |

O que a comparação mudou: os frames de TAA da URP e do HDRP mostram bordas sem halo, o que tirou o TAA e a nitidez
da lista de causas antes do isolamento; os frames das vilas deram a faixa de contraste sol/sombra (1,7 a 2,2) que a
correção precisava atingir.

## Conceito, implementado e testado

- **Implementado**: captura do céu sem objetos pelo caminho do PK-6, imagem própria no binding 3, recaptura por
  chave, escala por Intensity Multiplier, vista de câmera do editor, isolamentos 4/16/32/64/128/256, regra de
  recaptura em `renderer/reflection_probes` com teste de host.
- **Testado no aparelho**: Play e vista de edição na Vila (hora da cena), antes/depois no mesmo APK, custo em regime.
- **Não testado no aparelho**: ciclo de dia animado (recaptura contínua fora do bit 256), céu físico (Physical
  Atmosphere), noite (depende da V-2), HDRI autoral (o caminho não muda).
- Sem mudança de formato de cena nem de projeto: nada a migrar.

## Pendências para o merge

- PK-1/PK-2 (Ambiente v18): os campos novos já entram pelas linhas da chave; a exposição do céu entrou por
  `sceneAoDetail.z`.
- V-2 (noite): acrescentar o `skyNight` (offset 4240 no bloco da V-2) à chave de recaptura. O `specularScale` da
  V-2 continua valendo só para HDRI e mapa empacotado, porque o V-1 atribui `sceneAoDetail[3]` quando o reflexo
  vem do céu.
