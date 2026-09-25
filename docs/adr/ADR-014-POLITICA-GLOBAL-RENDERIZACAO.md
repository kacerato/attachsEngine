# ADR-014 — Política global de renderização orientada por budgets

- **Estado:** aceita; política consumida pelo renderer Android, persistência/calibração/térmica parciais
- **Data:** 29/08/2026
- **Itens do plano:** 0.5, 2.1.7–2.1.9, 2.5.6 e 7.6
- **Decisores:** arquitetura, renderer, Android, editor e performance

## Contexto

A cena externa com estrada/floresta apresenta 55–60 FPS reportados em um aparelho
forte quando o limite está em 120 Hz e 20–25 FPS num Samsung Galaxy A32 ou aparelho
equivalente. Otimizar somente esse mapa criaria regras impossíveis de sustentar quando
a engine possuir vários projetos, render pipelines e milhares de componentes.

O código já detecta capabilities Vulkan e classifica `DeviceProfile` S/A/B/C, mas
capability não mede throughput, bandwidth, custo do driver, resolução nem estado
térmico. O `PowerGovernor` também produz concessões próprias. Sem uma composição
única, renderer, editor e governor podem aplicar decisões contraditórias ou gravar no
projeto uma degradação que deveria ser apenas transitória.

O Android Emulator usa hardware do host ou renderer de software. Ele é apropriado
para correção e fallback, mas não representa o custo, driver e térmica de uma GPU
Mali/Adreno física.

## Decisão

O frame inteiro consumirá uma única `ResolvedRenderingPolicy`, imutável durante uma
época de configuração e resolvida a partir de quatro entradas separadas:

1. **DeviceCapabilities:** fatos reportados por Vulkan/Android, sem inferência de
   velocidade.
2. **DevicePerformanceCalibration:** benchmark curto e banco de combinações
   GPU+driver conhecidas, ambos versionados.
3. **ProjectRenderingSettings:** escolha global serializada Auto/S/A/B/C/Custom e
   preferências do autor; cenas não possuem perfil de desempenho.
4. **ThermalPowerState:** pressão transitória com piora imediata, recuperação com
   histerese e motivo registrado.

Presets serão recursos de dados versionados. Eles definem budgets de cadência,
visibilidade, LOD por erro projetado, sombras, GI/AO, pós, streaming, memória, uploads
e trabalho assíncrono. Materiais, luzes e câmeras mantêm propriedades semânticas e
artísticas; essas propriedades não podem selecionar caminhos específicos de uma cena.

Um AVD C sintético e um override de teste validam a resolução da política e os
fallbacks sem falsificar capabilities. Gates de FPS, GPU, potência e térmica exigem
hardware físico, cena/câmera determinísticas e manifesto de captura.

A primeira fatia implementada separa a capability física de refresh da preferência
global: no Android, `Auto` consulta os modos do display e seleciona até 120 Hz
(120/90/60), enquanto `aether.target_fps` permanece override de diagnóstico. O fixed
tick continua independente em 60 Hz. Não existe configuração de cadência por cena.
O caminho normal foi validado em hardware Android 16: capability e Surface em 120 Hz,
com 117,30 presents/s numa janela recente do SurfaceFlinger.

## Implementação do núcleo de qualidade — 31/08/2026

`native/renderer/rendering_policy.h/.cpp` entrega a `ResolvedRenderingPolicy` desta
ADR além da fatia de cadência. Três invariantes ficaram codificadas, não apenas
documentadas:

**O renderer nunca lê o nome do preset.** `QualityPreset` não aparece em nenhum campo
de `ResolvedRenderingPolicy`; o renderer lê eixos derivados. Um `if (preset == Alta)`
dentro do renderer é o mesmo defeito que `if (cena == dirt-road)`, que a seção 8 do
plano de otimização proíbe — presets são pontos no espaço de configuração, não
caminhos de código.

**Todo eixo é independente.** `ProjectRenderingSettings` carrega o preset e um
override por eixo, com `Inherit` como sentinela dentro do próprio enum (e não um
`optional` paralelo, que dessincronizaria no round-trip de serialização). Escolher
"preset C com sombras ultra" é uma configuração legítima e testada.

**Toda degradação é auditável.** Quando o valor resolvido difere do pedido, a política
registra eixo e motivo — `capability`, `budget` ou `thermal`. Sem isso, um aparelho que
silenciosamente desliga sombras vira "bug de iluminação" para quem lê o relatório.

Eixos entregues no schema v4: sombras (contagem/resolução de cascata, taps de PCF próximo/distante, alcance, bias
constante/slope/normal-offset, ancoragem em texel, cache estático e guard band), ambiente (constante, hemisfério,
hemisfério + sonda especular), pós (passe, bloom, FXAA, sharpen, contraste, saturação
e vinheta), texturas (bias de residência e anisotropia), LOD por erro projetado com
budgets independentes para malha sólida e alpha-coverage,
distância de detalhe material por mapa e resolução estática/dinâmica com histerese.

O schema v6 adiciona dois eixos que não podem ser inferidos do nome do preset:
`materialShaderVariants` e `environmentSplitSumBrdf`. O primeiro permite pipelines
especializados para normal/MR/emissivo, mas o default depende do perfil porque o A/B
no Adreno mostrou regressão em B. O segundo seleciona a integração BRDF da sonda e
continua independente de sua disponibilidade: pedir split-sum sem radiância especular
é resolvido para desligado com motivo auditável. Os recursos AEEN v3 descrevem o que
existe; a política decide o que usar.

O consumo pelo `InstancedRenderer` Android também foi implementado em 31/08/2026.
CSM direcional usa 1–4 cascatas em atlas, PCF configurável e alpha-test de vegetação;
o passe final usa um alvo de cena na escala resolvida e aplica os filtros independentes.
Normal map, probe especular e seleção de LOD leem seus budgets globais. A seleção usa
a altura lógica da resolução interna ativa; resolução dinâmica e LOD não mantêm
estimativas contraditórias do tamanho visível. A cena de
floresta é somente um consumidor: não há branch por nome de cena, asset ou preset.
Filtros de reconstrução não são ativados como efeito colateral da escala; o projeto
decide explicitamente FXAA/sharpen/TAA. O controller dinâmico observa somente tempo de
GPU para não reagir ao bloqueio de acquire ou ao frame pacing.

Desde o schema 5 de profiling, a decisão fica auditável por janela: p95 da render
thread e da GPU são comparados aos budgets resolvidos, acquire/present só classificam
`presentation` quando o intervalo também está atrasado, e a escala dinâmica registra
mínimo/máximo/final. A política não busca "uso equilibrado" de CPU/GPU; busca ambas as
trilhas abaixo do deadline com margem. O ADPF da render thread segue o mesmo contrato
e recebe CPU time, não wall time bloqueado pela GPU. O target do hint permanece o
intervalo completo do frame (8,33 ms a 120 Hz): reduzir artificialmente esse target
para 88% piorou o A/B físico e não é um mecanismo legítimo para solicitar clocks.
O Game Mode e a preferência de eficiência do Android são informados por APIs públicas;
a engine não tenta controlar frequências privadas de CPU/GPU.

O detalhe material distante tem duas camadas complementares. O shader ainda faz fade
contínuo das contribuições configuráveis, evitando uma transição visível. Para retirar
de fato o custo de amostrar o normal map, o renderer também escolhe uma variante
compilada sem normal somente quando a esfera de bounds inteira do draw está além da
distância máxima. Bounds inválido ou atravessando a faixa mantém qualidade completa.
Essa decisão é global, serializável na política e não conhece a cena de stress.

A cadência de apresentação é uma extensão opcional do RHI. No Android, o backend pode
usar AGDK Frame Pacing/Swappy por swapchain; em falha de capability ou inicialização,
retorna ao caminho FIFO + AChoreographer. Nenhum tipo genérico do renderer conhece a
biblioteca Android, e nenhuma cena decide o mecanismo de pacing.

A pressão térmica piora um degrau por nível e **não** grava no projeto: a mesma entrada
sem pressão devolve exatamente a qualidade original, o que um teste tranca. O monitor
NDK entregue amostra no máximo a cada 10 s, piora imediatamente e recupera somente um
nível após três amostras frias. A política ativa só reduz recursos já criados para a
época; portanto não recria atlas, pipelines ou assets no frame.

### API C# durante Play — 21/09/2026

`Astra.Graphics.State` consulta três estados distintos: a configuração pedida, a
política efetivamente publicada pelo renderer e as capabilities detectadas. Um script
pode copiar `State.Requested`, alterar qualquer eixo de `GraphicsSettings` e chamar
`Graphics.ApplyRuntime(settings)`. O retorno é o identificador do pedido; enquanto
`State.Pending` for verdadeiro, `State.Effective` continua descrevendo o renderer
anterior. A confirmação ocorre somente no ponto seguro de rebuild, depois de materiais,
texturas e recursos autorais serem republicados. Falha mantém o pedido para diagnóstico
e não inventa uma política efetiva; `State.EffectiveAvailable` fica falso quando o
rebuild destrutivo não deixou um renderer utilizável.

Essa alteração existe apenas na sessão Play. Ela não grava `rendering.astra`; Stop
enfileira a restauração autoral, inclusive quando substitui um pedido de gameplay que o
shell ainda não consumiu. Handles de uma execução anterior são recusados pelo id do
mundo. Pressão térmica e capabilities são atualizadas a partir do estado real do shell.
Assemblies de projeto publicados registram a identidade do compilador `Astra.Scripting`;
uma geração anterior à ABI atual é recusada e precisa ser recompilada. A ABI 7
acrescenta propriedades de material por slot; a estrutura da política gráfica
continua compartilhada entre o editor, o runtime e o renderer.

### Reconstrução espacial e curva de apresentação — 21/09/2026

`UpscalingFilter::Fsr1` / `GraphicsUpscaling.Fsr1` seleciona AMD FidelityFX
Super Resolution 1.0.2, commit `a21ffb8f6c13233ba336352bdff293894c706575`, MIT.
O backend Vulkan executa pós/AA na resolução interna, EASU na resolução de saída
e RCAS antes da interface. `postSharpen` controla a intensidade do RCAS; zero
preserva a reconstrução sem acrescentar nitidez. O filtro usa RGB perceptual,
com conversões explícitas quando as imagens Vulkan fazem transferência sRGB.
O recorte da câmera limita as amostras para não misturar painéis com a cena.
O grão fica depois da reconstrução; o histórico TAA permanece sem grão/nitidez.
Isto não fornece vetores de movimento, FSR 2, geração de frames ou Arm ASR.
Alocação/pipeline recusados falham a publicação do pedido, sem anunciar FSR ativo.
Referência: [AMD FSR 1](https://gpuopen.com/fidelityfx-superresolution/).

O Ambiente oferece Reinhard (id 0, antigo rótulo Neutral), ACES (id 1) e AgX
(id 2). Os ids antigos preservam a aparência. AgX reutiliza as funções MIT do
[shader da Godot 4.4-stable](https://github.com/godotengine/godot/blob/4.4-stable/servers/rendering/renderer_rd/shaders/effects/tonemap.glsl),
com fonte e licença identificadas em `native/third_party/godot_agx`.
O contrato usa entrada/saída linear sRGB e uma transferência de apresentação
posterior. Ambiente v12 e perfil v8 persistem AgX, neblina por altura, energia HDR da neblina e exposição automática; versões antigas continuam
legíveis. Não há saída HDR de monitor implícita nessa escolha.

A neblina usa `fog_base_height` (metros) e `fog_height_falloff` (1/m) no mesmo
componente, Inspector, perfil e API genérica. A densidade na altura base é
`fog_density`; acima dela decai exponencialmente. O shader integra a densidade
ao longo do raio reconstruído da câmera, respeitando perspectiva, ortográfica,
roll e início da neblina. Decaimento zero conserva a fórmula uniforme anterior.
O grupo Neblina dos volumes mistura os dois parâmetros. Trata-se de neblina
analítica por profundidade/altura; não há volume de dispersão iluminado.
Referências: [Unreal Exponential Height Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine),
[Godot Environment](https://docs.godotengine.org/en/stable/classes/class_environment.html)
e [Unity HDRP 17 Fog](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/fog-volume-override-reference.html).

### Exposição por câmera — 23/09/2026

`auto_exposure` usa histograma GPU de 64 faixas, com cortes percentuais baixo/alto,
cinza alvo, limites EV, peso central opcional e velocidades separadas de adaptação
em EV/s. `exposure_ev` soma a compensação autoral ao resultado. São multiplicadores
em stops sobre a luminância linear da Astra, não o contrato físico EV100 de uma câmera.
O histograma lê o HDR da cena antes da neblina e do bloom; a grade de medição é
limitada a 256 × 144 amostras. Não há leitura de volta à CPU por quadro.

Vista principal e prévia mantêm estados GPU distintos. Ativação, troca/corte de
câmera, resize, nova cena e retomada invalidam a adaptação anterior. O Ambiente,
Inspector, perfis e `Component.SetBool/SetFloat` compartilham os mesmos descritores
e validação, inclusive `min <= max` e percentil baixo estritamente menor que alto.
Volumes misturam os valores no grupo Pós; opções booleanas usam seleção discreta.
Documentos antigos abrem com exposição automática desligada.

Referências de comportamento: [Unity HDRP 17 Exposure](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/reference-override-exposure.html),
[Unreal Auto Exposure](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine)
e [Godot 4.4 CameraAttributes](https://docs.godotengine.org/en/4.4/classes/class_cameraattributes.html).
A implementação do histograma/adaptação é própria. HDR intermediário e compute
são requisitos; indisponibilidade deve ser diagnosticada, sem anunciar efeito ativo.

### Streaming de mipmaps — 24/09/2026 (S2)

Referência: [Unity 6.0 Mipmap Streaming](https://docs.unity3d.com/6000.0/Documentation/Manual/TextureStreaming.html)
e os campos `QualitySettings.streamingMipmaps*`. O eixo `textureStreaming` (Inherit =
desligado em todos os níveis, como na Unity) liga a residência por textura das
texturas de autoria. `textureStreamingBudgetMegabytes` (Memory Budget; padrão 256/384/
512/768 MB para Baixo/Médio/Alto/Ultra, 32–8192), `textureStreamingMaxLevelReduction`
(Max Level Reduction; padrão 2, 1–7) e `textureStreamingUploadKilobytesPerFrame`
(teto de envio por quadro; padrão 4 MB, 256 KB–64 MB) persistem em `rendering.astra`
com chaves novas no arquivo v3 — uma engine anterior as ignora e continua sem streaming.

O plano (`renderer/texture_streaming.h`) é puro e testado no host: o mip pedido por
um uso é `floor(log2(texels por pixel))`, com a densidade vinda da métrica de UV da
malha (raiz de área/área de UV, como `Mesh.GetUVDistributionMetric`) e da escala do
objeto; o conjunto cabe no orçamento tirando níveis primeiro de quem tem menor
prioridade; a redução é contada a partir do limite global de mip da qualidade; as
trocas descem antes de subir e respeitam o teto de bytes, com pelo menos uma por
quadro. No Android o consumidor roda logo após a fence do quadro anterior (há um
único quadro em voo), cria a imagem nova, sobe a cauda da cadeia e regrava o mesmo
slot bindless; a imagem anterior só sai depois de a nova existir.

Diferenças explícitas: a fonte dos mips é a cadeia preparada em memória, não o disco
— o streaming economiza residência na GPU, não RAM; todos os usos visíveis contam,
sem recorte de frustum (girar a câmera não provoca troca); o lado maior da superfície
é a altura da vista (erra para mais detalhe, nunca para menos). Aplicar pelo painel
ou por `Graphics.ApplyRuntime` segue o mesmo ponto seguro de reconstrução dos outros
eixos; a política ativa (térmica) também troca os campos de streaming sem recriar
recursos. Com o streaming ligado, a textura nova entra na publicação pelo fim da
faixa e sobe conforme a tela pede. `Graphics.State` expõe
`TextureStreaming` com `CurrentBytes`, `DesiredBytes`, `TargetBytes`, `TotalBytes`,
`NonStreamingBytes`, orçamento, pendências e falhas; `GraphicsSettings` e
`ResolvedGraphicsSettings` ganham os quatro campos no fim. O tamanho das estruturas
é travado dos dois lados (static_assert e teste gerenciado).

Por textura, como no Texture Importer da Unity: "Stream Mipmap Levels" e "Priority"
(-128..127) vivem no perfil da textura do projeto (schema 4) e, para as texturas de
uma fonte importada, no perfil de importação dela (schema 10). Nenhum dos dois muda
os bytes preparados: ficam fora da chave do cache e das comparações de preparo e
valem na publicação, que entrega ao renderer um parâmetro por textura na mesma ordem.
O Inspector de textura mostra o mip carregado e o que a tela pede, relatados pelo
renderer; no host, sem GPU, ele diz que não há renderer. A aba Texturas tem a vista de
depuração da Scene View (só no editor, fora do Play): a cor base do desenho é tingida
pelo estado da textura de cor dele — verde no nível pedido, vermelho abaixo (orçamento
ou pendente), azul acima, cinza fora do streaming.

### LOD na própria malha e estatísticas do quadro — 25/09/2026 (S3/S5)

Referências: [Unity 6 Mesh LOD](https://docs.unity3d.com/6000.3/Documentation/Manual/lod/mesh-lod-introduction.html)
e o [gerador](https://docs.unity3d.com/6000.3/Documentation/Manual/lod/mesh-lod-generator.html),
[Godot Mesh LOD](https://docs.godotengine.org/en/stable/tutorials/3d/mesh_lod.html) e o
[meshoptimizer v1.2](https://github.com/zeux/meshoptimizer) (MIT, já vendorizado; entraram
`vcacheoptimizer.cpp` e `indexanalyzer.cpp` do mesmo commit). O perfil de importação
(schema 11) tem "Níveis de detalhe (LOD)", "Máximo de níveis" (2–4, contando o 0) e
"Otimizar ordem dos polígonos"; fonte nova já vem com os dois ligados, fontes publicadas
continuam como estavam. Regras do gerador da Unity: começa com 256 triângulos, cada nível
com cerca de metade dos índices, para abaixo de 64 triângulos ou sem redução real, sem
vértice novo; cada nível parte do nível 0, com as bordas presas (desenhos vizinhos não
abrem fresta). Desenho com skin ou blend shapes não recebe LOD. A ordem de índices usa o
otimizador de cache de vértices e o ACMR (cache de 16) antes/depois aparece no resumo da
importação — o ganho declarado é essa medida, não FPS.

Os níveis são faixas extras do buffer de índices sobre os mesmos vértices, guardadas no
derivado de importação (seção opcional: derivados sem LOD continuam válidos e a chave só
muda com LOD ligado). O renderer escolhe o nível de cada desenho autoral uma vez por quadro
com a mesma regra do pacote de mapa (`selectLodLevel`: erro geométrico × escala do objeto
projetado em pixels da tela final, orçamento "Erro de LOD" e histerese do painel
Qualidade, "Seleção de LOD" desligada força o nível 0). A escolha vale para a cena, o
caminho indireto e as sombras. Diferenças: sem cross-fade entre níveis da malha (troca
seca com histerese); sombras usam o nível da câmera principal; sem viés por objeto ainda.

S5: "Estatísticas no viewport" (aba Desempenho) mostra intervalo do quadro, GPU (medida
só com o painel ou as estatísticas abertos), resolução, desenhos e triângulos realmente
enviados, LOD (reduzidos/total e triângulos do nível 0 → escolhidos) e texturas na GPU.
`Graphics.State.Frame` entrega os mesmos números aos scripts (`FrameStatistics`, 56 bytes,
travado dos dois lados).

A separação segue a diferença da Unity 6 entre a intenção alterada via `QualitySettings`
e o pipeline atual consultável. Na Astra, o acknowledge assíncrono é explícito porque
alguns eixos exigem reconstrução Vulkan; portanto um setter aceito não é apresentado
como efeito já aplicado.

### Validação em hardware

Em 01/09/2026, o AEMAP v3 de produção passou a fornecer 245 grupos espaciais com
LOD0/colisão preservados. No A/B aquecido da rota, coverage 32 mediu 109,49 FPS/
7,45 ms GPU contra LOD0 em 107,78/7,61 ms; coverage 48 mediu 111,01/7,37 ms em
ordem térmica diferente. O preset B resolve coverage em 48 px, mas o número continua
um eixo editável até 128 px, não um branch do preset. Uma pose fixa sustentou ~120 FPS
nos dois modos e passou inspeção visual (MAE 2,00/255); o gate multipose/soak/Mali
permanece aberto.

No Xiaomi de referência, com Release assinado:

```
[RenderPolicy] features: vulkan1_3=1 descriptor_indexing=1 nonuniform=1
               ray_query=0 mesh_shader=0 vrs=0 memoryless=0 -> perfil=1
[RenderPolicy] preset=auto perfil=1 sombras=on(2 cascatas @1024, 9 taps)
               ambiente=hemisferio pos=passe textura_mip_bias=0 aniso=1.0
               escala=1.00 clamps=1
[RenderPolicy] textures.samplerAnisotropy reduzido por capability.
```

O clamp é verdadeiro: a engine nunca habilitou `samplerAnisotropy` na criação do
device, então pedir anisotropia > 1 seria uso inválido do sampler. A política expõe
essa lacuna em vez de escondê-la.

### Lacunas que esta fatia tornou visíveis

1. **`DeviceFeatures` só preenche quatro campos.** `rayQuery`, `meshShader`,
   `variableRateShading` e `memorylessAttachments` nunca são consultados em
   `initializeDevice`, ficam sempre `false`, e por isso **os perfis A e S são
   inalcançáveis por detecção automática**. Antes desta fatia nada consumia isso de
   forma visível; agora define a qualidade padrão. `memorylessAttachments` reporta
   `false` mesmo no device que comprovadamente concede `LAZILY_ALLOCATED`.
2. **`samplerAnisotropy` não é habilitada** na criação do device.
3. **Persistência e calibração continuam incompletas.** O renderer consome os eixos,
   overrides de lançamento os exercitam e a pressão térmica já troca a policy ativa em
   fronteira segura de frame; Project Settings ainda não serializa o schema 5 e a
   calibração curta/banco de aparelhos ainda não existem.
4. **Cache de sombra está entregue, mas o modelo de caster ainda é parcial.** Culling
   conservador e cache por cascata com guard band/invalidação explícita reduziram o
   passe de 3,05 para 0,09 ms em rota Release; os chunks do asset ainda são grossos e
   o renderer precisa separar casters estáticos de dinâmicos antes de generalizar a
   atualização incremental para cenas animadas.

## Alternativas descartadas

1. **Configuração por cena.** Rejeitada porque duplica tuning, impede previsibilidade
   entre projetos e transforma samples em exceções do renderer.
2. **Tabela por modelo de telefone como única decisão.** Rejeitada porque um mesmo
   nome comercial pode ter variantes e drivers diferentes, e a tabela envelhece.
3. **Capabilities como proxy de velocidade.** Rejeitada porque suporte a extensão
   não informa o custo da implementação, bandwidth nem resolução de saída.
4. **PowerGovernor alterando diretamente sistemas gráficos.** Rejeitada porque cria
   múltiplas fontes de verdade e pode produzir oscilações/estados contraditórios.
5. **AVD como certificação de um A32.** Rejeitada porque o host executa a carga e não
   reproduz GPU, driver, DVFS, potência ou dissipação do aparelho.
6. **Redução fixa de resolução/conteúdo como primeira medida.** Rejeitada porque
   oculta trabalho invisível e reduz qualidade sem corrigir a arquitetura.

## Consequências

- Todo novo sistema gráfico declara seus budgets e lê a política resolvida; não
  consulta nome de cena, asset, fabricante ou modelo.
- Mudanças de política são atômicas, serializáveis para diagnóstico e acompanhadas de
  `reason`/telemetria. Estado térmico não modifica o arquivo do projeto.
- O editor expõe Project Settings globais e um comparador S/A/B/C. O Inspector de uma
  cena não oferece presets de desempenho locais.
- O importador gera LODs/mips/variantes reutilizáveis; o runtime escolhe por budget e
  erro de tela, sem remover conteúdo autoral.
- A classificação atual de `native/rhi/device_profile.*` permanece como base de
  capabilities e deverá ser composta com calibração, não substituída por hardcode.
- Métricas e presets precisam de versão/migração para preservar projetos antigos.

## Gate de aceitação

1. Testes unitários cobrem resolução determinística, override, capabilities ausentes,
   mudança térmica e migração de presets.
2. AVD C sintético executa a floresta nos caminhos de fallback, lifecycle e pressão de
   memória, marcando o resultado como sintético.
3. Aparelhos físicos Adreno e Mali C executam o mesmo APK, pacote/hash, câmera e
   resolução controlada; relatórios incluem p50/p95/p99, GPU por pass, compositor,
   memória, potência e térmica.
4. Perfil A sustenta 60 FPS; perfil C sustenta 30 FPS e busca 45 como meta evolutiva,
   sem remoção de conteúdo e dentro do gate visual SSIM/FLIP.
5. Nenhum consumidor gráfico mantém uma segunda tabela privada de qualidade.

## Referências

- [`PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md`](../PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md)
- [`PLANO-ENGINE-MOBILE.md`](../PLANO-ENGINE-MOBILE.md)
- [Configuração de AVD](https://developer.android.com/studio/run/managing-avds)
- [Aceleração do Android Emulator](https://developer.android.com/studio/run/emulator-acceleration)
- [Android GPU Inspector](https://developer.android.com/agi)
