#pragma once
#include <algorithm>
#include <cmath>
namespace ae::renderer {
struct WaterFoamSettings final {
  float compressionThreshold=.8f;
  float growth=4;
  float decay=.5f;
};
inline bool validateWaterFoam(const WaterFoamSettings &s) noexcept {
  return std::isfinite(s.compressionThreshold) && s.compressionThreshold>=0 && s.compressionThreshold<=2 &&
    std::isfinite(s.growth) && s.growth>=0 && s.growth<=100 &&
    std::isfinite(s.decay) && s.decay>=0 && s.decay<=100;
}
// Exact solution of dF/dt = source*(1-F)-decay*F for constant compression.
// Independent of update rate; invalid input preserves the previous value.
inline float evolveWaterFoam(float previous,float jacobian,float dt,const WaterFoamSettings &s) noexcept {
  if(!validateWaterFoam(s) || !std::isfinite(previous) || !std::isfinite(jacobian) ||
      !std::isfinite(dt) || dt<0) return previous;
  previous=std::clamp(previous,0.0f,1.0f);
  const float source=s.growth*std::clamp(s.compressionThreshold-jacobian,0.0f,1.0f);
  const float rate=source+s.decay;
  if(rate==0) return previous;
  const float equilibrium=source/rate;
  return std::clamp(equilibrium+(previous-equilibrium)*std::exp(-rate*dt),0.0f,1.0f);
}
}
