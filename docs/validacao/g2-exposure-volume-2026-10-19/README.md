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

- **Limites em EV100**: os −1 / 14 do HDRP supõem sol de 100.000 lux. A luz da Astra não é física (sol ~2–3 unidades), então a cena ensolarada mede ~EV100 2 e a faixa útil é outra. Padrões da Astra: −6 / 10 (calibrados no POCO, ver abaixo).
- Curve Mapping, Physical Camera, Mask Weighted e Procedural Mask não existem na Astra (sem consumidor): não aparecem no menu.
- Caixa por **grupo**, não por propriedade (a Unity tem por propriedade).

## Migração

- Cena: Ambiente v15 (e v16, a das nuvens) → **v17**. Sem exposição automática antiga → **Fixa** com o mesmo multiplicador (EV100 = −EV antigo − log2 1,2): a cena abre igual. Com a automática antiga → Automática por histograma, limites convertidos e trocados de ponta, percentis ×100, velocidades trocadas. A caixa antiga "Sobrescrever pós" vira o valor de todas as caixas novas de pós. A caixa das nuvens (G4b) foi para o cabeçalho da seção Nuvens.
- Perfil de ambiente: v10/v11 → **v12**, mesma conversão.
- API C#: `AutoExposure*`, `ExposureEv` e `AutoExposure`/`AutoExposureCenterWeighted` deram lugar a `Exposure*`, `ExposureCompensation`, enums `ExposureMode`, `ExposureMetering`, `ExposureAdaptation` e `Override*`.
