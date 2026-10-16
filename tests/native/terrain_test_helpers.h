#pragma once
#include "core/sha256.h"
#include "resources/terrain_asset.h"
namespace ae::test {
inline std::string terrainDigest(const resources::TerrainMesh &mesh) {
  std::vector<u8> bytes;
  for (const auto &p : mesh.parts) {
    auto *b = reinterpret_cast<const u8 *>(p.vertices.data());
    bytes.insert(bytes.end(), b,
                 b + p.vertices.size() * sizeof(resources::TerrainVertex));
  }
  return Sha256::hex(bytes);
}
} // namespace ae::test
