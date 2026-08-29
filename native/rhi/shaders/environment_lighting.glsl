#include "tonemap.glsl"
layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
  // 9 SH (bands l=0,1,2) irradiance coefficients baked from the same sky
  // radiance function as the visible dome and environmentMap (see
  // tools/cook-procedural-sky.py); rgb used, w padding for std140. Basis
  // order matches sh_basis() there exactly.
  vec4 skyIrradianceSH[9];
} environment;
layout(set=1,binding=1) uniform sampler2D environmentMap;

vec2 environmentUv(vec3 direction) {
  direction=normalize(direction);
  float phi=atan(direction.z,direction.x)+environment.parameters.y;
  return vec2(fract(phi/(2.0*PI)+.5),acos(clamp(direction.y,-1.0,1.0))/PI);
}

vec3 environmentRadiance(vec3 direction,float lod) {
  return textureLod(environmentMap,environmentUv(direction),clamp(lod,0.0,environment.parameters.z)).rgb;
}

// Reconstructs sky-only diffuse irradiance E(n) from the 9 baked SH
// coefficients. Direct sun light is handled separately by directLight();
// this deliberately excludes the sun disc (see cook tool docstring) so it
// is never counted twice.
vec3 evaluateSkyIrradiance(vec3 n) {
  vec3 e = environment.skyIrradianceSH[0].rgb * .282095;
  e += environment.skyIrradianceSH[1].rgb * (.488603*n.y);
  e += environment.skyIrradianceSH[2].rgb * (.488603*n.z);
  e += environment.skyIrradianceSH[3].rgb * (.488603*n.x);
  e += environment.skyIrradianceSH[4].rgb * (1.092548*n.x*n.y);
  e += environment.skyIrradianceSH[5].rgb * (1.092548*n.y*n.z);
  e += environment.skyIrradianceSH[6].rgb * (.315392*(3.0*n.y*n.y-1.0));
  e += environment.skyIrradianceSH[7].rgb * (1.092548*n.x*n.z);
  e += environment.skyIrradianceSH[8].rgb * (.546274*(n.x*n.x-n.z*n.z));
  return max(e,vec3(0));
}

vec3 toneMapEnvironment(vec3 color) {
  return agxTonemap(color*environment.parameters.x);
}
