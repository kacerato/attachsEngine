#pragma once

#include "renderer/rendering_policy.h"

namespace ae::renderer {

// Amostra portátil. O adaptador de plataforma decide como status/headroom são
// obtidos; a política não depende de Android nem de uma API específica.
struct ThermalObservation final {
  i32 status = -1;
  float headroom = 0.0f;
  bool statusValid = false;
  bool headroomValid = false;
};

struct ThermalPressureSettings final {
  float lightHeadroom = 0.85f;
  float severeHeadroom = 1.0f;
  float recoveryHeadroom = 0.70f;
  // O monitor Android amostra a cada 10 s: três amostras impedem recuperar
  // qualidade durante uma queda curta de temperatura ou frequência.
  u32 recoverySamples = 3;
};

struct ThermalPressureUpdate final {
  ThermalPressure pressure = ThermalPressure::None;
  bool changed = false;
};

// Piora imediatamente; recupera apenas um nível por streak sustentada. Sem
// amostra válida, mantém o estado anterior em vez de afirmar que o aparelho
// esfriou. Single-thread owned, sem heap e sem relógio interno.
class ThermalPressureController final {
public:
  void reset(const ThermalPressureSettings &settings = {});
  ThermalPressureUpdate observe(const ThermalObservation &observation);

  ThermalPressure pressure() const { return pressure_; }
  const ThermalPressureSettings &settings() const { return settings_; }

private:
  ThermalPressureSettings settings_{};
  ThermalPressure pressure_ = ThermalPressure::None;
  u32 recoveryStreak_ = 0;
};

const char *thermalPressureName(ThermalPressure pressure);

} // namespace ae::renderer
