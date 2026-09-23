#pragma once

#include "renderer/normal_matrix.h"

#include <cstring>
#include <type_traits>

namespace ae::renderer {

// GPU-facing instance layout for meshes that need a correct tangent basis.
// It is deliberately separate from the stable managed RenderInstance ABI:
// extraction remains compact/stable while the renderer can prepare derived
// data once per dirty transform instead of recomputing an inverse per vertex.
struct alignas(16) GpuMeshInstance final {
  float model[16];
  float tint[4];
  float normalColumns[12];
  // Previous authored transform for object motion. This is transient frame
  // state; scene files and the managed RenderInstance ABI remain unchanged.
  float previousModel[16];
};

static_assert(std::is_standard_layout_v<GpuMeshInstance>);
static_assert(sizeof(GpuMeshInstance) == 192);
static_assert(offsetof(GpuMeshInstance, tint) == 64);
static_assert(offsetof(GpuMeshInstance, normalColumns) == 80);
static_assert(offsetof(GpuMeshInstance, previousModel) == 128);

// Atomic conversion: invalid/singular transforms never leave a partly updated
// GPU record. This function belongs to the renderer and is reusable by every
// scene, importer and backend consumer; it contains no scene-specific policy.
inline bool buildGpuMeshInstance(const float *model, const float *tint,
                                 GpuMeshInstance *out) noexcept {
  if (model == nullptr || tint == nullptr || out == nullptr) return false;
  GpuMeshInstance prepared{};
  std::memcpy(prepared.model, model, sizeof(prepared.model));
  std::memcpy(prepared.previousModel, model, sizeof(prepared.previousModel));
  std::memcpy(prepared.tint, tint, sizeof(prepared.tint));
  if (!buildNormalMatrix(prepared.model, prepared.normalColumns)) return false;
  *out = prepared;
  return true;
}

} // namespace ae::renderer
