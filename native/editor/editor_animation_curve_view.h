#pragma once
// View transforms never modify the resource or its sampler. Bounds are rebuilt
// only on explicit framing / channel selection, so a drag cannot rescale itself.
#include "editor/editor_screen.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::editor {
inline EditorScreenState::ClipCurveRange animationCurveViewRange(
    const resources::AnimationClipTrack &track,u32 component,double first,double last) {
  EditorScreenState::ClipCurveRange result{track.id,component,0,1};
  if(component>=track.curves.size())return result;
  const auto &curve=track.curves[component];
  double low=std::numeric_limits<double>::infinity(),high=-low;
  const auto include=[&](double time) {
    resources::AnimationCurveSample sample;
    if(resources::sampleValidatedAnimationCurve(curve,time,sample)) {low=std::min(low,sample.value);high=std::max(high,sample.value);}
  };
  include(first);include(last);
  for(const auto &key:curve.keys)if(key.time>=first&&key.time<=last) {low=std::min(low,double(key.value));high=std::max(high,double(key.value));}
  std::vector<float> extrema;
  if(resources::animationCurveExtremaTimes(curve,extrema))for(const float time:extrema)if(time>=first&&time<=last)include(time);
  if(!std::isfinite(low)||!std::isfinite(high))return result;
  if(high-low<1e-12) {const double pad=std::max(.5,std::abs(low)*1e-6);low-=pad;high+=pad;}
  result.minimum=low;result.maximum=high;return result;
}
inline const EditorScreenState::ClipCurveRange *animationCurveViewRange(const EditorScreenState &state,u64 track,u32 component) {
  for(const auto &range:state.clipCurveRanges)if(range.track==track&&range.component==component)return &range;
  return nullptr;
}
inline double animationCurveViewValue(const EditorScreenState &state,u64 track,u32 component,double value) {
  if(state.clipCurveNormalized)if(const auto *range=animationCurveViewRange(state,track,component))return (value-range->minimum)/(range->maximum-range->minimum);
  return value;
}
inline double animationCurveAuthorValue(const EditorScreenState &state,u64 track,u32 component,double value) {
  if(state.clipCurveNormalized)if(const auto *range=animationCurveViewRange(state,track,component))return range->minimum+value*(range->maximum-range->minimum);
  return value;
}
inline double animationCurveAuthorSpan(const EditorScreenState &state) {
  const double span=double(state.clipMaximum)-state.clipMinimum;
  if(state.clipCurveNormalized)if(const auto *range=animationCurveViewRange(state,state.clipTrack,state.clipComponent))return span*(range->maximum-range->minimum);
  return span;
}
} // namespace ae::editor
