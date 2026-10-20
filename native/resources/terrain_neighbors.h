#pragma once
#include "resources/terrain_asset.h"

namespace ae::resources {
// Persist these identities with the owning scene/asset; zero is invalid.
// World axes follow Unity 6000.0 Terrain.SetNeighbors (top = +Z).
// References (capability research, not code copied from Unity):
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Terrain.SetNeighbors.html
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainLayer.html
using TerrainIdentity = u64;
struct TerrainNeighborMember {
  TerrainIdentity group = 0, member = 0;
  double x = 0, y = 0, z = 0;
  std::string sourceAsset;
  std::vector<TerrainIdentity> layers;
};
struct TerrainNeighborDescriptor {
  std::vector<TerrainNeighborMember> members;
};
// Versioned metadata only; limits: 4096 members, 256 layers/member,
// 1024-byte source identities and 4 MiB serialized input. Atomic on failure.
bool serializeTerrainNeighbors(const TerrainNeighborDescriptor &, std::string &,
                               std::string &error);
bool deserializeTerrainNeighbors(std::string_view, TerrainNeighborDescriptor &,
                                 std::string &error);
struct TerrainNeighborTile {
  TerrainIdentity group = 0, member = 0;
  double x = 0, y = 0, z = 0;
  TerrainField field;
  std::vector<TerrainIdentity> layers;
  // Sample-major normalized layer weights, width * depth * layers.size().
  std::vector<float> weights;
};
// Binds an already loaded source field to its persisted member metadata.
// Requires exact identity-weight dimensional agreement and validates the grid.
bool bindTerrainNeighbor(const TerrainNeighborMember &, const TerrainField &,
                         std::span<const float> weights, TerrainNeighborTile &,
                         std::string &error);
enum class TerrainNeighborSide : u32 { Left, Top, Right, Bottom };
struct TerrainNeighborLink {
  TerrainIdentity first = 0, second = 0;
  TerrainNeighborSide side = TerrainNeighborSide::Right;
};
// Full-edge aligned heightfields only. Partial overlaps, duplicate identities,
// non-finite samples, incompatible sizes/resolutions and ambiguous edges fail.
// Output is unchanged on failure. Tolerance is in world units and must be
// smaller than one quarter of every sample spacing.
bool terrainNeighborLinks(std::span<const TerrainNeighborTile>,
                          std::vector<TerrainNeighborLink> &, std::string &,
                          double tolerance = 1e-4);
// Atomic reconciliation: layer identities become a sorted union per connected
// component; weights follow identities. Shared boundary heights (world Y) and
// weights are averaged once over all coincident linked samples, including
// four-tile corners. Interiors are preserved. Tiles retain their stable IDs.
bool reconcileTerrainNeighbors(std::vector<TerrainNeighborTile> &,
                               std::string &, double tolerance = 1e-4);
} // namespace ae::resources
