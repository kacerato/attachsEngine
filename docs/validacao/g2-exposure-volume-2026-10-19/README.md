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
