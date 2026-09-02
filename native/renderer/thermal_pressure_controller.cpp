#include "renderer/thermal_pressure_controller.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
u32 rank(ThermalPressure pressure) { return static_cast<u32>(pressure); }
} // namespace

void ThermalPressureController::reset(const ThermalPressureSettings &settings) {
  settings_ = settings;
  settings_.lightHeadroom = std::clamp(settings_.lightHeadroom, 0.1f, 1.0f);
  settings_.severeHeadroom =
      std::clamp(settings_.severeHeadroom, settings_.lightHeadroom, 2.0f);
  settings_.recoveryHeadroom =
      std::clamp(settings_.recoveryHeadroom, 0.0f, settings_.lightHeadroom - 0.05f);
  settings_.recoverySamples = std::max(1u, settings_.recoverySamples);
  pressure_ = ThermalPressure::None;
  recoveryStreak_ = 0;
}

ThermalPressureUpdate ThermalPressureController::observe(
    const ThermalObservation &observation) {
  const bool statusValid = observation.statusValid && observation.status >= 0;
  const bool headroomValid = observation.headroomValid &&
                             std::isfinite(observation.headroom) &&
                             observation.headroom >= 0.0f;
  if (!statusValid && !headroomValid) return {pressure_, false};

  ThermalPressure observed = ThermalPressure::None;
  if ((statusValid && observation.status >= 3) ||
      (headroomValid && observation.headroom >= settings_.severeHeadroom)) {
    observed = ThermalPressure::Severe;
  } else if ((statusValid && observation.status >= 1) ||
             (headroomValid && observation.headroom >= settings_.lightHeadroom)) {
    observed = ThermalPressure::Light;
  }

  if (rank(observed) > rank(pressure_)) {
    pressure_ = observed;
    recoveryStreak_ = 0;
    return {pressure_, true};
  }
  if (rank(observed) == rank(pressure_)) {
    recoveryStreak_ = 0;
    return {pressure_, false};
  }

  // Status NONE isolado não prova recuperação em devices cujo Thermal Status
  // nunca muda. Quando headroom existe, ele é a autoridade para recuperar.
  const bool coolEnough = headroomValid
                              ? observation.headroom <= settings_.recoveryHeadroom &&
                                    (!statusValid || observation.status == 0)
                              : statusValid && observation.status == 0;
  if (!coolEnough) {
    recoveryStreak_ = 0;
    return {pressure_, false};
  }
  if (recoveryStreak_ < settings_.recoverySamples) ++recoveryStreak_;
  if (recoveryStreak_ < settings_.recoverySamples) return {pressure_, false};

  pressure_ = pressure_ == ThermalPressure::Severe ? ThermalPressure::Light
                                                    : ThermalPressure::None;
  recoveryStreak_ = 0;
  return {pressure_, true};
}

const char *thermalPressureName(ThermalPressure pressure) {
  switch (pressure) {
    case ThermalPressure::Light: return "light";
    case ThermalPressure::Severe: return "severe";
    default: return "none";
  }
}

} // namespace ae::renderer
