// Compactação dos comandos indiretos — o elo que faltava entre a oclusão GPU
// (gpu_draw_culling.h) e a submissão do passe opaco.
//
// Hoje draw_cull.comp zera o `instanceCount` de quem foi ocluído e o passe opaco
// continua submetendo `commandCount` comandos por lote. Um comando com
// instanceCount zero não rasteriza nada, mas ainda é buscado da memória,
// decodificado e processado pelo front-end: em uma vista com centenas de draws
// o custo de quem não aparece na tela não é zero, apenas invisível no frame.
//
// Esta camada fecha o laço: um segundo kernel varre cada lote, empurra os
// sobreviventes para o início de uma lista compacta e escreve quantos sobraram.
// A submissão passa a ser `vkCmdDrawIndexedIndirectCount`, que lê esse número da
// própria GPU. O que foi ocluído deixa de existir para o front-end.
//
// **Estabilidade é requisito, não detalhe.** A ordem relativa dos comandos
// sobreviventes é preservada exatamente. O lote já chega ordenado front-to-back
// e essa ordem alimenta o early-Z/LRZ do tile; compactar com `atomicAdd` seria
// mais curto de escrever e destruiria a ordem de forma não determinística, o que
// trocaria trabalho de front-end por overdraw e tornaria o frame irreprodutível
// entre execuções. Por isso o kernel usa soma de prefixo exclusiva, e a função
// de referência abaixo é o espelho instrução por instrução dele.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"

#include <span>

namespace ae::renderer {

// Tamanho do grupo de trabalho de draw_compact.comp. A varredura acontece em
// blocos desse tamanho e o resto do lote é levado adiante por um acumulador em
// memória compartilhada, de modo que um lote maior que o bloco continua estável.
// Sessenta e quatro cabe no mínimo garantido de 128 invocações por grupo do
// Vulkan, portanto o kernel não depende de nenhum limite negociado.
inline constexpr u32 kGpuCompactionGroupSize = 64;

// Um lote de comandos que compartilha pipeline e material, portanto um intervalo
// contíguo da lista indireta. Layout std430 de 16 bytes, espelhado campo a campo
// em draw_compact.comp; `static_assert` no .cpp tranca o tamanho.
//
// `compactedBase` é o destino do lote na lista compacta. Ele é um campo próprio,
// e não `firstCommand` reaproveitado, porque a lista compacta não precisa ter o
// mesmo particionamento da lista de origem — o chamador atual usa os dois iguais,
// mas a camada não impõe isso e o teste cobre o caso deslocado.
struct alignas(16) GpuCompactBatch final {
  u32 firstCommand = 0;
  u32 commandCount = 0;
  u32 compactedBase = 0;
  u32 reserved = 0;
};

// Espelho do bloco de push constants de draw_compact.comp.
struct alignas(16) GpuCompactParameters final {
  u32 batchCount = 0;
  u32 sourceCapacity = 0;
  u32 compactedCapacity = 0;
  u32 reserved = 0;
};

// Um lote é aceito quando cabe inteiro nas duas listas e não invade o destino de
// outro lote. Lotes vazios são legais: um material pode ficar sem nenhum draw
// visível neste frame e o kernel precisa escrever contagem zero para ele.
//
// A verificação de sobreposição é O(n²) e roda uma vez por reconstrução de lotes,
// não por frame — n aqui é o número de materiais visíveis, não de draws.
bool validateGpuCompactBatches(std::span<const GpuCompactBatch> batches, u32 sourceCapacity,
                               u32 compactedCapacity) noexcept;

// Referência de CPU do kernel: para cada lote, copia para `outSourceIndices` os
// índices de origem cujo `instanceCount` é diferente de zero, preservando a ordem,
// e escreve em `outCounts[lote]` quantos sobreviveram.
//
// Devolve false — sem escrever nada — quando os lotes não passam na validação ou
// quando algum span de saída é pequeno demais. Falhar fechado aqui importa: um
// `outCounts` truncado viraria um `vkCmdDrawIndexedIndirectCount` lendo lixo.
bool compactDrawCommandsReference(std::span<const u32> instanceCounts,
                                  std::span<const GpuCompactBatch> batches,
                                  std::span<u32> outSourceIndices,
                                  std::span<u32> outCounts) noexcept;

// Um grupo de trabalho por lote. O kernel varre o lote inteiro em blocos, então
// o número de grupos não depende de quantos comandos o lote tem.
inline constexpr u32 gpuCompactionGroupCount(u32 batchCount) noexcept { return batchCount; }

} // namespace ae::renderer
