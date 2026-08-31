#pragma once

#include "renderer/map_package.h"

#include <vector>

namespace ae::renderer {

struct SpatialRenderChunkSettings final {
  // Large enough to keep CPU submission bounded on mobile, small enough to
  // separate material primitives that span a complete outdoor level.
  u32 targetTrianglesPerChunk = 8192;
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
