#include "renderer/environment_map.h"

namespace ae::renderer {
namespace {
u32 word(std::span<const u8> bytes, usize offset) {
  return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8u |
         static_cast<u32>(bytes[offset + 2]) << 16u |
         static_cast<u32>(bytes[offset + 3]) << 24u;
}
}

bool decodeEnvironmentMapDescription(std::span<const u8> bytes,
                                     EnvironmentMapDescription &description) {
  if (bytes.size() < 16 || word(bytes, 0) != EnvironmentResourceMagic ||
      word(bytes, 8) != bytes.size()) return false;
  const u32 version = word(bytes, 4);
  const usize lightingBytes = version == 1 ? 64 : EnvironmentLightingPayloadBytes;
  const usize expectedBytes = 16 + lightingBytes + (version >= 3 ? 32 : 0);
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
  description = decoded;
  return true;
}

} // namespace ae::renderer
