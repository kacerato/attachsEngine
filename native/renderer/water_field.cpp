#include "renderer/water_field.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::renderer {
namespace {
bool finite(float value) noexcept { return std::isfinite(value); }

WaterVec3 normalized(WaterVec3 v) noexcept {
  const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  return length > 1.0e-8f ? WaterVec3{v.x / length, v.y / length, v.z / length}
                          : WaterVec3{0.0f, 1.0f, 0.0f};
}

// Slopes are recovered instead of averaging normals: two unit normals average
// to a direction that belongs to neither surface, and the error grows exactly
// where waves are steepest.
WaterVec2 slopeOf(WaterVec3 normal) noexcept {
  if (!(std::abs(normal.y) > 1.0e-6f)) return {0.0f, 0.0f};
  return {-normal.x / normal.y, -normal.z / normal.y};
}
} // namespace

bool validateWaterCurrents(const WaterCurrentSettings &settings) noexcept {
  if (settings.count > MaximumWaterCurrentSources || !finite(settings.uniform.x) ||
      !finite(settings.uniform.y) || !finite(settings.maximumSpeed) ||
      settings.maximumSpeed <= 0.0f || settings.maximumSpeed > 100.0f ||
      std::abs(settings.uniform.x) > 100.0f || std::abs(settings.uniform.y) > 100.0f)
    return false;
  for (u32 i = 0; i < settings.count; ++i) {
    const auto &source = settings.sources[i];
    if (source.kind != WaterCurrentKind::Directional && source.kind != WaterCurrentKind::Radial &&
        source.kind != WaterCurrentKind::Vortex) return false;
    if (!finite(source.center.x) || !finite(source.center.y) || !finite(source.radius) ||
        source.radius <= 0.0f || !finite(source.speed) || std::abs(source.speed) > 100.0f)
      return false;
    if (source.kind == WaterCurrentKind::Directional &&
        (!finite(source.direction.x) || !finite(source.direction.y) ||
         std::abs(std::hypot(source.direction.x, source.direction.y) - 1.0f) > 1.0e-4f))
      return false;
  }
  return true;
}

namespace {
WaterVec2 sampleValidatedCurrent(const WaterCurrentSettings &settings, WaterVec2 position) noexcept {
  WaterVec2 result = settings.uniform;
  for (u32 i = 0; i < settings.count; ++i) {
    const auto &source = settings.sources[i];
    const float x = (position.x - source.center.x) / source.radius;
    const float z = (position.y - source.center.y) / source.radius;
    const float distance = std::hypot(x, z);
    if (!(distance < 1.0f)) continue;
    const float envelope = 1.0f - distance * distance * (3.0f - 2.0f * distance);
    WaterVec2 direction = source.direction;
    // Linear core avoids a singular direction/speed at the source centre.
    if (source.kind == WaterCurrentKind::Radial) direction = {x, z};
    if (source.kind == WaterCurrentKind::Vortex) direction = {-z, x};
    result.x += source.speed * envelope * direction.x;
    result.y += source.speed * envelope * direction.y;
  }
  const float speed = std::hypot(result.x, result.y);
  if (speed > settings.maximumSpeed) {
    result.x *= settings.maximumSpeed / speed;
    result.y *= settings.maximumSpeed / speed;
  }
  return result;
}
} // namespace

WaterVec2 sampleWaterCurrent(const WaterCurrentSettings &settings, WaterVec2 position) noexcept {
  if (!validateWaterCurrents(settings) || !finite(position.x) || !finite(position.y)) return {};
  return sampleValidatedCurrent(settings, position);
}

bool validateWaterBathymetry(const WaterBathymetry &data) noexcept {
  if (data.width == 0 && data.height == 0) return true;
  if (data.width < 2 || data.height < 2 || data.width > WaterBathymetry::MaximumDimension ||
      data.height > WaterBathymetry::MaximumDimension || !finite(data.origin.x) ||
      !finite(data.origin.y) || !finite(data.spacing.x) || !finite(data.spacing.y) ||
      data.spacing.x <= 0 || data.spacing.y <= 0) return false;
  for (u32 i = 0; i < data.width * data.height; ++i)
    if (!finite(data.bottomHeights[i])) return false;
  return true;
}

namespace {
bool sampleValidatedBottom(const WaterBathymetry &data, WaterVec2 position, float &height) noexcept {
  if (data.width == 0) return false;
  const float x = (position.x - data.origin.x) / data.spacing.x;
  const float z = (position.y - data.origin.y) / data.spacing.y;
  if (!(x >= 0 && z >= 0 && x <= data.width - 1 && z <= data.height - 1)) return false;
  const u32 ix = std::min(static_cast<u32>(x), data.width - 2);
  const u32 iz = std::min(static_cast<u32>(z), data.height - 2);
  const float a = std::lerp(data.bottomHeights[iz * data.width + ix],
                            data.bottomHeights[iz * data.width + ix + 1], x - ix);
  const float b = std::lerp(data.bottomHeights[(iz + 1) * data.width + ix],
                            data.bottomHeights[(iz + 1) * data.width + ix + 1], x - ix);
  height = std::lerp(a, b, z - iz);
  return true;
}

bool validVolume(const WaterExclusionVolume &volume) noexcept {
  return (volume.shape == WaterExclusionShape::Circle || volume.shape == WaterExclusionShape::Box) &&
      finite(volume.center.x) && finite(volume.center.y) && finite(volume.halfExtent.x) &&
      finite(volume.halfExtent.y) && volume.halfExtent.x > 0 && volume.halfExtent.y > 0 &&
      finite(volume.rotationRadians) && finite(volume.feather) && volume.feather >= 0;
}
} // namespace

bool sampleWaterBottom(const WaterBathymetry &data, WaterVec2 position, float &height) noexcept {
  return validateWaterBathymetry(data) && sampleValidatedBottom(data, position, height);
}

bool WaterField::configure(const WaterFieldSetup &setup) noexcept {
  // Validate before committing: a bad live edit must not disable valid water.
  if (setup.requested != WaterFieldProvider::Analytic &&
      setup.requested != WaterFieldProvider::SpectralCpu &&
      setup.requested != WaterFieldProvider::SpectralGpu) return false;
  if (validateWaterProfile(setup.profile) != WaterValidationError::None) return false;
  if (!finite(setup.baseHeight) || !finite(setup.bottomHeight)) return false;
  if (setup.exclusionCount > MaximumWaterExclusionVolumes) return false;
  if (!validateWaterCurrents(setup.currents)) return false;
  if (!validateWaterBathymetry(setup.bathymetry)) return false;
  if (setup.bounded && !validVolume(setup.boundary)) return false;
  for (u32 i = 0; i < setup.exclusionCount; ++i)
    if (!validVolume(setup.exclusions[i])) return false;
  if (setup.hasBathymetry && !(setup.bottomHeight < setup.baseHeight)) return false;

  status_ = {};
  status_.requested = setup.requested;
  setup_ = setup;
  status_.configured = true;
  status_.resolved = WaterFieldProvider::Analytic;
  if (setup.requested == WaterFieldProvider::SpectralCpu) {
    if (setup.mirrorSet != nullptr && setup.mirrorSet->isReady()) {
      status_.resolved = WaterFieldProvider::SpectralCpu;
      status_.cascadesActive = setup.mirrorSet->cascadeCount();
      status_.cascadeResolution = setup.mirrorSet->maximumResolution();
      status_.simulationTime = setup.mirrorSet->simulationTime();
    } else if (setup.mirror != nullptr && setup.mirror->isReady()) {
      status_.resolved = WaterFieldProvider::SpectralCpu;
      status_.cascadesActive = 1;
      status_.cascadeResolution = setup.mirror->resolution();
      status_.simulationTime = setup.mirror->simulationTime();
    } else if (setup.mirrorSet != nullptr || setup.mirror != nullptr)
      status_.fallbackReason = "espelho espectral sem avaliacao";
    else status_.fallbackReason = "nenhum espelho espectral fornecido";
  } else if (setup.requested == WaterFieldProvider::SpectralGpu) {
    // The readback provider is deliberately absent: physics reads the mirror,
    // which has no frame-dependent age. Saying so beats reporting it resolved.
    status_.fallbackReason = "leitura de GPU nao e provedor de fisica";
  }
  return true;
}

WaterFieldStatus WaterField::status() const noexcept {
  auto result = status_;
  if (result.resolved == WaterFieldProvider::SpectralCpu) {
    if (setup_.mirrorSet != nullptr) result.simulationTime = setup_.mirrorSet->simulationTime();
    else if (setup_.mirror != nullptr) result.simulationTime = setup_.mirror->simulationTime();
  }
  return result;
}

WaterFieldSample WaterField::sampleOne(WaterVec2 position, float timeSeconds) const noexcept {
  WaterFieldSample result{};
  if (!status_.configured || !finite(position.x) || !finite(position.y) ||
      !finite(timeSeconds))
    return result;

  float coverage = setup_.bounded ? 1.0f - waterCoverage(setup_.boundary, position) : 1.0f;
  for (u32 index = 0; index < setup_.exclusionCount; ++index)
    coverage = std::min(coverage, waterCoverage(setup_.exclusions[index], position));
  float bottom = setup_.bottomHeight;
  const bool depthKnown = sampleValidatedBottom(setup_.bathymetry, position, bottom) || setup_.hasBathymetry;
  if (depthKnown && bottom >= setup_.baseHeight) coverage = 0.0f;
  result.coverage = coverage;
  if (coverage > 0.0f) result.flow = sampleValidatedCurrent(setup_.currents, position);
  result.flags = static_cast<u32>(WaterFieldFlag::Valid);
  if (coverage <= 0.0f) result.flags |= static_cast<u32>(WaterFieldFlag::Excluded);

  const WaterSample impulse = setup_.interaction.sample(position, timeSeconds);
  WaterVec2 impulseSlope = slopeOf(impulse.normal);
  // A ondulação soma na mesma conta dos impulsos analíticos: altura sobre a
  // superfície e inclinação sobre a normal. Fora da área simulada ela devolve
  // zero, e somar zero é exatamente o que se quer na fronteira — sem degrau.
  float rippleHeight = 0.0f;
  if (setup_.ripples != nullptr && setup_.ripples->isReady() && coverage > 0.0f) {
    rippleHeight = setup_.ripples->height(position.x, position.y);
    float rippleSlopeX = 0.0f, rippleSlopeZ = 0.0f;
    setup_.ripples->slope(position.x, position.y, rippleSlopeX, rippleSlopeZ);
    impulseSlope.x += rippleSlopeX;
    impulseSlope.y += rippleSlopeZ;
  }

  if (status_.resolved == WaterFieldProvider::SpectralCpu) {
    const WaterMirrorSample mirror = setup_.mirrorSet != nullptr
        ? setup_.mirrorSet->sample(position) : setup_.mirror->sample(position);
    result.height = setup_.baseHeight + mirror.height + impulse.height + rippleHeight;
    result.normal = normalized({-(mirror.slope.x + impulseSlope.x), 1.0f,
                                -(mirror.slope.y + impulseSlope.y)});
    result.velocity = {mirror.velocity.x, mirror.velocity.y + impulse.velocity.y,
                       mirror.velocity.z};
    result.foam = std::clamp(impulse.breaking, 0.0f, 1.0f);
    // The mirror does not evaluate the horizontal derivative fields, so the
    // determinant is unknown here rather than one: with choppy displacement the
    // mapping is not the identity, and claiming one would be a fabrication.
  } else {
    const WaterSample spectrum = sampleWaterSurface(setup_.profile, position, timeSeconds);
    result.height = setup_.baseHeight + spectrum.height + impulse.height + rippleHeight;
    const WaterVec2 slope = slopeOf(spectrum.normal);
    result.normal = normalized({-(slope.x + impulseSlope.x), 1.0f, -(slope.y + impulseSlope.y)});
    result.velocity = {spectrum.velocity.x, spectrum.velocity.y + impulse.velocity.y,
                       spectrum.velocity.z};
    result.foam = std::clamp(spectrum.breaking + impulse.breaking, 0.0f, 1.0f);
    // The analytic profile displaces vertically only, so the horizontal mapping
    // is the identity and its determinant is exactly one. A ondulação também é
    // deslocamento vertical puro, então não muda essa conta.
    result.jacobian = 1.0f;
    result.flags |= static_cast<u32>(WaterFieldFlag::JacobianKnown);
  }

  if (depthKnown) {
    result.depth = std::max(setup_.baseHeight - bottom, 0.0f);
    result.flags |= static_cast<u32>(WaterFieldFlag::DepthKnown);
  }
  return result;
}

bool WaterField::sample(std::span<const WaterVec2> positions, double timeSeconds,
                        std::span<WaterFieldSample> out) const noexcept {
  if (out.size() < positions.size()) return false;
  if (!status_.configured || !std::isfinite(timeSeconds) ||
      std::abs(timeSeconds) > std::numeric_limits<float>::max()) return false;
  for (const auto &position : positions)
    if (!finite(position.x) || !finite(position.y)) return false;
  const float time = static_cast<float>(timeSeconds);
  for (usize index = 0; index < positions.size(); ++index)
    out[index] = sampleOne(positions[index], time);
  return true;
}

float WaterField::heightOnly(WaterVec2 position, double timeSeconds) const noexcept {
  if (!std::isfinite(timeSeconds)) return setup_.baseHeight;
  return sampleOne(position, static_cast<float>(timeSeconds)).height;
}

} // namespace ae::renderer
