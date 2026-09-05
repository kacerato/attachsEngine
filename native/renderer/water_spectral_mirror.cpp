#include "renderer/water_spectral_mirror.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
constexpr double TwoPi = 6.2831853071795864769;
} // namespace

usize WaterSpectralMirror::packedIndex(Packed field) noexcept { return static_cast<usize>(field); }

bool validateWaterMirrorSettings(const WaterMirrorSettings &settings) noexcept {
  if (!validateWaterSpectrum(settings.spectrum)) return false;
  // The mirror exists to be cheap. A resolution that is not cheap defeats the
  // reason it is not a GPU readback in the first place.
  if (settings.spectrum.resolution < 8 || settings.spectrum.resolution > 128) return false;
  if (!std::isfinite(settings.displacementScale) || settings.displacementScale < 0.0f ||
      settings.displacementScale > 3.0f)
    return false;
  if (!std::isfinite(settings.choppiness) || settings.choppiness < 0.0f ||
      settings.choppiness > 2.0f)
    return false;
  if (!std::isfinite(settings.directionRadians)) return false;
  return settings.inversionIterations <= 8;
}

bool WaterSpectralMirror::initialize(const WaterMirrorSettings &settings) {
  resolution_ = 0;
  evaluated_ = false;
  if (!validateWaterMirrorSettings(settings)) return false;

  std::vector<std::complex<float>> amplitudes;
  if (!generateWaterSpectrum(settings.spectrum, amplitudes)) return false;

  WaterFftPlan plan;
  if (!plan.initialize(settings.spectrum.resolution)) return false;

  const u32 size = settings.spectrum.resolution;
  const usize count = static_cast<usize>(size) * size;
  std::vector<double> frequencies(count);
  for (u32 z = 0; z < size; ++z)
    for (u32 x = 0; x < size; ++x) {
      const int kx = x <= size / 2 ? static_cast<int>(x) : static_cast<int>(x) - static_cast<int>(size);
      const int kz = z <= size / 2 ? static_cast<int>(z) : static_cast<int>(z) - static_cast<int>(size);
      const double k = TwoPi * std::hypot(kx, kz) / settings.spectrum.patchLength;
      frequencies[z * size + x] = std::sqrt(9.81 * k * std::tanh(k * settings.spectrum.depth));
    }

  decltype(packed_) packed;
  for (auto &field : packed) field.assign(count, {});

  plan_ = std::move(plan);
  initial_ = std::move(amplitudes);
  angularFrequency_ = std::move(frequencies);
  for (usize field = 0; field < packedIndex(Packed::Count); ++field)
    packed_[field] = std::move(packed[field]);
  settings_ = settings;
  resolution_ = size;
  patchLength_ = settings.spectrum.patchLength;
  simulationTime_ = 0.0;
  return true;
}

bool WaterSpectralMirror::update(double simulationTime) noexcept {
  if (resolution_ == 0 || !std::isfinite(simulationTime)) return false;
  const u32 size = resolution_;
  const float dk = static_cast<float>(TwoPi) / patchLength_;

  auto &heightDisplacementX = packed_[packedIndex(Packed::HeightDisplacementX)];
  auto &displacementZRate = packed_[packedIndex(Packed::DisplacementZHeightRate)];
  auto &displacementRate = packed_[packedIndex(Packed::DisplacementRate)];
  auto &slope = packed_[packedIndex(Packed::Slope)];

  for (u32 z = 0; z < size; ++z)
    for (u32 x = 0; x < size; ++x) {
      const usize index = z * size + x;
      const usize opposite = ((size - z) % size) * size + (size - x) % size;
      const double phase = std::remainder(angularFrequency_[index] * simulationTime, TwoPi);
      if (!std::isfinite(phase)) return false;
      const std::complex<float> rotation{static_cast<float>(std::cos(phase)),
                                         static_cast<float>(std::sin(phase))};
      const auto forward = initial_[index] * rotation;
      const auto backward = std::conj(initial_[opposite]) * std::conj(rotation);
      const auto height = forward + backward;
      // The two travelling halves separate here: the surface is their sum and
      // its time derivative is i*omega times their difference. Deriving the
      // rate this way keeps it exact at the requested instant, where a finite
      // difference between frames would make it depend on the frame rate.
      const auto omega = static_cast<float>(angularFrequency_[index]);
      const std::complex<float> difference = forward - backward;
      const auto heightRate = std::complex<float>(-difference.imag(), difference.real()) * omega;

      // Odd derivative symbols must vanish at Nyquist to keep the field real.
      const float kx = x == size / 2 ? 0.0f
          : dk * static_cast<float>(x < size / 2 ? static_cast<int>(x)
                                                 : static_cast<int>(x) - static_cast<int>(size));
      const float kz = z == size / 2 ? 0.0f
          : dk * static_cast<float>(z < size / 2 ? static_cast<int>(z)
                                                 : static_cast<int>(z) - static_cast<int>(size));
      const float k = std::hypot(kx, kz);
      const float inverseK = k > 0.0f ? 1.0f / k : 0.0f;

      const auto timesI = [](std::complex<float> value) {
        return std::complex<float>(-value.imag(), value.real());
      };
      const auto displacementX = timesI(height) * (-kx * inverseK);
      const auto displacementZ = timesI(height) * (-kz * inverseK);
      const auto displacementXRate = timesI(heightRate) * (-kx * inverseK);
      const auto displacementZRateSpectrum = timesI(heightRate) * (-kz * inverseK);
      const auto slopeX = timesI(height) * kx;
      const auto slopeZ = timesI(height) * kz;

      heightDisplacementX[index] = height + timesI(displacementX);
      displacementZRate[index] = displacementZ + timesI(heightRate);
      displacementRate[index] = displacementXRate + timesI(displacementZRateSpectrum);
      slope[index] = slopeX + timesI(slopeZ);
    }

  for (auto &field : packed_)
    if (!plan_.transform2D(field, true)) return false;
  simulationTime_ = simulationTime;
  evaluated_ = true;
  return true;
}

std::complex<float> WaterSpectralMirror::bilinear(const std::vector<std::complex<float>> &field,
                                                  WaterVec2 parameter) const noexcept {
  const float size = static_cast<float>(resolution_);
  const float u = parameter.x / patchLength_ * size;
  const float v = parameter.y / patchLength_ * size;
  const float floorU = std::floor(u), floorV = std::floor(v);
  const float fractionU = u - floorU, fractionV = v - floorV;
  const auto wrap = [this](float value) {
    const int n = static_cast<int>(resolution_);
    int wrapped = static_cast<int>(std::fmod(value, static_cast<float>(n)));
    if (wrapped < 0) wrapped += n;
    return static_cast<u32>(wrapped);
  };
  const u32 x0 = wrap(floorU), z0 = wrap(floorV);
  const u32 x1 = (x0 + 1) % resolution_, z1 = (z0 + 1) % resolution_;
  const auto a = field[z0 * resolution_ + x0], b = field[z0 * resolution_ + x1];
  const auto c = field[z1 * resolution_ + x0], d = field[z1 * resolution_ + x1];
  const auto top = a + (b - a) * fractionU;
  const auto bottom = c + (d - c) * fractionU;
  return top + (bottom - top) * fractionV;
}

WaterMirrorSample WaterSpectralMirror::sampleParameter(WaterVec2 parameter) const noexcept {
  WaterMirrorSample result{};
  const float gain = settings_.displacementScale;
  const float choppiness = settings_.choppiness * gain;

  const auto heightDisplacementX = bilinear(packed_[packedIndex(Packed::HeightDisplacementX)], parameter);
  const auto displacementZRate = bilinear(packed_[packedIndex(Packed::DisplacementZHeightRate)], parameter);
  const auto displacementRate = bilinear(packed_[packedIndex(Packed::DisplacementRate)], parameter);
  const auto slope = bilinear(packed_[packedIndex(Packed::Slope)], parameter);

  result.height = heightDisplacementX.real() * gain;
  result.displacement = {heightDisplacementX.imag() * choppiness,
                         displacementZRate.real() * choppiness};
  result.velocity = {displacementRate.real() * choppiness, displacementZRate.imag() * gain,
                     displacementRate.imag() * choppiness};
  result.slope = {slope.real() * gain, slope.imag() * gain};
  return result;
}

WaterMirrorSample WaterSpectralMirror::sample(WaterVec2 worldPosition) const noexcept {
  if (!isReady() || !std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y))
    return {};

  const float cosine = std::cos(settings_.directionRadians);
  const float sine = std::sin(settings_.directionRadians);
  // Rotating into spectrum space instead of rotating the spectrum keeps the
  // stored fields independent of wind direction, so turning the wind costs a
  // rotation per query rather than a full respectralization.
  const WaterVec2 local{cosine * worldPosition.x + sine * worldPosition.y,
                        -sine * worldPosition.x + cosine * worldPosition.y};

  // Choppy waves move the surface horizontally, so the column above a world
  // position is not the parameter point with the same coordinates. Fixed point
  // converges wherever the mapping does not fold, which is the same condition
  // the renderer needs for the Jacobian to stay positive.
  WaterVec2 parameter = local;
  WaterMirrorSample sample = sampleParameter(parameter);
  for (u32 iteration = 0; iteration < settings_.inversionIterations; ++iteration) {
    parameter = {local.x - sample.displacement.x, local.y - sample.displacement.y};
    sample = sampleParameter(parameter);
  }

  WaterMirrorSample result = sample;
  result.velocity = {cosine * sample.velocity.x - sine * sample.velocity.z, sample.velocity.y,
                     sine * sample.velocity.x + cosine * sample.velocity.z};
  result.slope = {cosine * sample.slope.x - sine * sample.slope.y,
                  sine * sample.slope.x + cosine * sample.slope.y};
  result.displacement = {cosine * sample.displacement.x - sine * sample.displacement.y,
                         sine * sample.displacement.x + cosine * sample.displacement.y};
  return result;
}

} // namespace ae::renderer
