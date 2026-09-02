# Compute na Aether Engine

## Estado da fundação

A fundação compute está implementada no RHI Vulkan e validada em um aparelho
Android físico. Isso significa que a engine já tem um caminho reutilizável para
criar kernels, refletir SPIR-V, vincular recursos, despachar trabalho e
sincronizar filas. Não significa que todos os consumidores futuros — Forward+,
HZB, skinning, partículas e importação de assets — já estejam implementados.

O contrato é renderer-agnostic acima do backend Vulkan:

```text
shader compute
  -> glslc + spirv-val
  -> reflexão SPIR-V
  -> VulkanComputeKernel
  -> VulkanComputeContext
  -> Render Graph (acessos, barreiras e fila)
  -> Vulkan
```

## Responsabilidades

- `native/rhi/compute.h/.cpp`: kernel, descriptors, pipeline, dispatch direto e
  indireto, contexto de comandos, fence, semáforos e barreiras.
- `native/rhi/compute_validation.cpp`: validação pura e reflexão SPIR-V mínima.
- `native/rhi/device.h/.cpp`: descoberta da família de compute, preferência por
  fila dedicada e fallback para a fila gráfica.
- `native/rendergraph/render_graph.h/.cpp`: descrição do passe compute, seus
  acessos e escolha da fila.
- `native/rhi/render_graph_vulkan*.cpp`: conversão das barreiras compiladas em
  barreiras Vulkan reais, inclusive transferência de ownership entre famílias.
- `native/rhi/pipeline_cache.h/.cpp`: `VkPipelineCache` compartilhado pelo
  device e usado também pelos kernels compute.

## Contrato de kernel

`ComputeKernelDesc` declara o contrato esperado antes de criar objetos Vulkan:

- SPIR-V do estágio compute;
- bindings e tipos de descriptor;
- tamanho de push constants;
- tamanho local X/Y/Z;
- nome de debug.

A reflexão é comparada ao contrato. A criação falha com erro contextual quando
há estágio incorreto, set diferente de zero, binding ausente/duplicado, tipo
incompatível, local size divergente ou presença inesperada de push constants.
Todos os bindings obrigatórios também precisam ser escritos antes do dispatch.

A reflexão atual cobre descriptors no set 0, arrays desempacotados até o tipo
base, local size literal e presença de push constants. Múltiplos descriptor
sets, arrays de tamanho variável e inferência automática do tamanho exato do
bloco de push constants ficam para a evolução do sistema de shaders; até lá o
tamanho é explicitamente declarado e validado contra os limites do device.

## Dispatch e sincronização

Há dispatch direto e indireto. O indireto só aceita buffer criado com
`VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`. O contexto não usa `vkQueueWaitIdle` por
dispatch: ele grava, submete com fence e pode aguardar/sinalizar semáforos para
compor trabalho entre filas.

Quando existe uma família compute-only, o device a prefere. Caso contrário o
mesmo contrato roda na fila gráfica. Recursos exclusivos transferidos entre
compute e graphics usam release/acquire e semáforo; isso foi exercitado pelo
encoder ASTC, no qual o compute escreve o buffer e a fila gráfica faz a cópia
para a imagem.

## Integração com o Render Graph

O grafo distingue passes `Raster`, `Compute` e `Transfer`, imagens e buffers, e
os acessos:

- sampled/uniform read;
- storage read/write;
- indirect read;
- transfer read/write.

O compilador deriva dependências, estágios, acessos, layouts e fila de execução.
`preferAsyncCompute` é somente uma preferência: vira fila compute apenas quando
o device realmente oferece uma fila dedicada; o fallback permanece correto.

## Evidência física

O primeiro consumidor migrado é o probe ASTC 4x4. Em um Xiaomi SM8735/Adreno,
uma textura 4096 x 4096 foi codificada com sucesso, com diferença máxima de 7
por canal. A primeira validação marcou `gpu_encode_ms=2,233594`; a repetição
final da build entregue marcou `6,525208 ms`, ainda muito abaixo do gate de
300 ms. O fluxo dedicado compute -> graphics foi validado sem VUID de
compute/ownership.

Existe ainda um VUID de apresentação no caminho do swapchain
(`pSignalSemaphores-00067`) que já existia fora desta fundação. Ele deve ser
tratado no gate de frame pacing/presentação e não é evidência de falha do
compute.

## C1 — pirâmide em compute, residente na GPU

Entregue e ativa em hardware: `[HZB] produtor=compute níveis=6 base=160x346
readback=nao`. A cadeia de redução roda como seis dispatches, cada nível é uma
imagem `R32_SFLOAT` em `GENERAL`, e a pirâmide nunca volta para a CPU — o
readback existe só como caminho de validação (`aether.hzb_compute_validation`),
que compara toda redução 2x2 contra `validateHzbMaxReductionChain`.

**C1 sozinho não remove um triângulo.** A medição em Adreno da própria fatia
mostra o custo e o silêncio lado a lado: `gpu_hzb_ms` mediano de **1,07 ms** e
`hzb_tested=0 hzb_occluded=0` na mesma execução. A decisão de oclusão continuava
na CPU e a CPU tinha acabado de perder o único jeito de ler a pirâmide. Além
disso o depth deixa de ser memoryless enquanto o HZB o amostra
(`store=sim sampled=sim memoryless=nao`), o que devolve os 0,44 ms que P1 havia
economizado. É por isso que C1 permanece opt-in: sem C2 ele é custo puro.

## C2 — oclusão GPU-driven (implementada, não medida)

O consumidor que fecha o laço. Um kernel (`native/rhi/shaders/draw_cull.comp`)
lê a pirâmide residente e escreve o `instanceCount` dos comandos
`VkDrawIndexedIndirectCommand` que o passe opaco **já** submete desde o caminho
multi-draw. Nenhum estágio gráfico sabe que ele existe: um objeto ocluído vira
um comando com zero instâncias.

- **A matemática não é nova.** `renderer::cullDrawRecordReference` é o espelho
  instrução por instrução do shader, e um teste tranca que ele reproduz
  `projectBoundsToHzbScreenRect` + `isOccludedByHzb` + `updateHzbHysteresis`
  candidato a candidato quando a guarda de movimento é zero. Mover a decisão
  para a GPU não pode ser a oportunidade de mudar o que é considerado visível.
- **A guarda de movimento substitui um penhasco.** O caminho de CPU desligava o
  estágio inteiro assim que a câmera se mexia (`hzb_motion_skip`) — num jogo em
  primeira pessoa, nunca ocluir nada. `buildGpuCullMotionGuard` converte a
  diferença entre a pose que produziu a pirâmide e a pose atual em três folgas
  (rotação, aproximação e translação lateral) que só podem tornar o teste mais
  permissivo. Não é prova de conservadorismo — nenhuma existe para HZB temporal
  com câmera livre — e por isso a monotonicidade é testada explicitamente:
  aumentar o movimento nunca aumenta o conjunto cortado.
- **O dispatch cobre a capacidade, não o frame.** Ele é gravado antes do render
  pass, e a lista de comandos só é construída dentro dele; um dispatch já
  gravado não relê push constants. Slots de sobra ficam com `flags=0` e o kernel
  não escreve neles — nem no comando, nem no estado, nem na telemetria.
- **A telemetria é lida um frame depois**, no mesmo ponto de
  `collectPrevious()`, e nunca realimenta uma decisão. `submitted_draws` e
  `submitted_triangles` continuam medindo o que a **CPU** submeteu e, por
  construção, ainda incluem o que a GPU zerou.
- **Região de GPU própria:** `GpuPassClass::Culling`, entre Shadow e Opaque, com
  marcador de captura e timestamp. Sem ela o dispatch cairia dentro do balde do
  passe opaco e o custo do mecanismo de visibilidade ficaria invisível de novo.

Estado: compila em Android Debug/Release, SPIR-V validado e reproduzível,
contrato de bindings refletido em teste de host, 310/310 nativos, 48/48
FrameProfile e 46/46 Python. **Nada foi medido em hardware** — a fatia foi
escrita sem ADB na sessão. Continua opt-in por `aether.hzb_gpu_culling` (que
implica `aether.hzb_compute`) até existir A/B físico com gate de imagem.

## Ordem dos próximos consumidores

1. **C0 — fundação + ASTC técnico:** entregue.
2. **C1 — pirâmide em compute:** entregue e medida (1,07 ms, sem consumidor).
3. **C2 — culling GPU-driven:** implementado; falta A/B físico, gate de imagem
   na rota e decisão sobre compactação de comandos.
4. **C3 — Forward+:** froxels, light lists e limites por perfil; depende do
   grafo e de buffers storage confiáveis, não de C2.
5. **C4 — compute skinning:** pose/skin buffers e fallback vertex shader.
6. **C5 — partículas GPU:** pools, scan/compactação, simulação e sort limitado
   por perfil.
7. **C6 — ferramentas e assets:** ASTC/ETC2 de produção, KTX2/Basis, baking e
   importação assíncrona; o probe atual não substitui o compressor de produção.

Cada consumidor precisa ter fallback, capability gate, budget, marcadores e
timestamps próprios. Só depois de C1/C2 devem ser retomadas as otimizações de
FPS da cena: a meta é remover trabalho invisível, não reduzir resolução.

## Artefato desta entrega

- APK Release assinado para desenvolvimento:
  `build/aether-compute-foundation-r10-release.apk`.
- SHA-256:
  `E031F002FA2B75685A6CFB38A33BDEC1B942EFC3044CD1CA0DD85404F823C804`.
- Assinatura APK Signature Scheme v2/v3 verificada. A chave é a chave local de
  desenvolvimento Android; uma publicação exige a chave de release do produto.
