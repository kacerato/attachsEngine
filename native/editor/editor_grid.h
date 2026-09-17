#pragma once
#include "editor/editor_view.h"
#include "renderer/grid_plan.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
struct EditorGridSettings {
  float planeHeight = 0;
  float desiredCellPixels = 32;
  float minimumSpacing = .001f;
  float maximumSpacing = 1000000;
  // A grade termina de sumir a este múltiplo da distância observada. Um valor
  // pequeno demais corta o chão perto do horizonte; um grande demais devolve a
  // faixa branca de linhas acumuladas.
  float fadeDistanceFactor = 24;
};

// A POLÍTICA da grade: qual célula usar para a distância que a câmera observa,
// quanto a célula fina ainda pesa e onde a grade some. A coverage e a
// profundidade são do fragmento (ver renderer/grid_plan.h); aqui não se monta
// nenhuma lista de segmentos.
inline renderer::GridPlan buildEditorGridPlan(const EditorViewport &view,
                                              const EditorGridSettings &settings = {}) noexcept {
  renderer::GridPlan plan;
  if (!isViewportValid(view) || !std::isfinite(settings.planeHeight) ||
      !std::isfinite(settings.desiredCellPixels) || settings.desiredCellPixels <= 0 ||
      !std::isfinite(settings.minimumSpacing) || settings.minimumSpacing <= 0 ||
      !std::isfinite(settings.maximumSpacing) || settings.maximumSpacing < settings.minimumSpacing ||
      !std::isfinite(settings.fadeDistanceFactor) || settings.fadeDistanceFactor <= 0)
    return plan;
  const auto ray = screenPointToRay(view, {view.rect.x + view.rect.width * .5f,
                                           view.rect.y + view.rect.height * .5f});
  if (!ray.valid) return plan;
  const float altitude = std::abs(ray.origin[1] - settings.planeHeight);
  float depth = std::max(altitude, view.frustum.nearPlane * 10);
  // Limite contínuo perto do horizonte: evita trocar abruptamente entre a
  // interseção distante e a posição da câmera ao atravessar um limiar.
  const float facing = (settings.planeHeight - ray.origin[1]) * ray.direction[1];
  if (facing > 0) depth = std::max(depth, altitude / std::max(std::abs(ray.direction[1]), .1f));

  const float wanted = std::clamp(2 * renderer::projectionDivisor(view.frustum,depth) * renderer::projectionHalfHeight(view.frustum) *
                                      settings.desiredCellPixels / view.rect.height,
                                  settings.minimumSpacing, settings.maximumSpacing);
  plan.enabled = true;
  plan.planeHeight = settings.planeHeight;
  plan.minorSpacing = std::clamp(std::pow(10.0f, std::floor(std::log10(wanted))),
                                 settings.minimumSpacing, settings.maximumSpacing);
  plan.majorSpacing = std::min(plan.minorSpacing * 10, settings.maximumSpacing);
  // Mistura contínua dentro da década: a célula fina perde peso conforme a
  // observação se afasta, e chega a zero exatamente quando a próxima década
  // assume. Trocar as duas de uma vez é o que produzia o piscar no zoom.
  plan.minorOpacity = std::clamp((10 - wanted / plan.minorSpacing) / 9, 0.0f, 1.0f);
  plan.fadeDistance = depth * settings.fadeDistanceFactor;
  return plan;
}
} // namespace ae::editor
