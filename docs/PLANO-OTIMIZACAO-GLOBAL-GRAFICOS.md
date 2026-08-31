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

> **Rota determinística, HZB com histerese e LOD por erro projetado — revisão
> e primeira validação Adreno em 31/08/2026.** Os três sistemas da "próxima
> ordem aprovada" (seção 6.3) foram integrados e revisados antes do A/B físico.
> Nenhum ganho de HZB/LOD é declarado e as duas flags continuam desligadas por
> padrão: a rota foi validada; HZB só fechou correção estática; o asset entregue
> ainda é AEMAP v2 e portanto não contém grupos de LOD.
>
> **Rota de câmera** (`native/platform/camera_route.h/.cpp`, formato
> `.aeroute`): indexada por *ordinal de frame renderizado*, nunca por tempo —
> `tickRateHz` é só metadado, porque o loop atual não tem fixed tick de
> simulação e indexar por relógio faria duas execuções amostrarem poses
> diferentes sob FPS distinto, destruindo a comparação A/B que é o único
> motivo de a rota existir. Grava fingerprint FNV-1a da cena (mesmo algoritmo
> do AEMAP) e recusa reproduzir uma rota gravada para outro mapa. Novos
> launch options `aether.camera_route_mode` (off/record/replay) e
> `aether.camera_route_path`; replay reaproveita o gate de input de
> `aether.lock_camera`. `FrameProfileContext` schema v2→v3 (`camera_mode`,
> `camera_route_fingerprint/frame_ordinal/tick_count`). 7 testes nativos.
>
> **HZB** (`native/renderer/hzb_visibility.h/.cpp` + integração em
> `InstancedRenderer`): pirâmide Hi-Z construída por uma **cadeia de passes
> gráficos pequenos** (fullscreen triangle + fragment de redução MAX,
> `R32_SFLOAT`), não compute — o repositório não tinha nenhum compute
> reutilizável, e TBDR móvel historicamente paga caro a transição
> gráfico↔compute no meio do frame. Primeiro nível é um *block-max* real
> sobre a resolução exata do depth (evita subamostrar e superestimar
> conservadorismo); níveis seguintes são halving 2×2 exato via `texelFetch`.
> Teste de oclusão com histerese configurável (`aether.hzb_hysteresis_frames`,
> padrão 3): revive instantâneo em qualquer frame não-ocluído, só cula após N
> frames consecutivos ocluído, nunca testado = visível. Leitura do readback
> no início do frame seguinte, no mesmo ponto onde `gpuFrameTimer_` já lê sem
> stall novo — a espera de fence do `acquireNextImage` (1 frame em voo) já
> garante a cópia do frame anterior terminada. `depthImage_` ganhou
> `VK_IMAGE_USAGE_SAMPLED_BIT`; `VulkanMemoryAllocator` ganhou
> `invalidateBuffer` (espelho de `flushBuffer` para leitura CPU de escrita
> GPU). Opt-in via `aether.hzb_occlusion`. 10 testes nativos cobrindo
> MAX-reduction, layout de readback, projeção esfera→retângulo de tela e as
> transições de histerese — um deles pegou um bug real de arredondamento de
> índice de texel antes deste texto ser escrito.
>
> **LOD** (`native/renderer/lod_selection.h/.cpp` + AEMAP v3 +
> `tools/cook-gltf-map.py`): fórmula padrão de erro projetado
> (`geometricError × alturaViewportPx / (2×distância×tan(fovY/2))`, mesma de
> 3D Tiles/Simplygon), seleção com banda de histerese assimétrica (refina
> imediatamente, só simplifica com margem extra). `MapDrawRecord` cresceu de
> 96 para 108 bytes (`lodLevel`, `geometricError`, `lodGroupId`); como o
> decoder fazia `reinterpret_cast` direto dos bytes do arquivo, crescer a
> struct exigiria reescrever esse cast — em vez disso `decodeMapPackage`
> passa a desempacotar campo a campo por versão, e pacotes v1/v2 decodificam
> com LOD default (nível 0, erro 0, grupo = o próprio índice), permanecendo
> 100% legíveis. Simplificador do cooker é *vertex clustering* determinístico
> (grid-snap por posição + guarda de descontinuidade de normal + guarda de
> ilha de UV, nunca funde através de uma costura), com blend e alpha-mask
> **excluídos da simplificação nesta fatia** — vegetação/cutout é exatamente
> o conteúdo mais arriscado de colapsar sem alguém checando visualmente, e
> blend tem ordem de primitiva que participa da composição. Build falha se
> `geometricError` não crescer estritamente por nível ou se os bounds do
> nível simplificado ultrapassarem os originais além de um épsilon. Dither de
> transição usa o slot `normalColumns[7]` de `GpuMeshInstance` — sempre zero
> hoje (`buildNormalMatrix` só grava sinal de handedness em `column0.w`), sem
> crescer a struct de 128 bytes nem a stride do vertex binding — testado
> contra uma matriz Bayer 4×4 no shading compartilhado de
> `dirt_road.frag`/`dirt_road_fallback.frag`. Seleção por `lodGroupId` roda
> por frame, antes do frustum culling, decidindo quais chunks entram como
> candidatos. Opt-in via `aether.lod_selection`
> (`aether.lod_pixel_error_budget`, `aether.lod_hysteresis_band_ratio`). 6
> testes nativos (fórmula, seleção, histerese, dither) + 10 testes Python do
> simplificador — um deles pegou um bug real de offset de byte na leitura de
> `MapMaterialRecord.flags` (lia o campo errado, quebrando silenciosamente a
> exclusão de blend/alpha-mask).
>
> **Correção paralela:** `tools/android-frame-profile.ps1` validava
> `schemaVersion` contra `1` enquanto o C++ já emitia `2` — todo contexto de
> profiling era rejeitado. Corrigido antes de estender o schema para v3.
>
> **Revisão de correção e evidência física:** a integração inicial comparava
> profundidade Vulkan normalizada com metros em view-space, descartava o depth
> com `storeOp=DONT_CARE`, ignorava a pré-rotação da Surface, tratava cada chunk
> espacial como um nível de LOD separado e usava duas máscaras de dither que se
> sobrepunham. Esses cinco defeitos foram corrigidos. O formato de depth sampled
> agora é detectado por capability e falha para o depth normal quando ausente.
> Em Debug no Xiaomi, HZB forçado ficou sem VUID e a captura foi bit a bit idêntica
> ao caminho sem HZB; essa pose aérea vale apenas como gate de correção, não como
> benchmark. A leitura CPU custou mais que a oclusão de um único chunk, então a
> política global ganhou `hzbMinimumCandidateDraws=128`; abaixo do limiar nenhum
> passe/readback é gravado e `hzb_budget_skipped_draws` explica o fallback. Em
> movimento, a pirâmide temporal só é usada se a pose coincidir; caso contrário
> falha aberta. O próximo HZB de produto deve ser same-frame/GPU-driven, sem
> readback CPU.
>
> **Baseline móvel reproduzível v11:** `forest-walk-v1.aeroute` contém 6.611
> poses (55,09 s a 120 Hz), percorre 487,16 unidades dentro do mapa e carrega o
> fingerprint `18faf0f1d9d8ee90`. A passagem completa Release mediu 95,16
> presents/s, pior janela de 600 frames em 84,18, CPU média 1,34 ms, GPU média
> 9,13 ms e pior GPU-p95 de janela 12,93 ms. O maior intervalo isolado equivaleu
> a 53,9 FPS, reproduzindo a classe de queda percebida pelo usuário sem usar o
> overview distante. O profiler agora anexa `route_frame`, draws e triângulos
> visíveis ao fechamento de cada janela para localizar o hotspot exato.
>
> **Experimento rejeitado no mesmo percurso:** reduzir globalmente chunks de
> alpha-coverage de 8.192 para 2.048 triângulos criou 125 em vez de 58 chunks,
> mas GPU média ficou 9,14 ms e a estabilidade não melhorou. A configuração por
> classe de material permanece reutilizável, porém o default volta a 8.192.
> Um segundo A/B intercalado mediu o próprio prepass seletivo na rota real:
> ligado 93,61 → desligado 80,89 → ligado 92,45 presents/s. Sem o prepass, a GPU
> média subiu de 9,32 ms (média dos controles) para 11,01 ms (+18,1%), enquanto
> CPU média ficou praticamente estável (1,48/1,45/1,66 ms), `thermalStatus=0` e
> temperatura 34,5→37,0 °C ao longo da sequência. Portanto o prepass de materiais
> `MASK` permanece ligado globalmente; removê-lo aumenta overdraw e não libera CPU.
> O fechamento da pior janela (`route_frame≈1734`) foi convertido em pose fixa
> `(-15,71,145,27,-25,72; yaw=2,75; pitch=0,11)`. Nela, três controles `full`
> ficaram em 85,92/83,97/84,01 presents/s e 10,37–10,39 ms GPU; `base-color`
> chegou a 120,08/6,08 ms. `no-normal` isolou 94,41/9,14 ms e `no-ibl`
> 87,47/9,98 ms, todos com CPU 1,28–1,59 ms e `thermalStatus=0`. Assim, o hotspot
> é fragment/material e o maior subcusto medido é normal mapping, não CPU.
> A primeira correção exata removeu `nonuniformEXT` dos índices bindless: esses
> índices vêm de push constants aplicadas uma vez por lote que já é agrupado por
> material, portanto são dinamicamente uniformes. O SPIR-V mantém
> `RuntimeDescriptorArray` e deixa de declarar `ShaderNonUniform`, sem mudar pixels.
> O v15 foi recompilado/instalado e medido na mesma pose: cinco janelas ficaram em
> 83,05–83,65 presents/s e ~10,38 ms GPU, dentro/levemente abaixo dos controles v14
> (83,97–85,92 e 10,37–10,39 ms). O Adreno já scalarizava esse acesso ou o custo é
> irrelevante; a correção semântica permanece, mas **nenhum ganho é declarado**.
> O runner agora recusa profiling `dirt-road` sem `CameraPose` ou rota Record/
> Replay, impedindo que a câmera de overview gere um FPS ilusório. Antes de HZB
> ou LOD virarem padrão ainda faltam A/B frio/aquecido, diff SSIM/FLIP, soak de
> 30 min, captura AGI do hotspot e Mali físico. Verificação corrente: 225/225
> testes C++ Release, 10/10 do simplificador LOD e 35/35 do FrameProfile.

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
`gpu_geometry_ms`, `gpu_background_ms` e `gpu_transparent_ms` (esses três nomes foram
substituídos pelas seis regiões do v4 em 31/08; ver o progresso do item 3 de O0). Em GPU móvel TBDR,
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

**Progresso em 31/08/2026 (item 3 — regiões de GPU por classe de passe):** o frame
deixou de ser medido em três baldes (`gpu_geometry_ms`/`gpu_background_ms`/
`gpu_transparent_ms`) e passou a ser medido em seis regiões declaradas em
`native/core/gpu_pass_class.h`: **Opaque, Coverage, Sky, Transparent, UI e HZB**.
Três buracos de atribuição foram fechados:

- **opaco sólido e folhagem alpha-mask estavam somados** no mesmo balde `Geometry`.
  A frente P3 recebe uma faixa própria de 0,5–1,5 ms no portfólio da seção 6.0, e
  não havia como verificar essa faixa enquanto vegetação e terreno compartilhavam
  uma única métrica;
- **o HUD não tinha marca nenhuma:** era desenhado depois da última marca do frame,
  então seu custo caía no intervalo não atribuído entre a última classe e o fim;
- **a cadeia de redução do HZB não tinha marca:** o custo do próprio mecanismo de
  visibilidade era invisível para o relatório que decide se ele paga o que custa.

Cada região é marcador de debug **e** timestamp, gravados sempre em par por
`beginGpuRegion`/`endGpuRegion`. O motivo é que os dois falham de formas opostas: um
marcador sem métrica produz captura que não fecha com o relatório, e uma métrica sem
marcador produz um número que a captura não consegue explicar. Antes desta fatia o
frame inteiro tinha **um** rótulo (`DirtRoad/map`), ou seja, a captura AGI pedida no
item 5 chegaria como um bloco único e não atribuiria custo a nada.

O fio da atribuição também mudou de forma: `RenderPhaseTimings` carrega um array
indexado por `GpuPassClass` em vez de campos nomeados, e os nomes de métrica são lidos
da mesma tabela canônica pelo perfil e pelos marcadores. Antes, as listas viviam em
paralelo em quatro arquivos e nada impedia que um rótulo de captura apontasse para uma
região diferente da métrica de mesmo nome — falha silenciosa, sem erro de compilação.
Um teste nativo e um teste PowerShell trancam essa equivalência.

**Formato de fio:** a janela `[FrameProfile]` já ocupava **937 dos ~1023 bytes** que o
Logcat entrega antes de truncar em silêncio, com apenas três passes. Somar seis
regiões estouraria o limite e corromperia toda janela. As regiões passaram então a um
registro próprio `[FrameProfilePasses]`, pareado por `pid/epoch/window` — mesma razão
pela qual `FrameProfileContext` já era emitido separado. O consumidor **exige** o par:
uma janela órfã é recusada, porque aceitá-la atribuiria 0 ms a opaco/folhagem/céu e
produziria um relatório aparentemente válido. Também recusa regiões que somem mais que
o tempo total de GPU do frame. Ambos os registros vão para `schemaVersion` 4.

**Verificação:** 226/226 testes C++ (+1), 39/39 PowerShell de FrameProfile (+4),
21/21 Python do cooker e 31/31 nas demais suítes de ferramentas Android;
`libaether_android.so` reconstruído pelo NDK real via `assembleDebug`. **Nenhuma
medição física foi feita nesta fatia** — não havia ADB conectado. As regiões são
instrumentação: elas não reduzem nenhum milissegundo e nenhum ganho é declarado.
Restam do O0 a captura AGI/APA (item 5) e o soak (item 6).

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

### P1 medido em hardware — 31/08/2026

O A/B intercalado saiu: o depth memoryless vale **0,44 ms de GPU (+4,8% ao
desligar)** e **4,4 fps**, com os dois controles reproduzindo dentro de 0,3%.
Detalhes e tabela em `PROFILING-ANDROID.md`. A faixa hipotética de P1 era
0,3–1,2 ms; o resultado fica na ponta baixa e **deixa de ser hipótese**.

Antes disso foi verificado o que a fatia original não checava: se o driver
concede memória `LAZILY_ALLOCATED` quando a política pede um anexo transitório.
O Adreno concede. Sem essa consulta a engine afirmaria economia a partir de uma
preferência que o device pode ignorar em silêncio.

São 0,44 ms dos ~2,9 ms que separam os 9,06 ms medidos do gate de 6,20 ms —
cerca de 15% do caminho, sem alterar um pixel.

### Progresso em 31/08/2026 (P1 — primeira fatia: grafo consumidor e depth memoryless)

O Render Graph deixou de ser código morto. Até aqui `aether_rendergraph` era
linkado **apenas por `aether_tests`**: os itens 2.3.1–2.3.5 estavam implementados e
testados, mas nenhum frame de produção passava por eles. Agora `aether_renderer`
depende dele e a política de anexos do frame é *derivada*, não escrita à mão.

**O que estava errado.** A mesma pergunta — "alguém lê o depth depois do render pass
principal?" — era respondida em três lugares independentes de
`instanced_renderer.cpp`: o `storeOp` do anexo, a flag `VK_IMAGE_USAGE_SAMPLED_BIT` da
imagem e a escolha de formato. Três cópias de uma política que precisa concordar. Se
divergirem, o resultado é erro de validação ou — pior — banda desperdiçada em silêncio,
que é exatamente o que este programa está tentando medir. As três agora leem
`FrameAttachmentPolicy`, resolvida uma vez por `native/renderer/frame_graph.cpp`.

**O ganho de banda que estava bloqueado.** Com HZB desligado (o padrão), o depth é
escrito e descartado dentro do mesmo render pass — ninguém o lê depois. O `storeOp` já
era `DONT_CARE`, mas a imagem continuava sendo alocada como render target comum,
ocupando DRAM que nada consumia. Ela agora recebe
`VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` e memória **preferencialmente**
`LAZILY_ALLOCATED`: numa GPU TBDR o anexo vive na memória do tile e pode não receber
lastro em DRAM nenhum. "Preferencialmente" e não "obrigatoriamente" porque nem todo
device expõe esse tipo de memória — sem ele a alocação normal continua correta, só não
economiza. Exigir transformaria uma otimização em falha de inicialização.

**Defeito encontrado no compilador do grafo.** A regra de memoryless exigia que todos
os passes tocando o recurso estivessem no mesmo grupo de subpass **fundido**
(`merged`, isto é, ≥2 passes). Isso excluía justamente o caso memoryless mais comum em
mobile — e o único que esta engine tem hoje: o depth de um forward renderer de passe
único. Nenhum anexo real conseguia `LAZILY_ALLOCATED`. A contenção no grupo já é
condição suficiente e não se confunde com o `storeOp`: num grupo fundido o consumidor
lê como input attachment, dentro do tile, então o recurso é "consumido" e mesmo assim
nunca vai à DRAM. Se um pass fora do grupo lesse o recurso, ele seria o `lastUse` e
cairia em outro grupo, reprovando a checagem. A regra perdeu o `merged`; o teste do
GBuffer fundido continua passando.

`isImageDescValid` também passou a recusar transitório combinado com
SAMPLED/STORAGE/TRANSFER (VUID-VkImageCreateInfo-usage-00963), transformando um erro
de device num erro de contrato, no arquivo onde a política é escrita.

**Verificação:** 231/231 C++ (+5), 39/39 PowerShell de FrameProfile, 21/21 Python e
31/31 nas demais suítes; `assembleDebug` real pelo NDK. Um teste de mutação confirmou
que `frame_graph_sem_leitor_o_depth_nao_vai_a_dram` falha ao restaurar a regra antiga,
ou seja, a economia depende de verdade da correção. **Nada medido em hardware** — sem
ADB nesta sessão. A hipótese de P1 (0,3–1,2 ms) continua hipótese: o A/B com captura de
frame é o que a promove, e o log `[FrameGraph] depth ...: memoryless=sim` no aparelho é
a primeira coisa a conferir.

O que P1 ainda não fez: fundir passes de verdade, eliminar targets intermediários (não
há nenhum hoje além do depth) e usar render passes nativos com subpasses. As cinco
classes do frame vivem num único render pass, então não há fusão a ganhar antes que
CSM ou pós introduzam um segundo alvo.

### Progresso em 31/08/2026 (P3 — primeira fatia: semântica de cobertura e mips que a preservam)

**Defeito medido, não suposto.** O cooker gerava mips com box filter uniforme,
aplicado também ao canal alpha. Box filter preserva a **média** do alpha; um material
alpha-tested só enxerga a **fração de texels acima do cutoff**. As duas divergem
rápido. Medido no próprio cooker, sobre um atlas sintético de galhos:

| mip | cobertura vs. nível base |
|---|---:|
| 1 | 130,3% |
| 2 | 132,8% |
| 3 | 45,3% |
| 4 | 14,4% |
| 5 | **0,0%** |
| 6 | **0,0%** |

Ou seja: a vegetação **desaparece por completo** com a distância, e ainda sobra ~30%
nos dois primeiros níveis. Num atlas de folhas anti-aliased (mais próximo do real) a
cobertura ainda cai a zero no mip 6. Isso não é um detalhe estético — a seção 8 proíbe
explicitamente "remover árvores", e era exatamente o que o pipeline de assets fazia,
em silêncio, a cada cozimento.

**Semântica de cobertura versionada, vinda do material.** `coverage_alpha_cutoffs`
mapeia textura base-color → cutoff a preservar, decidido pelos **materiais** que a
amostram: `MASK` explícito do glTF, ou `BLEND` que a heurística de atlas já reconhecia
como recorte. Nunca por nome de textura ou de cena (ADR-014 e seção 8). Quando dois
materiais compartilham a textura com cutoffs diferentes, o **menor vence**: nenhuma
cadeia de mips é correta para dois cutoffs, e errar para o lado do menor preserva
texels a mais — folhagem levemente densa custa fragmentos, folhagem de menos remove
árvores. Só a base color recebe o tratamento; normal/ARM/emissive de um material
recortado não são alpha-tested.

**Correção.** Algoritmo de Castaño/NVIDIA, o mesmo do "Mip Maps Preserve Coverage" da
Unity: busca binária do multiplicador de alpha que faz o mesmo cutoff render a mesma
cobertura do nível base. Dois detalhes que importam:

- a escala sai da cópia de saída, não da cadeia usada para reduzir — reescalar antes
  de reduzir empilharia o erro nível a nível;
- a busca devolve o **limite superior convergido**, não a última sonda. Cobertura é uma
  função **escada** da escala (o alpha de um atlas de recorte é quase binário, e o box
  filter de 2×2 produz poucos valores distintos), então o alvo em geral cai entre dois
  degraus e nenhuma escala o atinge exatamente. Devolver o degrau de cima garante que
  um mip nunca sai com menos cobertura que o base. Devolver a última sonda foi
  literalmente o primeiro bug desta fatia: entregava 69% do alvo em vez de 100%.

Resultado no atlas anti-aliased: cobertura mantida entre **100% e 108%** até o mip 6,
onde antes era 0%.

**Gate de cozimento, não aviso.** O build agora falha se uma textura de cobertura
produzir um mip com cobertura zero. O defeito original chegou ao APK justamente por
ser silencioso; um warning teria o mesmo destino.

**Versionamento.** O manifesto passa a registrar, por textura, `alphaSemantics`,
`coverageCutoff` e `coverageByMip`. Sem isso, "a folhagem sumiu no mip 5" só aparece
olhando a imagem no aparelho.

**Limitação importante:** a fonte do mapa (`update_dirt_road_through_forest.zip`) não
está no repositório, então **o asset atual não foi recozido** — o `scene.aemap`
empacotado hoje ainda carrega os mips defeituosos. Esta fatia corrige o *pipeline*; o
ganho visual e de fill-rate só existe depois do recook, que é a mesma dependência de
"recuperar a fonte" já registrada para o AEMAP v3/LOD de P2. As duas devem sair no
mesmo recook.

**Verificação:** 36/36 Python (+15), 231/231 C++ e 70/70 PowerShell. Nada medido em
hardware. A faixa de 0,5–1,5 ms de P3 continua hipótese: ela depende de overdraw
medido, e overdraw de folhagem só muda depois do recook.

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
| **G4 — Visibilidade** | BVH, frustum, HZB, instancing, LOD/HLOD conservador | G0–G1 | custo proporcional ao visível, sem pop/sumiço |
| **G5 — Margem móvel** | Render Graph GPU, cache, mobile passes, memoryless e compactação GPU | G0–G4 | aparelho forte: rota GPU média ≤6,20 ms, p95 ≤7,00 ms, p99 ≤8,00 ms e CPU p95 ≤3 ms |
| **G6 — Sustentação** | soak, perfis, governor e caminho 90/120 Hz | G5 | 60 FPS/30 min ≤4 W no perfil A |

G1 e G2 podem avançar em paralelo depois de G0. G3 depende da semântica correta de
alpha; G4 não pode ser aprovado enquanto houver desaparecimentos sem diagnóstico.

## 6. Ciclos de implementação por retorno e dependência

### 6.0 Programa de margem — portfólio, não aposta única

O baseline de 31/08/2026 exige uma mudança de escala: 9,13 ms de GPU média na rota e
~10,38 ms no hotspot precisam cair para 6,20/6,50 ms. O objetivo não é mostrar “120”
quando a câmera para ou sai do mapa; é recuperar aproximadamente **3 ms de trabalho
GPU real** e manter p99 abaixo de 8 ms enquanto o jogador percorre a floresta.

Essa margem será buscada por um portfólio de intervenções. As faixas abaixo são
hipóteses de planejamento, não ganhos prometidos, não são aditivas e precisam de A/B:

| Frente | Problema que resolve | Hipótese no hotspot | Gate antes de promover |
|---|---|---:|---|
| P0 — captura GPU | impede escolher gargalo por intuição | instrumentação, sem ganho contado | AGI/APA com markers e counters na rota/hotspot |
| P1 — passes/attachments | tráfego externo, stores e targets inúteis em TBDR | 0,3–1,2 ms | Render Graph, load/store e memoryless validados por frame capture |
| P2 — visibilidade/LOD/HLOD | triângulos e pixels sem contribuição | 1,0–2,5 ms em vistas densas | fonte recooked, erro projetado e diff sem pop |
| P3 — foliage/coverage | overdraw, alpha test e cartões distantes | 0,5–1,5 ms | heatmap, coverage preservada e mesma silhueta próxima |
| P4 — bandwidth/material | fetch, mips, registers e residency | 0,3–1,0 ms | counters atribuem custo; SSIM/FLIP e memória passam |
| P5 — pacing/thermal | jitter, filas e comportamento diferente com gravador | estabilidade, não ms GPU | frame timeline, latência e soak; sem “ganho” fictício |
| P6 — reinvestimento | mais sombras, luz, céu, densidade e efeitos | consome a reserva | qualidade nova mantém todos os gates de G5/G6 |

#### Ordem executável e dependências

1. **Congelar a verdade reproduzível.** Manter a rota `forest-walk-v1`, hotspot dentro
   do mapa, APK/hash, pose, resolução e temperatura. Adicionar regiões GPU para
   coverage, opaque, sky/UI, HZB e post; capturar um frame AGI e um intervalo Android
   Performance Analyzer. Nenhuma refatoração grande recebe crédito sem essa atribuição.
2. **Fechar o caminho mobile do frame.** Ligar o Render Graph aos consumidores reais;
   eliminar attachment/intermediate sem leitor; escolher `loadOp/storeOp` pelo
   lifetime; experimentar transient/memoryless e render passes nativos. O coverage
   prepass continua ativo porque o A/B já provou +18,1% de GPU ao removê-lo; depth
   prepass geral não é presumido como bom em tile-based renderer.
3. **Fazer o custo acompanhar o visível.** Recuperar a fonte, produzir AEMAP v3 com
   LODs e HLOD/células, validar o erro em pixels e integrar HZB same-frame → compactação
   → indirect count. O caminho CPU/readback permanece diagnóstico e desligado quando
   não amortiza custo. Malha carregada não equivale a malha enviada.
4. **Tratar vegetação como classe de material, não como nome de cena.** Versionar
   semântica foliage/coverage/double-sided; gerar mips alpha coverage-aware; medir
   overdraw; agrupar próximos e usar HLOD/impostor distante. Transparência distante só
   entra se o fill-rate medido for menor que a alternativa geométrica.
5. **Otimizar assets e shaders conforme counters.** Aplicar ASTC por semântica,
   mip/residency budgets, cache/index/mesh optimization e material LOD por erro
   projetado. Isolar fetch da normal, TBN, IBL e register pressure continua útil, mas
   é uma subfrente P4 — não dita sozinho todo o roadmap.
6. **Corrigir estabilidade sem mascarar custo.** Adotar timing/pacing Android,
   considerar 2–3 frames em voo somente com recursos isolados, medir gravador on/off,
   thermal headroom e Game Mode. Nunca criar carga artificial ou gancho privado de
   clocks; o alvo é desempenho sustentável e repetível.
7. **Reinvestir com orçamento explícito.** Quando a cena-base alcançar G5, gastar a
   reserva medida em SH/IBL, CSM cacheada, contato, nuvens, partículas e densidade. Cada
   feature recebe budget de GPU/memória/potência e fallback por capability, não regra
   por cena.

#### Gates de aceitação por ciclo

- rota em movimento e hotspot dentro do mapa; benchmark parado é apenas diagnóstico;
- 3 runs frios + 3 aquecidos, janelas móveis, p50/p95/p99 e frame timeline;
- imagem por SSIM/FLIP mais inspeção de silhueta, foliage, estrada e céu;
- Adreno forte + Mali físico C; AVD valida somente lógica/fallback/lifecycle;
- 60 s por alteração e soak de 30 min para promoção global;
- registro do ganho individual e combinado; se o combinado não repetir, bisectar
  interação de cache, bandwidth, pass ou temperatura antes de seguir.

#### Referências oficiais que orientam a arquitetura

- Unreal separa caminhos de render móvel, perfis/escalabilidade, cache de PSO,
  auto-instancing e profiling por ferramentas nativas de cada GPU.
- Unity URP expõe render scale/shadows, mas também recomenda remover depth/opaque
  textures desnecessárias, usar store actions corretas, native render passes, SRP
  Batcher e Render Graph com pass/resource culling.
- Godot combina frustum, occlusion, mesh LOD, HLOD/visibility ranges, impostors e
  instancing; também alerta que billboards transparentes podem trocar geometria por
  fill-rate.
- Android recomenda ADPF/thermal headroom com ajustes granulares e independentes,
  testes sustentados, AGI para frame profiling e Frame Pacing para apresentação suave.

Fontes: [Unreal mobile performance](https://dev.epicgames.com/documentation/unreal-engine/performance-and-optimization-for-mobile-in-unreal-engine?lang=en-US),
[Unreal Android profiling](https://dev.epicgames.com/documentation/unreal-engine/profile-android-projects-with-platformnative-tools?lang=en-US),
[Unity URP performance](https://docs.unity3d.com/cn/6000.0/Manual/urp/configure-for-better-performance.html),
[Unity Render Graph](https://docs.unity3d.com/cn/6000.0/Manual/urp/whats-new/urp-whats-new.html),
[Godot 3D optimization](https://docs.godotengine.org/en/stable/tutorials/performance/optimizing_3d_performance.html),
[Godot HLOD](https://docs.godotengine.org/en/stable/tutorials/3d/visibility_ranges.html),
[Android ADPF](https://developer.android.com/games/optimize/adpf/best-practices-adpf?hl=en),
[Android AGI](https://developer.android.com/agi/frame-trace/frame-profiler?authuser=14) e
[Android Frame Pacing](https://developer.android.com/games/sdk/frame-pacing?authuser=77&hl=en).

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
5. **Fatia CPU/readback entregue apenas como referência:** HZB temporal com
   histerese e telemetria. A câmera móvel falha aberta e o gate de 128 candidatos
   evita alocação/store/readback quando a carga não paga o custo. O caminho de produto
   pendente é HZB same-frame/GPU-driven em dois passos, com counters de falso
   positivo/negativo e sem leitura CPU.
6. **Seleção/runtime entregue, conteúdo pendente:** LOD por erro projetado em pixels,
   transição dither complementar e múltiplos chunks por nível. O pacote atual é AEMAP
   v2/zero grupos; gerar LOD exige recook v3 a partir da fonte original e gate visual.

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

1. **Entregue:** rota determinística `forest-walk-v1` de 55,09 s/6.611 poses,
   atravessando 487,16 unidades dentro do mapa; o benchmark parado fica apenas como
   diagnóstico.
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
5. **Entregue como gate conservador, não promovido a default:** replay por frame e HZB
   temporal com histerese/estado “incerto = visível”. No mapa atual, apenas 54
   candidatos e uma oclusão não pagaram o readback; abaixo de 128 candidatos nem os
   recursos HZB são alocados. Substituir por HZB same-frame/GPU-driven antes de produto.
6. **Runtime entregue; asset pendente:** LOD por erro projetado em pixels, preservação
   de coverage e dither temporal. O AEMAP v2 atual não contém grupos; recook v3 da
   fonte original e diff visual são obrigatórios. Nenhum LOD é escolhido por nome de
   cena ou modelo de aparelho.
7. Repetir em Adreno forte, Mali físico classe C e AVD `Aether-C-Synthetic`. O AVD
   valida rotação, input, lifecycle, fallback e memória; não certifica FPS, clocks,
   bandwidth ou comportamento térmico do Galaxy A32.

**Ordem anterior substituída pelo programa 6.0:** normal fetch/TBN/IBL permanecem
variantes diagnósticas de P4, mas não centralizam o roadmap. A ordem agora é captura
AGI/APA → passes/attachments móveis → fonte + AEMAP v3/LOD/HLOD → HZB same-frame e
compactação GPU → foliage/overdraw → assets/shaders atribuídos por counters → pacing e
soak → reinvestimento visual. Dois frames em voo só entram depois que recursos por
frame forem isolados e o tempo GPU estiver abaixo do budget; eles podem esconder espera
da CPU, mas não reduzem o custo do pior quadro.

**Status em 31/08/2026:** rota e correção estática do HZB foram validadas no Xiaomi
físico; LOD compilou e foi testado, mas não pode atuar porque o asset empacotado ainda
é AEMAP v2/zero grupos. A rota completa mediu 95,16 presents/s e expôs uma janela
isolada equivalente a 53,9 FPS. O prepass `MASK` também foi confirmado por A/B móvel:
desligá-lo perdeu 13,0% de presents/s e adicionou 18,1% de GPU. Permanecem pendentes
AGI no hotspot, recook v3, SSIM/FLIP, soak de 30 min e Mali físico.

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
