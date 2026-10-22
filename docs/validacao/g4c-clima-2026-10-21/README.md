# G4c — Clima (chuva, neve, relâmpago) no modelo prático da Unity (20–21/10/2026)

Branch `claude/mystifying-visvesvaraya-fb2538`. Fluxo Unity × Astra e limites em
`docs/planos/G4c-CLIMA-2026-10-20.md`; módulos de partícula novos em
`docs/planos/G5-P2-MODULOS-2026-10-20.md`.

## O que é conceito, o que foi implementado, o que foi testado

| Item | Estado |
|---|---|
| Componente **Clima** (`astra.environment.weather` v1) no objeto de um Ambiente, escrevendo nuvens e neblina desse volume e a emissão/posição de Chuva e Neve | Implementado, host e POCO |
| **+ › Luzes › Clima** (volume global prioridade 2 + Clima + filhos Chuva e Neve já ligados) | Implementado, host e POCO |
| Troca de Tipo pelo Inspector (Limpo, Nublado, Chuva, Tempestade, Neve) | POCO (Chuva → Neve, Chuva → Tempestade) |
| Transição em Play | Host (8 s, chuva → neve); no POCO não medida |
| Relâmpago (clarão nas nuvens e na neblina, só em Play) | Host e POCO (captura 08) |
| Vento das nuvens inclinando a chuva e arrastando a neve (Velocidade no tempo, Mundo) | Host; no POCO visível na neve |
| Neblina por altura no volume do clima (céu com nuvens acima do horizonte) | POCO (capturas 04 × 06) |
| API C# `Astra.Components.Weather` | Gerada; não exercitada por script no aparelho |
| Variação natural (pancadas e calmarias) com Variação e Ritmo no Inspector | Host e POCO (APK 0.3.0-dev.261022999-clima-1311, capturas 12 e 13) |
| Teto do aparelho repartido pela necessidade (corrige a chuva "em ondas") | Host e POCO: no APK 410 a chuva ficava presa em 3.200 vivas; no 261022999 varia de 3.003 a 3.553 sem tocar o teto |
| Raio desenhado, trovão, respingo no chão, chuva que não entra sob telhado | **Não feito** |

## Referência Unity (frames de vídeo) e o que mudou por causa dela

Em `referencia-unity/` (480p/720p, timestamp no nome do arquivo):

| Arquivo | Vídeo | O que mostra |
|---|---|---|
| `chuva-tempestade-pixagon_EJgEuGe1Www_t098s.jpg` (e t046/t151/t177) | [Rain Storm Effects – Unity Asset Store (Pixagon)](https://www.youtube.com/watch?v=EJgEuGe1Www&t=98s) | Traços longos (~8–17% da altura), finos (1–2 px), translúcidos; céu azul-cinza com nuvens escuras; neblina baixa |
| `neve-synty_LcNmXM1DEXg_t231s.jpg` (e t194/t250) | [Create a snow particle system in Unity – Synty Studios](https://www.youtube.com/watch?v=LcNmXM1DEXg&t=231s) | Flocos pequenos (3–6 px em 720p), opacos, densos, sem rastro |
| `neve-chuva-prefab_nkq0hjVdrnA_t034s.jpg` | [Rain & Snow Particle Prefab (Unity)](https://www.youtube.com/watch?v=nkq0hjVdrnA&t=34s) | Neve densa de perto |
| `chuva-speedtutor_SrWrUN56UWU_t353s.jpg` | [EASY Rain Particle in Unity – SpeedTutor](https://www.youtube.com/watch?v=SrWrUN56UWU&t=353s) | Chuva com sub-emissor de respingo |

Mudanças tiradas da comparação (`prancha-chuva-pixagon-1m38-x-poco.png`):

1. Gota mais fina e translúcida (0,03 → 0,018 m, alfa 0,45 → 0,32); o comprimento (~0,6 m pelo
   esticamento) já batia com os traços da Pixagon.
2. Floco menor (0,07 → 0,045 m) para a escala dos flocos da Synty; neve mais densa (1.600/s numa
   caixa de 24 m à frente da câmera).
3. Céu "chapado": a neblina linear cobria o céu inteiro (distância infinita). O volume do clima usa
   neblina **por altura** (decaimento 0,06/m), como o fog do HDRP com Base/Maximum Height: as nuvens
   voltam a aparecer acima do horizonte (capturas 04 × 06).
4. Neve só de um lado da tela: o vento arrastava os flocos ~20 m na vida deles. O emissor agora fica
   à frente da câmera e contra o vento por meia vida da partícula.
5. Ainda claro e quente demais em relação à Pixagon: céu mais fechado na chuva (0,70–0,88) e na
   tempestade (0,85–0,95), neblina escurecida pelo tom das nuvens, sombra das nuvens no máximo com
   tempo fechado, chuva mais densa (3.500/s).

## Capturas do POCO F7

| Arquivo | APK | O que mostra |
|---|---|---|
| `01-clima-criado-pelo-menu.png` | 171 | + › Luzes › Clima na Vila: cena acinzenta, neblina e gotas já na edição |
| `02-ambiente-com-nuvens-escritas-pelo-clima.png` | 151 | Inspector do Ambiente com a Cobertura escrita pelo Clima (visível, como no script da Unity) |
| `03-inspector-clima.png` | 151 | Inspector do Clima: Tipo, Intensidade, Transição, Precipitação (Chuva/Neve ligadas) |
| `04-play-chuva-antes-ceu-chapado.png`, `04-play-chuva-neblina-linear-ceu-chapado.png` | 151/171 | Antes: céu cinza uniforme |
| `05-tipo-de-clima.png` | 151 | Lista de tipos |
| `06-play-chuva-neblina-por-altura.png` | 171 | Depois: nuvens visíveis, gotas por toda a tela |
| `07-play-tempestade.png`, `08-play-tempestade-clarao-do-relampago.png` | 171 | Tempestade e o quadro do clarão |

O MIUI encerrou o app por temperatura às 02:17 de 21/10 (bateria 52,8 °C, status térmico 3) durante a
sequência da tempestade; não foi falha do app.

## "Chove, para e volta" (relato do usuário, 21/10)

Causa: o teto de partículas do aparelho (8.000) era repartido pelo **Máximo** autoral de cada sistema.
A Neve (máx. 12.000) ficava com a maior fatia mesmo emitindo zero e a Chuva era cortada em 3.200
(o log do APK 410 mostra `vivas=3200` constante). Com o teto cheio, gotas novas só nasciam quando as
velhas morriam, todas juntas: a chuva vinha em ondas, como um loop. Correção: o teto é repartido pela
necessidade real (taxa × vida + rajadas, com piso de 256 para Emit e sub-emissores).

Além disso, o Clima ganhou **Variação natural** (0,35) e **Ritmo da variação** (45 s): em Play a
intensidade oscila por ruído suave entre o autoral e (1 − variação) × autoral, passando pela
Transição — pancadas e calmarias sem repetição, como os ciclos de intensidade dos scripts de clima.

Verificação no POCO (22/10, `0.3.0-dev.261022999-clima-1311`, Vila em Play, chuva 0,7, variação 0,35):
60 amostras do log `[Particles]` em 2 min (`gpu/chuva-continua-vivas-261022999.txt`) — as gotas vivas
sobem e descem devagar entre 3.003 e 3.553, longe do teto de 8.000, sem os buracos das ondas
(`12-chuva-continua-gotas-vivas.png`); quadros aos 10, 50 e 90 s em `13-chuva-continua-quadros-10s-50s-90s.png`.
Passe das partículas: 0,06–0,08 ms de GPU.

## Custo de GPU (POCO F7, nível S nativo 2772×1280, Play, APK 410)

`tools/measure-g0-baseline.ps1 -Order S -Seconds 40`, 4 rodadas intercaladas, bateria ≤ 37,9 °C no
início de cada uma, sem pressão térmica (`gpu/*.json`):

| Rodada | Clima | FPS | Quadro GPU (ms) | Opaco (ms) | Pós (ms) |
|---|---|---|---|---|---|
| 1 | não | 30,45 | 29,88 | 12,19 | 13,28 |
| 2 | chuva 0,7 | 29,53 | 30,81 | 12,75 | 13,66 |
| 3 | chuva 0,7 | 30,03 | 30,57 | 12,72 | 13,42 |
| 4 | não | 30,35 | 29,96 | 12,24 | 13,27 |

Média: **+0,77 ms por quadro (~2,6 %)**. O passe das partículas (3.200 gotas, timestamps próprios,
`gpu/*-particulas.txt`) custa **0,05–0,07 ms**; o resto vem das nuvens do volume, da sombra das
nuvens (opaco +0,52 ms) e da neblina por altura (pós +0,27 ms). Neve com ~4.800 flocos: passe de
0,43 ms (flocos maiores na tela, partícula suave). Simulação na CPU: 1,6–4,2 ms (thread do editor).

## Testes de host

- `aether_environment_authoring_tests` 50/50 (7 do clima: registro e arquivo, alvos por tipo, escrita
  em nuvens/neblina/emissão/posição, transição em Play, relâmpago só em Play, gotas caindo, receita
  com filhos e desfazer).
- `aether_particle_tests` 19/19 (9 novos do P2: arquivo v1→v2, força, velocidade, limite, ruído,
  colisão, sub-emissores, herança/taxa por distância, tamanho/cor/rotação pela velocidade, rosca e
  retângulo).
- `aether_tests` 1628/1628 depois do merge com `codex/ui-play-performance-apk-export`.
