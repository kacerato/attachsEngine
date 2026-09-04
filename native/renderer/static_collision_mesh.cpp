#include "renderer/static_collision_mesh.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace ae::renderer {
namespace {

CollisionVertex transformPosition(const CollisionVertex &local, const float model[16]) {
  // AEMAP matrices follow glTF/OpenGL column-major storage, matching the
  // Vulkan shader's mat4. Collision must consume the exact same world transform.
  return {
      model[0] * local.x + model[4] * local.y + model[8] * local.z + model[12],
      model[1] * local.x + model[5] * local.y + model[9] * local.z + model[13],
      model[2] * local.x + model[6] * local.y + model[10] * local.z + model[14],
  };
}

bool finite(const CollisionVertex &vertex) {
  return std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z);
}

} // namespace

bool buildStaticCollisionMesh(const MapPackageView &package, StaticCollisionMesh &out) {
  StaticCollisionMesh built;
  if (package.header.vertexCount == 0 || package.header.vertexStride < sizeof(CollisionVertex) ||
      package.vertices.size() < static_cast<usize>(package.header.vertexCount) *
                                    package.header.vertexStride ||
      package.materials.empty() || package.draws.empty()) {
    return false;
  }

  // A source vertex may legally be reused by multiple draws with different
  // transforms. A per-draw stamp preserves that case without an unordered_map
  // or a full vertex-count clear for every draw.
  std::vector<u32> remapped(package.header.vertexCount);
  std::vector<u32> stamps(package.header.vertexCount);
  built.vertices.reserve(package.header.vertexCount);
  built.indices.reserve(package.header.indexCount);

  for (usize drawIndex = 0; drawIndex < package.draws.size(); ++drawIndex) {
    const MapDrawRecord &draw = package.draws[drawIndex];
    if (draw.materialIndex >= package.materials.size() || draw.indexCount % 3 != 0 ||
        draw.firstIndex > package.indices.size() ||
        draw.indexCount > package.indices.size() - draw.firstIndex) {
      return false;
    }
    // LOD geometry is a render-only representation of the same physical
    // surface.  Feeding levels 1+ into collision would duplicate triangles,
    // increase build time/memory and make ray/shape queries report stacked
    // copies of one surface.  Level 0 is the importer's untouched source and
    // therefore remains the single authoritative collision representation.
    if (draw.lodLevel != 0) continue;
    const u32 flags = package.materials[draw.materialIndex].flags;
    const bool explicitlyDisabled = (flags & (MapMaterialNoCollision | MapMaterialWater)) != 0;
    const bool nonPhysicalCutout = (flags & MapMaterialAlphaMask) != 0 &&
                                   (flags & MapMaterialForceCollision) == 0;
    if (explicitlyDisabled || nonPhysicalCutout) continue;
    for (float value : draw.model) {
      if (!std::isfinite(value)) return false;
    }

    const u32 stamp = static_cast<u32>(drawIndex) + 1;
    auto remapVertex = [&](u32 encodedIndex, u32 &result) {
      const i64 sourceIndex = static_cast<i64>(encodedIndex) +
                              static_cast<i64>(static_cast<i32>(draw.vertexOffset));
      if (sourceIndex < 0 || sourceIndex >= package.header.vertexCount) return false;
      const u32 source = static_cast<u32>(sourceIndex);
      if (stamps[source] != stamp) {
        if (built.vertices.size() >= std::numeric_limits<u32>::max()) return false;
        CollisionVertex local{};
        std::memcpy(&local,
                    package.vertices.data() + static_cast<usize>(source) *
                                                  package.header.vertexStride,
                    sizeof(local));
        const CollisionVertex world = transformPosition(local, draw.model);
        if (!finite(world)) return false;
        remapped[source] = static_cast<u32>(built.vertices.size());
        stamps[source] = stamp;
        built.vertices.push_back(world);
      }
      result = remapped[source];
      return true;
    };

    const usize end = static_cast<usize>(draw.firstIndex) + draw.indexCount;
    for (usize index = draw.firstIndex; index < end; index += 3) {
      u32 triangle[3]{};
      if (!remapVertex(package.indices[index], triangle[0]) ||
          !remapVertex(package.indices[index + 1], triangle[1]) ||
          !remapVertex(package.indices[index + 2], triangle[2])) {
        return false;
      }
      // glTF and the Jolt integration use opposite handedness/front-face
      // conventions. Reverse once in the asset boundary, never in PhysicsWorld.
      built.indices.push_back(triangle[0]);
      built.indices.push_back(triangle[2]);
      built.indices.push_back(triangle[1]);
    }
  }

  // A renderable scene may intentionally contain no physical triangles (for
  // example an ocean whose interaction is analytic). Empty is a valid built
  // result after every draw and index range above has passed validation.
  out = std::move(built);
  return true;
}

} // namespace ae::renderer
