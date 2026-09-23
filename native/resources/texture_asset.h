#pragma once

#include "renderer/authoring_texture.h"
#include "resources/asset_registry.h"
#include "resources/image_decode.h"
#include "resources/texture_profile.h"

#include <atomic>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

inline constexpr u32 TextureAssetCacheSchema = 1;
inline constexpr u32 TextureAssetImporterRevision = 1;

struct TextureImportLimits final {
  ImageDecodeLimits image{};
  u32 projectMaximumDimension = 4096;
  u64 maximumWorkingBytes = 192ull << 20;
  u64 maximumDerivedBytes = 128ull << 20;
  bool valid() const noexcept;
};

struct PreparedTextureImport final {
  renderer::SharedAuthoringTexture texture;
  u32 sourceWidth = 0;
  u32 sourceHeight = 0;
  u32 droppedMipLevels = 0;
  bool sourceHasAlpha = false;
  std::string sourceHash;
  std::string cacheKey;
  std::string diagnostic;
  bool valid() const noexcept;
};

// Operação síncrona e thread-safe; deve rodar no worker do editor. `usageSrgb`
// resolve o modo "pelo uso". As outras interpretações o substituem.
bool prepareTextureImport(std::span<const u8> source,
                          const TextureProfile &settings,
                          bool usageSrgb,
                          u32 samplerFlags,
                          const TextureImportLimits &limits,
                          const std::atomic<bool> *cancel,
                          PreparedTextureImport &out);

std::string textureAssetCacheKey(std::string_view sourceHash,
                                 const TextureProfile &settings,
                                 bool usageSrgb,
                                 u32 samplerFlags,
                                 const TextureImportLimits &limits);
std::string textureAssetCachePath(const AssetGuid &guid);
bool writeTextureAssetCache(const PreparedTextureImport &prepared,
                            std::vector<u8> &out);
bool readTextureAssetCache(std::span<const u8> bytes,
                           std::string_view expectedKey,
                           const TextureImportLimits &limits,
                           PreparedTextureImport &out);

} // namespace ae::resources
