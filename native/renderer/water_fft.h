#pragma once

#include "core/base.h"
#include <complex>
#include <array>
#include <span>
#include <vector>

namespace ae::renderer {

struct WaterSpectrumSettings final {
  u32 resolution = 128;
  u32 seed = 1;
  float patchLength = 256.0f;
  float depth = 20.0f;
  float windSpeed = 10.0f; // metres/second; zero gives a flat sea
  float windDirection = 0.0f; // radians, zero along +X
  float fetch = 100000.0f; // metres
  float swell = 0.8f;
  float spread = 0.2f; // zero directional, one isotropic
  float shortWaveDamping = 0.1f; // metres
  float minimumWavelength = 0.0f;
  float maximumWavelength = 10000.0f;
};

bool validateWaterSpectrum(const WaterSpectrumSettings &settings) noexcept;
// Generates unshifted TMA/JONSWAP amplitudes for the normalized inverse FFT.
// Offline/reconfiguration operation, never a per-frame allocation.
bool generateWaterSpectrum(const WaterSpectrumSettings &settings,
                           std::vector<std::complex<float>> &amplitudes);

// CPU reference and fallback primitive for spectral water. Setup owns tables;
// execution allocates nothing. A plan can be shared across threads provided
// each caller owns its data. Inverse transforms normalize by N per axis.
class WaterFftPlan final {
public:
  bool initialize(u32 size);
  u32 size() const noexcept { return size_; }
  bool transform(std::span<std::complex<float>> data, bool inverse) const noexcept;
  bool transform2D(std::span<std::complex<float>> data, bool inverse) const noexcept;
private:
  void transformStrided(std::complex<float> *data, usize stride, bool inverse) const noexcept;
  u32 size_ = 0;
  std::vector<u32> permutation_;
  std::vector<std::complex<float>> roots_;
};

// Spectral evolution reference. h0 is authored in unshifted FFT order; opposite
// frequencies are paired internally to guarantee a real height field. Depth
// and patch length are metres, time is seconds. Not a GPU render pass.
class WaterSpectralField final {
public:
  enum class Channel : u32 { DisplacementX, DisplacementZ, SlopeX, SlopeZ,
                            DisplacementXX, DisplacementXZ, DisplacementZZ, Count };
  bool initialize(const WaterSpectrumSettings &settings);
  bool initialize(u32 size, float patchLength, float depth,
                  std::span<const std::complex<float>> initialSpectrum);
  bool update(double timeSeconds) noexcept;
  std::span<const std::complex<float>> heights() const noexcept { return spatial_; }
  // Unit choppiness. Apply the same multiplier to displacement and its derivatives.
  std::span<const std::complex<float>> channel(Channel channel) const noexcept;
  float jacobian(usize sample, float choppiness) const noexcept;
private:
  WaterFftPlan plan_;
  std::vector<std::complex<float>> initial_, spatial_;
  std::vector<double> angularFrequency_;
  std::array<std::vector<std::complex<float>>, static_cast<usize>(Channel::Count)> channels_;
  float patchLength_ = 1;
};

} // namespace ae::renderer
