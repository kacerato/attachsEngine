# Recursos RHI — fatia M0 (28/08/2026)

## Decisão e escopo

O cubo texturizado do M0 consome recursos reutilizáveis em `native/rhi/`, sem
levar VMA, parsing de assets ou tipos Android para o Core. O checker e a
geometria por índice são dados de demonstração do shell, não um Material ou
MeshResource de produto. Não há alteração de formatos persistidos/ABI C#.

## Ownership e threading

- `VulkanDevice` possui `VulkanMemoryAllocator`; ambos sobrevivem aos recursos.
- `VulkanBuffer` possui buffer/alocação/map; `VulkanImage`, imagem/view/alocação.
  Ambos são move-only, com `reset()` idempotente e rollback da reserva se a
  criação falhar. Não destruir enquanto comandos GPU os referenciam.
- `VulkanSampler` é move-only e depende do device vivo. O chamador deve respeitar
  features habilitadas e limites físicos, especialmente anisotropia. O shell
  usa nearest sem anisotropia.
- `MemoryBudgetTracker` é thread-safe; os recursos e o contexto de upload não
  são. Acesso à fila/pool precisa ser serializado pelo chamador.
- Shutdown: esperar GPU, destruir comandos/framebuffers/pipeline/descritores,
  sampler/imagens/buffers, upload context, swapchain, allocator e device.

Os budgets contabilizam tamanho de alocação VMA, não memória física total do
processo nem todos os blocos reservados pelo driver. Cotas são uma política
inicial estática (75% dos heaps device-local distribuídos por categoria), não
medição de pressão em tempo real. `VK_EXT_memory_budget` é trabalho futuro.

## Upload inicial

`uploadRgba8ToSampledImage` aceita somente imagem nova, não usada em comandos,
RGBA8 UNORM/SRGB 2D, uma camada e um mip, com linhas compactas. Recusa tamanho
incorreto/overflow, uso sem TransferDst+Sampled, formato/aspecto incompatível,
allocator/device diferente e segunda submissão pelo mesmo recurso.

Fluxo: staging mapeado → cópia/flush → UNDEFINED para TRANSFER_DST → cópia GPU →
SHADER_READ_ONLY → submit/fence → liberar staging. O destino fica pronto para
leitura no fragment shader da mesma fila gráfica. O fluxo é síncrono e restrito
à inicialização/recriação do shell; não é um sistema de importação assíncrona.

Depth tem seleção de formato suportado (D32, D24S8, D16), clear a cada frame e
store descartável. Há um depth compartilhado, correto somente enquanto existir
um frame em voo com fence aguardada antes da próxima gravação.

## Orientação e toque

O aparelho reportou `currentTransform=ROTATE_90`. Declarar essa pré-rotação sem
trocar o extent e transformar clip-space achatava o cubo. `SurfaceTransform`
agora centraliza rotações/espelhos; swapchain/depth usam dimensões naturais e a
projeção usa dimensões visíveis. Referência: [pré-rotação Vulkan no Android](https://developer.android.com/games/optimize/vulkan-prerotation).

O shell traduz o ID do dedo em yaw/pitch normalizados pelo tamanho da janela,
limita pitch e cancela o arraste ao suspender. O renderer recebe só radianos.
A captação ainda não alimenta `managed/Aether.Core/Input/InputState`: é a fatia
de órbita M0, não o sistema completo de input da engine.

## Shaders e verificação

`tools/generate-embedded-shaders.ps1 -ShaderName instanced` recompila com o NDK
pinado, valida SPIR-V e gera o header. Também aceita `triangle`. `-Check` não
altera arquivos e falha se o header divergir; o CI Android executa ambos.
Slang/HLSL, reflexão, variantes/cache e hot reload não estão implementados.

Validação local: 127/127 testes C++ e 442/442 C# (zero skips), debug/release ARM64,
lint debug e lint vital release. O runner real em `build/android-validation/
rhi-cube-final-20260828/` passou 1.000 frames, 2 retomadas, configuração e tela
off/on; capturas foram inspecionadas. Não foi usada validation layer Vulkan.
Os 100 ciclos históricos do triângulo não são atribuídos a esta nova versão.
A repetição recompilada `rhi-cube-verified-20260828` passou startup, retomadas e
configuração, mas excedeu 20 s no ciclo de tela; o mesmo PID retomou depois.
O motivo desse atraso não foi isolado. Relatório de falha permanece preservado.
A regressão final sem ciclo de tela (`rhi-cube-normal-20260828`) passou com
2 retomadas e configuração. Toque/captura final: yaw=2.802, pitch=-0.777;
`touch-evidence.txt` e `after-touch.png` no mesmo diretório.

M0 permanece aberto. Crossing C# medido isoladamente não comprova CPU total
<3 ms ou 60 FPS; novos picos de startup/retomada estão registrados em ESTADO.md.
Próximos critérios ainda incluem hot reload C#, soak de 30 min e matriz de GPUs.
