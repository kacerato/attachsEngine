#pragma once
#include "core/base.h"
#include <cstddef>
#include <type_traits>

namespace ae::renderer {
// Aether.Rendering.RenderInstance ABI v1. Column-major affine matrix;
// entity is a transient index/generation pair, not a serialized object ID.
struct RenderInstance {
  float model[16];
  float tint[4];
  u32 entityIndex;
  u32 entityGeneration;
};
inline constexpr int RenderInstanceAbiVersion = 1;
static_assert(std::is_standard_layout_v<RenderInstance>);
static_assert(sizeof(RenderInstance) == 88);
static_assert(offsetof(RenderInstance, tint) == 64);
static_assert(offsetof(RenderInstance, entityIndex) == 80);
} // namespace ae::renderer
