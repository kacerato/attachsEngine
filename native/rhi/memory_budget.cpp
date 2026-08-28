#include "rhi/memory_budget.h"

#include <algorithm>
#include <limits>

namespace ae::rhi {

MemoryBudgetConfig MemoryBudgetConfig::unlimited() {
  MemoryBudgetConfig config{};
  config.limits.fill(std::numeric_limits<u64>::max());
  return config;
}

MemoryBudgetConfig MemoryBudgetConfig::fromTotalBytes(u64 totalBytes) {
  MemoryBudgetConfig config{};
  // Orçamento explícito e conservador para o primeiro vertical slice:
  // texturas dominam; render targets e buffers recebem cotas iguais; staging
  // fica limitado para impedir uploads concorrentes de consumir o heap todo.
  config.limits[static_cast<usize>(MemoryClass::Buffer)] = totalBytes / 5;
  config.limits[static_cast<usize>(MemoryClass::Texture)] = totalBytes / 2;
  config.limits[static_cast<usize>(MemoryClass::RenderTarget)] = totalBytes / 5;
  config.limits[static_cast<usize>(MemoryClass::Staging)] =
      totalBytes - (totalBytes / 5) - (totalBytes / 2) - (totalBytes / 5);
  return config;
}

u64 MemoryBudgetSnapshot::totalUsedBytes() const {
  u64 total = 0;
  for (const MemoryBudgetEntry &entry : entries) total += entry.usedBytes;
  return total;
}

u64 MemoryBudgetSnapshot::totalPeakBytes() const {
  u64 total = 0;
  for (const MemoryBudgetEntry &entry : entries) total += entry.peakBytes;
  return total;
}

MemoryBudgetTracker::MemoryBudgetTracker(const MemoryBudgetConfig &config) {
  configure(config);
}

void MemoryBudgetTracker::configure(const MemoryBudgetConfig &config) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = {};
  for (usize i = 0; i < MemoryClassCount; ++i) state_.entries[i].limitBytes = config.limits[i];
}

usize MemoryBudgetTracker::indexOf(MemoryClass memoryClass) {
  return static_cast<usize>(memoryClass);
}

bool MemoryBudgetTracker::tryReserve(MemoryClass memoryClass, u64 bytes) {
  const usize index = indexOf(memoryClass);
  if (index >= MemoryClassCount || bytes == 0) return false;

  std::lock_guard<std::mutex> lock(mutex_);
  MemoryBudgetEntry &entry = state_.entries[index];
  if (bytes > entry.limitBytes || entry.usedBytes > entry.limitBytes - bytes) return false;
  entry.usedBytes += bytes;
  entry.peakBytes = std::max(entry.peakBytes, entry.usedBytes);
  return true;
}

bool MemoryBudgetTracker::release(MemoryClass memoryClass, u64 bytes) {
  const usize index = indexOf(memoryClass);
  if (index >= MemoryClassCount || bytes == 0) return false;

  std::lock_guard<std::mutex> lock(mutex_);
  MemoryBudgetEntry &entry = state_.entries[index];
  if (bytes > entry.usedBytes) return false;
  entry.usedBytes -= bytes;
  return true;
}

MemoryBudgetSnapshot MemoryBudgetTracker::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

const char *memoryClassName(MemoryClass memoryClass) {
  switch (memoryClass) {
  case MemoryClass::Buffer: return "buffer";
  case MemoryClass::Texture: return "texture";
  case MemoryClass::RenderTarget: return "render-target";
  case MemoryClass::Staging: return "staging";
  case MemoryClass::Count: break;
  }
  return "invalid";
}

} // namespace ae::rhi
