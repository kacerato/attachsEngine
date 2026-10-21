# G2 — Exposure e overrides do Volume no modelo da Unity (19/10/2026)

## Referências (Unity HDRP 17, branch `master` de Unity-Technologies/Graphics)

| Arquivo | O que tiramos dele |
|---|---|
| [`Runtime/PostProcessing/Components/Exposure.cs`](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Runtime/PostProcessing/Components/Exposure.cs) | Campos e padrões: Mode Fixed, Metering Center Weighted, Limit Min −1 / Max 14, Compensation 0, Adaptation Progressive, Speed Dark→Light 3, Light→Dark 1, Histogram Percentages 40–90, Target Mid Grey 12,5% |
| [`Editor/RenderPipelineResources/DefaultSettingsVolumeProfile.asset`](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Editor/RenderPipelineResources/DefaultSettingsVolumeProfile.asset) | O perfil padrão do projeto sobrescreve só o Mode = **Automatic**; Tonemapping ACES; Bloom 0,2 |
| [`Editor/PostProcessing/ExposureEditor.cs`](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Editor/PostProcessing/ExposureEditor.cs) | O que o Inspector mostra em cada modo (Fixed: Fixed Exposure + Compensation; automáticos: Metering, Limits, Compensation, Percentages só no Histogram, Adaptation + velocidades só no Progressive, Target Mid Grey) |
| [`Runtime/PostProcessing/Shaders/Exposure.compute`](https://github.com/Unity-Technologies/Graphics/blob/master/Packages/com.unity.render-pipelines.high-definition/Runtime/PostProcessing/Shaders/Exposure.compute) e `ExposureCommon.hlsl` | Algoritmo: média ponderada do log da luminância; EV100 = log2(L·100/K); `adapt(ev − compensation)` e depois `clamp(Limit Min, Limit Max)`; pesos Spot (círculo de 7,5%) e Center Weighted (cai linearmente até a borda); adaptação `prev + Δ·(1 − 2^(−dt·speed))` |
| Manual HDRP — [Exposure](https://docs.unity3d.com/Packages/hdrp/manual/Override-Exposure.html), Volume e Volume Profile | Fluxo no editor |

## Como funciona na prática na Unity

1. A cena tem um **Volume** (Mode Global ou Local; Local usa um collider, **Blend Distance**, **Weight** e **Priority**) com um **Volume Profile**.
2. No perfil, *Add Override › Exposure*. Cada propriedade tem uma caixa à esquerda: marcada, o volume decide aquele valor; desmarcada, vale o que vier de baixo (volume de prioridade menor e, no fim, o **Default Volume Profile** do projeto).
3. O Default Volume Profile do HDRP já liga Exposure **Automatic**: uma cena nova sem nenhum override já tem exposição automática.
4. Em Automatic, o HDRP mede a luminância da câmera a cada quadro, converte para EV100, soma a compensação, adapta com velocidades diferentes para escuro→claro e claro→escuro, e limita a [Limit Min, Limit Max] para que a noite não vire dia.

## O que a Astra passou a fazer

- Componente **Ambiente** = Volume (Global/Caixa/Esfera, Peso, Prioridade, Distância de mistura, Camada — já existia).
- **Um override por efeito**, com a caixa no cabeçalho da subseção do Inspector: Neblina, Luz indireta, Exposição, Tonemapping, Bloom, Vinheta, Ajustes de cor, Cor, Grão de filme, Oclusão ambiente. Desmarcado = "Herdado". Antes era uma caixa só ("Sobrescrever pós") para tudo.
- **Exposição** com os modos que a Astra executa: Fixa, Automática, Automática por histograma. Medição Média/Pontual/Peso no centro, Limite mínimo/máximo em EV100, Compensação, Percentual baixo/alto (histograma), Adaptação Imediata/Progressiva, Escuro para claro / Claro para escuro, Cinza médio alvo 12,5/14/18%.
- Padrão do mundo (o "Default Volume Profile" da Astra) = **Automática**, Peso no centro, Progressiva, 3 / 1, 40–90%, cinza 12,5%.
- GPU: `exposure_histogram.comp` (pesos do HDRP) e `exposure_reduce.comp` (EV100, compensação, adaptação exponencial, limites), por câmera.

## Desvios declarados

- **Limites em EV100**: os −1 / 14 do HDRP supõem sol de 100.000 lux (meio-dia ~EV100 14). A luz da Astra não é física: na Vila o meio-dia mede ~0, o poente ~−2,7 e a noite ~−4,7 (cinza 12,5%). Padrões da Astra: **Limite mínimo −2, máximo 10** — o poente fica legível e a noite não vira dia.
- Curve Mapping, Physical Camera, Mask Weighted e Procedural Mask não existem na Astra (sem consumidor): não aparecem no menu.
- Caixa por **grupo**, não por propriedade (a Unity tem por propriedade).

## Migração

- Cena: Ambiente v15 (e v16, a das nuvens) → **v17**. Sem exposição automática antiga → **Fixa** com o mesmo multiplicador (EV100 = −EV antigo − log2 1,2): a cena abre igual. Com a automática antiga → Automática por histograma, limites convertidos e trocados de ponta, percentis ×100, velocidades trocadas. A caixa antiga "Sobrescrever pós" vira o valor de todas as caixas novas de pós. A caixa das nuvens (G4b) foi para o cabeçalho da seção Nuvens.
- Perfil de ambiente: v10/v11 → **v12**, mesma conversão.
- API C#: `AutoExposure*`, `ExposureEv` e `AutoExposure`/`AutoExposureCenterWeighted` deram lugar a `Exposure*`, `ExposureCompensation`, enums `ExposureMode`, `ExposureMetering`, `ExposureAdaptation` e `Override*`.

## Medições e capturas no POCO F7 (20/10/2026)

APKs `0.3.0-dev.118-exposicao` e `0.3.0-dev.119-exposicao` (canal `dev.aether.editor.u07`), Vila Astra com um
"Sol com dia e noite" de teste, câmera do Play fixa. EV100 lidos da linha `[AutoExposure]` do logcat.

| Arquivo | Situação | EV100 medido → adaptado |
|---|---|---|
| `01-inspector-fixa-migrada.jpg` | Vila v15 aberta no 118: Exposição **Fixa**, −0,413 EV100 (= os +0,15 EV antigos) | — |
| `02-poente-17h-fixa.jpg` | 17,4 h, Fixa: zênite preto, praça apagada (o defeito do G4) | — |
| `04-inspector-automatica.jpg` | Modo Automática: aparecem Medição, Adaptação, Cinza alvo, Limites e velocidades | — |
| `03-poente-17h-auto-sem-limite.jpg` | Automática com os limites migrados (−8,3 a 7,7) e cinza 18%: clareia demais, parece dia nublado | −3,21 → −3,21 |
| `05-meio-dia-12h-auto.jpg` | 12 h, Automática (cinza 18%) | −0,52 → −0,52 |
| `07-inspector-cinza-12-limite-2.jpg` | Cinza 12,5% e Limite mínimo −2 | — |
| `06-poente-17h-auto-limite-min-2.jpg` | 17,4 h: poente legível, céu avermelhado escurecendo no zênite | −2,69 → −2,00 |
| `08-noite-22h-auto-limite-min-2.jpg` | 22 h: o limite segura; noite de lua com estrelas | −4,70 → −2,00 |
| `09-editor-17h-acompanha-hora-119.jpg` | APK 119, vista de edição: trocar a hora atualiza a exposição na hora | — |
| `10-poente-17h-padrao-119.jpg` | APK 119, Play 17,4 h, padrões novos (Automática, Peso no centro, 12,5%, −2/10) | −2,66 → −2,00 |
| `11-meio-dia-12h-padrao-119.jpg` | APK 119, Play 12 h, mesmos padrões | 0,01 → 0,01 |

A Vila foi devolvida ao final com o app parado: md5 `2e2e996841f1f0eb5af1572c68dec3fc`, igual ao de antes.

## Conceito, implementado e testado

- **Implementado**: modelo, migração, Inspector, API C#, medição e adaptação na GPU, overrides por efeito.
- **Testado no host**: aether_tests 1596/1596; autoria de ambiente 46/46 (com os testes novos de padrões HDRP,
  EV100, adaptação, herança de override e migrações v15/v16 e perfil v10/v11).
- **Testado no aparelho**: Fixa migrada, Automática, cinza alvo, Limite mínimo, poente/meio-dia/noite, vista de
  edição acompanhando a hora. **Não testado no aparelho**: Automática por histograma, Medição Pontual/Média,
  Adaptação Imediata, volume local com Exposição desmarcada (cobertos só no host).

### Defeito achado no aparelho e corrigido (119)

O editor redesenha sob demanda com o relógio parado: a adaptação Progressiva nunca andava e, depois de
mudar a hora, a vista de edição ficava presa na exposição antiga. Agora, sem tempo correndo (ou com salto
longo), a exposição vai direto ao alvo; o Play pausado continua congelado.

## Referência em vídeo da Unity (frames em `referencia-unity/`)

| Frame | Vídeo e instante | Para que serve |
|---|---|---|
| `unite2020-yqCHiZrgKzs-23m21s-inspector-exposure.jpg` | Unity, *Lighting & rendering tutorial for HDRP — Unite Now 2020*, [23:21](https://www.youtube.com/watch?v=yqCHiZrgKzs&t=1401s) | Inspector do override Exposure (Automatic, Average, Limits, Progressive) |
| `unite2020-yqCHiZrgKzs-17m36s-faixas-ev100.jpg` | mesmo vídeo, [17:36](https://www.youtube.com/watch?v=yqCHiZrgKzs&t=1056s) | Faixas de EV100 do HDRP: noite −3..0, lua 0..3, interior 3..6, poente 6..8, nublado 9..12, sol 13..16 |
| `unite2020-yqCHiZrgKzs-24m51s-noite-praca.jpg` | mesmo vídeo, [24:51](https://www.youtube.com/watch?v=yqCHiZrgKzs&t=1491s) | Noite com exposição automática: praça escura, lampiões acesos |
| `unite2020-yqCHiZrgKzs-25m51s-curve-mapping.jpg` | mesmo vídeo, [25:51](https://www.youtube.com/watch?v=yqCHiZrgKzs&t=1551s) | Curve Mapping (não implementado) |
| `hdrp-dusk-QCbGEsLGZsA-06m40s-poente-final.jpg` | Seta — Level Design, *Realistic Lighting in Unity HDRP: Dusk / Sunset*, [6:40](https://www.youtube.com/watch?v=QCbGEsLGZsA&t=400s) | Poente no HDRP e o Volume com overrides (ALL/NONE, caixa por propriedade) |
| `hdrp-dusk-QCbGEsLGZsA-03m20s-sol-baixo.jpg`, `-09m20s-poente-cena.jpg`, `-00m02s-resultado.jpg` | mesmo vídeo, [3:20](https://www.youtube.com/watch?v=QCbGEsLGZsA&t=200s), [9:20](https://www.youtube.com/watch?v=QCbGEsLGZsA&t=560s), [0:02](https://www.youtube.com/watch?v=QCbGEsLGZsA&t=2s) | Céu e luz quente com o sol baixo |
| `hdrp-dia-noite-sjSAP7USx2I-00m04s-dia.jpg`, `-00m07s-noite.jpg` | 988u, *Day/Night Cycle — Unity HDRP*, [0:04](https://www.youtube.com/watch?v=sjSAP7USx2I&t=4s), [0:07](https://www.youtube.com/watch?v=sjSAP7USx2I&t=7s) | Dia e noite no HDRP: a noite fica escura |
| `autoexposure-6oPLQbfhJfE-02m08s-histograma.jpg`, `-02m32s-parede-clara.jpg`, `-02m40s-adaptando.jpg` | Omar Hidoussi, *How to setup Auto Exposure effect in Unity correctly*, [2:08](https://www.youtube.com/watch?v=6oPLQbfhJfE&t=128s), [2:32](https://www.youtube.com/watch?v=6oPLQbfhJfE&t=152s), [2:40](https://www.youtube.com/watch?v=6oPLQbfhJfE&t=160s) | Histograma e a adaptação ao chegar perto de uma parede clara (pós v2, mesmo princípio) |

## Pranchas Unity × POCO e o julgamento

- `prancha-poente-unity-x-poco.jpg`: a **exposição** é comparável (cena bem exposta). O que difere é o **céu e a cor da luz**:
  no HDRP o céu acima do sol continua claro e dourado e a luz rasante é quente; na Astra o zênite fica preto e o
  chão sai cinza-claro. Causa: céu físico do G4 sem espalhamento múltiplo e cor da direcional no sol baixo. Fica
  como pendência do G4, não da exposição.
- `prancha-noite-unity-x-poco.jpg`: a noite da Astra fica **clara demais**. A noite da Vila mede só ~4,7 EV abaixo do
  meio-dia (no HDRP físico são ~15 EV, segundo as faixas do slide), então o Limite mínimo −2 ainda a clareia
  ~2,7 EV. O limite foi mantido em −2 porque o poente precisa dele; a radiância noturna do céu/lua é pendência do G4.
- `prancha-dia-unity-x-poco.jpg`: dia com os médios no cinza nas duas; o vídeo é dia nublado, a Vila é céu limpo.
- `prancha-inspector-unity-x-poco.jpg`: mesma estrutura (Modo, Medição, Compensação, Limites, Adaptação e
  velocidades). A Unity tem caixa **por propriedade** (desmarcada fica cinza com o valor herdado) e ALL/NONE; a
  Astra tem uma caixa por seção — próximo passo do Volume (V-02 da auditoria).

O que mudou por causa da comparação: o padrão de cinza ficou em 12,5% (o do HDRP), os limites ficaram em −2/10
(o poente sem limite, a −3,2, saía mais claro que o do vídeo) e a noite clara foi registrada como defeito do céu.

## PK-5 parcial: Tonemapping, Color Adjustments e Bloom (APK 170, 21/10/2026)

Referências: URP/HDRP 17 *Tonemapping* (None, Neutral, ACES), *Color Adjustments* (Post Exposure, Color Filter,
Hue Shift), *Bloom* (Scatter, Tint, Clamp); `Color.hlsl` (`NeutralTonemap`), `LutBuilderHdr.shader` (ordem:
filtro e matiz antes da curva) e `VolumeParameter.Interp` (valor discreto troca assim que t > 0).

| Arquivo | O que mostra |
|---|---|
| `14-menu-tonemapping-170.jpg` | Menu na ordem da Unity: Nenhum, Neutro, ACES, AgX (extra); Reinhard só existe como legado e abre como Neutro |
| `12-inspector-bloom-ajustes-170.jpg` | Bloom com Espalhamento (0 na cena migrada: o raio de antes), Tom e Brilho máximo; Ajustes de cor com Pós-exposição, Filtro de cor e Giro de matiz |
| `13-giro-de-matiz-60-170.jpg` | Giro de matiz em 60°: a vista de edição muda na hora |
| `15-neutro-x-nenhum-170.jpg` | Neutro (esq.) × Nenhum (dir.): o Nenhum corta os altos |

Host: aether_tests 1606/1606 e autoria de ambiente 48/48 (testes novos: menu e migração Reinhard → Neutral,
campos novos na cena e no perfil, mistura por override, troca discreta em t > 0). O build Android do merge com a base
(G3a, nuvens, PK-3/PK-8) compilou no clang (APK 170). Não testado no aparelho: Espalhamento > 0, Tom e Brilho máximo
do bloom, Pós-exposição e Filtro de cor (só no host). A Vila foi devolvida com md5 `2e2e996841f1f0eb5af1572c68dec3fc`.
