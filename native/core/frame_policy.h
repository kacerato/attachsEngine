#pragma once

#include "core/base.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ae {

// O runtime usa a maior cadência móvel oficialmente suportada pela política
// quando o projeto ainda não possui uma preferência serializada. A capacidade
// real do display continua sendo a autoridade que reduz 120 para 90/60.
inline constexpr u32 DefaultMaximumRenderHz = 120;

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

// Traduz a cadência portátil para o alvo de trabalho informado ao Android
// Performance Hint. O alvo padrão é o intervalo de apresentação: o A/B físico
// mostrou que usar a margem interna da trilha GPU como alvo ADPF pode piorar a
// seleção de operating point do OEM. targetRatioOverride permanece somente
// para profiling no mesmo APK; zero, NaN e valores fora da faixa segura herdam
// o intervalo completo.
inline i64 performanceHintTargetNanoseconds(const FrameBudget &budget,
                                            float targetRatioOverride = 0.0f) noexcept {
  const bool validOverride = std::isfinite(targetRatioOverride) &&
                             targetRatioOverride >= 0.5f &&
                             targetRatioOverride <= 1.0f;
  const float targetMs = validOverride
                             ? budget.frameIntervalMs * targetRatioOverride
                             : budget.frameIntervalMs;
  return std::max<i64>(1, static_cast<i64>(targetMs * 1'000'000.0f + 0.5f));
}

// Android 15's detailed ADPF report treats CPU and GPU as components of the
// same frame workload. Reporting only max(CPU,GPU) hides render-thread work
// from the governor and can select an operating point below the requested
// cadence. Keep the sum saturating because it crosses a platform ABI.
inline i64 performanceHintActualTotalNanoseconds(i64 cpuNanoseconds,
                                                 i64 gpuNanoseconds) noexcept {
  const i64 cpu = std::max<i64>(cpuNanoseconds, 0);
  const i64 gpu = std::max<i64>(gpuNanoseconds, 0);
  if (cpu > std::numeric_limits<i64>::max() - gpu)
    return std::numeric_limits<i64>::max();
  return std::max<i64>(cpu + gpu, 1);
}

// Visibility-stage budgets (HZB hysteresis, and future LOD pixel-error/dither
// budgets -- see docs/adr/ADR-014-POLITICA-GLOBAL-RENDERIZACAO.md, "budgets
// like LOD max error, shadow cascades, AO samples... belong to global
// policy"). Deliberately its own sibling struct, not fields bolted onto
// FrameBudget: this covers what the visibility stage is allowed to do with a
// frame's time budget, not the frame's cadence itself. Populated in
// android_main.cpp from launch options, never hardcoded in the renderer.
struct VisibilityBudget final {
  // CPU-readback HZB has a fixed reduction/copy cost. Below this number of
  // opaque/coverage candidates, frustum + indirect submission is cheaper and
  // the HZB stage must fail open. This is a global workload budget, not a
  // scene-name exception. GPU-driven same-frame HZB may use a lower default.
  u32 hzbMinimumCandidateDraws = 128;
  // Consecutive occluded frames an HZB-tested object must accumulate before
  // it is actually dropped from the draw list (grace period against a stale
  // Hi-Z sample flickering an object out for one frame). See
  // renderer::updateHzbHysteresis for the exact contract; 0 means no grace
  // period at all.
  u32 hzbHysteresisFrames = 3;
  // Extra normalized Vulkan depth separation required before HZB may cull a
  // candidate. This absorbs depth quantization/coplanar precision error and
  // belongs to the global visibility policy, never to a particular scene.
  float hzbNormalizedDepthBias = 1.0e-5f;
  // Projected geometric error allowed for an imported LOD level, in pixels.
  float lodPixelErrorBudget = 2.0f;
  // Alpha-tested coverage (foliage, grass and card clusters) has a distinct
  // perceptual/cost curve from closed opaque meshes. Its importer error is
  // measured in removed coverage-cluster radius, so sharing the opaque budget
  // would keep distant vegetation at LOD0 almost indefinitely.
  float coverageLodPixelErrorBudget = 32.0f;
  // A coarser LOD must fit this fraction of the full pixel budget before
  // switching, preventing oscillation at the boundary.
  float lodHysteresisBandRatio = 0.75f;
};

} // namespace ae
