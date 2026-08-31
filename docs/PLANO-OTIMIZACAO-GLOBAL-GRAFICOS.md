# Plano global de desempenho gráfico, iluminação e estabilidade visual

> **Estado inicial em 28/08/2026:** plano criado a partir da inspeção do APK em
> hardware físico e alinhado aos itens 2.1–2.5, 7.1–7.6 e aos KPIs da Parte 18 de
> `PLANO-ENGINE-MOBILE.md`. Este documento ordena trabalho já previsto no roadmap
> para atacar o frame completo; não cria um renderer paralelo nem declara M2/M7
> concluídos antecipadamente.

> **Baseline real, mesma câmera escolhida pelo usuário:** 28,68 FPS antes da
> ordenação; **48,42 FPS** depois de opacos sólidos antes de alpha-mask, com diferença
> média de imagem de 0,005. Um experimento de subdivisão em 396 draws caiu para
> **25,64 FPS** e foi removido. Isso fixa a ordem arquitetural: cena GPU persistente e
> indirect/compactação antes de granularidade espacial fina.

> **Cadência medida em 29/08/2026:** o painel e o Choreographer entregam 120 Hz,
> mas esta carga apresentou 30,86 FPS ao votar 120. Com Surface/pacer em 60 Hz,
> a segunda janela aquecida apresentou **54,61 FPS**, CPU média 2,64 ms e espera
> de acquire média 11,97 ms. Esse era o baseline histórico; em 29/08 o teto padrão
> de 60 foi removido. O modo global `Auto` agora consulta os modos físicos e solicita
> até 120/90/60 Hz, sem configuração por cena. Sustentação térmica continua sendo
> gate separado e pode futuramente reduzir a política resolvida com histerese. Em
> abertura fria normal pelo launcher no Xiaomi conectado, sem extras de benchmark,
> a engine resolveu/pediu 120 Hz, o Android aceitou e 126 presents recentes mediram
> **117,30 FPS** no SurfaceFlinger.

> **Baseline Release reproduzível após a primeira otimização, em 29/08/2026:**
> cena `dirt-road`, fingerprint `dd907ec34bbebc21`, câmera travada em
> `0,160,-100,0,0.08`, 2772×1280 e voto Android de 120 Hz confirmado. Em 69,121 s,
> a engine concluiu **78,12 presents/s** (pior janela 73,43), CPU média 1,41 ms e
> GPU média 11,40 ms, com pior p95 de janela em 18,41 ms. O aparelho aqueceu de
> 38,3 para 40,0 °C sem aviso térmico. Isso não é A/B da normal matrix nem soak,
> mas substitui a câmera distante por um baseline autoritativo do hotspot.
>
> **Experimento rejeitado em 29/08/2026:** backface culling orientado por
> `doubleSided`/handedness continuou removendo terreno/folhagem mesmo quando materiais
> `MASK` legados permaneceram dupla face: 9,42% dos pixels diferiram do baseline. Também
> perdeu no A/B físico consecutivo: **97,46 → 86,34 presents/s**, GPU média **9,04 →
> 9,98 ms** e GPU p95 **9,89 → 13,65 ms**, com `thermalStatus=0`. A implementação foi
> retirada. Culling não será tratado como ganho até AGI atribuir custo e um novo desenho
> passar desempenho e imagem.
>
> **Isolamento de custo em 30/08/2026:** modos diagnósticos globais passaram a ser
> variantes reais de pipeline Vulkan por specialization constant, sem alterar presets,
> projeto ou conteúdo. No A/B intercalado `full → base-color → full`, a variante mínima
> não foi mais rápida: GPU média **9,72 → 10,08 → 11,04 ms** e SurfaceFlinger
> **93,45 → 88,51 → 91,61/s**, mesma câmera, fingerprint, APK e `thermalStatus=0`.
> Portanto aritmética PBR/normal/IBL não foi comprovada como o próximo gargalo; o ciclo
> segue para atribuição de raster/vertex/tiles, visibilidade e frame pacing. Uma matriz
> anterior com branch dinâmico permanece apenas como smoke metodológico, pois o próprio
> controle `full` variou de 90,81 para 70,79 presents/s.
>
> **Primeiro ganho do ciclo seguinte em 30/08/2026:** AEMAP v2 compactou o layout
> global de vértices de 72 para 48 bytes, preservando posição/UV em float32 e mantendo
> leitura do v1. O mapa caiu **34,69→24,49 MB** e o APK **402,44→392,24 MB**. Em dois
> runs físicos, GPU média ficou em **8,89/7,42 ms**, engine em **99,40/117,29
> presents/s** e SurfaceFlinger em **110,20/111,85/s**; controles v1 imediatamente
> anteriores ficaram em 9,72/11,04 ms e 92,15/82,12 presents/s. Apenas 0,0147% dos
> pixels mudaram, sempre por no máximo 1/255. É ganho inicial em Adreno; A/B longo,
> soak e Mali continuam gates obrigatórios.

> **Céu e iluminação global em 30/08/2026:** a fotografia `sunset_forest`
> 4096×2048/RGBA16F deixou de ser o fundo e foi substituída por panorama diurno
> 2:1 de nuvens, cozido offline em 1024×512/RGBA8 sRGB com mips e seam horizontal.
> O recurso caiu de ~89,5 MB para **2,80 MB** e o céu voltou a custar uma única
> amostragem por pixel; a tentativa de ruído procedural por fragmento (84,82 FPS)
> foi rejeitada. A versão final mediu **117,27 FPS** logo após ativação de boost e
> ~98,7 FPS depois que o governador relaxou, sem alerta térmico. Iniciar `screenrecord`
> no mesmo APK/câmera elevou a cadência para **112,04 FPS**, confirmando que a melhora
> observada pelo usuário vem do voto de energia/compositor do sistema, não de um cap
> de 60 na engine. A luz difusa agora usa hemisfério global parametrizado em AEEN v2;
> a vegetação ganhou 21% de luminância média na pose fixa (37,34→45,18) sem passe,
> draw ou textura extra. CSM/GTAO/SH continuam pendentes e não são declarados prontos.

> **Benchmark navegável e colisão em 30/08/2026:** o controle FPS virou uma fatia
> global (`FirstPersonController` + input actions + joystick + `CharacterMotor`),
> sem lógica embutida na câmera ou na cena. A primeira malha física usava posições
> locais e causava queda infinita; a correção aplica a mesma matriz column-major de
> cada draw usada pelo shader. A estrada revelou outro contrato: seus materiais são
> BLEND, portanto alpha visual não pode desativar física. A política final inclui
> BLEND, exclui cartões alpha-mask por default e expõe flags explícitas de importação.
> O aparelho registrou 188.681 vértices/156.119 triângulos físicos, Y estável e
> `ground=0` na estrada. Este recurso melhora a qualidade do benchmark, mas não fecha
> o gate de desempenho: perfil CPU/GPU longo com gameplay ainda é obrigatório.

## 1. Objetivo e regra de qualidade

Elevar o desempenho de cenas reais para **60 FPS sustentados como piso**, com 90 e
120 Hz como alvos de primeira classe quando o painel permitir, enquanto melhora —
ou no mínimo preserva — a imagem. O ganho deve vir de eliminar trabalho invisível, redundante ou incorreto,
de melhor organização do frame e de algoritmos mais eficientes. Reduzir resolução,
distância, sombras, texturas, iluminação ou geometria sem equivalência visual não
conta como otimização deste plano.

Regras obrigatórias:

1. Toda mudança compara a mesma câmera, cena, resolução de saída, conteúdo e estado
   térmico antes/depois.
2. Nenhum ganho é aceito se testes de imagem mostrarem perda não aprovada.
3. Correção visual vem antes da otimização do caminho incorreto.
4. Sistemas são globais e dirigidos por dados de cena/material; não entram hacks
   por nome de asset, mapa, fabricante ou aparelho.
5. Perfis S/A/B/C selecionam algoritmos e budgets globais. O perfil A permanece a
   referência visual e de 60 FPS; degradação térmica é fallback explícito, não o
   mecanismo principal de ganho.
6. CPU, GPU, compositor, memória, energia e estabilidade de frame são medidos
   separadamente. FPS médio sozinho não fecha nenhum gate.

## 2. Evidência observada no aparelho

Inspeção no Xiaomi `25053PC47G`, Android 16, resolução 2772×1280:

| Evidência | Resultado |
|---|---|
| Cena visual inspecionada | estrada/floresta com 341.109 triângulos, 27 draws, 26 materiais e 70 texturas |
| Carga da cena | 3.847 ms até `ready`; primeiro frame após ativação em ~4.342 ms |
| Memória reportada | após o céu compacto: ~63,7 MB em texturas, ~24,5 MB em buffers e ~14,3 MB em render targets |
| Ambiente atual | panorama diurno 1024×512 RGBA8 sRGB, 2,80 MB, direção esférica e mips; AEEN v2 guarda sol, ambiente, exposição e parâmetros globais |
| Defeito visual reproduzido | grande conjunto de árvores/folhagem opaco e esbranquiçado à direita da imagem |
| Visibilidade antes da fatia de 30/08 | todos os 27 draws eram enviados; não havia frustum culling, occlusion culling/HZB ou LOD |
| Visibilidade integrada em 30/08 | frustum culling CPU conservador por draw, listas scratch sem alocação e telemetria de candidatos/visíveis/descartados/draw calls/triângulos; no aparelho variou de 27/27 a 18/27 draws visíveis durante movimento, ainda sem HZB/LOD/granularidade espacial |
| Perfil de 30 s | ~59,90 FPS exibidos, mas o runner ativou `poc-a-5000-textured-cubes`, não a cena de floresta |
| Baseline Release 120 Hz atual | 78,12 presents/s; CPU 1,41 ms; GPU 11,40 ms média e 18,41 ms no pior p95 de janela |

O resultado de ~59,90 FPS não invalida os ~45 FPS percebidos: hoje o perfilador
troca a carga e rotula a captura como PoC-A. Antes de otimizar, a cena real precisa
ser perfilável sem mudar de modo.

### 2.1.1 Baseline móvel reportado em 30/08/2026

No percurso livre do jogador, o aparelho forte reportou picos de 120 FPS, faixa mais
frequente de 60–80 FPS e quedas locais até ~47 FPS. Esse dado em movimento substitui
“FPS parado” como sinal de produto, mas ainda precisa de rota, duração, temperatura e
perfil v3 registrados para virar comparação aprovada. A aparente estabilização ao
ligar o gravador de tela é compatível com mudança de política de energia/DVFS e de
composição, porém ainda é hipótese: a engine não deve manter carga artificial nem
usar API privada para forçar clocks. O experimento obrigatório compara gravador
desligado/ligado na mesma rota e registra ADPF, estado térmico, clocks/frequências
expostos pelo sistema, GPU p95 e SurfaceFlinger.

### 2.1 Baseline de produto reportado em 29/08/2026

O comportamento percebido pelo usuário acrescenta dois pontos de referência que
precisam ser reproduzidos pelo runner antes de virarem métricas aprovadas:

| Faixa observada | Resultado reportado | Interpretação até existir captura reproduzível |
|---|---:|---|
| aparelho forte, painel/limite em 120 Hz | 55–60 FPS | o frame não cabe em 8,33 ms; também não sustenta margem confortável de 60 Hz |
| Samsung Galaxy A32 ou aparelho equivalente | 20–25 FPS | perfil C está abaixo do piso de 30 FPS definido em M2 |

“Galaxy A32” não pode virar uma condição no código: existem variantes de SoC, GPU,
memória e resolução. A captura deve registrar modelo/SKU, SoC, GPU, driver, Android,
resolução, refresh, bateria e estado térmico. Até isso acontecer, os números são
**baseline de produto reportado**, não benchmark científico nem ganho comprovado.

### 2.2 Causas já demonstráveis no código

- O importador distingue somente `OPAQUE` e `BLEND`. `alphaMode=MASK` é ignorado,
  embora `alphaCutoff` seja serializado. O shader só descarta alpha quase zero para
  materiais marcados como blend. Folhagem `MASK` acaba desenhando o fundo claro do
  atlas como superfície opaca — causa primária do branco observado.
- `doubleSided` também não é importado. O pipeline inteiro usa `CULL_MODE_NONE`,
  escondendo o erro de autoria às custas de shading duplicado em toda a cena.
- Transparência é separada apenas por um bit e ordenada por primitiva. Vegetação
  recortada não deve entrar no caminho blend; transparências reais precisam de
  contrato e ordenação estáveis.
- A iluminação difusa agora é hemisférica e global, mas ainda não possui sombra, AO
  ou visibilidade. CSM/GTAO continuam necessários para contraste sob a copa; a
  calibração transitória não fecha O2/E3.
- O céu visível usa panorama 2:1 compacto e direção reconstruída da câmera. Nesta
  primeira fatia, o mesmo AETX ainda alimenta reflexos seletivos; SH difuso e probe
  especular pré-filtrado independente permanecem o próximo contrato de ambiente.
- O renderer recria pipelines/render passes diretamente apesar da infraestrutura de
  `PipelineCache` já conectada ao `VulkanDevice`.
- O frame ainda não executa o Render Graph real na GPU e usa um command buffer único.

## 3. Orçamentos e gates

### 3.1 Gate visual

- Zero folhagem branca por erro de alpha/material.
- Zero objeto desaparecendo por bounds, culling, LOD, ordenação ou precisão.
- Vegetação mantém silhueta, densidade, normal mapping e sombras.
- Céu azul, nuvens e disco solar coerentes com a luz direcional.
- Interior/áreas sob copa preservam contraste sem preto esmagado nem branco lavado.
- Comparação automatizada por câmera usa SSIM/FLIP e máscara de pixels instáveis;
  qualquer diferença fora do orçamento exige aprovação visual.

### 3.2 Gates globais de cadência

| Alvo | Intervalo | CPU p95 | GPU p95 | Falha de sustentação |
|---:|---:|---:|---:|---:|
| 120 Hz | 8,33 ms | ≤ 6,50 ms | ≤ 7,33 ms | p95 > 8,33 ms |
| 90 Hz | 11,11 ms | ≤ 8,67 ms | ≤ 9,78 ms | p95 > 11,11 ms |
| 60 Hz | 16,67 ms | ≤ 13,00 ms | ≤ 14,67 ms | p95 > 16,67 ms |

Render e fixed tick são independentes: padrão de simulação 60 Hz, apresentação em
60/90/120 Hz e interpolação entre snapshots. Projetos podem selecionar 30/60/120 de
simulação por necessidade de gameplay; a tela nunca define a velocidade do jogo.

### 3.3 Gate sustentado do perfil A

| Métrica | Alvo | Falha |
|---|---:|---:|
| FPS exibido sustentado | ≥ 60 | < 60 |
| Frame GPU p50 / p95 / p99 | ≤ 13 / 15,5 / 16,6 ms | p99 > 16,6 ms |
| CPU de submissão p95 | ≤ 3 ms | > 4 ms |
| Stutter | nenhum frame > 33,3 ms em 10 min | qualquer ocorrência não explicada |
| Potência em 30 min | ≤ 4 W | > 5 W |
| Estado térmico | sem throttle sustentado | queda de clock/FPS não recuperada |
| Alocação no loop estável | 0 | qualquer GC induzido pelo renderer |
| Chamadas C#↔nativo | ≤ 200/frame | > 300/frame |

O frame mantém folga de GPU para UI, simulação e variação de driver. “16,6 ms exatos”
na cena vazia não é uma margem aceitável.

### 3.4 Metas por perfil de desempenho

Perfil não é sinônimo de qualidade artística nem de uma lista de modelos de telefone.
Ele é um orçamento global de frame resolvido a partir de capacidades, calibração,
preferência do projeto e estado térmico. O mesmo conteúdo, materiais e renderer são
usados em todas as cenas.

| Perfil | Gate sustentado inicial | Meta evolutiva | Contrato visual |
|---|---:|---:|---|
| S | 60 FPS | 90/120 FPS quando p95 e potência permitirem | referência máxima; efeitos avançados opcionais somam detalhe |
| A | 60 FPS | 60 com folga para editor/gameplay | referência visual de produção |
| B | 45 FPS, 60 desejável | 60 após visibilidade/indirect | mesma identidade, silhueta, materiais e iluminação; algoritmos equivalentes podem ter budgets menores |
| C | 30 FPS | 45 FPS como meta de otimização | nenhum asset removido e nenhum material simplificado por cena; menor frequência temporal/LOD por erro de tela somente dentro do gate visual |

O primeiro objetivo para a classe do A32 é sair de 20–25 para **30 FPS estáveis**
sem corte de conteúdo. Depois de cena GPU persistente, culling, LOD e redução de
bandwidth, o alvo passa a 45 FPS. Prometer 60 antes da medição física esconderia a
diferença entre desejo e orçamento real; 60 continua um alvo de pesquisa, não um gate
de M2 para perfil C.

### 3.5 Fonte única de configurações globais

O sistema final resolve uma política imutável por “época” de configuração:

```text
DeviceCapabilities (fatos Vulkan/Android)
        + DevicePerformanceCalibration (medição curta e banco conhecido)
        + ProjectRenderingSettings (preferência global serializada)
        + ThermalPowerState (pressão transitória com histerese)
        = ResolvedRenderingPolicy (budgets consumidos pelo frame inteiro)
```

Responsabilidades que não podem ser confundidas:

- `DeviceCapabilities` diz somente o que existe; suporte a bindless/mesh shader/VRS
  não prova que a implementação seja rápida. A classificação atual em
  `native/rhi/device_profile.*` é uma boa base de capability, mas ainda não é uma
  classificação de desempenho.
- `DevicePerformanceCalibration` mede fill rate, bandwidth, custo de shader,
  throughput de triângulos, upload e CPU de submissão com workloads curtos e
  determinísticos. Resultado é cacheado por GPU+driver+build do benchmark e pode ser
  invalidado após atualização de driver/engine.
- `ProjectRenderingSettings` define globalmente cadência-alvo, política visual,
  limites de memória e overrides Auto/S/A/B/C/Custom. Nenhuma cena escolhe um perfil.
- `ThermalPowerState` só reduz o budget resolvido depois de pressão observada. Não
  altera a configuração autoral nem grava degradação temporária no projeto.
- `ResolvedRenderingPolicy` é a única entrada do renderer para LOD por erro projetado,
  sombras, GI/AO, pós, streaming, uploads e trabalho assíncrono. Mudanças são atômicas,
  têm motivo/telemetria e não podem oscilar a cada frame.

Configurações artísticas continuam data-driven por material/luz/câmera. Por exemplo,
`alphaMode=Mask` e `doubleSided` descrevem o conteúdo; não são “tuning da floresta”.
Já budgets como erro máximo de LOD, cascatas de sombra, amostras de AO, tamanho de
atlas, taxa de atualização de probes, render scale e residência de textura pertencem
à política global, são refletidos, serializados, versionados e expostos no Project
Settings/Inspector. Presets são dados, não `if` espalhados pelo renderer.

## 4. Ordem global de execução

Esta ordem é uma dependência técnica, não uma lista de ideias: **frame policy e
instrumentação → fixed tick consumido pelo loop → change tracking de chunks ECS →
cena GPU persistente → culling/LOD/compactação GPU → indirect → Render Graph mobile →
shader/bandwidth → governor térmico**. Pular para subdivisão antes de indirect repete
o caso medido de 396 draws e aumenta CPU em vez de reduzi-la.

**Estado do ciclo atual:** política portátil 60/90/120 implementada; o Android separa
capability do painel e preferência global, e `Auto` usa até 120 Hz; Surface Android
recebe voto explícito; Choreographer filtra callbacks pela cadência resolvida; taxa
VSYNC observada é registrada; alpha-mask, céu panorâmico compacto, iluminação hemisférica e
ordenação solid-first estão ativos. Timestamps GPU por pass e o prepass seletivo de
cobertura estão ativos. O isolamento compilado de fragment confirmou que simplesmente
retirar PBR/normal/IBL não produz ganho mensurável nesta carga. Próximo gate obrigatório:
atribuir vertex/raster/tiles e frame pacing em captura GPU, depois reduzir trabalho
invisível por bounds/visibilidade e compactação comprovada. Somente então reavaliar
2–3 frames em voo sem compartilhar depth/command/instance buffers.

**Diagnóstico GPU de 29/08/2026:** timestamps Vulkan reais mediram 22,41–22,59 ms
de GPU média no hotspot, contra 4,05–5,07 ms de CPU de processo. A espera de acquire
de 23,12–23,38 ms é consequência dessa carga GPU, não prova de falta de frames em voo.
Logo, múltiplos frames em voo fica depois da redução de GPU para evitar apenas aumentar
latência. Próxima ordem: separar timestamps por pass → depth/coverage prepass A/B para
alpha-mask → reduzir overdraw/shading oculto → compactar IBL/bandwidth → reavaliar fila.

**Ciclo GPU concluído em 29/08/2026:** o FrameProfile v3 passou a registrar
`gpu_geometry_ms`, `gpu_background_ms` e `gpu_transparent_ms`. Em GPU móvel TBDR,
checkpoints dentro do mesmo render pass podem ser resolvidos no fim do tile e não
devem ser interpretados como uma captura AGI; ainda assim, o A/B isolou o custo de
cobertura. O prepass global de materiais `MASK` (alpha+depth, seguido de PBR com
depth `EQUAL`) reduziu a GPU fresca de **17,12 para 12,71–12,78 ms** e sustentou
**60,05–60,07 FPS** na câmera do hotspot, mesma saída 2772×1280. Já aquecido, o
A/B no mesmo APK reduziu 28,94→22,68 ms (−21,6%) e elevou 30,24→35,99 FPS (+19%).
A comparação visual manteve chão, árvores, recortes e iluminação; 99,966% dos
pixels foram idênticos. O próximo custo global é shader/normal transform e
bandwidth de materiais/IBL, antes de reavaliar frames em voo.

### O0 — Instrumentação da cena real e verdade de frame

**Alinha:** 2.1.6, 7.6.2–7.6.4 e Parte 18.

1. Fazer o runner receber `sceneId` explícito e registrar no relatório a identidade,
   hash e contagens do pacote realmente renderizado. Perfil nunca pode ativar PoC-A
   implicitamente.
2. Criar percurso determinístico de câmera para exterior com vegetação, estrada,
   céu, close de material e vista com alta oclusão.
3. Adicionar timestamp queries por pass e correlação CPU/GPU/SurfaceFlinger.
4. Registrar por frame: draws submetidos/visíveis, triângulos, mudanças de PSO,
   descriptors, pixels/tiles estimados, uploads, memória, resolução e motivo de
   fallback.
5. Capturar frame AGI/RenderDoc e estabelecer mapa de custos por pass, bandwidth,
   overdraw e stalls.
6. Rodar baseline debug e release por 60 s, depois soak de 30 min no mesmo percurso.

**Aceite:** o relatório identifica a floresta, reproduz a faixa percebida e permite
atribuir cada milissegundo a CPU, GPU, espera do compositor ou thermal governor.

**Progresso em 29/08/2026 (primeira fatia de G0):** o runtime passou a emitir
`FrameProfileContext` schema 1 com `sceneId`, fingerprint FNV-1a dos bytes AEMAP,
câmera/lock, alvo de FPS, resolução e contagens de instâncias, draws, materiais,
texturas e triângulos. O runner salva `frame-contexts.jsonl`, gera captura schema 2
e rejeita cena pedida diferente da cena realmente carregada, contexto ausente ou
mudança de identidade no mesmo PID/epoch. Em `-Scene dirt-road`, a câmera inicial é
travada mesmo sem pose explícita. Ainda faltam percurso animado, contadores
visível/ocluído/LOD/fallback, captura AGI e baseline em hardware C.

#### O0.1 — Android Studio, AVD sintético e laboratório físico

O Android Emulator usa CPU/GPU do host por aceleração ou um renderer de software;
portanto ele **não emula o custo real** de uma Mali-G52/G57, a largura de banda do
SoC, o driver Samsung, o TBDR físico, DVFS, potência ou dissipação térmica. Um AVD
que mostra 60 FPS não aprova desempenho no A32, e um AVD lento também não reprova o
renderer móvel.

Ainda assim, deve existir um AVD versionado `Aether-C-Synthetic` para regressão:

1. Perfil de hardware com resolução equivalente à faixa alvo, 2 cores e 4 GB de RAM;
   imagem Android/ABI e backend gráfico ficam registrados no relatório.
2. Vulkan com aceleração `host/auto` valida execução e correção; SwiftShader/Lavapipe
   valida fallback/portabilidade separadamente, nunca números de FPS.
3. Um override **de teste** força `DeviceProfile::C`/caminhos sem bindless e os budgets
   C sem falsificar as capabilities reportadas. O relatório marca claramente
   `syntheticProfile=true`.
4. O percurso determinístico da floresta roda em 30/45/60 Hz, resolução fixa e câmera
   idêntica; valida seleção de perfil, serialização, lifecycle, memória, imagem e
   ausência de dependência por cena.
5. Pressão de memória/lifecycle e estados térmicos do `PowerGovernor` são injetados
   por interfaces de teste. Valores artificiais validam transições, não energia real.
6. Entregáveis: a definição versionada e o comando PowerShell do AVD já existem e são
   testados; launcher do percurso com `profile/cameraPath` e manifesto completo de
   captura continuam planejados. Nenhum resultado emulado conta como certificação.

O perfil versionado fica em `tools/android-avd/Aether-C-Synthetic.json`. Ele fixa
Android 35/ARM64 (mesma ABI do APK), 2 cores, 4 GB, 1080×2400, 60 Hz e renderer
`auto`, e aparece no Device Manager do Android Studio após criação. Em host x64,
a CPU ARM64 não recebe aceleração WHPX; o script registra isso e o AVD pode ser lento
ou indisponível — um AVD x86_64 só entrará quando o runtime inteiro suportar essa ABI:

```powershell
# Instale antes a imagem system-images;android-35;google_apis;arm64-v8a pelo SDK Manager.
.\tools\ensure-android-performance-avd.ps1
# Caminho de portabilidade/fallback, sem comparar FPS:
.\tools\ensure-android-performance-avd.ps1 -SoftwareRenderer -Launch
```

O comando é idempotente para um AVD marcado pelo mesmo `profileId` e recusa substituir
silenciosamente um AVD homônimo do usuário. `-Recreate` é a única operação destrutiva
e precisa ser solicitada explicitamente.

O gate de desempenho permanece em aparelho físico:

- aparelho forte Adreno já disponível, na resolução nativa e em 60/90/120 Hz;
- um Mali perfil C físico, preferencialmente o A32 exato usado no relato; se não for
  possível, registrar o aparelho equivalente sem chamá-lo de A32;
- pelo menos um Mali intermediário para separar regressão de fabricante de regressão
  de classe;
- Firebase Test Lab com **Game Loop** determinístico para ampliar a matriz quando o
  modelo estiver no catálogo; resultado físico complementa, não substitui, o aparelho
  local usado para AGI e soak térmico.

Ferramentas por finalidade:

| Ferramenta | Uso válido | Não usar como |
|---|---|---|
| Android Studio CPU/Memory Profiler + Perfetto/Simpleperf | threads, JNI, alocações, stalls e scheduling | tempo GPU inferido |
| AGI System Profiler | CPU/GPU concorrentes, counters, bandwidth, energia suportada | comparador visual isolado |
| AGI Frame Profiler | passes Vulkan, draws, pipelines, shaders, texturas e framebuffer | soak longo sem overhead |
| AVD `Aether-C-Synthetic` | fallback, automação, memória, lifecycle e imagem | “emulador de A32” ou certificação de FPS |
| aparelho físico + runner | FPS exibido, GPU timestamps, driver, potência e térmica | teste manual sem câmera/hash controlados |

Referências operacionais oficiais: [configuração de AVD](https://developer.android.com/studio/run/managing-avds),
[aceleração do Emulator](https://developer.android.com/studio/run/emulator-acceleration),
[Android GPU Inspector](https://developer.android.com/agi) e
[Game Loop no Firebase Test Lab](https://firebase.google.com/docs/test-lab/android/game-loop).

### O1 — Correção global de materiais e visibilidade

**Alinha:** 2.2.2, 2.2.5, 2.4.2, 2.4.5, 7.3.5.

1. Tornar `AlphaMode` enum estável (`Opaque`, `Mask`, `Blend`) no formato de material;
   preservar `alphaCutoff`, `doubleSided` e alpha do `baseColorFactor` no import.
2. Criar variantes/pipelines globais por estado de rasterização, não por asset:
   opaque single-sided, opaque double-sided, alpha-mask single/double-sided e blend.
3. Executar alpha-mask no caminho depth-writing/opaque, com `discard` pelo cutoff,
   alpha-to-coverage quando MSAA existir e preservação de cobertura nos mipmaps.
4. Usar backface culling nos materiais single-sided. Double-sided corrige a normal
   na face traseira e só paga custo onde declarado.
5. Separar blend real, ordenar de modo estável e documentar premultiplied versus
   straight alpha. Não usar blending para folhas recortadas.
6. Validar tangentes, normal map, espaço de cor, canais MR e UV por material.
7. Criar corpus de regressão com pinheiro, folhas finas, cerca, vidro, emissivo e
   materiais vistos pelos dois lados.
8. Investigar “objetos que somem” com IDs de draw, bounds e captura de câmera. Nenhum
   culling novo entra antes de provar que os bounds transformados são conservadores.

**Aceite:** a cena não possui áreas brancas; os mesmos materiais funcionam em qualquer
mapa; backface culling reduz shading sem remover geometria válida.

### O2 — Céu azul, nuvens, sol e iluminação coerente

**Alinha:** 2.4.3, 2.4.4, 2.4.6, 7.2, 7.3 e 7.4.

**Progresso validado em hardware em 30/08/2026 (primeira fatia dos itens 2–5):**
`tools/cook-sky-panorama.py` transforma um source 2:1 pertencente ao projeto em
AETX 1024×512 sRGB, cadeia completa de mips em espaço linear e seam horizontal.
O shader reconstrói a direção do pixel a partir da câmera, portanto o céu gira com
o mundo e não estica como uma imagem de tela. O runtime aceita AEEN v1 por migração
e AEEN v2 de 144 bytes, que serializa oito `vec4`: sol, ambiente, exposição e a
fundação de céu/ground bounce. A luz difusa hemisférica usa esses dados globalmente.
No Xiaomi físico, a imagem foi validada em yaw 0°/90°, o AETX caiu 89,5→2,80 MB,
o caminho de uma amostra atingiu 117,27 FPS em estado de boost e a tentativa
procedural full-screen foi descartada a 84,82 FPS. Ainda não há atmosfera analítica,
SH9, probe pré-filtrado separado, CSM, GTAO, exposição temporal ou AgX; esses itens
permanecem abertos e não devem ser inferidos desta fatia.

1. Separar três conceitos: **céu visível**, **irradiância difusa** e **reflexão
   especular**. Trocar a fotografia visível não pode destruir o IBL dos materiais.
2. Substituir o HDRI visível por céu procedural analítico com gradiente atmosférico,
   horizonte, disco solar com limb/soft edge e camada de nuvens 2D/3D barata, animada
   em coordenadas de mundo e temporalmente estável.
3. Manter opção global de HDRI autoral, mas usar representação própria e adequada ao
   céu. O padrão do sample passa a ser azul/nuvens/sol.
4. Derivar luz solar, cor do céu, exposição e IBL de um único `EnvironmentState`.
   Alterar hora/sol atualiza todos os consumidores de forma coerente.
5. Substituir ambiente difuso constante por SH de irradiância; adicionar probes de
   reflexão pré-filtradas e BRDF multiscatter.
6. Implementar CSM para o sol com splits estabilizados, atlas, PCF inicialmente e
   cache de casters estáticos. Vegetação usa alpha-test no shadow pass.
7. Adicionar oclusão de contato eficiente: primeiro GTAO/bent normals temporal;
   depois DDGI/GlowField conforme 7.2. Isso corrige áreas atrás de árvores sem
   falsificar cor por material.
8. Implementar exposição temporal por histograma, tone mapping AgX e white balance.
   Limitar adaptação para evitar bombeamento ao atravessar copa/clareira.
9. Nuvens volumétricas permanecem opcionais ao perfil S; o céu procedural padrão deve
   preservar a mesma composição visual nos perfis A/B/C.

**Aceite:** sol visível coincide com direção das sombras; nenhuma copa fica lavada;
o céu custa menos memória/bandwidth que o HDRI visível atual e materiais continuam
com reflexos/irradiância de qualidade igual ou superior.

### O3 — Eliminar trabalho invisível antes de reduzir pixels

**Alinha:** 2.5.1–2.5.5 e 7.1.1–7.1.6.

1. Construir BVH incremental global com bounds por draw/instância.
2. Aplicar frustum culling conservador e ordenar opacos front-to-back. Fatia CPU
   global integrada em 30/08/2026: seis planos, mesma matriz yaw/pitch do shader,
   expansão `radius*1,05 + 0,5`, fallback visível para dados inválidos, listas sem
   alocação por frame e contadores no log/perfil. Os bounds atuais são por draw de
   material e alguns cobrem grande parte do mapa; portanto esta fatia elimina trabalho
   seguramente, mas não substitui render chunks espaciais + indirect.
3. Gerar HZB e occlusion culling de dois passos com histerese; objetos recém-visíveis
   e bounds incertos ficam visíveis, nunca somem por um frame.
4. Agrupar instâncias automaticamente por malha+material+PSO e emitir draw packets
   persistentes. Remover trabalho por objeto da CPU.
5. Adotar `DrawIndexedIndirectCount`; meshlets/cluster culling entram como evolução
   global, com fallback discreto completo.
6. Gerar LODs no import por erro geométrico e selecionar por erro projetado em pixels.
   A transição dither temporal evita pop; silhueta e densidade visual são o gate, não
   apenas contagem de triângulos.
7. Ordenar PSOs e materiais para reduzir binds; transparências reais continuam
   back-to-front após a fase opaca.

**Aceite:** a imagem de referência é equivalente, nenhum objeto válido desaparece e
o custo escala com o conteúdo visível, não com o total carregado.

#### Contrato de chunks e geração de trabalho

1. Chunks ECS de 16 KiB armazenam componentes e versões; mudança de versão alimenta
   extração incremental, sem varredura de entidades estáveis.
2. Render chunks são persistentes e independentes dos chunks ECS. A chave de lote é
   mesh + material + PSO; bounds espaciais são metadados de visibilidade, não uma
   ordem para emitir um draw de CPU por célula.
3. O culling grava uma lista compacta de instâncias e `DrawIndexedIndirectCount`.
   Frustum é conservador; HZB tem histerese; objetos novos/incertos começam visíveis.
4. Streaming chunks possuem budget de bytes e tempo por frame, prioridade por
   distância/necessidade e nunca bloqueiam tick ou render thread.
5. Todo produtor declara frequência (`fixed`, `per-frame`, `on-change`, assíncrona),
   budget, ownership e contador de descarte/backpressure.

### O4 — Frame Vulkan orientado a TBDR

**Alinha:** 2.1.3–2.1.5 e 2.3.1–2.3.6.

1. Migrar todos os renderers para `PipelineCache`; persistir e pré-aquecer o cache.
2. Executar o Render Graph real na GPU, com anexos transient/memoryless, aliasing e
   fusão de passes comprovados por captura.
3. Tratar depth prepass como decisão medida em TBDR: usar onde alpha-mask/occlusion ou
   complexidade de fragmento paga o custo; evitar duplicar geometria cegamente.
4. Implementar Forward+ clusterizado e listas de luz por tile/froxel.
5. Gravar command buffers em paralelo por conjuntos de draw suficientemente grandes,
   usando timeline semaphores e múltiplos frames em voo sem aumentar latência.
6. Remover espera e upload do frame estável; streaming usa fila, staging ring e
   budget. Recriar surface não deve recarregar assets imutáveis.
7. Usar load/store ops explícitos e formatos compactos adequados ao mobile.

**Aceite:** captura AGI prova menos tráfego externo de memória, nenhuma criação de
pipeline no frame e ausência de bolhas evitáveis entre CPU, GPU e apresentação.

### O5 — Shaders, texturas e pós com equivalência visual

**Alinha:** 2.2.1–2.2.5, 2.4.2/2.4.6 e 7.5.1–7.5.6.

1. Migrar para biblioteca de shaders com reflexão e precisão explícita. Usar mediump
   somente onde teste quantitativo comprovar erro imperceptível.
2. Gerar variantes por capabilities/material e impor orçamento para evitar explosão.
3. Pré-filtrar IBL offline: SH pequeno para diffuse, cubemap/lat-long especular com
   resolução e mips adequados. Não manter 89,5 MB só para desenhar o céu.
4. Escolher ASTC por semântica: normal, HDR, alpha-mask e cor têm métricas e blocos
   próprios. Mips de alpha-mask preservam coverage.
5. Implementar TAA robusto e resolução dinâmica apenas após existir tempo GPU
   confiável. AetherSR/GSR/FSR devem demonstrar equivalência temporal e não podem ser
   usados para ocultar overdraw/culling ausente.
6. VRS atua somente em regiões de baixa importância comprovada e nunca em silhuetas,
   texto, folhagem fina ou highlights.

**Aceite:** queda de bandwidth/tempo GPU sem perda aprovada em FLIP/SSIM, sem shimmer,
ghosting ou perda de detalhe fino em movimento.

### O6 — Escalabilidade, 120 Hz e governança térmica

**Alinha:** 7.6.1–7.6.5 e `PowerGovernor`.

1. Calibrar o aparelho em 5 s e registrar GPU/driver/capabilities conhecidos.
2. Definir budgets, não listas arbitrárias de features, por perfil. O mesmo renderer e
   materiais são usados em S/A/B/C.
3. Perfil S oferece 90/120 Hz quando o frame p95 e a potência sustentada permitem.
4. PowerGovernor reduz primeiro frequência de atualização de sistemas temporais,
   budgets invisíveis e trabalho assíncrono. Alteração visível é o último estágio e
   deve ter histerese e indicação ao usuário.
5. Comparador visual dos perfis impede regressões silenciosas.

**Aceite:** 60 FPS sustentados no perfil A e transições térmicas sem oscilação,
stutter ou mudança visual abrupta.

## 5. Marcos executáveis

| Marco | Entrega demonstrável | Dependências | Critério de saída |
|---|---|---|---|
| **G0 — Verdade** | perfil da floresta, câmera determinística, timestamps GPU e captura AGI | nenhuma | reproduz FPS/defeitos sem trocar de cena |
| **G1 — Correção** | pipeline Opaque/Mask/Blend/DoubleSided e corpus de materiais | G0 | zero branco e zero desaparecimento conhecido |
| **G2 — Ambiente** | céu azul/nuvens/sol desacoplado do IBL, SH, exposição AgX | G1 | imagem melhor e custo do céu menor |
| **G3 — Sol e contato** | CSM cacheada + alpha-test de vegetação + GTAO/bent normals | G1–G2 | iluminação sob copa coerente |
| **G4 — Visibilidade** | BVH, frustum, HZB, instancing, LOD conservador | G0–G1 | custo proporcional ao visível, sem pop/sumiço |
| **G5 — Frame móvel** | Render Graph GPU, cache, Forward+, memoryless e paralelismo | G0–G4 | p99 GPU ≤16,6 ms e CPU p95 ≤3 ms |
| **G6 — Sustentação** | soak, perfis, governor e caminho 90/120 Hz | G5 | 60 FPS/30 min ≤4 W no perfil A |

G1 e G2 podem avançar em paralelo depois de G0. G3 depende da semântica correta de
alpha; G4 não pode ser aprovado enquanto houver desaparecimentos sem diagnóstico.

## 6. Ciclos de implementação por retorno e dependência

### 6.1 Ciclo já executado

Alpha-mask, ordenação solid-first, timestamps GPU por pass e prepass seletivo de
cobertura já demonstraram ganho grande mantendo 99,966% dos pixels idênticos. Esse
resultado é baseline; não autoriza pular os gates restantes.

### 6.2 Próximo ciclo — custo GPU imediato e verdade no perfil C

1. **Aparelho forte concluído; Mali C pendente.** O Xiaomi foi capturado em Release
   com APK/hash, fingerprint da cena, câmera, resolução, refresh físico confirmado,
   temperatura e métricas v3. Repetir o mesmo contrato no Mali C físico.
2. **Atribuição fina pendente:** AGI não está instalado no host e sua instalação exige
   aceite explícito da licença do SDK; o Android Emulator também não é alvo suportado
   para captura AGI. Como diagnóstico intermediário, o runner ganhou variantes globais
   `full`, `no-normal`, `no-ibl` e `base-color`, resolvidas por specialization constant
   na criação do pipeline e registradas no `FrameProfileContext`. O A/B especializado
   `full → base-color → full` não mostrou benefício de fragment: GPU média
   9,72→10,08→11,04 ms e SurfaceFlinger 93,45→88,51→91,61/s. Fazer a captura física
   AGI/Android Performance Analyzer do hotspot e ordenar por vertex, raster/tiles,
   render-target traffic, texture bandwidth, pipeline/descriptor e sincronização.
3. **Implementado e baseline físico pós-mudança capturado; A/B ainda pendente:**
   `transpose(inverse(mat3(inModel)))` saiu
   do vertex shader da floresta. Uma função reutilizável prepara três colunas da normal
   matrix uma vez por draw/transform, carrega o sinal do determinante para handedness
   da tangente e rejeita matriz singular/NaN sem escrita parcial. Há testes para escala
   não uniforme, escala pequena válida e espelhamento. O baseline pós-mudança é 78,12
   presents/s, CPU 1,41 ms e GPU 11,40/18,41 ms média/pior p95; falta reconstruir uma
   variante anterior controlada e executar o A/B frio/aquecido para atribuir ganho.
4. **Tentativa medida e retirada:** o backface culling por `doubleSided` e handedness
   permaneceu estável dentro de cada execução, mas diferiu do baseline em 9,42% dos
   pixels e removeu terreno/folhagem mesmo com `MASK` legado dupla face. Também regrediu
   o A/B físico: 97,46→86,34 presents/s; GPU média 9,04→9,98 ms e p95 9,89→13,65 ms.
   Não reaplicar por intuição. Antes de uma segunda tentativa,
   capturar AGI, versionar no AEMAP a proveniência single/double-sided de cutouts e
   provar ganho em Adreno e Mali sem multiplicação prejudicial de PSOs/buckets.
5. **AEMAP v2 integrado e v1 compatível:** o layout passou de 72 para 48 bytes por
   vértice. Posição/UV continuam float32; normal/tangente usam SNORM16 e cor UNORM8.
   O asset economizou 10,20 MB, o gate visual ficou em máximo 1/255 e dois runs Adreno
   reduziram GPU para 8,89/7,42 ms. Fechar A/B longo e Mali antes de considerar o gate
   completo. Fetches de normal/IBL isolados continuam hipóteses secundárias, não ajustes
   de qualidade autorizados.
6. **AEEN v2 e céu compacto implementados:** manter migração v1, completar SH de
   irradiância e probe especular pré-filtrado sobre o mesmo `EnvironmentState`, e
   depois medir os caminhos independentes. O importador HDRI legado não é o céu ativo.
7. Fechar com A/B frio e aquecido, captura visual, 60 s e soak físico; o AVD executa a
   mesma matriz apenas para correção/fallback.

**Saída:** perfil A com GPU p95 ≤14,67 ms e classe C em ≥30 FPS, sem alteração de
conteúdo ou diferença visual fora do gate. Se shader/bandwidth não entregar isso, os
counters da captura — e não preferência — escolhem a próxima intervenção.

### 6.3 Ciclo seguinte — cena persistente, visibilidade e complexidade

1. Change tracking ECS e extração incremental por chunk.
2. Render scene persistente, draw packets por mesh+material+PSO e uploads por dirty
   range; zero reconstrução estável e zero alocação no frame.
3. **Primeira fatia entregue:** bounds conservadores por draw, frustum CPU,
   ordenação front-to-back apenas do conjunto visível e telemetria. O visualizador
   de bounds e a validação por imagens durante uma rota determinística continuam
   como instrumentos obrigatórios para HZB/LOD.
4. **Segunda fatia entregue:** render chunks espaciais persistentes no import/runtime,
   índice opaco/coverage reordenado sem alterar triângulos e submissão multi-draw
   indirect agrupada por material. A subdivisão nunca volta ao caminho de dezenas de
   draws CPU quando `multiDrawIndirect` está disponível; o fallback permanece correto
   e detectado por capability.
5. HZB em dois passos com histerese e counters de falso positivo/falso negativo.
6. LOD gerado no import por erro geométrico, selecionado por erro projetado em pixels,
   com transição dither temporal e preservação de coverage da vegetação.

**Saída:** custo cresce com draws/triângulos visíveis, e não com tudo que está
carregado. A cena pode ganhar densidade/complexidade mantendo o mesmo budget.

#### Escopo executável para a regressão 60–80 → 47 FPS em movimento

##### Diagnóstico físico e entrega G4.1 — 30/08/2026

A utilização baixa de CPU não foi tratada como falta de clock. No Xiaomi de referência,
o processo ficou em aproximadamente 7,3% de CPU enquanto o `acquire` aguardava a GPU por
9–22 ms; com um frame em voo, a thread naturalmente fica bloqueada quando o frame GPU
ultrapassa o orçamento de 8,33 ms para 120 Hz. O isolamento na mesma câmera confirmou o
custo de fragment/texture: PBR completo 85,86 FPS/10,25 ms GPU; sem normal map 101,52
FPS/8,59 ms; sem IBL 97,54 FPS/8,91 ms; base color diagnóstico 120,11 FPS/6,45 ms. Essas
variantes continuam apenas como instrumentação e não viraram preset de qualidade.

A correção global manteve a imagem e atacou o trabalho real:

1. yaw/pitch e linhas world-to-view são calculados uma vez por frame na CPU, em vez de
   repetir trigonometria por vértice; Fresnel `pow5`, TBN e direção solar eliminaram
   normalizações e `pow` redundantes sem trocar o modelo PBR;
2. os 27 draws de origem são convertidos deterministicamente em 58 render chunks de até
   8.192 triângulos; índices e triângulos são preservados, e primitivas blend mantêm
   integridade e ordem;
3. chunks visíveis são agrupados por material e enviados com multi-draw indirect. No
   ponto fixo, 82 chamadas CPU da primeira versão de chunking caíram para 34, com os
   mesmos 485.139 triângulos submetidos;
4. no mesmo ponto, a build v7 foi de 85,86 FPS para 99,46 FPS na v10 (+15,8%), com GPU
   média de 10,25 para 9,23 ms. Na rota manual de 30 s, as cinco primeiras janelas
   ficaram em média 98,34 FPS; quando a câmera voltou a expor 57/58 chunks e 525.704
   triângulos, a janela caiu a 64,39 FPS e 13,73 ms GPU. Portanto o ganho é real, mas
   120 FPS estáveis ainda não estão alcançados.

O modo sustentado público do Android foi integrado como política global e condicionado
à capability. Este Xiaomi respondeu `supported=false`; nesse caso a engine não força
estado privado nem cria carga artificial. ADPF/Game State continuam sendo apenas hints,
e a próxima redução precisa vir de visibilidade e bandwidth medidos.

1. Gravar uma rota determinística de 60 s (posição/yaw/pitch por tick) atravessando os
   pontos de 120, 60–80 e ~47 FPS; o benchmark parado fica apenas como diagnóstico.
2. Registrar por janela CPU/GPU p50/p95/p99, acquire/present, temperatura, ADPF,
   candidatos/visíveis/descartados, draw calls e triângulos submetidos. Executar A/B
   com gravador desligado/ligado sem alterar cena ou preset.
3. Visualizar bounds e provar que o frustum atual não cria pop. Medir quantos dos 27
   draws ele consegue remover em cada trecho; bounds gigantes identificam necessidade
   de repartição, não autorização para reduzir distância ou geometria.
   A primeira execução física já confirmou variação de 27/27 para 18/27 draws e
   341.109 para 325.827 triângulos lógicos visíveis; como o número de triângulos cai
   muito menos que o de draws, os grupos mais pesados continuam espacialmente amplos.
4. **Entregue:** render chunks espaciais persistentes e multi-draw indirect, com
   fallback por capability, testes de preservação geométrica e imagem equivalente.
5. Gravar/reproduzir a rota determinística antes do próximo A/B e adicionar HZB de
   dois passos com histerese e estado “incerto = visível”; medir
   falso positivo/negativo e ganho nas áreas fechadas da floresta.
6. Só então ativar LOD por erro projetado em pixels, preservação de coverage e dither
   temporal. Nenhum LOD é escolhido por nome de cena ou modelo de aparelho.
7. Repetir em Adreno forte, Mali físico classe C e AVD `Aether-C-Synthetic`. O AVD
   valida rotação, input, lifecycle, fallback e memória; não certifica FPS, clocks,
   bandwidth ou comportamento térmico do Galaxy A32.

**Próxima ordem aprovada:** rota determinística → HZB conservador com histerese → LOD
por erro projetado/coverage+dither → nova captura GPU. Dois frames em voo só entram
depois que recursos por frame forem isolados e o tempo GPU estiver abaixo do budget;
eles podem esconder espera da CPU, mas não reduzem o custo de 13–19 ms do pior quadro.

### 6.4 Ciclo estrutural — Render Graph móvel e qualidade financiada

1. Pipeline cache persistente/pré-aquecido e nenhum `vkCreate*Pipeline` no frame.
2. Render Graph real, anexos transient/memoryless, load/store corretos e fusão medida
   em AGI.
3. Frames em voo reavaliados só após GPU abaixo do budget; recursos per-frame não são
   compartilhados indevidamente e latência toque→pixel continua no gate.
4. Forward+ e sombras CSM cacheadas financiam mais luzes e sombras mais estáveis sem
   custo linear por luz/caster.
5. GTAO/bent normals temporal em resolução adequada ao perfil acrescenta contato sob
   a copa; TAA preserva folhagem e habilita reconstrução quando mensurada.
6. Streaming por mip, ASTC por semântica e residency budgets permitem mais detalhe de
   textura sem manter tudo na memória/bandwidth.

**Saída:** captura AGI prova redução de tráfego externo e o ganho é reinvestido em
sombras, contato, iluminação e densidade, mantendo p99 e potência dentro do perfil.

### 6.5 Backlog original ainda válido

A ordem original permanece como checklist de integração, mas itens já executados não
devem ser contados novamente:

1. Corrigir o runner para perfilar `dirt-road` sem habilitar PoC-A.
2. Adicionar timestamp GPU do frame atual e captura AGI marcada por pass.
3. Corrigir import/formato/shader para `MASK`, `alphaCutoff` e `doubleSided`.
4. Criar regressões de imagem da câmera que mostra a floresta branca.
5. Separar sky visual de IBL e implementar o céu azul procedural com sol e nuvens.
6. Migrar `InstancedRenderer`/consumidores para `PipelineCache`.
7. Implementar ordenação opaca front-to-back e backface culling por material.
8. Introduzir frustum culling conservador com visualizador de bounds.
9. Medir novamente; só então escolher entre HZB, batching/indirect ou shader/bandwidth
   como próximo maior ganho.
10. Fechar o ciclo com build release, lint, testes, captura visual, 60 s e soak.

## 7. Testes obrigatórios

- Unitários do importador para todos os modos alpha, cutoff, double-sided, UV e canais.
- Golden packages versionados e teste de migração do formato de material.
- Testes de imagem estáticos e percurso em movimento, incluindo folhagem e reflexos.
- Bounds/culling property-based: culling otimizado nunca remove objeto que a referência
  conservadora considera potencialmente visível.
- Perfil bindless e fallback convencional.
- Perfis debug/release e validation layers sem VUID.
- Matriz mínima Adreno + Mali + perfil C; 60 s por PR e 30 min noturno.
- Regressão de lifecycle garantindo que cache de pipeline/assets sobrevive à surface.

## 8. Decisões proibidas

- Reduzir a resolução fixa, cortar sombras, remover árvores ou baixar texturas para
  “bater 60” antes de medir o gargalo.
- Criar condição `if (dirt-road)` ou por modelo de telefone no renderer.
- Tratar `MASK` como `BLEND` ou `OPAQUE` para simplificar pipeline.
- Ativar occlusion/LOD sem debug visual e bounds conservadores.
- Chamar acquire/present/CPU de “tempo GPU”.
- Declarar ganho usando PoC-A quando o problema reportado está em outra cena.
- Somar efeitos de Fase 7 antes de fechar o pipeline base M2 e seus budgets.

## 9. Relação com o plano principal

Este plano acelera o caminho para **M2 — cena PBR com sombras a 60 FPS no perfil A**
e prepara M7 sem inverter dependências. A prioridade imediata permanece nos itens
2.1.3/2.1.5/2.1.6, 2.2, 2.3 GPU real, 2.4 e 2.5. Os itens 7.1–7.6 entram somente
quando sua fundação correspondente estiver medida e correta. Céu procedural básico,
CSM, transparência correta e culling pertencem ao pipeline base; atmosfera Bruneton,
nuvens volumétricas, DDGI, VSM, AetherSR e VRS continuam evoluções da Fase 7. A
separação entre capabilities, calibração, Project Settings e estado térmico é
formalizada em [`ADR-014`](adr/ADR-014-POLITICA-GLOBAL-RENDERIZACAO.md).
