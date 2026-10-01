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
#include <limits>
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
    if (keys.size() > kMaximumKeys || static_cast<u32>(pre)>2 || static_cast<u32>(post)>2) return false;
    for (usize i = 0; i < keys.size(); ++i) {
      const auto &k = keys[i];
      if (!std::isfinite(k.time) || !std::isfinite(k.value) || !std::isfinite(k.in) || !std::isfinite(k.out)) return false;
      if (static_cast<u32>(k.left) > 4 || static_cast<u32>(k.right) > 4) return false;
      if (i && keys[i - 1].time >= k.time) return false;  // tempos distintos e crescentes
    }
    return true;
  }
  // Astra recalcula Auto/ClampedAuto/Linear a partir das chaves atuais.
  // Auto difere do modo legado Unity, que exige reseleção após editar a curva.
  // Free e Constant preservam a intenção explícita do autor.
  double tangent(usize i,bool incoming) const {
    const auto &k=keys[i];const auto mode=incoming?k.left:k.right;
    const auto *previous=i?&keys[i-1]:nullptr,*next=i+1<keys.size()?&keys[i+1]:nullptr;
    const double before=previous?(double(k.value)-previous->value)/(double(k.time)-previous->time):0;
    const double after=next?(double(next->value)-k.value)/(double(next->time)-k.time):0;
    const double smooth=previous&&next?(double(next->value)-previous->value)/(double(next->time)-previous->time):0;
    if(mode==CurveTangentMode::Linear)return incoming?before:after;
    if(mode==CurveTangentMode::Auto)return smooth;
    if(mode==CurveTangentMode::ClampedAuto)
      return before*after>0?std::copysign(std::min(std::abs(smooth),3*std::min(std::abs(before),std::abs(after))),smooth):0;
    if(!incoming && !k.broken && k.left==CurveTangentMode::Free && k.right==CurveTangentMode::Free)return k.in;
    return incoming?k.in:k.out;
  }
  void updateTangents() {
    if(!valid())return;
    const double limit=std::numeric_limits<float>::max();
    for(usize i=0;i<keys.size();++i) {
      const double incoming=tangent(i,true),outgoing=tangent(i,false);
      keys[i].in=static_cast<float>(std::clamp(incoming,-limit,limit));
      keys[i].out=static_cast<float>(std::clamp(outgoing,-limit,limit));
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
  out << std::setprecision(std::numeric_limits<float>::max_digits10) << static_cast<u32>(curve.pre) << ' ' << static_cast<u32>(curve.post) << ' ' << curve.keys.size();
  for (const auto &k : curve.keys)
    out << ' ' << k.time << ' ' << k.value << ' ' << k.in << ' ' << k.out << ' ' << static_cast<u32>(k.left) << ' '
        << static_cast<u32>(k.right) << ' ' << (k.broken ? 1 : 0);
  return out.str();
}

// Unity AnimationCurve.Evaluate. Curva vazia vale 0; uma chave, o valor dela.
inline bool tryEvaluateScriptCurve(const ScriptCurve &curve, float time,float &result) {
  if(!curve.valid() || !std::isfinite(time))return false;
  const auto &keys = curve.keys;
  if(keys.empty()){result=0;return true;}
  if(keys.size()==1){result=keys[0].value;return true;}
  double sample=time;
  const double start = keys.front().time, end = keys.back().time, length = end - start;
  if (sample < start || sample > end) {
    const auto mode = sample < start ? curve.pre : curve.post;
    if (mode == CurveWrapMode::Clamp) sample = std::clamp(sample, start, end);
    else {
      double local = std::fmod(sample - start, 2 * length);
      if (local < 0) local += 2 * length;
      if (mode == CurveWrapMode::Loop) local = std::fmod(local, length);
      else if (local > length) local = 2 * length - local;
      sample = start + local;
    }
  }
  usize i = 1;
  while (i < keys.size() - 1 && keys[i].time < sample) ++i;
  const auto &a = keys[i - 1], &b = keys[i];
  if(a.right==CurveTangentMode::Constant || b.left==CurveTangentMode::Constant){result=sample>=b.time?b.value:a.value;return true;}
  const double dt = double(b.time) - a.time;
  const double s = (sample - a.time) / dt, s2 = s * s, s3 = s2 * s;
  const double h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
  const double value=h00*a.value+h10*dt*curve.tangent(i-1,false)+h01*b.value+h11*dt*curve.tangent(i,true);
  const double limit=std::numeric_limits<float>::max();
  result=static_cast<float>(std::clamp(value,-limit,limit));return true;
}
inline float evaluateScriptCurve(const ScriptCurve &curve,float time) {
  float result=std::numeric_limits<float>::quiet_NaN();
  tryEvaluateScriptCurve(curve,time,result);return result;
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
