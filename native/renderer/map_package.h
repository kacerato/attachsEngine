#pragma once

#include "core/base.h"

#include <cmath>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace ae::renderer {

inline constexpr u32 MapPackageMagic = 0x504D4541; // AEMP, little-endian
inline constexpr u32 MapPackageMinimumVersion = 1;
inline constexpr u32 MapPackageVersion = 3;
inline constexpr u32 MapPackageHeaderSize = 144;
inline constexpr u32 MapVertexStrideV1 = 72;
inline constexpr u32 MapVertexStride = 48;
// v1/v2 share the 96-byte MapDrawRecord layout (no LOD fields); v3 adds
// lodLevel/geometricError/lodGroupId (108 bytes). Decoding must never
// reinterpret_cast the file's bytes directly onto the current in-memory
// struct across this boundary -- see decodeMapPackage's per-version unpack.
inline constexpr u32 MapDrawRecordStrideV1V2 = 96;
inline constexpr u32 MapDrawRecordStride = 108;
// Tres niveis de simplificacao geometrica saem do cooker (MAX_LOD_LEVELS em
// tools/cook-gltf-map.py) e o quarto slot fica reservado ao impostor assado que
// tools/bake-foliage-impostors.py acrescenta depois, como nivel mais grosseiro
// do grupo. Sao limites diferentes de proposito: o cooker nao sabe assar um
// impostor e o baker nao simplifica malha; quem os junta e a cadeia de LOD.
inline constexpr u32 MapMaximumLodLevels = 4;
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
// Impostor de folhagem distante: um quad assado offline que substitui centenas
// de cards alfa como ultimo nivel da cadeia de LOD do grupo. O vertice e local e
// centrado na origem, e o shader o gira em torno de Y para encarar a camera --
// sem isso o quad so ficaria correto visto da direcao em que foi assado.
// Ver tools/bake-foliage-impostors.py.
inline constexpr u32 MapMaterialImpostor = 1u << 8;

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
  // Optional multiview impostor layout (zero = legacy single view):
  // macro columns/rows log2 in bits 0..7, view columns/rows in bits 8..15.
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
  // v3+ only. v1/v2 packages decode to lodLevel=0, geometricError=0.0,
  // lodGroupId=<this draw's index> -- "level 0, no LOD" for legacy content,
  // which renderer::computeScreenSpaceError always selects (see
  // renderer/lod_selection.h). lodGroupId ties the N discrete levels of one
  // spatial chunk together; it is NOT a material or draw index.
  u32 lodLevel;
  float geometricError;
  u32 lodGroupId;
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
  // Owned (not a zero-copy span): v1/v2 draws are 96 bytes on disk, v3 draws
  // are 108. decodeMapPackage unpacks either into this struct's CURRENT
  // (v3-shaped) layout explicitly, one field at a time -- a raw
  // reinterpret_cast of file bytes onto MapDrawRecord would silently
  // misread every legacy package the moment the struct grew. See
  // decodeMapPackage's per-version unpack loop.
  std::vector<MapDrawRecord> draws;
  std::span<const u8> vertices{};
  std::span<const u32> indices{};
};

static_assert(std::is_standard_layout_v<MapTextureRecord> && sizeof(MapTextureRecord) == 16);
static_assert(std::is_standard_layout_v<MapMaterialRecord> && sizeof(MapMaterialRecord) == 80);
static_assert(std::is_standard_layout_v<MapDrawRecord> && sizeof(MapDrawRecord) == 108);

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
  // v3 keeps v2's packed vertex layout; only MapDrawRecord's on-disk stride
  // changes at v3 (see drawRecordStride below).
  if (word(0) != MapPackageMagic || version < MapPackageMinimumVersion ||
      version > MapPackageVersion || word(8) != MapPackageHeaderSize ||
      (version == 1 && vertexStride != MapVertexStrideV1) ||
      ((version == 2 || version == 3) && vertexStride != MapVertexStride)) return false;
  const u32 drawRecordStride = version >= 3 ? MapDrawRecordStride : MapDrawRecordStrideV1V2;

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
      !section(decoded.header.drawOffset, decoded.header.drawCount, drawRecordStride) ||
      !section(decoded.header.vertexOffset, decoded.header.vertexCount, vertexStride) ||
      !section(decoded.header.indexOffset, decoded.header.indexCount, sizeof(u32))) return false;
  if ((decoded.header.textureOffset | decoded.header.materialOffset | decoded.header.drawOffset |
       decoded.header.vertexOffset | decoded.header.indexOffset) % 16 != 0) return false;
  const u64 textureEnd = decoded.header.textureOffset +
                         static_cast<u64>(decoded.header.textureCount) * sizeof(MapTextureRecord);
  const u64 materialEnd = decoded.header.materialOffset +
                          static_cast<u64>(decoded.header.materialCount) * sizeof(MapMaterialRecord);
  const u64 drawEnd = decoded.header.drawOffset +
                      static_cast<u64>(decoded.header.drawCount) * drawRecordStride;
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
  // Never reinterpret_cast this section: v1/v2 files are 96 bytes/record,
  // v3 is 108. Unpacking one field at a time is what lets a v1/v2 package
  // stay readable forever even as MapDrawRecord keeps growing.
  decoded.draws.clear();
  decoded.draws.reserve(decoded.header.drawCount);
  for (u32 drawIndex = 0; drawIndex < decoded.header.drawCount; ++drawIndex) {
    const usize base =
        static_cast<usize>(decoded.header.drawOffset) + static_cast<usize>(drawIndex) * drawRecordStride;
    MapDrawRecord record{};
    record.firstIndex = word(base + 0);
    record.indexCount = word(base + 4);
    record.vertexOffset = word(base + 8);
    record.materialIndex = word(base + 12);
    for (u32 i = 0; i < 16; ++i) record.model[i] = real(base + 16 + i * 4);
    for (u32 i = 0; i < 3; ++i) record.boundsCenter[i] = real(base + 80 + i * 4);
    record.boundsRadius = real(base + 92);
    if (version >= 3) {
      record.lodLevel = word(base + 96);
      record.geometricError = real(base + 100);
      record.lodGroupId = word(base + 104);
    } else {
      // "Level 0, no LOD" for legacy content: computeScreenSpaceError always
      // selects level 0 when geometricError is 0, and a group of size one
      // never collides with a real multi-level group from a v3 package.
      record.lodLevel = 0;
      record.geometricError = 0.0f;
      record.lodGroupId = drawIndex;
    }
    decoded.draws.push_back(record);
  }
  decoded.vertices = {bytes.data() + decoded.header.vertexOffset,
                      static_cast<usize>(decoded.header.vertexCount) * vertexStride};
  decoded.indices = {reinterpret_cast<const u32 *>(bytes.data() + decoded.header.indexOffset),
                     decoded.header.indexCount};
  for (const auto &draw : decoded.draws) {
    if (draw.materialIndex >= decoded.header.materialCount || draw.indexCount == 0 ||
        draw.firstIndex > decoded.header.indexCount ||
        draw.indexCount > decoded.header.indexCount - draw.firstIndex ||
        !std::isfinite(draw.geometricError) || draw.geometricError < 0.0f ||
        draw.lodLevel >= MapMaximumLodLevels ||
        draw.lodGroupId >= decoded.header.drawCount ||
        (draw.lodLevel == 0 && draw.geometricError != 0.0f)) return false;
  }
  for (const auto &material : decoded.materials) {
    for (u32 texture : material.textureIndices) {
      if (texture != InvalidMapTexture && texture >= decoded.header.textureCount) return false;
    }
    if ((material.flags & MapMaterialImpostor) != 0 && material.reserved != 0) {
      const u32 columns = (material.reserved >> 8u) & 15u;
      const u32 rows = (material.reserved >> 12u) & 15u;
      // Zero dimensions would divide by zero in atlas addressing. Require the
      // replacement-normal resource and supported power-of-two layout at import,
      // not after malformed metadata has reached a fragment shader.
      if ((material.reserved >> 16u) != 0 || columns == 0 || rows == 0 ||
          (columns & (columns - 1u)) != 0 || (rows & (rows - 1u)) != 0 ||
          (material.flags & MapMaterialNormalMap) == 0 ||
          material.textureIndices[0] == InvalidMapTexture ||
          material.textureIndices[1] == InvalidMapTexture) return false;
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
