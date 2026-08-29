#pragma once

#include "core/base.h"

#include <algorithm>
#include <cmath>

namespace ae {

// Contrato portátil de cadência. Android/iOS escolhem a apresentação usando
// este mesmo orçamento; gameplay/física continuam num fixed tick independente.
struct FrameBudget final {
  u32 renderHz = 60;
  u32 simulationHz = 60;
  float frameIntervalMs = 1000.0f / 60.0f;
  float cpuLaneBudgetMs = 13.0f;
  float gpuLaneBudgetMs = 14.666667f;
  float compositorReserveMs = 1.666667f;
};

// Seleciona somente cadências suportadas pelo contrato global da engine. Uma
// tela intermediária (por exemplo 75 Hz) usa 60; nunca solicita uma taxa acima
// do painel/cap informado nem reduz qualidade gráfica para alcançar o budget.
inline FrameBudget makeFrameBudget(float displayHz, float requestedMaximumHz,
                                   u32 simulationHz = 60) noexcept {
  const float safeDisplay = std::isfinite(displayHz) && displayHz >= 1.0f ? displayHz : 60.0f;
  const float safeMaximum = std::isfinite(requestedMaximumHz) && requestedMaximumHz >= 1.0f
                                ? requestedMaximumHz : 60.0f;
  const float available = std::min(safeDisplay, safeMaximum);
  const u32 renderHz = available >= 119.0f ? 120u
                       : available >= 89.0f ? 90u
                       : available >= 59.0f ? 60u
                       : available >= 29.0f ? 30u
                       : std::max(1u, static_cast<u32>(available));
  const float interval = 1000.0f / static_cast<float>(renderHz);
  return {renderHz, std::max(1u, simulationHz), interval,
          interval * 0.78f, interval * 0.88f, interval * 0.10f};
}

} // namespace ae
