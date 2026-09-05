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

// Attribution for the water surface specifically. A tile-deferred GPU collapses
// per-subpass timestamps, so the only honest way to learn what the water
// fragment costs is to remove one term at a time and difference interleaved
// runs. Geometry, depth and blending are preserved in every mode except
// SkipDraw, so a difference is shading and not scene complexity.
enum class WaterCostIsolation : u32 {
  Full = 0,
  NoShadow = 1,             // skip the cascade lookup on the water surface
  NoSpectralDetail = 2,     // skip the per-cascade slope fetches in the fragment
  NoMicroNormal = 3,        // skip the two tiling normal-map fetches
  NoReflection = 4,         // skip environment radiance and the BRDF lookup
  Flat = 5,                 // constant colour: geometry and blending only
  SkipDraw = 6,             // the water is not submitted at all
  Count = 7,
};

inline constexpr WaterCostIsolation sanitizeWaterCostIsolation(u32 value) noexcept {
  return value < static_cast<u32>(WaterCostIsolation::Count)
             ? static_cast<WaterCostIsolation>(value)
             : WaterCostIsolation::Full;
}

inline constexpr const char *waterCostIsolationName(WaterCostIsolation mode) noexcept {
  switch (mode) {
    case WaterCostIsolation::Full: return "full";
    case WaterCostIsolation::NoShadow: return "no-shadow";
    case WaterCostIsolation::NoSpectralDetail: return "no-spectral-detail";
    case WaterCostIsolation::NoMicroNormal: return "no-micro-normal";
    case WaterCostIsolation::NoReflection: return "no-reflection";
    case WaterCostIsolation::Flat: return "flat";
    case WaterCostIsolation::SkipDraw: return "skip-draw";
    case WaterCostIsolation::Count: break;
  }
  return "full";
}

} // namespace ae::renderer
