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


}
