# M09–M10: contrato dos fontes fornecidos

Auditoria estática em 20/10/2026. Fonte: `build/terrain-packages/MicroSplat - Terrain Collection v3.9.41/Packages/com.jbooth.microsplat.global-texture/`, versão interna **3.9.0 / Unity 2019.4**. Não comprova execução Unity, Astra ou Android. Este documento complementa `PROXIMO-M09-M10-2026-10-20.md`.

## Conversão completa e explícita

Proposta de API: `convertMicroSplatGlobalMaterial(savedProperties, keywords, propData, guidResolver, options) -> {modules, sourceProperties, sourceKeywords, sourcePropData, diagnostics}`. A resolução recebe GUID **e fileID**, pois curvas/texturas geradas podem ser subassets. Não basta um GUID para escolher subasset. Converter em objeto temporário; publicar somente após validação completa. Opções não consumidas permanecem na proveniência, com diagnóstico por campo; modo estrito recusa capacidades ativas sem consumidor, referências faltantes e enums desconhecidos. Campos desativados também são preservados. Nunca importar um material completo silenciosamente como preset parcial.

| Campo original | Conversão / contrato |
| --- | --- |
| `_GeoTex`, `_GeoNormal` | GUID/fileID para mapas; cor e normal têm papéis distintos; normal linear e mip de normal. Não inferir packing pelo nome. |
| `_GeoParams.x` | Blend global multiplicado pelo alfa Geo. |
| `_GeoParams.y` | Frequência vertical: `scaleMeters=1/frequency`; zero/negativo não representável em escala positiva exige diagnóstico, sem clamp silencioso. |
| `_GeoParams.z` | Offset em UV vertical: `offsetMeters=offsetUV*scaleMeters`. Apesar do rótulo World Offset no editor original, shader soma depois da frequência. |
| `_GeoParams.w` | Reservado, preservar na proveniência. |
| `_GeoNormalStrength` | Força normal, multiplicada por blend/alfa/faixa/participação/filtro. |
| `_GeoRange.xyzw` | Quatro alturas mundiais; rampas **lineares** `saturate((y-a)/max(b-a,.0001))` e saída equivalente. Alturas coincidentes no original produzem rampa de .0001 m; não reordenar na leitura. |
| `_GeoCurveParams.x/y/z/w` | Escala horizontal em metros; offset único aplicado **a X e Z antes da rotação**; radianos para graus `*180/pi`; W reservado. Original efetiva `max(scale,.01)`, que deve ser diagnosticada se não preservada. |
| `_GeoCurve` + `propData.geoCurve` | LUT R acrescentada diretamente ao **UV vertical**, sem amplitude extra; equivalente em metros é `LUT*scaleMeters`. Eixo da curva é `((worldX+offset)*cos(angle)+(worldZ+offset)*sin(angle))/curveScale`. Preservar chaves/tangentes/wrap e/ou LUT original com procedência; não substituir curva desconhecida por linear. |
| `_GeoHeightContrast` | Contraste do filtro; padrão do shader 10, embora controle editor use slider 0..1. Não limitar preset 10 a 1 silenciosamente. |
| `propData`, textura de parâmetros slot 5 R/G/B/A | Participações Geo/Tint/Normal/SAOM; slot 6 A = emissão; slot 16 A = specular. Sem keyword por textura, influência efetiva é 1. |
| Parâmetros por textura slot 8 B/A | Limiar assinado Geo e força; original `filter=max(1-strength, signedFilter)`, `signedFilter=saturate((abs(threshold)-height)*contrast+.5)` ou seu inverso. `mix(1,filter,strength)` é escolha diferente. Original usa alfa albedo; usar Height real é adaptação Astra explícita. |
| `_GlobalTintTex`, `_GlobalNormalTex`, `_GlobalSAOMTex`, `_GlobalEmisTex` | Quatro mapas independentes. Tint/SAOM usam alfa para mascarar; normal/emissão não usam alfa como máscara no fonte. |
| `_GlobalTextureParams.xyzw` | Intensidades Tint/Normal/SAOM/Emission, respectivamente. |
| `_Global{Tint,Normal,SAOM,Emis}Fade.xyzw` | Início/comprimento/perto/longe; `fadeEnd=start+length`. Fonte usa interpolação **linear saturada**, divisor `max(length,.01)`, não smoothstep. Comprimentos zero/negativos precisam de política explícita/diagnóstico. |
| `_Global{Tint,Normal,SAOM,Emis}UVScale.xyzw` | UV fonte `(domainUV+zw)*xy`; com contrato Astra `domainUV*scale+offset`, importar `scale=xy`, `offset=zw*xy`. Preservar valores zero/negativos ou diagnosticar se não representáveis. `TextureEnv.m_Scale/m_Offset` separados não são lidos por esses snippets; preservar como proveniência. |
| SAOM RGBA | R smoothness -> roughness `1-R`; G AO; B metallic; A máscara. Seleção/remap/inversão/fator permanecem explícitos; uma amostra compatível pode servir todos. |

Enums originais: `GeoTextureMode={Off:0,Albedo:1,AlbedoNormal:2}`; `GeoTextureBlendMode={Multiply2X:0,LightColor:1}`; `BlendMode={Off:0,Multiply2X:1,Overlay:2,LightColor:3,CrossFade:4}`; `NormalBlendMode={Off:0,NormalBlend:1,CrossFade:2}`; `SAOMBlendMode={Off:0,CrossFade:1}`. Converter por nomes/semântica, nunca reinterpretar inteiros entre enums diferentes. NormalBlend -> RNM é adaptação de algoritmo declarada. Participação Geo/Tint original 0..1; Normal/SAOM/Emission 0..2, portanto limitar todos a 1 perde presets.

## Pendências reconhecidas

`NormalPackMode={Separate:0,Packed:1}`: Packed usa **GA normal**, R smoothness, B AO; não interpretar como RG. `WrapMode={Clamp:0,Wrap:1}` e cada `SamplingMode={Linear:0,Quadratic:1}` exigem consumidor ou diagnóstico ativo. Quadratic usa quatro amostras biquadráticas do snippet, não um enum arbitrário de qualidade. `_GlobalSpecularTex/Fade/UVScale/Blend`, workflow specular, `_GeoSlopeTex`, `geoSlopeFilter`, `_GlobalSlopeTex`, `globalSlopeFilter`, `_GlobalTexNoiseUVParams`, `globalNoiseUV`, `splatFade`, `perTexSpecularStr`, presets Floor/Slopes/Cliff e dados de curvas de inclinação ficam preservados e diagnosticados enquanto não suportados. Presets são autoria, não novos modos de mistura. Não declarar o módulo fornecido inteiro concluído pelo recorte M09/M10.

O material de demonstração usa `_GeoParams=(1,.00125,13,0)`: escala **800 m**, offset **10400 m**; `_GeoCurveParams.z=.4`: **22.9183118 graus**. Fade `(50,150,0,1)` vira início 50/fim 200. `_GlobalTintUVScale=(2,2,0,0)` conserva offset zero. `_GlobalNormalTex` GUID `d40cd46ddae216d4c8509d450a2e1230` e `_GlobalTintTex` GUID `1a24e320ed74d43be8d727cef94006ff` estão ausentes no manifesto deste pacote. `_GeoTex` do demo aponta para `gradient.png`, não para o Geo default: defaults são uma seleção explícita, nunca reconstrução fiel desse material. `_GeoCurve`/`_GeoSlopeTex` são subassets de `MicroSplat_propdata.asset` com fileIDs diferentes.

## Assets originais incorporados

Arquivo de origem: `MicroSplat - Terrain Collection v3.9.41.unitypackage`; SHA256 `071cbf56ceebf0dc9e2edd8e1bec2ed8219315772808fa9d78406305949a6772`.

| PNG em `Packages/com.jbooth.microsplat.global-texture/Textures/` | GUID | SHA256 | Dimensão / bytes |
| --- | --- | --- | --- |
| `microsplat_def_geomap_01.png` | `863f19b183b2443859f959a96f7dce32` | `a3b69d0594c9e95b2cfc2f9b844d2ca6903a112e0856293de4d66c84e527851a` | 1×256 / 15544 |
| `microsplat_def_geomap_norm_01.png` | `192aaea3237044f608d663c9884e0c70` | `ff742cdcd52c1c87bd361d821dc6d46b4061f52007f5f6e169628730f2a0fe62` | 1×256 / 16092 |

`tools/prepare-terrain-global-textures.py` verifica manifesto/hash/dimensões e acrescenta apenas os dois registros de textura ao catálogo real. `sources/<sha>.png` e runtime usam bytes originais iguais; `thumbnails/<sha>.png` segue política derivada RGBA 160 px da biblioteca. Os 82 materiais e sua ordem permanecem iguais; nenhum preset/material falso foi criado e o atlas não é alterado. Execute esse helper **depois** de `prepare-terrain-library.py`, que reconstrói o catálogo. A integração não implica publicação de APK ou prova visual/runtime.

## Tempos originais da curva Geo

A auditoria encontrou uma restrição indevida na primeira implementação: exigir bordas0/1 não preservava toda curva original. `MicroSplatGlobalTexture.cs` usa CurveField sem retângulo restritivo e não atribui à curva o array modificado na tentativa de clamp. Chaves externas podem influenciar tangentes e interpolação dentro de0..1.

A implementação seguinte remove a restrição usando política Signed de autoria/validação no mesmo motor de curvas. APIs de animação existentes continuam NonNegative. Tempos, chaves, pesos, modos e IDs são preservados; não há deslocamento, recorte nem renormalização dos dados. Curva vazia produz zero; uma chave produz valor constante. A LUT de 256 amostra 0..1 com o avaliador original considerando todas as chaves. O gráfico cobre os tempos autorados, destaca 0..1 e permite seleção exata mesmo em curvas densas. Esta correção passou nos 64 cenários Core, 33 regressões de animação e casos do shader incluídos nos 199 modos GPU; não comprova importação automática de todo asset Unity, políticas adicionais de wrap nem paridade completa do pacote.
