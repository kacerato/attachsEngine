#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace ae::resources {
// Transient pointer state, never authoring data. Persist the emitted stamps.
// Carry arc length across input segments instead of measuring a chord from the
// last stamp: subdividing the same polyline must not change brush exposure.
class TerrainStrokeSampler {
public:
  void reset(float spacing) {
    spacing_ = std::isfinite(spacing) ? std::max(.1, double(spacing)) : .1;
    disconnect();
  }
  void disconnect() { connected_ = false; carried_ = 0; }

  // emit returns false when the caller's gesture budget is exhausted. The
  // caller then discards the whole uncommitted gesture, including earlier hits.
  template<class Emit>
  bool sample(const std::array<float, 3> &point, bool volume, bool finish, Emit emit) {
    for (float value : point) if (!std::isfinite(value)) return false;
    if (!connected_) {
      previous_ = point;
      carried_ = 0;
      connected_ = true;
      if (!emit(point)) return false;
    } else {
      double delta[3];
      for (unsigned i = 0; i < 3; ++i) delta[i] = double(point[i]) - previous_[i];
      const double length = std::sqrt(delta[0]*delta[0] + delta[2]*delta[2] +
                                     (volume ? delta[1]*delta[1] : 0));
      double at = spacing_ - carried_;
      // Roundoff at a stamp boundary must not add a stamp on the next event.
      const double epsilon = spacing_ * 1e-6;
      while (length > 0 && at <= length + epsilon) {
        const double t = std::min(1., at / length);
        std::array<float, 3> stamp;
        for (unsigned i = 0; i < 3; ++i) stamp[i] = float(previous_[i] + delta[i]*t);
        if (!emit(stamp)) return false;
        at += spacing_;
      }
      carried_ = std::max(0., length - (at - spacing_));
      previous_ = point;
      // Preserve the existing endpoint policy, but use travelled arc length.
      if (finish && carried_ > spacing_ * .1 && !emit(point)) return false;
    }
    if (finish) disconnect();
    return true;
  }

private:
  std::array<float, 3> previous_{};
  double spacing_ = .1, carried_ = 0;
  bool connected_ = false;
};
} // namespace ae::resources
