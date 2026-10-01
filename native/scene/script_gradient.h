#pragma once
// Gradiente de campo de script (Unity 6000.0 ScriptReference/Gradient e Manual/
// InspectorColorPicker — Gradient Editor). Paradas de cor e de alfa são
// independentes, de 1 a 8 cada, com tempo em [0,1]; o modo decide a
// interpolação: Blend (linear em RGB linear), Perceptual (em Oklab, como o
// "Perceptual blend" da Unity) e Fixed (sem interpolação, degraus).
//
// Texto: "<modo> <nCor> t r g b … <nAlfa> t a …", cor em RGB linear. É o
// valor autoral do campo e também o das bibliotecas de presets.
#include "core/base.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ae::scene {

enum class GradientMode : u32 { Blend = 0, Fixed = 1, Perceptual = 2 };

struct GradientColorKey { float time = 0; float rgb[3]{1, 1, 1}; };
struct GradientAlphaKey { float time = 0; float alpha = 1; };

struct ScriptGradient {
  static constexpr usize kMaximumKeys = 8;
  GradientMode mode = GradientMode::Blend;
  std::vector<GradientColorKey> colors{{0, {1, 1, 1}}, {1, {1, 1, 1}}};
  std::vector<GradientAlphaKey> alphas{{0, 1}, {1, 1}};

  void sort() {
    std::stable_sort(colors.begin(), colors.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
    std::stable_sort(alphas.begin(), alphas.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
  }
  bool valid(bool hdr) const {
    if (static_cast<u32>(mode)>2 || colors.empty() || alphas.empty() || colors.size() > kMaximumKeys || alphas.size() > kMaximumKeys) return false;
    for(usize i=1;i<colors.size();++i)if(colors[i-1].time>colors[i].time)return false;
    for(usize i=1;i<alphas.size();++i)if(alphas[i-1].time>alphas[i].time)return false;
    for (const auto &key : colors) {
      if (!std::isfinite(key.time) || key.time < 0 || key.time > 1) return false;
      for (const float c : key.rgb) if (!std::isfinite(c) || c < 0 || c > (hdr ? 65504.f : 1.f)) return false;
    }
    for (const auto &key : alphas)
      if (!std::isfinite(key.time) || key.time < 0 || key.time > 1 || !std::isfinite(key.alpha) || key.alpha < 0 || key.alpha > 1)
        return false;
    return true;
  }
};

inline bool scriptGradientType(std::string_view type, bool *hdr = nullptr) {
  if (type == "gradient") { if (hdr) *hdr = false; return true; }
  if (type == "gradient:hdr") { if (hdr) *hdr = true; return true; }
  return false;
}

inline bool parseScriptGradient(std::string_view text, ScriptGradient &out) {
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  u32 mode = 0, colors = 0, alphas = 0;
  ScriptGradient value;
  if (!(in >> mode >> colors) || mode > 2 || colors == 0 || colors > ScriptGradient::kMaximumKeys) return false;
  value.mode = static_cast<GradientMode>(mode);
  value.colors.assign(colors, {});
  for (auto &key : value.colors) if (!(in >> key.time >> key.rgb[0] >> key.rgb[1] >> key.rgb[2])) return false;
  if (!(in >> alphas) || alphas == 0 || alphas > ScriptGradient::kMaximumKeys) return false;
  value.alphas.assign(alphas, {});
  for (auto &key : value.alphas) if (!(in >> key.time >> key.alpha)) return false;
  in >> std::ws;
  if (!in.eof()) return false;
  value.sort();
  if(!value.valid(true))return false;
  out = std::move(value);
  return true;
}

inline std::string scriptGradientValue(ScriptGradient value) {
  value.sort();
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10) << static_cast<u32>(value.mode) << ' ' << value.colors.size();
  for (const auto &key : value.colors) out << ' ' << key.time << ' ' << key.rgb[0] << ' ' << key.rgb[1] << ' ' << key.rgb[2];
  out << ' ' << value.alphas.size();
  for (const auto &key : value.alphas) out << ' ' << key.time << ' ' << key.alpha;
  return out.str();
}

namespace detail {
// Oklab (Björn Ottosson): RGB linear ↔ espaço perceptual usado pelo blend.
inline void linearToOklab(const float (&c)[3], float (&lab)[3]) {
  const float l = std::cbrt(0.4122214708f * c[0] + 0.5363325363f * c[1] + 0.0514459929f * c[2]);
  const float m = std::cbrt(0.2119034982f * c[0] + 0.6806995451f * c[1] + 0.1073969566f * c[2]);
  const float s = std::cbrt(0.0883024619f * c[0] + 0.2817188376f * c[1] + 0.6299787005f * c[2]);
  lab[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
  lab[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
  lab[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}
inline void oklabToLinear(const float (&lab)[3], float (&c)[3]) {
  const float l = std::pow(lab[0] + 0.3963377774f * lab[1] + 0.2158037573f * lab[2], 3.f);
  const float m = std::pow(lab[0] - 0.1055613458f * lab[1] - 0.0638541728f * lab[2], 3.f);
  const float s = std::pow(lab[0] - 0.0894841775f * lab[1] - 1.2914855480f * lab[2], 3.f);
  c[0] = std::max(0.f, 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s);
  c[1] = std::max(0.f, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s);
  c[2] = std::max(0.f, -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}
} // namespace detail

// Unity Gradient.Evaluate: antes da primeira parada vale a primeira, depois da
// última vale a última; Fixed usa a parada cujo tempo é o primeiro ≥ t.
inline bool tryEvaluateScriptGradient(const ScriptGradient &g, float t, float (&rgba)[4]) {
  if(!g.valid(true) || !std::isfinite(t))return false;
  t = std::clamp(t, 0.f, 1.f);
  const auto &colors = g.colors;
  if (colors.size() == 1 || t <= colors.front().time) std::copy(colors.front().rgb, colors.front().rgb + 3, rgba);
  else if (t >= colors.back().time) std::copy(colors.back().rgb, colors.back().rgb + 3, rgba);
  else {
    usize i = 1;
    while (i < colors.size() && colors[i].time < t) ++i;
    const auto &a = colors[i - 1], &b = colors[i];
    if (g.mode == GradientMode::Fixed) std::copy(b.rgb, b.rgb + 3, rgba);
    else {
      const float span = b.time - a.time, f = span > 0 ? (t - a.time) / span : 1.f;
      if (g.mode == GradientMode::Perceptual) {
        float la[3], lb[3], mix[3], out[3];
        detail::linearToOklab(a.rgb, la);
        detail::linearToOklab(b.rgb, lb);
        for (u32 k = 0; k < 3; ++k) mix[k] = la[k] + (lb[k] - la[k]) * f;
        detail::oklabToLinear(mix, out);
        std::copy(out, out + 3, rgba);
      } else for (u32 k = 0; k < 3; ++k) rgba[k] = a.rgb[k] + (b.rgb[k] - a.rgb[k]) * f;
    }
  }
  const auto &alphas = g.alphas;
  if (alphas.size() == 1 || t <= alphas.front().time) rgba[3] = alphas.front().alpha;
  else if (t >= alphas.back().time) rgba[3] = alphas.back().alpha;
  else {
    usize i = 1;
    while (i < alphas.size() && alphas[i].time < t) ++i;
    const auto &a = alphas[i - 1], &b = alphas[i];
    const float span = b.time - a.time, f = span > 0 ? (t - a.time) / span : 1.f;
    rgba[3] = g.mode == GradientMode::Fixed ? b.alpha : a.alpha + (b.alpha - a.alpha) * f;
  }
  return true;
}
inline void evaluateScriptGradient(const ScriptGradient &g,float t,float (&rgba)[4]) {
  if(!tryEvaluateScriptGradient(g,t,rgba))std::fill(std::begin(rgba),std::end(rgba),std::numeric_limits<float>::quiet_NaN());
}

} // namespace ae::scene
