#pragma once
// Authoring and runtime share this evaluator. Time is seconds, slopes are
// value/second and weights are fractions of the adjacent segment's duration.
// References: Unity 6000.0 Keyframe / EditingCurves; Godot 4.5 Animation
// Bezier tracks. A weight changes the time coordinate of the handle, not just
// its slope. Neither IDs nor tangent modes depend on a visible editor window.
#include "core/base.h"
#include <span>
#include <vector>

namespace ae::resources {
enum class AnimationTangentMode : u8 {
  Free, Linear, Constant, Auto, ClampedAuto, Flat,
  // Exact time reversal of Constant. Necessary to preserve step curves;
  // reversing their key order alone produces a different animation.
  NextConstant
};
struct AnimationCurveKey {
  u64 id=0;
  float time=0,value=0,inSlope=0,outSlope=0,inWeight=1.f/3,outWeight=1.f/3;
  AnimationTangentMode incoming=AnimationTangentMode::ClampedAuto;
  AnimationTangentMode outgoing=AnimationTangentMode::ClampedAuto;
  bool broken=false,weightedIn=false,weightedOut=false;
};
struct AnimationCurve {
  static constexpr usize MaximumKeys=262144;
  std::vector<AnimationCurveKey> keys;
};
struct AnimationCurveSample { double value=0,derivative=0; };
bool validAnimationCurve(const AnimationCurve &curve);
double animationCurveSlope(const AnimationCurve &curve,usize index,bool incoming);
// The checked entry point is for authoring / arbitrary API input. The unchecked
// evaluator is only for immutable data validated when published to the runtime.
bool sampleAnimationCurve(const AnimationCurve &curve,double time,AnimationCurveSample &out);
bool sampleValidatedAnimationCurve(const AnimationCurve &curve,double time,AnimationCurveSample &out);
// Exact stationary values of each cubic Bezier segment, expressed as clip
// times. Used by bake to avoid hiding overshoots/turns between sample frames.
bool animationCurveExtremaTimes(const AnimationCurve &curve,std::vector<float> &times);
// Operations are transactional: failure leaves the curve and ID frontier intact.
bool putAnimationCurveKey(AnimationCurve &curve,AnimationCurveKey key,u64 &nextId,u64 &resultId);
bool eraseAnimationCurveKeys(AnimationCurve &curve,std::span<const u64> ids);
bool retimeAnimationCurve(AnimationCurve &curve,double scale,double offset);
bool reverseAnimationCurve(AnimationCurve &curve,double start,double end);
// Inserts a key without changing the curve, including weighted Bezier handles.
// Linear quaternion groups must be split as a group by the clip authoring layer.
bool splitAnimationCurve(AnimationCurve &curve,float time,u64 &nextId,u64 &resultId);
bool cropAnimationCurve(AnimationCurve &curve,float start,float end,u64 &nextId);
} // namespace ae::resources
