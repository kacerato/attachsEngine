#pragma once
// Curva de campo de script (Unity 6000.0 ScriptReference/AnimationCurve,
// Keyframe, WrapMode e Manual/EditingCurves). Chaves com tempo, valor e
// tangentes de entrada e saída; cada lado tem um modo (Free, Auto, Linear,
// Constant, ClampedAuto) e a chave pode estar "quebrada" (lados
// independentes). Entre chaves a curva é Hermite; Constant segura o valor da
// chave até a próxima. Antes da primeira e depois da última chave vale o modo
// de repetição: Clamp, Loop ou PingPong.
//
// Texto: "<pré> <pós> <n> t v entrada saída esquerda direita quebrada …".
// É o valor autoral do campo e o dos presets de curva.
#include "core/base.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ae::scene {

enum class CurveTangentMode : u32 { Free = 0, Auto = 1, Linear = 2, Constant = 3, ClampedAuto = 4 };
enum class CurveWrapMode : u32 { Clamp = 0, Loop = 1, PingPong = 2 };

struct CurveKey {
  float time = 0, value = 0, in = 0, out = 0;
  CurveTangentMode left = CurveTangentMode::ClampedAuto, right = CurveTangentMode::ClampedAuto;
  bool broken = false;
};

struct ScriptCurve {
  static constexpr usize kMaximumKeys = 256;
  CurveWrapMode pre = CurveWrapMode::Clamp, post = CurveWrapMode::Clamp;
  std::vector<CurveKey> keys;

  void sort() {
    std::stable_sort(keys.begin(), keys.end(), [](const CurveKey &a, const CurveKey &b) { return a.time < b.time; });
  }
  bool valid() const {
    if (keys.size() > kMaximumKeys) return false;
    for (usize i = 0; i < keys.size(); ++i) {
      const auto &k = keys[i];
      if (!std::isfinite(k.time) || !std::isfinite(k.value) || !std::isfinite(k.in) || !std::isfinite(k.out)) return false;
      if (static_cast<u32>(k.left) > 4 || static_cast<u32>(k.right) > 4) return false;
      if (i && keys[i - 1].time >= k.time) return false;  // tempos distintos e crescentes
    }
    return true;
  }
  // Unity AnimationUtility: Auto/ClampedAuto/Linear são recalculadas sempre que
  // a curva muda; Free e Constant guardam o que o autor pôs.
  void updateTangents() {
    const usize n = keys.size();
    for (usize i = 0; i < n; ++i) {
      auto &k = keys[i];
      const CurveKey *previous = i ? &keys[i - 1] : nullptr, *next = i + 1 < n ? &keys[i + 1] : nullptr;
      const float toPrevious = previous ? (k.value - previous->value) / std::max(1e-6f, k.time - previous->time) : 0;
      const float toNext = next ? (next->value - k.value) / std::max(1e-6f, next->time - k.time) : 0;
      float smooth = 0;
      if (previous && next) smooth = (next->value - previous->value) / std::max(1e-6f, next->time - previous->time);
      // ClampedAuto não passa do valor dos vizinhos: extremo local fica plano
      // e a inclinação é limitada para a Hermite não sobrar (Fritsch–Carlson).
      float clamped = 0;
      if (previous && next && toPrevious * toNext > 0) {
        clamped = smooth;
        const float limit = 3.f * std::min(std::abs(toPrevious), std::abs(toNext));
        if (std::abs(clamped) > limit) clamped = std::copysign(limit, clamped);
      }
      const auto side = [&](CurveTangentMode mode, bool incoming, float current) {
        switch (mode) {
          case CurveTangentMode::Auto: return smooth;
          case CurveTangentMode::ClampedAuto: return clamped;
          case CurveTangentMode::Linear: return incoming ? toPrevious : toNext;
          default: return current;
        }
      };
      k.in = side(k.left, true, k.in);
      k.out = side(k.right, false, k.out);
      // Lados alinhados (não quebrada) com modo livre: a saída acompanha a entrada.
      if (!k.broken && k.left == CurveTangentMode::Free && k.right == CurveTangentMode::Free) k.out = k.in;
    }
  }
};

inline bool scriptCurveType(std::string_view type) { return type == "curve"; }

inline bool parseScriptCurve(std::string_view text, ScriptCurve &out) {
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  u32 pre = 0, post = 0, count = 0;
  ScriptCurve curve;
  if (!(in >> pre >> post >> count) || pre > 2 || post > 2 || count > ScriptCurve::kMaximumKeys) return false;
  curve.pre = static_cast<CurveWrapMode>(pre);
  curve.post = static_cast<CurveWrapMode>(post);
  curve.keys.assign(count, {});
  for (auto &k : curve.keys) {
    u32 left = 0, right = 0, broken = 0;
    if (!(in >> k.time >> k.value >> k.in >> k.out >> left >> right >> broken) || left > 4 || right > 4 || broken > 1) return false;
    k.left = static_cast<CurveTangentMode>(left);
    k.right = static_cast<CurveTangentMode>(right);
    k.broken = broken != 0;
  }
  in >> std::ws;
  if (!in.eof() || !curve.valid()) return false;
  out = std::move(curve);
  return true;
}

inline std::string scriptCurveValue(const ScriptCurve &curve) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(7) << static_cast<u32>(curve.pre) << ' ' << static_cast<u32>(curve.post) << ' ' << curve.keys.size();
  for (const auto &k : curve.keys)
    out << ' ' << k.time << ' ' << k.value << ' ' << k.in << ' ' << k.out << ' ' << static_cast<u32>(k.left) << ' '
        << static_cast<u32>(k.right) << ' ' << (k.broken ? 1 : 0);
  return out.str();
}

// Unity AnimationCurve.Evaluate. Curva vazia vale 0; uma chave, o valor dela.
inline float evaluateScriptCurve(const ScriptCurve &curve, float time) {
  const auto &keys = curve.keys;
  if (keys.empty()) return 0;
  if (keys.size() == 1) return keys[0].value;
  const float start = keys.front().time, end = keys.back().time, length = end - start;
  if (time < start || time > end) {
    const auto mode = time < start ? curve.pre : curve.post;
    if (mode == CurveWrapMode::Clamp || length <= 0) time = std::clamp(time, start, end);
    else {
      float local = std::fmod(time - start, 2 * length);
      if (local < 0) local += 2 * length;
      if (mode == CurveWrapMode::Loop) local = std::fmod(local, length);
      else if (local > length) local = 2 * length - local;
      time = start + local;
    }
  }
  usize i = 1;
  while (i < keys.size() - 1 && keys[i].time < time) ++i;
  const auto &a = keys[i - 1], &b = keys[i];
  if (a.right == CurveTangentMode::Constant || b.left == CurveTangentMode::Constant) return time >= b.time ? b.value : a.value;
  const float dt = b.time - a.time;
  if (dt <= 0) return b.value;
  const float s = (time - a.time) / dt, s2 = s * s, s3 = s2 * s;
  const float h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
  return h00 * a.value + h10 * dt * a.out + h01 * b.value + h11 * dt * b.in;
}

// Presets de fábrica (Unity: "Add Factory Presets To Current Library").
struct CurveFactoryPreset { const char *name; const char *value; };
inline constexpr CurveFactoryPreset curveFactoryPresets[]{
  {"Constante", "0 0 2 0 1 0 0 0 0 0 1 1 0 0 0 0 0"},
  {"Linear", "0 0 2 0 0 1 1 2 2 0 1 1 1 1 2 2 0"},
  {"Suave (ease in-out)", "0 0 2 0 0 0 0 4 4 0 1 1 0 0 4 4 0"},
  {"Acelera (ease in)", "0 0 2 0 0 0 0 0 0 0 1 1 2 2 0 0 0"},
  {"Desacelera (ease out)", "0 0 2 0 0 2 2 0 0 0 1 1 0 0 0 0 0"},
  {"Degrau", "0 0 2 0 0 0 0 3 3 1 1 1 0 0 3 3 1"},
};

} // namespace ae::scene
