#pragma once

#include "renderer/environment_map.h"

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
