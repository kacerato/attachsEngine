#pragma once
#include "core/base.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::runtime {
// Session data, never authoring data. Snapshot the scale before dispatching
// Update so writes made by a script affect the next frame, including physics.
// Unity 6000.0: Time.timeScale, Time.unscaledDeltaTime, Time.maximumDeltaTime.
class SimulationClock final {
public:
  static constexpr double maximumDelta = .25;
  static constexpr float maximumScale = 4;
  void reset() noexcept { *this = {}; }
  bool setScale(float value) noexcept {
    if(!std::isfinite(value) || value<0 || value>maximumScale) return false;
    scale_=value;return true;
  }
  bool beginFrame(double elapsed,bool editorStep=false) noexcept {
    if(!std::isfinite(elapsed) || elapsed<0 || elapsed>std::numeric_limits<float>::max() ||
       !std::isfinite(unscaledTime_+elapsed) || frame_==std::numeric_limits<u64>::max()) return false;
    frameScale_=editorStep?1.f:scale_;
    delta_=static_cast<float>(std::min(elapsed*frameScale_,maximumDelta));
    unscaledDelta_=static_cast<float>(elapsed);
    unscaledTime_+=elapsed;++frame_;return true;
  }
  float scale() const noexcept {return scale_;}
  float frameScale() const noexcept {return frameScale_;}
  float delta() const noexcept {return delta_;}
  float unscaledDelta() const noexcept {return unscaledDelta_;}
  double unscaledTime() const noexcept {return unscaledTime_;}
  u64 frameCount() const noexcept {return frame_;}
private:
  double unscaledTime_=0;
  u64 frame_=0;
  float scale_=1,frameScale_=1,delta_=0,unscaledDelta_=0;
};
} // namespace ae::runtime
