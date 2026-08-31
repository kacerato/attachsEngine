#pragma once

#include "core/base.h"
#include "renderer/map_package.h"

#include <vector>

namespace ae::renderer {

struct CollisionVertex {
  float x=0.0f,y=0.0f,z=0.0f;
};

// CPU-side handoff produced by the import/runtime asset layer and consumed once
// by PhysicsWorld. It deliberately contains no Vulkan or Jolt types.
struct StaticCollisionMesh {
  std::vector<CollisionVertex> vertices;
  std::vector<u32> indices;
  bool empty() const { return vertices.empty()||indices.empty(); }
  void clear(){vertices.clear();indices.clear();vertices.shrink_to_fit();indices.shrink_to_fit();}
};

// Converts cooked render geometry to an independent, compact, world-space
// collision resource. Keeping this conversion renderer/platform agnostic lets
// every scene importer feed the same PhysicsWorld contract; Android is only a
// consumer. Blend mode alone never disables physics: roads, glass and decals
// may be real surfaces. Cutout cards default to non-physical unless import
// metadata explicitly marks them with MapMaterialForceCollision.
bool buildStaticCollisionMesh(const MapPackageView &package, StaticCollisionMesh &out);

} // namespace ae::renderer
