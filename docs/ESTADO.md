# Estado da execução — o que existe de verdade

Instantâneo do repositório contra o roadmap de `PLANO-ENGINE-MOBILE.md`.
Regra: só entra nesta tabela o que **compila e passa em teste**.

O plano de resolução das lacunas deste instantâneo está em
`PLANO-FECHAMENTO-LACUNAS.md`. Ele define prioridades, dependências, migrações,
testes em hardware e critérios de aceite sem substituir o roadmap principal.

## Estabilização gráfica — 28/08/2026

Contrato bindless/fallback corrigido e validado no Adreno com Khronos validation
ativa. O renderer agora respeita as capabilities, consulta os limites do array
e funciona sem descriptor indexing habilitado no device. PoC-E reproduzível
por opção de lançamento, com worker/device exclusivos, barreiras e cleanup
corrigidos, timestamps válidos e comparação integral de 16.777.216 texels:
**2,829844 ms GPU**, erro máximo **7/255** no corpus sintético RGB opaco.
O probe completo debug levou **2.393,012655 ms**; não é tempo de importação.

Evidência e sequência de integração: `EXECUCAO-EDITOR-ANDROID.md`.
`tools/validate-android-rendering.ps1` passou nos dois caminhos;
`stabilization-lifecycle-20260828/report.json` passou em 3 retomadas e mudança
de configuração. Não foi retomado soak de 30 minutos. Editor integrado, UI
retida/GPU, Inspector conectado e Play isolado continuam pendentes.

PoC-D permanece **parcial**: 12,8 ms mediam troca de DLL pré-compilada, não o
fluxo editar/compilar/aplicar/ver no celular. 2.1.4, 2.1.6 e 2.1.7 também
permanecem parciais no escopo completo, apesar dos caminhos já validados:
faltam tabelas globais restantes, captura AGI/RenderDoc inspecionada e
detecção/matriz completas de GPUs, respectivamente.

## Integração cena/render — 28/08/2026

Implementados `MeshRenderer` com referências ResourceId persistentes,
serialização de Guid, extração de matrizes afins completas e tint em lote,
ABI C#/C++ de 88 bytes e apresentação Vulkan com depth/perspectiva.
`Aether.Rendering` é a raiz do componente hospedado, sem Core depender do
renderer. A PoC-A de 5.000 instâncias continua separada e disponível.

15 testes novos cobrem ABI, hierarquia/escala não uniforme, entidades
removidas/recriadas, buffer insuficiente sem escrita parcial, recursos
desconhecidos, dados inválidos, zero alocação e round-trip texto/binário.
Android debug/release/lint e shaders foram compilados/validados.

O cubo/checker continua como fixture isolada; o launcher corrente já possui os
vertical slices PBR/esfera e mapa real descritos abaixo. Isso ainda não equivale
a Asset Browser, Inspector, save/load pela UI ou Play isolado.
Contrato e evidências: `SCENE-RENDER-INTEGRATION.md`.
`scene-snapshot-20260828/report.json` passou em bindless/fallback com
Khronos validation ativa e estado preservado após retomada. Capturas foram
revisadas: objetos idênticos; diferenças restritas ao indicador lateral OEM,
mantidas explicitamente no relatório como desigualdade da tela completa.
### Esfera PBR 8K — lote seguinte implementado

O launcher agora abre uma esfera de 36.480 triângulos com albedo/normal/ARM
**8192×8192 reais**, ASTC 6×6 e 14 mips, iluminação GGX/Burley/Fresnel e IBL
especular de estúdio pré-filtrada offline. IDs/JSON v1 de material, RHI de
cadeias de mip, fallback 1K RGBA8 e limite de residência por quota/capability.
Carga em worker exclusivo, com cancelamento antes de teardown da surface.

`material-final-20260828/report.json`: bindless, descritores convencionais e
fallback de textura passaram em Android, com criação/remoção/transform e
snapshot preservado na retomada. Cubos e PoC-A também passaram nas regressões.
HOME durante carga exercitou cancelamento e retomada no mesmo processo.
Texturas ASTC ocupam ~114 MiB de payload, não a memória total do app; carga
debug observada ~1,5–1,9 s. 16K não foi ativado. Inspector, multiscatter,
SH/probes, sombras e pós completo continuam pendentes; M2 continua aberto.
Detalhes e evidência: `MATERIAL-PREVIEW.md`.
Build otimizado instalado preservando dados. Checkpoint curto da esfera:
120,11 FPS exibidos/5,14 s; CPU média por janela 1,15–1,21 ms, pico 4,37 ms.
Não é medição de Sponza/PoC-A, tempo GPU ou aceite térmico/energético.

### Mapa glTF real + câmera livre — vertical slice implementado

`[UPDATE] Dirt Road Through Forest`, de 99.Miles (CC BY 4.0), foi importado
offline pelo `tools/cook-gltf-map.py` para o formato versionado AEMAP v1 e AETX
v1. O runtime não interpreta JSON nem decodifica imagens: carrega 27 draws,
26 materiais, 424.849 vértices, 341.109 triângulos e 70 texturas, com buffers
de geometria device-local via staging. ASTC 6×6 preserva fontes até 4096² e o
fallback RGBA8 é limitado a 512 px; ambos têm cadeias completas de mip.

O renderer usa dois pipelines: opacos com depth write e transparências com
blend, depth read e ordenação draw-level traseira→frontal. PBR móvel cobre
base color/factor, normal/scale, metallic-roughness/factors, emissive, cor de
vértice, UV0/UV1 e fator especular. Bindless e descritores convencionais por
material foram exercitados no mesmo Adreno. A carga observada foi 1,19–1,57 s;
memória RHI após carga: ~34,7 MiB de buffers, ~60,9 MiB de texturas e ~14,3 MiB
de render targets. Não é ainda streaming/LOD/culling, sombras ou pós-processo.

A câmera livre é um controlador portátil independente de Android/Vulkan: um
dedo olha, arrasto com dois dedos desloca lateralmente e sobe/desce, pinça
avança/recua; mudanças no conjunto de pointer IDs recalibram sem salto. Posição
e orientação sobrevivem à recriação da surface. No hardware real, o mapa
apresentou o primeiro frame e um swipe alterou yaw de 0 para 1,360 rad; pan e
pinça possuem regressão nativa determinística. APK debug instalado com `-r`,
sem desbloqueio automático nem mudança de timeout global.

Artefatos e atribuição: `samples/dirt-road/README.md`, `manifest.json` e
`LICENSE.txt`. A validação Python confere identidade, todos os SHA-256,
estrutura/contagens AEMAP, formatos, mips e limite do fallback.

### Primeira otimização global da floresta — integrada em 29/08/2026

O shader da floresta não calcula mais `inverse(mat3(model))` por vértice. O renderer
genérico prepara `GpuMeshInstance` com uma normal matrix de três colunas uma vez por
draw/transform, incluindo sinal de handedness para tangentes; o mapa é o primeiro
consumidor, não uma condição de política gráfica.
O ABI público `RenderInstance` de 88 bytes não mudou. Matrizes singulares/NaN falham
na carga; escala não uniforme, escala pequena e espelhamento possuem regressão nativa.

O pacote AEMAP agora expõe fingerprint estável dos bytes carregados. Em profiling, o
runtime publica cena real, fingerprint, câmera, resolução e contagens; o runner recusa
rótulo de cena falso e preserva o contexto incrementalmente. O AVD sintético C também
foi materializado como perfil JSON e criador idempotente para Android Studio, mantendo
explicitamente `performanceCertification=false`. O `Aether-C-Synthetic` ARM64 foi criado
no Device Manager local com 2 cores/4 GB/1080×2400; não foi iniciado porque o host é x64
e não oferece aceleração de CPU para a ABI ARM64 exclusiva do APK.

O APK Release assinado foi então medido no Xiaomi `25053PC47G`/SM8735/Adreno, Android 16,
na cena e câmera travadas em `dirt-road`, fingerprint `dd907ec34bbebc21`, pose
`0,160,-100,0,0.08`, 2772×1280, 27 draws, 70 texturas e 341.109 triângulos. Com voto de
120 Hz confirmado pelo Android (`mActiveModeId=1`), 60 s úteis produziram 5.400 frames em
69,121 s: **78,124 presents/s**, pior janela de 600 frames **73,431**, CPU de processo
média **1,408 ms**/p95 de pior janela **2,643 ms** e GPU média **11,396 ms**/p95 de pior
janela **18,409 ms**. Bateria 38,3→40,0 °C, status térmico 0 e alimentação externa; não é
soak nem medição de potência válida. Isso localiza o limite na GPU/variância da geometria:
a CPU possui folga, mas o p95 GPU não cabe nos 8,33 ms de 120 Hz e ainda excede o orçamento
confortável de 60 Hz. O relatório longo está em
`build/android-validation/dirt-road-close-release-120hz-60s-20260829/report.json`.

O coletor SurfaceFlinger anterior perdeu trechos da janela circular ao disputar ADB com
logcat e potência, portanto essa série longa permaneceu corretamente inválida. O runner
agora usa coleta concorrente, aceita somente sobreposição/fronteira adjacente e continua
invalidando perda real; também aceita serial mDNS republicado com sufixo `(2)`. O smoke
posterior passou com 16,784 s contínuos de `actualPresentTime`. Essa cadência mede frames
novos da layer tornados visíveis, mas usa janela independente da captura nativa; as duas
taxas só podem ser comparadas quando os intervalos estiverem temporalmente alinhados.

Backface culling também foi prototipado como política global por semântica de material e
handedness. A versão conservadora manteve `MASK` AEMAP v1 dupla face e ficou estável
dentro de cada execução, mas a comparação cruzada ainda mostrou terreno/folhagem removidos:
334.281 de 3.548.160 pixels (9,42%) divergiram do baseline. O A/B físico consecutivo também
regrediu de 97,463 para 86,339 presents/s, GPU média de 9,035 para 9,983 ms e pior p95 de
9,891 para 13,652 ms, com `thermalStatus=0`. A implementação foi retirada e o resultado
negativo foi registrado em `PROFILING-ANDROID.md`; uma nova tentativa requer captura AGI e
contrato de cobertura versionado, não ajuste por cena.

O diagnóstico seguinte passou a isolar custo de fragment como variante compilada de
pipeline Vulkan, não como flag de qualidade. O modo é validado no launch, propagado ao
renderer por enum, registrado no contexto do perfil e resolvido por specialization
constant; `full` é sempre o default/fallback. No A/B físico intercalado
`full → base-color → full`, mesma cena/fingerprint/câmera/APK, `base-color` não melhorou:
GPU média 9,715→10,080→11,040 ms e SurfaceFlinger 93,446→88,507→91,612/s, com
`thermalStatus=0`. Isso evita remover detalhes gráficos sem evidência e move o próximo
passo para atribuição de vertex/raster/tiles, visibilidade e frame pacing. A matriz
dinâmica anterior ficou invalidada para atribuição porque seu controle `full` oscilou
90,808→70,786 presents/s.

O primeiro ajuste orientado por esse diagnóstico é o AEMAP v2: posição/UV permanecem
float32, normal/tangente usam SNORM16 e cor UNORM8. O stride global caiu 72→48 bytes,
com decoder v1 preservado e rejeição de versão/layout mistos. `scene.aemap` caiu
34.688.380→24.491.996 bytes e o APK 402.443.803→392.244.763. Em dois runs Adreno,
GPU média foi 8,887/7,420 ms, engine 99,395/117,294 presents/s e SurfaceFlinger
110,202/111,855/s. A comparação v1/v2 mudou só 523/3.548.160 pixels, máximo 1/255.
É evidência inicial; A/B longo, soak e Mali permanecem obrigatórios.

### Céu/ambiente global — 30/08/2026

O `sunset_forest` 4096×2048 RGBA16F (~89,5 MB), que aparecia como árvores
esticadas no fundo, saiu do caminho ativo. `tools/cook-sky-panorama.py` cozinha
`samples/dirt-road/Source/day-clouds-panorama-v1.png` para AETX 1024×512 sRGB
com 11 mips (~2,80 MB) e seam horizontal; o céu faz um lookup direcional e foi
validado no aparelho em yaw 0°/90°. A tentativa procedural full-screen caiu para
84,82 FPS e foi descartada; a versão de uma amostra mediu 117,27 FPS sob boost.

AEEN v2 possui 144 bytes e serializa a configuração global de sol, ambiente,
exposição, céu e ground bounce; AEEN v1/80 bytes segue legível por migração. O
shading usa irradiância hemisférica sem passe/draw/fetch extra e elevou a
luminância média da vegetação na pose fixa de 37,34 para 45,18. CSM, GTAO, SH,
probe especular separado e exposição temporal continuam abertos.

O comportamento relatado durante gravação foi reproduzido: após o governador
relaxar, a engine observou ~98,72 Hz; com `adb screenrecord` ativo no mesmo APK e
câmera, o SurfaceFlinger mediu 112,04 FPS, com `Thermal Status: 0`. Não há cap de
60 na engine. O runner futuro deve registrar estado de gravação/GameTurbo e votos
de energia como contexto, sem regras por fabricante ou aparelho.

Validação desta fatia: CMake Release limpo e **180/180 C++**, **26/26** contratos de
FrameProfile, **2/2** planos AVD, SPIR-V reproduzível, além de Android Debug + Release
+ Lint offline. O build Windows precisou ampliar para GCC as supressões já existentes
somente em volta da implementação vendorizada VMA; `-Werror` permanece nos fontes da
engine.

### Benchmark FPS com personagem e colisão — 30/08/2026

O mapa real agora pode ser percorrido com joystick flutuante inferior esquerdo e
look simultâneo na metade direita. `FirstPersonController` produz ações; a câmera
não contém gameplay. `CharacterMotor` possui mundo Jolt, cápsula, gravidade e fixed
step, consumindo `StaticCollisionMesh` do renderer. O conversor aplica a matriz
mundial de cada draw AEMAP, compacta vértices e reverte winding na fronteira de
asset. Materiais BLEND continuam físicos; alpha-mask é não físico por default e
flags `MapMaterialNoCollision`/`MapMaterialForceCollision` permitem override.

No Xiaomi, a colisão final carregou 188.681 vértices e 156.119 triângulos. O log
registrou Y entre 148 e 157 durante deslocamento e `ground=0` sobre a estrada, em
vez dos valores negativos crescentes da falha anterior. O HUD Vulkan fica no topo;
o shader v5 remove a reflexão local que invertia a ordem e corrige o bit-column das
fontes 3×5. O SPIR-V foi validado e o APK assinado, mas a inspeção visual v5 ficou
pendente porque o aparelho descarregou. A suíte completa anterior passou **185/185**;
depois da política BLEND/cutout, o teste focal passou **4/4**.

## Rota determinística, HZB e LOD — 31/08–01/09/2026

A rota e o HZB conservador continuam disponíveis; HZB permanece diagnóstico opt-in
porque o readback CPU não é a arquitetura final. O LOD, porém, fechou uma fatia física:
o asset de produção agora é AEMAP v3, preserva exatamente o LOD0 e a colisão e contém
245 grupos espaciais. São 635 draws/341.109 triângulos no nível autoral, 245/126.033 no
nível 1 e 238/66.118 no nível 2. Os níveis derivados reutilizam os 424.849 vértices de
origem; o pacote inteiro caiu de 800.321 para 533.260 triângulos armazenados.

O cooker particiona por célula mundial de 64 m e teto de 2.048 triângulos, simplifica
malha sólida por clustering com proteção de normal/UV e trata alpha-mask por densidade
de componentes desconectados. BLEND mantém ordem autoral e não é simplificado. O runtime
seleciona sólido e coverage por budgets independentes (`lodPixelErrorBudget` e
`coverageLodPixelErrorBudget`), aplica histerese/dither antes do frustum e calcula erro
com a resolução interna ativa, inclusive sob resolução dinâmica. LOD desligado significa
LOD0, nunca todos os níveis simultâneos. Colisão e sombra também usam somente o LOD0;
isso remove geometria duplicada dos picos de atualização do cache de sombra.

O A/B Release a 120 Hz, na mesma rota/fingerprint, confirmou ganho pequeno mas real no
par aquecido: coverage 32 ficou em 109,49 FPS exibidos/7,45 ms GPU contra LOD0 em
107,78/7,61 ms; pior janela 103,11 contra 101,47 FPS. Coverage 48 reduziu mais geometria
(323.407 contra 381.575 triângulos submetidos no contexto inicial) e mediu 111,01 FPS/
7,37 ms, mas a ordem térmica impede atribuir toda a diferença ao budget. Uma passada
menos aquecida em 32 alcançou 118,94 FPS exibidos/6,43 ms GPU; é teto observado, não
média garantida. `thermalStatus` permaneceu 0.

Na pose fixa do hotspot, LOD0 e coverage 48 sustentaram ~120 FPS e preservaram a imagem
visualmente; a comparação pixel a pixel teve MAE 2,00/255 e RMSE 5,40/255 (dither/ruído
temporal impedem igualdade binária). Ainda faltam SSIM/FLIP em várias poses, soak de
30 minutos e Mali físico antes de transformar esse teste numa matriz de produto. A
validação corrente passa 267/267 testes nativos, 46/46 testes Python e 44/44 testes do
FrameProfile. O APK Release validado tem SHA-256
`5251A387C6CEC81207242AC5A2015BE05B4F5E709288137DB8AC7280C4F86D30`.

## Regiões de GPU por classe de passe — 31/08/2026

Primeira fatia do programa de margem gráfica (P0 — verdade de GPU). O frame passou
de três baldes de timestamp para **seis regiões** — Opaque, Coverage, Sky,
Transparent, UI e HZB — declaradas uma única vez em `native/core/gpu_pass_class.h`
e consumidas pelo timer, pelo relatório de perfil e pelos marcadores de captura.

Fechou três buracos de atribuição reais: opaco sólido e folhagem alpha-mask estavam
somados no mesmo balde (a folhagem tem faixa própria no portfólio da seção 6.0 e não
havia como verificá-la); o HUD não tinha marca nenhuma e caía no intervalo não
atribuído no fim do frame; e a cadeia do HZB também não tinha marca, deixando
invisível o custo do próprio mecanismo de visibilidade. O frame inteiro também tinha
**um** rótulo de debug (`DirtRoad/map`), então a captura AGI prevista chegaria como
bloco único.

O registro `[FrameProfilePasses]` foi separado da janela porque esta já ocupava 937
dos ~1023 bytes que o Logcat entrega antes de truncar em silêncio. O par é
obrigatório: janela órfã é recusada em vez de virar 0 ms por região.

Verificação: **226/226** C++, **39/39** PowerShell de FrameProfile, **21/21** Python
do cooker e 31/31 nas demais suítes Android; `libaether_android.so` reconstruído pelo
NDK real. **Nada foi medido em hardware** — não havia ADB nesta sessão. Regiões são
instrumentação e não reduzem nenhum milissegundo; nenhum ganho é declarado.

## Render Graph consumidor e depth memoryless — 31/08/2026

Segunda fatia do programa de margem (P1 — pipeline móvel). O Render Graph saiu de
código morto: até aqui `aether_rendergraph` era linkado apenas por `aether_tests`,
com 2.3.1–2.3.5 implementados e testados mas fora de qualquer frame de produção.
Agora `aether_renderer` depende dele e a política de anexos do frame é derivada.

Três decisões que precisavam concordar — `storeOp` do depth, `SAMPLED_BIT` da imagem
e escolha de formato — eram escritas à mão em três pontos de
`instanced_renderer.cpp`. Passaram a ler um `FrameAttachmentPolicy` único, resolvido
por `native/renderer/frame_graph.cpp` a partir da topologia compilada.

Com HZB desligado (o padrão) ninguém lê o depth depois do pass; o `storeOp` já era
`DONT_CARE`, mas a imagem seguia alocada como render target comum ocupando DRAM sem
consumidor. Agora é `TRANSIENT_ATTACHMENT` com memória preferencialmente
`LAZILY_ALLOCATED`.

Defeito corrigido no compilador: a regra de memoryless exigia grupo de subpass
**fundido**, o que excluía o caso mais comum em mobile e o único desta engine — depth
de forward renderer de passe único. Nenhum anexo real conseguia `LAZILY_ALLOCATED`.

Verificação: **231/231** C++, 39/39 PowerShell de FrameProfile, 21/21 Python, 31/31
demais suítes Android e `assembleDebug` pelo NDK real. Teste de mutação confirma que a
economia depende da correção. **Nada medido em hardware** — sem ADB nesta sessão; a
hipótese de 0,3–1,2 ms de P1 permanece hipótese.

## Mips que preservam cobertura de alpha — 31/08/2026

Terceira fatia do programa de margem (P3 — vegetação e overdraw). O cooker gerava
mips por box filter uniforme, inclusive no alpha. Box filter preserva a média do
alpha; um material alpha-tested só enxerga a fração de texels acima do cutoff.
Medido no cooker sobre um atlas de galhos, a cobertura ia a **zero no mip 5** — a
vegetação desaparecia com a distância — e sobrava ~30% nos mips 1–2.

A semântica de cobertura passou a vir do material (`MASK` explícito ou `BLEND`
promovido pela heurística de atlas), nunca de nome de textura ou cena. Cutoffs
divergentes na mesma textura usam o menor, que erra para o lado de preservar texels.
A correção é a de Castaño/NVIDIA: busca binária do multiplicador de alpha que faz o
mesmo cutoff render a cobertura do nível base. A busca devolve o limite superior
convergido, não a última sonda — cobertura é função escada da escala e o alvo cai
entre degraus. Resultado: 100–108% de cobertura até o mip 6, onde antes era 0%.

O cozimento agora **falha** se uma textura de cobertura zerar um mip, e o manifesto
versiona `alphaSemantics`, `coverageCutoff` e `coverageByMip` por textura.

**O asset não foi recozido:** a fonte do mapa não está no repositório, então o
`scene.aemap` empacotado ainda carrega os mips defeituosos. Esta fatia corrige o
pipeline; o ganho depende do mesmo recook pendente do AEMAP v3/LOD.

Verificação: **36/36** Python (+15), 231/231 C++ e 70/70 PowerShell. Nada medido em
hardware.

## Cadência, DVFS e atribuição por passe — validação física em 31/08/2026

Primeira sessão com ADB conectado desde as fatias P0/P1/P3. Release assinado,
rota `forest-walk-v1`, 60 s por rodada.

**A regressão de FPS relatada era o APK de debug.** O pacote instalado estava
`DEBUGGABLE` com `VK_LAYER_KHRONOS_validation` carregada e ativa. Em Release, dois
controles intercalados mediram **95,23 e 95,67 presents/s**, contra a linha de base
documentada de 95,16 — não há regressão de código nas fatias P0/P1/P3.

**P1 confirmado no aparelho:** `[FrameGraph] depth 1280x2772: store=nao sampled=nao
memoryless=sim`. A política derivada do render graph atua em hardware real.

**P0 estava mentindo e foi corrigido.** As regiões de GPU mediram
`gpu_opaque_ms` = 8,89 ms igual ao frame inteiro, com Coverage/Sky/Transparent/UI
fixos em 0,0007 ms — embora remover o prepass de coverage custe +18% de GPU. Numa
GPU TBDR os timestamps internos a um render pass podem ser todos satisfeitos quando
o pass resolve, no fim do tile. O runtime agora detecta a assinatura e publica
`attribution: tile-deferred`; o relatório recusa tratar a divisão como atribuição.
Os marcadores de debug continuam válidos e são o que a captura AGI precisa.

**Cadência adaptativa: implementada, medida, retirada.** Ver
`PROFILING-ANDROID.md`. Reduzir a cadência derrubou o clock da GPU e o mesmo
trabalho passou de 8,90 para 22,11 ms, estabilizando em 30 fps — com
`thermalStatus` 0 e o aparelho **mais frio** que nas rodadas rápidas. O voto fixo de
60 Hz, testado em A/B intercalado, melhorou a uniformidade (93% contra 75%) mas
piorou o pior segundo (45 contra 71 fps). Ambos rejeitados.

**O que isso fixa para o programa de margem:** o quarto de quadros que cai para
60 fps não sai por redistribuição de tempo, só por frame mais barato. É a
validação numérica do gate de 6,20 ms da seção 0.6.

## Precisão explícita no PBR — ganho medido em 31/08/2026

O fragmento do mapa não tinha **nenhum** qualificador de precisão: tudo rodava em
`highp` por omissão. Item 2.2.5 estava marcado *parcial* e nunca fora puxado.

Cor, normal, tangente e material passaram a `mediump`; a numérica do GGX
permanece `highp` porque `rough` mínimo de 0,07 produz `alpha²` = 2,4e-5, abaixo
do menor normal do fp16 — em mediump o especular sumiria nas superfícies lisas.
A subtração de coordenadas de mundo também fica em highp. O SPIR-V carrega 90
decorações `RelaxedPrecision`.

Gate de imagem em pose fixa: **erro máximo de 1/255, zero pixels acima disso**.
A/B intercalado na rota: **8,53 ms contra 9,14 ms de GPU (−6,7%)** e +5,7 fps,
com os controles reproduzindo dentro de 1,7%.

Acumulado medido do programa de margem: **1,05 ms** (0,44 memoryless + 0,61
precisão) dos ~2,9 ms até o gate de 6,20 ms — cerca de 36% do caminho, sem
remover conteúdo.

## Precisão dos varyings e piso de ruído da bancada — 31/08/2026

Segunda metade do item 2.2.5: `vNormal`, `vTangent`, `vColor` e `vDither`
passaram a `mediump` nos dois estágios; `vPosition`, as UV e toda a cadeia de
posição/view/clip permaneceram `highp`. O SPIR-V confirma que a interface casa.

Gate de imagem: máximo de 1/255 e a divergência contra fp32 caiu de 7,06% para
5,72% dos pixels. **Nenhum ganho de desempenho é declarado:** no A/B com
aquecimento descartado, a diferença entre variantes (0,10 ms) ficou menor que o
espalhamento dentro de cada variante (0,36 e 0,24 ms). A mudança permanece por
ser semanticamente correta, mesmo tratamento dado à remoção do `nonuniformEXT`.

**Piso de ruído medido nesta bancada:** repetições da mesma build variaram ~8%,
e ~4,6% já com aquecimento descartado; a primeira execução após `adb install` é
sistematicamente pior. Efeito abaixo de **~0,4 ms** não é distinguível em duas
rodadas. Os ganhos aceitos (0,44 ms memoryless, 0,61 ms precisão do shading)
estão acima do piso; a precisão dos varyings não está.

## Política global, sombras direcionais e pós-processamento — 31/08/2026

Núcleo de qualidade da ADR-014 implementado: `renderer/rendering_policy.*` resolve
uma `ResolvedRenderingPolicy` a partir de perfil de dispositivo, escolha do projeto
e pressão térmica, com eixos independentes de sombras, ambiente, pós, texturas,
LOD, detalhe de material por distância e escala de resolução. Preset é um ponto no
espaço de configuração — `QualityPreset`
não aparece em nenhum campo da política resolvida, de modo que o renderer não pode
ramificar no nome. Toda redução registra eixo e motivo.

`renderer/shadow_cascades.*` entrega a matemática de sombra direcional em cascata:
divisão mista uniforme/logarítmica, volume pela esfera circunscrita (invariante à
rotação da câmera) e ancoragem do centro em texels inteiros. Os três artefatos
clássicos de CSM estão trancados por teste.

O `InstancedRenderer` Android agora consome essa política sem ler nome de preset.
O sol possui passe Vulkan de CSM com 1–4 cascatas estabilizadas em atlas, PCF
configurável (1/9/25 amostras), bias constante/slope/normal-offset e variante
alpha-mask para a folhagem. A seleção de cascata usa profundidade de câmera, não
distância radial. Quando depth amostrável não existe, a capability desativa sombras
e registra o fallback em vez de criar recurso inválido.

Pós-processamento deixou de ser apenas tonemap embutido no material: há alvo de cena
na escala resolvida e passe final configurável com upscale, bloom por limiar, FXAA,
sharpen, contraste, saturação e vinheta. Distâncias globais independentes desligam
normal map e probe especular quando o detalhe já não é perceptível; o runtime de LOD
recebe o budget de erro projetado e a histerese da mesma política. O sample da
floresta é apenas consumidor/stress test; nenhuma regra consulta seu nome.

Validado no Xiaomi em Debug, nos caminhos bindless e fallback, com camada Khronos:
`auto` resolve perfil B com 2 cascatas @1024 e PCF 3×3, renderiza sombra de troncos e
folhagem e executa o passe final sem VUID/crash. O relatório reproduzível é
`build/android-validation/rendering-20260831-144357/report.json`. Esta validação é de
correção; custo/FPS sustentado exige perfil separado e não é inferido dela.

O shadow pass ganhou culling conservador próprio e cache de cascatas estáticas por
tile. Cada cascata reserva uma margem espacial configurável, só é invalidada quando o
novo receptor sai do volume cacheado e preserva os demais tiles com `loadOp=LOAD`;
mudança de caster/material/sol possui API explícita de invalidação. O modo sem cache
continua disponível para diagnóstico. Na pose fixa, as duas saídas foram idênticas
bit a bit e sombra caiu de 1,82 ms para 0,0006 ms.

Na rota determinística de 60 s/6.611 poses, Release, cache ligado mediu 65,05 FPS,
pior janela 58,26, GPU 13,80 ms e sombra 0,09 ms média/1,41 ms p95; desligado mediu
53,21 FPS, pior janela 49,07, GPU 17,11 ms e sombra 3,05/3,85 ms. A ordem foi
sequencial (cache ligado antes), portanto temperatura ainda impede atribuir toda a
diferença de FPS; o timestamp isolado do passe comprova o financiamento. O item
continua parcial: separar casters dinâmicos, granularidade menor dos chunks, blend de
splits, AEMAP v3/LOD real e otimização do passe opaco seguem necessários. Evidências
em `shadow-cache-on-route-20260831` e `shadow-cache-off-route-20260831`.

Resolução dinâmica deixou de ser apenas um valor fixo: `DynamicResolutionController`
usa exclusivamente o timestamp de GPU (não acquire/pacing), cede rápido, recupera
devagar, possui histerese, piso/teto/degraus configuráveis e muda viewport/scissor sem
recriar imagens. O passe final restringe a amostragem à região ativa do alvo máximo.
O schema global v4 também separa taps de sombra próxima/distante, distâncias de
normal/probe/metallic-roughness/emissivo e budgets de LOD sólido/coverage. Nenhum filtro é ligado
implicitamente: FXAA, sharpen, bloom, vinheta, contraste e saturação continuam eixos
autorais independentes.

No mesmo Xiaomi/painel 120 Hz, a rota Release visualmente correta a piso 0,58 mediu
**96,71 FPS**, GPU **8,88 ms**, opaco **7,40 ms** e pós **1,33 ms**; a melhor janela
de 600 frames chegou a 107,11 FPS. Em pose fixa, duas capturas foram idênticas e a
apresentação mediu 110,93 FPS. Logo reduzir pixels sozinho já não financia 120
sustentados. O sample AEMAP v3 agora publica 245 grupos e o par aquecido ganhou ~2%
de GPU; a rota ainda fica em ~108–111 FPS exibidos. O próximo ganho estrutural é
HLOD/impostors, redução de transições/draws e compactação GPU-driven; baixar
mais a resolução apenas degradaria imagem sem atacar o gargalo geométrico. Um header
SPIR-V de pós inicialmente obsoleto produziu mosaicos/rastros e invalidou suas medidas;
foi corrigido e o gerador agora possui `-All -Check` para validar as 19 famílias.
Relatórios válidos: `dynamic-visual-fix-20260831` e
`dynamic-correct-route-20260831`.

Três lacunas que esta fatia tornou visíveis: `DeviceFeatures` só preenche quatro
campos, deixando **os perfis A e S inalcançáveis por detecção**; `samplerAnisotropy`
nunca é habilitada; e os arquivos de lock do Gradle estavam versionados, causando
`Failed to release lock` em todo build — corrigido no `.gitignore`.

Verificação: **265/265** C++, 42/42 PowerShell de FrameProfile, SPIR-V reproduzível,
`assembleDebug` e `assembleRelease` pelo NDK real, smoke físico bindless/fallback com
validação Vulkan e perfil Release reproduzível.

## EnvironmentMap v3 e IBL móvel — 01/09/2026

O ambiente avançado deixou de depender de amostrar o panorama bruto no fragmento.
`renderer/environment_map.*` define o contrato portátil AEEN v3 e migração v1/v2; o
cooker gera `environment-specular.aetex` octaédrico RGBA16F 256²/9 mips GGX e
`environment-brdf.aetex` RGBA16F 128²/512 amostras. O céu visível continua separado.
O runtime Android valida metadados, carrega imagens/samplers próprios e usa cinco
bindings globais. O PBR troca trigonometria esférica por projeção octaédrica.

Há oito variantes compactas de features de material, porém a primeira medição mostrou
regressão quando foram impostas ao perfil B. A engine agora expõe
`materialShaderVariants` e `environmentSplitSumBrdf` como overrides globais; B/C não
especializam por padrão. Isso preserva a possibilidade sem transformar uma hipótese
de driver em regra universal.

Em Xiaomi 25053PC47G, Release, mesma pose, escala fixa 0,58 e sem pressão térmica:
controle sem IBL 108,58 FPS/7,79 ms GPU; octaédrico analítico 107,03/8,04 ms;
octaédrico split-sum 105,04/8,04 ms. CPU média ficou 1,34–1,40 ms. Todas as janelas
foram GPU-bound e o custo do IBL ficou em ~0,25 ms; a diferença de GPU entre as duas
BRDFs ficou sob o piso de ruído. A imagem foi inspecionada sem corrupção e o par
before/after de lifecycle foi idêntico. Relatórios em
`build/android-validation/advanced-*-fixed-20260901` e contrato em
`ENVIRONMENT-MAP.md`.

Validação corrente: **281/281 C++**, **48/48 PowerShell FrameProfile**, **46/46 Python
tools**, 506/506 C#, shader embed reproduzível, lint e `assembleRelease` aprovados. O
APK final assinado é `build/aether-environment-v3-final-r2-release.apk`. A revisão
final também suprimiu rebinds redundantes do mesmo pipeline quando variantes estão
desligadas; o build passou, mas a repetição física posterior ficou bloqueada pelo
keyguard seguro do aparelho. As métricas acima pertencem à build imediatamente
anterior, com shaders/resources/política idênticos. Permanecem abertos AGI, SSIM/FLIP
multipose, rota/soak, Mali físico, SH9 e a migração do resource para Asset Database.

## Fundação compute — 01/09/2026

- RHI compute reutilizável com reflexão SPIR-V, contrato de bindings/local size,
  buffers/imagens/samplers, push constants e dispatch direto/indireto.
- Contexto com command buffer reutilizável, fence, wait/signal semaphores e
  barreiras; não há `queueWaitIdle` por dispatch.
- `VulkanDevice` detecta limites e topologia, prefere compute-only e mantém
  fallback graphics+compute.
- Render Graph possui passes/acessos compute, escolha de fila e mapper Vulkan
  para barreiras e transferência de ownership.
- Primeiro consumidor: probe ASTC 4096×4096 no Xiaomi SM8735/Adreno, sucesso em
  2,233594 ms na primeira validação e 6,525208 ms na repetição final, diferença
  máxima 7/canal e sem VUID de compute/ownership.
- Validação desta fatia: 299/299 testes nativos, shaders por geração e
  `spirv-val`, builds Android Debug/Release e repetição física aprovados.
- APK Release de desenvolvimento: `build/aether-compute-foundation-r10-release.apk`,
  SHA-256 `E031F002FA2B75685A6CFB38A33BDEC1B942EFC3044CD1CA0DD85404F823C804`.
- Pendentes: executor integral do Render Graph, `synchronization2`, timeline
  semaphores, persistência do cache e consumidores HZB/culling/Forward+/
  skinning/partículas. Ver `COMPUTE.md` e ADR-016.

## Oclusão GPU-driven — consumidor do HZB em compute — 02/09/2026

A pirâmide já era construída em compute e ficava residente na GPU
(`[HZB] produtor=compute níveis=6 base=160x346 readback=nao`). **Ninguém a
consumia.** A medição em Adreno da fatia anterior mostra as duas metades do
problema na mesma execução: `gpu_hzb_ms` mediano de **1,07 ms** e
`hzb_tested=0 hzb_occluded=0`. A decisão de oclusão continuava na CPU, que
tinha acabado de perder o readback; e o depth deixou de ser memoryless enquanto
o HZB o amostra (`store=sim sampled=sim memoryless=nao`), devolvendo os 0,44 ms
que P1 havia economizado. C1 sem C2 é custo puro.

Esta fatia entrega o consumidor. `native/rhi/shaders/draw_cull.comp` lê a
pirâmide residente e escreve o `instanceCount` dos `VkDrawIndexedIndirectCommand`
que o passe opaco já submete desde o caminho multi-draw; um objeto ocluído vira
um comando com zero instâncias e nenhum estágio gráfico sabe que o kernel existe.

Três decisões que valem mais que o código:

- **A matemática de visibilidade não é nova.** `renderer::cullDrawRecordReference`
  é o espelho instrução por instrução do shader, e um teste tranca que ele
  reproduz `projectBoundsToHzbScreenRect` + `isOccludedByHzb` +
  `updateHzbHysteresis` candidato a candidato com guarda de movimento zero.
  Migrar a decisão para a GPU não podia ser a oportunidade de mudar em silêncio
  o que a engine considera visível.
- **A guarda de movimento substitui um penhasco por uma rampa.** O caminho de
  CPU desligava o estágio inteiro assim que a câmera se mexia
  (`hzb_motion_skip`) — em primeira pessoa, nunca ocluir nada.
  `buildGpuCullMotionGuard` converte a diferença entre a pose que produziu a
  pirâmide e a pose atual em três folgas (rotação, aproximação e translação
  lateral). Não é prova de conservadorismo, e o texto não finge que seja:
  nenhuma existe para HZB temporal com câmera livre. O que é provado por teste é
  a monotonicidade — aumentar o movimento nunca aumenta o conjunto cortado.
- **O dispatch cobre a capacidade da lista, não os comandos do frame.** Ele é
  gravado antes do render pass e a lista só é construída dentro dele; um
  dispatch já gravado não relê push constants. Slots de sobra ficam com `flags=0`
  e o kernel não escreve neles — escrever apagaria a histerese do draw 0, cujo
  índice um slot zerado carrega.

O frame ganhou a região `GpuPassClass::Culling` entre Shadow e Opaque, com
marcador de captura e timestamp próprios: sem ela o dispatch cairia no balde do
passe opaco e o custo do mecanismo de visibilidade ficaria invisível outra vez —
o mesmo defeito que P0 corrigiu para o HUD e para a própria cadeia do HZB.

Verificação: **310/310** testes nativos (+9), **48/48** de FrameProfile,
**46/46** Python, Android Debug + Release + lint pelo NDK real e SPIR-V validado
e reproduzível por `-All -Check`. O contrato de bindings do kernel é refletido do
binário num teste de host, de modo que uma divergência entre shader e declaração
falha no build e não no aparelho.

### Medido no aparelho no mesmo dia — e rejeitado

A/B intercalado no Xiaomi SM8735/Adreno, Release assinado, pose fixa do hotspot,
**1280×2772 nativo**, 30 s por rodada, `Thermal Status: 0`, com os dois controles
reproduzindo em **0,031 ms** de tempo de frame. Tabela completa em
`PROFILING-ANDROID.md`.

**O kernel funciona:** `gpu_cull_tested=244 gpu_cull_occluded=28`, com
`screenshot idêntica=True` contra o controle. A oclusão em compute remove draws
reais sem mudar um pixel.

**E ainda assim não paga.** O produtor custa **+0,99 ms** de frame e o consumidor
não devolve isso: com culling ligado o frame fica **+1,28 ms**. Ocluir 28 de 244
draws — os pequenos e distantes — não move o passe principal o bastante para
pagar a cadeia de redução. Mesmo veredito da cadência adaptativa e do backface
culling: implementado, medido, não aceito. Continua opt-in e desligado por
padrão; o código fica porque a fatia seguinte (produtor mais barato, ou
candidatos maiores via HLOD) muda só um dos dois lados da conta.

**O resultado que importa veio junto.** Dois pontos de resolução e as variantes
de isolamento decompõem o passe opaco nesta pose: **custo fixo ≈ 4,8 ms**
(geometria/binning) e **custo por pixel ≈ 9,1 ms**, dos quais **5,1 ms** são
material completo menos `base-color` (2,1 ms só de IBL). Como 120 Hz exige
8,33 ms de frame e o opaco **só com base color** já custa 8,811 ms, fica provado
por aritmética que **nenhum ajuste de shading leva esta pose a 120 Hz na
resolução nativa**. O próximo alvo tem de ser quantas vezes cada pixel é
sombreado — overdraw e shading por fragmento — e não o custo de cada amostra.

## Resumo

| | |
|---|---|
| Testes C# | **506 passando**, 0 falhando, 0 pulados (inclui 5 de material/esfera e 15 de integração cena/render); interop nativo obrigatório na regressão |
| Verificações das ferramentas Android | FrameProfile **48/48** na verificação corrente; os demais grupos preservam suas suítes próprias |
| Testes C++ | **299 passando** na suíte corrente, 0 falhando. Inclui AEEN/AEMAP, política/pressão térmica, colisão, câmera/rota, HZB/sombras/LOD e os contratos compute/Render Graph Vulkan |
| Ferramentas/import gráfico | **46 testes Python passando**: material, pacote/mapa/HUD, AEEN/AETX do ambiente, alpha coverage, câmera e simplificação/upgrade de LOD |
| Linhas C# | ~11.000 |
| Linhas C++ (próprias, sem código vendorizado) | ~3.400 |
| Dependências baixadas no build | **nenhuma** — build e testes rodam offline; Jolt Physics, Box2D, SQLite, runtime .NET e VMA são vendorizados em `native/third_party/`, com versão/licença/hash ou commit registrados |

## Progresso do plano de fechamento

| Medida formal | Estado |
|---|---|
| Lacunas reais registradas em `PLANO-FECHAMENTO-LACUNAS.md` | 13 — cinco entregas futuras foram devolvidas ao plano principal |
| Lacunas integralmente fechadas | **11/13** — `GAP-FLOW-01`, `GAP-FLOW-02`, `GAP-PHY-01`, `GAP-PHY-02`, `GAP-PHY-03`, `GAP-PHY-04`, `GAP-JOINT-01`, `GAP-JOB-01`, `GAP-ECS-01`, `GAP-SER-01`, `GAP-CHAR-01` |
| Gates M0–M9 fechados | **0/10** |
| Hardware M0 | parcial — 1 aparelho Adreno; matriz mínima, Mali e perfil C pendentes |
| Shell gráfico mínimo (0.1.3 + critério visual M0) | cubo, esfera PBR e mapa real com depth/staging/câmera livre validados no Android; projeção corrigida para pré-rotação da surface. Validation layers ativas em build debug (item 2.1.6). M0 continua aberto: hot reload C#, soak formal e matriz de aparelhos pendentes |
| §4.1 Inventário executável | `docs/MATRIZ-MARCOS.md`: **339 registros** (334 itens numerados + 5 PoCs): 249 não iniciados, 45 parciais, 31 implementados, 8 PoCs, 5 validados em hardware e 1 aceito (2.5.4 saiu de "não iniciado" para "parcial" em 31/08/2026 com a primeira fatia de LOD). Contagens reconciliadas com as linhas; reclassificações refletem escopo integral/evidência, não remoção de funcionalidades. |
| §4.2 CI confiável | ✅ implementação concluída — `.github/workflows/ci.yml` separa `unit`/`native`/`interop`/`android-host`/`benchmark`/`clean-build-nightly`; integração e clean build exigem a DLL nativa; cada job publica evidência; o clean build instala headers Vulkan isolados do NDK e foi reproduzido localmente; a regressão corrente passa em 506/506 testes C# e 165/165 nativos. `metrics/budgets.v1.json` versiona P/Invoke, alocação, CPU, GPU, memória e energia, com gate positivo e negativo. `android-device`/`soak` ficam num workflow manual para runner físico. A execução hospedada continua pendente, pois ainda não há remote nem runner `android-device-lab` |
| §4 — verdade operacional/correções | ✅ implementação local completa (§4.1–§4.4); execução CI hospedada, laboratório Android e métricas coletadas em hardware permanecem evidências operacionais dos gates seguintes |

`GAP-FLOW-01` está fechado porque `return`, `break` e `continue` agora são nós
terminais explícitos, possuem escopo validado, propagam controle corretamente no
interpretador, sobrevivem à serialização e ao round-trip C#. O teste diferencial
compila e executa offline o C# original e o regenerado e compara ambos com o
estado determinístico do interpretador.

`GAP-FLOW-02` está fechado com `FlowExecutionContext`: `World`, `PhysicsWorld`,
tempo, input e logger são dependências explícitas por execução, sem singleton.
Cada nó persiste suas `RequiredCapabilities`; validador e interpretador recusam
contextos incompletos com diagnóstico do nó. O nó `log.message` prova o fluxo
vertical pelo formato `.aflow`, interpretador e C# gerado, que recebe o contexto
por parâmetro e também foi compilado/executado no teste. Nós de física concretos
continuam sendo entregas dos itens 4.1.4/5.5 do plano principal.

`GAP-CHAR-01` está fechado com `CharacterMotorSystem`: o fixed step recebe só a
velocidade desejada e compõe gravidade limitada, aderência e velocidade da
plataforma antes do `ExtendedUpdate`. `CharacterMotorState` publica
`Grounded`/`Rising`/`Falling`/`Sliding` e stance; a troca de forma é transacional.
O sistema não possui `PhysicsWorld`/handle, não contém gameplay específico e não
aloca heap gerenciada no caminho estável. Escalada, natação e criação já
agachada continuam no plano principal, sem contaminar o critério desta lacuna.

`GAP-PHY-01` está fechado com `AetherPhysicsWorldDescV2` versionado por
`structSize`/`apiVersion`, capacidades independentes de corpos, pares, contatos e
buffer da broad phase, `StepV2` com flags e contadores cumulativos e símbolo V1
preservado com defaults conservadores. Debug/teste falha imediatamente pelo
assert do Jolt; Release foi exercitado por probe induzindo as três categorias de
overflow, com warning e contadores, sem encerramento. Pilhas densas de 500, 1.000
e 5.000 corpos passam sem descarte e o benchmark não infla mais `maxBodies`.

`GAP-PHY-02` está fechado com `AetherPhysics_MoveKinematicV2`, autoridade de
transform explícita por tipo de corpo e sincronização ECS→Jolt antes do `Step`.
O cache exato de alvo por corpo evita crossings estáveis sem depender da versão
suja ainda imprecisa do ECS; após uma mudança, um único comando final zera a
velocidade cinemática calculada pelo Jolt. Plataformas transladando e girando
transportam tanto corpo dinâmico quanto character, sem feedback Jolt→ECS.

`GAP-PHY-03` está fechado com `AetherBodyDescV2`/`CreateBodyV2` preservando a
ABI V1 e expondo `mIsSensor` mais filtro Static/Dynamic. Um listener concorrente
agrega subshapes por par dirigido, publica fotografias ordenadas Enter/Stay/Exit
após cada step e devolve contagem real mesmo com buffer curto. Corpos móveis em
overlap são mantidos acordados para que o sleep do Jolt não produza Exit falso;
destroy publica Exit no step seguinte. `Trigger` participa da criação ECS, resolve
handles de evento para `EntityId` e sobrevive ao round-trip binário/texto.

`GAP-PHY-04` está fechado com `AetherPhysics_CreateBodiesV2` transacional e
`AetherPhysics_DestroyBodies`: o Jolt cria IDs fora da broadphase, faz rollback
integral em qualquer falha e insere/remove o lote pela API ampla. A fachada Span
mantém chamadas unitárias como wrappers, mede crossings/bytes/corpos e o
`PhysicsSyncSystem` agrega todas as entidades novas de um Step num único lote.
Escalas 100/1.000/10.000 passam com um crossing de criação, um de destruição e
zero alocação gerenciada dentro do crossing medido.

`GAP-JOINT-01` está fechado com `AetherJointDescV2`: a ABI V1 continua
congelada/World, enquanto a V2 aceita `World`, `LocalToBody1` e
`LocalToBody2`, convertendo pontos e eixos pelo corpo de referência. A validação
recusa descritor, eixo ou limites Hinge incompatíveis antes de entrar no Jolt e
preserva `-pi/+pi` como rotação contínua. O componente `Joint` serializa
referências `EntityId`, limites e motor; `JointSyncSystem` resolve corpos
tardiamente, preserva handles estáveis, recria apenas em hot-edit e limpa
constraints após remoção de componente/entidade/corpo. Inspector real continua
dependente dos itens 3.x do plano principal. O contrato de serviços do Flow está
fechado; os nós concretos de juntas pertencem aos itens 4.1.4/5.5.

`GAP-JOB-01` está fechado com um grafo explícito de pré-requisitos e esperas
runtime. Cada `Complete()` feito dentro de um job publica sua aresta sob lock e
procura o caminho inverso antes de esperar; ciclos de autoespera, dependência,
dois jobs e três jobs são recusados com o caminho completo. O timeout de 3 s
permanece apenas como watchdog para trabalho externo que o grafo não observa.

`GAP-ECS-01` está fechado com APIs explícitas `Read<T>`/`Write<T>` no mundo e
`GetReadOnlySpan<T>`/`GetWritableSpan<T>` no chunk. Leitura não altera versão;
cada acesso mutável incrementa uma vez somente a coluna do chunk acessado. As APIs
antigas permanecem deprecated e conservadoras para não quebrar consumidores. Duas
regressões cobrem leitura, escrita, coluna e isolamento entre chunks.

`GAP-SER-01` está fechado com formato texto v2, `ComponentField.Id` persistente e
aliases explícitos de componente/campo. A leitura continua aceitando v1; fixtures
v1, v2 e v3 preservam rename, default de campo novo e descarte de campo removido.
Incompatibilidades e schemas futuros falham com contexto. O binário também prova
uma cadeia v1→v2→v3 de dois migradores resolvendo o nome antigo do componente.

## Por fase

| Fase | Item | Estado |
|---|---|---|
| **0.1** | Monorepo, build C# + CMake/Ninja, runner de testes próprio | ✅ |
| **0.1.3 + shell gráfico mínimo** | NativeActivity ARM64, Gradle/NDK, landscape imersivo, lifecycle, surface, swapchain e frame Vulkan | ⚠️ Cubo texturizado + depth e toque em 1 aparelho físico (Xiaomi SM8735/Adreno, Android 16). Regressão em 28/08 com 2 retomadas, configuração e screen-cycle; os 100 ciclos históricos eram do triângulo. Matriz de GPUs e demais critérios M0 continuam abertos |
| **0.1.4** | Integração .NET no processo nativo: carregar CoreCLR, chamar C# do C++ e vice-versa | ✅ **Completo e integrado ao APK de produção, validado em hardware real** (Xiaomi SM8735). `DotNetHost` (`native/platform/android/dotnet_host.h/.cpp`) hospeda CoreCLR via `hostfxr_initialize_for_runtime_config` → `hostfxr_get_runtime_delegate` → `load_assembly_and_get_function_pointer` (resolvido via `dlopen`/`dlsym`, sem `nethost`). `ensureDotNetAssetsExtracted` (`dotnet_assets.h/.cpp`) copia o runtime + BCL de `assets/dotnet/` (empacotados pelo Gradle, guiados por um manifesto gerado em build) para um diretório real de arquivos na primeira execução, idempotente. `getNativeLibraryDir` (`android_paths.h/.cpp`) resolve o path via a única chamada JNI necessária (`ApplicationInfo.nativeLibraryDir`). Chamado uma vez em `android_main`, antes do loop de eventos — log real do dispositivo: `CoreCLR carregado com sucesso via hostfxr` seguido de `CoreCLR hospedado no shell: Ping(2,3)=5`, com o shell gráfico (surface/swapchain/triângulo) continuando normalmente logo em seguida no mesmo processo. Sobrevive a ciclo background→foreground sem re-inicializar nem crashar. Duas descobertas só encontradas em execução real, documentadas em `native/third_party/dotnet-runtime/README.md`: (1) `hostfxr` recusa o diretório do framework sem os manifestos `Microsoft.NETCore.App.deps.json`/`.runtimeconfig.json`, mesmo com todos os `.so` presentes; (2) recusa `.runtimeconfig.json` self-contained — o assembly gerenciado precisa ser publicado framework-dependent. Terceira descoberta, em `AndroidManifest.xml`: com `extractNativeLibs` no padrão do AGP moderno, `.so` carregam direto de dentro do `.apk` sem extração real em disco — funciona para o `lib_name` do NativeActivity (carregado pelo próprio sistema), mas quebra `dlopen()` manual do nosso código; corrigido com `android:extractNativeLibs="true"` + `packaging.jniLibs.useLegacyPackaging = true`. Binários do runtime vendorizados em `native/third_party/dotnet-runtime/` (~6,5 MB) |
| **0.3.2** | Protótipo de interação: câmera, gizmos, menu radial, inspector | ✅ `prototype/editor.html` |
| **0.4.1** | ADRs para linguagem, ECS vs cena, formato de arquivo, build, backend gráfico: `docs/adr/ADR-01` a `ADR-12`, cada uma com contexto, alternativas descartadas e evidência real do código (paths de arquivo, contagens de teste). 7 das 12 decisões (linguagens, ECS, Jolt, formato dual, WAL — mais Vulkan e render graph com limitação registrada) têm implementação testada; 4 (processo Play separado, build em nuvem, pipeline único de renderização, menu radial de produto) são registradas como decisão preventiva sem código ainda, exatamente como o texto de cada ADR declara — nenhuma inflada para parecer mais adiantada do que é | ✅ 12 documentos + `docs/adr/README.md` de índice. Nenhuma reescreve o roadmap do plano principal — cada uma cita o item correspondente e para de escrever quando a implementação não existe |
| **0.3.4** | `managed/Aether.Core/Design/`: sistema de design. `ColorToken`/`ColorPalette` — temas `Dark`/`Light` transcritos exatamente dos valores hex de `prototype/editor.html` (item 0.3.2, já validado visualmente), incluindo cores de eixo X/Y/Z consistentes com CONVENCOES.md §5. `SpacingScale` — progressão geométrica base-4 (4/8/12/16/24/32) mais as três dimensões estruturais citadas literalmente pelo plano (TopBar 40dp, Dock 64dp, Rail 44dp). `TypographyScale` — 6 degraus nomeados (`Caption` a `Headline`) cobrindo a faixa de 9 a 19px observada no protótipo, mais as duas famílias de fonte já usadas ali. `HapticVocabulary` — mapeia `HapticCue` (seleção, snap, duplicar/descolar, erro, confirmação, abertura de menu radial) para `HapticIntensity`, o "vocabulário definido" citado pelo item 3.1.4, sem ainda integrar a API real de vibração de nenhuma plataforma | ✅ 20 testes (`DesignTokensTests.cs`) — cobre conversão hex exata contra os valores do protótipo, paletas dark/light distintas com checagem básica de contraste (fundo mais escuro que texto), progressão estritamente crescente de espaçamento/raio/tipografia, e todo membro de `HapticCue` com intensidade mapeada (o teste que impede alguém de adicionar um cue novo e esquecer de mapeá-lo). Continua **parcial**: ícones vetoriais ficam de fora — o protótipo usa só glifos de texto/emoji, sem pipeline SDF real (trabalho genuíno do item 3.1.1); nenhuma UI de produto consome os tokens ainda (item 3.1.2) |
| **0.2 (PoC-D)** | Loader C# via `AssemblyLoadContext` collectible e entrypoints nativos implementados; fluxo completo editar/compilar/aplicar/ver permanece parcial | 11 testes host. Evidência anterior no Android: 10 ciclos de DLLs pré-compiladas em 128 ms; não mede compilação nem alteração visível no editor. Coleta do ALC no Android e ausência de vazamento prolongado não comprovadas. |
| **0.2 (PoC-E)** | Encoder ASTC 4×4 sintético e probe reproduzível em worker com device/fila próprios; flags, barreiras, timestamps e cleanup corrigidos | Revalidado com Khronos ativa: 4096×4096, 16.777.216 texels RGBA comparados, erro máximo 7/255, GPU 2,829844 ms. Probe total debug 2.393,012655 ms, não tempo de importação. Limites/corpus/evidência em `EXECUCAO-EDITOR-ANDROID.md`. |
| **0.4.2** | Especificação do IDL de fronteira C#↔C++: `docs/idl/FORMATO-IDL.md` documenta um formato texto próprio (sem dependência externa — sem parser YAML disponível, `CONVENCOES.md` §7 proíbe `PackageReference`), com uma entrada por função/enum/struct, e a decisão registrada de ser **descritivo e validado**, não gerador de código — resolve a divergência real que a `ADR-01` documentou (P/Invoke escrito à mão) sem o risco de regressão de reescrever bindings já testados/validados em hardware. Três arquivos `docs/idl/*.idl` (`physics.idl`, `sqlite.idl`, `transform.idl`) descrevem fielmente os três bindings reais. `IdlValidationTests.cs` implementa o parser do formato e compara cada `.idl` contra o tipo `Native*` real via reflection (nome de símbolo nativo via `EntryPoint` quando declarado, não o nome do método C#; campos de struct incluindo não-públicos; enum e `[Flags]`) | ✅ 7 testes (`IdlValidationTests.cs`) — 4 cobrem o parser isoladamente (cabeçalho, seção vazia, enum com flags, linha de comentário ignorada), 3 validam cada `.idl` contra o binding real. Validado empiricamente que o comparador de fato detecta divergência: uma corrupção deliberada de assinatura (parâmetro extra) foi introduzida, confirmada como pegada pelo teste, e revertida — não é só um teste que sempre passa. O processo de escrever os `.idl` já pegou 2 divergências reais entre a leitura inicial do código e a assinatura verdadeira (campo de padding `_reserved` esquecido, nome de método C# diferente do `EntryPoint` nativo em `NativeTransformKernel`), confirmando o valor do validador. Não gera código C#/C++ a partir do IDL — deliberadamente fora de escopo, ver `docs/idl/FORMATO-IDL.md` |
| **1.5.1** | Matemática: float2/3/4, quaternion, float4x4, Transform, Bounds, Ray, Plane, Frustum | ✅ 27 testes |
| **1.5.2** | `FixedClock`: converte delta real em N passos de fixed step determinístico + fração de interpolação, escala de tempo, pausa distinta de escala zero, teto anti-espiral-da-morte com contador de passos descartados | ✅ 20 testes — inclui espiral da morte (frame de 5s não trava simulando 300 passos), drift de precisão (10.000 frames a 59,94 Hz não desvia do total esperado), pausa/retomada preservando escala configurada, `Assert.NoAlloc` no caminho de `Advance` |
| **1.5.3** | `Signal<T>`: publicador/assinante tipado em processo, array de slots pré-alocado (sem `List<T>`/dicionário) com alça geracional (`SignalSubscription`) — mesmo padrão índice+geração de `PhysicsBodyHandle`. `Subscribe`/`Unsubscribe` idempotentes e seguros contra reciclagem de slot; `Publish` varre sem alocar (o delegate do assinante pode alocar se capturar estado — responsabilidade de quem assina, não do tipo) | ✅ 13 testes (`SignalTests.cs`) — cobre reciclagem de slot com geração desatualizada não removendo o assinante novo, auto-remoção durante a própria `Publish` sem corromper a varredura, crescimento do array de slots, idempotência de `Unsubscribe` |
| **1.5.4** | `ConfigurationStore`: dicionário chave→valor tipado (`int`/`float`/`bool`/`string`, union manual sem boxing), leitura tolerante (`GetXOrDefault`, tipo errado cai no default) e estrita (`TryGet`+`ConfigValue.AsX`, tipo errado lança). Serialização texto determinística (chaves em ordem alfabética ordinal, mesma disciplina de `TextSerializer`), sem acoplar a path/stream — onde persistir é decisão do chamador, não deste tipo (1.1.1 já fornece `IFileSystem`, mas a integração `ConfigurationStore`↔`IFileSystem` ainda não foi feita) | ✅ 25 testes (`ConfigurationStoreTests.cs`) — cobre round-trip completo dos 4 tipos, escape de string com aspas/barra, determinismo de serialização, arquivo corrompido falha com contexto em vez de ser descartado, chave com espaço rejeitada |
| **1.1.1** | `Aether.Platform.IFileSystem`: abstração de sistema de arquivos por escopo nomeado (`FileSystemScope.Assets` somente leitura, `PersistentData`, `Cache`) — nenhuma API aceita caminho absoluto livre. `StandardFileSystem` implementa sobre `System.IO`, com raiz configurável por escopo; é a implementação usada em desktop/testes e a base sobre a qual a integração Android real vai injetar `internalDataPath`/`externalDataPath` de `ANativeActivity` (já resolvidos nativamente, sem JNI, em `native/platform/android/android_paths.h`) como roots | ✅ 19 testes (`PlatformFileSystemTests.cs`) — cobre round-trip via `WriteAllBytes`/`ReadAllBytes` e via streams, `..`/caminho absoluto rejeitados na resolução de caminho, escrita/delete em `Assets` lança `InvalidOperationException`, escopos diferentes isolados fisicamente, enumeração não-recursiva, diretório ausente não é erro. Item permanece **parcial**: falta a implementação Android real consumindo os paths nativos e a migração de `WriteAheadLog`/`ConfigurationStore` para usar esta abstração em vez de `path: string` direto |
| **1.1.2** | `managed/Aether.Core/Input/`: camada portátil de entrada. `TouchPoint`/`TouchPhase` cobrem o ciclo completo (Began→Moved/Stationary→Ended/Cancelled) com posição/anterior/delta/pressão/raio; `InputState.PredictPosition` extrapola linearmente a posição por um horizonte configurável (predição pedida pelo item, deliberadamente ingênua — não Kalman). `PenInfo` (pressão/tilt X-Y/rotação/botão lateral/hover) associado por Id de toque. `KeyCode`/`KeyEvent`/`KeyPhase` para teclado, `MouseState`/`MouseButtons` para mouse/trackpad, `GamepadState`/`GamepadButtons` para até N controles configuráveis. `InputState` agrega tudo por frame com modelo push (`PushTouch`/`PushPenInfo`/`PushKey`/`SetMouse`/`SetGamepad`); `EndFrame` promove toques sobreviventes para `Stationary`, remove `Ended`/`Cancelled` (e o `PenInfo` associado) e limpa eventos de tecla do frame | ✅ 25 testes (`InputTests.cs`) — cobre ciclo de vida completo de multi-touch (incluindo remoção do meio da lista preservando os demais), predição com delta constante e com toque inexistente, caneta desassociada ao finalizar o toque, teclado Down/Up, isolamento de gamepads por índice com índice inválido rejeitado, e `Assert.NoAlloc` na leitura de `ActiveTouches` (o caminho quente de todo frame com input). Continua **parcial**: a órbita M0 já capta `AInputEvent` no shell, mas não alimenta este `InputState` gerenciado. Ponte Android completa, iOS e sensores continuam futuros |
| **1.1.4** | `managed/Aether.Core/Platform/`: janela/display portátil. `SafeAreaInsets` (margem por borda, futuramente consumida pela UI do item 4.6.1), `DisplayInfo` (resolução, `DensityScale`, `RefreshRateHz` variável — suporte a taxa adaptativa —, `SafeAreaSize()` saturando em zero se os insets excederem a dimensão) e `WindowState` (display principal + displays externos via modelo push, mesma disciplina de `InputState`; "multi-janela" = múltiplos displays simultâneos, não split-screen de processo, que é lifecycle — item 1.1.3) | ✅ 23 testes (`WindowStateTests.cs`) — cobre validação de dimensão/densidade/taxa negativa rejeitada, saturação de safe-area em configuração degenerada, troca de Id do display principal rejeitada (identidade não muda em runtime), display externo declarado `IsBuiltIn` rejeitado, múltiplos displays externos coexistindo independentemente. Continua **parcial**: nenhuma captação real de `Display`/`DisplayCutout` (Android) ou `UIScreen` (iOS) existe — shell Android segue landscape fixo simples |
| **1.2** | Memória: FrameArena, PoolAllocator, NativeList/Array, MemoryBudget | ✅ 22 testes |
| **1.2 + GAP-JOB-01** | Job system com work-stealing, dependências, política big.LITTLE e grafo explícito de pré-requisitos/esperas runtime. Ciclos são detectados atomicamente antes do bloqueio e diagnosticados como caminho; timeout não é mais o detector primário | ✅ 13 testes, incluindo ciclos de 1, 2 e 3 jobs e ciclo prerequisito↔dependente; suíte repetida 10 vezes sem flutuação |
| **1.2.3** | `AtomicCounter` (wrapper fino sobre `Interlocked`) e `SpscRingBuffer<T>` (fila circular single-producer/single-consumer, wait-free, sobre `NativeArray<T>` — sem alocação após construção, `Volatile.Read`/`Write` para release/acquire semantics entre produtor e consumidor). Complementa `ConcurrentQueue<T>` do BCL já usada em `JobSystem` (MPMC genérico) para o caso mais específico e mais barato de exatamente um produtor e um consumidor | ✅ 12 testes (`ConcurrencyTests.cs`) — inclui prova real de atomicidade (8 threads × 10.000 incrementos concorrentes sem perda) e prova real de visibilidade de memória entre threads (produtor/consumidor em threads de verdade, 200.000 itens, sem perda/duplicação/corrupção); suíte repetida 5 vezes em Release sem flutuação |
| **1.2.4** | `AllocationTracker` (mede bytes alocados por thread via `GC.GetAllocatedBytesForCurrentThread`, mesma API base de `Assert.NoAlloc`) + `JobDiagnostics` (visualizador de jobs: buffer circular dos últimos jobs concluídos com duração/alocação/falha, mais totais agregados por `Label`). `JobSystem.Schedule` ganhou overload com `label` (categoriza o job, ex. `"physics.step"`) e instrumenta automaticamente cada execução em `RunEntry`, exposto via `JobSystem.Diagnostics` | ✅ 13 testes (`DiagnosticsTests.cs`) — cobre medição real de alocação (array de bytes detectado), agregação por label entre múltiplos jobs, job com falha marcado `Faulted` no snapshot, buffer circular respeitando capacidade |
| **1.2.5** | `[NoAlloc]` (atributo de marcação, `Diagnostics/NoAllocAttribute.cs`) + analisador Roslyn `Aether.Analyzers/NoAllocAnalyzer.cs` (projeto novo, `net8.0`, referenciando `Microsoft.CodeAnalysis(.CSharp)` direto do SDK via `$(MSBuildToolsPath)\Roslyn\bincore\`, sem NuGet — mantém zero dependência externa). Cinco regras estáticas em tempo de build sobre métodos/propriedades marcados: AETH001 `new` de tipo referência, AETH002 lambda que captura variável externa, AETH003 uso de LINQ (`System.Linq`), AETH004 concatenação/interpolação de string, AETH005 `foreach` sobre expressão de tipo interface (risco de boxing do enumerador). Referenciado em `Aether.Core.csproj` via `ProjectReference` com `OutputItemType="Analyzer"` + `ReferenceOutputAssembly="false"` (roda no build, não vira dependência de runtime) | ✅ Validado com build real e arquivo probe descartável: as 5 regras dispararam nas linhas exatas esperadas (AETH001/002/004 numa rodada, AETH003/005 noutra) e nenhuma disparou em código limpo equivalente — incluindo o caso de não-falso-positivo `foreach` sobre `int[]` (tipo concreto) não acionando AETH005. Suíte completa (363 testes) permanece verde após adicionar o projeto à solução |
| **3.4** | PowerGovernor com histerese | ✅ 6 testes |
| **1.3 + GAP-ECS-01** | ECS por arquétipos: chunks de 16 KB, `CompiledQuery`, filtros With/Without/Changed sem GC estável, command buffer e leitura/escrita com versões exatas por chunk/coluna | ✅ 29 testes |
| **1.3.4–1.3.6** | Hierarquia, plano topológico O(N), limite atômico de 64 níveis, fachada `Node` e kernel nativo de transform em lote | ✅ 20 testes; benchmark 100k zero-GC. Host: p50 0,96 ms no run da regressão completa. **Hardware real (Xiaomi SM8735/Adreno, CoreCLR ARM64 via `adb shell`)**: três execuções consecutivas deram p50 total 0,59/0,59/2,29 ms e p95 0,66/0,66/2,47 ms. O backend registrado foi `NativeBatch`; o subgate <6 ms está fechado nesse aparelho. `taskset f0` restringiu afinidade aos núcleos 4–7, mas a frequência continuou dinâmica — não se afirma clock travado |
| **1.4.1–1.4.2 + GAP-SER-01** | Metadados runtime, IDs/aliases persistentes, serializador binário bit-exato e texto v2 determinístico/compatível com v1, migração v1→v2→v3 | ✅ 22 testes |
| **1.4.3** | Sistema de recursos: `ResourceId`, `ResourceRef`/`WeakResourceRef` com posse linear, `ResourceHandleTable` com contagem de uso, carregamento assíncrono via JobSystem | ✅ 18 testes |
| **1.4.4** | Undo/redo (`UndoStack`) + WAL de recuperação com fsync, checksum FNV-1a por registro, recuperação parcial ante corrupção | ✅ 16 testes |
| **1.4.5** | SQLite 3.53.4 vendorizado (amálgama C oficial, verificado por hash SHA3-256 contra o publicado em sqlite.org — `native/third_party/sqlite/VENDORED_COMMIT.txt`), exposto por `native/resources/sqlite_bridge.h/.cpp`: fronteira C ABI blittable (abrir/fechar conexão, preparar/step/reset de statement, bind de parâmetro int64/double/texto/null, leitura de coluna por tipo) — texto sempre como (ponteiro UTF-8, comprimento), nunca `char*` terminado em nul confiado, mesma disciplina de CONVENCOES.md §2. Lado C# em `managed/Aether.Core/Resources/`: `NativeSqlite` (P/Invoke cru via `[LibraryImport]`, mesma disciplina de `NativePhysics`) e o wrapper de alto nível `SqliteConnection`/`SqliteStatement` (`IDisposable`, converte `string`↔UTF-8 na fronteira via `Encoding.UTF8`, traduz erro nativo em `SqliteException` com a mensagem real do SQLite). Deliberadamente **não inclui o schema do índice de dependências em si** ("quem usa este asset") — isso é trabalho da Fase 7 (pipeline de import) quando existir consumidor real; esta fatia entrega só o binding testado, ponta a ponta (nativo + gerenciado), que qualquer schema futuro pode usar, evitando abstração precoce (docs/PLANO-FECHAMENTO-LACUNAS.md §8) | ✅ 7 testes C++ (`test_sqlite_bridge.cpp`) + 12 testes C# (`SqliteBridgeTests.cs`, via `NativeInterop.SqliteLibraryAvailable`, mesmo padrão de skip condicional de `PhysicsTests`) — cobre open/close em arquivo real, diretório inexistente falhando explicitamente, SQL inválido com mensagem recuperável, round-trip dos três tipos com parâmetros bindados, round-trip UTF-8 com acentuação e emoji (fora do plano ASCII), NULL lido de volta como tipo Null, Reset entre execuções, iteração multi-linha respeitando ORDER BY, reabertura do mesmo arquivo após Dispose, uso após Dispose lançando `ObjectDisposedException`. `aether_resources`/`aether_resources_shared` linkados e testados; build nativo completo (`aether_tests`) 119/119 verde, suíte C# 442/442 verde, com a DLL nativa de fato carregada (zero pulados) |
| **5.1–5.6 (fundação)** | AetherFlow: AST, validador, gerador de C#, parser de volta, interpretador, serializador, controle terminal simétrico e contexto explícito de serviços/capabilities | ⚠️ 48 testes verdes; `GAP-FLOW-01/02` fechados, mas catálogo de domínio, editor e API de produto de 5.1–5.6 continuam no plano principal |
| **2.1.2–2.1.3 (recursos e upload)** | VMA 3.4.0 encapsulado por `VulkanMemoryAllocator`; buffer/imagem/view move-only, sampler e upload RGBA8 com staging, flush explícito, barreiras e fence. Budgets Buffer/Texture/RenderTarget/Staging; nenhum upload no frame estável. `SurfaceTransform` mantém dimensões naturais da swapchain e projeção visível coerentes, incluindo rotação/espelho. Contratos em `RHI-RECURSOS.md`. `rhi/pipeline_cache.h/.cpp` (novo): `PipelineCache` real generaliza `DescriptorCache<Desc,Handle>` (já testado headless) para `RenderPassCache`/`PipelineLayoutCache`/`GraphicsPipelineCache`, ancorados em `VulkanDevice` — sobrevivem a `shutdown()`/`initialize()` de um renderer individual, compartilháveis entre `TriangleRenderer`/`InstancedRenderer`. `GraphicsPipelineCacheDesc` referencia render pass/pipeline layout pelo handle já cacheado (não descrição recursiva) mais um `vertexInputHash` do chamador para os aspectos que não cabem como campo escalar. Renomeado `SamplerDesc` de teste em `descriptor_cache.h` para `SamplerCacheTestDesc` — colidia com o `SamplerDesc` real de `resource.h` em `ae::rhi` assim que as duas unidades de tradução se encontraram via `device.h` | ✅ Debug/release ARM64 e lint; 170/170 C++ e 506/506 C# sem skips. Cubo/depth/textura reais no Xiaomi, relatório `build/android-validation/rhi-cube-final-20260828/report.json`. **`PipelineCache` conectado ao `VulkanDevice` e compilado/linkado, mas ainda não consumido**: `TriangleRenderer`/`InstancedRenderer` continuam chamando `vkCreateGraphicsPipelines`/`vkCreateRenderPass` diretamente em cada `initialize()` — a recriação por troca de tela ainda não foi eliminada, só a infraestrutura para eliminá-la existe. Validado em hardware físico (Xiaomi) que a integração não introduziu regressão: cena "dirt road" completa renderiza sem erro, `validate-android-shell.ps1` PASS em 3 ciclos. Pressure budgets e uploads assíncronos continuam futuros; migração dos renderers para o cache é o próximo passo real |
| **2.1/2.3** | RHI Vulkan: cache de descritores, perfis de dispositivo S/A/B/C e Render Graph headless | ✅ lógica testada; execução completa do Render Graph na GPU permanece futura |
| **2.1.4** | Registro bindless de texturas integrado; contrato de quatro sub-features e seis limites; fallback convencional real sem descriptor indexing habilitado | Ambos os caminhos passam em hardware com Khronos ativa (`rendering-20260828-140330/report.json`); 3 novas regressões headless. Escopo integral parcial: faltam tabelas globais restantes/consumidores; ver `EXECUCAO-EDITOR-ANDROID.md`. |
| **2.1.6** | Validation layers e debug markers integrados em debug; semáforos por imagem e negociação de bindless corrigidos no commit herdado d56c992 | Logs atuais sem VUID nos dois caminhos e no probe ASTC. Ainda falta captura de frame RenderDoc/AGI inspecionada para fechar o item integral; screenshot não a substitui. |
| **2.3** | **Render graph**: topológico, poda, aliasing, barreiras, load/store ops, memoryless, fusão de subpasses | ✅ 41 testes |
| **4.1.1 + GAP-PHY-01** | Física Jolt e ABI de capacidade segura: mundo/corpos/Step/queries básicos, mais `AetherPhysicsWorldDescV2` com quatro limites independentes, política de overflow, `StepV2` com `EPhysicsUpdateError` espelhado e estatísticas cumulativas. O símbolo V1 permanece e mapeia para defaults conservadores; o alocador temporário acompanha as capacidades configuradas | ✅ 7 testes físicos existentes + 3 testes de ABI/capacidade; pilhas densas de 500/1.000/5.000 sem overflow; probe Release retorna `0x7` e contabiliza as três categorias induzidas |
| **4.1.2 + GAP-PHY-02/03/04** | Fachada C# de física: componentes ECS `RigidBody`/`Collider`/`Trigger`, wrapper `PhysicsWorld` e `PhysicsSyncSystem`. Autoridade estático/dinâmico/cinemático explícita; trigger persistente com filtro e Enter/Stay/Exit; `CreateBodiesV2`/`DestroyBodies` transacionais em `Span`, telemetria de crossings/bytes e spawn ECS agregado. ABI V1 permanece congelada | ✅ três gaps de física fechados; 9 testes C++ de trigger/batching + 15 testes C# de fachada/sync; 100/1.000/10.000 corpos com 1 crossing create + 1 destroy e zero GC no crossing |
| **4.1.3 + GAP-JOINT-01** | Juntas Point/Hinge/Slider/Distance com motor real e handles índice+geração. `AetherJointDescV2` preserva V1 e acrescenta `World`/`LocalToBody1`/`LocalToBody2`, com conversão física testada e validação Hinge sem clamp. A fachada C# usa V2; `Joint` é componente declarativo serializável por `EntityId`, e `JointSyncSystem` resolve tardiamente, mantém lifecycle idempotente e recria só quando descriptor/corpos mudam. O gizmo continua apenas no protótipo HTML; SixDOF e tipos especializados não foram adicionados sem consumidor | ✅ `GAP-JOINT-01` fechado; 11 testes C++ + 13 testes C# de juntas, mais round-trip binário/texto em serialização. O item 4.1.3 global permanece parcial somente pelo viewport/editor de produto e tipos futuros explicitamente fora desta fatia |
| **4.1.4** | Queries: extensão de `jolt_bridge.h/.cpp` com `AetherPhysics_RayCastAll` (multi-hit, via `JPH::AllHitCollisionCollector`), `AetherPhysics_ShapeCastClosest` (varredura de forma, `JPH::NarrowPhaseQuery::CastShape`) e `AetherPhysics_OverlapShape` (overlap parado, `JPH::NarrowPhaseQuery::CollideShape`) — complementam `RayCastClosest` (4.1.1). Filtro de camada (`AetherQueryLayerMask`, estático/dinâmico combinável) e de corpo a ignorar (espelha `JPH::IgnoreSingleBodyFilter`). `AetherShapeQueryHit` novo (ponto de contato nos dois lados + eixo de penetração) para os dois tipos com contato real; `RayCastAll` continua só corpo+fração (raio não gera contato Jolt sem uma segunda consulta). Todas as três seguem o padrão "buffer do chamador, devolve contagem real" (a fronteira C ABI não pode devolver `std::vector`). Fachada C# em `PhysicsWorld.RayCastAll/ShapeCastClosest/OverlapShape` sobre `Span<T>`, `QueryLayerMask`, `ShapeQueryHit` | ✅ 10 testes C++ (`test_query_bridge.cpp`) + 9 testes C# (`PhysicsQueryTests.cs`) — multi-hit ordenado por distância, contagem real excede buffer sem truncar silenciosamente, filtro de camada, `ignoreBody`, shapecast acerta o ponto certo de contato, overlap encontra/ignora por raio e por `ignoreBody` |
| **4.1.5 + GAP-CHAR-01** | Character controller sobre `JPH::CharacterVirtual`, com cápsula, degraus/stick-to-floor via `ExtendedUpdate`, rampas, plataforma, ground queries, stance transacional e handles geracionais. `CharacterMotorSystem` fecha a composição de fixed step: gravidade, limite de queda, velocidade desejada e plataforma deixam de depender da ordem manual do chamador; estados de movimento são explícitos. **Escalar, nadar e nascer agachado permanecem escopo futuro do item 4.1.5**, portanto o item global continua parcial | ✅ 11 testes C++ + 17 testes C#; inclui piso/teto, rampa andável/íngreme, degrau, plataforma, rising/falling, stance, lifecycle e zero GC no motor estável |
| **4.1.8** | Determinismo cross-platform (**modo opcional de build**, não "ponto fixo" literal — o nome do item no plano diverge do que existe de verdade a implementar; ver limitação abaixo): opção CMake `AETHER_PHYSICS_DETERMINISTIC` (`native/CMakeLists.txt`) que liga `CROSS_PLATFORM_DETERMINISTIC` — nativo do próprio Jolt, não código nosso — desligando fusão multiplicação-adição (FMA, via `/fp:precise`/`-ffp-contract=off` em vez de fast-math) e travando o nível de instrução SIMD em SSE4 (`USE_AVX`/`USE_AVX2`/`USE_FMADD`/`USE_F16C` todos `OFF`; SSE4.1/4.2/LZCNT/TZCNT continuam ligados, são baseline em qualquer x86-64 relevante). Mesmo padrão de opção de build que `DOUBLE_PRECISION`/`USE_ASSERTS` já usavam neste arquivo — alterna entre duas configurações de build da MESMA lib vendorizada, escolhida em tempo de `cmake -D...` (não dois binários coexistindo escolhidos em runtime). Validado que a flag realmente muda o código gerado (não é um no-op silencioso): a mesma simulação produz posições finais numericamente DIFERENTES entre as duas builds (ex.: `x=-0.0412826203` na build normal vs. `x=-0.0412814654` na determinística, mesma semente/cenário) | ✅ 3 testes C++ novos (`test_determinism.cpp`) + reexecução dos 43 testes de física pré-existentes em ambas as configurações de build (89 execuções extras, todas idênticas) — mesma simulação (cabo de 4 corpos com juntas Hinge motorizadas caindo sobre rampa) rodada 2x e 3x seguidas no mesmo processo produz resultado bit-exato (posição+rotação+velocidade via `memcmp`), com teste de guarda contra falso positivo de "bit-exato porque nada se moveu" |
| **4.1.6 + GAP-2D-01 (parcial)** | Física 2D mantém `AetherAllowedDOFs.Plane2D` no runtime; Box2D v3.1.1 foi vendorizado **somente para decisão**, sem entrar na ABI/ECS. O harness A/B usa cenário ativo equivalente, quatro passos internos em ambos, três trials, 50–5.000 corpos, p50/p95/p99, RSS isolado, tamanho, validade numérica e equivalência geométrica. No host, Box2D reduziu p50/p95 em 87,6%/82,6% com 1.000 corpos; no Android A, 93,7%/94,6%. Ambos ficaram estáveis, mas CPU e RSS divergem em 5.000 corpos. É evidência de harness, não decisão: ADR-013 continua proposta até perfis Android B/C | ✅ 7 testes C++ + 6 testes C# de comportamento; 3 binários compilados, host e ARM64 cross-build verdes, execução integral host + Android A verde. `run-physics2d-ab.ps1`/CI publicam relatórios, temperatura e metadata; runner físico executa a mesma matriz |

## O que ainda não existe

- **Validado em uma GPU física, ainda não numa matriz representativa.** O shell Android cria instance, device, surface, swapchain, pipeline e apresenta frames contra um driver Adreno real (ver "Shell Android — validação atual" abaixo), mas isso cobre só 1 dos ~6 aparelhos pedidos antes de M0 representativo. Faltam Mali e perfil C fraco.
- Existe app Android com .NET, 5.000 cubos instanciados, textura via staging, depth e órbita touch; a toolchain GLSL embutida é reproduzível. Faltam app iOS, geometria importada, pipeline Slang/HLSL/reflection, hot reload C# e demais gates M0. Após cache do workload e correção do empacotamento C#, o smoke mediu 60,039 FPS, CPU média 2,412 ms/pico 18,237 ms e interop wall médio 0,537 ms. Falta reduzir picos e repetir na matriz; ver PROFILING-ANDROID.md.
- O protótipo do editor é HTML/Canvas, não a engine — valida **interação**, não desempenho gráfico. Usa as mesmas convenções de espaço do núcleo em C# de propósito, para que o que se aprende ali transfira.
- Animação, áudio e pipeline de assets/exportação de produto continuam futuros. O profiler da Fase 8 tem coleta CPU/frame parcial, não timeline de jobs/threads.
- **Física (Fase 4) possui fatias adiantadas, não a fase pronta.** 4.1.1/4.1.2 têm seus slices atuais validados; 4.1.3, 4.1.4, 4.1.5 e 4.1.6 continuam parciais conforme `MATRIZ-MARCOS.md`. Não iniciados: 4.1.7 (decomposição convexa real) e 4.1.9 (sub-stepping térmico). Em 4.1.3 faltam tipos futuros e integração de editor; em 4.1.4 faltam os nós Flow do catálogo; em 4.1.5 faltam escalar, nadar e criação agachada; em 4.1.6 o A/B local está pronto, mas faltam os perfis móveis B/C e a decisão final da ADR-013. Esses trabalhos pertencem ao plano principal, salvo os critérios corretivos explicitamente registrados no plano de lacunas.

## Shell Android — validação atual

**Aparelho de referência (primeira entrada do laboratório de dispositivos, §6.1 de
`PLANO-FECHAMENTO-LACUNAS.md`):** Xiaomi 25053PC47G (device `onyx`, placa `sun`), SoC Snapdragon
SM8735, arm64-v8a, 11.5 GB RAM, Android 16 (API 36), Vulkan 1.3 com
`android.hardware.vulkan.compute` e deqp level 132645633 — conectado via ADB-over-WiFi.
Cobre o perfil S/A do laboratório; falta ainda um aparelho Mali e um perfil C (fraco)
para a matriz mínima de 6 aparelhos que o plano pede antes de considerar M0 representativo.

| Verificação | Resultado |
|---|---|
| APK debug ARM64 | ✅ gerado e assinado com certificado de desenvolvimento |
| APK release ARM64 | ✅ gerado sem assinatura de publicação |
| Android Lint debug/release | ✅ sem erros |
| Lifecycle portátil | ✅ 7 testes: ativação, pausa/retomada, perda de janela, encerramento, diagnóstico, seis ordens de retomada e keyguard |
| Instalação em aparelho físico | ✅ `adb install -r` — sucesso, sem erro de assinatura/ABI |
| Carregamento da lib nativa | ✅ confirmado via logcat (`nativeloader: Load .../libaether_android.so ... ok`) — `android_main` executa (`Aether.Android: Shell nativo iniciado.`) |
| Abertura em aparelho físico | ✅ `NativeActivity` inicia, fica em foreground (`mFocusedApp` confirmado via `dumpsys activity`), sem `FATAL EXCEPTION`/crash no logcat |
| **Criação real de surface Vulkan** | ✅ **confirmado em hardware físico** — log `Aether.Android: Surface Vulkan pronta: janela=2772x1280, imagens=4..64, fila=0.` contra o driver Adreno real (`AdrenoVK-0`, `vulkan.adreno.so` versão 0800.71, Qualcomm build `208ca19915`) |
| Criação real de swapchain | ✅ imagem natural 1280×2772, 5 imagens, pré-rotação de 90° para display 2772×1280; capability selection e FIFO. A orientação anterior usava dimensões incorretas e achatava o cubo |
| Pipeline e apresentação | ✅ cubo central texturizado e 4.999 mini-cubos, depth, descritor de sampler e upload real. Captura inspecionada em `build/android-validation/rhi-cube-final-20260828/after-lifecycle.png` |
| Toque | ✅ `AInputEvent` rastreia ID do dedo, move/cancel/up e converte deltas normalizados em radianos; renderer não recebe tipos Android. Arraste final de 28/08: `yaw=2.802`, `pitch=-0.777`, em `rhi-cube-normal-20260828/touch-evidence.txt`, com captura após gesto. Não equivale ao teste de UX com 10 usuários |
| Regressão final sem ciclo de tela | ✅ `build/android-validation/rhi-cube-normal-20260828/report.json`: 1.000 frames, 2 retomadas, configuração, trim-memory proxy e capturas; PID 28139 preservado |
| Runner ADB reproduzível | ✅ `tools/validate-android-shell.ps1` instala/limpa, inventaria o aparelho, espera 1.000 frames, marca cada transição no Logcat, detecta crash/ANR/erro Vulkan, captura evidências e restaura o estado global do aparelho em caso de sucesso ou falha |
| VMA + budgets | ✅ hardware: buffer 100.032 bytes, textura 20.480 bytes, depth 14.344.192 bytes; staging volta a zero após upload (pico 16.384 bytes). Valores estáveis nas recriações observadas; não representam RSS total do driver |
| Checkpoint térmico curto | ✅ coleta real via `dumpsys thermalservice` integrada ao runner: execução aprovada em `build/android-validation/rhi-vma-thermal-20260826-171317/report.json`, 61,507 s/11 amostras, média 1,7136 W, pico 4,1096 W e status térmico máximo 0; média abaixo do orçamento de 4 W. Isto valida o coletor e um checkpoint curto, **não** fecha a PoC-C de 30 min |
| Ciclo background→foreground | ✅ **100/100 ciclos automatizados**: mesmo PID `14366`, suspensão e retomada observadas, primeiro frame apresentado após cada retorno, sem ANR/crash. Neste driver a mesma `ANativeWindow` foi retida nos 100 ciclos, um comportamento válido do Android; o teste não exige recriação inexistente |
| Recriação por mudança de configuração | ✅ alternar e restaurar `uiMode` gerou `APP_CMD_CONFIG_CHANGED` nas duas direções, com swapchain/pipeline reconstruídos e PID preservado. Uma rodada manual anterior também percorreu destruição/recriação completa da janela; injeção determinística de `SurfaceLost` ainda não existe no runner |
| Estabilidade visual | ✅ screenshots antes/depois dos 100 ciclos e das duas mudanças de configuração possuem o mesmo SHA-256 `949E1829D4D53BDC63F1B548C88ECF05ECD27FEDD47E8E03FDF4FDB9B2A59719` |
| Memória baixa simulada | ✅ `am send-trim-memory RUNNING_CRITICAL` — processo sobrevive, sem crash (não dispara `APP_CMD_LOW_MEMORY` de verdade, é um proxy do ADB, não o teste completo do plano) |
| Tela em landscape imersivo | ✅ confirmado por screenshot (`screencap`) — 2772×1280, sem barras de sistema visíveis |
| Frame visível | ✅ fundo escuro, cubo checker central e mini-cubos animados. SHA-256 diferente entre capturas é esperado; runner usa `-AllowScreenshotDifference` |
| Screen off/on | ✅ Runner corrigido distingue keyguard de retomada gráfica. `lifecycle-keyguard-20260828-verified/report.json`: 3 retomadas + configuração + screen off/on, PID 4256; desbloqueio 6.218 ms, recriação 43,901 ms e primeiro present 1,540 ms após ativação. O aparelho tem senha (confirmado pelo usuário); falhas históricas preservadas. Ver ANDROID-SHELL.md |
| Rotação para além de landscape e matriz de múltiplos aparelhos | ⏳ `uiMode` foi exercitado; rotação fora de landscape é bloqueada por design por `screenOrientation="sensorLandscape"`; só há um aparelho no laboratório (faltam Mali e perfil C) |
| Revalidação pós-sessão (28/08, tarde) | ✅ `tools/validate-android-shell.ps1` reexecutado no mesmo Xiaomi após os commits desta sessão (extração atômica de assets, `InstancingWorkload`, hot reload): **20 ciclos** background/foreground + mudança de configuração + screen off/on, PID `17496` estável, zero recriações de recurso — confirma que nada regrediu. Profiling de frame separado (`-ProfileSeconds 60`): SurfaceFlinger 60,06 FPS/61,6 s contínuos, `presentFps` 60,06, CPU processo média 2,33 ms (pico 22,1 ms) — consistente com medições anteriores da PoC-A, mesma limitação já documentada (CPU excede o orçamento de 3 ms em picos, matriz de 1 aparelho não fecha o gate). **Achado de ferramental**: rodar os scripts com `powershell` (Windows PowerShell 5.1) corrompe literais de string acentuados no próprio código-fonte `.ps1` (lido com a codepage do sistema, não UTF-8), quebrando o casamento de padrão contra o logcat; `pwsh` (PowerShell 7) também exige forçar `[Console]::OutputEncoding`/`$OutputEncoding` para UTF-8 explicitamente neste ambiente, senão herda `IBM850` do console — documentado aqui para quem for rodar estes scripts localmente fora do CI (que já usa `pwsh` com ambiente configurado) |
| Otimização móvel v10 (30/08) | ✅ Diagnóstico de variantes na mesma câmera provou gargalo de fragment/texture (PBR completo 85,86 FPS/10,25 ms GPU; base-color apenas diagnóstico 120,11/6,45 ms), não falta de carga CPU. World-to-view por frame e simplificações matemáticas equivalentes reduziram o PBR sem mudar a imagem. O import/runtime cria 58 render chunks determinísticos, preserva geometria/blend e usa multi-draw indirect por material (`indirect=multi-draw` no Xiaomi): 82 chamadas CPU da primeira versão caíram para 34. Ponto fixo v10: 99,46 FPS/9,23 ms GPU; rota manual, primeiras cinco janelas 98,34 FPS em média, pior janela 64,39 FPS/13,73 ms com 57/58 chunks. `PowerManager` informou sustained performance não suportado, então o fallback público permaneceu ativo. APK instalado: `build/aether-spatial-indirect-v10-release.apk`, SHA-256 `30A6A5B5FF0FB0C55D8FF6A8E8B906268F49A5F205138DB4D49CF8DA246D6285`. Suíte nativa Release 190/190 e build Android Release aprovados. Ainda não é 120 FPS estável; HZB/LOD e rota reproduzível são os próximos gates. |
| Hotspot/DVFS + material LOD (01/09) | ✅ Pose direcional reproduzível confirmou GPU-bound (CPU 1,2–1,6 ms). `screenrecord` leva o mesmo renderer a 120,113 FPS/6,401 ms GPU média/p95 7,104 ms, enquanto sem gravação o OEM alterna operating points e produz 86–106 FPS. ADPF 7,333 ms piorou contra o alvo correto de 8,333 ms e foi rejeitado; Android 15 agora recebe `prefer_power_efficiency=false` explicitamente. Normal mapping foi o maior eixo isolado (`no-normal` 111,90 FPS). Material LOD compilado remove normal/TBN somente quando o bounds inteiro está além do alcance global, preservando detalhe próximo; alpha cutoff também ocorre antes do PBR. APK mais recente instalado: `build/aether-material-distance-adpf-r6-release.apk`, SHA-256 `0EE006319ECCCBBC45969E251A32E57981840BECBCC35533B48B0D07ACE5A0EA`. 284/284 testes nativos e Release aprovados. 120 FPS sem gravação ainda não é declarado estável: próximo gate é AChoreographer × Swappy e soak intercalado, sem clock privado/dummy load. |

O APK é um **shell gráfico de fundação**, não um editor demonstrativo. Ele
desenha continuamente apenas quando lifecycle está ativo e bloqueia o looper
quando suspenso ou sem renderer, evitando consumo térmico fora de foreground.

## O que é protótipo ou prova de conceito

| Artefato | Natureza | O que prova | O que não prova |
|---|---|---|---|
| `prototype/editor.html` | Protótipo interativo HTML/Canvas | UX de câmera, seleção, gizmos, menu radial e Inspector em landscape | Renderer, desempenho mobile ou integração com a engine |
| `Aether.Flow` | PoC arquitetural funcional | AST única, validação, interpretação, serialização e round-trip parcial com C# | Catálogo completo de nós, CFG geral, integração com Inspector/runtime |
| `native/rhi/device.cpp` + shell Android | Vertical slice gráfico validado em 1 GPU física | Instance/device/surface, swapchain, pré-rotação, VMA/budgets, buffer/imagem/sampler, textura via staging e depth integrados | Compatibilidade na matriz, cache genérico de pipelines, Render Graph executado ou desempenho da cena M2 |
| Device profiles e render graph | Implementação headless testada | Regras de capability, ordenação, barreiras, aliasing e fusão | Execução dessas decisões numa GPU real |

## Limitações conhecidas que um leitor precisa saber

| Onde | Limitação | Consequência |
|---|---|---|
| `.github/workflows/*.yml` | Nenhum workflow rodou no GitHub ainda — o repositório não tem remote configurado (`git remote -v` vazio) | YAML foi parseado e o caminho limpo exato foi reproduzido localmente, incluindo headers Vulkan isolados; a regressão corrente passa em 506/506 testes C# e 165/165 nativos. A primeira execução hospedada ainda é necessária para validar permissões, cache, publicação de artefatos e ambiente dos runners |
| `World.GetComponent<T>`/`Chunk.GetSpan<T>` | APIs legadas continuam marcando escrita em toda chamada porque devolvem acesso mutável | Compatibilidade é preservada sem falsos negativos. Código novo e todos os sistemas internos usam `Read`/`Write` e spans explícitos; remoção das APIs antigas exige janela de depreciação |
| `Aether.Flow` | Biblioteca de nós mínima (~12 nós), não o catálogo da Parte 9.5 | Prova a tese, não entrega o produto |
| `prototype/editor.html` | Rasterização por painter's algorithm em Canvas 2D | Artefatos de ordenação entre objetos grandes que se interpenetram. Irrelevante para o que o protótipo testa |
| `VulkanSwapchain` | Um único frame em voo e máximo técnico atual de 8 imagens | Correto para o shell gráfico mínimo; múltiplos frames em voo, pacing e política dinâmica pertencem ao RHI completo (item 2.1 do plano principal) |
| `TriangleRenderer` | Render hardcoded, sem depth, vertex buffer, textura ou Render Graph. Não é mais o renderer ativo no loop principal (substituído por `InstancedRenderer`, ver PoC-A) | Continua no repositório como evidência histórica do item 5.2 (shell gráfico mínimo, 1.000 frames/100 retomadas validados) — a classe compila e funciona, só não é chamada de `android_main.cpp` |
| `InstancedRenderer` | Sample padrão esfera/PBR 8K com cena serializável e assets cozidos; cubos/PoC-A preservados. Só um par mesh/material conhecido por fixture, sem catálogo GPU genérico ou Render Graph | Não inclui Inspector, undo/NoCode de materiais, múltiplos pares simultâneos nem renderer PBR completo; MATERIAL-PREVIEW.md |
| `MaterialPreviewResources` | ~114 MiB de payload dos três mapas ASTC; fallback 1K; limite de residência inicial e worker de carga, sem UI de progresso nem streaming | 8K não é garantia em todo aparelho. Cerca de 130,4 MiB adicionais de assets no pacote; 16K não habilitado. Sombras, multiscatter, SH/probes e pós completo pendentes |
| `VulkanMemoryAllocator`/upload | Buffers e imagens 2D de uma camada com cadeia de mips; upload inicial RGBA8/ASTC/RGBA16F na fila gráfica, exclusivo do worker durante carga do material; device/allocator sobrevivem aos recursos | 2.1.2/2.1.3 permanecem parciais: cotas estáticas precisam de calibração por pressão/perfil; faltam `VK_EXT_memory_budget`, streaming e pipeline cache completo |
| PoC-C térmica | Runner integra CPU, FPS exibido e ibat×vbat com cobertura/integração temporal. Smoke de 28/08: 60,039 FPS, mínimo 59/1 s, média 2,273 W/status térmico 0. Ensaio longo interrompido a pedido do usuário: 769,373 s de CPU recuperados do Logcat, sem série longa válida de energia/FPS | PoC-C continua parcial: 30 min não foram concluídos, sensor não alimenta o PowerGovernor e matriz pendente. A perda de dados ao interromper motivou persistência JSONL incremental; não reconstruímos métricas inexistentes. Ver PROFILING-ANDROID.md |
| `InstancedRenderer` (PoC-A) | `m0-batch-20260828/optimized-smoke/`: 60,039 FPS exibidos; CPU média 2,412 ms/pico 18,237 ms, thread de render média 1,217 ms/pico 2,318 ms. Cache de 100 KB prepara cores/layout sem alterar a animação; interop wall médio 0,537 ms, antes 1,192 ms | PoC-A parcial: o processo excede 3 ms. Simpleperf atribuiu custo a trigonometria e threads Binder/BLAST, mas não a causa de cada pico; não há prova contra GC/JIT. Faltam matriz e timestamps GPU; ver PROFILING-ANDROID.md |
| Identidade do C# empacotado | Gradle agora publica C# Release para ARM64 e gera assets/manifesto/build ID pelo conteúdo; a DLL legada é excluída. Android troca arquivos atomicamente e confirma a geração por último; tamanho igual não significa versão igual | Corrigido e integrado: build debug/release/lint e verificações de identidade passam; relatório v4 compara o build ID do APK com o confirmado pelo processo. Não é hot reload: a atualização ocorre antes de carregar CoreCLR |
| `triangle_spirv.h`/`instanced_spirv.h` | Geração reproduzível com `tools/generate-embedded-shaders.ps1`, NDK pinado, `glslc` e `spirv-val`; `-Check` detecta divergência no CI Android | Compilação é offline/development. Slang/HLSL, reflection, cache e hot reload runtime continuam no plano principal |
| `PhysicsSyncSystem` × dirty tracking do ECS | A sincronização cinemática mantém cache de alvo por body mesmo com versões exatas por chunk/coluna | A versão elimina trabalho quando uma coluna inteira está estável, mas não identifica qual body mudou nem conserva o alvo anterior necessário à parada final. Integrar a versão como fast-path é otimização futura; o cache por body continua sendo a fonte correta |
| `Trigger` durante edição/runtime | Alterar `Enabled` ou `EventLayerMask` depois que o corpo nativo já foi criado não muta o Jolt em-place | O contrato explícito atual é `PhysicsSyncSystem.DestroyBody` + próximo `Step`, que recria o corpo com a configuração nova. Hot-edit incremental e Inspector pertencem ao slice de editor; não há estado implícito divergente |
| `Trigger` e sleep | Corpos móveis sobrepostos a sensores são mantidos acordados enquanto o par permanece ativo | Evita `Exit` falso causado pelo lifecycle de contatos adormecidos do Jolt, mas consome simulação proporcional ao número de overlaps persistentes; precisa entrar no profiling móvel da Fase 4 |
| Telemetria global de interop | A telemetria implementada em `PhysicsBodyInteropStatistics` cobre criação/destruição de corpos; as demais APIs nativas ainda não alimentam um contador global comum | O risco específico de spawn massivo (`GAP-PHY-04`) está medido e fechado. O gate global `interop.native_calls_per_frame` continua `device-required` e precisa agregar renderer, Android e demais subsistemas na PoC-A |
| `jolt_bridge.h` (juntas) | `AetherJointDesc` só expõe `EConstraintSpace::WorldSpace` — não há como descrever uma junta no referencial local de um corpo | Criar uma junta antes de posicionar os corpos no lugar final não funciona direito (os pontos são absolutos, não relativos); é preciso posicionar os corpos primeiro, depois criar a junta com coordenadas mundiais. Espaço local é um incremento futuro, documentado no comentário de `AetherJointDesc` |
| `HingeConstraint` do Jolt | `mLimitsMin`/`mLimitsMax` são exigidos em `[-pi,0]`/`[0,pi]` — não existe "sem limite" fora dessa faixa como em Slider (`FLT_MAX`) | Uma dobradiça de rotação livre contínua (ex.: roda motorizada) precisa usar exatamente `-pi`/`+pi`, não um valor "bem grande" qualquer — é o ponto exato em que o Jolt desliga a checagem de limite internamente (`HingeConstraint::SetLimits`). Documentado em `jolt_bridge.h`, `PhysicsWorld.cs` (`JointDesc`) e nos testes |
| `PhysicsSyncSystem`/fachada de juntas | Nenhuma sincronização automática entre `RigidBody`/componente ECS de junta — `PhysicsWorld.CreateJoint` é chamado direto pelo código do usuário, não por um `JointSyncSystem` análogo ao `PhysicsSyncSystem` de corpos | Não existe hoje um componente ECS `Joint`/`HingeJoint` que o `PhysicsSyncSystem` resolva automaticamente a partir de duas entidades — a fachada 4.1.3 é a API C# (`PhysicsWorld.CreateJoint`), não um componente declarativo. Adicionar isso é extensão natural, não escopo deste item (que era "juntas e motores" na física, não "juntas no ECS") |
| `Aether.Flow` | Nenhum nó de física (`physics.raycast`, `physics.overlap`, etc.) — o plano (tabela 9.5) reserva a categoria "Física" com nós nomeados, mas eles não existem | O item 4.1.4 diz "queries expostas a script E a nós" — a parte "script" está feita (`PhysicsWorld` é chamável direto por qualquer C#, mesmo padrão que os próprios testes usam); a parte "nós" foi deliberadamente adiada. Motivo: o Aether.Flow hoje só conhece o ECS `World`, nunca um `PhysicsWorld` — não existe um jeito do interpretador/gerador de código referenciar um mundo de física em contexto (não há `Behavior`/API de gameplay da Parte 11.3 do plano ainda implementada, que é onde o plano prevê essa ponte). Resolver isso de verdade é decisão arquitetural maior (como um nó Flow acessa recursos externos ao ECS), não uma extensão isolada de "adicionar mais um nó" — forçar um `PhysicsContext.Current` estático só para destravar os nós seria a gambiarra que este projeto se recusa a fazer |
| `AetherPhysics_CreateCharacter` | Sempre cria com a forma DE PÉ (`StandingHalfHeight`) — não há "criar já agachado" | Nascer num espaço apertado demais para a forma de pé (ex.: dentro de um vão baixo) faz a resolução de penetração da CRIAÇÃO empurrar o personagem para uma posição inesperada — geralmente para cima de um teto fino, não para o chão abaixo — antes mesmo de um `SetCharacterCrouching(1)` seguinte ter qualquer efeito (`SetShape` só age sobre a posição atual, não reposiciona pela geometria da forma NOVA). Para entrar num vão baixo, crie o personagem num espaço livre, agache, e só então mova-o até lá — documentado em `jolt_bridge.h` |
| `CharacterVirtual::Update`/`UpdateCharacter` | Nunca integra gravidade na velocidade vertical do próprio personagem — só usa `gravity` para empurrar objetos abaixo dele | Responsabilidade inteira do chamador (mesmo padrão documentado no comentário oficial de `CharacterVirtual::ExtendedUpdate` do Jolt, não invenção nossa): esquecer de somar gravidade a `SetCharacterVelocity` a cada frame faz o personagem flutuar. Ver `PhysicsCharacterTests.cs`/`test_character_bridge.cpp` para o padrão de acumulação correto — e note que o padrão usado nos testes de DEGRAU é deliberadamente diferente (velocidade vertical reinicia a cada frame, não acumula) porque acumular atrapalha `WalkStairs`; ver limitação abaixo |
| Testes de degrau (`Character_SobeDegrauComUpdateCharacter`) | A velocidade vertical usada para "andar" NÃO acumula frame a frame (reinicia em `-g*dt` a cada chamada, não em `v_anterior - g*dt`) | Padrão deliberado, não descuido: acumular gravidade livremente faz o personagem ganhar momento de queda vertical significativo mesmo andando devagar sobre um piso, o que atrapalha `WalkStairs` a "ver" o degrau como subível (o character parece estar caindo rápido demais para o algoritmo tentar). É diferente do padrão de queda livre pura (`StepCharacterFreefall`, que acumula normalmente) — cada cenário de movimento tem sua própria composição de velocidade, não existe uma fórmula universal |
| Character sobre objeto fino no caminho vertical | O algoritmo de recuperação de penetração do Jolt sempre resolve pelo caminho de MENOR penetração — um personagem que penetra pouco um teto fino por cima mas muito o chão por baixo é empurrado para CIMA do teto, não mantido no chão | Medido experimentalmente ao desenhar o teste de "agachar sob teto baixo": um vão menor que a altura da forma agachada (ex.: 1.2m de vão para uma cápsula de 1.4m agachada) faz o personagem "vazar" para cima de um teto de 0.1m, mesmo estando exatamente encostado no chão (penetração zero) — não é bug nosso, é o comportamento correto do algoritmo para uma geometria que de fato não comporta a forma. O vão precisa ter folga real (testado com 0.2m de sobra) para o teste ser sobre a mecânica de agachar, não sobre essa interação de geometria-limite |
| Física 2D (`GAP-2D-01`) | A comparação A/B existe e está verde no host, mas ainda não há evidência nos perfis móveis B e C exigidos pelo gate | Box2D v3.1.1 permanece isolado aos benchmarks. `ADR-013-PHYSICS-2D-BACKEND.md` não aceita nem rejeita o backend antes dos relatórios Android; implementar API/ECS de Box2D agora anteciparia indevidamente o plano principal |
| `EAllowedDOFs` travado | Travar graus de liberdade no Jolt NÃO reduz o custo de CPU por corpo no solver — é uma restrição de comportamento/correção, não uma otimização de performance | Confirmado por leitura completa de `MotionProperties.h/.cpp/.inl`: toda operação de massa/inércia/força é calculada em SIMD 3-wide completo e só MASCARADA (zerada seletivamente) depois — não há early-exit por DOF travado em nenhum lugar do motor. Um corpo `Plane2D` custa aproximadamente o mesmo que um corpo 3D pleno de mesma forma; o benchmark de 4.1.6 mede throughput absoluto do Jolt, não um ganho de "modo 2D restrito" sobre "modo 3D" |
| `AETHER_PHYSICS_DETERMINISTIC` (4.1.8) | O nome do item no plano ("determinismo em ponto fixo") diverge do que foi implementado (`CROSS_PLATFORM_DETERMINISTIC` do Jolt, que continua usando `float`, não ponto fixo inteiro) | Ponto fixo de verdade exigiria reescrever o solver/matemática interna do Jolt inteiros para um tipo fixed-point — reescrita profunda de código de terceiros vendorizado, não uma opção de configuração; fora de escopo desta fatia. `CROSS_PLATFORM_DETERMINISTIC` é o que o próprio Jolt oferece nativamente para o mesmo objetivo prático (reprodutibilidade entre plataformas), sem tocar uma linha do código dele — ver comentário completo em `native/CMakeLists.txt` |
| `AETHER_PHYSICS_DETERMINISTIC` (4.1.8) | Os testes de `test_determinism.cpp` provam determinismo RUN-TO-RUN (mesma máquina/build, múltiplas execuções bit-exatas), não determinismo CROSS-PLATFORM de verdade | Provar cross-platform de verdade exigiria rodar a mesma simulação em hardware/SO fisicamente diferentes e comparar os resultados — fora do alcance de uma suíte de teste rodando numa única máquina de CI/dev. Run-to-run é pré-requisito necessário (se a mesma máquina não reproduz a própria simulação, nenhuma outra reproduziria) mas não suficiente sozinho; a garantia cross-platform vem da CONFIGURAÇÃO de build (documentada e aceita pelo próprio Jolt), não de algo que este teste consiga verificar sozinho. Documentado no cabeçalho de `test_determinism.cpp` |

## Correções desta revisão (não são limitações — já resolvidas)

- **Diagnóstico do timeout ao reacender a tela (28/08):** instrumentação por
  evento confirmou ausência de foco enquanto o keyguard estava visível;
  recriação gráfica em dezenas de milissegundos, não um bloqueio de 20 s.
  O aparelho possui senha. O runner agora confirma tela ligada/desbloqueada,
  separa espera por autenticação e espera por frame, preserva diagnóstico antes
  do cleanup e retorna `blocked`/código 1 se não houver desbloqueio. Não foi
  removida a exigência de foco nem alterada a proteção do aparelho. Builds
  debug/release/lint, 130 testes C++, 442 C# sem skips e 12 do runner passaram.

- **`TransformSystem` / subgate 1.3.6 — a conclusão anterior sobre a lentidão ARM64 estava
  incorreta e a correção era necessária.** O benchmark antigo fazia múltiplos lookups de
  `World`/arquétipo por entidade e recompunha a topologia a cada frame; por isso não era válido
  concluir que o ECS não participava do custo. Um controle no mesmo Android mediu a mesma
  composição em array C# denso em ~31 ms e em C++ NDK `-O3` em ~1,9 ms: havia dois fatores,
  overhead de acesso do caminho ECS antigo **e** throughput/codegen ARM64 do loop matemático.
  Agora estrutura e parentesco compilam um plano topológico invalidado por mudanças estruturais,
  versão de hierarquia ou dirty version de `Parent`. Chunks usam buffers estáveis no Pinned Object
  Heap e uma única chamada P/Invoke por frame passa descritores blittable para
  `aether_transform`; não há crossing nem cópia por entidade. A biblioteca ausente/ilegível cai
  automaticamente no mesmo algoritmo gerenciado. Resultado no Xiaomi SM8735, três corridas
  consecutivas de 60 amostras/100 mil entidades: p50 total 0,59/0,59/2,29 ms, p95
  0,66/0,66/2,47 ms, p99 0,68/0,69/3,52 ms e zero GC/frame. A variação acompanha DVFS:
  `taskset f0` fixa afinidade, não frequência; durante a investigação a CPU variou de 633 MHz a
  2,96 GHz mesmo com status térmico nominal. Validação: 300/300 C#, 112/112 C++, 20/20 testes de
  hierarquia no Android e `assembleDebug` com `libaether_transform.so` empacotada.
- **PoC-A (item 0.2) — bug real de use-after-scope em `DotNetHost`, pego só em execução no
  aparelho, não por inspeção.** `DotNetHost::initialize` guardava só um `const char *` para
  `managedAssemblyPath` ("sobrevive ao objeto", dizia o comentário original — suposição errada).
  O primeiro consumidor (`Ping`, chamado na mesma função que monta o buffer) funcionava porque a
  pilha do caller ainda não tinha sido reutilizada; o segundo consumidor
  (`FillInstanceBuffer`, resolvido bem depois por `InstancedRenderer::initialize`, em outro ponto
  do lifecycle) lia memória de pilha já sobrescrita e falhava com
  `hostfxr` código `0x80070002` ("arquivo não encontrado" — mensagem enganosa, o arquivo existia;
  o path lido é que virou lixo). Isolado comparando um teste standalone (que funcionava, porque
  chamava os dois métodos na mesma função) contra a integração real (que falhava) — a diferença
  de comportamento entre os dois foi o que expôs a causa. Corrigido trocando o ponteiro por uma
  cópia própria (`char[512]` interno, copiado uma vez em `initialize`) — `DotNetHost` agora é dono
  de verdade da própria string, não depende do caller manter o buffer vivo pelo tempo de vida do
  host.
- **§4.2 (CI confiável) — skip silencioso de teste de física deixou de ser indistinguível de
  "passou de verdade".** Os 43 testes C# que tocam `aether_physics` (Physics/PhysicsJoint/
  PhysicsQuery/PhysicsCharacter/Physics2DTests) tinham cada um sua própria cópia de
  `NativeLibraryAvailable()` e retornavam cedo silenciosamente quando a DLL nativa não estava
  presente — um `TUDO VERDE` local não distinguia "43 testes passaram" de "43 testes nunca
  rodaram". Centralizado em `NativeInterop.PhysicsLibraryAvailable()` (conta quantos testes
  pularam por ausência da lib) e o `TestRunner` agora aplica a regra do plano: com
  `AETHER_REQUIRE_NATIVE=1` (o job de integração), qualquer skip por lib nativa ausente vira
  falha explícita com contagem; sem a variável (fluxo de desenvolvimento local sem toolchain
  nativa), o comportamento continua idêntico ao de antes. Validado nos dois sentidos: com a DLL
  presente `AETHER_REQUIRE_NATIVE=1` também dá `TUDO VERDE` (não é um modo mais rígido por
  acaso, só quando a causa real de skip é ausência de lib); com a DLL removida, falha com exit
  code 1 e a mensagem nomeia os 43 testes silenciosos. `unit` (padrão) e `interop`
  (`AETHER_REQUIRE_NATIVE=1`) continuam o mesmo executável, mas agora rodam em jobs separados.
  O registro `metrics/budgets.v1.json` cobre os seis domínios exigidos e o validador
  rejeita tanto contrato incompleto quanto valor fora do limite; a regressão de 301
  chamadas/frame falha de propósito. O clean build também foi corrigido para compilar o
  nativo antes do gerenciado e usa somente os headers `vulkan`/`vk_video` extraídos do NDK,
  evitando contaminar o build host com headers C do Android.
- **`GAP-FLOW-01` — controle terminal simétrico no AetherFlow.** `flow.return`,
  `flow.break` e `flow.continue` são nós explícitos sem saída de execução. Parser,
  gerador, interpretador, validador e serializer compartilham o mesmo contrato;
  sinais atravessam `if` aninhados, `return` encerra o evento e `break`/`continue`
  são consumidos apenas pelo laço mais interno. O parser rejeita retorno com valor
  em eventos `void` e instrução inalcançável em vez de reinterpretá-los. Nove
  regressões novas incluem laços aninhados, round-trip `.aflow` e compilação real
  offline do C# original/regenerado; 48/48 testes Flow e 274/274 testes C# passam.
- **`GAP-PHY-01` — capacidades e overflow de física explícitos.** A ABI V2
  separa `maxBodies`, `maxBodyPairs`, `maxContactConstraints` e
  `maxBroadPhasePairs` (`mMaxInFlightBodyPairs` no Jolt), versiona descritores,
  preserva V1 e dimensiona memória transitória pela carga declarada. `StepV2`
  devolve flags e mantém contadores por categoria; logs usam primeira ocorrência
  e potências de dois para limitar repetição. Debug/teste permanece fail-fast;
  o probe Release comprovou warning recuperável e `flags=0x7`. Três regressões
  nativas exercitam ABI, V1 e pilhas densas; três regressões C# exercitam a fachada.
- **`GAP-PHY-02` — cinemáticos com autoridade ECS→Jolt.** A ABI versionada
  expõe `AetherPhysics_MoveKinematicV2`; `PhysicsSyncSystem` envia o alvo antes do
  step e nunca devolve a transform cinemática pelo caminho dinâmico. Um cache
  exato por body envia somente mudanças reais e uma parada final, mantendo frames
  estáveis sem P/Invoke. Regressões cobrem alvo traduzido/rotacionado, transporte
  de corpo apoiado e character, e a ausência de feedback/crossings permanentes.
- **`GAP-JOB-01` — ciclos gerais detectados antes da espera.** `JobEntry`
  registra pré-requisitos permanentes e a aresta temporária de cada `Complete()`
  interno. A publicação e a busca de caminho ocorrem sob o mesmo lock, então
  duas threads não conseguem fechar um ciclo sem detecção. A mensagem lista o
  caminho `job#N -> ... -> job#N`; o timeout ficou restrito a watchdog externo.
  Três regressões adicionais cobrem ciclos por dependência e ciclos concorrentes
  de 2–3 jobs; 10 repetições da suíte completa passaram.
- **Shell gráfico Vulkan — quatro falhas de lifecycle que a captura visual feliz não
  denunciava.** (1) A primeira implementação recriava a swapchain antes de destruir os
  framebuffers do `TriangleRenderer`; `vkDeviceWaitIdle` evita uso pela GPU, mas não muda a
  regra de lifetime: uma `VkImageView` não pode ser destruída enquanto um `VkFramebuffer`
  ainda a referencia. A ordem agora é renderer/framebuffers → image views/swapchain →
  surface/device. (2) Se `vkCreateSwapchainKHR` falhasse, a swapchain antiga era destruída,
  embora a spec só a aposente quando a criação da nova tem sucesso; uma falha recuperável de
  resize virava perda completa do renderer. A troca agora preserva a antiga na falha e só
  publica os novos handles depois de obter imagens/views válidas. (3) `SurfaceLost` e
  `OutOfDate` eram reduzidos ao mesmo `bool`; tentar resolver surface perdida recriando só a
  swapchain não pode funcionar. `SwapchainStatus` ganhou `FatalError`, o renderer preserva os
  estados e o shell escolhe entre reconstruir swapchain ou surface inteira. (4)
  `compositeAlpha=OPAQUE` era presumido; agora é escolhido entre os bits realmente suportados,
  e uso de color attachment/limite de imagens são validados antes da criação.
- **Rotação landscape, visibilidade e submissão global (30/08/2026).** A troca entre os dois
  lados de `sensorLandscape` podia preservar uma `VkSurfaceKHR` com transformação
  antiga porque 2772×1280 continua 2772×1280; o HUD ainda removia o sinal da matriz.
  O shell agora consulta `Display.getRotation()`, cancela gestos na mudança, recria a
  Surface apenas quando a rotação física muda; o HUD usa somente a troca de eixos,
  pois suas posições já estão no espaço visível do Android. No renderer, frustum
  culling conservador, render chunks espaciais determinísticos e multi-draw indirect
  por material foram integrados antes da gravação Vulkan, com scratch lists sem
  alocação e telemetria de draws/triângulos. A suíte nativa Release passa em 190/190,
  o APK arm64 Release compila e a v10 foi instalada no Xiaomi. Capturas ADB confirmam
  joystick no canto inferior esquerdo e imagem equivalente. O ponto fixo passou de
  85,86 FPS/10,25 ms GPU na v7 para 99,46/9,23 ms na v10; a rota ainda possui janela
  de 64,39 FPS quando 57/58 chunks ficam visíveis. A virada física de 180° ainda
  requer confirmação manual, e a rota móvel reproduzível continua pendente.
- **Build host Windows — alocação alinhada portável.** O clean build com LLVM-MinGW revelou
  que essa CRT não fornece `std::aligned_alloc` de forma utilizável. `alignedAlloc/alignedFree`
  agora usam o par obrigatório `_aligned_malloc/_aligned_free` em `_WIN32` e preservam
  `std::aligned_alloc/free` nas demais plataformas. Os seis testes nativos de allocator passam
  no build host limpo e o mesmo header compila pelo NDK no APK Android.
- **Shaders do triângulo conferidos contra a fonte.** `triangle.vert` e `triangle.frag` foram
  recompilados com o `glslc -O` do NDK 27.1, validados com `spirv-val` e comparados byte a byte
  com os arrays de `triangle_spirv.h`: SHA-256 idênticos para vertex e fragment. Isso elimina
  divergência atual entre fonte e embed, embora a geração automática continue pendente na
  toolchain de shaders 2.2.
- **`ComponentRegistry` calculava offset de campo com `Marshal.OffsetOf`**, que reflete o layout de
  interop/marshaling, não o layout gerenciado real que o resto da engine usa (`Unsafe`/`MemoryMarshal`).
  `bool` marshala como o `BOOL` de 4 bytes do Win32 por padrão; comprovado experimentalmente nesta
  máquina que isso diverge do offset real sempre que um `bool` é seguido por um campo sem exigência de
  alinhamento de 4 bytes (ex.: outro `bool`/`byte`) — nenhum componente registrado até agora tinha
  `bool`, então o bug era invisível. Corrigido para calcular o offset observando o layout gerenciado de
  verdade (preenche o campo com um valor "todo-bits-1" via reflexão direta, sem marshaling, e observa
  qual byte mudou). Teste de regressão:
  `OffsetDeCampoAposBool_NaoUsaLayoutMarshaledDoInteropERespeitaOTamanhoRealDaStruct`.
- **`Aether.Flow`: `if` já pode ser seguido de mais instruções no mesmo bloco.** `flow.if` ganhou um
  terceiro pino de execução, `"depois"` — o ponto de convergência dos ramos `"entao"`/`"senao"`, existindo
  `else` ou não — espelhando exatamente o padrão que `flow.while`/`"fim"` já usava. Gerador, parser e
  interpretador dos três atualizados de forma simétrica; validador e serializador de `.aflow` já eram
  genéricos sobre pinos e não precisaram mudar. Corrige também um bug latente (nunca exercitado, porque a
  checagem antiga sempre lançava antes): `CSharpToFlow.ParseIf` devolvia `"senao"` como o pino de
  continuação, quando esse pino é a ENTRADA do ramo else, não uma saída de convergência.
- **Integração do Jolt (4.1.1): três bugs reais pegos só ao compilar/testar de verdade, não por inspeção.**
  (1) `JPH::RVec3` é um `using RVec3 = Vec3` (não um tipo distinto) quando `DOUBLE_PRECISION=OFF` — a
  configuração que escolhemos, porque a engine usa float em toda a matemática — então um segundo overload
  `FromJolt(JPH::RVec3)` ao lado de `FromJolt(JPH::Vec3)` era uma redefinição da mesma função, não uma
  sobrecarga; o compilador acusou. (2) `BroadPhaseLayerInterface::GetBroadPhaseLayerName` é puro quando
  `JPH_PROFILE_ENABLED` está definido (ligado por padrão no build Debug do Jolt) — `BroadPhaseLayerInterfaceImpl`
  não a implementava, então a classe ficava abstrata e nem podia ser instanciada como campo de
  `AetherPhysicsWorld`; corrigido implementando-a (só usada para rotular camadas em captura de profile, não
  afeta simulação). (3) Os handlers padrão do Jolt para `Trace`/`AssertFailed` (`DummyTrace`/`DummyAssertFailed`,
  ver `Jolt/Core/IssueReporting.cpp`) descartam a mensagem e o assert simplesmente devolve `true` — dispara
  um trap sem nenhum diagnóstico legível. Instalados handlers reais em `jolt_bridge.cpp`
  (`TraceImpl`/`AssertFailedImpl`), no mesmo estilo stderr de `core/assert.cpp` (`AE_CHECK`), antes de
  qualquer uso do Jolt (`EnsureGlobalTypesRegistered`). Além disso, o teste de queda livre precisou de uma
  tolerância maior para a velocidade (0.15 m/s, não 0.05) depois de medir que o `mLinearDamping` padrão do
  Jolt (0.05, `dv/dt = -c*v`, ligado por padrão em todo corpo dinâmico) desvia a velocidade real da fórmula
  ideal `v = -g*t` em ~0.06 m/s mesmo em queda livre "pura" — não é imprecisão do teste, é o Jolt simulando
  um amortecimento real que a fórmula ideal não modela.
- **Fachada C# de física (4.1.2): primeiro P/Invoke real do repositório — não havia nenhum precedente**
  (`[DllImport]`/`[LibraryImport]`) em `managed/`. Duas decisões de fronteira que valem registrar: (1) o
  CMake só produzia `aether_physics` como biblioteca ESTÁTICA (`native/CMakeLists.txt`); P/Invoke exige uma
  biblioteca dinâmica carregável em runtime, então foi adicionado um alvo irmão `aether_physics_shared`
  (`SHARED`) compilando o mesmo `jolt_bridge.cpp` — os dois alvos coexistem (o estático continua linkado
  direto no executável de teste nativo `aether_tests`, sem carregamento em runtime). (2) O nome do artefato
  gerado por padrão diverge da convenção de resolução do .NET: MinGW/CMake no Windows prefixam `lib` por
  padrão (`libaether_physics.dll`), mas `[LibraryImport("aether_physics")]` resolve para
  `aether_physics.dll` (sem prefixo) no Windows e `libaether_physics.so` (com prefixo) no Linux — são
  convenções de plataforma DIFERENTES. Corrigido com `set_target_properties(... PROPERTIES OUTPUT_NAME
  "aether_physics")` mais `PREFIX ""` condicionado a `WIN32` (o Linux já usa o prefixo default do CMake, que
  bate com o que o runtime espera). Sem isso, `DllNotFoundException` em runtime — nenhum erro de compilação
  denunciaria o problema. `float3`/`quaternion` (item 1.5) são reusados diretamente como parâmetros
  blittable dos bindings — têm o mesmo layout de `AetherVec3`/`AetherQuat`, evitando duplicar um par de
  structs só para a fronteira P/Invoke.
- **Juntas e motores (4.1.3): três bugs reais pegos só ao rodar o teste real contra o Jolt, não por
  inspeção.** (1) Criar uma junta trava os dois corpos com `JPH::BodyLockWrite` (necessário porque
  `Constraint::Create` pede `Body&`, não `BodyID` — diferente do resto da fronteira, que só usa
  `BodyInterface`); dois `BodyLockWrite` sequenciais disparam o assert de possível deadlock do Jolt
  (`PhysicsLock.h`: "A lock of same or higher priority was already taken") porque o segundo pega a
  MESMA categoria de mutex (`PerBody`) que o primeiro já detém. Corrigido trocando por
  `JPH::BodyLockMultiWrite`, que trava N corpos de uma vez resolvendo a ordem de mutex internamente —
  e o mesmo problema reapareceu entre o escopo desse lock e a chamada de `ActivateBody` logo depois
  (ver item 3), corrigido isolando a criação da constraint num bloco `{}` próprio para o lock ser
  liberado antes do `ActivateBody`. (2) Um corpo mantido parado por uma junta (ex.: pêndulo em
  repouso) é colocado para dormir pelo Jolt como qualquer corpo dinâmico inativo — e destruir a junta
  NÃO o acorda sozinho: ficava "congelado" no ar indefinidamente até algo mais o acordar por acidente.
  Corrigido chamando `JPH::BodyInterface::ActivateBody` nos dois corpos tanto em `CreateJoint`
  (senão a junta nova "não pega" num corpo já dormindo) quanto em `DestroyJoint` (senão o corpo solto
  não recomeça a cair). (3) Os testes de motor (Hinge velocidade, Slider posição) inicialmente
  "falhavam" com o motor girando/convergindo bem mais devagar/menos que o esperado — não era bug de
  sinal ou eixo, era `maxForceOrTorque`/limite de força do motor de teste baixo demais para a
  inércia/massa do corpo (mesma classe de pegadinha do `mLinearDamping` documentada para 4.1.1): com
  torque/força uma ou duas ordens de grandeza maiores, ambos os motores convergem corretamente para o
  alvo. Documentado nos comentários dos testes (`test_joint_bridge.cpp`, `PhysicsJointTests.cs`) para
  não ser "corrigido" de volta por engano numa revisão futura achando que é imprecisão de teste.
- **`prototype/editor.html` não tinha `<meta charset="utf-8">`** (é um fragmento HTML solto, sem
  `<head>`). Servido por um servidor HTTP que não declara `charset=utf-8` no header
  `Content-Type` (ex.: `python -m http.server` puro), o navegador cai para Latin-1 por padrão e
  corrompe todo texto acentuado do editor ("Chão" vira "ChÃ£o") — bug pré-existente, não introduzido
  nesta revisão, mas só descoberto ao validar o gizmo de junta num servidor local de teste (abrir o
  arquivo direto via `file://` não expõe o problema, porque não há header HTTP envolvido). Corrigido
  adicionando a meta tag.
- **Queries (4.1.4): um bug de ergonomia de API pego na revisão do próprio código, antes de rodar
  qualquer teste.** O parâmetro opcional `ignoreBody` de `RayCastAll`/`ShapeCastClosest`/`OverlapShape`
  tinha `PhysicsBodyHandle ignoreBody = default` — mas `PhysicsBodyHandle.Invalid` é `static readonly`
  (construído em runtime), não `const`, então não pode ser o valor default de um parâmetro; o C#
  aceita silenciosamente `default(PhysicsBodyHandle)` (campo `Value=0`) em seu lugar. O problema:
  `Value=0` é um handle REAL e válido (corpo de índice 0), não "nenhum corpo para ignorar" — um
  chamador que não passasse `ignoreBody` explicitamente estaria, sem saber, ignorando qualquer corpo
  que por acaso fosse o primeiro criado no mundo. Corrigido trocando a assinatura para
  `PhysicsBodyHandle? ignoreBody = null`: `null` (ausência real de valor) é inequivocamente distinto
  de qualquer handle, válido ou `Invalid`, que o chamador possa passar. Também extraída a conversão
  `AetherShapeDesc→JPH::Shape` de dentro de `AetherPhysics_CreateBody` para uma função `ToJoltShape`
  compartilhada (antes só existia inline em `CreateBody`), reusada pelas três queries que também
  precisam de forma sem criar um corpo.
- **Character controller (4.1.5): nenhum bug de compilação/deadlock desta vez (ao contrário de
  4.1.3), mas três horas de geometria de teste mal calculada até acertar cenários que refletissem
  o comportamento real do Jolt** — vale registrar para não repetir o mesmo erro em testes
  futuros de personagem/colisão. (1) Uma caixa larga rotacionada para simular "parede/rampa
  íngreme" não cobre a mesma área XZ que a caixa original antes de rotacionar — um personagem
  posicionado na mesma coordenada X/Z da versão não-rotacionada cai no vazio ao lado dela,
  reportando `InAir` (não `OnSteepGround`), um falso negativo que parece "o Jolt não detectou a
  rampa íngreme" quando na verdade é "o personagem nunca tocou a rampa". Resolvido calculando a
  posição real da face rotacionada antes de posicionar o personagem, não chutando a mesma
  coordenada da caixa não-rotacionada. (2) Mover um personagem horizontalmente contra a QUINA de
  um objeto fino (quando a intenção era "entrar por baixo dele") trava na borda — `WalkStairs`
  só dispara com movimento horizontal desejado e a normal da quina pode ser classificada como
  "rampa íngreme demais" mesmo sem ser uma rampa de verdade, cancelando o avanço via
  `CancelVelocityTowardsSteepSlopes`. Resolvido evitando esse caminho de teste inteiramente:
  entrar num vão baixo tem que ser testado com o personagem JÁ POSICIONADO dentro do vão (mesma
  disciplina documentada na limitação de `AetherPhysics_CreateCharacter` acima), não simulando
  uma aproximação lateral genérica. (3) O extrato de estado do `AetherCharacterGroundState` no
  código de teste inicial media só a posição/estado FINAL depois de várias dezenas de frames —
  para rampas íngremes isso é sempre ambíguo (o personagem pode ter escorregado de volta para
  `InAir`, que parece um resultado "neutro" mas na verdade mascara se a detecção de rampa íngreme
  funcionou). Corrigido medindo o PRIMEIRO frame em que o estado deixa de ser `InAir`, não o
  estado final — o padrão usado em `character_sobre_rampa_ingreme_demais_reporta_steep_ground`/
  `Character_ContraRampaIngremeDemais_PrimeiroContatoNaoEhOnGround`.
- **Física 2D (4.1.6): o próprio benchmark de carga alta (1000 corpos empilhados) pegou um bug
  real de dimensionamento pré-existente em `AetherPhysicsWorld`** — `maxBodyPairs`/
  `maxContactConstraints` (jolt_bridge.cpp, construtor de `AetherPhysicsWorld`) são dimensionados
  como `max(1024, maxBodies)`, ou seja, um valor por CORPO. Mas o Jolt limita PARES/CONSTRAINTS
  de contato simultâneos, não corpos — um cenário denso (muitos corpos empilhados tocando vários
  vizinhos ao mesmo tempo) gera mais pares de contato que corpos, e o teto insuficiente faz o
  Jolt descartar contatos silenciosamente em produção (`JPH::EPhysicsUpdateError::
  BodyPairCacheFull`/`ContactConstraintsFull`) ou abortar com o assert de debug instalado em
  4.1.1 (`AssertFailedImpl`). Não chamamos isso de "corrigido na fronteira" — não é um bug de
  4.1.6 em si, é uma limitação de dimensionamento que já existia desde 4.1.1 e nunca tinha sido
  exercitada por nenhum teste anterior (o maior cenário de física antes deste item envolvia
  poucos corpos por vez). O benchmark contorna isso passando um `maxBodies` maior que o número
  de corpos reais (4x de folga) ao criar o mundo — não alterei o dimensionamento interno do
  `AetherPhysicsWorld`, porque a correção de verdade (separar `maxBodies` de
  `maxBodyPairs`/`maxContactConstraints` como parâmetros independentes na API pública) é uma
  mudança de ABI que merece ser feita deliberadamente, não como efeito colateral de um
  benchmark — fica registrado aqui como candidato a próxima limitação a resolver, não como algo
  já corrigido.
- **Determinismo cross-platform (4.1.8): decisão de escopo tomada ANTES de escrever código, não
  uma correção de bug.** O plano pedia "determinismo em ponto fixo (modo opcional)" — investigação
  prévia (leitura do solver do Jolt) confirmou que não existe modo fixed-point no Jolt: ele usa
  `float`/`Vec4`/SIMD em toda a matemática interna, e trocar isso por ponto fixo seria reescrever
  o solver de terceiros vendorizado por inteiro, não configurar uma flag. O item 4.1.7
  (decomposição convexa) foi avaliado junto e descartado desta rodada pelo mesmo motivo de
  disciplina: o Jolt só tem `ConvexHullShape` de hull ÚNICO (quickhull), decomposição convexa real
  precisa de uma lib externa (V-HACD/CoACD) nunca vendorizada, e não há importador de malha no
  repositório para alimentar nenhum dos dois — implementar "decomposição" que decompõe em UM hull
  só seria a gambiarra que este projeto se recusa a fazer. Optou-se por implementar só 4.1.8, via
  o que o Jolt OFERECE nativamente para o mesmo objetivo prático (`CROSS_PLATFORM_DETERMINISTIC`,
  que desliga FMA e trava SIMD em SSE4 — ver `Build/CMakeLists.txt` do próprio Jolt), como opção de
  build CMake (`AETHER_PHYSICS_DETERMINISTIC`), no mesmo padrão já estabelecido por
  `DOUBLE_PRECISION`/`USE_ASSERTS`. Nenhum bug de compilação ou runtime nesta fatia — a flag
  compilou e todos os 43 testes de física pré-existentes passaram de primeira na build
  determinística. Confirmado experimentalmente (fora da suíte de teste, via binário de comparação
  descartável) que a flag realmente altera o código de máquina gerado: a mesma simulação produz
  posições finais numericamente diferentes entre build normal e determinística — descartando a
  hipótese de que `CROSS_PLATFORM_DETERMINISTIC` fosse um no-op silencioso nesta configuração de
  compilador/plataforma. Nenhuma mudança foi necessária no lado C# (`managed/Aether.Core/Physics/`):
  é uma opção de tempo de build da `.dll` nativa, e o P/Invoke `[LibraryImport("aether_physics")]`
  já existente funciona identicamente com qualquer uma das duas variantes compiladas.
