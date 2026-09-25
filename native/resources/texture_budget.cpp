#include "resources/texture_budget.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace ae::resources {
namespace {
u64 topLevelBytes(const renderer::AuthoringTexture &texture) {
  return renderer::authoringTextureLevelBytes(texture.format, texture.width, texture.height);
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
    // Cadeia inteira, esteja o topo na memória ou no derivado em disco (bloco C):
    // é o que a textura ocupa residente por inteiro.
    report.requestedBytes += texture->expectedBytes();
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
    if (texture.firstLevel) {
      // O nível cortado está no arquivo: a cauda na memória não muda, e a
      // fonte passa a começar no nível seguinte.
      auto file = std::make_shared<renderer::AuthoringTextureFile>(*texture.file);
      file->offset += largestBytes;
      next->file = std::move(file);
      next->firstLevel = texture.firstLevel - 1;
      next->mipChain = texture.mipChain;
    } else
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
