#pragma once
#include <cmath>
namespace ae::renderer {
struct EnvironmentLighting final {
  float sunDirectionIntensity[4]{};
  float sunColorAngularRadius[4]{};
  float ambientColorStrength[4]{};
  float parameters[4]{}; // exposure, rotation, maximum environment LOD, reserved
  // AEEN v2 serializa estes parâmetros no Environment Resource global. O
  // decoder mantém migração explícita para projetos AEEN v1.
  float skyZenithCloudCoverage[4]{}; // rgb linear, cobertura 0..1
  float skyHorizonCloudDensity[4]{}; // rgb linear, densidade 0..1
  float groundColorSaturation[4]{};  // rgb linear, saturação global
  float cloudLightWindSpeed[4]{};    // rgb linear, velocidade angular
};
static_assert(sizeof(EnvironmentLighting) == 128);

inline bool adjustEnvironmentLighting(const EnvironmentLighting &source,const float values[4],EnvironmentLighting &out) {
  if(!values) return false;
  for(unsigned i=0;i<4;++i) if(!std::isfinite(values[i])) return false;
  if(values[0]<0 || values[1]<0 || values[2]<=0) return false;
  auto result=source;
  result.sunDirectionIntensity[3]*=values[0];result.ambientColorStrength[3]*=values[1];
  result.parameters[0]*=values[2];result.parameters[1]+=values[3]*0.0174532925199433f;
  out=result;return true;
}
}
