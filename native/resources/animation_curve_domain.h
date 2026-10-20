#pragma once
#include "resources/animation_curve.h"
namespace ae::resources {
// Geo authoring may retain keys outside its sampled 0..1 interval. Existing
// animation APIs continue to enforce NonNegative time and are unchanged.
enum class AnimationCurveDomain : u8 { NonNegative, Signed };
// Empty curves are not evaluable here. A consumer such as Geo may explicitly
// define empty as constant zero; a single key is already a constant curve.
bool validAnimationCurveDomain(const AnimationCurve &,AnimationCurveDomain);
bool putAnimationCurveKeyDomain(AnimationCurve &,AnimationCurveKey,u64 &nextId,u64 &resultId,AnimationCurveDomain);
bool eraseAnimationCurveKeysDomain(AnimationCurve &,std::span<const u64>,AnimationCurveDomain);
bool splitAnimationCurveDomain(AnimationCurve &,float time,u64 &nextId,u64 &resultId,AnimationCurveDomain);
} // namespace ae::resources
