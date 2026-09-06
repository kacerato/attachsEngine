# Plano — água, céu e atmosfera de nível profissional

- **Estado:** proposto; nenhuma fase implementada
- **Data:** 04/09/2026
- **Depende de:** ADR-014 (política global), ADR-016 (compute no RHI), ADR-017 (água nativa)
- **Substitui:** a linha "água" de `docs/VISIBILIDADE-E-AGUA.md`, que passa a apontar para cá

## 0. Contrato de entrega

Este documento é plano, não entrega. Nenhum número de custo aqui é medição: são
**alocações de orçamento** que cada fase precisa provar no aparelho antes de entrar
em `Auto`. Onde escrevo "estimado", é estimativa e será substituída por medição.

Três regras herdadas do projeto e mantidas sem exceção:

1. Propriedade serializada sem consumidor real **não conta** como entregue. O plano
   audita isso a cada fase, porque a auditoria de 04/09 encontrou quatro APIs e um
   slider inteiro nessa condição.
2. Nada depende de nome de cena, de material ou de perfil nominal dentro do shader.
   Toda configuração nova nasce como eixo global versionado.
3. Solicitado, resolvido, indisponível e fallback são estados distintos e visíveis.

## 1. Diagnóstico medido

Tudo nesta seção foi verificado no código, não inferido.

### 1.1 Por que a textura tem "quadradinhos"

O normal map da água é gerado por `tools/build-ocean-demo.py:38`. Quatro causas
somam-se, e nenhuma delas é resolvida trocando a textura por outra:

| # | Causa | Evidência | Efeito visível |
| --- | --- | --- | --- |
| 1 | Apenas **5 harmônicos**, todos com **fase zero** | `spectrum = ((3,5,.44),(-7,2,.28),(11,-9,.16),(17,13,.075),(-29,19,.035))` e `cos(phase)` sem offset | Interferência de 5 cossenos alinhados em u=v=0 forma uma **treliça regular**, não ruído gaussiano. É literalmente um padrão xadrez. |
| 2 | Período de repetição de **32,3 m e 17,5 m** | `uv0=xz*0.031`, `uv1=rotation*xz*0.057` em `water_surface.frag:41` | Duas repetições curtas, correlacionadas porque usam **a mesma textura** → padrão plaid em escala de oceano. |
| 3 | **RGBA8** para inclinação | `AETX_RGBA8_UNORM` em `write_water_normal` | Os dois harmônicos finos contribuem ~0,059 e ~0,029 de inclinação → cerca de **15 e 7 níveis de quantização**. O detalhe fino vira degrau. |
| 4 | **Sem filtragem anisotrópica** | `maxAnisotropy` nunca é definido em `dirt_road_resources.cpp:150-165` | Um plano visto quase de perfil — que é toda a vista de um oceano — recebe seleção isotrópica de mip. As transições de mip viram **faixas concêntricas** e a superfície achata. |

Uma superfície de mar real é **gaussiana**: a soma de centenas de componentes com
**fase aleatória independente**. Cinco cossenos em fase nunca produzirão isso, em
nenhuma resolução. Por isso a fase 1 do plano não é "arrumar a textura" — é
**substituir a fonte do espectro**.

### 1.2 Por que o céu estica

`dirt_road_sky.frag` faz uma amostragem de panorama equiretangular de
**1024×512** (`samples/ocean/manifest.json`, confirmado pelo tamanho de
`environment.aetex`: 1024·512·4·1,333 + header = 2 796 236 bytes).

A projeção usa `1.732050808 = cot(30°)`, logo meio-FOV vertical = 30°, FOV
vertical = 60°. Em 2772×1280 (aspecto 2,166), o FOV horizontal é
2·atan(tan30°·2,166) = **102,7°**.

- Horizontal: 102,7° de 360° = **292 texels** esticados sobre 2772 px → **9,5 px
  por texel**.
- Vertical: 60° de 180° = **171 texels** sobre 1280 px → **7,5 px por texel**.

O céu está subamostrado cerca de **10×**. Não é bug de shader; é resolução de
fonte. Somam-se dois agravantes:

- `textureLod(environmentMap, uv, 0.0)` força **mip 0** sempre. Ao subir a
  resolução, isso passa a produzir aliasing severo no horizonte.
- A costura de `fract()` na UV faz a derivada explodir na emenda; hoje isso está
  mascarado pelo `textureLod` fixo e vira uma linha vertical assim que houver mip.
- Perto do zênite, `acos(y)/PI` comprime toda a linha de texels num ponto — o
  pinçamento de polo clássico do equiretangular.

Sobre "o céu se mexe": rotacionar com a câmera está **correto** — `vDirection`
usa só rotação, nunca translação. O que se percebe como movimento errado é o
borrão de 9,5 px/texel deslizando. Some a causa e some o sintoma.

### 1.3 Débitos herdados que este plano fecha

Da auditoria de 04/09, todos verificados no código:

1. Absorção cromática morta — `transmittance` é `vec3` mas só a luminância é usada
   (`water_surface.frag:76-78`).
2. Espuma translúcida — o `mix` de espuma não levanta `compositeAlpha`
   (`water_surface.frag:96`); na costa o alpha fica ≈0,04.
3. Slider "Inclinação / cristas" inerte — `waterWaveMotion[i][1]` não é lido por
   nenhum shader.
4. `planWaterClipmap`, `resolveWaterPipeline`, `waterCoverage` e
   `sampleWaterSurface` sem consumidor de runtime.
5. `WaterProfile` nativo (v1, struct) e managed (v2, JSON) desconectados;
   `WaterProfileMagic` declarado e nunca usado; `foamDecay` (nativo) vs
   `FoamStrength` (managed) para o mesmo campo, com o managed correto.
6. Toque intersecta o plano y=0, ignorando onda e `waterBaseHeight_`.
7. `speed` ocupa o lugar de ω, então a dispersão é autorada e arbitrária.

## 2. O pacote de referência

Inventariado em `C:/Users/donod/Downloads/extracted/extracted` (KWS 1.4.03,
licença detida pelo proprietário, autorizada em 04/09/2026). Continua sendo
**referência de amplitude funcional, não dependência** — os scripts são
UnityEngine e não carregam na Aether.

O que a inspeção deste plano extraiu e que a ADR-017 ainda não registrava:

- **Três cascatas FFT** com domínios `{20, 40, 160}` m (`KWS_Settings.cs:30`).
  LOD0 na resolução escolhida (32…512), LOD1/2 fixos em 64 ou 128
  (`FFT_GPU.cs:81-85`).
- Saídas por cascata: `DisplaceTexture` ARGB float (deslocamento xyz) e
  `NormalTexture` RGBA16F **com cadeia de mips** (`FFT_GPU.cs:131-135`).
- Caminho de flutuação separado: `HeighDataTexture` R16F + compute buffer, com
  requisição assíncrona de vida útil de 10 frames.
- Quadtree de malha com atualização por histerese: 3 m para frente, 0,5 m para
  trás, 1° de rotação (`KWS_Settings.cs:18-20`); LOD do chunk escalado pelo vento
  em `{0.5, 0.75, 1, 1.5, 2, 2.5}`.
- `MaxWindSpeed = 15`, `MaxNormalsAnisoLevel = 4`, `MaxRefractionDispersion = 5`,
  `OrthoDepthResolution = 2048`, `ShorelineWavesTextureResolution = 2048`.
- Superfície de autoria completa em `WaterSystemScriptableData.cs`: 90
  propriedades, mapeadas integralmente na seção 8.

**Onde vamos além da referência**, e por quê:

| Ponto | Referência | Aether | Motivo |
| --- | --- | --- | --- |
| Domínios de cascata | 20 / 40 / 160 m (razões 1:2:8) | **19,7 / 51,3 / 163,1 m** | Razões inteiras realinham as cascatas a cada 160 m e recriam repetição visível. Domínios mutuamente não-comensuráveis não repetem dentro do alcance visível. |
| Fonte da espuma | limiar sobre dados de onda | **Jacobiano do deslocamento** | Jacobiano negativo = dobra da onda = whitecap. É a origem física da espuma, não um limiar ajustado. |
| Cintilação a distância | mips da normal | mips **+ variância de inclinação dobrada na rugosidade** (LEAN/Toksvig) | Frequência abaixo do pixel vira rugosidade especular em vez de ruído de normal. Remove shimmer sem borrar. |
| Céu | cubemap/HDRI + reflexão | **atmosfera pré-computada** (LUTs) | Um céu analítico e suave magnifica sem esticar, acompanha o sol em movimento e alimenta a reflexão da água com a mesma radiância. |

## 3. Arquitetura alvo

Três camadas, com fronteira dura entre elas:

```
AUTORIA (backend-neutral, versionada, serializável)
  WaterProfile v3 ....... espectro, óptica, espuma, costa, interação, malha
  AtmosphereProfile v1 .. densidades de Rayleigh/Mie, ozônio, sol, chão
  WaterQualityTier ...... S/A/B/C resolvido por ADR-014, nunca por cena
        |
RESOLUÇÃO (capacidades + orçamento + térmica)
  ResolvedWaterPipeline .. cascatas ativas, resolução, refração, reflexão,
                           interação, cadência de cada passe, fallback nomeado
        |
BACKEND (Vulkan; nenhum tipo Vulkan sobe para as camadas acima)
  compute: espectro, FFT, normais+mips, dynamic waves, fluidos, cáusticas
  gráfico: clipmap → subpass 0 opacos/céu → subpass 1 água → pós
```

`WaterProfile` v3 unifica nativo e managed num só esquema, com migração
explícita de v1/v2 e `WaterProfileMagic` finalmente usado no cabeçalho binário.
`foamDecay` é renomeado para `foamStrength` (o managed já está certo).

## 4. Fases da água

Cada fase tem entrega, orçamento alocado e gate. Nenhuma fase entra em `Auto`
sem passar as portas da seção 9.

---

### A0 — Zero medido da cena oceânica *(pré-requisito de tudo)*

A cena `ocean` **nunca foi medida**. O `dirt-road` tem zero (115,56 presents/s,
GPU p95 7,08–7,11 ms), a água não tem. Sem isso não existe teste de regressão.

- Rota AERT fechada para o oceano via `tools/generate-camera-sweep.py`: horizonte
  raso, mergulho, sobrevoo rápido e pose parada.
- Registrar p50/p95/p99 por passe, escala interna, espera de apresentação e
  primeiro frame.
- Repetir com clock normal e após soak. A memória do projeto já registra que
  GameTurbo, Game Mode e estado de clock invalidam rodadas: a rodada válida é a
  documentada.

**Entrega:** `build/android-validation/ocean-zero-*` + tabela no plano.
**Orçamento:** n/a. **Gate:** existir e ser reproduzível.

---

### A1 — Correções de crédito imediato

Defeitos já localizados, baratos, e que mudam a imagem hoje. Entram antes da
reconstrução para que o zero de A0 não seja medido sobre bugs.

1. **Espuma opaca:** `compositeAlpha = max(compositeAlpha, foam)` — a espuma
   passa a ocluir o fundo. Corrige a espuma de costa invisível.
2. **Anisotropia:** `maxAnisotropy` até 4 no sampler da normal da água, atrás de
   `WaterFiltering.anisotropy` (eixo global, capability-gated).
3. **Toque na superfície real:** `waterHitFromScreen` passa a iterar 3 vezes
   contra `sampleWaterSurface` + `waterBaseHeight_` em vez do plano y=0.
4. **Slider inerte:** "Inclinação / cristas" é removido do painel até A3
   entregar deslocamento horizontal. Um controle morto contamina a bancada.
5. **Nomes:** `foamDecay` → `foamStrength`, nativo e managed no mesmo esquema.

**Orçamento:** ≤ 0,05 ms (só anisotropia tem custo). **Gate:** captura A/B da
linha de costa e do horizonte raso.

---

### A2 — Malha: clipmap instanciado

Hoje: grade estática 257×257, passo 3 m, 768×768 m, 131 072 triângulos, um draw
fixo, sem LOD, sem alcance.

`planWaterClipmap` **já está correto e testado** — 4 patches centrais + 12 por
anel, `MaximumWaterPatches = 88`. Falta só a malha e a instanciação.

Alvo: um único patch de **33×33 vértices (1024 quads)** num vertex buffer de 1089
vértices, instanciado 88 vezes com origem/tamanho/nível por instância.

| | Hoje | A2 |
| --- | ---: | ---: |
| Célula junto à câmera | 3,00 m | **0,50 m** |
| Alcance | 768 m | **8 192 m** |
| Triângulos | 131 072 | **180 224** |
| Draws | 1 | **1 instanciado** |
| Vértices em buffer | 66 049 | **1 089** |

Seis vezes mais densidade perto, dez vezes mais alcance, 1,4× os triângulos.

- Costura 2:1 entre níveis resolvida por `WaterPatch::skirtDepth`, que já existe
  no contrato.
- Snap da origem à grade do próprio nível (já implementado) impede a geometria
  distante de nadar sob movimento sub-patch.
- Histerese de atualização no estilo da referência: reconstruir o plano ao andar
  3 m para frente, 0,5 m para trás ou girar 1°.
- `maximumDistance` e `basePatchSize` passam a ter consumidor real.

**Orçamento:** vértice ≤ 0,45 ms com espectro completo. **Gate:** rota de
aproximação/afastamento sem popping, sem costura e sem nadar.

---

### A3 — Espectro: três cascatas FFT em compute

O coração da "densidade, ondas e fluidez". Roda sobre `VulkanComputeKernel` /
`VulkanComputeContext` (ADR-016), preferindo a fila compute dedicada.

**Cascatas:** domínios 19,7 / 51,3 / 163,1 m. Resolução por tier:

| Tier | C0 | C1 | C2 | Cadência |
| --- | ---: | ---: | ---: | --- |
| S | 256² | 128² | 128² | C0 a 40 Hz, C1/C2 alternando |
| A | 256² | 128² | 128² | C0 a 30 Hz, C1/C2 alternando |
| B | 128² | 64² | — | 30 Hz, 2 cascatas |
| C | — | — | — | fallback analítico (Gerstner atual) |

O espectro não precisa da cadência de render: a superfície evolui centímetros em
33 ms. Amortizar por cascata e submeter em async compute é o que faz o custo
caber. **Nenhuma cascata é interpolada por reprojeção** — cada uma é função do
tempo absoluto, então reavaliar em 30 Hz é exato, só menos frequente.

**Pipeline por cascata:** `SpectrumInit` (uma vez por mudança de vento) →
`SpectrumUpdate` (evolução temporal) → FFT butterfly log₂N passos × 2 direções ×
2 FFTs complexos empacotados → `Assemble` (deslocamento xyz + jacobiano) →
`NormalsAndMips`.

**Saídas por cascata:**
- `Displacement` RGBA16F — xyz + jacobiano no alpha.
- `Slope` RG16F **com cadeia de mips completa**. RG16F em vez de RGBA8 elimina a
  quantização de 7 níveis diagnosticada em 1.1.
- `SlopeVariance` R16F por mip — a variância dobrada na rugosidade especular.

**Espectro autorável:** JONSWAP com `windSpeed` (0–25 m/s), `windDirection`,
`fetch`, `windTurbulence`, espalhamento direcional cosseno-2s, mistura
`swell`/`windSea`, `timeScale`, `choppiness` por cascata (λ do deslocamento
horizontal) e peso por cascata.

**Dispersão derivada, nunca autorada:** ω = √(g·k·tanh(k·d)), com `d` da
batimetria. Isso conserta o débito 1.3.7 e é pré-requisito para que o provedor
FFT e o analítico concordem — a promessa da ADR-017 de trocar o provedor sem
alterar `WaterSurface`.

**Espuma pelo jacobiano:** J < `foamThreshold` marca dobra de onda. Acumulada num
buffer persistente com decaimento (`foamStrength`, `foamFadeDistance`), não
recalculada do zero por frame — é assim que a espuma fica na crista e escorre.

**Flutuação:** readback assíncrono do heightmap da cascata 0 para um ring buffer
com validade temporal explícita, no modelo de 10 frames da referência.
`sampleWaterSurface` ganha um provedor: analítico (C) ou FFT (S/A/B). Mesma
assinatura, mesmo determinismo.

**Orçamento alocado:** 0,90 ms GPU para toda a simulação, em async compute. Se
exceder na medição, a queda é escalonada: C2 sai, depois C1 cai para 64², depois
C0 para 128². **Nunca** se paga excedente reduzindo qualidade próxima.
**Gate:** sweep longo em Adreno e Mali sem shimmer; espectro determinístico para
o mesmo tempo; readback concordando com o vértice dentro de 1 cm.

---

### A4 — Óptica: profundidade real

O que hoje é uma interpolação entre duas cores autoradas vira transporte.

1. **Color input attachment.** A água já é o subpass 1 do mesmo render pass — a
   cor opaca **está no tile**. Adicionar um input attachment de cor custa zero
   DRAM e destrava absorção cromática verdadeira: `dst · exp(−σ·d)` por canal, em
   vez do escalar de hoje. É a correção do débito 1.3.1 e a resposta ao pedido de
   "mais transparente": a transparência passa a emergir da espessura, ficando
   límpida no raso e densa no fundo, em vez de sair de um clamp.
2. **Refração com distorção.** Requer imagem de cor amostrável (subpass lê só o
   pixel corrente). Cópia de cor em meia resolução após os opacos, UV deslocada
   pela normal, com **guarda de profundidade**: se o pixel amostrado estiver na
   frente da água, cai para a amostra não-deslocada. Sem essa guarda aparece o
   artefato clássico de objeto em primeiro plano "vazando" para dentro d'água.
   Eixos: `refractionMode` (Off/Simple/Physical), `refractionStrength`,
   `refractionDepth`, `dispersion`, `dispersionStrength` (≤5, como a referência).
3. **Batimetria como fonte de profundidade**, não só o depth da tela. O chão já
   existe (96 segmentos, 2,25–32 m). Profundidade fora da tela deixa de ser
   inventada.
4. **Reflexão em tiers:** `Environment` (LUT da atmosfera de C1, sempre
   disponível) → `ScreenSpace` (com preenchimento de buracos e estiramento de
   borda) → `Planar` (culling mask e cadência próprios). `resolveWaterPipeline`
   ganha seu primeiro consumidor real.
5. **Sol refletido** com lóbulo próprio, disco analítico e força autorada, mais
   **reflexão anisotrópica** ao longo da direção do vento.

**Orçamento:** input attachment ≤ 0,10 ms; refração meia-res ≤ 0,35 ms; SSR ≤
0,60 ms (opt-in, nunca em `Auto` no tier B/C).
**Gate:** cena com objetos submersos, prova visual raso/fundo, e a borda da tela
sem estiramento em SSR.

---

### A5 — Interatividade completa

O pedido explícito. Quatro níveis, todos publicando no mesmo contrato.

| Nível | Mecanismo | Estado |
| --- | --- | --- |
| 1 | 8 impulsos radiais analíticos | existe |
| 2 | **Dynamic waves**: equação de onda 2D em textura compute | novo |
| 3 | **Fluidos 2D**: advecção de velocidade ao redor de objetos | novo |
| 4 | **Flow map**: correnteza autorada para rios | novo |

**Dynamic waves** é o salto que dá interferência real, reflexão em obstáculos e
esteiras — coisas que 8 impulsos analíticos nunca produzem. Textura centrada na
câmera, `dynamicWavesAreaSize`, `resolutionPerMeter`, `simulationFPS` (timestep
**fixo**, independente do render), `propagationSpeed`, `damping`. Máscara de
obstáculo alimentada pela cena para as ondas refletirem em cascos e pedras.

**Fontes de impulso**, todas no mesmo `WaterImpulse`/`WaterSplat`:
- toque curto → impulso radial;
- arrastar → esteira contínua;
- **corpos Jolt** → o projeto já tem física; um corpo cruzando a superfície gera
  splat proporcional ao volume deslocado e à velocidade;
- chuva → campo de impulsos poissonianos com `rainStrength`;
- **esteira de Kelvin** para embarcações — a cunha de 19,47° é analítica e barata,
  e é o detalhe que faz um barco parecer um barco.

**Orçamento:** dynamic waves 128² a 30 Hz ≤ 0,15 ms; fluidos opt-in ≤ 0,30 ms.
**Gate:** interferência entre dois impulsos visível e estável; timestep fixo
comprovado sob queda de FPS; zero alocação por frame.

---

### A6 — Costa, espuma e cáusticas

- **Ondas de costa** com perfis de quebra assados ao longo do litoral, LOD
  costeiro por distância e transição estável; textura de 2048² como na referência.
- **Espuma de costa** com cor, tamanho, distância de fade e recepção de sombra
  direcional; partículas no tier S.
- **Cáusticas** projetadas via ortho-depth (2048², área configurável), com
  bicúbica opcional, dispersão, escala por profundidade e LODs ativos.
- **Máscaras/buracos**: `WaterExclusionVolume` ganha upload de buffer, avaliação
  no shader e picking — o quarto consumidor ausente do débito 1.3.4.

**Orçamento:** cáusticas ≤ 0,45 ms, opt-in. **Gate:** costa sem cintilação em
aproximação; cáustica alinhada com a normal da superfície.

---

### A7 — Subaquático e volumetria

- Detecção por volume/câmera com transição contínua na travessia da superfície.
- Névoa por absorção real, blur configurável, e a superfície vista **de baixo**
  com reflexão interna total.
- Luz volumétrica (god rays) com resolução, iterações e raio de blur próprios.
- Ordenação configurável antes/depois de transparentes.

**Orçamento:** volumétrica ≤ 0,50 ms, opt-in, nunca em `Auto` abaixo do tier A.

---

## 5. Fases do céu e da atmosfera

### C1 — Atmosfera pré-computada *(substitui a foto)*

Modelo de espalhamento no estilo Bruneton–Hillaire, que é o que UE5 (Sky
Atmosphere), HDRP (Physically Based Sky) e Frostbite usam.

| LUT | Resolução | Formato | Cadência |
| --- | --- | --- | --- |
| Transmittance | 256×64 | RGBA16F | uma vez por atmosfera |
| Multi-scattering | 32×32 | RGBA16F | uma vez por atmosfera |
| Sky-view | 192×108 | RGBA16F | por frame (ou 2 em 2) |
| Aerial perspective | 32×32×32 | RGBA16F | por frame, opcional |

**Por que isso resolve o esticamento e a foto não resolve.** O sky-view LUT
também é pequeno — magnificação de ~14× na horizontal. A diferença é que a
radiância atmosférica é uma **função suave por construção**: um gradiente
magnificado por bilinear é indistinguível do original. Uma fotografia magnificada
10× destrói estrutura, e é exatamente isso que se vê hoje. O disco solar e as
nuvens são desenhados **separadamente, em resolução plena**, então o que precisa
ser nítido é nítido. Além disso, o eixo vertical do LUT usa parametrização
não-linear que concentra texels no horizonte, onde o gradiente é forte.

Ganhos que a foto não dá:
- **Sol dinâmico** — hora do dia, nascer e pôr corretos, sem trocar asset.
- **Perspectiva aérea** consistente com o céu, gratuita a partir do mesmo LUT.
- **Alimenta a água diretamente**: o vetor refletido consulta o sky-view LUT, e a
  reflexão da água passa a concordar com o céu por construção — não por um
  cubemap pré-filtrado que envelhece quando o sol se move.
- Disco solar analítico com escurecimento de limbo, para o glint especular.

`AtmosphereProfile` v1: raio planetário, altura da atmosfera, escala e coeficiente
de Rayleigh, escala e coeficiente de Mie, assimetria de Mie, camada de ozônio,
albedo do solo, intensidade e ângulo do sol, turbidez.

**Orçamento estimado:** sky-view ≤ 0,20 ms (20 736 px × ~30 passos). Os dois LUTs
estáticos saem do frame de carga, com orçamento assíncrono (regra 5 do
PLANO-RECONSTRUCAO: nada concentra trabalho no primeiro frame visível).

### C2 — Nuvens

Duas camadas de cirros/cúmulos rolando na cúpula, dirigidas pelos uniformes que
**já existem e não têm consumidor**: `skyZenithCloudCoverage`,
`skyHorizonCloudDensity`, `cloudLightWindSpeed`. Cobertura, densidade, velocidade
e direção do vento como eixos globais — o mesmo vento que alimenta o espectro da
água, para que mar e céu concordem.

Raymarching volumétrico fica planejado, com orçamento próprio, e só depois de C1
provar folga.

### C0 — Mitigação imediata *(caminho A/B, entra antes de C1)*

Se C1 escorregar, existe um conserto barato que já melhora muito:

1. Panorama de **4096×2048** (4× a densidade linear → 2,4 px/texel na horizontal,
   dentro do razoável) a partir de um HDRI CC0.
2. Remover `textureLod(...,0.0)`; usar `textureGrad` com derivadas calculadas
   analiticamente a partir da direção, contornando a explosão de derivada na
   costura de `fract()`.
3. Anisotropia no sampler do panorama.
4. Mip chain já existe no cooker; passa a ser usada de fato.

Custo em memória: 4096·2048·4·1,333 = **44,7 MB** RGBA8 com mips, contra 2,8 MB
hoje. Isso precisa passar pelo `memory_budget` antes de entrar — e é o principal
argumento a favor de C1, que gasta menos de 1 MB em LUTs.

### Sobre baixar texturas

Recomendação honesta, que contraria em parte o pedido:

- **Não usar foto de água para a superfície do oceano.** Uma normal fotográfica é
  precisamente o que gera repetição e plaid — a causa nº 2 do diagnóstico. O
  espectro FFT de A3 é superior em todos os eixos e não repete.
- **Usar textura real onde ela ganha:** espuma (padrões estocásticos, não
  periódicos), detalhe de turbulência e respingo, e o HDRI do céu de C0. Fontes
  CC0 adequadas: Poly Haven (HDRIs) e ambientCG (espuma, respingo).

Baixar arquivo é ação que precisa da sua autorização explícita. Quando chegarmos
em C0/A6 eu listo arquivo, origem, licença e tamanho e pergunto antes de baixar
qualquer coisa.

## 6. UI de cena

O painel atual passa 13 floats posicionais por JNI
(`nativeApplyControls(float,int,boolean,float,...)`). Acrescentar as ~90
propriedades desta seção por esse caminho é insustentável e violaria o requisito
de "global".

**Tabela de parâmetros versionada e refletida.** O nativo declara uma única fonte
de verdade:

```
struct WaterParameterDescriptor {
  u32 id; const char *name; const char *group; const char *unit;
  ParameterType type;      // float, int, bool, enum, color, vec2, vec3
  float minimum, maximum, defaultValue, step;
  u32 tierMask;            // em quais tiers o eixo existe
  const char *requires;    // capability necessária, ou nulo
};
```

JNI reduz-se a cinco entradas estáveis: `parameterCount()`,
`parameterDescriptor(i)`, `setParameter(id, value)`, `getParameter(id)`,
`applyPreset(name)`. **O Java constrói o painel a partir da tabela** — acrescentar
um eixo no nativo passa a exigir zero mudança em Java. É também a migração que a
ADR-017 já prometeu para Inspector/NoCode: a mesma tabela alimenta os três.

Painel da cena oceânica:

- **Grupos colapsáveis** espelhando a seção 8: Espectro, Malha, Óptica, Reflexão,
  Espuma, Costa, Interação, Cáusticas, Subaquático, Céu, Diagnóstico.
- **Leitura numérica** ao lado de cada slider, com unidade. Toque duplo no rótulo
  volta ao padrão.
- **Presets** como chips: Calmaria, Brisa, Mar aberto, Tempestade, Lago,
  Tropical raso. São recursos de dados versionados, não constantes em código.
- **A/B**: congela um snapshot e alterna, para comparar dois ajustes na mesma pose.
- **Exportar perfil** como JSON do `WaterProfile` v3, colável direto num recurso —
  fecha o ciclo entre a bancada e a autoria.
- **Custo ao vivo** por passe da água em ms, lido do `gpu_frame_timer` já
  existente, para que o ajuste seja feito com o orçamento à vista.
- **Vistas de diagnóstico**: normal, inclinação, espessura, máscara de espuma,
  jacobiano, nível de cascata, wireframe do clipmap. A referência tem
  `WireframeMode`; nós expomos o conjunto.
- **Modos de pincel de interação**: toque = impulso, arrastar = esteira,
  dois dedos = chuva local, botão = tempestade na área.

Fora do painel, o HUD mantém FPS, ms por passe e tier resolvido, para que nenhum
ajuste seja avaliado sem custo visível.

## 7. Orçamento em 120 Hz

Frame de 8,333 ms, teto de GPU de 7,333 ms. Alocação proposta para a cena
oceânica no tier S, **a ser confirmada contra o zero de A0**:

| Passe | Alocado | Fila | Fase |
| --- | ---: | --- | --- |
| Espectro FFT (3 cascatas, amortizado) | 0,90 ms | async compute | A3 |
| Céu — sky-view LUT | 0,20 ms | gráfica | C1 |
| Opacos + céu + chão | 1,60 ms | gráfica | — |
| Sombras direcionais | 0,80 ms | gráfica | — |
| Vértice da água (clipmap 180k tri) | 0,45 ms | gráfica | A2 |
| Fragmento da água (subpass 1) | 1,30 ms | gráfica | A4 |
| Refração meia-res | 0,35 ms | gráfica | A4 |
| Dynamic waves | 0,15 ms | async compute | A5 |
| Pós (AA + nitidez + bloom) | 1,33 ms | gráfica | — |
| **Soma na fila gráfica** | **6,03 ms** | | |
| **Margem** | **1,30 ms** | | |

Cáusticas, SSR, volumétrica e fluidos ficam **fora** desta soma: são opt-in, com
orçamento próprio, e cada um precisa provar que cabe na margem antes de entrar em
`Auto`. A margem de 1,30 ms é deliberada — o `dirt-road` mostrou GPU p95 de
10,777 ms em rota de distância, e nenhum orçamento sobrevive sem folga para
movimento e térmica.

## 8. Matriz de paridade

Todas as propriedades de `WaterSystemScriptableData.cs`, mapeadas. Tier é o
mínimo em que o eixo existe; ausência vira fallback nomeado, nunca crash.

### Superfície e espectro
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| WindSpeed, WindRotation, WindTurbulence | `spectrum.wind{Speed,Direction,Turbulence}` | C | A3 |
| TimeScale | `spectrum.timeScale` | C | A3 |
| FFT_SimulationSize | `spectrum.cascadeResolution[]` | B | A3 |
| (novo) | `spectrum.fetch`, `directionalSpread`, `swellMix` | B | A3 |
| (novo) | `spectrum.choppiness[]`, `cascadeWeight[]`, `domainSize[]` | B | A3 |

### Malha
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| MeshSize, OceanDetailingFarDistance | `mesh.{basePatchSize,maximumDistance}` | C | A2 |
| Quadtree quality levels | `mesh.clipmapLevels`, `patchQuads` | C | A2 |
| UseTesselation, TesselationFactor, …MaxDistance | `mesh.tessellation*` | S | A2 |
| RiverSpline{NormalOffset,VertexCount,Depth} | `mesh.river*` | A | A6 |

### Óptica
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| Transparent, WaterColor, TurbidityColor, Turbidity | `optics.{opacity,deepColor,turbidityColor,turbidity}` | C | A4 |
| (novo) | `optics.absorption` por canal, com Beer–Lambert real | C | A4 |
| RefractionAproximatedDepth, RefractionSimpleStrength | `optics.refraction{Depth,Strength}` | B | A4 |
| UseRefractionDispersion, RefractionDispersionStrength | `optics.dispersion{,Strength}` ≤5 | A | A4 |

### Reflexão
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| UseScreenSpaceReflection, HolesFilling, BordersStretching | `reflection.screenSpace*` | A | A4 |
| UsePlanarReflection, PlanarCullingMask, ClipPlaneOffset | `reflection.planar*` | S | A4 |
| RenderPlanar{Shadows,VolumetricsAndFog,Clouds} | `reflection.planarContents` | S | A4 |
| CubemapUpdateInterval, CubemapCullingMask | `reflection.cubemap*` | B | A4 |
| UseAnisotropicReflections, HighQuality, Scale | `reflection.anisotropic*` | A | A4 |
| ReflectSun, ReflectedSunStrength, CloudinessStrength | `reflection.sun*` | C | A4 |

### Espuma e costa
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| UseFoamRendering, FoamColor, FoamSize, FoamFadeDistance | `foam.*` | C | A3/A6 |
| (novo) | `foam.jacobianThreshold`, `foam.persistence` | B | A3 |
| UseShorelineRendering, ShorelineColor | `shoreline.*` | A | A6 |
| UseShorelineFoamFastMode, ReceiveDirShadows | `shoreline.foam*` | A | A6 |

### Interação
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| UseDynamicWaves, AreaSize, SimulationFPS, ResolutionPerMeter | `interaction.dynamicWaves.*` | B | A5 |
| DynamicWavesPropagationSpeed | `interaction.dynamicWaves.propagationSpeed` | B | A5 |
| UseDynamicWavesRainEffect, RainStrength | `interaction.rain*` | B | A5 |
| UseFluidsSimulation, AreaSize, Iterrations, TextureSize, FPS, Speed, FoamStrength | `interaction.fluids.*` | S | A5 |
| UseFlowMap, AreaPosition, AreaSize, Speed | `interaction.flowMap.*` | A | A5 |
| (novo) | `interaction.kelvinWake.*` | A | A5 |
| Buoyancy | `sampleWaterSurface` + readback assíncrono | C | A3 |

### Luz e volume
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| UseCausticEffect, TextureSize, MeshResolution, ActiveLods, Strength | `caustics.*` | A | A6 |
| UseCausticBicubicInterpolation, Dispersion | `caustics.{bicubic,dispersion}` | S | A6 |
| UseDepthCausticScale, CausticDepthScale, OrthoDepth* | `caustics.orthoDepth*` | A | A6 |
| UseVolumetricLight, Iteration, BlurRadius | `volumetric.*` | S | A7 |
| UseUnderwaterEffect, UseUnderwaterBlur, BlurRadius | `underwater.*` | B | A7 |

### Global e diagnóstico
| Referência | Eixo Aether | Tier | Fase |
| --- | --- | --- | --- |
| UseFiltering, UseAnisotropicFiltering | `filtering.{mode,anisotropy}` | C | A1 |
| WireframeMode | `debug.view` (7 modos) | C | UI |
| DrawToPosteffectsDepth | `water.writeToPostDepth` | B | A4 |
| EnabledMeshRendering | `water.enabled` | C | A2 |
| (novo) | `water.exclusionVolumes[]` | B | A6 |
| (novo) | `network.timeSource` | A | A3 |

## 9. Portas de aceitação

As nove portas do `PLANO-RECONSTRUCAO-RENDERIZACAO`, mais quatro específicas:

1. testes puros e validação de shaders;
2. build Android Debug e Release;
3. zero VUID no Debug físico;
4. três poses fixas na cena oceânica;
5. rota de aproximação/afastamento **e** rota de mergulho;
6. comparação visual com mesma exposição e mesmo frame;
7. GPU p95 ≤ 7,333 ms e CPU p95 ≤ 6,50 ms em 120 Hz;
8. sem pico de primeiro frame causado pelo recurso;
9. soak térmico antes de virar padrão em `Auto`;
10. **paridade CPU/GPU**: o provedor de altura e o vértice concordam dentro de 1 cm;
11. **auditoria de consumidor**: nenhum eixo novo sem leitor real, verificado por
    grep no fim da fase;
12. **fallback nomeado**: cada capability ausente produz um estado registrado no
    log, nunca um shader indefinido;
13. **sweep longo sem shimmer** em Adreno e Mali, com a câmera em movimento.

## 10. Ordem de execução

```
A0 zero medido
 └─ A1 correções baratas
     ├─ C0 céu 4K + textureGrad + anisotropia   ─┐ (paralelo, baixo risco)
     └─ A2 clipmap instanciado                   │
         └─ A3 espectro FFT (3 cascatas)         │
             ├─ A4 óptica: color input, refração │
             │   └─ C1 atmosfera pré-computada ──┘ (alimenta reflexão de A4)
             ├─ A5 interatividade
             │   └─ A6 costa, espuma, cáusticas
             └─ UI tabela de parâmetros (após A3 definir os eixos)
                 └─ A7 subaquático e volumetria
                     └─ C2 nuvens
```

**Caminho crítico:** A0 → A2 → A3 → A4. É o que entrega densidade, fluidez e
profundidade. C1 é paralelo até A4, onde converge para alimentar a reflexão.

## 11. Riscos

| Risco | Mitigação |
| --- | --- |
| FFT estoura 0,90 ms | Degradação escalonada já definida em A3: C2 sai → C1 64² → C0 128². Nunca reduzir qualidade próxima. |
| Sem fila compute dedicada | `deviceFeatures_.computeShaders` já é detectado; sem async, a FFT cai para 30 Hz na fila gráfica e o tier cai para B. |
| Panorama 4K estoura memória | 44,7 MB precisa passar por `memory_budget`. É o argumento a favor de C1 (<1 MB). |
| Refração meia-res vaza primeiro plano | Guarda de profundidade obrigatória, com fallback para amostra não-deslocada. |
| Ainda há déficit de GPU no `dirt-road` (p95 10,777 ms) | A cena oceânica é validada isolada. Integrar água na floresta é uma fase à parte, depois de o déficit fechar. Nenhuma medição do oceano é extrapolada para a floresta. |
| Tabela de parâmetros vira API pública cedo demais | Versionar a tabela junto com `WaterProfile` v3 e exigir migração explícita. |
