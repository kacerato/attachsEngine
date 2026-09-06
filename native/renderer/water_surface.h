#pragma once

#include "core/base.h"

#include <array>
#include <span>

namespace ae::renderer {

inline constexpr u32 WaterProfileMagic = 0x52574541; // AEWR
inline constexpr u32 WaterProfileVersion = 1;
inline constexpr u32 MaximumWaterWaves = 8;
inline constexpr u32 MaximumWaterClipmapLevels = 8;
inline constexpr u32 MaximumWaterPatches = 4 + 12 * (MaximumWaterClipmapLevels - 1);
inline constexpr u32 MaximumWaterInteractions = 8;

enum class WaterDomain : u32 { InfiniteOcean = 0, FiniteSurface = 1, RiverSpline = 2 };
enum class WaterReflection : u32 { Environment = 0, ScreenSpace = 1, Planar = 2 };

struct WaterVec2 final { float x = 0.0f, y = 0.0f; };
struct WaterVec3 final { float x = 0.0f, y = 0.0f, z = 0.0f; };

struct WaterWave final {
  WaterVec2 direction{1.0f, 0.0f};
  float amplitude = 0.25f;
  float wavelength = 8.0f;
  float speed = 1.0f;
  float steepness = 0.35f;
  float phase = 0.0f;
};

// Backend-neutral, serializable authoring resource. GPU handles, scene names and
// editor state deliberately do not belong here.
struct WaterProfile final {
  u32 schemaVersion = WaterProfileVersion;
  WaterDomain domain = WaterDomain::InfiniteOcean;
  WaterReflection reflection = WaterReflection::Environment;
  u32 waveCount = 0;
  std::array<WaterWave, MaximumWaterWaves> waves{};
  WaterVec3 deepColor{0.015f, 0.12f, 0.19f};
  WaterVec3 shallowColor{0.05f, 0.42f, 0.48f};
  WaterVec3 absorption{0.18f, 0.055f, 0.028f};
  float refractiveIndex = 1.333f;
  float roughness = 0.22f;
  float turbidity = 0.10f;
  float foamThreshold = 0.82f;
  float foamDecay = 0.65f;
  float surfaceOpacity = 0.72f;
  float microWaveStrength = 1.0f;
  float maximumDistance = 8000.0f;
  float basePatchSize = 16.0f;
  u32 clipmapLevels = 7;
};

// Engine fallback resource used until a surface selects an authored profile.
// It is ordinary replaceable data; no wave constant is embedded in a shader.
WaterProfile defaultOceanWaterProfile() noexcept;

// Composable authoring operation; leaves the destination untouched on error.
// Length changes preserve deep-water dispersion. Spread is relative to +X;
// a caller may rotate the complete result to its authored wind direction.
struct WaterWaveAuthoring final {
  float lengthScale = 1.0f;
  float directionalSpread = 1.0f;
  float crossSwell = 0.0f;
};
bool authorWaterWaves(const WaterProfile &source, const WaterWaveAuthoring &settings,
                      WaterProfile &destination) noexcept;

// Linear finite-depth long wave for stress testing, not coastal inundation.
// Caller reserves a slot explicitly; existing waves are never overwritten.
bool appendLongWaterWave(WaterProfile &profile, float amplitude, float wavelength,
                         float depth, WaterVec2 direction) noexcept;

enum class WaterValidationError : u32 {
  None = 0, Schema, Domain, Reflection, WaveCount, Wave, Optical, Distance, Clipmap
};

WaterValidationError validateWaterProfile(const WaterProfile &profile) noexcept;

struct WaterSample final {
  float height = 0.0f;
  WaterVec3 normal{0.0f, 1.0f, 0.0f};
  WaterVec3 velocity{};
  float breaking = 0.0f;
};

// Same analytic spectrum is consumed by rendering, buoyancy and effects. No
// frame state is retained, so queries are deterministic and thread-safe.
WaterSample sampleWaterSurface(const WaterProfile &profile, WaterVec2 position,
                               float timeSeconds) noexcept;

// Conservative vertical extent used by visibility, shadow and streaming
// bounds. It is derived from authored data instead of being guessed by a
// backend or scene. Gerstner-style horizontal displacement can be added to
// this contract without changing callers.
float maximumWaterDisplacement(const WaterProfile &profile) noexcept;

struct WaterImpulse final {
  WaterVec2 center{};
  float startTime = 0.0f;
  float amplitude = 0.0f;
  float wavelength = 2.5f;
  float speed = 4.0f;
  float decay = 1.25f;
  float duration = 4.0f;
};

// Small bounded interaction field for touch, rain, boats and rigid bodies.
// It allocates nothing per frame and uses the same analytic ring on CPU/GPU.
class WaterInteractionField final {
public:
  bool addImpulse(const WaterImpulse &impulse) noexcept;
  WaterSample sample(WaterVec2 position, float timeSeconds) const noexcept;
  const std::array<WaterImpulse, MaximumWaterInteractions> &impulses() const { return impulses_; }
  u32 activeCount(float timeSeconds) const noexcept;
  void clear() noexcept { impulses_ = {}; next_ = 0; }
private:
  std::array<WaterImpulse, MaximumWaterInteractions> impulses_{};
  u32 next_ = 0;
};

enum class WaterExclusionShape : u32 { Circle = 0, Box = 1 };
struct WaterExclusionVolume final {
  WaterExclusionShape shape = WaterExclusionShape::Circle;
  WaterVec2 center{};
  WaterVec2 halfExtent{1.0f, 1.0f};
  float rotationRadians = 0.0f;
  float feather = 0.25f;
};
// 0 removes water, 1 keeps it. Feathering is analytic and stable under motion.
float waterCoverage(const WaterExclusionVolume &volume, WaterVec2 position) noexcept;

struct WaterPatch final {
  WaterVec2 origin{};
  float size = 0.0f;
  float skirtDepth = 0.0f;
  u32 level = 0;
};
struct WaterPatchPlan final {
  std::array<WaterPatch, MaximumWaterPatches> patches{};
  u32 count = 0;
};

// Camera-centred nested clipmap. Origins snap to each level's patch grid, so
// sub-patch camera motion cannot make distant geometry swim.
bool planWaterClipmap(const WaterProfile &profile, WaterVec2 camera,
                      WaterPatchPlan &plan) noexcept;

struct WaterCapabilities final {
  bool sampledSceneDepth = false;
  bool sampledSceneColor = false;
  bool computeShaders = false;
  bool halfFloatStorage = false;
  bool tessellation = false;
};
struct ResolvedWaterPipeline final {
  WaterReflection reflection = WaterReflection::Environment;
  bool refraction = false;
  bool spectralSimulation = false;
  bool tessellation = false;
  bool usedFallback = false;
};
ResolvedWaterPipeline resolveWaterPipeline(const WaterProfile &profile,
                                           const WaterCapabilities &capabilities) noexcept;

} // namespace ae::renderer
