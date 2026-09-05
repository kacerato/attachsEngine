#pragma once

#include "renderer/water_fft.h"
#include "renderer/water_surface.h"

#include <complex>
#include <vector>

namespace ae::renderer {

// Physics evaluates the same spectrum the GPU renders, at a lower resolution,
// on the CPU. The alternative — reading the GPU result back — delivers a
// surface that is one to three frames old, with an age that changes whenever
// the frame rate does. A body cannot be integrated deterministically against
// that, so the mirror trades a bounded, measurable truncation error for an
// exact answer at the requested time.
struct WaterMirrorSettings final {
  WaterSpectrumSettings spectrum{};  // resolution here is the physics resolution
  float displacementScale = 1.0f;
  float choppiness = 1.0f;
  float directionRadians = 0.0f;
  u32 inversionIterations = 3;
};

bool validateWaterMirrorSettings(const WaterMirrorSettings &settings) noexcept;

struct WaterMirrorSample final {
  float height = 0.0f;
  WaterVec3 velocity{};   // orbital velocity of the water particle, m/s
  WaterVec2 slope{};      // dh/dx, dh/dz at the parameter point
  WaterVec2 displacement{};
};

// Setup allocates; update and sample do not. A single mirror may be sampled
// from any number of threads, but never while update is running: update
// rewrites the spatial fields in place.
class WaterSpectralMirror final {
public:
  bool initialize(const WaterMirrorSettings &settings);
  bool update(double simulationTime) noexcept;

  // World XZ in, surface at that column out. Horizontal displacement is
  // inverted by fixed point, so the answer belongs to the position asked for
  // rather than to the parameter point that happens to share its coordinates.
  WaterMirrorSample sample(WaterVec2 worldPosition) const noexcept;

  bool isReady() const noexcept { return resolution_ != 0 && evaluated_; }
  double simulationTime() const noexcept { return simulationTime_; }
  u32 resolution() const noexcept { return resolution_; }
  const WaterMirrorSettings &settings() const noexcept { return settings_; }

private:
  // Two real fields ride in one complex transform: both are real, so the
  // inverse yields one in the real part and the other in the imaginary part.
  enum class Packed : u32 { HeightDisplacementX, DisplacementZHeightRate,
                            DisplacementRate, Slope, Count };

  static usize packedIndex(Packed field) noexcept;
  WaterMirrorSample sampleParameter(WaterVec2 parameter) const noexcept;
  std::complex<float> bilinear(const std::vector<std::complex<float>> &field,
                               WaterVec2 parameter) const noexcept;

  WaterMirrorSettings settings_{};
  WaterFftPlan plan_;
  std::vector<std::complex<float>> initial_;
  std::vector<double> angularFrequency_;
  std::vector<std::complex<float>> packed_[static_cast<usize>(Packed::Count)];
  u32 resolution_ = 0;
  float patchLength_ = 1.0f;
  double simulationTime_ = 0.0;
  bool evaluated_ = false;
};

} // namespace ae::renderer
