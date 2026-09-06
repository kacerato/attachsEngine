# Plano global de água — física, gráficos e configuração

- **Estado:** proposto. Nada aqui está entregue.
- **Data:** 05/09/2026
- **Escopo:** água como sistema global da engine — simulação, física de corpos,
  renderização, autoria e diagnóstico.
- **Complementa:** `PLANO-AGUA-CEU-E-ATMOSFERA.md` (fases A0–A7 e C0–C2 de
  gráficos/atmosfera continuam válidas e são referenciadas por sigla).
  Este documento **acrescenta a trilha de física**, que aquele plano não cobria,
  e reescreve as trilhas de gráficos e configuração contra o estado medido de
  05/09/2026 — que mudou desde 04/09.
- **Depende de:** ADR-014 (política global), ADR-016 (compute no RHI),
  ADR-017 (água nativa), `CONVENCOES.md`.

---

## 0. Linha de excelência

Critérios inegociáveis. Toda etapa deste plano é reprovada por qualquer um deles,
independentemente de estar "funcionando na tela".

| # | Critério | Como se verifica |
| --- | --- | --- |
| E1 | **Nenhum eixo sem consumidor.** Propriedade serializada, slider ou campo de struct que nenhum código lê não conta como entregue. | `grep` do identificador no fim de cada etapa, registrado no relatório da etapa. |
| E2 | **Nenhuma decisão por nome.** Nem de cena, nem de material, nem de perfil, dentro de shader ou runtime. Comportamento vem de capability, tier e perfil versionado. | Revisão de diff: qualquer comparação de string em caminho de render/física reprova. |
| E3 | **Quatro estados distintos e visíveis:** solicitado, resolvido, indisponível, fallback nomeado. Nunca "silenciosamente desligado". | Log estruturado + campo no HUD + teste que força cada estado. |
| E4 | **Determinismo.** Dado (posição, tempo, perfil, seed), a consulta de água devolve o mesmo valor em qualquer thread, qualquer frame rate, CPU e GPU. | Teste de replay: 30/60/120 Hz produzem a mesma trajetória de corpo dentro de tolerância declarada. |
| E5 | **Zero alocação no caminho de frame.** Vale para C++ e C#. | `Assert.NoAlloc` no managed; contadores de alocador no nativo. |
| E6 | **Medição só vale com estado de clock declarado.** GameTurbo, Game Mode e estado térmico invalidam a rodada. Comparação A/B exige as duas pontas no mesmo estado. | Toda tabela de medição neste plano carrega PID, epoch, window, escala de render e pressão térmica. |
| E7 | **Escala de render é parte do resultado.** Nenhum número de FPS é publicado sem a escala efetiva ao lado. 120 FPS a 0,5 de escala não é 120 FPS. | Campo obrigatório em qualquer evidência. |
| E8 | **Física e gráficos leem a mesma superfície.** O que o corpo sente e o que o pixel mostra vêm do mesmo campo, com erro declarado em centímetros. | Porta de paridade (seção 8, P10). |
| E9 | **Degradação é escalonada e nomeada.** Falta de orçamento reduz qualidade distante/opcional antes da qualidade próxima, e o nível resultante aparece no HUD. | Teste de tier forçado + captura por tier. |
| E10 | **Fronteira nativa em lote.** Orçamento de 200 chamadas nativas por frame vale também para água e empuxo. Nenhuma API por corpo, por ponto ou por partícula. | Contador de chamadas na fronteira, com teto no teste. |

---

## 1. Estado medido em 05/09/2026

Verificado no código e nas capturas do repositório, não inferido.

### 1.1 O que existe e funciona

| Item | Onde | Situação |
| --- | --- | --- |
| Provedor analítico (Gerstner, ≤8 ondas) | `native/renderer/water_surface.cpp` | Produção. É o caminho padrão de lançamento. |
| Referência FFT em CPU, 8 canais + jacobiano | `native/renderer/water_fft.cpp` | Oráculo de teste. 8 transformadas inversas por update; explicitamente **não** é fallback móvel. |
| Espectro TMA/JONSWAP | `native/renderer/water_spectrum.cpp` | Gera h₀ com vento, fetch, profundidade, swell, spread. |
| Provedor espectral em compute Vulkan | `native/rhi/water_spectral_compute.cpp` + 4 shaders | Ativo por `aether.water_fft=true`, padrão declarativo dos templates Ocean Lab/Boat On Water e fallback analítico por capability. Evolução → IFFT → espuma → empacotamento de inclinação. |
| Cascatas | `native/renderer/water_cascades.cpp` | 3 por padrão, teto de 4. Validação de sobreposição e de orçamento de buffer. |
| Espuma persistente por jacobiano | `water_foam_update.comp` + `water_foam.h` | Solução exata da EDO, independente da taxa de update. Regressão CPU compara 30/60/120 updates. |
| Inclinação com filtragem de hardware | imagem RGBA32F por cascata, bindings 11–14 | Substituiu a leitura de storage buffer no fragmento. |
| Grade camera-relative | material `MapMaterialWaterCameraGrid` | Draw único preservado pelo particionador. 149.504 triângulos, sem upload por frame. |
| Bounds conservadores por desigualdade triangular | `instanced_renderer_water.cpp` | Cobre todo o envelope de controles ao vivo (ganho ≤3, choppiness ≤2). |
| Controles ao vivo | `android_runtime_controls.h` | Snapshot coerente; contrato JNI posicional chegou a 49 eixos e deve migrar para bloco versionado (§6). |

### 1.2 O que **não** existe

Esta lista registra o diagnóstico de abertura do plano. O estado executado mais
recente está nas trilhas F/G abaixo e em `water-world-runtime.md`.

1. **Física de água: zero.** `native/physics/jolt_bridge.h` não expõe empuxo,
   arrasto hidrodinâmico, acoplamento onda→corpo ou corpo→onda. A única
   ocorrência de "buoyancy" no bridge é um comentário dizendo que ficou de fora.
2. **Consulta de água pela física: inexistente.** `sampleWaterSurface` não tem
   consumidor de runtime; o toque intersecta o plano y=0.
3. **Provedor de altura para gameplay:** nem readback assíncrono, nem espelho
   de CPU. A física não tem de onde ler a superfície do provedor FFT.
4. **Solver de ondas dinâmicas, fluidos 2D, flowmap, esteira:** nada.
5. **Refração, SSR, planar, subaquático, cáusticas, costa:** nada.
6. **Mips e variância de inclinação** nas texturas espectrais: não implementados;
   o fade por pegada de pixel é a única defesa contra cintilação.
7. **Multi-água:** a UBO global de ambiente carrega um conjunto de parâmetros.
   Não há ownership por superfície, então oceano + lago + rio na mesma cena não
   é suportado, apesar de `WaterDomain` já ter três valores.
8. **`WaterProfile` unificado:** nativo v1 (struct, `WaterProfileMagic` declarado
   e nunca usado) e managed v2 (JSON) são esquemas diferentes.
9. **Serialização de cascata/espectro:** os controles ao vivo não persistem.

### 1.3 Medições que condicionam o plano

| Captura | Resultado | Leitura honesta |
| --- | --- | --- |
| PID 25261, epoch 4, window 3, 600 frames | 111,44 FPS; GPU média 7,78 ms / p95 8,32 ms; simulação 0,92 ms; raster 4,53 ms; pós 2,33 ms; pressão térmica nenhuma | **Escala de render 0,5.** Não é evidência de qualidade a 120 Hz. |
| PID 31004 (primeiro detalhe no fragmento) | 76,31 FPS; GPU 11,61 ms; raster 7,23 ms | Reprovou o orçamento; motivou a troca por textura de inclinação. |
| PID 17069, schema 8 | simulação média 0,9191 ms / p95 0,9806 ms | O custo de simulação está dentro da alocação de 0,90 ms **por pouco**, com 3 cascatas 128². |
| Captura visual do oceano | pálido, pouco detalhe próximo, faixa no horizonte | Qualidade não aceita. Lançamento padrão continua analítico. |

**Consequência para o plano:** a etapa G0 (fechar o orçamento em escala nativa)
é bloqueante. Enquanto a evidência for a 0,5, nenhuma outra etapa gráfica pode
declarar sucesso, porque metade do custo de fragmento está escondida.

### 1.4 Dívida bloqueante de lifecycle

FORTIFY `pthread_mutex_lock called on a destroyed mutex` reproduz ao trocar de
cena no mesmo processo (09:17:28 via `recreate()` e 09:24:14 em saída normal).
Sem stack completa não há dono identificado. Isso impede validar troca oceano ↔
floresta e, portanto, impede validar água integrada a uma cena com vegetação.
**É pré-requisito da etapa G8 e da porta P8**, e entra como item L1 na ordem de
execução — não como nota de rodapé.

---

## 2. Referências e o que cada uma contribui

| Fonte | Licença / propriedade | O que contribui | O que **não** entra |
| --- | --- | --- | --- |
| GodotOceanWaves (2Retr0; fork krautdev) | MIT, autor Ethan Truong, 2024 | Modelo TMA com spread misto flat/Hasselmann, cascatas com fase deslocada, FFT Stockham com transposição, espuma por jacobiano com crescimento linear e decaimento exponencial, shading GGX no estilo da palestra *Atlas*, filtragem bicúbica/bilinear por densidade de pixel, clipmap de 8 km, `get_height()` no fork. | Código transplantado. A cópia de estudo permanece fora da árvore de build. |
| KWS 1.4.03 (`Downloads/extracted/extracted`) | propriedade declarada pelo usuário em 04/09/2026 | **Amplitude funcional**: 3 cascatas com domínios distintos, quadtree com histerese, ondas dinâmicas, fluidos 2D, flowmap, empuxo por voxels, costa com perfis assados, cáusticas por ortho-depth, subaquático, volumétrica, SSR/planar/cubemap, ~90 propriedades de autoria. | Scripts UnityEngine. Não carregam na Aether e não serão portados linha a linha. |
| Vídeo "Create Stunning Water in Unity 3D with KWS Water System" | — | Confirma o alvo visual e o conjunto de recursos que o usuário quer atingir. É a mesma tecnologia do pacote local. | Não foi assistido por mim; não uso o vídeo como fonte técnica. Toda afirmação técnica sobre KWS neste plano vem do código local inspecionado. |

**Onde este plano vai além das duas referências**, com motivo:

| Ponto | Referências | Aether | Motivo |
| --- | --- | --- | --- |
| Fonte de altura para a física | KWS: readback GPU assíncrono com validade de 10 frames. Godot: `get_height()` no fork, sem física. | **Espelho espectral em CPU de baixa resolução**, avaliado no job system, mesmo h₀ e mesmo tempo do GPU. Readback vira ferramenta de validação, não dependência. | Elimina latência, sobrevive a frame drop, é determinístico e replicável em rede. Readback é assíncrono por natureza; física determinística não pode depender disso. |
| Empuxo | KWS: voxels do bounding box, `sqrt(k)` de profundidade, damping fixo. | **Volume submerso analítico** para caixa/esfera/cápsula/casco convexo, com centróide correto, massa adicionada e arrasto quadrático relativo à **velocidade orbital da água**. | Voxels erram o calado e o metacentro; sem velocidade relativa o barco não balança com o swell. |
| Espuma | limiar sobre onda (KWS) / jacobiano por cascata (Godot) | Jacobiano por cascata **advectado pelo campo de fluxo** e com mip próprio. | Espuma parada em espaço de parâmetro desliza junto com a onda em vez de escorrer da crista. |
| Cintilação | mips da normal | mips **+ variância de inclinação dobrada na rugosidade** (LEAN/Toksvig). | Frequência abaixo do pixel vira rugosidade especular em vez de ruído. É a diferença entre "borrar" e "resolver". |
| Autoria | inspetor Unity com 90 campos fixos | **Tabela de descritores versionada** que gera painel, Inspector e NoCode a partir de uma fonte só. | Acrescentar eixo não pode custar mudança em Java, C# e C++ ao mesmo tempo. |

---

## 3. Arquitetura alvo

Cinco camadas, com fronteira dura. A novidade em relação ao plano anterior é a
camada **CAMPO**, que é o que permite física e gráficos concordarem.

```
AUTORIA — backend-neutral, versionada, serializável
  WaterProfile v3 ......... espectro, óptica, espuma, malha, costa, interação
  WaterBodyProfile v1 ..... densidade, arrasto, massa adicionada, amostragem
  WaterZone[] ............. batimetria, corrente, exclusão, temperatura
  WaterPreset[] ........... dados versionados, não constantes de código
        |
RESOLUÇÃO — capabilities + orçamento + térmica  (ADR-014)
  ResolvedWaterPipeline ... cascatas ativas, resolução, cadência por passe,
                            reflexão, refração, interação, fallback nomeado
        |
CAMPO — a superfície como função, única para todo consumidor
  WaterField .............. sample(span<posição>, tempo) -> span<amostra>
    provedores: Analytic | SpectralCpu | SpectralGpu(+validação por readback)
    consumidores: física, gameplay, áudio, partículas, câmera, IA, render
        |
        +----------------------------+----------------------------+
        |                            |                            |
FÍSICA (CPU, job system)      GRÁFICOS (Vulkan)          GAMEPLAY (C#, lote)
  empuxo, arrasto,              compute: espectro,          consulta de altura,
  hidrodinâmica de casco,       IFFT, espuma, ondas         impulsos, registro
  personagem, splats            dinâmicas, fluidos,         de corpo flutuante
  corpo->água                   cáusticas
                                gráfico: clipmap ->
                                subpass opacos -> subpass
                                água -> pós
```

Regras de fronteira:

- Nenhum tipo Vulkan sobe acima da camada de gráficos.
- Nenhum tipo Jolt sobe acima da camada de física.
- `WaterField` não conhece nem GPU nem corpo rígido. É a única coisa que os dois
  lados compartilham, e é onde a porta de paridade P10 age.
- A camada de autoria não conhece nenhuma das outras.

---

## 4. Trilha F — Física

O maior salto deste plano. Hoje a física de água é zero.

---

### F0 — Contrato de campo (`WaterField`)

**Entrega:** interface única de consulta, batelada, determinística, sem alocação.

#### F0.1 — Tipos e assinatura

`native/renderer/water_field.h` (novo):

```cpp
struct WaterFieldSample final {
  float height = 0;             // metros, mundo
  WaterVec3 normal{0,1,0};
  WaterVec3 velocity{};         // velocidade orbital da partícula de água
  WaterVec2 flow{};             // corrente/flowmap, m/s
  float foam = 0;               // 0..1
  float jacobian = 1;           // <1 comprimindo, <0 dobrando
  float depth = 0;              // batimetria: fundo até superfície em repouso
  u32 flags = 0;                // WaterFieldFlag: Valid, Excluded, Extrapolated
};

enum class WaterFieldProvider : u32 { Analytic, SpectralCpu, SpectralGpu };

struct WaterFieldStatus final {
  WaterFieldProvider requested, resolved;   // E3: os quatro estados
  u32 cascadesActive, cascadeResolution;
  double simulationTime;
  u32 ageFrames;                            // 0 no espelho de CPU
  const char *fallbackReason;               // nulo quando resolved==requested
};

class WaterField final {
public:
  void sample(std::span<const WaterVec2> positions, double timeSeconds,
              std::span<WaterFieldSample> out) const noexcept;
  float heightOnly(WaterVec2 position, double timeSeconds) const noexcept;
  WaterFieldStatus status() const noexcept;
};
```

- **Batelada obrigatória (E10).** Não existe versão por ponto exposta na
  fronteira C#/C++; `heightOnly` é conveniência interna do C++.
- `out.size() < positions.size()` é erro de contrato, não truncamento silencioso.
- Sem estado de frame: `sample` é `const` e reentrante.

#### F0.2 — Provedor analítico

Reaproveita `sampleWaterSurface` + `WaterInteractionField` + `waterCoverage`.
Isso finalmente dá consumidor de runtime a três APIs que hoje não têm (dívida
1.3.4 do plano anterior). `waterCoverage` passa a preencher `flags::Excluded`.

#### F0.3 — Provedor espectral em CPU

Ver F1. É o provedor padrão para física assim que existir.

#### F0.4 — Provedor espectral em GPU

Readback opcional, com `ageFrames` real e `flags::Extrapolated` quando a amostra
é mais velha que o limite. **Nunca** é o provedor padrão de física.

#### F0.5 — Testes

- Determinismo: mesma posição/tempo em 4 threads → resultado idêntico bit a bit.
- Contrato de span: tamanhos incompatíveis reprovam sem escrever.
- `Excluded` respeita feathering analítico e é estável sob movimento.
- Benchmark: 10.000 posições em uma chamada, sem alocação, tempo registrado.

**Orçamento:** ≤ 0,10 ms CPU para 4.096 consultas no provedor analítico.
**Gate:** E1, E4, E5, E10 verificados; nenhum consumidor ainda é exigido.

---

### F1 — Espelho espectral em CPU (fonte de verdade da física)

**Estado implementado em 2026-09-06:** `WaterSpectralMirrorSet` recebe as
cascatas validadas do renderer, cria espelhos adaptativos 32²/32²/128² no perfil
padrão, soma as bandas antes da inversão horizontal e fornece altura, inclinação,
deslocamento e velocidade orbital ao `WaterField`. O hook `beforeStep` o avalia
no relógio fixo da física; o renderer recebe a mesma origem temporal. Não há
readback. O teste Android confirmou poses móveis e inclinação do barco. A porta
formal P10 de paridade GPU/CPU em centímetros e a migração para job dedicado
continuam abertas.

**Decisão de arquitetura, e é a mais importante da trilha:** a física **não**
espera readback da GPU. Ela avalia o mesmo espectro numa resolução menor, no job
system, com o mesmo `h₀`, a mesma dispersão e o mesmo relógio de simulação.

Por quê: readback é assíncrono e tem idade variável. Um empuxo que lê uma altura
de 2–3 frames atrás oscila e nunca fica determinístico sob queda de FPS — e E4
não admite isso. Truncar o espectro é um erro **limitado e mensurável**; latência
não é.

#### F1.1 — Truncamento e erro

- A política atual usa todas as cascatas, cada uma na menor potência de dois que
  preserva ao menos duas amostras por menor comprimento de onda, com teto 128.
  Esse teto e a densidade de amostragem são configuração do conjunto.
- As cascatas curtas contribuem ao campo combinado, inclusive inclinação. Um
  futuro filtro por footprint de casco pode retirar energia imperceptível para
  corpos grandes sem reduzir o detalhe visual próximo.
- **O erro precisa ser medido, não assumido:** teste compara altura truncada
  contra a referência completa `WaterSpectralField` em 10.000 amostras e publica
  RMS e máximo em centímetros. Se o RMS passar de 5 cm no perfil "mar aberto", a
  resolução sobe antes de a etapa fechar.

#### F1.2 — Agendamento

- O owner do laboratório atualiza as cascatas em sequência no hook `beforeStep`,
  imediatamente antes das consultas/forças no timestep fixo. Job por cascata é
  otimização futura, não requisito funcional já entregue.
- Reuso de plano FFT e buffers alocados na inicialização; zero alocação por
  update (E5).
- Cadência configurável (`physics.spectrumHz`, padrão 30 Hz) com interpolação
  **no tempo do espectro**, não interpolação de resultado: como cada cascata é
  função fechada do tempo, reavaliar a 30 Hz é exato, só menos frequente.

#### F1.3 — Canais necessários à física

Altura, deslocamento horizontal X/Z, e derivada temporal para velocidade orbital.
A velocidade orbital sai analiticamente de ∂h/∂t = Σ −ω·(parte imaginária), sem
diferença finita entre frames — diferença finita quebraria E4 sob frame variável.

#### F1.4 — Validação contra a GPU

Readback de diagnóstico compara espelho CPU × saída GPU no mesmo tempo de
simulação. **Porta P10:** concordância dentro de **1 cm** para a cascata longa.
Divergência maior é bug de fase, de ordem de FFT ou de relógio — e é exatamente
o tipo de erro que passa despercebido sem esta porta.

**Orçamento:** ≤ 0,35 ms CPU por update a 30 Hz num núcleo grande, fora da thread
principal. **Gate:** P10 passa; RMS de truncamento publicado; determinismo em
30/60/120 Hz.

---

### F2 — Empuxo

**Entrega:** corpos flutuam com calado correto, estáveis, sem jitter, sem custo
por corpo na fronteira.

#### F2.1 — Volume submerso analítico

Para cada corpo, ajusta-se um **plano local de água** a partir de altura e normal
no centro do corpo (uma consulta `WaterField`, não uma malha).

- **Caixa, esfera, cápsula, cilindro:** volume submerso e centróide em forma
  fechada contra um plano arbitrário. Sem voxels, sem amostragem.
- **Casco convexo:** recorte do hull pelo plano, volume por decomposição em
  tetraedros a partir de um ponto interno. O:(faces), estável, exato.
- **Malha côncava:** voxelização **offline**, gravada no asset. Nunca raycast em
  runtime como faz a referência — aquilo é O(voxels × 6 raios) por ativação.

#### F2.2 — Ondulação sob o corpo

Um plano só é suficiente para corpo pequeno em relação ao comprimento de onda.
Para corpo grande (`bodyLength > waveLength/4`), o plano vira **um conjunto de
planos por seção**: o corpo é fatiado ao longo do eixo maior em
`sectionCount` (2–8, autorado), cada seção com seu plano ajustado. Isso é o que
faz um barco longo cavalgar a onda em vez de deslizar num plano inclinado.

#### F2.3 — Forças

Por seção submersa:

| Força | Fórmula | Papel |
| --- | --- | --- |
| Empuxo | `ρ_água · g · V_sub` no centróide submerso | sustentação e torque restaurador |
| Arrasto quadrático | `½ ρ C_d A |v_rel| v_rel`, separado em componente normal e tangencial | resistência realista, com `A` = área projetada da seção |
| Arrasto angular | `−C_ω · ω · |ω|` | amortece rolagem sem congelar |
| Massa adicionada | `m_a = C_a ρ V_sub`, aplicada como impulso proporcional à aceleração relativa | o que faz o corpo "sentir peso" ao acelerar dentro d'água |
| Impacto (slamming) | proporcional a `d(V_sub)/dt` positivo | entrada na água tem pancada, não afundamento suave |

**`v_rel = v_corpo − (v_orbital + v_flow)`.** Sem isso, corpo parado em swell não
balança e barco em correnteza não é levado. É o item que separa "flutua" de
"navega".

#### F2.4 — Estabilidade

- Substep interno se `dt` exceder o limite de estabilidade do empuxo.
- Clamp de força por seção em múltiplo do peso do corpo (padrão 8×), com contador
  de saturação exposto no diagnóstico — clamp silencioso esconde erro de tuning.
- **Sleeping:** corpo em repouso na superfície precisa dormir. A força de empuxo
  em equilíbrio deve cair abaixo do limiar do Jolt; teste dedicado, porque água
  que não deixa corpo dormir é dreno de bateria contínuo em mobile.

#### F2.5 — Dois caminhos, mesma configuração

| Caminho | Mecanismo | Tier | Custo |
| --- | --- | --- | --- |
| A — simples | `JPH::Body::ApplyBuoyancyImpulse` com um plano ajustado | C/B | ~1 consulta + 1 chamada por corpo |
| B — seções | acumulação própria de forças por seção (F2.2/F2.3) | A/S | `sectionCount` consultas por corpo |

O componente autorado é o mesmo; a resolução escolhe o caminho e o HUD mostra
qual foi resolvido (E3). Não existe "caminho B degradado silenciosamente para A".

#### F2.6 — Fronteira

```cpp
// Uma chamada por passo de física para todos os corpos flutuantes.
ae::i32 AetherPhysics_ApplyWaterForces(AetherPhysicsWorld *world,
    const AetherWaterBodyBatch *batch, float deltaTime,
    AetherWaterForceStats *outStats);
```

`AetherWaterBodyBatch` é blittable: span de handles, span de índices de perfil,
span de amostras já colhidas do `WaterField`. **Zero string, zero por-corpo.**

#### F2.7 — Testes

- **Calado teórico:** cubo de densidade 500 kg/m³ estabiliza com metade submersa,
  erro < 2%.
- **Metacentro:** caixa achatada inclinada retorna à horizontal; caixa alta
  emborca. Se as duas se comportam igual, o centróide está errado.
- **Sem oscilação perpétua:** amplitude decai monotonicamente em água parada.
- **Determinismo (E4):** mesma trajetória a 30/60/120 Hz dentro de 1 cm ao fim de
  10 s simulados.
- **Sleep:** corpo estabilizado dorme em ≤ 3 s e não acorda sozinho.
- **Alocação:** `Assert.NoAlloc` no caminho managed; contador de alocador zerado
  no nativo.

**Orçamento:** ≤ 0,25 ms CPU para 64 corpos flutuantes no caminho B.
**Gate:** todos os testes acima + E10 (uma chamada de fronteira por passo).

---

### F3 — Hidrodinâmica de casco

**Entrega:** embarcações que se comportam como embarcações. Opt-in, tier A/S.

#### F3.1 — Malha hidrodinâmica dedicada

Casco simplificado (≤ 256 triângulos), autorado ou decimado offline, separado da
malha de render e da de colisão. Orçamento de triângulos submersos por corpo é
explícito (`hull.maxSubmergedTriangles`).

#### F3.2 — Recorte pela linha d'água

Cada triângulo é classificado contra a altura de água nos seus três vértices e
recortado em 0, 1 ou 2 triângulos submersos. É o algoritmo clássico de Kerner —
barato, estável e correto na fronteira.

#### F3.3 — Forças por triângulo

- **Hidrostática:** `ρ g h · A · n̂`, com `h` a profundidade do centro.
- **Hidrodinâmica:** arrasto e sustentação em função do ângulo entre `n̂` e
  `v_rel`, com coeficientes autorados por perfil.
- **Slamming:** limitado pela taxa de variação de área submersa, com teto.
- **Resistência viscosa:** placa plana equivalente, `C_f` por número de Reynolds.

#### F3.4 — Propulsão e governo

Leme e hélice como motores de junta — a API de juntas e motores já existe
(`AetherPhysics_SetJointMotor`). Propulsor aplica empuxo no ponto do casco;
leme aplica força lateral proporcional a `v²·sin(δ)`.

#### F3.5 — Testes

- Resistência em linha reta cresce ~quadraticamente com a velocidade.
- Círculo de giro estável e repetível.
- Sem explosão numérica a velocidade alta com `dt` grande (substep obrigatório).
- Casco a 30/60/120 Hz converge para a mesma trajetória dentro da tolerância.

**Orçamento:** ≤ 0,40 ms CPU por embarcação ativa, teto de 4 simultâneas no tier
S. **Gate:** os quatro testes + orçamento medido no aparelho.

---

### F4 — Acoplamento corpo → água

**Entrega:** o corpo deixa rastro. Hoje a água ignora completamente os corpos.

#### F4.1 — Fonte unificada de perturbação

Um só tipo, `WaterSplat`, consumido por três destinos: ondas dinâmicas (F5),
emissor de spray (F8) e áudio.

```cpp
struct WaterSplat final {
  WaterVec2 position; float radius, strength, verticalVelocity; u32 kind;
};
```

Gerado a partir de `d(V_sub)/dt` e da velocidade do corpo — não de um evento de
colisão. Corpo entrando gera splat forte; corpo deslizando gera esteira contínua.

#### F4.2 — Orçamento e ring buffer

Teto de splats por frame (`interaction.maxSplats`, padrão 64), ring buffer
pré-alocado, descarte por menor energia quando estoura — **com contador visível**,
nunca descarte silencioso.

#### F4.3 — Esteira de Kelvin

Para corpos acima de um limiar de velocidade e comprimento, a cunha de 19,47° é
analítica: uma perturbação paramétrica somada ao campo, sem custo de simulação.
É o detalhe barato que faz um barco parecer um barco.

#### F4.4 — Testes

- Dois splats próximos produzem interferência visível e simétrica.
- Estouro de orçamento descarta o de menor energia e incrementa o contador.
- Determinismo: mesma sequência de splats → mesmo campo.

**Orçamento:** geração ≤ 0,05 ms CPU. **Gate:** interferência comprovada em
captura; contador de descarte exposto.

---

### F5 — Solver de ondas dinâmicas

**Entrega:** ondas locais que refletem em obstáculos e interferem entre si — o
que 8 impulsos analíticos jamais produzem.

#### F5.1 — Equação e discretização

Equação de onda 2D com amortecimento, em textura centrada na área de interesse.
Compute, dois buffers de estado (`u_n`, `u_{n-1}`), ping-pong.

- **Condição CFL imposta em código:** `c·dt/dx ≤ 1/√2`. Se a configuração do
  usuário violar, a resolução é ajustada e o ajuste é **registrado**, não aceito
  em silêncio (E3).
- Timestep fixo com acumulador; render a 120 Hz e simulação a 60 Hz é normal.

#### F5.2 — Rolagem da área

Ao mover, a textura é deslocada por **número inteiro de texels** e só a faixa
nova é inicializada. Deslocamento fracionário introduz difusão numérica que se
manifesta como perda de energia ao andar — bug clássico e difícil de diagnosticar
depois.

#### F5.3 — Máscara de obstáculo

Alimentada por um passe ortográfico de profundidade da cena. Obstáculo vira
condição de contorno reflexiva; a borda da área vira contorno absorvente, para
que a onda não volte da borda como de uma parede.

#### F5.4 — Acesso pela física

Ondas dinâmicas vivem na GPU. Para a física, duas opções, e a escolha é explícita
por tier:

- **Tier A/S:** readback da textura em baixa resolução, com `ageFrames` real e
  `flags::Extrapolated`. Afeta empuxo.
- **Tier B/C:** visual apenas. `WaterField` não reporta ondas dinâmicas, e o HUD
  diz isso.

Não existe meio-termo em que a física às vezes vê e às vezes não.

#### F5.5 — Testes

- CFL respeitado em toda configuração autorável.
- Energia decai monotonicamente sem fonte.
- Interferência de dois impulsos é simétrica.
- Rolagem de área não perde energia (teste: impulso, andar 100 m, medir).

**Orçamento:** 128² a 30 Hz ≤ 0,15 ms GPU. **Gate:** os quatro testes + captura.

---

### F6 — Fluxo, correnteza e fluidos 2D

**Entrega:** a água tem direção, e essa direção é a mesma para o pixel e para o
corpo.

#### F6.1 — Flowmap autorado

Campo vetorial serializado no `WaterProfile` v3, com área e escala. Consumidores:
`WaterFieldSample::flow` (física), advecção de UV de detalhe e espuma (render),
transporte de partículas (F8).

**E8 aplicado:** se a espuma escorre numa direção e o barco é levado noutra, a
etapa está reprovada.

#### F6.2 — Rios por spline

Spline → malha + campo de fluxo derivado da tangente e da largura. `WaterDomain::
RiverSpline` já existe no enum e ganha implementação aqui.

#### F6.3 — Fluidos 2D (tier S, opt-in)

Advecção de velocidade com obstáculos, para redemoinho ao redor de corpos. É o
mais caro e o menos essencial: entra por último e só entra em `Auto` se sobrar
orçamento medido.

**Orçamento:** flowmap ≈ 0 (leitura de textura); fluidos ≤ 0,30 ms, opt-in.
**Gate:** coerência física/visual comprovada em captura com objeto boiando à deriva.

---

### F7 — Personagem na água

**Entrega:** `character_motor` reconhece água. Hoje não reconhece.

- **Estados:** seco → vadeando (`depth < altura do quadril`) → nadando.
  Transição com histerese, para não alternar na crista da onda.
- Vadeando: velocidade reduzida por profundidade, câmera com balanço.
- Nadando: empuxo simplificado + arrasto, sem corpo rígido completo, com a
  cabeça acompanhando a superfície local.
- Saída da água: passo de subida usa a batimetria, não um raycast para y=0.
- **Testes:** entrar e sair não teleporta; nadar em mar agitado mantém a cabeça
  acima da superfície local; transição estável com onda passando.

**Orçamento:** ≤ 0,05 ms. **Gate:** rota gravada de entrada/saída sem snap.

---

### F8 — Chuva, impactos e spray

- **Chuva:** campo de impulsos poissonianos com `rain.strength` e `rain.radius`,
  alimentando F5. Determinístico por seed + tempo (E4).
- **Impactos:** eventos de trigger já existem (`AetherPhysics_GetTriggerEvents`);
  travessia da superfície gera splat + som + partícula.
- **Spray:** emissor GPU alimentado pelas regiões de espuma alta **e** por splats,
  não distribuído uniformemente pela superfície. Orçamento fixo de partículas,
  com prioridade por energia.

**Orçamento:** spray ≤ 0,20 ms GPU, opt-in. **Gate:** partículas concentradas
onde há espuma/impacto, comprovado por captura com a vista de diagnóstico.

---

### F9 — Tempo, determinismo e rede

- **Relógio único:** `WaterSpectralClock` vira `WaterClock`, com epoch explícito,
  serializável e restaurável. Nada em água lê relógio de parede.
- **Passo fixo** para toda a física de água; render interpola, simulação não.
- **Teste de replay:** gravar entradas, reproduzir, exigir trajetória idêntica
  dentro da tolerância declarada.
- **`network.timeSource`:** eixo que permite ancorar a simulação a um tempo
  autoritativo. Entra como eixo com consumidor real (o relógio), não como campo
  reservado para o futuro — E1 vale aqui também.

**Gate:** replay determinístico em 3 taxas de frame; troca de time source sem
descontinuidade visível.

---

## 5. Trilha G — Gráficos

Ordem escolhida por dependência de medição, não por impacto visual.

---

### G0 — Fechar o orçamento em escala nativa *(bloqueante)*

Nenhuma outra etapa gráfica pode declarar sucesso enquanto a evidência for a 0,5
de escala (E7).

- **G0.1 — Zero medido do oceano a escala 1,0.** Rota AERT fechada: horizonte
  raso, mergulho, sobrevoo rápido, pose parada. p50/p95/p99 por passe.
- **G0.2 — Atribuição correta.** O schema 8 já isolou `gpu_water_simulation_ms`.
  Falta separar o raster da água do agregado `Opaque` — hoje o subpass tile-
  deferred colapsa os timestamps e a água aparece custando ~zero, o que é falso.
  Sem isso, otimizar água é adivinhação.
- **G0.3 — Custo de fragmento isolado.** Medir com a água desligada e ligada na
  mesma pose, mesmo estado de clock (E6).
- **G0.4 — Decidir a escala alvo.** Se 1,0 não couber em 8,33 ms, o alvo passa a
  ser 60 Hz a 1,0 **declarado como tal**, não 120 Hz a 0,5 apresentado como 120 Hz.

**Gate:** tabela de zero publicada com escala, estado térmico e PID/epoch/window.

---

### G1 — Espectro de nível superior

- **G1.1 — Domínios não comensuráveis.** Trocar 32/128/2048 m por
  **19,7 / 51,3 / 163,1 m**. Razões inteiras realinham as cascatas e recriam
  repetição visível; domínios mutuamente irracionais não repetem no alcance útil.
- **G1.2 — Resolução por tier.** S: 256²/128²/128². A: 256²/128²/128², cadência
  menor. B: 128²/64². C: analítico. **Limite conhecido:** o butterfly atual usa
  `shared vec4[256]`, então N ≤ 256. Passar disso exige mudar o kernel — está
  documentado aqui para não virar surpresa.
- **G1.3 — Mips + variância de inclinação.** Cadeia de mips na textura de
  inclinação e um canal `SlopeVariance` por mip, dobrado na rugosidade especular
  (LEAN/Toksvig). **É a correção estrutural da cintilação**, e hoje só existe o
  fade por pegada de pixel como paliativo.
- **G1.4 — Deslocamento horizontal completo** no vértice, com a correção de
  normal pelas derivadas horizontais (já calculadas: XX/XZ/ZZ).
- **G1.5 — Reconfiguração ao vivo do espectro** sem recriar device: vento, fetch,
  profundidade, swell, spread, segundo trem, deslocamento e choppiness por
  cascata. **Implementado para propriedades que não mudam a estrutura alocada:**
  os modos GPU são atualizados ao soltar o slider e o espelho físico adota a
  mesma revisão. Persistência e edição ao vivo de domínio/resolução continuam
  pendentes porque exigem reconstrução estrutural, não apenas novos modos.
- **G1.6 — Espuma advectada** pelo campo de fluxo e com mip próprio.
- **G1.7 — Fila assíncrona.** Hoje a evolução e as duas IFFT são gravadas na fila
  gráfica. Migrar para compute assíncrona onde houver família dedicada; onde não
  houver, cair para 30 Hz na fila gráfica e **rebaixar o tier**, com log.

**Orçamento:** 0,90 ms GPU para toda a simulação (medido hoje em 0,92 ms com 3×128²
na fila gráfica — subir para 256² **exige** a fila assíncrona ou cadência menor).
**Degradação escalonada:** C2 sai → C1 para 64² → C0 para 128². Nunca reduzir
qualidade próxima primeiro.
**Gate:** sweep longo em Adreno e Mali sem cintilação; P10 (paridade com o
espelho de CPU) dentro de 1 cm.

---

### G2 — Malha: clipmap instanciado

Mantém-se o que o plano anterior definiu em A2, agora com a nota de que a grade
atual é de 149.504 triângulos (não 131.072).

- Patch de 33×33 vértices (1.089 vértices em buffer), instanciado até 88 vezes.
- Célula próxima de 0,50 m, alcance de 8.192 m, ~180.224 triângulos, **um draw
  instanciado**.
- Costura 2:1 por `WaterPatch::skirtDepth` (já no contrato).
- Histerese: reconstruir ao andar 3 m à frente, 0,5 m atrás ou girar 1°.
- `planWaterClipmap` ganha seu primeiro consumidor de runtime (dívida 1.3.4).
- O particionador precisa preservar o draw instanciado como já preserva a grade.

**Orçamento:** vértice ≤ 0,45 ms com espectro completo.
**Gate:** rota de aproximação/afastamento sem popping, sem costura, sem nadar.

---

### G3 — Óptica com profundidade real

1. **Input attachment de cor.** A água já é o subpass 1 — a cor opaca está no
   tile. Absorção cromática verdadeira `dst · exp(−σ·d)` por canal, custo de DRAM
   zero. Mata a dívida da absorção monocromática.
2. **Refração com distorção e dispersão**, com **guarda de profundidade
   obrigatória**: pixel amostrado à frente da água cai para a amostra não
   deslocada. Sem a guarda, objeto em primeiro plano vaza para dentro d'água.
3. **Batimetria como fonte de profundidade**, não só o depth de tela.
4. **Espalhamento subsuperficial** dependente da espessura e do ângulo — o que
   dá a crista translúcida iluminada por trás. É metade do "patamar" pedido.
5. **Espuma opaca:** `compositeAlpha = max(compositeAlpha, foam)`. Correção de
   uma linha que hoje deixa espuma de costa com alpha ≈ 0,04.

**Orçamento:** input attachment ≤ 0,10 ms; refração meia-resolução ≤ 0,35 ms.
**Gate:** cena com objeto submerso; prova visual raso/fundo; sem vazamento.

---

### G4 — Reflexão em camadas

| Camada | Fonte | Tier |
| --- | --- | --- |
| Environment | LUT da atmosfera pré-computada (C1) | C |
| Cubemap | cadência e culling próprios | B |
| Screen-space | preenchimento de buracos, estiramento de borda controlado | A |
| Planar | máscara de culling e conteúdo configurável | S |

Mais: lóbulo solar com disco analítico, e reflexão anisotrópica ao longo da
direção do vento. `resolveWaterPipeline` ganha consumidor real aqui.

**Dependência dura:** a qualidade da reflexão está limitada pelo céu. Com o
panorama atual de 1024×512 subamostrado ~10×, reflexão boa não é alcançável.
**C1 (atmosfera pré-computada) é pré-requisito de G4, não um item paralelo.**

**Orçamento:** SSR ≤ 0,60 ms, opt-in, nunca em `Auto` nos tiers B/C.

---

### G5 — Espuma, costa e detalhe

- Espuma por jacobiano acumulada (existe) + **espuma de contato** por intersecção
  com geometria + **espuma de costa** com perfis de quebra assados ao longo do
  litoral.
- Textura estocástica de espuma — este é o lugar certo para textura fotográfica,
  ao contrário da normal da superfície, onde ela é a causa do padrão xadrez.
- Máscaras/buracos: `WaterExclusionVolume` ganha upload, avaliação no shader e
  picking.

---

### G6 — Subaquático e volume

- Detecção por volume com transição contínua na travessia.
- Névoa por absorção real (mesma `σ` da óptica, não uma segunda cor autorada).
- Superfície vista de baixo com reflexão interna total.
- Cáusticas por ortho-depth, com escala por profundidade.
- Luz volumétrica com resolução, iterações e raio próprios (tier S).

---

### G7 — Estabilidade temporal

Transversal, e é o que separa "screenshot bonito" de "imagem boa em movimento":

- Interação com o AA temporal existente: deslocamento espectral não pode gerar
  ghosting; vetores de movimento da água precisam incluir o deslocamento
  horizontal.
- Anisotropia até 4× no sampler de inclinação.
- Sem `textureLod` fixo no horizonte.
- **Teste de movimento longo**, não pose parada, em Adreno e Mali.

---

### G8 — Água em cena com vegetação

Integrar água à cena da floresta. **Bloqueado por L1** (o defeito de lifecycle):
sem troca de cena validada, não há como medir a combinação. Nenhuma medição do
oceano isolado é extrapolada para a floresta.

---

## 6. Trilha P — Configuração e possibilidades

"As possibilidades e configurações devem subir" é um requisito de arquitetura,
não de quantidade de sliders.

---

### P0 — `WaterProfile` v3 unificado

- Um esquema para nativo e managed. Hoje são dois (v1 struct / v2 JSON).
- `WaterProfileMagic` finalmente usado no cabeçalho binário — hoje é declarado e
  nunca lido.
- Migração explícita v1→v3 e v2→v3, com teste por versão.
- `foamDecay` → `foamStrength` (o managed já está certo).
- Blocos: `spectrum`, `mesh`, `optics`, `reflection`, `foam`, `shoreline`,
  `interaction`, `caustics`, `underwater`, `filtering`, `debug`.

### P1 — Tabela de descritores refletida

O JNI posicional, já expandido de 19 para 49 eixos durante a integração, não
escala para ~90 eixos. Substituir por:

```cpp
struct WaterParameterDescriptor final {
  u32 id; const char *name, *group, *unit;
  ParameterType type;                 // float,int,bool,enum,color,vec2,vec3
  float minimum, maximum, defaultValue, step;
  u32 tierMask;                       // em que tiers o eixo existe
  const char *requiresCapability;     // nulo quando sempre disponível
};
```

Fronteira reduzida a cinco entradas estáveis: `parameterCount`,
`parameterDescriptor(i)`, `setParameter(id,value)`, `getParameter(id)`,
`applyPreset(name)`. **O painel Java passa a ser construído a partir da tabela**:
acrescentar eixo no nativo custa zero mudança em Java. A mesma tabela alimenta
Inspector e NoCode — é a promessa da ADR-017, cumprida uma vez só.

### P2 — Presets como dados versionados

Calmaria, Brisa, Mar aberto, Tempestade, Lago, Tropical raso, Rio. Recursos
versionados, nunca constantes em código. Preset carrega o tier mínimo em que faz
sentido.

### P3 — Multi-água

Hoje há uma UBO global de ambiente: uma água por cena, de fato. Alvo:

- `WaterInstance` com perfil próprio, transformada, domínio e camada.
- Oceano infinito + lago finito + rio spline + piscina na mesma cena.
- Ownership explícito de recursos por instância; cascatas compartilhadas quando
  os espectros forem idênticos (deduplicação por hash do espectro).
- Consulta `WaterField` resolve **qual** água responde numa posição, por
  prioridade e volume, e devolve `flags::Excluded` fora de todas.

### P4 — Zonas e volumes

`WaterZone` com forma (caixa/esfera/spline) e carga: batimetria local, corrente,
modificador de espuma, exclusão, temperatura (para névoa e áudio). Avaliadas em
CPU para física e enviadas como buffer para o shader — mesma fonte, E8.

### P5 — Tiers e fallback nomeado

S/A/B/C resolvidos por capability + orçamento + térmica (ADR-014), **nunca por
cena**. Cada capability ausente produz um estado registrado (E3) e um nome de
fallback exibido no HUD. Teste que força cada tier e cada fallback.

### P6 — Diagnóstico

- Vistas: normal, inclinação, espessura, máscara de espuma, jacobiano, nível de
  cascata, wireframe do clipmap, **campo de fluxo**, **forças de empuxo**,
  **volume submerso**. As três últimas são da trilha de física e não existem em
  nenhuma das referências.
- A/B por congelamento de snapshot na mesma pose.
- Custo ao vivo por passe, lido do `gpu_frame_timer`.
- Exportar perfil como JSON colável num recurso — fecha o ciclo bancada→autoria.

### P7 — API managed de gameplay

```csharp
// Batelada, blittable, sem alocação (E5, E10).
int Water.SampleHeights(ReadOnlySpan<float2> positions, double time, Span<float> heights);
int Water.Sample(ReadOnlySpan<float2> positions, double time, Span<WaterFieldSample> samples);
void Water.AddSplat(in WaterSplat splat);
WaterBodyHandle Water.RegisterBuoyantBody(BodyHandle body, in WaterBodyProfile profile);
WaterFieldStatus Water.Status { get; }
```

`Status` expõe os quatro estados de E3 ao gameplay, para que um jogo possa
decidir sem adivinhar o que a plataforma resolveu.

### P8 — Editor no aparelho

Painel com grupos colapsáveis, leitura numérica com unidade, toque duplo no
rótulo para restaurar o padrão, chips de preset, e pincéis de interação (toque =
impulso, arrastar = esteira, dois dedos = chuva local). Área de toque de 48 dp
mantida.

---

## 7. Orçamentos

Alocações, não medições (E6). Cada uma precisa ser provada no aparelho antes de
a etapa entrar em `Auto`.

### GPU — cena oceânica, tier S, alvo de 8,333 ms com teto de GPU 7,333 ms

| Passe | Alocado | Fila | Etapa |
| --- | ---: | --- | --- |
| Espectro FFT (3 cascatas, amortizado) | 0,90 ms | async compute | G1 |
| Céu — sky-view LUT | 0,20 ms | gráfica | C1 |
| Opacos + céu + chão | 1,60 ms | gráfica | — |
| Sombras direcionais | 0,80 ms | gráfica | — |
| Vértice da água (clipmap) | 0,45 ms | gráfica | G2 |
| Fragmento da água | 1,30 ms | gráfica | G3 |
| Refração meia-resolução | 0,35 ms | gráfica | G3 |
| Ondas dinâmicas | 0,15 ms | async compute | F5 |
| Pós (AA + nitidez + bloom) | 1,33 ms | gráfica | — |
| **Soma na fila gráfica** | **6,03 ms** | | |
| **Margem** | **1,30 ms** | | |

Fora desta soma, cada um com orçamento próprio e prova de que cabe na margem:
SSR (0,60), cáusticas (0,45), volumétrica (0,50), fluidos (0,30), spray (0,20).

**Aviso de honestidade:** a medição atual de 0,92 ms de simulação foi obtida a
0,5 de escala e na fila gráfica. Subir para 256² sem fila assíncrona não cabe
neste orçamento. G0 existe para descobrir o quanto disso é real.

### CPU — por frame

| Trabalho | Alocado | Thread |
| --- | ---: | --- |
| Espelho espectral (30 Hz, amortizado) | 0,35 ms | job |
| Consulta de campo (64 corpos + gameplay) | 0,10 ms | job |
| Empuxo, 64 corpos, caminho B | 0,25 ms | física |
| Hidrodinâmica, 1 embarcação | 0,40 ms | física |
| Geração de splats | 0,05 ms | física |
| **Total de água na CPU** | **1,15 ms** | |

Teto de CPU p95 de 6,50 ms em 120 Hz: água ocupa ~18%. Se estourar, a
degradação é `sectionCount` → 1 (caminho A), depois hidrodinâmica desliga,
depois espelho cai para 32².

### Memória

| Recurso | Tier S | Tier B |
| --- | ---: | ---: |
| Cascatas GPU (68·N² por cascata, com imagens) | ~10,6 MiB (256²+2×128²) | ~1,4 MiB (128²+64²) |
| Espelho de CPU | 64² × 4 canais ≈ 256 KiB | 32² ≈ 64 KiB |
| Ondas dinâmicas (2 estados) | 128² × RG16F ≈ 128 KiB | desligado |
| LUTs de atmosfera | < 1 MiB | < 1 MiB |

O panorama 4K de 44,7 MiB é o argumento contra a foto e a favor de C1 — e agora
também a favor de G4, que depende do céu.

---

## 8. Portas de aceitação

As nove portas de `PLANO-RECONSTRUCAO-RENDERIZACAO` continuam valendo. Somam-se:

| # | Porta | Aplica-se a |
| --- | --- | --- |
| P10 | **Paridade campo/GPU**: espelho de CPU e saída de GPU concordam dentro de 1 cm no mesmo tempo de simulação | F1, G1 |
| P11 | **Paridade física/render**: o corpo flutua na altura que o pixel mostra, verificado por captura com marcador na linha d'água | F2, G2 |
| P12 | **Auditoria de consumidor (E1)**: grep de cada eixo novo, publicado no relatório da etapa | todas |
| P13 | **Fallback nomeado (E3)**: cada capability ausente produz estado registrado, nunca shader indefinido | todas |
| P14 | **Determinismo (E4)**: 30/60/120 Hz produzem a mesma trajetória dentro da tolerância declarada | F1–F9 |
| P15 | **Sweep longo sem cintilação** em Adreno e Mali, câmera em movimento | G1, G7 |
| P16 | **Escala declarada (E7)**: nenhuma evidência publicada sem escala efetiva | todas |
| P17 | **Sono**: corpo em repouso na água dorme e não acorda sozinho | F2 |
| P18 | **Sem alocação (E5)** no caminho de frame, nativo e managed | todas |
| P19 | **Lote na fronteira (E10)**: contador de chamadas nativas por frame com teto no teste | F2, P7 |

---

## 9. Ordem de execução

```
L1  lifecycle FORTIFY isolado                    ── desbloqueia G8 e troca de cena
 │
G0  zero medido a escala 1,0 + atribuição        ── bloqueia toda a trilha G
 │
 ├── F0  contrato WaterField ──┬── F1  espelho espectral em CPU
 │                             │    │
 │                             │    ├── F2  empuxo ──┬── F3  hidrodinâmica de casco
 │                             │    │                └── F7  personagem na água
 │                             │    └── F9  tempo e determinismo
 │                             │
 │                             └── P0  WaterProfile v3 ── P1  tabela de descritores
 │                                                         │
 │                                                         ├── P2  presets
 │                                                         ├── P6  diagnóstico
 │                                                         └── P8  painel gerado
 │
 ├── G1  espectro superior ── G2  clipmap ── G3  óptica ──┬── G4  reflexão
 │                                                        │      ↑
 │                                            C1  atmosfera ──────┘  (pré-requisito)
 │                                                        │
 │                                                        └── G6  subaquático
 │
 ├── F4  corpo→água ── F5  ondas dinâmicas ── F6  fluxo e rios ── F8  spray e chuva
 │                                                  │
 │                                                  └── P3  multi-água ── P4  zonas
 │
 └── G5  espuma e costa ── G7  estabilidade temporal ── G8  água na floresta
```

**Caminho crítico de física:** F0 → F1 → F2. Entrega flutuação correta e
determinística, que é o que hoje não existe em nenhuma forma.

**Caminho crítico de gráficos:** G0 → G1 → G3. Entrega detalhe, profundidade e o
fim da cintilação.

**Paralelismo seguro:** F0/F1 não tocam em código de render e podem correr junto
com G0/G1. P0/P1 tocam em serialização e fronteira, e devem entrar antes de a
contagem de eixos crescer — refatorar 90 eixos depois custa muito mais.

**Sequência mínima para um marco demonstrável** (barco flutuando em mar espectral
com espuma na crista, medido e honesto): L1 → G0 → F0 → F1 → F2 → G1 → G3.

---

## 10. Rastreabilidade

Cada eixo novo entra nesta tabela ao nascer. Linha sem consumidor e sem teste
reprova a etapa (E1, P12).

| Bloco | Eixos | Consumidor | Teste | Etapa |
| --- | --- | --- | --- | --- |
| `spectrum` | wind{Speed,Direction,Turbulence}, fetch, depth, swell, spread, timeScale, cascade{domain,resolution,weight,choppiness}[] | `generateWaterCascades`, compute de evolução, espelho de CPU | espectro determinístico; RMS de truncamento | G1, F1 |
| `mesh` | basePatchSize, maximumDistance, clipmapLevels, patchQuads, skirtDepth, hysteresis{forward,backward,rotation} | `planWaterClipmap`, vértice instanciado | sem costura; sem popping | G2 |
| `optics` | absorption[3], scattering, turbidity, refraction{mode,strength,depth}, dispersion{,strength}, ior, roughness | fragmento da água, input attachment | raso/fundo; sem vazamento | G3 |
| `reflection` | mode, screenSpace{holesFilling,borderStretch}, planar{mask,contents}, cubemap{interval,mask}, sun{strength,size}, anisotropic{,scale} | `resolveWaterPipeline`, passes de reflexão | por tier; fallback nomeado | G4 |
| `foam` | jacobianThreshold, growth, decay, color, size, fadeDistance, advection | compute de espuma, fragmento | EDO independente da taxa | G1, G5 |
| `interaction` | dynamicWaves{area,resolutionPerMeter,fps,propagationSpeed,damping}, maxSplats, rain{strength,radius}, flowMap{area,scale,speed}, kelvinWake{threshold,strength} | F4, F5, F6, `WaterFieldSample::flow` | CFL; interferência; rolagem sem perda | F4–F6 |
| `physics` | provider, spectrumHz, mirrorResolution, maxBodies, sectionCount, density, drag{linear,angular,quadratic}, addedMass, slamming, forceClamp | `AetherPhysics_ApplyWaterForces` | calado; metacentro; sono; determinismo | F1–F3 |
| `body` | volumeSource, hullMesh, sectionCount, densityOverride, sampleBudget | componente flutuante | calado teórico por forma | F2, F3 |
| `zones` | shape, bathymetry, current, foamModifier, exclusion, temperature | `WaterField`, buffer de shader | coerência física/visual | P4 |
| `quality` | tier, filtering{mode,anisotropy}, debug.view, writeToPostDepth | resolução, sampler, vistas | por tier forçado | P5, P6 |
| `time` | clockEpoch, network.timeSource, fixedStep | `WaterClock` | replay determinístico | F9 |

---

## 11. Riscos

| Risco | Probabilidade | Mitigação |
| --- | --- | --- |
| **A escala 0,5 esconde um déficit grande de fragmento.** Ao subir para 1,0, a água pode custar 2–3× o medido. | Alta | G0 é bloqueante e vem antes de qualquer promessa. Se 1,0 não couber, o alvo declarado muda para 60 Hz — não se ajusta a evidência. |
| Sem fila compute dedicada, 256² não cabe | Média | Cair para 128², cadência 30 Hz e rebaixar o tier, com log. Já previsto na degradação escalonada. |
| Espelho de CPU diverge da GPU e P10 reprova | Média | A porta existe justamente para pegar isso cedo. Causas prováveis: ordem de FFT, convenção de fase, relógio. Teste de oráculo com onda oblíqua fechada já existe e é o ponto de partida. |
| Empuxo instável (jitter, corpo que não dorme) | Média | Substep, clamp com contador, teste de sono dedicado, massa adicionada. É a falha mais comum em água de jogo e tem porta própria (P17). |
| Truncamento espectral erra o suficiente para o barco flutuar visivelmente errado | Média | RMS medido e publicado antes de fechar F1; resolução sobe se passar de 5 cm. P11 verifica com marcador visual. |
| L1 (lifecycle) não é isolado e trava G8 | Média | L1 tem dono e vem primeiro na ordem. Se não fechar, a água na floresta é adiada explicitamente, não empurrada com "funciona em processo novo". |
| Tabela de descritores vira API pública cedo demais | Baixa | Versionada junto com `WaterProfile` v3, com migração obrigatória. |
| Crescimento de eixos sem consumidor | Alta se não vigiada | P12 é porta de etapa, com grep publicado. A auditoria de 04/09 já encontrou quatro APIs e um slider nessa condição. |
| Multi-água exige refatorar a UBO global tarde demais | Média | P3 entra logo após P0/P1, antes de a óptica e a reflexão multiplicarem os pontos de acesso à UBO. |

---

## 12. O que este plano não promete

- Não promete paridade com KWS nem com GodotOceanWaves. Promete um conjunto de
  sistemas com contrato, orçamento e prova.
- Não promete 120 FPS. Promete medir a escala real e declarar o alvo que couber.
- Não promete que a física de casco chegue ao tier B. Ela nasce opt-in em A/S.
- Não trata nenhum item como entregue por estar escrito aqui. O estado de cada
  etapa muda no `ESTADO.md` só depois de integração e prova no aparelho.
