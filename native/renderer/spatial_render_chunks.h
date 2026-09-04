#pragma once

#include "renderer/map_package.h"

#include <vector>

namespace ae::renderer {

struct SpatialRenderChunkSettings final {
  // Opaque surfaces usually have useful early-Z and tolerate larger bounds.
  // Alpha-tested vegetation is dominated by overdraw, so it has an independent
  // global budget. Hardware A/B must justify lowering it: 2,048 created 125
  // chunks on the reference map without reducing average GPU time, therefore
  // the production default remains equal to opaque instead of guessing.
  u32 opaqueTrianglesPerChunk = 8192;
  u32 coverageTrianglesPerChunk = 8192;
  // Maximum shader-driven motion outside the source mesh. Kept in the
  // backend-neutral build settings so animated surfaces remain cullable
  // without encoding a scene-specific constant in this module.
  float waterDisplacementAllowance = 0.0f;
};

struct SpatialRenderChunks final {
  std::vector<MapDrawRecord> draws;
  std::vector<u32> indices;
};

// Reorders triangle-list indices into deterministic spatial chunks while
// retaining the original vertex buffer, transforms, materials and triangle
// data. Opaque/cutout ordering is visually irrelevant; blended draws remain
// intact because their primitive order can participate in alpha composition.
bool buildSpatialRenderChunks(const MapPackageView &package,
                              const SpatialRenderChunkSettings &settings,
                              SpatialRenderChunks &out);

} // namespace ae::renderer
