#pragma once

#include "core/base.h"

#include <cstring>
#include <span>
#include <type_traits>

namespace ae::renderer {

inline constexpr u32 MapPackageMagic = 0x504D4541; // AEMP, little-endian
inline constexpr u32 MapPackageMinimumVersion = 1;
inline constexpr u32 MapPackageVersion = 2;
inline constexpr u32 MapPackageHeaderSize = 144;
inline constexpr u32 MapVertexStrideV1 = 72;
inline constexpr u32 MapVertexStride = 48;
inline constexpr u32 InvalidMapTexture = 0xFFFFFFFFu;

// Material flags are shared by the offline glTF cooker and the Vulkan shader.
// Keep alpha masking out of the blended bit: cutout vegetation belongs in the
// depth-writing opaque pass, while glass/water/soft decals remain back-to-front.
inline constexpr u32 MapMaterialBlend = 1u << 0;
inline constexpr u32 MapMaterialNormalMap = 1u << 1;
inline constexpr u32 MapMaterialMetallicRoughnessMap = 1u << 2;
inline constexpr u32 MapMaterialEmissiveMap = 1u << 3;
inline constexpr u32 MapMaterialAlphaMask = 1u << 4;
inline constexpr u32 MapMaterialDoubleSided = 1u << 5;
// Rendering transparency and physical participation are separate concerns.
// Import settings can suppress any surface explicitly or opt an alpha-cutout
// mesh (for example, a gameplay fence) back into collision.
inline constexpr u32 MapMaterialNoCollision = 1u << 6;
inline constexpr u32 MapMaterialForceCollision = 1u << 7;

struct MapTextureRecord {
  u32 flags;
  u32 reserved[3];
};

struct MapMaterialRecord {
  u32 textureIndices[4];
  float baseColorFactor[4];
  float emissiveFactorAndStrength[4];
  float roughness;
  float metallic;
  float normalScale;
  float specular;
  float alphaCutoff;
  u32 flags;
  u32 textureCoordinates;
  u32 reserved;
};

struct MapDrawRecord {
  u32 firstIndex;
  u32 indexCount;
  u32 vertexOffset;
  u32 materialIndex;
  float model[16];
  float boundsCenter[3];
  float boundsRadius;
};

struct MapPackageHeader {
  u32 version = 0;
  u32 vertexStride = 0;
  u32 textureCount = 0;
  u32 materialCount = 0;
  u32 drawCount = 0;
  u32 vertexCount = 0;
  u32 indexCount = 0;
  u32 triangleCount = 0;
  u64 textureOffset = 0;
  u64 materialOffset = 0;
  u64 drawOffset = 0;
  u64 vertexOffset = 0;
  u64 indexOffset = 0;
  float boundsMinimum[3]{};
  float boundsMaximum[3]{};
  float defaultCameraPosition[3]{};
  float defaultCameraYaw = 0.0f;
  float defaultCameraPitch = 0.0f;
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;
};

struct MapPackageView {
  MapPackageHeader header{};
  // FNV-1a over the exact validated package bytes. This is diagnostic
  // identity, not a security checksum; the APK SHA-256 remains the artifact
  // integrity source in the Android runner.
  u64 contentFingerprint = 0;
  std::span<const MapTextureRecord> textures{};
  std::span<const MapMaterialRecord> materials{};
  std::span<const MapDrawRecord> draws{};
  std::span<const u8> vertices{};
  std::span<const u32> indices{};
};

static_assert(std::is_standard_layout_v<MapTextureRecord> && sizeof(MapTextureRecord) == 16);
static_assert(std::is_standard_layout_v<MapMaterialRecord> && sizeof(MapMaterialRecord) == 80);
static_assert(std::is_standard_layout_v<MapDrawRecord> && sizeof(MapDrawRecord) == 96);

inline bool decodeMapPackage(std::span<const u8> bytes, MapPackageView &out) {
  if (bytes.size() < MapPackageHeaderSize) return false;
  auto word = [&](usize offset) {
    return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8 |
           static_cast<u32>(bytes[offset + 2]) << 16 |
           static_cast<u32>(bytes[offset + 3]) << 24;
  };
  auto wide = [&](usize offset) {
    return static_cast<u64>(word(offset)) | static_cast<u64>(word(offset + 4)) << 32;
  };
  auto real = [&](usize offset) {
    const u32 encoded = word(offset);
    float value = 0.0f;
    std::memcpy(&value, &encoded, sizeof(value));
    return value;
  };
  const u32 version = word(4);
  const u32 vertexStride = word(12);
  if (word(0) != MapPackageMagic || version < MapPackageMinimumVersion ||
      version > MapPackageVersion || word(8) != MapPackageHeaderSize ||
      (version == 1 && vertexStride != MapVertexStrideV1) ||
      (version == 2 && vertexStride != MapVertexStride)) return false;

  MapPackageView decoded;
  decoded.header.version = version;
  decoded.header.vertexStride = vertexStride;
  decoded.header.textureCount = word(16);
  decoded.header.materialCount = word(20);
  decoded.header.drawCount = word(24);
  decoded.header.vertexCount = word(28);
  decoded.header.indexCount = word(32);
  decoded.header.textureOffset = wide(40);
  decoded.header.materialOffset = wide(48);
  decoded.header.drawOffset = wide(56);
  decoded.header.vertexOffset = wide(64);
  decoded.header.indexOffset = wide(72);
  for (u32 index = 0; index < 3; ++index) {
    decoded.header.boundsMinimum[index] = real(80 + index * 4);
    decoded.header.boundsMaximum[index] = real(92 + index * 4);
    decoded.header.defaultCameraPosition[index] = real(104 + index * 4);
  }
  decoded.header.defaultCameraYaw = real(116);
  decoded.header.defaultCameraPitch = real(120);
  decoded.header.nearPlane = real(124);
  decoded.header.farPlane = real(128);
  decoded.header.triangleCount = word(132);

  constexpr u32 MaximumTextures = 256;
  constexpr u32 MaximumMaterials = 4096;
  constexpr u32 MaximumDraws = 65536;
  constexpr u32 MaximumVertices = 16 * 1024 * 1024;
  constexpr u32 MaximumIndices = 48 * 1024 * 1024;
  if (decoded.header.textureCount > MaximumTextures || decoded.header.materialCount == 0 ||
      decoded.header.materialCount > MaximumMaterials || decoded.header.drawCount == 0 ||
      decoded.header.drawCount > MaximumDraws || decoded.header.vertexCount == 0 ||
      decoded.header.vertexCount > MaximumVertices || decoded.header.indexCount == 0 ||
      decoded.header.indexCount > MaximumIndices ||
      decoded.header.triangleCount != decoded.header.indexCount / 3 ||
      decoded.header.nearPlane <= 0.0f || decoded.header.farPlane <= decoded.header.nearPlane) {
    return false;
  }

  auto section = [&](u64 offset, u64 count, u64 stride) {
    return offset >= MapPackageHeaderSize && offset <= bytes.size() &&
           count <= (bytes.size() - static_cast<usize>(offset)) / stride;
  };
  if (!section(decoded.header.textureOffset, decoded.header.textureCount, sizeof(MapTextureRecord)) ||
      !section(decoded.header.materialOffset, decoded.header.materialCount, sizeof(MapMaterialRecord)) ||
      !section(decoded.header.drawOffset, decoded.header.drawCount, sizeof(MapDrawRecord)) ||
      !section(decoded.header.vertexOffset, decoded.header.vertexCount, vertexStride) ||
      !section(decoded.header.indexOffset, decoded.header.indexCount, sizeof(u32))) return false;
  if ((decoded.header.textureOffset | decoded.header.materialOffset | decoded.header.drawOffset |
       decoded.header.vertexOffset | decoded.header.indexOffset) % 16 != 0) return false;
  const u64 textureEnd = decoded.header.textureOffset +
                         static_cast<u64>(decoded.header.textureCount) * sizeof(MapTextureRecord);
  const u64 materialEnd = decoded.header.materialOffset +
                          static_cast<u64>(decoded.header.materialCount) * sizeof(MapMaterialRecord);
  const u64 drawEnd = decoded.header.drawOffset +
                      static_cast<u64>(decoded.header.drawCount) * sizeof(MapDrawRecord);
  const u64 vertexEnd = decoded.header.vertexOffset +
                        static_cast<u64>(decoded.header.vertexCount) * vertexStride;
  const u64 indexEnd = decoded.header.indexOffset +
                       static_cast<u64>(decoded.header.indexCount) * sizeof(u32);
  if (textureEnd > decoded.header.materialOffset || materialEnd > decoded.header.drawOffset ||
      drawEnd > decoded.header.vertexOffset || vertexEnd > decoded.header.indexOffset ||
      indexEnd != bytes.size()) return false;

  decoded.textures = {reinterpret_cast<const MapTextureRecord *>(bytes.data() + decoded.header.textureOffset),
                      decoded.header.textureCount};
  decoded.materials = {reinterpret_cast<const MapMaterialRecord *>(bytes.data() + decoded.header.materialOffset),
                       decoded.header.materialCount};
  decoded.draws = {reinterpret_cast<const MapDrawRecord *>(bytes.data() + decoded.header.drawOffset),
                   decoded.header.drawCount};
  decoded.vertices = {bytes.data() + decoded.header.vertexOffset,
                      static_cast<usize>(decoded.header.vertexCount) * vertexStride};
  decoded.indices = {reinterpret_cast<const u32 *>(bytes.data() + decoded.header.indexOffset),
                     decoded.header.indexCount};
  for (const auto &draw : decoded.draws) {
    if (draw.materialIndex >= decoded.header.materialCount || draw.indexCount == 0 ||
        draw.firstIndex > decoded.header.indexCount ||
        draw.indexCount > decoded.header.indexCount - draw.firstIndex) return false;
  }
  for (const auto &material : decoded.materials) {
    for (u32 texture : material.textureIndices) {
      if (texture != InvalidMapTexture && texture >= decoded.header.textureCount) return false;
    }
  }
  u64 fingerprint = 14695981039346656037ull;
  for (u8 byte : bytes) {
    fingerprint ^= byte;
    fingerprint *= 1099511628211ull;
  }
  decoded.contentFingerprint = fingerprint;

  out = decoded;
  return true;
}

} // namespace ae::renderer
