#pragma once

#include "renderer/environment_map.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

inline constexpr u32 EnvironmentMapCacheSchema = 1;
inline constexpr u32 EnvironmentMapImporterRevision = 1;

struct EnvironmentMapImportSettings final {
  u32 panoramaWidth = 1024;
  u32 specularSize = 256;
  u32 brdfSize = 128;
  u32 specularSamples = 128;
  u32 brdfSamples = 512;
  bool valid() const noexcept;
};

// Degraus que o editor oferece para a receita — o painel de importação e o
// mapa HDRI em Propriedades usam os mesmos, para uma receita escolhida num
// nunca aparecer fora da faixa do outro. Todos passam em `valid()`.
inline constexpr std::array<u32, 4> EnvironmentPanoramaSteps{256, 512, 1024, 2048};
inline constexpr std::array<u32, 4> EnvironmentSpecularSizeSteps{64, 128, 256, 512};
inline constexpr std::array<u32, 3> EnvironmentBrdfSizeSteps{64, 128, 256};
inline constexpr std::array<u32, 4> EnvironmentSpecularSampleSteps{32, 64, 128, 256};
inline constexpr std::array<u32, 4> EnvironmentBrdfSampleSteps{128, 256, 512, 1024};
// Próximo degrau acima/abaixo de `value`; um valor fora da lista (receita
// antiga) cai no degrau mais próximo na direção pedida.
inline u32 stepEnvironmentMapChoice(u32 value, std::span<const u32> steps, bool up) noexcept {
  if (steps.empty()) return value;
  usize index = 0;
  while (index + 1 < steps.size() && steps[index] < value) ++index;
  if (up) {
    if (steps[index] <= value && index + 1 < steps.size()) ++index;
  } else if (steps[index] >= value && index > 0) --index;
  return steps[index];
}

struct EnvironmentMapImportLimits final {
  u64 maximumSourceBytes = 128ull << 20;
  u32 maximumSourceDimension = 8192;
  u64 maximumSourcePixels = 32ull * 1024 * 1024;
  u64 maximumDecodedBytes = 128ull << 20;
  u64 maximumWorkingBytes = 128ull << 20;
  u64 maximumOutputBytes = 128ull << 20;
  bool valid() const noexcept;
};

struct EnvironmentMapCancel final {
  bool (*requested)(void *) = nullptr;
  void *context = nullptr;
  bool cancelled() const noexcept { return requested && requested(context); }
};

// Operação síncrona e thread-safe. O editor deve executá-la no worker de
// importação; o callback permite interromper reimportações substituídas.
bool importRadianceEnvironmentMap(std::span<const u8> source,
                                  const EnvironmentMapImportSettings &settings,
                                  const EnvironmentMapImportLimits &limits,
                                  EnvironmentMapCancel cancel,
                                  renderer::SharedEnvironmentMap &out,
                                  std::string &diagnostic);

std::string environmentMapCacheKey(std::string_view sourceHash,
                                   const EnvironmentMapImportSettings &settings,
                                   const EnvironmentMapImportLimits &limits);
std::string environmentMapCacheRelativePath(std::string_view key);
std::string writeEnvironmentMapImportSettings(const EnvironmentMapImportSettings &settings);
bool readEnvironmentMapImportSettings(std::string_view text,EnvironmentMapImportSettings &settings);
bool writeEnvironmentMapCache(const renderer::EnvironmentMapResource &resource,
                              std::vector<u8> &out);
bool readEnvironmentMapCache(std::span<const u8> bytes, std::string_view expectedKey,
                             const EnvironmentMapImportLimits &limits,
                             renderer::SharedEnvironmentMap &out);

} // namespace ae::resources
