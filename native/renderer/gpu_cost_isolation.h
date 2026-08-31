#pragma once

#include "core/base.h"

namespace ae::renderer {

// Diagnostic-only switches for controlled GPU attribution. These modes never
// participate in project quality presets and Full remains the runtime default.
// They deliberately preserve geometry, depth and alpha coverage so comparisons
// isolate shading work instead of silently changing scene complexity.
enum class GpuCostIsolation : u32 {
  Full = 0,
  NoNormalMap = 1,
  NoSpecularEnvironment = 2,
  BaseColorOnly = 3,
  Count = 4,
};

inline constexpr GpuCostIsolation sanitizeGpuCostIsolation(u32 value) noexcept {
  return value < static_cast<u32>(GpuCostIsolation::Count)
             ? static_cast<GpuCostIsolation>(value)
             : GpuCostIsolation::Full;
}

inline constexpr const char *gpuCostIsolationName(GpuCostIsolation mode) noexcept {
  switch (mode) {
    case GpuCostIsolation::Full: return "full";
    case GpuCostIsolation::NoNormalMap: return "no-normal";
    case GpuCostIsolation::NoSpecularEnvironment: return "no-ibl";
    case GpuCostIsolation::BaseColorOnly: return "base-color";
    case GpuCostIsolation::Count: break;
  }
  return "full";
}

} // namespace ae::renderer
