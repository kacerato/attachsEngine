#pragma once

#include "renderer/water_ripples.h"
#include "renderer/water_spectral_mirror.h"
#include "renderer/water_surface.h"
#include "renderer/water_route.h"

#include <array>
#include <span>

namespace ae::renderer {

inline constexpr u32 MaximumWaterExclusionVolumes = 8;
inline constexpr u32 MaximumWaterCurrentSources = 16;

enum class WaterCurrentKind : u32 { Directional = 0, Radial = 1, Vortex = 2 };
struct WaterCurrentSource final {
  WaterCurrentKind kind = WaterCurrentKind::Directional;
  WaterVec2 center{};
  WaterVec2 direction{1.0f, 0.0f}; // unit vector for directional sources
  float radius = 10.0f;
  // Signed velocity scale (m/s), not the peak speed of radial/vortex sources:
  // their linear core multiplies it by distance/radius before boundary falloff.
  float speed = 1.0f;
};

// Value-owned, bounded current field. Orbital velocity remains separate so
// consumers add the current exactly once. This is not a fluid solver.
struct WaterCurrentSettings final {
  WaterVec2 uniform{};
  std::array<WaterCurrentSource, MaximumWaterCurrentSources> sources{};
  u32 count = 0;
  float maximumSpeed = 30.0f;
};
bool validateWaterCurrents(const WaterCurrentSettings &settings) noexcept;
WaterVec2 sampleWaterCurrent(const WaterCurrentSettings &settings, WaterVec2 position) noexcept;

// Shared query contract for physics/gameplay. Matching the renderer's spectrum,
// domain and clock is the integration owner's responsibility, not a guarantee
// made by selecting a provider here.
enum class WaterFieldProvider : u32 { Analytic = 0, SpectralCpu = 1, SpectralGpu = 2 };

enum class WaterFieldFlag : u32 {
  Valid = 1u << 0,
  Excluded = 1u << 1,      // coverage collapsed to zero at this position
  DepthKnown = 1u << 2,    // bathymetry configured; depth is not a guess
  Extrapolated = 1u << 3,  // provider data older than the requested time
  JacobianKnown = 1u << 4, // the horizontal mapping determinant was computed
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
  float jacobian = 1.0f;       // only meaningful with JacobianKnown
  float depth = 0.0f;          // still surface down to the fixed bottom, metres
  float instantaneousDepth = -1.0f; // wave surface to bottom; negative when unavailable
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

// Small value-owned bathymetry tile; importers may resample larger terrain into
// tiles. Width/height zero disables the tile. Outside it depth stays unknown
// unless the caller also supplies a flat-bottom fallback.
struct WaterBathymetry final {
  static constexpr u32 MaximumDimension = 32;
  u32 width = 0, height = 0;
  WaterVec2 origin{};
  WaterVec2 spacing{1.0f, 1.0f};
  std::array<float, MaximumDimension * MaximumDimension> bottomHeights{};
};
bool validateWaterBathymetry(const WaterBathymetry &data) noexcept;
bool sampleWaterBottom(const WaterBathymetry &data, WaterVec2 position, float &height) noexcept;

// Copied wholesale on configure. Holding pointers would make a query depend on
// the lifetime of scene objects, and the field is read from jobs.
struct WaterFieldSetup final {
  WaterRoute route{};
  float routeTransform[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  float waveScale=1,rippleScale=1,foamScale=1;
  WaterProfile profile{};
  float baseHeight = 0.0f;
  WaterVec2 planeOrigin{},planeSlope{};
  float bottomHeight = 0.0f;
  bool hasBathymetry = false;
  WaterBathymetry bathymetry{};
  bool bounded = false;
  // Uses the same rotated box/circle and feather contract as exclusions,
  // inverted to retain water inside instead of outside.
  WaterExclusionVolume boundary{};
  WaterInteractionField interaction{};
  WaterCurrentSettings currents{};
  std::array<WaterExclusionVolume, MaximumWaterExclusionVolumes> exclusions{};
  u32 exclusionCount = 0;
  WaterFieldProvider requested = WaterFieldProvider::Analytic;
  // Borrowed, not copied: a mirror owns megabytes of spectral fields. It must
  // outlive the field, and update() must never run while sample() is in flight.
  const WaterSpectralMirror *mirror = nullptr;
  // Multicascata e preferido quando presente. `mirror` permanece como contrato
  // compativel para ferramentas que avaliam uma unica banda isoladamente.
  const WaterSpectralMirrorSet *mirrorSet = nullptr;
  // Ondulação dinâmica, também emprestada e pelas mesmas razões. Ela entra aqui,
  // e não em cada consumidor, porque este é o ponto onde os sistemas de onda se
  // somam: quem pergunta a altura da água recebe o mar, a interação e a
  // ondulação juntos, sem saber que são três coisas. Nulo mantém o
  // comportamento anterior.
  const WaterRippleField *ripples = nullptr;
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

  WaterFieldStatus status() const noexcept;

private:
  WaterFieldSample sampleOne(WaterVec2 position, float timeSeconds) const noexcept;

  WaterFieldSetup setup_{};
  WaterFieldStatus status_{};
};

} // namespace ae::renderer
