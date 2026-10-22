# V-2 — Noite que parece noite (22/10/2026)

Pendência: `docs/planos/PENDENCIAS-VISUAIS-2026-10-21.md` (V-2). Branch
`claude/upbeat-noyce-708c19`, sobre a base `codex/ui-play-performance-apk-export`
com o PK-1/PK-2 (ambiente) mesclado. Aparelho: POCO F7, canal `dev.aether.editor.u07`,
APKs `0.3.0-dev.261022999-noite-HHMM`. Cena: cópia da Vila Astra (md5
`2e2e996841f1f0eb5af1572c68dec3fc`, devolvida ao aparelho com o mesmo md5) com
Hora do dia no sol e a Lua ligada (`tools/v2_night_scene_author.cpp`), câmera fixa do Play.

Prancha: `prancha-noite-unity-poco.png` (montada de `prancha.json`).

## Referência da Unity (frames com link e minuto)

`referencia-unity/`, medidas em `referencia-unity/medidas-unity.json`
(`tools/validation/measure-night-frames.py`: sRGB equivalente da luminância média).

| Vídeo | Minuto | Quadro | Céu | Chão | Azul/vermelho |
|---|---|---|---|---|---|
| HDRP Night, Seta — youtu.be/vnzt3aRTd5o | 0:02 | 37,8 | 43,8 | 29,7 | 2,03 |
| idem | 10:59 | 35,3 | 39,2 | 28,5 | 1,80 |
| Day/Night HDRP, Seta — youtu.be/UqQsfRUdukE | 0:00 | 15,2 | 15,4 | 12,6 | 2,78 |
| Unite 2020 (praça com postes) — youtu.be/yqCHiZrgKzs | 24:51 | 88,2 | 91,0 | 48,9 | 1,44 |
| PBSky com estrelas — youtu.be/tpTzwyiGByE | 6:02 | estrelas e Via Láctea | | | |

Prática da Unity tirada dos vídeos: Sun e Moon como duas Directional Lights girando
juntas (DaNManager em UqQsfRUdukE 9:25), Moon com 0,5 lux e temperatura fria
(8570 K e 20000 K em vnzt3aRTd5o 2:20–2:40), Exposure da noite fixa em EV 1 (2:40),
Volume noturno com Limit Min próprio (Unite 2020 24:51).

## O que mudou

- **Hora do dia › Lua** (componente v2): referência a uma direcional que gira até a lua
  da mesma hora; brilho pela fase (fração acesa ao cubo) e pela altura, apagada de dia.
  À noite a Lua vira a luz principal (cascatas), como a troca de sombra do ciclo do HDRP.
  "Sol com dia e noite" cria Sol + Lua ligados num passo de desfazer.
- **Céu**: lê o sol verdadeiro (`skySun`) mesmo com a Lua como luz principal; disco da lua
  na direção da direcional; brilho azul-marinho do luar (`skyNight`).
- **Estrelas**: contagem e brilho por magnitude (Pogson, limite 6,5), duas camadas, cor
  por temperatura, Via Láctea, giro pela esfera celeste da hora e latitude.
- **Ambiente da noite**: luar espalhado (20% da Lua, tom frio) mais um piso de estrelas;
  sonda SH do PK-1 misturada ao céu noturno; reflexo do ambiente apagado junto; neblina
  "Acompanhar o céu" com a cor do céu noturno (antes guardava o piso de 0,015 do dia e
  virava faixa branca com a exposição da noite).
- **Exposição**: Limite mínimo padrão −6 (era −2).

## POCO (`poco/`, medidas em `poco/medidas-poco.json`)

| Captura | Quadro | Céu | Chão | B/R | Leitura |
|---|---|---|---|---|---|
| 00 Fixa −0,41 (a da Vila) | 1,2 | 0,1 | 2,9 | — | preta: exposição do dia |
| 01 Automática, limite −8,26 | 138 | 109 | 151 | 0,84 | noite vira dia (o defeito do dono) |
| 03 limite −5 | 24,8 | 0,9 | 16,4 | 0,76 | escura, sem céu |
| 05 22 h, limite −6, céu ×12 | 38,1 | 3,5 | 51,6 | 0,46 | quadro e praça na faixa da Unity |
| 06 2 h, idem | 38,1 | 3,9 | 51,3 | 0,46 | estrelas giradas pela hora |
| 07 céu ×300 | 128 | 182 | 51,6 | 1,37 | passou: céu de anoitecer |

Logcat: `[AutoExposure] medido=-8.50 adaptado=-6.00`; `[NightSky] noite=1.00 lua=4.8e-4`.

## Não validado

- O brilho final do céu (`NightSkyGlow` = 60) está no APK `noite-1408`, mas a captura falhou:
  o POCO bloqueou com tela segura no meio da sequência.
- Azul/vermelho fica abaixo da Unity enquanto as lanternas da Vila forem 2300 K (autoria da
  cena); o luar e o céu já são frios.
- A Vila do dono continua com a Exposição Fixa herdada; para o ciclo de dia e noite ela precisa
  de Automática no Volume, como no HDRP.
