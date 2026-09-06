# Plano de excelência — água, ambiente e diversidade de ondas

Alvo: qualidade de água e ambiente comparável às melhores referências de PC,
rodando **global** (oceano, lago, rio, poça, interação, submerso) na engine
Aether em Android, com orçamento de 120 Hz em escala de render 1,0.

Este documento é um **plano de execução com orçamento e critério de aceite por
etapa**. Ele parte de números medidos, não de intenção. Onde a referência não
prova nada em mobile, isso está escrito.

- Estado medido de partida: `AGUA-G0-ZERO-OCEANO.md`
- Contrato de sistema já acordado: `PLANO-SISTEMA-GLOBAL-AGUA.md`
- Inventário anterior de referências: `REFERENCIAS-SISTEMAS-DE-AGUA.md`

---

## 0. O que muda em relação aos planos anteriores

Os planos de água existentes descrevem **o que falta**. Este descreve **em que
ordem**, **quanto custa** e **com base em qual implementação comprovada**, e
inverte uma prioridade que estava errada.

A varredura de isolamento (G0 §3.3) mediu:

| Modo | O que sai do frame | Δ GPU | Δ opaco |
| --- | --- | ---: | ---: |
| `skip-draw` | a água inteira | **2,70 ms** | 3,46 ms |
| `flat` | só o sombreamento; geometria/depth/blend ficam | **0,51 ms** | 1,26 ms |

**A óptica do fragmento custa 0,51 ms dos 2,70 ms da água.** Os outros ~2,2 ms
são vértice, rasterização e blend de 149.504 triângulos em tela cheia. Todo
plano anterior tratava o fragmento como alvo principal. Está invertido: a
geometria e o estágio de vértice são o gargalo, e é lá que o ganho está.

Isso é ótimo, porque significa que **há orçamento para qualidade óptica** assim
que a geometria for corrigida. As etapas estão ordenadas por isso.

---

## 1. Diagnóstico visual — o que as capturas mostram

Três defeitos aparecem nas capturas de `build/android-validation` e cada um tem
uma causa técnica identificada, não estética.

### 1.1 Periodicidade do domínio FFT visível

`water-fft-validation.png` mostra uma grade diagonal regular cobrindo o plano
médio. Não é ruído: é o *patch* FFT se repetindo. Uma cascata única com
`patchLength` fixo produz um padrão com período exatamente igual ao domínio, e o
olho o encontra em qualquer distância intermediária.

**Causa:** cascatas com comprimentos de domínio harmônicos (ou uma só cascata
ativa). **Correção:** W3 — domínios mutuamente não harmônicos + separação
espectral estrita por banda.

### 1.2 Corpo leitoso e sem separação de energia

`water-pixel-detail.png`: a água é uma superfície clara quase uniforme, com
variação de tom mas quase nenhuma variação de *comportamento* — não há trilha
solar, não há escurecimento em ângulo de visão, não há contraste entre face
voltada ao céu e face voltada à câmera.

**Causa:** o Fresnel é integrado (bom) mas o reflexo é só o cubemap de ambiente,
sem anisotropia por rugosidade direcional nem SSR; e o `body` é multiplicado por
irradiância antes de qualquer separação entre transmissão e espalhamento.
**Correção:** W5 (óptica) + W6 (reflexos).

### 1.3 Sem espuma, sem quebra, sem spray

`ocean-stress-active.png` já tem deslocamento vertical real e cristas com forma —
mas as cristas são lisas. Não existe acúmulo de espuma, nem quebra, nem
partícula. A referência (`GodotOceanWaves`, §2.2) tem exatamente a mesma
geometria de crista *com* espuma acumulada, e a diferença visual é enorme.

**Causa:** a espuma hoje é máscara instantânea de Jacobiano por vértice
(`vSpectralFoam`) com quebra por textura no fragmento. Não há campo persistente
por cascata com crescimento/decaimento, nem emissor de spray.
**Correção:** W4.

### 1.4 Faixa clara no horizonte

Já registrada em `PLANO-SISTEMA-GLOBAL-AGUA.md` e visível nas três capturas. A
água e o céu não compartilham domínio HDR nem exposição. **Correção:** W10.

---

## 2. Varredura de referências — o que cada uma prova

| Fonte | Natureza | O que **prova** | O que é adaptável aqui |
| --- | --- | --- | --- |
| **KWS 3 / Kripto FX** (`Downloads/extracted`) | Unity, código-fonte completo, 26.768 linhas | Sistema completo de produção: quadtree instanciado, costura por bits, costa por flipbook, fluidos, caustics, volumétrica, underwater, perfis | **Arquitetura e técnica.** É a referência mais completa disponível |
| **GodotOceanWaves** (2Retr0, MIT) | Godot 4, compute | TMA/JONSWAP + spread de Hasselmann + swell; espuma por Jacobiano acumulada; spray por partículas culled por espuma; load-balance de cascatas | **Modelo espectral e espuma.** Licença MIT permite transplante direto |
| **Crest** (Unity, SIGGRAPH 2017) | LOD/clipmap | Cada componente de onda renderizado **uma vez** na LOD mais adequada + *combine pass* descendo para LODs finas; CDClipmaps | **Política de LOD espectral.** Resolve banda por pixel sem custo por cascata |
| **UE5 Water** | Engine comercial | Water Mesh Component = quadtree com transição contínua; Single Layer Water = água em um passe único, sem passe de refração separado | **Confirma o passe único.** Valida a escolha de subpassInput |
| **ARM OpenGL ES FFT Ocean** | Sample oficial mobile | N=256; FFT **FP16** economiza banda e computação; FFT complexo→real ~2× mais rápido; mip por **compute**, nunca por fragmento, para não travar o pipeline em GPU tile-based | **A única referência mobile-comprovada.** Governa formato e cadência |
| **Sea of Thieves** (SIGGRAPH 2018) | Talk | FFT Tessendorf estilizada em mundo aberto com orçamento de console | Direção de arte e estilização sobre FFT |

### 2.1 Inventário técnico do KWS — o que existe de verdade no pacote

Caminho: `Downloads/extracted/extracted/Assets/KriptoFX/WaterSystem/WaterResources`.

**Simulação**
- `Shaders/Resources/Common/FFT/Spectrum_GPU.compute` — Pierson–Moskowitz +
  espalhamento cos² com `turbulence`, WangHash + xorshift para o ruído gaussiano,
  dispersão de água profunda, deslocamento horizontal derivado de `h` e `k`.
  **É mais simples que o nosso**: nós já temos TMA/JONSWAP com fetch, swell,
  spread e profundidade em `native/renderer/water_fft.h`. Não há o que importar
  aqui — nosso espectro é superior.
- `FFT/ComputeFFT_GPU.compute` e `ComputeFFT_Height.compute` — kernels separados
  para altura (complexo→real) e deslocamento. Confirma a separação que a ARM
  recomenda.
- `Core/FFT/FFT_GPU.cs` — três domínios em `KWS_Settings.Water.FftDomainSize = {20, 40, 160}`.
  **Note a razão: 20 / 40 / 160 → 2× e 4×. São harmônicos, e é exatamente isso
  que produz o tiling que vimos em §1.1.** É um defeito da referência, não um
  modelo a copiar.

**Geometria (o que mais interessa)**
- `Core/Mesh/MeshQuadTree.cs` (625 linhas) — quadtree finito/infinito, LOD por
  distância, malha de instância única por nível, `ComputeBuffer` de chunks
  visíveis, **um `DrawMeshInstancedIndirect`**.
- `Shaders/Resources/Common/KWS_Instancing.cginc` — costura resolvida **no
  vértice por bits em UV**:
  ```hlsl
  vertex.x -= quadOffset * GetFlag(mask, 1) * meshData.downSeam;
  vertex.z -= quadOffset * GetFlag(mask, 2) * meshData.leftSeam;
  ```
  Cada vértice de borda carrega em `uvData.x` uma máscara de qual borda ele
  pertence e em `uvData.y` o offset do quad. Quando o vizinho é de LOD mais
  grossa, o vértice ímpar colapsa sobre o par. **Zero T-junction, zero
  geometria extra, custo de 4 mad no vértice.**
- Os bits 5–8 da mesma máscara são o *skirt* infinito: os vértices da borda
  externa saltam 1000 unidades para fechar o horizonte sem malha.
- `UpdateQuadTree` só recalcula a cada **3 m para frente, 0,5 m para trás ou 1°
  de rotação** (`KWS_Settings.Water.UpdateQuadtreeEvery*`). A assimetria
  frente/trás é deliberada: andar para trás revela chunk novo imediatamente.
- `UpdateQuadTreeDetailingRelativeToWind` — a resolução do chunk sobe com o vento
  (`QuadTreeChunkLodRelativeToWind = {0.5, 0.75, 1, 1.5, 2, 2.5}`). Mar calmo
  não paga triângulo.

**Óptica** (`Common/KWS_WaterHelpers.cginc`, `KWS_WaterFragPass.cginc`)
- Refração real por IOR: `ComputeWaterRefractRay` traça o raio refratado a uma
  profundidade aproximada, reprojeta em clip space e amostra a cor de cena nesse
  UV. Não é distorção por normal — é refração com paralaxe.
- Dispersão: três amostras da cor de cena com IOR ligeiramente diferente por canal.
- `ComputeUnderwaterColor` separa **absorção** (`pow(waterColor, 25*fade/transparent)`)
  de **turbidez** (mistura para uma cor de turbidez independente), e ambas
  multiplicadas pela luz volumétrica local. É a separação que nos falta.
- `GetFilteredNormal_lod0` — mistura bicúbico (perto) e AA (longe) por distância,
  e devolve um `normalFilteringMask` derivado do **comprimento da normal
  interpolada**: `rcp(1 + 100*(rcp(len)-1))`. Essa é a técnica de Toksvig, e é
  mais barata que a nossa variância por `dFdx/dFdy`.
- `ComputeSSS` no passe de depth: `pow(saturate(dot(viewDir, -(lightDir + normalLod*(-1,1,-1)))), 3)`
  escrito em MRT junto com a máscara, limitado por vento (`windLimit`) e por
  distância. Espalhamento subsuperficial em onda contraluz, custo de um MRT.

**Costa — a técnica mais reaproveitável**
- `Common/CommandPass/KWS_ShorelineWaves.shader` — as ondas de arrebentação
  **não são simuladas**. São um flipbook de 14×15 = 210 quadros a 18 FPS
  (`ShorelinePos.png`, `ShorelineNorm.png`, `ShorelineAlpha.png`), instanciados
  ao longo da linha de costa com ângulo, escala e `timeOffset` por instância, e
  renderizados num RT de deslocamento + normal de 2048² cobrindo 150 m.
  Interpolação entre quadros adjacentes elimina o stepping.
- `GetWaterOrthoDepth` — um render ortográfico do terreno (2048², área 200 m)
  dá profundidade de fundo para amortecer o deslocamento perto da praia:
  `waterOffset = lerp(waterOffset, 0, saturate(terrainDepth + 0.85))`.
- Custo: um draw instanciado em RT pequeno, por câmera, com LOD por distância
  (`Shoreline.LodDistances = {20..200}`). Isso cabe em mobile.

**Interação**
- `Common/GraphicsPass/KWS_DynamicWaves.shader` — equação de onda 2D clássica em
  RT seguindo a câmera: `data += (right+left+top+down)*0.5 - prevFrame; data *= 0.992;`
  Chuva é um limiar sobre ruído procedural. Normal por diferença central.
  Resolução por metro configurável (`DynamicWavesResolutionPerMeter = 34`).
- `KWS_FluidSimulation.shader` + `KW_FluidsSimulation2D.cs` — advecção 2D com
  dois LODs (área pequena de alta resolução + área grande), usada em rios; a
  velocidade resultante vira flowmap que distorce a amostragem da FFT
  (`ComputeNormalUsingFlowMap`, blend de duas fases com `frac(t)` e `frac(t+0.5)`).
- `Core/KW_Buoyancy.cs` — buoyancy por requisição assíncrona com vida de 10
  frames (`BuoyancyRequestLifetimeFrames`). Nós já temos volume submerso exato
  em `native/physics/water_buoyancy.cpp`, que é melhor.

**Volume**
- `Common/CommandPass/KWS_Caustic_Pass.shader` — caustics **sem ray tracing**:
  renderiza uma malha 256² deslocada pela mesma FFT e mede a razão de área
  `oldArea/newArea` por `ddx/ddy`. Compressão de área = concentração de luz.
  Três LODs (`Caustic.LodSettings = {10, 20, 40, 80}` metros), 512² por LOD.
- `PlatformSpecific/KWS_VolumetricLighting.shader` — raymarch com Mie, 4
  iterações em resolução 35 % (`VolumetricLightResolutionQuality: 35`), blur
  bilateral, caustics amostradas dentro do march.
- `KWS_Underwater.shader` + máscara de face (`VFACE`) no passe de depth.

**Autoria**
- 11 perfis reais em `Resources/SavedData/*.WaterSettings.asset` — praia rochosa,
  praia de areia, pôr do sol, caverna, galeão, cidade noturna, piscina, rio,
  vila. **São valores testados por um autor profissional**, e servem de tabela de
  partida para os nossos presets.

### 2.2 Restrição de licença — como adaptar sem copiar

KWS 3 é um asset comercial da Unity Asset Store. A EULA padrão da loja permite
uso em produtos do licenciado, mas **não** permite redistribuir o código-fonte
como parte de outro produto ou engine. Você declarou propriedade do projeto, e
isso cobre o uso; não converte o código em base redistribuível.

Consequência prática, e é a única regra deste plano sobre o assunto:

> **Transplantamos técnica, não arquivo.** Cada item deste plano descreve o
> mecanismo (a matemática, o layout de dado, a política de cadência) e ele é
> reimplementado em GLSL/C++ contra as nossas estruturas. Nenhum `.cginc`,
> `.compute` ou `.cs` do KWS entra no repositório, nem traduzido linha a linha.
> Onde há alternativa MIT que prova a mesma coisa — `GodotOceanWaves` para
> espectro/espuma, o sample da ARM para FFT mobile — a citação vai para ela.

Isso não limita nada do que está planejado abaixo: técnica de renderização não é
protegível, e todas as ideias-chave (quadtree com costura por bits, flipbook de
arrebentação, caustics por razão de área, equação de onda 2D) são públicas e
anteriores ao KWS.

---

## 3. Arquitetura alvo

```
                          ┌──────────────────────────────────────┐
                          │  WaterWorld (registro global)        │
                          │  volumes × WaterField × prioridade   │
                          └───────────────┬──────────────────────┘
                                          │
   ┌──────────────────────────────────────┼──────────────────────────────────────┐
   │                                      │                                      │
┌──▼───────────────┐   ┌──────────────────▼───────────┐   ┌────────────────────▼─┐
│ SIMULAÇÃO (async)│   │ CAMPOS LOCAIS (RT seguindo   │   │ CONSULTA CPU          │
│                  │   │ câmera, cadência própria)    │   │                       │
│ espectro→FFT     │   │                              │   │ WaterField mirror     │
│ 3 cascatas       │   │ ondas dinâmicas 2D           │   │ buoyancy, gameplay    │
│ FP16, fila comp. │   │ fluidos/flowmap (rio)        │   │ IA, áudio             │
│ → disp RGBA16F   │   │ costa: flipbook + ortho depth│   │                       │
│ → slope RG16F    │   │ espuma persistente por casc. │   │                       │
│ → foam R8        │   │                              │   │                       │
└──────┬───────────┘   └──────────────┬───────────────┘   └───────────────────────┘
       │                              │
       └──────────────┬───────────────┘
                      │
        ┌─────────────▼─────────────────────────────────────────┐
        │ GEOMETRIA: quadtree instanciado, 1 draw indireto      │
        │ costura por bits em UV, skirt de horizonte            │
        │ resolução por distância × vento                       │
        │ vértice lê disp por textura (1 tap/cascata)           │
        └─────────────┬─────────────────────────────────────────┘
                      │
        ┌─────────────▼─────────────────────────────────────────┐
        │ SUPERFÍCIE (subpasse único, tile-resident)            │
        │ depth opaco por subpassInput · sem cópia full-res     │
        │ Fresnel IOR · refração com paralaxe · dispersão       │
        │ absorção ⊥ turbidez ⊥ SSS · espuma multi-escala       │
        │ reflexo: ambiente aniso → SSR → planar                │
        └─────────────┬─────────────────────────────────────────┘
                      │
        ┌─────────────▼─────────────────────────────────────────┐
        │ VOLUME (só quando há água na tela / câmera submersa)  │
        │ caustics por razão de área · volumétrica 35 % + blur  │
        │ underwater · spray (partículas culled por espuma)     │
        └───────────────────────────────────────────────────────┘
```

Regra que vale em toda a árvore: **nenhum passe existe porque o hardware suporta.
Cada passe tem enable, distância máxima, resolução e cadência próprios, e sai do
frame graph quando não há consumidor.** Já é assim para os descritores de água;
passa a valer para todos os novos.

---

## 4. Etapas

Cada etapa tem: **problema medido**, **técnica e referência**, **mudança**,
**orçamento**, **aceite**. O aceite é sempre um par intercalado na mesma rodada
(§6), nunca um número absoluto entre sessões.

### W1 — Quadtree instanciado com costura por bits · *maior ganho*

**Problema.** 149.504 triângulos submetidos por frame, resolução uniforme, custo
de vértice+raster+blend de ~2,2 ms. A grade graduada camera-relative atual
resolve costura por construção, mas paga densidade alta em toda a extensão.

**Técnica.** Quadtree com um mesh de instância por nível de LOD, um buffer de
chunks visíveis, um draw indireto. Costura resolvida no vértice por máscara de
bits em `uvData` colapsando o vértice ímpar da borda sobre o par quando o
vizinho é mais grosso. Skirt de horizonte pelos bits 5–8. Referência: KWS
`MeshQuadTree.cs` + `KWS_Instancing.cginc`; UE5 Water Mesh Component; Crest
CDClipmaps.

**Mudança.**
- `native/renderer/water_surface.{h,cpp}` — já existe `MaximumWaterClipmapLevels`
  e `MaximumWaterPatches`. Ativar o planner existente e dar-lhe draw path.
- Novo `native/renderer/water_quadtree.{h,cpp}` — nós, vizinhança por hash de UV,
  seleção por distância, emissão de `WaterChunkInstance { pos, size, seamBits }`.
- `native/rhi/shaders/dirt_road_vertex.glsl` — ramo de água: aplicar máscara de
  costura antes de qualquer amostragem.
- Cook offline gera os meshes de instância por nível com a máscara em UV1.
- Cadência: recalcular a 3 m para frente, 0,5 m para trás, 1° de rotação.
- Resolução por nível escalada pelo vento, tabela por qualidade
  (ultra/alta/média/baixa/muito baixa), como `QuadTreeChunkQuailityLevels*`.

**Orçamento.** Alvo: **≤ 40.000 triângulos** na mesma pose, com densidade
próxima **maior** que hoje. Meta de Δ: −1,2 ms de `gpu_opaque_ms`.

**Aceite.** (a) Par intercalado grade-atual × quadtree na mesma rodada, com
dispersão publicada. (b) Teste nativo determinístico: para uma pose fixa,
nenhum par de chunks vizinhos difere mais de um nível, e todo vértice de borda
com `seamBit` colapsa exatamente sobre o vértice par. (c) Captura Android sem
emenda visível em movimento — vídeo, não frame único. (d) Zero alocação por frame.

---

### W2 — Deslocamento por textura no vértice · *segundo maior ganho*

**Problema.** `water_spectral_sampling.glsl` faz bilinear **manual sobre storage
buffer** no vértice: 4 leituras × 2 canais × 4 cascatas = **32 fetches escalares
por vértice**, mais 16 para espuma. Isso é o coração dos ~2,2 ms.

**Técnica.** O compute já escreve `slope` em `rg16f` (`water_slope_pack.comp`).
Escrever também **deslocamento em `rgba16f`** (`xyz` = deslocamento, `w` = espuma)
e amostrar no vértice com `textureLod` — **1 tap com filtro de hardware por
cascata** em vez de 8 leituras manuais. Referência: ARM (FP16 economiza banda e
computação; mip gerada por compute, nunca por fragmento, para não travar o
pipeline em GPU tile-based); KWS `KW_DispTex`; GodotOceanWaves.

**Mudança.**
- Novo `native/rhi/shaders/water_displacement_pack.comp`, espelhando
  `water_slope_pack.comp`, escrevendo `rgba16f`.
- `water_spectral_sampling.glsl` — ramo de vértice passa a `textureLod`;
  manter o caminho SSBO atrás de uma constante de especialização como oráculo
  de teste e fallback de device sem filtro linear em `rgba16f`.
- Mip por compute para as cascatas, com o offset de 0,5 texel que a ARM descreve
  (o primeiro texel em uv=0, não em uv=0,5), senão as cascatas desalinham entre
  níveis.
- O espelho CPU (`water_spectral_mirror.cpp`) continua lendo o buffer — física
  não muda.

**Orçamento.** Meta de Δ: **−0,6 ms** de `gpu_opaque_ms` no modo 7
(`no-vertex-spectral`) fechando com o modo 0.

**Aceite.** (a) Par intercalado SSBO × textura. (b) Teste nativo comparando o
campo amostrado por textura contra o oráculo SSBO: erro máximo por componente
dentro da resolução de meio-float. (c) `VK_FORMAT_R16G16B16A16_SFLOAT` com
`SAMPLED_IMAGE_FILTER_LINEAR` verificado por capability, com fallback declarado.

---

### W3 — Cascatas não harmônicas e banda por pixel · *mata o tiling*

**Problema.** §1.1 — a repetição do domínio FFT é visível. É o defeito mais
caro visualmente e custa zero para corrigir.

**Técnica.** Três correções combinadas:

1. **Domínios mutuamente não harmônicos.** KWS usa `{20, 40, 160}` — razões 2 e
   4, e por isso os três se realinham a cada 160 m. Usar razões irracionais na
   prática: por exemplo `{7,3 m · 41 m · 233 m}`, escolhidas para que o mínimo
   múltiplo comum fique além da distância de detalhe. O validador
   (`validateWaterCascades`) já rejeita bandas sobrepostas; passa a **exigir**
   também não-harmonicidade dentro de uma tolerância.
2. **Cada comprimento de onda em exatamente uma cascata.** É a *combine pass* do
   Crest: o espectro de cada cascata é janelado no seu k-range, sem sobreposição
   de energia. Já temos `minimumWavelength` / `maximumWavelength` em
   `WaterSpectrumSettings`; passam a ser derivados do domínio, não autorados
   soltos.
3. **Rotação por cascata.** Já existe (`waterWaveMotion[cascade].yz` guarda
   cos/sin). Passa a ser derivada do espectro e não da direção global, para que
   as três não compartilhem eixo.

O fragmento já pesa cada cascata por `pixelSpacing` derivado de
`dFdx/dFdy(vSpectralCoordinates)` — essa parte está correta e fica.

**Mudança.** `native/renderer/water_cascades.{h,cpp}` — `defaultWaterCascadeSettings()`
passa a devolver domínios não harmônicos e bandas derivadas; `validateWaterCascades`
ganha `WaterCascadeError::HarmonicDomains`.

**Orçamento.** Zero. É mudança de parâmetro e de validação.

**Aceite.** (a) Teste nativo: para o conjunto padrão, nenhum par de domínios tem
razão racional simples dentro da tolerância, e a soma das bandas cobre
[λmin, λmax] sem buraco nem sobreposição. (b) Captura na mesma pose de
`water-fft-validation.png`, com a grade diagonal ausente. (c) FFT autocorrelação
do heightfield amostrado: nenhum pico secundário acima do limiar dentro da
distância de detalhe.

---

### W4 — Espuma como campo persistente, quebra e spray

**Problema.** §1.3 — cristas lisas. A espuma atual é instantânea.

**Técnica.**
- **Campo persistente por cascata.** `water_foam_update.comp` já implementa
  crescimento/decaimento exponencial sobre o Jacobiano e já é persistente entre
  frames. O que falta é *usá-lo com energia suficiente* e **advectá-lo pelo
  deslocamento horizontal**, senão a espuma fica presa ao grid e não à onda.
  Referência: GodotOceanWaves (crescimento linear, decaimento exponencial,
  parâmetros `foam grow/decay`).
- **Quebra por Jacobiano com limiar por cascata.** Hoje o limiar é global. Ondas
  longas quebram com Jacobiano menos negativo que capilares.
- **Ruptura multi-escala no fragmento.** Já existe (`foamPattern` com
  `smoothstep(.28,.68)`); passa a duas oitavas com UVs contra-rotacionadas, como
  `microSlope` já faz para normal.
- **Spray.** Partículas GPU distribuídas na região ativa e **culled pela espuma
  no seu ponto** — não pelo bounding box inteiro. O README do GodotOceanWaves
  documenta o defeito dessa abordagem quando a distribuição é uniforme: a maioria
  das partículas é descartada. Emissor por região ativa evita isso, e é
  exatamente o que `REFERENCIAS-SISTEMAS-DE-AGUA.md` já registrou como requisito.

**Mudança.**
- `water_foam_update.comp` — advecção por `horizontal` da própria cascata,
  limiar por cascata via push constant.
- `water_foam.h` — limiar/crescimento/decaimento por cascata.
- `water_surface_shading.glsl` — segunda oitava de ruptura; espuma entra na
  `compositeAlpha` antes da absorção, não depois.
- Novo emissor de spray em `native/renderer/` alimentado pelas regiões com
  espuma acima do limiar.

**Orçamento.** Espuma: dentro do custo atual do compute (o kernel já roda).
Ruptura extra: ≤ +0,08 ms de fragmento. Spray: budget próprio, teto declarado,
desligado por padrão em qualidade baixa.

**Aceite.** (a) A/B do fragmento com o modo de isolamento, medindo só a segunda
oitava. (b) Captura lado a lado contra `ocean-stress-active.png` na mesma pose e
mesmo vento. (c) Teste nativo do campo de espuma: para deslocamento nulo, o campo
converge para zero; para Jacobiano constante negativo, converge para o equilíbrio
analítico `source/(source+decay)`.

---

### W5 — Óptica: refração com paralaxe, dispersão, absorção ⊥ turbidez ⊥ SSS

**Problema.** §1.2. Hoje a transmissão é `exp(-absorption*thickness)` misturando
duas cores autoradas, sem cor de cena refratada, sem dispersão, sem SSS.

**Técnica (na ordem de custo crescente; cada item é um enable independente):**

1. **Absorção ⊥ turbidez.** Separar o que o KWS separa: absorção é
   Beer–Lambert sobre a cor de água; turbidez é mistura para uma cor de turbidez
   independente com sua própria curva (`1 - exp(-5*fade/transparent)`). Hoje as
   duas estão fundidas em `waterOptics.z`. **Custo zero**, é reorganização.
2. **SSS de onda contraluz.** `pow(saturate(dot(v, -(l + n_lod*(-1,1,-1)))), 3)`,
   limitado por vento e por distância, usando a normal de mip alta (a cascata
   longa) e não a normal detalhada. Escrever no mesmo MRT do depth de água.
   Custo: um canal a mais no MRT que já existe.
3. **Refração com paralaxe (IOR).** Traçar o raio refratado por
   `Refract(v, n, 1.333)` até uma profundidade aproximada, reprojetar e amostrar
   a cor de cena nesse UV. **Exige cor de cena amostrável.** Em tile-based isso é
   uma resolve — é o item mais caro deste plano e por isso fica atrás de um
   enable de qualidade, com fallback explícito para o caminho depth-only atual
   (que é honesto e já funciona).
4. **Dispersão.** Três amostras da cor refratada com IOR por canal. Só liga se
   (3) estiver ligado. `MaxRefractionDispersion = 5` no KWS.
5. **Filtro de normal por Toksvig.** Substituir a variância por `dFdx/dFdy` pelo
   comprimento da normal interpolada: `rcp(1 + k*(rcp(len(n)) - 1))`. Mais barato
   e mais estável que o caminho atual. O caminho atual vira oráculo de teste
   (`water_shading.h` já tem o oráculo CPU; ganha o segundo modelo).

**Mudança.** `water_surface_shading.glsl`, `water_shading.h`, `water_surface.h`
(perfil ganha `turbidityColor`, `sssStrength`, `refractionMode`,
`refractionDepth`, `dispersionStrength`).

**Orçamento.** (1)+(2)+(5): ≤ +0,10 ms — o fragmento inteiro custa 0,51 ms hoje,
há folga. (3)+(4): orçados **separadamente** contra a resolve da cor de cena, e
só entram se o par intercalado provar que cabem no déficit restante.

**Aceite.** Isolamento por termo (o mecanismo `WaterCostIsolation` já existe e
ganha modos novos), par intercalado por termo, capturas raso/profundo/contraluz.

---

### W6 — Reflexos com cadência e degradação declarada

**Problema.** Só ambiente. Sem SSR, sem planar, sem anisotropia.

**Técnica.** Cadeia de três níveis com fallback explícito, na ordem em que a
qualidade cresce e o custo também:
1. **Ambiente anisotrópico** — o lóbulo de reflexão da água é alongado na
   direção do vento. KWS resolve com um deslocamento vertical da direção de
   reflexão proporcional ao vento (`GetCubemapReflectionFiltered`) — barato e
   convincente. Primeiro alvo.
2. **SSR** em resolução reduzida (KWS usa 35 %) com preenchimento de falhas e
   *stretching* de borda. Cadência própria; não roda todo frame se o orçamento
   apertar.
3. **Planar** só para superfícies finitas pequenas (piscina, poça, interior), com
   máscara de culling e cadência. Nunca para oceano aberto.

**Mudança.** `WaterReflection` já é enum no perfil (`Environment/ScreenSpace/Planar`).
Ganha o modo anisotrópico e a política de cadência.

**Orçamento.** Aniso: ≤ +0,05 ms. SSR: teto de 0,45 ms em 35 % de resolução;
desliga sozinho por pressão térmica antes de a resolução dinâmica cair.

**Aceite.** Par intercalado por nível; captura de horizonte com sol rasante;
prova de que a degradação térmica desliga SSR antes de reduzir escala de render.

---

### W7 — Costa: arrebentação por flipbook, ortho depth e espuma de contato

**Problema.** Não existe costa. Água encontra terreno numa faixa de espessura.

**Técnica.** Reimplementar o mecanismo do KWS, que é o melhor custo/benefício
conhecido para mobile:
- **Ortho depth do terreno** — um render ortográfico topo-baixo (2048², área
  200 m, cadência baixa: só quando a área muda) dá profundidade de fundo.
  Amortece o deslocamento perto da praia e alimenta a espuma.
- **Arrebentação por flipbook instanciado** — um atlas de deslocamento+normal de
  uma quebra simulada offline, instanciado ao longo da spline de costa com
  ângulo/escala/offset temporal por instância, renderizado num RT de 2048²
  cobrindo 150 m, com interpolação entre quadros. **Isso dá diversidade de onda
  costeira sem simulação em runtime.**
- **Espuma de costa** por partículas com LOD por distância.

**Por que flipbook e não simulação:** simular arrebentação exige SWE ou SPH e não
cabe no orçamento. O flipbook é a técnica que estúdios usam há uma década, e a
diversidade vem de instância (ângulo × escala × fase), não de simulação.

**Mudança.** Novo módulo `native/renderer/water_shoreline.{h,cpp}`, passe ortho
depth no frame graph, atlas cozido offline por `tools/`, spline de costa no
perfil.

**Orçamento.** Ortho depth: amortizado (cadência por movimento de área).
Flipbook: um draw instanciado em 2048², ≤ 0,25 ms. Espuma de costa: budget
próprio com LOD.

**Aceite.** Cena de praia dedicada; captura de câmera na linha d'água; prova de
que o deslocamento vai a zero sobre terreno acima do nível.

---

### W8 — Interação local: ondas dinâmicas, fluxo, chuva

**Técnica.**
- **Equação de onda 2D** em RT seguindo a câmera, com resolução por metro
  configurável, damping 0,992, normal por diferença central, e uma máscara de
  borda para não vazar. Referência: KWS `KWS_DynamicWaves.shader`. É simples,
  estável e barata.
- **Chuva** como limiar sobre ruído procedural dentro do mesmo passe. Custo
  marginal zero.
- **Flowmap + fluidos 2D** para rio: velocidade advectada distorce a amostragem
  da FFT com blend de duas fases (`frac(t)` / `frac(t+0.5)`), que é o truque
  padrão de flowmap e elimina o esticamento.
- **Entrada genérica**: toque, corpos Jolt (já temos volume submerso exato) e
  chuva alimentam o mesmo buffer de impulsos. Não há caminho especial por tipo.

**Mudança.** Novo `native/renderer/water_ripples.{h,cpp}` + compute; o registro
de impulsos entra em `WaterField`. `MaximumWaterInteractions` já existe no perfil.

**Orçamento.** RT de 512² a 60 Hz fixo (não por frame): ≤ 0,12 ms amortizado.

**Aceite.** Teste nativo do solver: energia decai monotonicamente sem fonte;
impulso pontual produz frente circular com velocidade correta. Captura de toque
na tela produzindo onda visível.

---

### W9 — Volume: caustics, volumétrica, submerso

**Técnica.**
- **Caustics por razão de área.** Renderizar uma malha 256² deslocada pela mesma
  FFT num RT e medir `length(ddx(old))*length(ddy(old)) / (mesmo para new)`.
  Três LODs por distância. **Sem ray tracing, sem simulação extra**: reusa o
  deslocamento que já existe. Isso é barato o suficiente para mobile.
- **Volumétrica** em 35 % de resolução, 4 passos de raymarch com fase de Mie,
  blur bilateral, caustics amostradas dentro do march. Só quando há água na tela.
- **Submerso** por `VFACE` no passe de depth + composição com absorção/turbidez
  da mesma equação da superfície — não uma cor chapada sobre a tela.

**Orçamento.** Caustics: ≤ 0,20 ms (3 LODs, 512²). Volumétrica: ≤ 0,50 ms em
35 %; desliga por pressão térmica. Submerso: dentro do passe existente.

**Aceite.** Captura submersa e de superfície na mesma cena com a mesma exposição;
par intercalado por módulo.

---

### W10 — Ambiente: céu, IBL, exposição e névoa na mesma fonte

**Problema.** §1.4 — faixa clara no horizonte. Água e céu não compartilham
domínio HDR nem exposição.

**Técnica.** Uma única fonte de radiância ambiente: o céu gera o cubemap, o
cubemap gera o IBL da água **e** dos opacos, a exposição é uma só, e o tone
mapping acontece uma vez no fim. A névoa é aplicada na mesma unidade
(`GetInternalFogVariables` no KWS é chamado depois de tudo, com a mesma
profundidade linear). Isso já está parcialmente feito — `environment_lighting.glsl`
e `microfacet_brdf.glsl` já são compartilhados entre água e opacos.

**Mudança.** Fechar a composição HDR: a água escreve radiância linear e o tone
map é do pós, não do fragmento da água. Hoje `water_surface_shading.glsl` chama
`toneMapEnvironment` e aplica sRGB dentro do próprio shader — isso impede
transmissão RGB correta e é a causa direta da faixa do horizonte.

**Orçamento.** Neutro a negativo (remove trabalho do fragmento da água).

**Aceite.** Captura de horizonte sem descontinuidade; prova numérica de que a
radiância da água e a do céu no mesmo ponto do horizonte diferem menos que o
limiar de quantização antes do tone map.

---

## 5. Orçamento de quadro fechado

Painel a 120 Hz, escala 1,0, 2772×1280. Orçamento GPU p95 = **7,333 ms**.
Partida medida (espectral): **9,165 ms**. Déficit: **1,832 ms**.

| Item | Hoje | Depois | Δ |
| --- | ---: | ---: | ---: |
| Vértice + raster + blend da água (W1) | ~2,19 ms | ~0,95 ms | **−1,24** |
| Amostragem espectral no vértice (W2) | incluído acima | −0,60 ms | **−0,60** |
| Simulação espectral, fila assíncrona (G1.7) | 0,585 ms | ~0,10 ms visível | **−0,49** |
| Fragmento da água (W3 + W4 + W5 barato) | 0,51 ms | 0,66 ms | +0,15 |
| Pós-processamento (G7) | 1,66 ms | 1,20 ms | **−0,46** |
| Reflexo aniso (W6.1) | 0 | 0,05 ms | +0,05 |
| **Subtotal** | **9,165** | **~6,6 ms** | **−2,6** |
| Folga para módulos opcionais | — | **~0,73 ms** | |

Os módulos opcionais — SSR (0,45), caustics (0,20), volumétrica (0,50), costa
(0,25), ripples (0,12), spray — **somam mais que a folga**. Isso é deliberado:
eles não ligam todos ao mesmo tempo. A política de qualidade escolhe o
subconjunto por dispositivo e por cena, e a pressão térmica desliga na ordem
inversa da contribuição visual. Nenhum deles entra no orçamento base.

Perfis de partida:

| Perfil | Cascatas | Quadtree | Reflexo | Volume | Costa | Alvo |
| --- | --- | --- | --- | --- | --- | --- |
| **Excelência** | 3 × 256² | Ultra | SSR 35 % | caustics + volumétrica | flipbook + espuma | 60 Hz |
| **Alta** | 3 × 128² | Alta | aniso | caustics | flipbook | 120 Hz |
| **Média** | 2 × 128² | Média | aniso | — | espuma de contato | 120 Hz |
| **Baixa** | 1 × 64² | Baixa | ambiente | — | — | 60 Hz, escala < 1 |

**120 Hz não é promessa de clock.** A escala de render efetiva e o alvo de
apresentação vão em toda publicação de número, conforme já corrigido em G0 §1.1.

---

## 6. Protocolo de medição — obrigatório

G0 §3.1 mediu **22 % de variação no mesmo binário, mesma pose, mesma escala,
`thermal_pressure=none` do começo ao fim**, com temperatura de bateria subindo de
32,3 °C para 42,1 °C. A API térmica do Android não sinalizou nada.

Portanto, e sem exceção neste plano:

1. Nenhum número absoluto de GPU vale como comparação entre rodadas separadas.
2. Toda mudança é medida por **par intercalado dentro da mesma rodada**,
   alternando A e B várias vezes, **diferenciando mínimos**, com a dispersão
   entre repetições publicada ao lado.
3. Um delta menor que a dispersão é publicado como **sem valor**, não como ganho.
4. `dumpsys battery` (temperatura) entra na evidência de toda captura — já
   implementado no commit `7d2af8e`.
5. Sessões de varredura são curtas com intervalo de resfriamento.
6. Painel, alvo de apresentação e `game_mode` são fixados e declarados.

Comando base: `pwsh tools/measure-ocean.ps1 -Name <rodada> -ResolutionScale 1.0 -TargetFps 120 -SpectralWater -LockCamera`

---

## 7. Matriz de adaptação — de onde vem cada coisa

| Alvo Aether | Arquivo destino | Mecanismo de referência | Fonte | Reimplementar |
| --- | --- | --- | --- | --- |
| Quadtree + costura por bits | `renderer/water_quadtree.cpp`, `dirt_road_vertex.glsl` | máscara de borda em UV colapsando vértice ímpar; skirt por bits 5–8 | KWS `MeshQuadTree.cs` + `KWS_Instancing.cginc`; UE5 Water Mesh | sim |
| Cadência de quadtree | idem | 3 m frente / 0,5 m trás / 1° | KWS `KWS_Settings.Water` | sim |
| LOD por vento | idem | tabela `{0.5, 0.75, 1, 1.5, 2, 2.5}` | KWS | sim |
| Deslocamento por textura | `water_displacement_pack.comp` | `rgba16f`, 1 tap/cascata, mip por compute com offset 0,5 texel | ARM ES SDK; GodotOceanWaves | sim |
| Espectro TMA/JONSWAP + spread | já existe em `water_fft.h` | — | GodotOceanWaves (MIT) confirma o mesmo modelo | **nada a fazer** |
| Domínios não harmônicos | `water_cascades.cpp` | evitar razões 2×/4× | correção do defeito do KWS | sim |
| Uma onda por cascata | `water_cascades.cpp` | combine pass | Crest (SIGGRAPH 2017) | sim |
| Espuma persistente + advecção | `water_foam_update.comp` | crescimento linear, decaimento exponencial, advectada pelo deslocamento | GodotOceanWaves (MIT) | sim |
| Spray culled por espuma | novo emissor | distribuir na região ativa, não no bbox | GodotOceanWaves + correção documentada no README | sim |
| Filtro de normal Toksvig | `water_surface_shading.glsl` | `rcp(1 + k*(rcp(len(n))-1))` | KWS `GetFilteredNormal_lod0`; Toksvig 2005 | sim |
| SSS de onda contraluz | MRT do depth de água | `pow(dot(v, -(l + n_lod*(-1,1,-1))), 3)` limitado por vento | KWS `fragDepth` | sim |
| Absorção ⊥ turbidez | `water_surface_shading.glsl` | duas curvas independentes × luz volumétrica | KWS `ComputeUnderwaterColor` | sim |
| Refração com paralaxe | idem | raio refratado reprojetado em clip space | KWS `GetRefractedUV_IOR` | sim, atrás de enable |
| Reflexo aniso por vento | idem | deslocar reflDir para cima ∝ vento | KWS `GetCubemapReflectionFiltered` | sim |
| Arrebentação costeira | `water_shoreline.cpp` | flipbook 14×15 @ 18 FPS instanciado com ângulo/escala/fase | KWS `KWS_ShorelineWaves.shader` | sim, atlas próprio cozido |
| Ortho depth de terreno | novo passe | 2048², 200 m, amortiza deslocamento na praia | KWS `GetWaterOrthoDepth` | sim |
| Ondas dinâmicas 2D | `water_ripples.cpp` | `d += (r+l+t+b)*0.5 - prev; d *= 0.992` | KWS `KWS_DynamicWaves.shader`; equação de onda clássica | sim |
| Chuva | idem | limiar sobre ruído no mesmo passe | KWS | sim |
| Flowmap de duas fases | rio | blend `frac(t)` / `frac(t+0.5)` | KWS `ComputeNormalUsingFlowMap`; técnica pública | sim |
| Caustics por razão de área | novo passe | `oldArea/newArea` por `ddx/ddy` sobre malha deslocada | KWS `KWS_Caustic_Pass.shader` | sim |
| Volumétrica 35 % + bilateral | novo passe | 4 passos, Mie, caustics no march | KWS `KWS_VolumetricLighting.shader` | sim |
| Buoyancy | já existe, melhor | volume submerso exato + Jolt em lote | commits `ca6ce4b`, `12fd89a` | **nada a fazer** |
| Presets de autoria | `WaterProfile` | 11 perfis testados (praia, caverna, rio, piscina, noturno) | KWS `SavedData/*.WaterSettings.asset` | valores de partida, retunados |

Duas linhas dizem **nada a fazer**, e isso é resultado da varredura: nosso
espectro (TMA/JONSWAP com fetch, swell, spread, profundidade e damping de onda
curta) é mais completo que o Pierson–Moskowitz do KWS, e nossa buoyancy por
volume submerso exato é melhor que o sistema de requisições dele.

---

## 8. Ordem de execução e dependências

```
W3 (cascatas) ─────────┐
                       ├──► W4 (espuma/spray)
W2 (disp por textura) ─┤
                       └──► W9 (caustics reusam o deslocamento)
W1 (quadtree) ─────────────► libera orçamento para W5..W9
W10 (HDR único) ───────────► pré-requisito de W5 (transmissão RGB) e W6
W7 (costa) depende de ortho depth ──► W9 (volume raso)
W8 (interação) independente
```

Ordem recomendada: **W3 → W2 → W1 → W10 → W4 → W5 → W6 → W8 → W7 → W9.**

W3 primeiro porque custa zero e corrige o defeito visual mais gritante. W2 antes
de W1 porque é isolável e o par intercalado fica limpo. W10 antes de W5 porque
sem domínio HDR único a óptica não tem como ficar certa.

---

## 9. O que este plano não promete

- **Não promete 120 FPS.** Promete um orçamento fechado de 7,333 ms com uma
  trajetória de −2,6 ms partindo de 9,165 ms medidos, e um protocolo que
  distingue ganho de ruído de clock. Cada etapa publica o par intercalado.
- **Não promete paridade visual com KWS.** KWS roda em desktop com SSR full-res,
  planar reflections e volumétrica sem teto térmico. O alvo aqui é o **conjunto
  de mecanismos** dele dentro de um orçamento mobile, com degradação declarada.
- **Não promete simulação de arrebentação.** W7 é flipbook instanciado. É a
  escolha certa para o orçamento, e está dito.
- **Não promete refração com paralaxe.** W5.3 depende de cor de cena amostrável,
  que em GPU tile-based é uma resolve. Fica atrás de enable e só entra se o par
  intercalado provar que cabe.
- **Nenhum arquivo do KWS entra no repositório.** §2.2.
- Nada aqui muda de estado por estar escrito. Uma etapa só muda de estado em
  `ESTADO.md` quando compila, passa em teste, e tem par intercalado publicado.
