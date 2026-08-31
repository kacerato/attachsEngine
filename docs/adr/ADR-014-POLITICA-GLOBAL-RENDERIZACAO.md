# ADR-014 — Política global de renderização orientada por budgets

- **Estado:** aceita; cadência e núcleo de qualidade implementados, consumo pelo renderer parcial
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

Eixos entregues: sombras (contagem/resolução de cascata, taps de PCF, alcance, bias
constante/slope/normal-offset, ancoragem em texel), ambiente (constante, hemisfério,
hemisfério + sonda especular), pós (nenhum, tonemap, tonemap + bloom), texturas (bias
de residência e anisotropia) e escala de resolução.

A pressão térmica piora um degrau por nível e **não** grava no projeto: a mesma entrada
sem pressão devolve exatamente a qualidade original, o que um teste tranca. A
recuperação com histerese continua pendente e entra junto do soak — histerese sem
medição reproduziria o laço de realimentação que a cadência adaptativa produziu (ver
`PROFILING-ANDROID.md`, "Cadência de apresentação e DVFS").

### Validação em hardware

No Xiaomi de referência, com Release assinado:

```
[RenderPolicy] features: vulkan1_3=1 descriptor_indexing=1 nonuniform=1
               ray_query=0 mesh_shader=0 vrs=0 memoryless=0 -> perfil=1
[RenderPolicy] preset=auto perfil=1 sombras=on(3 cascatas @1536, 9 taps)
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
3. **O renderer ainda não consome os eixos.** A política é resolvida, registrada e
   auditável, mas sombras, pós e escala de resolução ainda não têm implementação de
   frame que os leia. É o próximo passo, e a matemática de cascata já está pronta e
   testada em `renderer/shadow_cascades.h`.

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
