# G1/G2 · pós: bloom em cadeia de mips e AO em meia resolução (17–18/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`. Base de comparação: o pós
com a névoa já resolvida por texel (promovida no mesmo dia). POCO F7, Astra Dev
`dev.aether.editor.u07` 0.3.0-dev.100-g1c (SHA-256
`493E3AD326DB99D40066D4B77302F35E0C52262646ED51BDF5206B7428449E67`), Vila Astra em
Play, nível S, carregador ligado. Comparação por pós ÷ opaco, porque o clock variou
entre rodadas no experimento do bloom.

## Implementação (`native/platform/android/instanced_post_chain.inl`)

- **Bloom em cadeia** (`aether.post_bloom_chain`): pré-filtro com limiar suave e média
  de Karis em 1/2, descida em 5 níveis até 1/32 com o filtro duplo de Kawase (5
  leituras) e subida com tenda de 8 leituras somada em cada nível (mistura aditiva).
  O passe principal lê só o nível 1/2. Referência: M. Bjørge, *Bandwidth-Efficient
  Rendering*, ARM, SIGGRAPH 2015; bloom da URP
  (https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/post-processing-bloom.html).
- **AO em 1/2** (`aether.post_half_ao`): o mesmo SSAO de 12 amostras num alvo R8 com
  1/4 dos pixels; reconstrução no passe principal com `textureGather` das quatro
  amostras bilineares, ponderadas pela profundidade de resolução cheia.
- Os dois usam o shader do pós com bits de modo (2048, 8192, 16384, 32768 e, no
  passe principal, 1024/4096) e conjuntos copiados do conjunto do pós; o layout do
  pós ganhou os bindings 8 (bloom) e 9 (AO). Os pré-passes ligam alvos que nunca são
  o próprio destino. Prévia de câmera e caminho temporal não executam a cadeia.

## Bloom em cadeia (A = base, B = cadeia)

| Rodada | FPS | GPU quadro | Opaco | Pós | pós/opaco |
|---|---|---|---|---|---|
| 01-A | 28,44 | 32,25 | 16,04 | 10,05 | 0,627 |
| 02-B | 20,21 | 46,36 | 22,85 | 14,22 | 0,622 |
| 03-B | 20,33 | 45,80 | 22,72 | 14,05 | 0,618 |
| 04-A | 30,76 | 26,36 | 13,12 | 8,29 | 0,632 |

A queda de FPS em B é clock (o opaco também subiu ~42%). Normalizado: **pós −1,5%**,
custo neutro. A rodada 04-A foi repetida após o POCO cair do ADB sem fio na primeira tentativa.
**Aparência diferente**: o brilho se espalha sobre a praça ensolarada e o céu, com menos
contraste (`bloom-A01-B02.png`). Não promovido: é decisão visual e entra no G2, com
intensidade/limiar ajustáveis e perfis de pós.

## AO em meia resolução (A = base, B = AO em 1/2)

| Rodada | FPS | GPU quadro | Opaco | Pós | pós/opaco |
|---|---|---|---|---|---|
| 01-A | 30,12 | 30,70 | 15,45 | 9,46 | 0,612 |
| 02-B | 31,07 | 29,34 | 15,29 | 8,16 | 0,534 |
| 03-B | 31,35 | 29,17 | 15,27 | 8,12 | 0,532 |
| 04-A | 29,94 | 30,72 | 15,24 | 9,53 | 0,625 |

Clock estável. **Pós −13,9%** (−1,35 ms), **quadro −4,7%**, FPS +3,7%. Imagem: 97,3% dos
pixels estáticos comparáveis; |B−A| ≤ 10 níveis em 99,0%, máximo 36, nas juntas da
pedra, levemente mais suaves (`ao-A01-B02.png`).

**Não promovida**: o ganho de quadro (4,7%) fica abaixo do limite de 5% do protocolo.
Em aparelhos em que o pós pesa mais no quadro (nível C, resolução maior que a do
opaco) o ganho tende a ser maior; reavaliar com os dados de campo do bloco GT.

## Estado

As duas candidatas ficam no código, desligadas por padrão, com chaves de diagnóstico
e checagem no `tools/measure-g0-baseline.ps1` (`-BloomChain`, `-HalfAo`). O pós da Vila
em S está em ~8–9,5 ms (era 15–18 ms na linha de base); o maior custo agora é o opaco
(~15 ms) e a CPU (18–21 ms), próximos alvos do G1.
