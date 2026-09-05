#pragma once

#include "renderer/water_surface.h"

#include <array>
#include <span>

namespace ae::renderer {

inline constexpr u32 MaximumWaterExclusionVolumes = 8;

// The single surface every consumer reads: physics, gameplay, audio, particles
// and rendering. Providers differ in how the surface is produced; none of them
// changes what a caller receives, so a body never floats on a surface the
// renderer is not drawing.
enum class WaterFieldProvider : u32 { Analytic = 0, SpectralCpu = 1, SpectralGpu = 2 };

enum class WaterFieldFlag : u32 {
  Valid = 1u << 0,
  Excluded = 1u << 1,      // coverage collapsed to zero at this position
  DepthKnown = 1u << 2,    // bathymetry configured; depth is not a guess
  Extrapolated = 1u << 3,  // provider data older than the requested time
};

inline constexpr bool hasWaterFieldFlag(u32 flags, WaterFieldFlag flag) noexcept {
  return (flags & static_cast<u32>(flag)) != 0;
}

struct WaterFieldSample final {
  float height = 0.0f;         // world metres, already including the base height
  WaterVec3 normal{0.0f, 1.0f, 0.0f};
  WaterVec3 velocity{};        // orbital velocity of the water particle, m/s
  WaterVec2 flow{};            // current and flow map, m/s
  float coverage = 1.0f;       // 0 removes water, 1 keeps it
  float foam = 0.0f;
  float jacobian = 1.0f;       // below one compresses, below zero folds
  float depth = 0.0f;          // still surface down to the bottom, metres
  u32 flags = 0;
};

// Requested and resolved are separate on purpose: a caller must be able to see
// that it asked for the spectral provider and received the analytic one, and
// why. Silence here is what turns a missing capability into a mystery.
struct WaterFieldStatus final {
  WaterFieldProvider requested = WaterFieldProvider::Analytic;
  WaterFieldProvider resolved = WaterFieldProvider::Analytic;
  u32 cascadesActive = 0;
  u32 cascadeResolution = 0;
  u32 ageFrames = 0;
  double simulationTime = 0.0;
  const char *fallbackReason = nullptr;  // null when resolved equals requested
  bool configured = false;
};

// Copied wholesale on configure. Holding pointers would make a query depend on
// the lifetime of scene objects, and the field is read from jobs.
struct WaterFieldSetup final {
  WaterProfile profile{};
  float baseHeight = 0.0f;
  float bottomHeight = 0.0f;
  bool hasBathymetry = false;
  WaterInteractionField interaction{};
  std::array<WaterExclusionVolume, MaximumWaterExclusionVolumes> exclusions{};
  u32 exclusionCount = 0;
  WaterFieldProvider requested = WaterFieldProvider::Analytic;
};

// Deterministic and reentrant: a query is a pure function of position, time and
// configuration. Nothing is cached between calls, so any number of threads may
// sample the same field while a frame is in flight.
class WaterField final {
public:
  bool configure(const WaterFieldSetup &setup) noexcept;

  // Batched by contract. A per-point entry point on the managed boundary would
  // spend the whole native call budget on buoyancy alone.
  bool sample(std::span<const WaterVec2> positions, double timeSeconds,
              std::span<WaterFieldSample> out) const noexcept;

  // Convenience for native callers that genuinely need one value, such as a
  // camera height probe. Not exposed across the managed boundary.
  float heightOnly(WaterVec2 position, double timeSeconds) const noexcept;

  WaterFieldStatus status() const noexcept { return status_; }

private:
  WaterFieldSample sampleOne(WaterVec2 position, float timeSeconds) const noexcept;

  WaterFieldSetup setup_{};
  WaterFieldStatus status_{};
};

} // namespace ae::renderer
