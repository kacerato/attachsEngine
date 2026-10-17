# G1 · pós-processamento: névoa resolvida por texel (17/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco G1, primeiro alvo
(pós de 10–18 ms na linha de base do G0). Aparelho POCO F7, Astra Dev
`dev.aether.editor.u07`, carregador ligado e aparelho quente por decisão do proprietário.

## 1. Atribuição (`aether.post_isolation`)

Máscara de diagnóstico que desliga etapas do passe de pós (1 neblina, 2 AO, 4 bloom,
8 FXAA, 16 nitidez); não é opção de jogo. Vila Astra em Play, nível S, uma rodada por
máscara (`docs/validacao/g1-post-attribution-2026-10-17/`). O clock da GPU variou
entre rodadas (FPS 28,6 e 19,1 na mesma máscara 0), então a comparação usa
**pós ÷ opaco**: o opaco não depende da máscara e as duas rodadas de controle deram
0,810 e 0,804.

| Desligado | pós/opaco | Economia do pós |
|---|---|---|
| nada | 0,81 / 0,80 | — |
| neblina | 0,44 | 45% |
| bloom | 0,51 | 37% |
| AO | 0,59 | 27% |
| FXAA | 0,62 | 23% |
| nitidez | 0,80 | ~0 |
| tudo | 0,08 | 90% |

As economias somam mais de 100% porque a neblina é avaliada **dentro** de cada
amostra: `sampleScene()` aplica `applySceneFog()` (leitura de profundidade + log/exp)
e é chamada até 13 vezes pelo FXAA, 8 pelo bloom e 9 pela reconstrução.

## 2. Candidata

Pré-passe na resolução interna grava cor + neblina uma vez por texel
(`native/platform/android/instanced_post_fog.inl`, bit 256 do shader); o passe
principal lê essa cor com a neblina desligada (bit 512). AO, FXAA, bloom, grade e UI
não mudam. Só em tela cheia (Play/jogo), sem TAA nem ampliador temporal, e com
neblina ativa; no editor, o caminho anterior. O custo do pré-passe é medido dentro
da região de GPU "Post".

## 3. A/B/B/A (Vila em Play, nível S)

| Rodada | Névoa resolvida | FPS | GPU quadro | Opaco | Pós | pós/opaco |
|---|---|---|---|---|---|---|
| 01-A | não | 26,19 | 35,68 | 14,81 | 15,37 | 1,038 |
| 02-B | sim | 31,39 | 29,76 | 14,79 | 9,34 | 0,631 |
| 03-B | sim | 20,57 | 45,41 | 22,30 | 14,08 | 0,632 |
| 04-A | não | 17,23 | 54,80 | 22,72 | 22,94 | 1,009 |

- Pós **−38,3%** (pós/opaco 1,024 → 0,632), estável nos dois estados de clock.
- Mesmo clock (01-A × 02-B): pós 15,37 → 9,34 ms, quadro 35,68 → 29,76 ms (**−16,6%**),
  FPS 26,19 → 31,39 (**+19,9%**).
- Cada rodada confirmou no log o caminho pedido (`[PostFogResolve]` presente só em B).

## 4. Imagem

Moradores se movem ao acaso, então a comparação usa os pixels estáticos (idênticos
entre as duas rodadas A e entre as duas B): 97,3% da tela.

- |B−A| ≤ 3 níveis em 99,63% dos pixels estáticos; média 0,14. A diferença de 1–2 níveis
  vem do alvo intermediário RGBA16F.
- 0,13% acima de 10 níveis: contorno de 1 px de silhuetas finas contra o céu enevoado
  (postes, beirais). Antes a neblina era aplicada sobre a cor já misturada com uma
  única profundidade; agora cada texel recebe a sua e só depois é misturado. Nos
  recortes ampliados não há halo nem diferença visível.
- Fileira y=0 (1 px acima da barra da UI): 16,16,29 → 27,30,37.

## 5. Decisão

Promovida como padrão (`aether.post_fog_resolve`, padrão `true`; `false` volta ao
caminho anterior para A/B). APK final 0.3.0-dev.98-g1, SHA-256
`EAEB50244ED3D9CB75F1C5516489A53935DFCEBABFED60794660D514129ED4E4`, instalado sem
apagar dados; Play da Vila abre com o caminho novo e imagem correta.

Não resolvido aqui: o restante do pós (bloom de 8 amostras, AO de 12, FXAA) e a
neblina por material (forma definitiva, junto do G2/G4); custo de memória do alvo
RGBA16F extra (resolução interna × 8 bytes, ~28 MB a 2772×1280); não medido no
nível C nem com TAA/ampliador temporal, onde a candidata não roda.
