#include "renderer/gpu_draw_compaction.h"

namespace ae::renderer {
namespace {

// O kernel indexa `batches[]`, `commands[]` e `counts[]` como std430. Um campo a
// mais aqui e o shader passa a ler o lote seguinte sem nenhum sintoma imediato.
static_assert(sizeof(GpuCompactBatch) == 16, "draw_compact.comp le lotes de 16 bytes.");
static_assert(alignof(GpuCompactBatch) == 16, "std430 alinha o lote em 16 bytes.");
static_assert(sizeof(GpuCompactParameters) == 16,
              "draw_compact.comp declara quatro escalares de push constant.");

// Soma de dois índices sem estourar u32. O kernel roda em aritmética modular e
// silenciosamente daria a volta; a referência precisa recusar em vez de imitar
// esse comportamento, porque é ela quem decide se o lote é legal.
bool addFits(u32 base, u32 count, u32 capacity) noexcept {
  return count <= capacity && base <= capacity - count;
}

bool rangesOverlap(u32 firstBase, u32 firstCount, u32 secondBase, u32 secondCount) noexcept {
  if (firstCount == 0 || secondCount == 0) return false;
  return firstBase < secondBase + secondCount && secondBase < firstBase + firstCount;
}

} // namespace

bool validateGpuCompactBatches(std::span<const GpuCompactBatch> batches, u32 sourceCapacity,
                               u32 compactedCapacity) noexcept {
  for (usize index = 0; index < batches.size(); ++index) {
    const GpuCompactBatch &batch = batches[index];
    if (!addFits(batch.firstCommand, batch.commandCount, sourceCapacity)) return false;
    // O destino reserva o pior caso do lote: se todos sobreviverem, todos cabem.
    // Reservar apenas o número de visíveis exigiria conhecer o resultado antes de
    // calculá-lo, e o kernel escreve enquanto conta.
    if (!addFits(batch.compactedBase, batch.commandCount, compactedCapacity)) return false;
    for (usize other = 0; other < index; ++other) {
      if (rangesOverlap(batch.compactedBase, batch.commandCount, batches[other].compactedBase,
                        batches[other].commandCount))
        return false;
    }
  }
  return true;
}

bool compactDrawCommandsReference(std::span<const u32> instanceCounts,
                                  std::span<const GpuCompactBatch> batches,
                                  std::span<u32> outSourceIndices,
                                  std::span<u32> outCounts) noexcept {
  const u32 sourceCapacity = static_cast<u32>(instanceCounts.size());
  const u32 compactedCapacity = static_cast<u32>(outSourceIndices.size());
  if (instanceCounts.size() > 0xffffffffu || outSourceIndices.size() > 0xffffffffu) return false;
  if (outCounts.size() < batches.size()) return false;
  if (!validateGpuCompactBatches(batches, sourceCapacity, compactedCapacity)) return false;

  for (usize batchIndex = 0; batchIndex < batches.size(); ++batchIndex) {
    const GpuCompactBatch &batch = batches[batchIndex];
    // `survivors` é a soma de prefixo exclusiva que o kernel calcula em memória
    // compartilhada. Aqui ela é um contador porque a varredura é sequencial; o
    // resultado — posição e ordem de cada sobrevivente — é idêntico.
    u32 survivors = 0;
    for (u32 offset = 0; offset < batch.commandCount; ++offset) {
      const u32 sourceIndex = batch.firstCommand + offset;
      if (instanceCounts[sourceIndex] == 0) continue;
      outSourceIndices[batch.compactedBase + survivors] = sourceIndex;
      ++survivors;
    }
    outCounts[batchIndex] = survivors;
  }
  return true;
}

} // namespace ae::renderer
