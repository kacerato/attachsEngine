# Plano de água e ambiente — ASTRA

Plano novo, escrito depois de varrer os plugins de referência, ler o código da
própria engine e **medir o oceano no aparelho**. Substitui os planos de água
anteriores deste repositório; nada aqui foi herdado deles.

Três marcações aparecem em todo o documento e valem literalmente:

- **[MEDIDO]** — número obtido neste aparelho, nesta semana, com o comando ao lado.
- **[CITADO]** — valor lido no código-fonte de um plugin ou na documentação dele.
- **[ESTIMADO]** — projeção minha. Nenhum critério de aceite depende de estimativa.

---

## 1. Evidência: o que foi varrido

### 1.1 Plugin com código-fonte na máquina

`C:\Users\donod\Downloads\extracted\extracted` é um projeto Unity contendo o
**KWS2 — KriptoFX Water System**, completo e com fonte: 383 arquivos, 44
shaders, 86 scripts C#. É exatamente o produto do vídeo que você mandou
(`youtu.be/2l2Er_NyGMs` → "KWS2 Dynamic Water System").

O que ele resolve, e como — lido no código, não na página de venda:

| Subsistema | Arquivo | Técnica |
|---|---|---|
| Espectro do oceano | `FFT/Spectrum_GPU.compute` | Pierson-Moskowitz + espalhamento cos², dispersão de água profunda, ruído gaussiano por Wang hash |
| FFT | `FFT/ComputeFFT_GPU.compute` | FFT GPU, 3 cascatas de domínio **{20, 40, 160} m** [CITADO] |
| Malha do oceano | `Mesh/MeshQuadTree.cs` (625 linhas) | quadtree com instâncias por nível, culling de frustum exato/aproximado, atualização a cada **3 m para frente / 0,5 m para trás / 1°** [CITADO] |
| Ondas dinâmicas | `GraphicsPass/KWS_DynamicWaves.shader` | **equação de onda em diferenças finitas**, 5 taps + frame anterior, RT `R32_SFloat` + normais `R16G16_SFloat`, teto 2048² [CITADO] |
| Fluido / correnteza | `GraphicsPass/KWS_FluidSimulation.shader` | advecção semi-lagrangiana + viscosidade + confinamento de vorticidade; constantes `K=0.15, v=0.06, dt=0.13` [CITADO]; flowmap entra como força externa; LOD em duas áreas + simulação pré-assada como fundo |
| Rios | `KW_FlowMap.cs`, `KWS_SplineMesh.cs` (714 linhas) | flowmap `R16G16_SFloat` pintado no editor com pincel + malha gerada por spline; **5 a 25 vértices por rio** [CITADO] |
| Arrebentação | `KW_ShorelineWaves.cs` | ondas de quebra **pré-assadas e instanciadas** ao longo da costa (não simuladas), área 150 m, textura 2048², LODs a 20…200 m [CITADO] |
| Cáusticas | `CommandPass/KWS_Caustic_Pass.shader` | projeção da malha da água contra um depth ortográfico do terreno; textura 768², malha 320², 3 LODs, dispersão cromática [CITADO] |
| Reflexão | `KWS_SSR.compute`, `PlanarReflection.cs`, `CubemapReflection.cs` | SSR por hash + preenchimento de buracos, resolução **75%…20%** da tela conforme perfil [CITADO]; planar 768…128; cubemap com intervalo de 10 s |
| Submerso | `CommandPass/KWS_Underwater.shader` | névoa volumétrica + máscara de superfície + blur opcional |
| Volumétrica | `KWS_VolumetricLighting.shader` | 6 iterações, blur bilateral, resolução 75%…15% [CITADO] |
| Flutuação | `KW_Buoyancy.cs` | **voxelização do collider**, 1–6 fatias por eixo, teto de 16 voxels, força de Arquimedes dividida igualmente [CITADO] |

### 1.2 Repositórios abertos

- **[GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves)** — MIT
  (Ethan Truong, 2024), pode ser adaptado com atribuição. Espectro **TMA**
  (JONSWAP + atenuação por profundidade) com fetch, profundidade, `spread` (μ),
  `swell` (ξ) e `detail` (δ); FFT **Stockham** (dispensa a permutação
  bit-reversa); espuma por **jacobiano negativo**, acúmulo linear e decaimento
  exponencial; BSDF do GDC "Atlas" com **GGX no lugar de Beckmann**; filtragem
  mista bicúbica/bilinear das normais por densidade de pixel. O próprio autor
  registra que o controle de interferência entre cascatas é frágil.
- **[Waterways](https://github.com/Arnklit/Waterways)** — MIT, rios por curva de
  Bézier com **assamento de flow map e foam map**, e um `WaterSystem` que gera
  mapas globais de altura e fluxo a partir dos rios filhos.

### 1.3 Sistemas comerciais consultados (documentação)

- **[Crest](https://crest.readthedocs.io/en/stable/about/introduction.html)** —
  FFT e Gerstner, LOD em cascatas, *ripple sim* para esteira de barco, *wave
  splines* para rios/lagos/costa. A própria documentação diz que é **mirado em
  PC e console** e que "pode funcionar" em mobile.
- **[UE5 Water](https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine)**
  — Gerstner analítico, rio por spline que esculpe o landscape. Sem simulação.
- **[Fluid Flux](https://www.strayspark.studio/blog/ocean-water-simulation-ue5-guide)**
  — **Shallow Water Equations** em grade 2D: altura + velocidade, propagação,
  refração e fusão de ondas. Heightfield, não volume: não faz respingo vertical.

### 1.4 Licença — o que pode ser copiado

- **KWS2 é licenciado ao autor deste projeto.** A cópia em
  `Downloads/extracted` não traz arquivo de licença porque a Asset Store não
  põe um em cada pacote — a licença é da conta, não do diretório. O acesso ao
  fonte é legítimo e o código pode ser lido, adaptado e usado nos produtos do
  licenciado.
- **GodotOceanWaves é MIT** (Ethan Truong, 2024): adaptar mantendo o aviso de
  copyright. É a fonte recomendada para o espectro e para a FFT, e por um motivo
  técnico antes de qualquer outro — o TMA dele é melhor que o Pierson-Moskowitz
  do KWS2, e é o mesmo modelo que a ASTRA já implementa.
- **Waterways é MIT**: idem, para o assamento de rio.

**A distinção que continua valendo** não é sobre ler o código, é sobre *para
onde ele vai*: usar KWS2 nos jogos do licenciado é uso normal; embutir o fonte
dele numa engine que outras pessoas vão usar para fazer os jogos delas é
redistribuição de asset, que é o que a EULA da Asset Store restringe. Como a
ASTRA é uma engine com launcher e criação de projeto — feita para terceiros
criarem —, essa fronteira é do produto, não do desenvolvimento.

Isso não muda a §9 na prática. Onde ela diz "reimplementar pela técnica", a
razão é técnica antes de jurídica: o KWS2 é HLSL sobre o pipeline da Unity, com
`CommandBuffer`, `RTHandle`, `multi_compile` e macros de SRP. Traduzir isso para
GLSL sobre o frame graph Vulkan desta engine não é copiar — é reescrever com o
algoritmo à vista, que é exatamente o que o plano descreve. Ter a licença
significa que o algoritmo pode ser lido de perto, e é o que torna a §9
executável em vez de adivinhação.

---

## 2. Onde a ASTRA está hoje — medido, não suposto

### 2.1 O que a engine já tem (lido no código)

Bem mais do que os planos antigos sugeriam:

- `native/renderer/water_fft.h` — espectro **TMA/JONSWAP** já com `fetch`,
  `swell`, `spread`, `depth`, `shortWaveDamping`, banda de comprimento de onda.
  O mesmo modelo do GodotOceanWaves, e superior ao Pierson-Moskowitz do KWS2.
- `water_cascades.h` — até **4 cascatas**, validação que **rejeita bandas
  sobrepostas** em vez de silenciosamente somar energia. Isso resolve por
  construção o problema que o autor do GodotOceanWaves admite não ter resolvido.
- `rhi/water_spectral_compute.h` — FFT em Vulkan compute (evolve → inverse →
  foam → pack), slopes em half-float com fallback largo.
- `water_foam.h` — espuma com **solução exata** de `dF/dt = source(1−F) − decay·F`,
  independente da taxa de atualização.
- `water_field.h` / `water_world.h` — consulta compartilhada (altura, normal,
  velocidade orbital, fluxo, cobertura, espuma, jacobiano, profundidade), com
  correntes direcional/radial/vórtice, batimetria, volumes de exclusão, até 16
  volumes com prioridade e camadas, **em lote e reentrante**.
- `physics/water_buoyancy.h` — **volume submerso exato por tetraedro**, massa
  adicionada, arrasto viscoso e quadrático, saturação contada como falha de
  tuning. Isto é **melhor que a flutuação por voxel do KWS2**.
- `water_surface_shading.glsl` — Beer-Lambert com espessura real vinda do
  subpass depth, GGX, filtro de variância de normal (specular AA), espuma de
  contato por espessura, e um modo de **isolamento de custo** por termo.

### 2.2 O que está faltando (verificado por busca no código)

`grep` por `ScreenSpace|Planar|caustic|underwater` em `native/`: só existe o
**enum** `WaterReflection::ScreenSpace/Planar` e o *fallback* em
`resolveWaterPipeline`, que rebaixa os dois para `Environment`. Não há
implementação. Também não há cáusticas, submerso, volumétrica, refração com
distorção, arrebentação, rio, nem ondas dinâmicas de interação — o que existe
são **8 impulsos analíticos** (`WaterInteractionField`).

### 2.3 A captura — as imagens que você pediu

Não achei valor em colar screenshot de jogo de terceiro: o que interessa é a
água **desta** engine, neste aparelho, com defeito apontado. Duas imagens
versionadas:

- `assets/astra-visual/reference/water-baseline-ocean.png` — a captura limpa.
- `assets/astra-visual/reference/water-baseline-gaps.png` — a mesma, com os seis
  defeitos marcados.

Como referência externa de para onde isso vai, as fontes da §1 são o alvo
declarado: KWS2 (o vídeo que você mandou), GodotOceanWaves e Crest.



`assets/astra-visual/reference/water-baseline-*.png` — oceano espectral rodando
no aparelho, 2772×1280. O que essa imagem prova que funciona: ondas espectrais
com cristas coerentes, espuma nas cristas, absorção Beer-Lambert (o fundo raso
puxa para o amarelo), quatro corpos flutuando com sombra projetada no fundo,
céu HDR.

E o que ela prova que falta, sem precisar de opinião:

1. **O barco não se reflete na água.** Só existe reflexão de ambiente.
2. **Nada refrata.** O fundo submerso aparece sem deslocamento pela superfície.
3. **Não há cáustica** no fundo iluminado.
4. **O barco não gera esteira nem espuma de casco** — ele flutua sobre uma
   superfície que ignora que ele existe.
5. **O horizonte é uma linha dura**: sem perspectiva aérea, a água encontra o
   céu num degrau.
6. **A espuma é isotrópica** — não se alonga na direção da crista.

### 2.4 Os números [MEDIDO]

Aparelho `25053PC47G` (SM8735 / Adreno), Android 16, 2772×1280 = 3,55 Mpx,
escala 1,00, resolução dinâmica desligada, **build debug**, cena `samples/ocean`
com 16 draws / 168.729 triângulos, câmera travada na pose `(0, 12, −34)`,
`yaw 0`, `pitch 0,28` — verificada byte a byte no log de cada rodada.

#### A bancada teve de ser construída antes dos números

A primeira tentativa de decompor o custo falhou de forma clara: três dos cinco
modos de `WaterCostIsolation` "economizaram" tempo negativo, e remover a
reflexão deixou o frame 11 ms **mais caro**. Investigando, a mesma configuração
com a **mesma pose** mediu 15,575 ms numa execução e 26,640 ms noutra — 71% de
diferença sem nada mudar na cena.

A causa é o relógio da GPU. O aparelho roda sem root, `/sys/class/kgsl` não é
acessível e o governor não pode ser fixado: quando a carga cai, a frequência
cai junto, e o tempo por quadro deixa de ser monotônico com o trabalho.

`tools/measure-ocean-paired.ps1` resolve isso sem root, intercalando as pontas —
`A B A B … A` — e comparando cada B com a média dos dois A vizinhos, o que
cancela deriva linear dentro do par. O desvio entre as réplicas de A vira a
régua da bancada, e o relatório carrega um campo `conclusive` que recusa
qualquer efeito menor que ela.

O resultado justifica a ferramenta: **dentro de uma sessão contínua a referência
ficou em 26,584 ms com desvio de 0,039 ms entre quatro réplicas — 0,15%.** O
problema nunca foi medir; era comparar rodadas separadas por minutos.

**E a bancada tem uma segunda condição, descoberta do jeito difícil.** Das cinco
decomposições, três saíram inconclusivas — e a ferramenta as recusou sozinha:

| Pareamento | Referência | Desvio entre réplicas | Efeito | Veredito |
|---|---:|---:|---:|---|
| água chapada | 26,584 ms | **0,039** | −7,766 ms | conclusivo |
| reflexão | 26,834 ms | **0,047** | −0,443 ms | conclusivo |
| sombra | 25,919 ms | 1,421 | −1,218 ms | inconclusivo |
| micro-normais | 25,951 ms | 1,410 | −0,379 ms | inconclusivo |
| detalhe espectral | 20,483 ms | **6,058** | +5,614 ms | inconclusivo |

O que separa as duas primeiras linhas das outras não é a técnica medida: é a
temperatura. As séries conclusivas correram com o aparelho já quente e faixa
térmica de 0,4 a 1,0 °C; a pior começou a 30 °C, com o aparelho recém-esfriado,
e mediu desvio de 6 ms entre réplicas da **mesma** configuração.

Intercalar cancela deriva *linear*, e o aquecimento de um Adreno não é linear no
tempo. Por isso a bancada ganhou um aquecimento descartado antes da primeira
medida e uma guarda térmica: uma série que varia mais de 1,5 °C não conclui
nada, por mais limpos que os números pareçam.

Fica registrado um terceiro modo de falha, que nenhuma guarda resolve: o
aparelho é de uso pessoal, e uma série foi perdida quando outro aplicativo subiu
e o gerenciador de energia do sistema matou o processo em segundo plano. Medição
longa nesse aparelho precisa de modo avião e nada mais rodando.

E um quarto, que era da própria ferramenta: a classificação das pontas usava
`-like '*-A*'`, insensível a maiúsculas no PowerShell, então um experimento
chamado `f0-ambiente` casava com o "-a" do próprio nome. As cinco medidas
entraram como referência, o desvio inflou de 0,005 para 0,188 ms e um efeito
real de 0,342 ms foi declarado inconclusivo. O filtro passou a ser `-cmatch`
ancorado no sufixo, e a contagem das pontas agora falha alto quando não bate —
uma bancada que classifica errado em silêncio é pior que não ter bancada.

```bash
$env:ANDROID_SERIAL="<serial>"
./tools/measure-ocean-paired.ps1 -Name f0-flat -IsolationB 5 -Repeats 3
python tools/summarize_water_runs.py --out docs/measurements/<arquivo>.json
```

#### O frame do oceano, aberto

Com a régua de pé, a telemetria de `[FrameProfilePasses]` fecha a conta. Todas
as colunas abaixo têm desvio ≤ 0,04 ms entre réplicas:

| Componente | ms | % do frame |
|---|---:|---:|
| Passe opaco — **tudo menos o shading da água** | **12,639** | **48%** |
| Passe opaco — shading da água | 7,493 | 28% |
| Pós-processamento | 4,736 | 18% |
| Simulação espectral (compute, 3 cascatas) | 1,767 | 7% |
| **Frame** | **26,578** | |

O efeito pareado do shading inteiro é **−7,766 ms ± 0,008** (o passe opaco cede
7,493 e o post cede 0,321, porque menos brilho gera menos bloom). Dentro dele, o
único termo isolado com veredito conclusivo até agora é a **reflexão de
ambiente: 0,443 ms ± 0,046** — barata, e portanto não é onde o shading gasta.
A subdivisão do restante (detalhe espectral, sombra, micro-normais) fica
pendente: as três séries foram recusadas pela guarda térmica e precisam ser
refeitas com o aparelho em regime.

**O que isso diz, e reordena o plano:**

1. **O maior consumidor não é a água — são os 12,639 ms do resto do passe
   opaco**, numa cena com 16 draws e 168 mil triângulos. Isso não é geometria:
   é a superfície transparente cobrindo a tela e gerando fragmento. Overdraw.
2. **O pós-processamento custa 4,736 ms**, 18% do frame, para tonemap, bloom e
   FXAA. O `ORCAMENTO-120HZ.md` mediu 1,10 ms na cena de estrada, **em release**.
   A diferença precisa ser explicada antes de qualquer coisa ser otimizada — ou
   é custo de debug, ou é bloom em resolução cheia.
3. **A simulação FFT é barata: 1,767 ms** para três cascatas, e não muda quando
   o shading sai. Ela não é o problema, e cabem mais cascatas se houver
   orçamento — o gargalo está no consumo, não na geração.
4. O orçamento de 120 Hz é 8,333 ms e o de 60 Hz é 16,6 ms. Só o passe opaco sem
   água já gasta 12,6 ms.

O diagnóstico é o mesmo do `ORCAMENTO-120HZ.md` na cena de estrada — lá o passe
opaco só com base color custava 8,811 ms, mais que o frame inteiro a 120 Hz:
**o gargalo é quantidade de fragmentos, não custo por fragmento.** Por isso a §7
vem antes da §5.

> Ressalva: tudo acima é **debug**. `android/app/build.gradle.kts` agora tem
> `signingConfig` no `release`, então a rodada release já pode existir; ela é a
> primeira tarefa restante da Fase 0. Até lá esses números são um teto.

As rodadas, os relatórios pareados e a decomposição por passe estão em
`docs/measurements/`.

---

## 3. A restrição que governa o plano inteiro

Um plano de água para esta engine que comece por "adicionar SSR, cáusticas e
volumétrica" está morto na chegada: são três consumidores de fragmento numa cena
que já gasta de 1,9× a 3,2× o orçamento de 120 Hz em fragmentos, e estoura o de
60 Hz assim que a câmera desce para perto da água.

Então a ordem é invertida em relação a como esses plugins são normalmente
adotados:

**Primeiro se compra orçamento. Depois se gasta.**

Cada fase de qualidade tem, ao lado, o custo em ms que ela pode consumir, e esse
custo tem de vir de uma economia já medida. Uma fase que não cabe não entra —
ela espera a economia que a habilita.

---

## 4. Diversidade de ondas: seis sistemas, um campo

Você pediu diversidade de ondas. Diversidade real não é aumentar o número de
cascatas: é ter **sistemas de onda com física diferente** somados no mesmo campo
consultável por render e por física. São seis, e a engine hoje tem um e meio.

| # | Sistema | O que produz | Estado | Fonte da técnica |
|---|---|---|---|---|
| 1 | **Oceano espectral** | mar aberto, swell, vagas, capilaridade | **existe** (TMA, 4 cascatas, FFT GPU) | GodotOceanWaves (MIT) |
| 2 | **Swell cruzado** | dois trens de onda em direções diferentes; mar confuso | parcial — `WaterWaveAuthoring.crossSwell` existe no autor, não no espectro | TMA com dois picos |
| 3 | **Ondas dinâmicas** | ondulação de impacto, chuva, esteira de casco | só 8 impulsos analíticos | KWS2 `DynamicWaves` (diferenças finitas) |
| 4 | **Arrebentação de costa** | quebra, crista virando, língua de espuma na areia | **não existe** | KWS2 `ShorelineWaves` (instanciada) |
| 5 | **Rio / correnteza** | fluxo direcional, remanso, corredeira | domínio `RiverSpline` declarado, sem implementação | Waterways (MIT) + KWS2 flowmap |
| 6 | **Onda longa** | maré, tsunami, marulho de canal | `appendLongWaterWave` existe, é linear | águas rasas (SWE 1D) |

O ponto de junção já está construído: `WaterField::sample` devolve altura,
normal, velocidade orbital, fluxo, espuma e jacobiano num lote só. **Todo sistema
novo entra por ali**, e por consequência a física, a câmera e o áudio recebem a
onda nova de graça, sem que nenhum deles saiba que ela existe.

### 4.1 O que fazer em cada um

**(1) Oceano — afinar, não reescrever.** Trocar a FFT atual pela **Stockham** do
GodotOceanWaves elimina a permutação bit-reversa e melhora o padrão de acesso;
adotar a filtragem mista bicúbica/bilinear por densidade de pixel resolve o
cintilar das normais ao longe. Adotar os nomes de parâmetro dele (`spread`,
`swell`, `detail`) mantém as configurações do repositório dele diretamente
utilizáveis como presets.

**(2) Swell cruzado.** Somar um segundo pico direcional no TMA, com direção e
período independentes. Custo: zero em runtime — é geração de espectro, que já é
operação de reconfiguração, não de frame.

**(3) Ondas dinâmicas — a maior diferença visual por ms gasto.** Uma textura
`R32F` de altura mais duas de histórico, atualizada por diferenças finitas
(5 taps), com injeção por corpos e por chuva; normais em `R16G16F`. É o método do
KWS2 e é barato: numa área de 25 m a 40 px/m dá 1024², um passe de compute com
5 leituras por texel. É o que faz o barco **existir** para a água: esteira,
ondulação de impacto, chuva.

**(4) Arrebentação.** A escolha do KWS2 aqui é a certa para mobile e vale copiar
como decisão de arquitetura: **não simular**. Ondas de quebra são malhas
animadas pré-assadas, instanciadas ao longo da linha de costa, com LOD por
distância e culling de frustum. A engine já tem batimetria (`WaterBathymetry`)
para saber onde a costa está e a que profundidade a onda quebra.

**(5) Rio.** Spline → malha + flow map assado, como no Waterways. O `WaterField`
já tem `flow` no resultado da amostra e `WaterCurrentSettings` com fontes
direcional/radial/vórtice: o rio preenche esse campo em vez de inventar um
caminho paralelo. `WaterDomain::RiverSpline` já está declarado.

**(6) Onda longa.** Manter linear até existir um caso que exija SWE. Fluid Flux
mostra que SWE em grade resolve, e mostra o custo: é uma simulação por frame
sobre a área inteira. Não cabe no orçamento atual.

---

## 5. Qualidade gráfica da água

Ordenado por **razão entre ganho visual e ms**, do melhor para o pior. Os custos
são [ESTIMADO] até serem medidos; nenhum entra sem medição prévia.

| Ordem | Recurso | Por que vem aqui | Custo alvo |
|---|---|---|---:|
| 1 | **Refração com distorção** | o depth do subpass já está na memória do tile; falta a cor. Corrige o item 2 da §2.3 | ≤ 0,6 ms |
| 2 | **Espuma direcional** | alongar a espuma ao longo da crista usando o jacobiano anisotrópico que a FFT já produz. É mudança de shader, sem passe novo | ≈ 0,1 ms |
| 3 | **Espuma e onda de casco** | vem de graça com o sistema (3); é o que faz o barco parecer pesado | incluso |
| 4 | **Subsurface nas cristas** | retroiluminação da crista fina: o termo de espalhamento já existe no shader, falta modulá-lo pela altura e pela espessura da crista | ≈ 0,15 ms |
| 5 | **Perspectiva aérea** | mata a linha dura do horizonte e é global, não só da água (§6) | ≤ 0,4 ms |
| 6 | **SSR** | corrige o item 1 da §2.3, o mais visível de todos — e o mais caro. KWS2 roda a **20–75% da resolução** por perfil; em mobile só a ponta baixa é discutível | 1,2–2,5 ms |
| 7 | **Cáusticas** | forte em água rasa e clara, invisível em mar aberto. Depende de um depth ortográfico do fundo | 0,8–1,5 ms |
| 8 | **Submerso** | muda o jogo quando a câmera entra na água; até lá, custo zero e ganho zero | 1,0–2,0 ms, só submerso |
| 9 | **Volumétrica na água** | god rays sob a superfície. KWS2 roda a 15–75% da resolução | ≥ 1,5 ms |

Itens 1 a 5 somam **≈ 1,25 ms** e cabem. Itens 6 a 9 somam **4,5–7,5 ms** e
**não cabem hoje** — dependem inteiramente da §7.

---

## 6. Ambiente: o que "HDRP excepcional" significa aqui

A engine já tem a espinha certa e isso precisa ser dito: IBL com specular
pré-filtrado octaédrico + BRDF LUT split-sum, sombras em atlas com cascatas e
PCF, bloom, FXAA, TAA, tonemap, resolução dinâmica e controlador térmico. Isso é
arquitetura de pipeline HDR moderno, não de um renderer de brinquedo.

O que falta para o ambiente ficar à altura da água:

1. **Atmosfera fisicamente baseada** (Bruneton/Hillaire com LUTs pré-computadas)
   no lugar do céu procedural atual. Ela entrega, do mesmo cálculo: cor de céu
   correta ao longo do dia, **perspectiva aérea** (item 5 da §5) e a luz
   ambiente que a água usa. Uma LUT de transmitância 256×64 e uma de scattering
   32×32×32 são baratas e se atualizam por quadro só quando o sol muda.
2. **Névoa por altura com profundidade**, coerente com a atmosfera — é o que
   separa "água boa" de "cena boa".
3. **Exposição automática**. Hoje a exposição é constante
   (`parameters.x`). Entrando e saindo da água, ou do dia para a noite, o
   resultado é lavado ou escuro; com adaptação, não é.
4. **Reflexão especular do sol com forma de disco** e nuvens refletidas — o KWS2
   trata isso como item de primeira classe (`ReflectSun`,
   `ReflectedSunCloudinessStrength`) porque é o que dá escala ao mar.
5. **Subir o ambiente da cena de água — medido, e é barato.** O log da rodada
   dizia `perfil=1 ... ambiente=hemisferio`. Em `rendering_policy.cpp`,
   `AmbientQuality::Hemispheric` resolve para `{true, false, false}`: **ambiente
   hemisférico sem probe especular e sem IBL pré-filtrado**. Só os perfis S e A
   recebem `HemisphericSpecular`. A reflexão pobre da captura nunca foi
   limitação do shader da água — era o perfil deste aparelho rebaixando o
   ambiente.

   O experimento foi feito: **subir esta cena para `HemisphericSpecular` custa
   0,342 ms ± 0,018** [MEDIDO], contra uma referência de 26,551 ms ± 0,005.
   São 1,3% do frame, e a comparação visual está em
   `assets/astra-visual/reference/water-ambient-ab.png`: as caixas flutuantes
   passam de chapadas a refletindo o céu, com nuvens visíveis na face azul.

   Este é o melhor negócio de qualidade por milissegundo que a medição encontrou
   até agora, e ele não é da água — é da cena inteira. A decisão de perfil que o
   rebaixou merece ser revista com esse número na mão.

---

## 7. Desempenho fora do comum — a fase que paga todas as outras

A §2.4 já disse onde o frame está. A fila abaixo é a mesma decomposição, virada
em trabalho, do maior alvo para o menor. Nenhuma dessas técnicas é específica de
água: todas valem para a engine inteira, que é o que você pediu com "aplicado de
maneira global".

| # | Alvo medido | Técnica | Por que ataca este alvo |
|---|---:|---|---|
| 1 | **12,639 ms** — passe opaco sem a água | **Cortar o overdraw da superfície** | 48% do frame numa cena de 16 draws não é geometria, é a superfície transparente cobrindo a tela. Um *depth prepass* de água, ou clipe por cobertura antes do sombreamento, remove fragmento que nunca vai aparecer. É o maior item da lista por larga margem. |
| 2 | idem | **Densidade do clipmap ligada ao vento** | KWS2 escala o LOD do chunk pelo vento (`{0.5, 0.75, 1, 1.5, 2, 2.5}` [CITADO]): mar calmo não precisa de malha de tempestade. Menos vértice, menos fragmento. |
| 3 | idem | **Perfis de malha de verdade** | KWS2 tem 5 níveis Ultra→VeryLow com contagens explícitas de chunk [CITADO]; a ASTRA tem `clipmapLevels` e pouco mais. |
| 4 | **4,736 ms** — pós-processamento | **Explicar antes de otimizar** | 18% do frame para tonemap, bloom e FXAA, contra 1,10 ms medidos em release na cena de estrada. Ou é custo de debug, ou é bloom em resolução cheia — e isso se descobre com uma rodada, não com uma reescrita. |
| 5 | **7,493 ms** — shading da água | **Escala de render separada para a água** | O resto da cena a 1,00 e a água a 0,7 é imperceptível em movimento e corta o custo por pixel dela quase pela metade. |
| 6 | idem | **Resolução dinâmica ligada** | Está **desligada** nas rodadas medidas, e a engine já tem o controlador pronto. |
| 7 | **1,767 ms** — simulação FFT | **Cascatas em ritmos diferentes** | O menor alvo da lista, e por isso o último. O GodotOceanWaves atualiza cascatas seletivamente; a cascata de 160 m não precisa mudar a 120 Hz. |
| 8 | custo futuro | **Refração e reflexão em meia resolução** | É como todo plugin da §1 faz; nenhum roda SSR em resolução nativa. Vale fixar isso antes de a §5 item 6 existir, não depois. |

A ordem importa: os itens 1 a 3 atacam 12,639 ms, o item 4 mais 4,736 ms, e só
então se chega ao shading. Um plano que começasse otimizando o shader da água
estaria mexendo em 28% do frame e deixando 48% intocado.

### 7.1 O que "fora do comum" quer dizer, em número

Vale fixar a régua, senão a frase não significa nada. Crest se declara mirado em
PC e console. KWS2 tem perfis para mobile, mas os seus mínimos são SSR a 20% da
tela e volumétrica a 15%. A régua desta engine é diferente e mais dura:

> **Oceano espectral, com corpos flutuando, a 3,55 Mpx nativos, em 60 Hz
> sustentados sem resolução dinâmica; e a 120 Hz com resolução dinâmica dentro
> da janela [0,7 … 1,0].**

Isso é 16,6 ms no alvo baixo e 8,3 ms no alto, contra **26,578 ms** [MEDIDO] na
pose de referência, em debug. Falta cortar 10 ms para 60 Hz e 18,2 ms para
120 Hz — e a §7 mostra que 12,6 deles estão num único lugar.

É por isso que a §7 vem antes da §5, e não depois.

### 7.2 Memória, não só milissegundos

Seis sistemas de onda somados são texturas residentes, e VRAM em mobile é
compartilhada com o sistema. O orçamento precisa ser escrito junto com o de ms,
na Fase 0, e cada sistema novo declara o seu:

| Sistema | Recursos residentes | Ordem de grandeza [ESTIMADO] |
|---|---|---|
| Oceano (4 cascatas 256²) | h0, espectro evoluído, deslocamento, slopes, espuma | ~24 MB |
| Ondas dinâmicas 1024² | altura + 2 históricos (`R32F`) + normais (`RG16F`) | ~14 MB |
| Flow map de rio 1024² | `RG16F` + foam | ~4 MB |
| Arrebentação | malhas pré-assadas + atlas de espuma 2048² | ~20 MB |
| Cáusticas | depth ortográfico 2048² + textura 768² | ~10 MB |
| SSR a 50% | cor + hash em meia resolução | ~12 MB |

A engine já tem `rhi/memory_budget.h`; o que falta é o orçamento de água entrar
nele com um teto declarado por perfil de dispositivo.

### 7.3 Critério de saída

O critério de saída desta fase é numérico: **a cena de oceano em release, escala
1,00, abaixo de 16,6 ms de GPU na mesma pose**. Sem isso, as fases 6 a 9 da §5
não abrem.

---

## 8. Física real

Aqui a ASTRA já está à frente do KWS2 e o plano é aproveitar isso, não refazer.

**O que já é melhor:** volume submerso **exato por tetraedro** contra o plano
d'água ajustado, centro de empuxo correto, massa adicionada, arrasto viscoso e
quadrático, aplicação em lote (`AetherPhysics_ApplyWaterForces`), saturação
contada. O KWS2 divide o collider em no máximo 16 voxels e aplica a mesma força
em cada um — isso erra o calado e põe o centro de empuxo no lugar errado, que é
exatamente por que um barco adorna para o lado errado.

**O que falta, em ordem:**

1. **Casco de verdade.** Hoje só há esfera e caixa (`BuoyantShapeKind`). O passo
   é o casco convexo do corpo Jolt, integrado tetraedro a tetraedro contra o
   plano — a matemática já está escrita, falta a fonte de geometria.
2. **Plano ajustado por mínimos quadrados**, com 3 a 5 amostras do `WaterField`
   por corpo em vez de uma. Um casco longo numa onda curta sente um plano
   inclinado; com uma amostra só, sente a média e não balança.
3. **Reação do corpo na água.** Hoje a água empurra o corpo e o corpo não
   empurra a água. Fechar o laço com o sistema (3) da §4: a região submersa
   injeta altura na textura dinâmica. É o que produz esteira e ondulação de
   impacto — e não custa quase nada, porque a injeção é um desenho na textura
   que já vai existir.
4. **Arrasto por painel** para cascos, no lugar de um coeficiente global: força
   normal proporcional à área projetada de cada face contra o fluxo relativo. É
   o que faz um leme funcionar e um casco planar.
5. **Personagem na água** — nadar, boiar, sair na margem. `character_motor.cpp`
   e `WaterRuntime` existem; falta o estado.
6. **Passo fixo declarado.** `WaterRuntime::apply` já exige ser chamado
   imediatamente antes de `Physics_Step` com o mesmo relógio. Isso precisa virar
   teste, não comentário: flutuação instável quase sempre é relógio, não força.

---

## 9. Adaptação direta dos plugins

O mapeamento pedido, técnica por técnica, com o destino dentro desta engine.
Onde a fonte é MIT, é adaptação de código com atribuição. Onde é KWS2, é
reescrita a partir do algoritmo lido — o fonte é acessível pela licença do
autor (§1.4), mas é HLSL sobre o pipeline da Unity e não tem para onde ser
colado dentro de um frame graph Vulkan.

| Origem | Arquivo de origem | Destino na ASTRA | Forma |
|---|---|---|---|
| GodotOceanWaves | espectro TMA + spread/swell/detail | `renderer/water_fft.cpp` | adaptar código (MIT, com atribuição) |
| GodotOceanWaves | FFT Stockham | `rhi/shaders/water_fft_inverse.comp` | adaptar código (MIT) |
| GodotOceanWaves | filtragem bicúbica/bilinear por densidade | `water_spectral_sampling.glsl` | adaptar código (MIT) |
| GodotOceanWaves | presets de vento/fetch/profundidade | `assets/.../water/*.aewr` | usar como valores |
| Waterways | assamento de flow/foam map por Bézier | ferramenta de rio + `WaterCurrentSettings` | adaptar código (MIT) |
| KWS2 | diferenças finitas de onda dinâmica | novo `renderer/water_dynamic.cpp` + `.comp` | reescrever para compute (o original é um blit de fragment shader) |
| KWS2 | LOD de chunk relativo ao vento | `planWaterClipmap` | usar as tabelas direto (§1.1 tem os valores) |
| KWS2 | arrebentação instanciada e pré-assada | novo `renderer/water_shoreline.cpp` | copiar a arquitetura; as malhas assadas são conteúdo, não código |
| KWS2 | cáustica por depth ortográfico | novo passe no frame graph | reescrever (depende de `CommandBuffer` e decal da Unity) |
| KWS2 | SSR em fração da resolução, com preenchimento | `WaterReflection::ScreenSpace` | reescrever (depende de `RTHandle` e do SRP) |
| KWS2 | perfis de qualidade por plataforma | `rendering_policy.cpp` | reimplementar (é política) |
| Crest | *wave splines* | domínio `RiverSpline` | conceito |
| Fluid Flux | SWE em grade | fora de escopo por orçamento | registrado, não adotado |

---

## 10. Ordem de execução

Cada fase tem um portão numérico. Uma fase não começa antes de a anterior passar.

**Fase 0 — a bancada, antes de qualquer shader.** Parcialmente feita:

- ✅ Bancada pareada com aquecimento e guarda térmica
  (`tools/measure-ocean-paired.ps1`), régua de 0,04 ms em regime.
- ✅ Decomposição do frame por passe (§2.4) e o custo total do shading da água.
- ✅ Assinatura de release configurada — `assembleRelease` agora produz um APK
  instalável.
- ⬜ Refazer as três decomposições recusadas pela guarda térmica.
- ⬜ Rodar a base em **release** e comparar com o orçamento.
- ✅ Custo de `AmbientQuality::HemisphericSpecular` medido: **0,342 ms ± 0,018**,
  1,3% do frame, com ganho visual direto (§6, item 5).
- ⬜ Declarar o orçamento de VRAM da §7.2.

*Portão: a tabela de custo por termo, em release, com todos os termos
conclusivos.* Sem esse portão, nenhuma otimização das fases seguintes é
verificável.

**Fase 1 — comprar o orçamento** (§7, itens 2 a 8). *Portão: oceano em release,
escala 1,00, ≤ 16,6 ms de GPU na mesma pose.*

**Fase 2 — o que cabe agora** (§5, itens 1 a 5; §4 itens 2 e 3; §8 itens 1 a 3).
Refração, espuma direcional, ondas dinâmicas com esteira, subsurface de crista,
casco real. *Portão: ≤ 16,6 ms mantidos; o barco gera esteira visível; o casco
adorna na direção certa numa onda lateral.*

**Fase 3 — ambiente** (§6, itens 1 a 4). Atmosfera, névoa, exposição
automática, sol especular. *Portão: ≤ 16,6 ms mantidos; horizonte sem degrau.*

**Fase 4 — mundo de água** (§4 itens 4, 5 e 6). Arrebentação, rio, onda longa.
*Portão: uma cena de costa e uma de rio nas cenas de exemplo do shell, dentro do
orçamento.*

**Fase 5 — o que é caro** (§5, itens 6 a 9). SSR, cáusticas, submerso,
volumétrica — **cada um só entra se a Fase 1 tiver sobrado ms para ele**, e cada
um com perfil por dispositivo, como todo plugin da §1 faz.

---

## 11. Riscos, ditos agora

1. **Os 26,662 ms são debug.** Se o release não abrir uma folga grande, a Fase 1
   fica mais dura do que este plano supõe e a Fase 5 pode nunca abrir neste
   aparelho. Por isso a Fase 0 é a primeira.
2. **SSR em mobile é o item mais provável de ser cortado.** Ele corrige o defeito
   mais visível da captura e é o mais caro. Se não couber, o substituto honesto
   é reflexão planar só para o plano d'água, com lista de objetos reduzida.
3. **A medição é frágil, e isso já se manifestou aqui.** A decomposição por
   termo da §2.4 saiu não-monotônica: três modos de isolamento "economizaram"
   tempo negativo. GameTurbo, Game Mode, governor de GPU e janela térmica
   invalidam rodadas. Enquanto a Fase 0 não fechar, **nenhum ganho reivindicado
   por este plano deve ser aceito sem duas execuções concordantes.**
4. **KWS2 é referência de leitura, não fonte de arquivo.** O código dele é
   HLSL acoplado ao pipeline da Unity; e, se a ASTRA for distribuída para
   terceiros criarem jogos, embutir o fonte dele cruza a linha de
   redistribuição de asset da §1.4. As duas razões apontam para o mesmo
   caminho: ler de perto, reescrever para o frame graph desta engine.
5. **Diversidade de ondas custa memória, não só ms.** Seis sistemas somados
   significam texturas dinâmicas, mapas de fluxo e malhas de arrebentação
   residentes. O orçamento de VRAM precisa entrar na Fase 0 junto com o de ms.
