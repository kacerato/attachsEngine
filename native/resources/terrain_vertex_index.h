#pragma once
#include "resources/terrain_asset.h"
#include <cstring>
#include <unordered_map>

namespace ae::resources {
// Lossless sharing across authoring preview and GLB publication. Do not weld
// by position: coincident vertices may have distinct normals, UVs or weights.
inline void indexTerrainVertices(std::span<const TerrainVertex> source,
                                 std::vector<TerrainVertex> &vertices,
                                 std::vector<u32> &indices) {
  static_assert(sizeof(TerrainVertex) == 14 * sizeof(u32));
  using Key = std::array<u32, 14>;
  struct Hash {
    usize operator()(const Key &key) const noexcept {
      usize hash = 1469598103934665603ull;
      for (u32 word : key) { hash ^= word; hash *= 1099511628211ull; }
      return hash;
    }
  };
  std::unordered_map<Key, u32, Hash> lookup;
  lookup.reserve(source.size());
  vertices.clear(); indices.clear(); indices.reserve(source.size());
  for (const auto &vertex : source) {
    Key key{}; std::memcpy(key.data(), &vertex, sizeof(vertex));
    const auto [found, inserted] = lookup.emplace(key, u32(vertices.size()));
    if (inserted) vertices.push_back(vertex);
    indices.push_back(found->second);
  }
}
} // namespace ae::resources
