#include "resources/texture_budget.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace ae::resources {
namespace {
u64 topLevelBytes(const renderer::AuthoringTexture &texture) {
  if (texture.format == renderer::AuthoringTextureAstc4x4)
    return static_cast<u64>((texture.width + 3) / 4) * ((texture.height + 3) / 4) * 16;
  return static_cast<u64>(texture.width) * texture.height * 4;
}

bool reducible(const renderer::AuthoringTexture &texture, u32 minimumDimension) {
  return texture.levels > 1 && std::min(texture.width, texture.height) / 2 >= std::max<u32>(minimumDimension, 1);
}
} // namespace

TextureBudgetReport applyTextureBudget(std::vector<renderer::SharedAuthoringTexture> &textures, u64 budgetBytes,
                                       u32 minimumDimension) {
  TextureBudgetReport report;
  report.budgetBytes = budgetBytes;
  // Uma entrada por textura distinta; `working` guarda a versão corrente dela.
  struct Entry {
    const renderer::AuthoringTexture *original;
    std::shared_ptr<renderer::AuthoringTexture> working;
  };
  std::vector<Entry> entries;
  std::unordered_map<const renderer::AuthoringTexture *, usize> byPointer;
  for (const auto &texture : textures) {
    if (!texture || !texture->valid() || byPointer.contains(texture.get())) continue;
    byPointer.emplace(texture.get(), entries.size());
    entries.push_back({texture.get(), nullptr});
    report.requestedBytes += texture->mipChain.size();
  }
  report.textures = static_cast<u32>(entries.size());
  report.residentBytes = report.requestedBytes;
  const auto current = [](const Entry &entry) -> const renderer::AuthoringTexture & {
    return entry.working ? *entry.working : *entry.original;
  };
  while (report.residentBytes > budgetBytes) {
    Entry *largest = nullptr;
    u64 largestBytes = 0;
    for (auto &entry : entries) {
      const auto &texture = current(entry);
      if (!reducible(texture, minimumDimension)) continue;
      const auto bytes = topLevelBytes(texture);
      if (bytes > largestBytes) {
        largest = &entry;
        largestBytes = bytes;
      }
    }
    if (!largest) break; // todas no piso: o relatório diz que não coube
    const auto &texture = current(*largest);
    auto next = std::make_shared<renderer::AuthoringTexture>();
    next->width = std::max<u32>(texture.width / 2, 1);
    next->height = std::max<u32>(texture.height / 2, 1);
    next->levels = texture.levels - 1;
    next->srgb = texture.srgb;
    next->samplerFlags = texture.samplerFlags;
    next->format = texture.format;
    next->mipChain.assign(texture.mipChain.begin() + static_cast<std::ptrdiff_t>(largestBytes), texture.mipChain.end());
    if (!next->valid()) break; // cadeia fora do layout esperado: não arrisca
    if (!largest->working) ++report.reducedTextures;
    ++report.droppedLevels;
    report.residentBytes -= largestBytes;
    largest->working = std::move(next);
  }
  for (auto &texture : textures) {
    if (!texture) continue;
    const auto found = byPointer.find(texture.get());
    if (found == byPointer.end()) continue;
    const auto &entry = entries[found->second];
    if (entry.working) texture = entry.working;
  }
  return report;
}
} // namespace ae::resources
