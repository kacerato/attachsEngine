#pragma once
// Janela do editor de curvas: qual faixa de tempo × valor aparece no gráfico.
// A tela desenha e a sessão interpreta o toque pelas MESMAS funções, então a
// chave que o dedo vê é a chave que o dedo pega.
#include "scene/script_curve.h"
#include "ui/ui_geometry.h"

#include <algorithm>

namespace ae::editor {

// view = {tempo mínimo, valor mínimo, tempo máximo, valor máximo}.
inline ui::UiPoint curveToScreen(const ui::UiRect &graph, const float (&view)[4], float time, float value) {
  const float tw = std::max(1e-6f, view[2] - view[0]), vh = std::max(1e-6f, view[3] - view[1]);
  return {graph.x + (time - view[0]) / tw * graph.width, graph.bottom() - (value - view[1]) / vh * graph.height};
}
inline void screenToCurve(const ui::UiRect &graph, const float (&view)[4], ui::UiPoint point, float &time, float &value) {
  const float tw = view[2] - view[0], vh = view[3] - view[1];
  time = view[0] + (graph.width > 0 ? (point.x - graph.x) / graph.width : 0) * tw;
  value = view[1] + (graph.height > 0 ? (graph.bottom() - point.y) / graph.height : 0) * vh;
}
// "Enquadrar" (tecla F da Unity): todas as chaves com folga; sem chaves, o
// quadrado 0–1.
inline void frameCurve(const scene::ScriptCurve &curve, float (&view)[4]) {
  if (curve.keys.empty()) { view[0] = 0; view[1] = 0; view[2] = 1; view[3] = 1; return; }
  float t0 = curve.keys.front().time, t1 = curve.keys.back().time, v0 = curve.keys.front().value, v1 = v0;
  // Amostra também entre as chaves: a Hermite pode passar dos valores delas.
  for (u32 i = 0; i <= 64; ++i) {
    const float v = scene::evaluateScriptCurve(curve, t0 + (t1 - t0) * i / 64.f);
    v0 = std::min(v0, v);
    v1 = std::max(v1, v);
  }
  if (t1 - t0 < 1e-3f) { t0 -= .5f; t1 += .5f; }
  if (v1 - v0 < 1e-3f) { v0 -= .5f; v1 += .5f; }
  const float tp = (t1 - t0) * .08f, vp = (v1 - v0) * .12f;
  view[0] = t0 - tp; view[2] = t1 + tp; view[1] = v0 - vp; view[3] = v1 + vp;
}
// Ponta de uma alça de tangente, a 60 dp da chave na direção da inclinação
// (em tela), para o dedo pegar independente do zoom.
inline ui::UiPoint curveHandle(const ui::UiRect &graph, const float (&view)[4], const scene::CurveKey &key, bool incoming) {
  const auto origin = curveToScreen(graph, view, key.time, key.value);
  const float slope = incoming ? key.in : key.out;
  const auto ahead = curveToScreen(graph, view, key.time + 1, key.value + slope);
  float dx = ahead.x - origin.x, dy = ahead.y - origin.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length < 1e-4f) { dx = 1; dy = 0; } else { dx /= length; dy /= length; }
  const float sign = incoming ? -1.f : 1.f;
  return {origin.x + sign * dx * 60.f, origin.y + sign * dy * 60.f};
}

} // namespace ae::editor
