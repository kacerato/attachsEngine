#pragma once

#include "core/base.h"

#include <array>
#include <mutex>

namespace ae::rhi {

// Classes de uso, não tipos Vulkan. A separação permite impor limites por
// subsistema sem vazar VkMemoryPropertyFlags para o restante da engine.
enum class MemoryClass : u32 {
  Buffer = 0,
  Texture,
  RenderTarget,
  Staging,
  Count,
};

constexpr usize MemoryClassCount = static_cast<usize>(MemoryClass::Count);

struct MemoryBudgetConfig {
  std::array<u64, MemoryClassCount> limits{};

  static MemoryBudgetConfig unlimited();
  static MemoryBudgetConfig fromTotalBytes(u64 totalBytes);
};

struct MemoryBudgetEntry {
  u64 limitBytes = 0;
  u64 usedBytes = 0;
  u64 peakBytes = 0;
};

struct MemoryBudgetSnapshot {
  std::array<MemoryBudgetEntry, MemoryClassCount> entries{};

  u64 totalUsedBytes() const;
  u64 totalPeakBytes() const;
};

// Visão completa da memória gráfica. allocationBudget contém somente recursos
// pertencentes à engine e é sempre preciso. Os campos deviceLocal* descrevem o
// heap inteiro visto pelo driver (engine + compositor + outros consumidores) e
// só budget/usage são válidos quando VK_EXT_memory_budget foi habilitada.
// Em Android/UMA o heap device-local pode ser a própria RAM compartilhada;
// unifiedMemory torna essa distinção explícita para o profiler não somar RAM e
// "VRAM" como se fossem dispositivos físicos separados.
struct DeviceMemorySnapshot final {
  MemoryBudgetSnapshot allocationBudget{};
  bool driverBudgetAvailable = false;
  bool unifiedMemory = false;
  u64 deviceLocalHeapBytes = 0;
  u64 deviceLocalBudgetBytes = 0;
  u64 deviceLocalUsageBytes = 0;
};

// Tracker pequeno e independente de Vulkan/VMA. É thread-safe porque IO e
// upload de assets passarão a reservar memória em workers nas próximas fases.
class MemoryBudgetTracker final {
public:
  explicit MemoryBudgetTracker(const MemoryBudgetConfig &config =
                                   MemoryBudgetConfig::unlimited());

  void configure(const MemoryBudgetConfig &config);
  bool tryReserve(MemoryClass memoryClass, u64 bytes);
  bool release(MemoryClass memoryClass, u64 bytes);
  MemoryBudgetSnapshot snapshot() const;

private:
  static usize indexOf(MemoryClass memoryClass);

  mutable std::mutex mutex_;
  MemoryBudgetSnapshot state_{};
};

const char *memoryClassName(MemoryClass memoryClass);

} // namespace ae::rhi
