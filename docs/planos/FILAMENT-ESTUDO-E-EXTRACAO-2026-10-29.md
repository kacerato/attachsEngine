# Filament como referência — estudo e plano de extração para o renderer da Astra

Data: 29/10/2026. Estado: **plano**. Nada deste documento foi implementado nem
medido no aparelho. Os ganhos citados são hipóteses ordenadas por evidência; cada
pacote só vira ganho aceito com A/B/B/A no POCO F7, comparação de pixels e
apresentação contínua, como nos aceites de `docs/validacao/`.

## 1. Decisão

**Não trocar o renderer pelo Filament (nem pelo The Forge). Estudar o Filament e
extrair técnicas para o renderer Vulkan próprio.**

- O Filament é um renderer completo, com backend, material system (`matc`), cena e
  frame graph próprios. Adotá-lo inteiro descartaria consumidores já validados:
  água FFT em compute, skinning em compute compartilhado entre passes, cache de
  sombra estática, reuso estático do editor (M7), especialização opaca sem
  recorte, LOD com cross-fade, ARM ASR/FSR2, profiler por região de GPU, grade,
  gizmos e prévia de câmera.
- `docs/REFUNDACAO-ASTRA.md:116` já registra "Não substituir Vulkan por
  Godot/Three.js/Filament". `docs/planos/FECHAMENTO-DE-CAPACIDADES-2026-10-02.md:122`
  exige problema demonstrado antes de trocar o renderer. Este plano respeita as duas
  decisões.
- O plano Astra 2 sobre The Forge (`docs/planos/astra-forge-2026-10-01/`) é outro
  produto. Este documento trata só da Astra atual (C#/Vulkan próprio) e não mistura
  os dois.

### O que o estudo mostrou

O Filament **não** resolve sozinho os gargalos medidos da Astra. Em alguns pontos a
Astra já está à frente:

| Tema | Filament v1.80.0 | Astra hoje |
|---|---|---|
| Cache de sombra | Não existe. O mapa é textura transitória recriada e limpa em todo quadro (`filament/src/ShadowMapManager.cpp:380-386`, `:628`) | Cache estático por cascata, mas invalidado por qualquer movimento |
| LOD de malha | Não existe no núcleo: "only 0 can be specified" (`filament/include/filament/RenderableManager.h:580`) | LOD gerado na importação, com cross-fade por dither |
| Skinning | No vertex shader, refeito em cada passe (`shaders/src/surface_getters.vs:78-125`) | Compute, saída reutilizada por sombra, profundidade e cor |
| Culling na GPU / HZB | Não tem; culling SIMD na CPU | Tem, mas só no caminho MDI, que cenas autorais desligam |
| Auto-instancing | Existe, **desligado por padrão** (`filament/src/details/Engine.h:687`) | Não tem |

O valor do Filament para a Astra está em técnicas pontuais e maduras para mobile:

- profundidade D16 e PCF de 4 buscas;
- atribuição de luzes por froxel;
- BRDF em mediump;
- pós-processamento barato;
- controle de resolução dinâmica filtrado;
- EASU mobile com divisão para Adreno;
- descarte de recursos por fence.

## 2. Fonte, versão e licença

- Repositório: https://github.com/google/filament. Versão **v1.80.0**, commit
  `981a42242090346970fa85318c981a90e9ba6f96` (29/10/2026). As referências de linha
  `filament/...` e `shaders/...` abaixo são relativas a esse commit. Link
  permanente: `https://github.com/google/filament/blob/981a422/<caminho>#L<linha>`.
- Licença **Apache-2.0**. Código portado mantém o cabeçalho de copyright original,
  ganha um comentário de origem (arquivo, commit, linhas) e entra no `NOTICE` e na
  tela de licenças.
- O EASU mobile do Filament (`filament/src/materials/fsr/ffx_fsr1_mobile.fs`) é
  **MIT** (atyuwen) e precisa de atribuição própria.
- Documentação conceitual: *Physically Based Rendering in Filament*,
  https://google.github.io/filament/Filament.html, já citada em
  `docs/MATERIAL-PREVIEW.md:61`.
- Pela regra do `AGENTS.md`, cada pacote cita também uma referência Unity ou Godot,
  com versão e link. O Filament é referência de técnica. Ele não autoriza sozinho
  uma capacidade nova de autoria.

## 3. Ponto de partida medido

Todos os dados abaixo são do POCO F7 / Adreno, em Release.

| Fato | Valor | Fonte |
|---|---|---|
| Limitante | GPU. CPU ativa de 2 a 4 ms; `acquire_wall` acompanha `gpu_frame` | `docs/ORCAMENTO-120HZ.md` §3 |
| Sponza em Play, escala 1,0 | GPU 19,37 ms; opaco cerca de 15,5 ms; pós 3,46 ms; cerca de 46 FPS | `docs/validacao/play-lighting-2026-10-05/README.md` |
| Sponza em Play, sessão normal (DRS 0,75, filtro 9) | GPU cerca de 14,4 ms; cerca de 62 FPS | idem |
| Isolamento do opaco na Sponza, antes da especialização | 17,17 ms → 13,57 sem pontuais (−3,6) → 14,30 sem sombra direcional (−2,9) | `docs/validacao/play-lighting-2026-10-05/isolation-summary.json` |
| Opaco só com base color (dirt-road, escala 1,0) | 8,81 ms, mais que o frame inteiro a 120 Hz | `docs/ORCAMENTO-120HZ.md` §2 |
| Gravação + submissão (dirt-road) | 0,52 ms | `docs/ORCAMENTO-120HZ.md` §3 |
| Sombras nos aceites da Sponza | `shadow_cascades=0/4` (cache) em 272 de 293 amostras, porque nada se movia | logs em `docs/validacao/sponza-player-2026-10-05/` e `play-lighting-2026-10-05/` |

**Lacuna de medição:** nenhum aceite mediu uma cena com personagem animado ou corpo
em movimento. Pelo código, nesse caso as quatro cascatas são redesenhadas em todo
quadro (§5, F1). É o cenário mais comum dos projetos dos usuários e o primeiro a
medir.

## 4. Mapa Filament ↔ Astra

| Subsistema | Filament (o que extrair) | Astra hoje | Pacote |
|---|---|---|---|
| Sombras | D16 (`ShadowMapManager.cpp:1404`); PCF LOW com 4 buscas em tenda (`shaders/src/surface_shadowing.fs:35-87`, padrão em `filament/src/details/Material.cpp:93`); near/far apertados pela faixa z dos casters (`ShadowMap.cpp:387-506`, `:1203-1229`); bias de polígono + normal (`ShadowMapManager.cpp:546-551`, `:167`) | D32 (`instanced_renderer.cpp:272-285`, `:2373`); 9 buscas no raio 2 e 18 na faixa de blend (`environment_lighting.glsl:205-251`); invalidação total (`instanced_renderer.cpp:5608`, `:5652`; `instanced_skinning.inl:270`); pass em cache com LOAD do atlas inteiro (`instanced_renderer.cpp:2598-2633`) | F1 |
| Poses e cena | Sujeira por bitset sem teto (`filament/src/details/Scene.cpp:335-366`, `:406-533`) | Teto de 128 poses (`instanced_renderer.h:1161`); estouro republica a cena (`instanced_renderer.cpp:5382`; `android_main.cpp` ~3756) | F2 |
| Dados por objeto e submissão | UBO por renderable com offset dinâmico (`filament/src/RenderPass.cpp:1186-1193`); material no descriptor da instância; chave de ordenação de 64 bits com Z-bucket + material (`filament/src/RenderPass.h:68-187`, `RenderPass.cpp:442-456`) | Material em 128 B de push constant por draw (`instanced_renderer.cpp:88-98`, `:6218-6277`); MDI desligado no autoral (`:5448`, `:5460`); ordenação só por distância (`:6384-6386`) | F3 |
| Luzes pontuais | Froxels de 16 fatias e grade de 8192 (`filament/src/Froxelizer.cpp:81-91`, `:283-318`, `:737-856`); laço só na lista do froxel (`shaders/src/surface_light_punctual.fs:23-91`) | Cada fragmento percorre até 8 luzes e lê 4 vec4 antes do teste de alcance (`rhi/shaders/dirt_road_shading.glsl:148-186`) | F4 |
| BRDF e IBL | D_GGX por Lagrange em mediump, V "fast" (`shaders/src/surface_brdf.fs:31-34`, `:54-79`, `:115-120`); SH pré-escalado (`shaders/src/surface_light_indirect.fs:41-62`) | D/V em highp (`dirt_road_shading.glsl:60-100`); SH com cos/sin por fragmento, chamado 2× (`environment_lighting.glsl:70-97`) | F5 |
| Pós | SSAO em meia resolução com blur bilateral (`filament/src/PostProcessManager.cpp:1003-1009`, `:1278`); LUT 3D de grade (`filament/src/details/ColorGrading.cpp:180-199`); pós em resolução interna antes do upscale (`filament/src/details/Renderer.cpp:1640-1690`); EASU mobile + `SPLIT_EASU` no Qualcomm (`PostProcessManager.cpp:3769-3827`; `backend/src/vulkan/VulkanDriver.cpp:1890-1893`) | Névoa refeita em cada tap do FXAA (`post_process_common.glsl:79-145`, `:236-289`); AO de 12 taps em resolução cheia sem blur (`:193-234`); tonemap 3× por pixel por causa do sharpen (`:317-328`); pós em resolução de saída no caminho bilinear | F6 |
| Resolução dinâmica | Mediana de 15 quadros (`filament/src/FrameInfo.cpp:278-310`); PID com zona morta −1%/+5% e anti-windup (`filament/src/details/View.cpp:97-98`, `:151`, `:310-403`) | Tempo cru; 6 quadros seguidos para baixar, 120 para subir (`native/renderer/dynamic_resolution.cpp:33-66`) | F7 |
| Skinning | Ossos só sobem quando a aplicação muda a pose (`filament/src/details/SkinningBuffer.cpp:153-206`) | Dispatch para todos os slots em todo quadro (`instanced_skinning.inl:273-281`) | F8 |
| Quadro e Vulkan | Anel de command buffers com fence (`backend/src/vulkan/VulkanCommands.cpp:265-312`); `FrameSkipper` sem bloqueio (`filament/src/FrameSkipper.cpp:50-70`); descarte adiado por fence (`VulkanDriver.cpp:470-490`) | Uma fence, um command buffer (`rhi/device.cpp:349-367`; `instanced_renderer.h:1039`); `vkDeviceWaitIdle` ao trocar HDRI (`instanced_renderer.cpp:5539`) | F9 |
| Robustez de driver | Contornos para Adreno (`backend/src/vulkan/VulkanFboCache.cpp:280`; `VulkanPlatform.cpp:296`); prewarm de pipelines (`VulkanPipelineCache.cpp:152-161`) | Pipeline genérico criado **sem** `driverCache_` (`rhi/pipeline_cache.cpp:116`) | F0 / F10 |

## 5. Pacotes de trabalho

Ordem: medição, depois o maior custo provável nos projetos reais, depois o que
destrava culling na GPU, depois o custo por fragmento. Cada pacote tem chave de
diagnóstico para A/B no mesmo APK e só é promovido com ganho acima do ruído, sem
piorar caudas nem imagem.

### F0 — Medição e correções triviais (pré-requisito)

1. **Cenário de bancada novo:** Sponza (ou um projeto Synty/BoZo) com personagem
   animado em Play, câmera fixa. Registrar `shadow_cascades`, `shadow_draws`, GPU
   por região, triângulos e draws. Repetir com N corpos físicos (> 128) para expor F2.
2. Registrar no `FrameProfile` a fração serializada: tempo entre a espera da fence
   (`acquireNextImage`) e o submit. Isso decide se F9 vale a pena.
3. Registrar no contexto da Sponza se névoa e AO estão ligados (ler
   `.astra/rendering.astra`). Isso decide a prioridade de F6.
4. Passar `driverCache_` em `createGraphicsPipelineReal` (`rhi/pipeline_cache.cpp:116`).
   Persistir o cache em disco é melhoria própria; o Filament não persiste
   (`VulkanPipelineCache.cpp:147-150`). O efeito é em hitch e carregamento, não em FPS.

### F1 — Sombras (maior impacto esperado em projetos com movimento)

O Filament redesenha tudo em todo quadro e por isso não oferece modelo de cache. A
peça central é a separação estático/dinâmico. Do Filament vêm o enxugamento por
cascata e o filtro.

1. **Render pass por cascata, sem LOAD.**
   - Hoje, com o atlas inicializado, o pass em cache usa `LOAD_OP_LOAD` sobre o
     atlas inteiro e limpa quadrantes com `vkCmdClearAttachments`
     (`instanced_renderer.cpp:2598-2633`).
   - Trocar por um `vkCmdBeginRenderPass` por cascata suja, com `renderArea` igual
     ao quadrante, `loadOp=CLEAR` e `storeOp=STORE`.
   - Conferir `vkGetRenderAreaGranularity`.
2. **Casters estáticos e dinâmicos separados.**
   - Novo `shadowDynamic_[draw]`: liga quando o draw é skinned/morph animado ou teve
     a pose alterada. Volta a estático depois de cerca de 30 quadros parado.
     Opcional: levar `isStatic` (`native/runtime/scene_graph.h:68`) até
     `MapDrawState`.
   - Pose (`instanced_renderer.cpp:5625-5652`): remover o `0xffffffff`. Na
     transição para dinâmico, sujar só as cascatas cuja esfera intersecta os bounds
     antigos (`renderer::isShadowCasterVisible`). Quando o draw volta a estático,
     sujar as cascatas dos bounds atuais.
   - Skin (`instanced_skinning.inl:270`): apenas marcar como dinâmico.
   - Aparência (`:5608`): sujeira só pelos bounds daquele draw.
   - Atlas estático (o atual): desenha só os estáticos.
   - Novo `shadowDynamicAtlas_`:
     - mesmo layout 2×2 e mesmas matrizes em cache (`shadowCascades_`);
     - por quadro, só os dinâmicos que passam no teste da cascata;
     - cascata sem dinâmico não é desenhada.
   - Shader (`environment_lighting.glsl:173`):
     - novo sampler, com máscara de cascatas dinâmicas em
       `shadowTransitionParameters.w` (hoje livre);
     - `visibilidade = min(estático, dinâmico)`;
     - o dinâmico com 4 buscas e só sob o bit (branch uniforme).
   - Rejeitado: copiar o cache para um atlas vivo. São cerca de 16 a 32 MiB de
     tráfego por cascata por quadro, pior num TBDR que um mapa separado limpo
     em GMEM.
3. **D16 só para sombra** (`ShadowMapManager.cpp:1404`).
   - Formato próprio do atlas, preferindo `D16_UNORM`; D32 continua para cena e HZB.
   - Reajustar `depthBiasConstant`: em UNORM a unidade é 2⁻¹⁶, e o 1,25 atual em
     float quase não atua.
   - Apertar a extrusão (`instanced_renderer.cpp:6021`, hoje igual a
     `shadowDistance`, até 320 m) pela faixa z dos casters, como
     `updateSceneInfoDirectional` (`ShadowMap.cpp:1203-1229`).
4. **Orçamento de resolução.**
   - Teto de 2048 por cascata no mobile, salvo pedido explícito do projeto.
   - Registrar a memória no log: 4096 em atlas 2×2 com D32 ocupa 256 MiB.
5. **Filtro em tenda com 4 buscas** (Castaño 2013, `surface_shadowing.fs:35-87`).
   - Substitui as 9 buscas do raio 2, cobrindo a mesma área.
   - A faixa de blend entre cascatas passa a ser seleção com dither, em vez de
     amostragem dupla.
   - Portar a função com cabeçalho Apache-2.0.

- **Referências:**
  - Unity HDRP *Shadows in HDRP*, seção "Mixed cached shadow maps" / "Always draw
    dynamic", HDRP 17.1:
    https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.1/manual/Shadows-in-HDRP
  - Unity HDRP *Shadow update mode*, HDRP 16.0:
    https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@16.0/manual/shadow-update-mode.html
- **Validação:**
  - testes em `tests/native/test_shadow_cascades.cpp`: máscara por bounds,
    histerese, skin sem invalidação;
  - telemetria nova: `shadow_dynamic_draws`, `shadow_dynamic_cascades`,
    `shadow_static_refresh`;
  - A/B nos cenários estático, personagem animado, câmera em movimento e objeto
    que para;
  - capturas de acne e peter-panning antes e depois do D16;
  - sem fantasma da posição antiga.
- **Impacto esperado:** alto em cena com movimento, de quatro vezes a cena inteira
  por quadro para quatro vezes só os dinâmicos. Médio pelo filtro (2,9 ms
  atribuídos à sombra na Sponza). Nenhum dos dois foi medido.
- **Risco:** médio. O `min` entre as duas fontes é aproximação, e a diferença de
  filtro entre elas pode aparecer.

### F2 — Poses sem teto e material efetivo cacheado

1. Trocar `std::array<MapDrawUpdate,128>` (`instanced_renderer.h:1161`) por vetor
   dimensionado no commit. Acima de um limiar, escrever as instâncias em massa.
   Republicar só quando a topologia mudar.
2. Calcular o material efetivo (`applyMaterialOverride`) uma vez no commit e na
   mudança de estado. Hoje ele roda várias vezes por draw por quadro, inclusive por
   cascata (`instanced_renderer.cpp:2641`, `:6222`, `:6285`, `:6332`, `:6603`).
3. Montar uma SoA de bounds. Portar o `Culler` NEON (`filament/src/Culler.cpp:42-160`)
   só se a medição mostrar cenas com mais de cerca de 5 mil draws.

- **Referência:** Godot 4.5 *Internal rendering architecture*:
  https://docs.godotengine.org/en/4.5/engine_details/architecture/internal_rendering_architecture.html
- **Validação:** cena com mais de 128 corpos em Play. Esperado: zero republicações,
  histórico do TAA preservado e cache de sombra sem reset total.
- **Impacto e risco:** robustez alta; FPS médio em cenas dinâmicas. Risco baixo.

### F3 — Material em SSBO, MDI e culling na GPU nas cenas autorais

1. SSBO `DrawMaterialRecord` com o material efetivo resolvido, indexado por
   `gl_InstanceIndex`, que já carrega `drawIndex`. É atualizado de forma incremental
   a partir de `pendingAuthoredState_` (`instanced_renderer.cpp:5598-5616`).
2. Tirar das push constants o dado por quadro (câmera, `surfaceTransform`,
   near/far) e levá-lo para o UBO de quadro.
3. Chave de lote: variante de pipeline, cull mode, distante, e set quando não houver
   bindless. Reativar `useMultiDrawIndirect_` no autoral e, com ele, o culling na
   GPU e o HZB existentes (`:6738-6756`).
4. Chave de ordenação de 64 bits:
   - Z-bucket de 10 bits mais id de pipeline/material, como o Filament
     (`RenderPass.h:68-187`);
   - cache de VB/IB/cull mode em `drawMapPrimitive` (`:6326-6327` revincula em
     todo draw).
5. Auto-instancing (`RenderPass.cpp:287-368`) fica para depois, só com buffer de
   indireção. O próprio Filament o deixa desligado.

- **Referência:** Unity 6.0 *GPU Resident Drawer* (BatchRendererGroup):
  https://docs.unity3d.com/6/Documentation/Manual/urp/gpu-resident-drawer.html
- **Validação:**
  - A/B de GPU e CPU com contagem de submissões;
  - capturas idênticas pixel a pixel;
  - overrides por instância (UV, alpha source, cutoff) conferidos em cena de teste;
  - `tools/validate-opaque-shaders.ps1` estendido ao novo ABI.
- **Impacto esperado:** alto. É o único item que leva culling e oclusão na GPU ao
  caminho autoral. Os dados anteriores de HZB (`estrutural.md`: 21 draws e 48
  triângulos removidos) recomendam cautela.
- **Risco:** médio-alto. Muda o ABI dos shaders de mapa e de sombra; a serialização
  não muda.

### F4 — Luzes pontuais por froxel

- Grade de tiles quadrados (cerca de 64 a 80 px) com fatias log2 de profundidade,
  como `Froxelizer.cpp:283-318` e `:460-471`.
- Atribuição na CPU por esfera/cone por froxel (`Froxelizer.cpp:868`).
- Com até 8 luzes (`native/renderer/punctual_lights.h:18`), basta uma máscara `uint8`
  por froxel num SSBO. O shader faz `findLSB` e lê só as luzes do froxel; o record
  buffer compactado do Filament não é necessário.
- Manter a especialização de pontuais já aceita (`play-lighting-2026-10-05`) como
  caminho rápido quando o predicado for verdadeiro.
- **Referências:**
  - Godot 4.5 *Internal rendering architecture* (Forward+ clustered):
    https://docs.godotengine.org/en/4.5/engine_details/architecture/internal_rendering_architecture.html.
    A Godot avisa que o clustered é menos eficiente no mobile; por isso a variante
    proposta é de máscara simples e precisa de A/B.
  - Unity URP 17.0 *Forward+ Rendering Path*:
    https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/rendering/forward-plus-rendering-path.html
- **Validação:** luzes de alcance curto espalhadas; imagem idêntica com teste
  conservador; GPU do opaco.
- **Impacto esperado:** médio a alto na Sponza. Restam cerca de 2 ms atribuídos às
  pontuais depois da especialização, mas o número e o alcance das luzes não estão
  nos logs (não verificado).
- **Risco:** baixo para a imagem; médio para o custo, já que o Adreno tem sua
  própria divergência.

### F5 — BRDF e IBL mais baratos

1. D_GGX pela identidade de Lagrange, em mediump, com clamp (`surface_brdf.fs:54-79`).
2. V Smith "fast" (`surface_brdf.fs:115-120`).
3. Burley contra Lambert fica como opção separada, porque muda a borda rasante e a
   identidade visual.
4. SH rotacionado e pré-escalado na CPU, deixando só MADs no shader
   (`surface_light_indirect.fs:41-62`).
5. Não adotar a compensação multiscatter nem o specular AA sem decisão do
   proprietário: são mudanças de aparência, não de custo.

- **Referência:** Godot 4.5 *Internal rendering architecture*, mesmo link de F2.
  O renderer Mobile da Godot também privilegia precisão reduzida.
- **Validação:** comparação de pixels com limite de erro em rugosidade baixa (0,07)
  e alta; `spirv-val`.
- **Impacto esperado:** médio (0,3 a 1 ms, não verificado). **Risco:** baixo a médio.

### F6 — Pós-processamento

1. Névoa uma vez no texel central, não em cada tap do FXAA
   (`post_process_common.glsl:142`, `:236-289`).
2. AO em passe próprio, em meia resolução, com blur bilateral
   (`PostProcessManager.cpp:1003-1009`, `:1278`).
3. Tonemap e grade avaliados uma vez por pixel, com o sharpen em luma. Opcional:
   LUT 3D de 16³ ou 32³ (`ColorGrading.cpp:180-199`) gerada a partir de
   `astraToneMap`, preservando a curva.
4. Pós em resolução interna antes do upscale bilinear, reaproveitando o caminho do
   FSR (`instanced_fsr.inl`). Requer A/B, porque uma tentativa anterior de remover
   passe foi pior (`docs/ORCAMENTO-120HZ.md` §6).
5. EASU mobile (MIT) mais `SPLIT_EASU` no Adreno, se o FSR1 estiver ativo.
6. HUD e UI num único passe no caminho não fundido (`instanced_renderer.cpp:1714-1723`,
   `:1792-1798`).
7. Opcional com A/B: cor HDR em R11G11B10F (`filament/src/details/Renderer.cpp:184-189`).
   Exige dither, e hoje o alfa é lido como fallback reativo do diagnóstico temporal.

- **Referências:**
  - Godot 4.5 *Resolution scaling*:
    https://docs.godotengine.org/en/4.5/tutorials/3d/resolution_scaling.html
  - Unity 6.3 LTS URP *Color Lookup*:
    https://docs.unity3d.com/Manual/urp/post-processing-color-lookup.html
- **Validação:** A/B do passe de pós; capturas; névoa e AO ligados e desligados.
- **Impacto esperado:** médio se névoa ou AO estiverem ativos na Sponza (pós de 3,46
  contra 1,10 ms na dirt-road); baixo se não. **Risco:** baixo.

### F7 — Resolução dinâmica filtrada

- Mediana de 15 quadros (`FrameInfo.cpp:278-310`).
- Zona morta assimétrica de −1%/+5% e anti-windup (`View.cpp:151`, `:310-403`).
- Os passos discretos atuais podem ser mantidos, alimentados pela mediana.
- Cada mudança de escala invalida TAA e HZB (`instanced_renderer.cpp:5577-5580`), por
  isso menos oscilação também é ganho de imagem.
- **Referência:** Godot 4.5 *Resolution scaling* (mesmo link de F6).
- **Validação:** contagem de mudanças de escala por minuto e p95 de GPU no mesmo
  cenário. **Impacto:** médio em estabilidade. **Risco:** baixo.

### F8 — Skinning sob demanda

- Pular o `vkCmdDispatch` do slot cuja pose não mudou, ou que não está visível nem
  para a câmera nem para uma cascata (`instanced_skinning.inl:273-281`).
- Cuidar da pose anterior usada pelos vetores de movimento.
- Opcional: paleta 3×4 + cofator (`SkinningBuffer.cpp:153-206`), −25% de envio.
- **Referência:** Unity 6.0 *GPU Resident Drawer* (mesmo link de F3), sobre trabalho
  por objeto só quando necessário.
- **Impacto esperado:** médio com vários personagens. **Risco:** médio.

### F9 — Dois quadros em voo (só se F0.2 justificar)

- A medição da dirt-road (0,52 ms de gravação + submit) indica ganho pequeno. Cenas
  autorais com draws individuais podem gravar por mais tempo. Decidir pelo dado.
- Pré-requisito: fila de destruição adiada por fence, como `VulkanDriver.cpp:470-490`,
  substituindo o `vkDeviceWaitIdle` de `instanced_renderer.cpp:5539`.
- Anel de 2 slots com command buffer, fence, semáforo, UBO, buffers de instância e
  indiretos, query pool e readbacks lendo N−2.
- Há 48 usos de `mappedData()` em `instanced_renderer.cpp` a auditar.
- **Risco:** alto (sincronização). Também aumenta a ocupação da GPU, logo o calor; o
  DRS compensa baixando a escala.

### F10 — Robustez de driver

- Auditar os contornos de Adreno do backend do Filament (`VulkanFboCache.cpp:280`,
  `VulkanPlatform.cpp:296`).
- Prewarm de pipelines numa thread de baixa prioridade (`VulkanPipelineCache.cpp:152-161`).
- Ganho de robustez e hitch. Importante quando houver aparelhos Mali/Xclipse para
  testar.

### Fora do Filament, mas no mesmo pacote de desempenho

- **Reimportar fontes antigas com o preset atual** (LOD + ASTC 6x6). Fontes
  publicadas antes do preset mantêm `generateLods=false` e `textureCompression=0`
  (`native/resources/import_profile.h:60`, `:71`).
- Oferecer no editor uma ação explícita de reimportação, sem migração silenciosa.
- Malhas com skin continuam sem LOD.
- **Referência:** Godot 4.5 *Mesh level of detail*:
  https://docs.godotengine.org/en/4.5/tutorials/3d/mesh_lod.html
- Qualquer botão novo no editor segue as regras de interface do `AGENTS.md`:
  captura real, proposta visual, ícone e captura posterior.

## 6. O que não extrair

- Backend Vulkan, frame graph e `matc`/material system do Filament: são
  arquitetura, não técnica. O render graph e o material próprios já atendem.
- LiSPSM: incompatível com cascata estável e com o cache estático (o próprio
  Filament o desliga em modo `stable`, `ShadowMap.cpp:173-185`).
- Shadow atlas quadtree: está atrás de feature flag desligada
  (`libs/utils/include/private/utils/FeatureFlagManager.h:54`).
- Auto-instancing antes de F3.
- VSM/PCSS: custo de blur por quadro, sem relação com o gargalo.

## 7. Cronograma e gates

| Ordem | Pacote | Gate para avançar |
|---|---|---|
| 1 | F0 | Cenário com personagem animado medido; fração serializada e configuração de pós registradas |
| 2 | F1.1, F1.3 a F1.5 | A/B com imagem dentro do limite; GPU da região Shadow menor |
| 3 | F1.2 | Personagem animado: cache estático 0/4 na maioria dos quadros, sem fantasma |
| 4 | F2 | Mais de 128 corpos sem republicação |
| 5 | F6.1 a F6.3, F7 | A/B do pós; oscilação de DRS menor |
| 6 | F4, F5 | A/B do opaco; capturas |
| 7 | F3 | Pixels idênticos, overrides corretos, ganho líquido |
| 8 | F8, F6.4 a F6.7 | A/B próprio |
| 9 | F9, F10 | Só com dado de F0.2 e aparelho adicional |

Cada pacote fecha com:

- testes host;
- build Release;
- A/B/B/A de 30 s por rodada com SurfaceFlinger contínuo;
- comparação de pixels;
- registro térmico;
- hashes da cena e da configuração;
- relatório em `docs/validacao/<pacote>-<data>/`.

Nenhum ganho de um pacote é somado ao de outro sem novo A/B.

## 8. Riscos

| Risco | Mitigação |
|---|---|
| D16 traz acne ou peter-panning | Bias reajustado, extrusão apertada, capturas por cascata |
| Atlas dinâmico dobra a memória de sombra | Com os dois atlas em D16, a memória fica igual à de hoje com um atlas D32; teto de 2048 |
| ABI do F3 quebra overrides por instância | Cena de teste com todos os overrides; validação de SPIR-V |
| Clustered no Adreno não compensa | Máscara simples; especialização atual mantida como caminho rápido |
| 120 Hz em resolução nativa continua fora de alcance | Fato medido: só o opaco com base color já passa do frame. O plano reduz custo; não promete 120 Hz nativo |

## 9. Registro da entrega

- **Conceito:** este plano. Os relatórios de estudo foram feitos sobre o código do
  Filament v1.80.0 e o código atual da Astra; as afirmações-chave foram conferidas
  por amostragem nos dois.
- **Implementado:** nada.
- **Testado no aparelho:** nada.
- Não há mudança de comportamento do app; quando um pacote for implementado, a nota
  correspondente entra em `AstraDocs/data/updates.json`, conforme o `AGENTS.md`.
