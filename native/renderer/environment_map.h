#pragma once

#include "core/base.h"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ae::renderer {

inline constexpr u32 EnvironmentResourceMagic = 0x4E454541; // AEEN, little-endian.
inline constexpr u32 EnvironmentResourceCurrentVersion = 4;
inline constexpr usize EnvironmentLightingPayloadBytes = 128;
inline constexpr usize EnvironmentMapDescriptionBytes = 32;
inline constexpr usize DiffuseIrradianceShCoefficientCount = 9;

enum class EnvironmentProjection : u32 {
  Equirectangular = 0,
  Octahedral = 1,
};

enum EnvironmentMapFlags : u32 {
  EnvironmentMapPrefilteredGgx = 1u << 0,
  EnvironmentMapSplitSumBrdf = 1u << 1,
  EnvironmentMapDiffuseIrradianceSh9 = 1u << 2,
};

// Real, orthonormal, y-up spherical harmonics after convolution with the
// Lambertian cosine kernel. Each coefficient is RGB irradiance; W is reserved
// and serialized as zero so the same data can be copied directly into std140.
// Coefficient order is documented by diffuseIrradianceShBasis().
using DiffuseIrradianceSh9 =
    std::array<std::array<float, 4>, DiffuseIrradianceShCoefficientCount>;

// Backend-neutral description serialized after the stable lighting payload in
// AEEN v3/v4. Asset paths remain project/AssetDatabase concerns; this structure
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
  DiffuseIrradianceSh9 diffuseIrradianceSh{};

  bool hasPrefilteredSpecular() const {
    return specularProjection == EnvironmentProjection::Octahedral &&
           specularWidth != 0 && specularHeight != 0 && specularMipLevels != 0 &&
           (flags & EnvironmentMapPrefilteredGgx) != 0;
  }
  bool hasSplitSumBrdf() const {
    return brdfWidth != 0 && brdfHeight != 0 && brdfMipLevels != 0 &&
           (flags & EnvironmentMapSplitSumBrdf) != 0;
  }
  bool hasDiffuseIrradianceSh() const {
    return (flags & EnvironmentMapDiffuseIrradianceSh9) != 0;
  }
};

// Payload linear e independente de backend. Cada texel ocupa quatro halfs
// IEEE-754 e os níveis ficam concatenados do maior para o menor.
struct Rgba16fMipChain final {
  u32 width = 0;
  u32 height = 0;
  u32 levels = 0;
  std::vector<u16> texels;

  u64 expectedHalfCount() const noexcept;
  bool valid() const noexcept { return expectedHalfCount() != 0 && texels.size() == expectedHalfCount(); }
};

struct EnvironmentMapResource final {
  EnvironmentMapDescription description{};
  Rgba16fMipChain panorama;
  Rgba16fMipChain specular;
  Rgba16fMipChain brdf;
  std::string sourceHash;
  std::string cacheKey;

  bool valid() const noexcept;
};
using SharedEnvironmentMap = std::shared_ptr<const EnvironmentMapResource>;

// Reads only the portable map-description trailer. AEEN v1-v3 remain valid
// and decode to the explicit legacy representation, allowing runtime fallback.
bool decodeEnvironmentMapDescription(std::span<const u8> bytes,
                                     EnvironmentMapDescription &description);

// CPU reference used by validation and editor diagnostics. Runtime shading
// evaluates the same polynomial in environment_lighting.glsl.
std::array<float, DiffuseIrradianceShCoefficientCount>
diffuseIrradianceShBasis(const float direction[3]);
bool evaluateDiffuseIrradianceSh(const DiffuseIrradianceSh9 &coefficients,
                                 const float direction[3], float irradiance[3]);

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
