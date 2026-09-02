#pragma once

#include "core/base.h"

#include <span>

namespace ae::renderer {

inline constexpr u32 EnvironmentResourceMagic = 0x4E454541; // AEEN, little-endian.
inline constexpr u32 EnvironmentResourceCurrentVersion = 3;
inline constexpr usize EnvironmentLightingPayloadBytes = 128;

enum class EnvironmentProjection : u32 {
  Equirectangular = 0,
  Octahedral = 1,
};

enum EnvironmentMapFlags : u32 {
  EnvironmentMapPrefilteredGgx = 1u << 0,
  EnvironmentMapSplitSumBrdf = 1u << 1,
};

// Backend-neutral description serialized after the stable lighting payload in
// AEEN v3. Asset paths remain project/AssetDatabase concerns; this structure
// describes how the renderer must interpret the referenced cooked textures.
struct EnvironmentMapDescription final {
  EnvironmentProjection specularProjection = EnvironmentProjection::Equirectangular;
  u32 specularWidth = 0;
  u32 specularHeight = 0;
  u32 specularMipLevels = 0;
  u32 brdfWidth = 0;
  u32 brdfHeight = 0;
  u32 brdfMipLevels = 0;
  u32 flags = 0;

  bool hasPrefilteredSpecular() const {
    return specularProjection == EnvironmentProjection::Octahedral &&
           specularWidth != 0 && specularHeight != 0 && specularMipLevels != 0 &&
           (flags & EnvironmentMapPrefilteredGgx) != 0;
  }
  bool hasSplitSumBrdf() const {
    return brdfWidth != 0 && brdfHeight != 0 && brdfMipLevels != 0 &&
           (flags & EnvironmentMapSplitSumBrdf) != 0;
  }
};

// Reads only the portable map-description trailer. AEEN v1/v2 remain valid
// and decode to the explicit legacy representation, allowing runtime fallback.
bool decodeEnvironmentMapDescription(std::span<const u8> bytes,
                                     EnvironmentMapDescription &description);

// Specialization variants cover only texture-backed material capabilities.
// Alpha mode, sidedness and collision participation belong to other pipeline
// dimensions and deliberately do not inflate this compact 3-bit key.
inline constexpr u32 MaterialFeatureVariantCount = 8;
inline constexpr u32 DynamicMaterialFeatureMask = 0xffffffffu;

inline constexpr u32 materialFeatureVariant(u32 materialFlags) {
  return ((materialFlags >> 1u) & 1u) | ((materialFlags >> 2u) & 1u) << 1u |
         ((materialFlags >> 3u) & 1u) << 2u;
}

inline constexpr u32 materialFeatureMaskForVariant(u32 variant) {
  return ((variant & 1u) << 1u) | ((variant & 2u) << 1u) | ((variant & 4u) << 1u);
}

} // namespace ae::renderer
