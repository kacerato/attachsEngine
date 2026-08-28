# Esfera de materiais — recorte vertical da Fase 2

## Decisão e vínculo com o plano

Solicitação: substituir a referência de cubos por uma esfera de materiais com
qualidade alta, sem abandonar mobile-first. Entrega: esfera UV indexada, PBR
direto, reflexão de estúdio pré-filtrada e três mapas **8K reais**. Os itens
**2.4.2, 2.4.4 e 2.4.6** avançam para **parcial**; **2.5.3** continua parcial.
Nenhum gate M0–M9 foi fechado por esta amostra.

Alternativas consideradas: aumentar somente a resolução do checker não testa
materiais; 16K RGBA8 consome 1 GiB por mapa sem mips. Adotamos 8K ASTC 6×6 com
cadeia completa de mipmaps, fallback e limite residente. Não fazemos upscale
nem anunciamos 16K. A esfera é uma primitiva regular para testar materiais,
não uma cópia do modelo proprietário de uma shader ball com recortes.

## Fluxo e ownership

`ScenePreview → MeshRenderer/ResourceId → RenderSceneExtractor → RenderInstance
(ABI v1, 88 bytes) → InstancedRenderer → Vulkan`.

- `PbrMaterial`: recurso gerenciado imutável, IDs estáveis de albedo/normal/ARM,
  fatores de roughness/metallic/normal, JSON v1 com validação. O Core não conhece
  Vulkan nem o sample. `PbrMaterialParameters` cruza uma vez a fronteira nativa
  em 12 bytes; a matriz/tint continuam no lote de instâncias existente.
- `makeUvSphere`: utilitário de geometria reutilizável, UV, normais, tangente
  com handedness, costura duplicada e sem triângulos degenerados nos polos.
  Sample: 192 segmentos × 96 anéis, **36.480 triângulos**, 18.721 vértices.
- Normais usam inversa transposta; tangentes respeitam a transformação e o
  sinal do determinante. A extração do material recusa matrizes singulares.
- `MaterialPreviewResources`: adaptador Android do sample, dono dos buffers,
  cinco imagens e cinco samplers. Somente aqui os IDs embutidos são associados
  ao pacote conhecido. Ainda não é catálogo GPU genérico nem Asset Browser.
- RHI: `ImageDesc.mipLevels`, descrição de recursos, upload de cadeia completa,
  barreiras de todos os níveis e formatos ASTC/sRGB/RGBA8/RGBA16F. O contrato
  antigo `uploadRgba8` de um nível continua válido e é testado separadamente.
- Bindless registra cinco mapas; fallback convencional usa cinco descritores
  fixos e funciona com descriptor indexing realmente desabilitado no device.

No carregamento, um worker possui exclusivamente renderer e fila gráfica;
a thread de eventos continua atendendo Android. Um `future.get()` transfere
ownership antes do primeiro frame. Mudança/destruição de surface cancela e
aguarda o worker **antes** de liberar device/swapchain. IO verifica cancelamento
entre blocos de 1 MiB; upload já submetido espera sua fence. Portanto não há
promessa de cancelamento instantâneo caso o driver demore.

Após inicialização, draw/extração/mutações ocorrem na thread de render/eventos,
sem uploads, bakes ou construção de malha por frame. Há no máximo uma textura
temporária de CPU e seu staging durante cada upload. Descritores são liberados
antes das imagens; staging após fence. Surface recriada recarrega recursos GPU,
mas não apaga o World gerenciado. Não há cache ilimitado de bitmaps.

## Material e iluminação que realmente existem

GGX de espalhamento único, visibilidade Smith correlacionada, Fresnel Schlick,
difuso Burley e normal mapping em TBN. Duas luzes direcionais diagnósticas e
IBL especular split-sum: estúdio lat-long RGBA16F 256×128/9 mips, pré-filtrado
offline com 256 amostras GGX por texel, LUT BRDF 128²/512 amostras. O nível zero
do estúdio amostra diretamente o ambiente analítico.

Referência matemática: [Filament](https://google.github.io/filament/main/filament.html).
Shaders próprios; não representam implementação integral de Filament.
Difuso ambiente é preenchimento constante, **não SH**. Exposição fixa 1,15 e
Reinhard são aplicados no shader; a conversão sRGB ocorre exatamente uma vez,
por attachment sRGB ou explicitamente no fallback UNORM.

Faltam multiscatter/validação contra render offline, SH, skybox visual,
reflection probes, luzes como componentes, sombras, AgX, bloom, TAA, AA
configurável e pós-processamento em passes próprios. Não se anuncia o
renderer PBR completo do plano nem qualidade máxima universal.

## Formato e custo

`AETX v1`: header little-endian de 32 bytes (`magic`, versão, largura, altura,
encoding, níveis como u32; tamanho do payload como u64), seguido dos mips
contíguos. Encodings 1/2 = ASTC6×6 sRGB/linear; 3/4 = RGBA8 sRGB/linear;
5 = RGBA16F. Decoder verifica versão, formato, dimensão, tamanho exato e
overflow. Não é substituto de KTX2/import pipeline definitivo.

| Recurso residente | Dimensões/mips | Payload |
|---|---|---|
| Albedo + normal + ARM ASTC | 3 × 8192², 14 níveis | 119.450.496 bytes (~113,9 MiB) |
| Os três mapas fallback RGBA8 | 3 × 1024², 11 níveis | 16.777.212 bytes (~16 MiB) |
| Estúdio + BRDF RGBA16F | 256×128/9 + 128²/1 | 480.600 bytes |

Valores são payload, **não RSS nem consumo total de GPU**: alignment/VMA,
depth/swapchain, runtime, geometria e staging são adicionais. A seleção usa
capabilities ASTC/formato, maxImageDimension2D e quota de texturas disponível;
limite do sample 8192 e até 128 MiB para os três mapas, com margem da quota.
Mips superiores são pulados por seek quando o orçamento não comporta 8K.
Sem ASTC, usa fontes reduzidas RGBA8. Recursos sem formato ou orçamento válido
falham com log e cleanup; não são substituídos silenciosamente por um cubo.

Essa seleção é de **residência inicial**, não streaming dinâmico, adaptação
térmica, descoberta da RAM física ou `VK_EXT_memory_budget`. A quota é a
política do allocator existente, não garantia de memória livre do Android.

## Uso e reprodução

Ao abrir pelo launcher, a esfera é o sample padrão. Arrastar gira a câmera.
Ainda não há pan/zoom nem Inspector/edição de materiais pela UI.

```powershell
./android/gradlew.bat -p android :app:assembleDebug :app:assembleRelease :app:lintDebug --offline --console=plain
./tools/validate-android-scene.ps1 -MaterialPreview
./tools/validate-android-rendering.ps1
./tools/validate-android-scene.ps1
```

Opções explícitas de inicialização: `aether.material_preview=true` esfera,
`aether.scene_preview=true` fixture de cubos, `aether.poc_a=true` benchmark de
5.000 cubos. Material explícito tem prioridade sobre os demais. Os runners de
benchmark usam `poc_a`, evitando medir a esfera como se fossem 5.000 objetos.
`aether.force_texture_fallback=true` força 1K RGBA8 para testar o caminho.
Opções são lidas ao iniciar processo; para trocar, use `am start -S`.

Teste de cena exige debug com Khronos validation, aparelho desbloqueado pelo
usuário e preserva dados (`install -r`). Exercita 1/1/0/1 instâncias, hierarquia,
escala não uniforme, remoção/recriação e background/foreground. Compara snapshot
de CPU e câmera e mantém screenshots completas; não confunde hash CPU com
readback da GPU. Diferenças de display são reportadas e exigem revisão visual.

## Evidência — 28/08/2026

- 506 testes C#, 165 C++, 47 verificações PowerShell e 6 testes do import.
- SPIR-V gerado e validado nos dois caminhos; debug/release/lint compilados.
- Xiaomi 25053PC47G / SM8735 / Adreno / Android 16, surface 2772×1280:
  `build/android-validation/material-async-20260828/report.json` passou nos
  caminhos bindless, convencional e textura fallback, com mesmo PID/snapshot
  após retomada. Logs confirmam três mapas 8192²/14 mips no caminho ASTC.
- Capturas antes/depois foram inspecionadas: esfera visualmente preservada;
  a desigualdade da captura completa, incluindo indicador OEM, continua no
  relatório. Não se exige igualdade entre ASTC 8K e RGBA8 1K.
- Carga debug observada ~1,5–1,9 s nessa rodada; agora fora da thread de eventos.
  Ainda há espera visual sem painel de progresso. IO/cache/driver afetam esse
  tempo; não é promessa de desempenho em outros aparelhos.
- `material-async-20260828/cancel-loading-logcat.txt`: HOME durante a carga
  exercitou cancelamento e retomada no mesmo PID, sem VUID/crash observado.
- `material-final-20260828/report.json`: rodada debug final dos três caminhos,
  sem erros Vulkan detectados; regressões separadas em
  `material-regression-poca-20260828/` e `material-regression-cube-20260828/`.
- Build otimizado instalado preservando dados. No primeiro checkpoint,
  `material-final-20260828/release-profile-logcat.txt` contém três janelas de
  600 frames: CPU média por janela 1,15–1,21 ms, pico 4,37 ms; thread de render
  média 0,64–0,69 ms. Carga de recursos ~370 ms nessa execução com cache quente.
  São métricas da esfera, não da PoC-A nem do tempo GPU.
- `release-surface-fps.json`: amostra separada de SurfaceFlinger,
  **120,11 FPS exibidos em 5,14 s**, mínimo móvel 120/1 s. Esse checkpoint curto
  não prova estabilidade térmica, orçamento energético ou outros aparelhos.
  Game Turbo/Wild Boost do fabricante apareceu na captura; não foi configurado
  por esta implementação nem controlado como variável de benchmark.
- APK otimizado final instalado: `build/material-preview-release-final.apk`,
  SHA-256 `69e3b1378e8c2e2d218b83104ddfb865636a4a1b14500d31a5623ab88f1aa9e0`.
  `release-final-logcat.txt` confirma carga 8K e primeiro frame ao iniciar sem
  opções de sample. Assinado com a chave local de desenvolvimento para manter
  dados; não é assinatura de distribuição. A revisão final nomeou explicitamente
  os campos do push constant e validou seus offsets, sem mudar o layout/shader.
  A coleta de perfil anterior corresponde ao APK
  `7b13dd9403fad3ba7928638dc3e5d5872c6f7f654b9242fe10675ac9a351cf95`;
  não foi repetido o perfil inteiro após essa revisão de nomes/offsets.

## Próxima dependência, sem ampliar este recorte

Conectar o caminho de recursos ao editor: UI retida/GPU, seleção/Inspector
alimentados pela metadata e comandos de edição. Em paralelo ao roadmap de
render, ainda faltam múltiplos pares mesh/material, importadores, luzes/sombras
e os testes completos de 2.4. O sample não substitui Sponza-equivalente, FPS,
energia e matriz de aparelhos exigidos por M2; soak de 30 min não foi retomado.
