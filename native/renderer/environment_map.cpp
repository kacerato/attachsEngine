#include "renderer/environment_map.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace ae::renderer {
namespace {
u32 word(std::span<const u8> bytes, usize offset) {
  return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8u |
         static_cast<u32>(bytes[offset + 2]) << 16u |
         static_cast<u32>(bytes[offset + 3]) << 24u;
}

bool finiteNonNegativeHalf(u16 value) {
  return (value & 0x8000u) == 0 && (value & 0x7c00u) != 0x7c00u;
}

bool hashText(std::string_view value) {
  if (value.size() != 64) return false;
  return std::all_of(value.begin(), value.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
}

u64 Rgba16fMipChain::expectedHalfCount() const noexcept {
  if (!width || !height || width > 16384 || height > 16384 || !levels) return 0;
  u32 maximumLevels=1;
  for(u32 size=std::max(width,height);size>1;size/=2)++maximumLevels;
  if(levels>maximumLevels)return 0;
  u64 count = 0;
  u32 w = width, h = height;
  for (u32 level = 0; level < levels; ++level) {
    const u64 pixels = static_cast<u64>(w) * h;
    if (pixels > (std::numeric_limits<u64>::max() - count) / 4u) return 0;
    count += pixels * 4u;
    w = std::max(1u, w / 2u);
    h = std::max(1u, h / 2u);
  }
  return count;
}

bool EnvironmentMapResource::valid() const noexcept {
  for (const auto &coefficient : description.diffuseIrradianceSh)
    for (usize channel = 0; channel < coefficient.size(); ++channel)
      if (!std::isfinite(coefficient[channel]) ||
          (channel == 3 && coefficient[channel] != 0.0f)) return false;
  const auto validHalfChain=[](const Rgba16fMipChain &chain) {
    if(!chain.valid())return false;
    for(usize index=0;index<chain.texels.size();++index)
      if(!finiteNonNegativeHalf(chain.texels[index]) ||
         (index%4==3 && chain.texels[index]!=0x3c00u)) return false;
    return true;
  };
  const auto fullLevels=[](u32 size){u32 levels=1;for(;size>1;size/=2)++levels;return levels;};
  return validHalfChain(panorama) && panorama.width == panorama.height * 2u &&
         panorama.levels==fullLevels(panorama.width) && validHalfChain(specular) &&
         specular.width==specular.height && specular.levels==fullLevels(specular.width) &&
         validHalfChain(brdf) && brdf.width==brdf.height && brdf.levels==1 &&
         hashText(sourceHash) && hashText(cacheKey) &&
         description.specularProjection == EnvironmentProjection::Octahedral &&
         description.specularWidth == specular.width && description.specularHeight == specular.height &&
         description.specularMipLevels == specular.levels && description.brdfWidth == brdf.width &&
         description.brdfHeight == brdf.height && description.brdfMipLevels == brdf.levels &&
         description.hasPrefilteredSpecular() && description.hasSplitSumBrdf() &&
         description.hasDiffuseIrradianceSh();
}

namespace {
float scalar(std::span<const u8> bytes, usize offset) {
  return std::bit_cast<float>(word(bytes, offset));
}
}

bool decodeEnvironmentMapDescription(std::span<const u8> bytes,
                                     EnvironmentMapDescription &description) {
  if (bytes.size() < 16 || word(bytes, 0) != EnvironmentResourceMagic ||
      word(bytes, 8) != bytes.size()) return false;
  const u32 version = word(bytes, 4);
  const usize lightingBytes = version == 1 ? 64 : EnvironmentLightingPayloadBytes;
  const usize expectedBytes = 16 + lightingBytes +
      (version >= 3 ? EnvironmentMapDescriptionBytes : 0) +
      (version >= 4 ? sizeof(DiffuseIrradianceSh9) : 0);
  if (version == 0 || version > EnvironmentResourceCurrentVersion || bytes.size() != expectedBytes)
    return false;

  EnvironmentMapDescription decoded{};
  if (version < 3) {
    description = decoded;
    return true;
  }
  const usize offset = 16 + EnvironmentLightingPayloadBytes;
  const u32 projection = word(bytes, offset);
  if (projection > static_cast<u32>(EnvironmentProjection::Octahedral)) return false;
  decoded.specularProjection = static_cast<EnvironmentProjection>(projection);
  decoded.specularWidth = word(bytes, offset + 4);
  decoded.specularHeight = word(bytes, offset + 8);
  decoded.specularMipLevels = word(bytes, offset + 12);
  decoded.brdfWidth = word(bytes, offset + 16);
  decoded.brdfHeight = word(bytes, offset + 20);
  decoded.brdfMipLevels = word(bytes, offset + 24);
  decoded.flags = word(bytes, offset + 28);
  if (decoded.specularWidth > 4096 || decoded.specularHeight > 4096 ||
      decoded.specularMipLevels > 16 || decoded.brdfWidth > 1024 ||
      decoded.brdfHeight > 1024 || decoded.brdfMipLevels > 16) return false;
  if ((decoded.flags & EnvironmentMapPrefilteredGgx) != 0 &&
      !decoded.hasPrefilteredSpecular()) return false;
  if ((decoded.flags & EnvironmentMapSplitSumBrdf) != 0 && !decoded.hasSplitSumBrdf()) return false;
  if (version >= 4) {
    const usize shOffset = offset + EnvironmentMapDescriptionBytes;
    for (usize coefficient = 0; coefficient < decoded.diffuseIrradianceSh.size(); ++coefficient) {
      for (usize channel = 0; channel < 4; ++channel) {
        const float value = scalar(bytes, shOffset + (coefficient * 4 + channel) * sizeof(float));
        if (!std::isfinite(value) || (channel == 3 && value != 0.0f)) return false;
        decoded.diffuseIrradianceSh[coefficient][channel] = value;
      }
    }
  } else {
    decoded.flags &= ~EnvironmentMapDiffuseIrradianceSh9;
  }
  description = decoded;
  return true;
}

std::array<float, DiffuseIrradianceShCoefficientCount>
diffuseIrradianceShBasis(const float direction[3]) {
  if (!direction) return {};
  const float lengthSquared = direction[0] * direction[0] + direction[1] * direction[1] +
                              direction[2] * direction[2];
  if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f) return {};
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  const float x = direction[0] * inverseLength;
  const float y = direction[1] * inverseLength;
  const float z = direction[2] * inverseLength;
  return {0.2820947918f, 0.4886025119f * y, 0.4886025119f * z,
          0.4886025119f * x, 1.0925484306f * x * y,
          1.0925484306f * y * z, 0.3153915653f * (3.0f * y * y - 1.0f),
          1.0925484306f * x * z, 0.5462742153f * (x * x - z * z)};
}

bool evaluateDiffuseIrradianceSh(const DiffuseIrradianceSh9 &coefficients,
                                 const float direction[3], float irradiance[3]) {
  if (!direction || !irradiance) return false;
  const auto basis = diffuseIrradianceShBasis(direction);
  const bool validDirection = std::isfinite(direction[0]) && std::isfinite(direction[1]) &&
      std::isfinite(direction[2]) &&
      direction[0] * direction[0] + direction[1] * direction[1] +
          direction[2] * direction[2] > 1.0e-12f;
  if (!validDirection) return false;
  for (usize channel = 0; channel < 3; ++channel) {
    double value = 0.0;
    for (usize coefficient = 0; coefficient < coefficients.size(); ++coefficient) {
      if (!std::isfinite(coefficients[coefficient][channel])) return false;
      value += static_cast<double>(coefficients[coefficient][channel]) * basis[coefficient];
    }
    if (!std::isfinite(value)) return false;
    irradiance[channel] = static_cast<float>(value);
  }
  return true;
}

} // namespace ae::renderer
