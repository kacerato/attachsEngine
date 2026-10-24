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

## Chuva sem cor depois do merge da base (23/10)

Depois do merge com `codex/ui-play-performance-apk-export` (V-2, PK-1/PK-2, PK-9) a Vila ficava
sépia com o Clima (`16-defeito-...`, `18-defeito-play-chuva-sem-cor-1033.png`). Três causas, em ordem:

1. **Grupos de pós no volume do Clima.** A base dividiu o pós em overrides por efeito (Exposição,
   Ajustes de cor, Grading...); a receita os deixava ligados e o volume trocava a exposição e a cor da
   cena pelos padrões. Receita desliga todos (`73b970ca`).
2. **Neblina nova.** Volumes novos nascem com Exp² e cor fixa (o céu fica limpo); o Clima herdou e a
   chuva perdeu o véu cinza sobre céu e cena. Diagnóstico de host com a cena puxada do aparelho
   (Ambiente resolvido com e sem o Clima): só neblina e nuvens mudam. Receita: Exponencial + Acompanhar
   o céu (`21021140`), o visual calibrado no APK 410 (`17-clima-criado-edicao-1033.png`).
3. **Sombra das nuvens sem a luz que elas espalham.** A/B no POCO: com "Projetar sombras" desligado a
   cor volta (`19-ab-sombra-das-nuvens-desligada-1033.png`). A sombra tira ~80 % do sol no tempo
   fechado e, com a sonda SH do PK-1, o ambiente era só o céu acinzentado — sobrava o reflexo bege.
   Agora a parte bloqueada do sol volta como céu cinza difuso (× 0,45, céu/equador/chão 1/0,7/0,25),
   somada à sonda e ao ambiente constante, como a sonda do HDRP renderizada com as nuvens
   (`platform/android/instanced_renderer.cpp`).

### Resultado no POCO (23/10, `0.3.0-dev.261023999-clima-1245`)

Clima criado pelo menu na Vila (`20-clima-criado-edicao-1245.png`), chuva em Play
(`21-play-chuva-1245.png`) e neve pelo Tipo do Inspector (`22-play-neve-1245.png`, 4.701 flocos,
passe 0,075 ms de GPU). Telhados, grama e personagens voltaram a ter cor sob o céu cinza.

Pranchas Unity × POCO:

- `prancha-chuva-pixagon-1m38-x-poco-1245.png`: Pixagon Rain Storm (1:38) × Vila. Traço e densidade
  das gotas parecidos. A tempestade da Pixagon é bem mais escura e azulada; a Vila com Chuva 0,7 fica
  clara. O tipo Tempestade escurece mais, mas ainda sem chão molhado nem poças.
- `prancha-neve-synty-3m51-x-poco-1245.png`: Synty (3:51) × Vila. Flocos pequenos e opacos como os da
  Synty. A Synty tem neve acumulada nos telhados e no chão; a Astra ainda não acumula neve.

Lacunas que continuam: chão molhado/poças, acúmulo de neve, respingo no chão (sub-emissor de
colisão disponível, sem receita pronta).

### Superfícies do clima no POCO (24/10, `0.3.0-dev.261024999-clima-0058`)

Depois das pranchas -1245 (cena clara e seca), o clima passou a agir nas superfícies, como
nos climas da Unity (Enviro 3: wetness/snow globais com acumular e secar, luz direta e ambiente por
tipo; UniStorm: neve pela normal com grão e relevo).

- Chuva: chão escuro, liso e refletindo; chuva 7.000/s com rastros perto da câmera; céu
  fechado; luz direta 0,5 e ambiente 0,6 em Chuva cheia (`23-play-chuva-molhada-0058.png`,
  `24-edicao-chuva-chao-molhado-0058.png`).
- Neve: cobre o chão, os telhados inclinados, as copas e o topo do muro em ~60 s de Play
  (`25-play-neve-acumulada-0058.png`, `26-edicao-neve-acumulada-0058.png`).
- Pranchas: `prancha-chuva-pixagon-1m38-x-poco-0058.png` e `prancha-neve-synty-3m51-x-poco-0058.png`.

Defeito achado no caminho: o APK clima-2003/2349 caía no compilador de shader da Adreno ao criar o
pipeline principal. A causa era ler uma linha nova do bloco de ambiente depois de 4112 (offset 4256).
A bisseção no aparelho confirmou: sem o bloco, o APK abria. Agora os valores vão em campos
reservados do começo do bloco (`shadowTransitionParameters.w`; neve e poças empacotadas em
`materialDistanceParameters.w`), e o relógio vem do da água.

Lacunas desta rodada:
- As poças não apareceram no 0058, porque a máscara usava a normal do normal map e o relevo das
  pedras tirava o piso da faixa "plano". A correção usa a normal da geometria e ainda não foi vista no
  aparelho.
- A chuva continua mais quente e clara que a tempestade azulada da Pixagon.
- A neve fica num dia cinza; a Synty é ensolarada.
- PK-10 no aparelho: Efeitos › Faíscas cria e emite (36 vivas). O passe desenha 3 lotes (neve,
  faíscas e as trilhas delas, ~330 segmentos a mais que as vivas). Mas faíscas aditivas pequenas
  sobre a neve clara não aparecem na captura (`27-faiscas-emitindo-0058.png`). Luzes e Folha de
  textura ainda não foram vistas.

### Atmosfera por tipo, poças e respingos (24/10, `0.3.0-dev.261024999-clima-0215`)

- Chuva: cada tipo traz cor e densidade da neblina, filtro de cor, saturação, contraste e exposição
  (presets por Weather Type do Enviro 3). A chuva fica azul-acinzentada, dessaturada e escura, com
  névoa que apaga o fundo. O céu nublado deixou de ser cinza neutro e passou a ser cinza-azulado:
  era a causa comum do bege, também sob as nuvens do G4. As poças apareceram (máscara pela normal da
  geometria) e refletem o céu. As gotas são finas e translúcidas, de tamanho e velocidade
  sorteados, e respingam ao bater no "Chão da chuva"
  (`28-play-chuva-pocas-0215.png`, `29-edicao-chuva-pocas-0215.png`,
  `prancha-chuva-pixagon-1m38-x-poco-0215.png`).
- Neve: flocos bem visíveis; telhados, chão, copas e muro cobertos com borda
  (`30-play-neve-0215.png`, `prancha-neve-synty-3m51-x-poco-0215.png`).
- PK-10 na Tempestade: Faíscas com trilhas visíveis, arcos laranja pontilhados
  (`31-faiscas-trilhas-tempestade-0215.png`, `32-faiscas-trilhas-detalhe-0215.png`). Log do passe:
  3 sistemas, 4 lotes (chuva, respingos, faíscas e trilhas), 0,078 ms de GPU.

Lacunas:
- A neve não ficou ensolarada como na Synty: o céu continua cinza escuro. A cobertura de nuvens mais
  baixa não bastou; o céu base da Vila é nublado.
- A tempestade da Pixagon é mais azulada e tem névoa mais clara.
- Luzes das faíscas não se notam no chão.
- Folha de textura e Material de projeto ainda não foram vistos no aparelho.
- A linha em arco no chão é a face vertical da borda da praça, que não recebe neve.

### Chuva azul-petróleo, neve com sol e PK-10 no aparelho (24/10, clima-0921 e clima-0953)

- Chuva (0921): filtro e névoa azul-esverdeados, névoa densa cobrindo céu e fundo
  (`33-play-chuva-petroleo-0921.png`, `prancha-chuva-pixagon-1m38-x-poco-0921.png`).
- Neve: no 0921 saía com o céu da chuva (`34-defeito-neve-com-ceu-da-chuva-0921.png`), porque o preset
  escrevia filtro 1,04 e o Ambiente aceita só [0, 1]. Com isso recusava o volume inteiro, que ficava
  com a chuva. Corrigido no 0953: céu aberto com nuvens soltas, sol cheio, sombras longas azuladas,
  neve branca e telhados com espessura (subida de 7 cm nos vértices voltados para cima)
  (`37-play-neve-com-sol-0953.png`, `38-edicao-neve-com-sol-0953.png`, `prancha-neve-synty-3m51-x-poco-0953.png`).
  O horizonte fica esverdeado com o sol baixo da Vila (fim de tarde); a Synty é meio-dia.
- Luzes das faíscas (0921, Tempestade): entram na cena. O log do editor diz "Orçamento de luzes: 16
  pontuais acesas, 6 fora", mas os postes da Vila ocupam quase todo o teto e no chão só aparecem
  reflexos laranja fracos (`35`/`36-faiscas-luzes-*`).
- Folha de textura (0953): Partículas com a folha 2×2 na textura própria. Cada partícula mostra um
  quarto do ponto (`39-folha-de-textura-2x2-0953.png`, `40-folha-de-textura-detalhe-0953.png`). O
  Inspector mostra Colunas, Linhas, Animação, Quadro no tempo (curva), Quadro inicial e Ciclos.
- Material do projeto e luzes à noite (24/10 16:43, mesmo APK clima-0953): projeto de exemplo
  "PK10 Noite", gerado por `aether_gui_preview write-pk10-night <pasta>`. A cena tem chão escuro,
  Sol com dia e noite às 23 h, nenhum poste, Faíscas e Partículas com o Material "Fogo animado"
  (shader Partícula, aditivo). A folha `fogo-folha-4x4.png` sai de `tools/generate-fire-flipbook.py`
  com Folha 4×4. No POCO, o halo quente das luzes das faíscas acende o chão e muda de um quadro para
  outro, e a chama troca de forma com a animação da folha (`41-pk10-noite-edicao-0953.png` e
  `42-pk10-noite-play-3-quadros-0953.png`, três quadros em Play com cerca de 0,4 s entre eles).

## Mescla com o G4 (nuvens volumétricas): trecho de céu fechado

Conflito esperado em `native/platform/android/instanced_renderer.cpp`, no bloco
`G4b: Cloud Layer ... cloudSkyOcclusion` dentro de `applySceneEnvironmentLook`.
Vale a versão da g4c-clima nestes três pontos, aplicados sobre a ocupação de nuvens que o G4
calcular (2D ou volumétrica):
1. O acinzentamento do ambiente e da sonda SH puxa para cinza-AZULADO
   (`overcastTint` 0,9 / 0,97 / 1,12), não para cinza neutro. O cinza neutro era a causa do bege.
2. A parte do sol bloqueada pela sombra das nuvens volta como céu difuso
   (`scattered`, gradiente céu/equador/chão 1 / 0,7 / 0,25), com transmissão
   `clamp(0,5 − 0,35 × densidade, 0,1, 0,5)`.
3. Depois de `applySceneEnvironmentLook`: os multiplicadores do clima no sol e no ambiente
   (`weatherSunScale_`, `weatherAmbientScale_`) e as superfícies em
   `shadowTransitionParameters[3]`/`materialDistanceParameters[3]` (não usar linha nova no fim do
   bloco: leitura depois de 4112 derruba o compilador da Adreno).
Se o G4 tiver o próprio termo de luz espalhada pelas volumétricas, fica o dele no lugar do item 2,
mantendo o tom do item 1.

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
