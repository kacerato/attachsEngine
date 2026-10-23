# Medições da base atual no POCO F7 e V-3 (22–23/10/2026)

Base: `codex/ui-play-performance-apk-export` (aee32953) com G1, G2, G3a, G4 dia/noite e nuvens 2D,
PK-1/PK-2, PK-3/PK-8, PK-6, PK-7, PK-9, V-1 e V-2, mais a chave de A/B do gather desta entrega.
APK Astra Dev `0.3.0-dev.261022999-medicao-2324` (Sponza base) e `0.3.0-dev.261023999-medicao-0833`
(mesmo código, reempacotado com o versionCode do dia; Vila e A/B), conferidos com `dumpsys` antes e
depois de cada bloco. Coletor: `tools/measure-g0-baseline.ps1`; A/B: `tools/measure-feature-ab.ps1`;
tabelas: `tools/summarize-feature-ab.py` (→ `tabelas.md`, `tabelas-sponza-ab.md`).

- V-3 (sombra e sol por objeto): [`v3/README.md`](v3/README.md) e `v3/prancha-v3-unity-poco.jpg`.
  **As três opções valem no aparelho; nada a corrigir.**

## Condições

- Mesmas cenas e câmeras do G0: Vila Astra em Play (câmera do player), Sponza em Edit na pose fixa
  (-6,101; 5,220; -8,918; yaw 0,6; pitch 0,45). Resolução dinâmica desligada, qualidade travada,
  nível C = `preset=c` + referência 1600×720 (interna 1579×729). 40 s por rodada.
- Névoa resolvida por texel ligada, como no APK (o coletor do G0 a desligava se não fosse pedida).
- Sponza base: bateria (sem carregador), 39–40 °C. Vila e A/B: **no carregador**, como o G0.
- Por pedido do dono, a partir da Vila não houve pausa de resfriamento: a bateria foi de 41,9 a 49,3 °C
  no A/B da Vila e chegou a 50 °C no Sponza. A temperatura de cada rodada está nas tabelas.
  Os controles da Vila variaram só 2,4% (26,20 → 26,84 ms) ao longo de 1h15.
- G0 (17/10): aparelho quente (43,9–45,8 °C) e no carregador. Comparação direta S×S e C×C.

## Custo por quadro: base atual × G0 (ms, média das rodadas)

| Cena / nível | FPS interno | GPU quadro | CPU processo | Opaco | Pós | Sombra | EASU+RCAS | Skinning | Exposição |
|---|---|---|---|---|---|---|---|---|---|
| Vila S — G0 | 18,5 | 50,8 | 23,6 | 23,2 | 18,1 | 5,5 | 2,3 | 1,4 | — |
| **Vila S — agora** | **32,7** | **28,2** | **11,2** | **14,1** | **8,9** | **3,1** | **1,3** | **0,7** | 0 (fixa) |
| Vila C — G0 | 30,1 | 30,8 | 18,0 | 9,9 | 6,1 | 5,5 | 7,5 | 1,6 | — |
| **Vila C — agora** | **52,3** | **17,0** | **9,6** | **6,0** | **3,0** | **2,8** | **4,4** | **0,6** | 0 (fixa) |
| Sponza S — G0 | 39,9 | 22,7 | 4,5 | 12,1 | 10,5 | 0 | 0 | 0 | — |
| **Sponza S — agora** | **57,4** | **14,6** | **5,0** | **9,3** | **5,2** | 0 | 0 | 0 | **0,14** |
| Sponza C — G0 | 40,9 | 22,1 | 4,5 | 8,1 | 13,9 | 0 | 0 | 0 | — |
| **Sponza C — agora** | **59,9** | **11,9** | **5,0** | **6,0** | **5,7** | 0 | 0 | 0 | **0,15** |

Tabela completa por passe e por rodada em `tabelas.md`. Leitura:

- **Vila: GPU −44% em S e −45% em C; CPU −52% em S.** O Sponza fica preso em 60 FPS (o app não passa
  de 60 Hz nem com `aether.target_fps=120`): ali a GPU trabalha abaixo do vsync e o governador baixa o
  clock, então os 14,6 ms são teto, não custo puro.
- Tudo o que entrou depois do G0 (exposição automática, sondas, materiais URP, noite, nuvens 2D,
  reflexo pelo céu) cabe nessa queda. Custo isolado visível: exposição automática 0,14 ms no Sponza
  (a Vila usa Exposição Fixa, 0 ms); sondas 0 ms (nenhuma sonda de reflexo nas duas cenas); céu e
  nuvens 2D não têm região própria no perfilador e entram no opaco.
- No C da Vila o EASU custa 3,5 ms (21% do quadro) porque a saída é 2772×1280; num celular 720p a saída
  tem ~⅓ dos pixels (mesma ressalva do G0).
- A Vila no C ainda roda **4 cascatas @4096** (2,8 ms de passe de sombra): os eixos autorais do projeto
  vencem o preset, como no G0.

## A/B por recurso (Vila Astra, S, no carregador)

Cada recurso foi medido entre dois controles da base completa; Δ = recurso − média dos controles vizinhos.
Controles: 26,20 / 26,51 / 26,88 / 26,84 ms (passos 1, 5, 9, 13).

| Recurso | Δ GPU (ms) | Onde | Leitura |
|---|---|---|---|
| **G1 — gather PCF desligado** (`aether.shadow_gather=false`, nove buscas) | **+0,66** | opaco +0,71 | O gather economiza 0,7 ms (5% do opaco). Fica ligado. |
| **G1 — máscara de luzes por desenho** ligada (`aether.punctual_light_mask`) | **−0,44** | opaco −0,41 | Ganho real; hoje vem desligada. |
| Sombra do sol na iluminação desligada (`gpu_isolation=5`) | −1,22 | opaco −1,20 | Custo da busca nas cascatas (o passe de sombra, 2,6 ms, é à parte). |
| Luzes pontuais desligadas (`gpu_isolation=4`) | **−2,93** | opaco −2,77 | 8 lâmpadas acesas na praça. |
| AO desligado (`post_isolation=2`) | −0,07 | pós −2,81, opaco +1,63 | O AO custa 2,8 ms no pós; o opaco subiu na mesma rodada (bateria 48 °C), resultado ambíguo no total. |
| Bloom desligado (`post_isolation=4`) | −1,48 | pós −1,39 | |
| **Névoa por pixel** (G1 desligado, `post_fog_resolve=false`) | **+5,83** | pós +5,65 | A névoa resolvida por texel do G1 é o maior ganho medido. |
| Candidato: AO em meia resolução (`post_half_ao`) | **−1,54** | pós −1,43 | Metade do custo do AO. |
| Candidato: bloom em cadeia de mips (`post_bloom_chain`) | −0,65 | pós −0,47 | |

A coluna de CPU (em `tabelas.md`) acompanha a temperatura (10,6 → 14,3 ms ao longo da série) e não
serve para A/B de recurso de GPU. O A/B do Sponza (`tabelas-sponza-ab.md`, alvos 60 e 120) ficou
preso em 60 FPS: as diferenças ficam em ±0,5 ms, exceto "só cor base" (−2,2 ms no total, −4,1 no opaco),
e o pós sobe quando o opaco cai — sinal de clock variando. Vale como indicação, não como número.

## Os 5 maiores custos e o corte proposto para celular fraco

Quadro de referência: Vila S, 26,5 ms de GPU e 11 ms de CPU.

1. **Opaco: 14 ms (53%).** O G1 mediu 47% disso como geometria e cor base em 3,5 MP: é custo de
   resolução. **Corte:** no nível C a escala 0,57 já leva o opaco a 6 ms; no celular fraco, manter a
   escala por política (não por projeto) e usar a resolução dinâmica, que hoje vem desligada nas medições.
2. **Pós: 8,6 ms (32%)**, com AO 2,8 ms e bloom 1,4 ms. **Corte medido:** AO em meia resolução
   (−1,5 ms) e bloom em cadeia (−0,65 ms) como padrão de B e C, depois de conferir a imagem lado a lado;
   no C, AO desligado.
3. **Sombra do sol: ~3,8 ms** (passe 2,6–3,1 + busca 1,2). A Vila mantém 4 cascatas @4096 até no C porque
   os eixos autorais vencem o preset. **Corte:** o preset C vira teto (por exemplo 2 cascatas @1024 e
   PCF de 9 buscas), como a política já faz com outros clamps; o gather PCF fica ligado (−0,7 ms medido).
4. **Luzes pontuais: 2,9 ms** com 8 lâmpadas. **Corte:** máscara por desenho ligada por padrão
   (−0,44 ms medido) e, no C, até 4 luzes por objeto.
5. **CPU: 11 ms em S, 9,6 em C** (animação de 11 personagens e scripts). Não limita o POCO, mas num
   celular fraco a CPU passa do orçamento de 16 ms antes da GPU. **Próximo passo:** simpleperf no APK
   profileable para separar animação, scripts e envio de desenhos; sem medir não há corte proposto.

Fora da lista porque não se transfere: EASU de 3,5 ms no C do POCO (saída 2772×1280).

## O que não foi medido

- Custo isolado de materiais URP, noite, nuvens 2D e reflexo pelo céu: não há chave por recurso nem
  região própria; entram no total (que caiu).
- Sponza com GPU saturada: o app não passa de 60 Hz no POCO.
- Nenhuma otimização foi feita nesta entrega: as propostas acima precisam de A/B de imagem e de custo
  antes de virar padrão.

## Arquivos

- `vila-base/`, `sponza-base/`: S/C/C/S, com `rows.json`, `baseline.md` e por rodada logcat, política,
  janelas, superfície, captura, bateria e térmica antes/depois.
- `vila-ab/`, `sponza-ab-alvo60/`, `sponza-ab-alvo120/`: um diretório por passo; `ab-rows.json` junta tudo.
- `v3/`: V-3.
