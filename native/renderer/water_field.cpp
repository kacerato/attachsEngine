#include "renderer/water_field.h"

#include <algorithm>
#include <cmath>

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

bool WaterField::configure(const WaterFieldSetup &setup) noexcept {
  status_ = {};
  status_.requested = setup.requested;
  if (validateWaterProfile(setup.profile) != WaterValidationError::None) return false;
  if (!finite(setup.baseHeight) || !finite(setup.bottomHeight)) return false;
  if (setup.exclusionCount > MaximumWaterExclusionVolumes) return false;
  if (setup.hasBathymetry && !(setup.bottomHeight < setup.baseHeight)) return false;

  setup_ = setup;
  status_.configured = true;
  status_.resolved = WaterFieldProvider::Analytic;
  if (setup.requested == WaterFieldProvider::SpectralCpu) {
    if (setup.mirror == nullptr) status_.fallbackReason = "nenhum espelho espectral fornecido";
    else if (!setup.mirror->isReady()) status_.fallbackReason = "espelho espectral sem avaliacao";
    else {
      status_.resolved = WaterFieldProvider::SpectralCpu;
      status_.cascadesActive = 1;
      status_.cascadeResolution = setup.mirror->resolution();
      status_.simulationTime = setup.mirror->simulationTime();
    }
  } else if (setup.requested == WaterFieldProvider::SpectralGpu) {
    // The readback provider is deliberately absent: physics reads the mirror,
    // which has no frame-dependent age. Saying so beats reporting it resolved.
    status_.fallbackReason = "leitura de GPU nao e provedor de fisica";
  }
  return true;
}

WaterFieldSample WaterField::sampleOne(WaterVec2 position, float timeSeconds) const noexcept {
  WaterFieldSample result{};
  if (!status_.configured || !finite(position.x) || !finite(position.y) ||
      !finite(timeSeconds))
    return result;

  float coverage = 1.0f;
  for (u32 index = 0; index < setup_.exclusionCount; ++index)
    coverage = std::min(coverage, waterCoverage(setup_.exclusions[index], position));
  result.coverage = coverage;
  result.flags = static_cast<u32>(WaterFieldFlag::Valid);
  if (coverage <= 0.0f) result.flags |= static_cast<u32>(WaterFieldFlag::Excluded);

  const WaterSample impulse = setup_.interaction.sample(position, timeSeconds);
  const WaterVec2 impulseSlope = slopeOf(impulse.normal);

  if (status_.resolved == WaterFieldProvider::SpectralCpu) {
    const WaterMirrorSample mirror = setup_.mirror->sample(position);
    result.height = setup_.baseHeight + mirror.height + impulse.height;
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
    result.height = setup_.baseHeight + spectrum.height + impulse.height;
    const WaterVec2 slope = slopeOf(spectrum.normal);
    result.normal = normalized({-(slope.x + impulseSlope.x), 1.0f, -(slope.y + impulseSlope.y)});
    result.velocity = {spectrum.velocity.x, spectrum.velocity.y + impulse.velocity.y,
                       spectrum.velocity.z};
    result.foam = std::clamp(spectrum.breaking + impulse.breaking, 0.0f, 1.0f);
    // The analytic profile displaces vertically only, so the horizontal mapping
    // is the identity and its determinant is exactly one.
    result.jacobian = 1.0f;
    result.flags |= static_cast<u32>(WaterFieldFlag::JacobianKnown);
  }

  if (setup_.hasBathymetry) {
    result.depth = setup_.baseHeight - setup_.bottomHeight;
    result.flags |= static_cast<u32>(WaterFieldFlag::DepthKnown);
  }
  return result;
}

bool WaterField::sample(std::span<const WaterVec2> positions, double timeSeconds,
                        std::span<WaterFieldSample> out) const noexcept {
  if (out.size() < positions.size()) return false;
  if (!status_.configured || !std::isfinite(timeSeconds)) return false;
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
