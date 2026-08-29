#version 450
#extension GL_GOOGLE_include_directive : require
const float PI=3.141592653589793;
#include "tonemap.glsl"
// Only the leading 64 bytes of the EnvironmentLighting buffer are declared
// here; the trailing SH9 irradiance (dirt_road_shading.glsl/
// environment_lighting.glsl) is not needed to draw the visible dome.
layout(set=0,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
} environment;
layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outColor;

float hash21(vec2 p) {
  p=fract(p*vec2(123.34,456.21));
  p+=dot(p,p+45.32);
  return fract(p.x*p.y);
}
float valueNoise(vec2 p) {
  vec2 cell=floor(p), f=fract(p);
  f=f*f*(3.0-2.0*f);
  return mix(mix(hash21(cell),hash21(cell+vec2(1,0)),f.x),
             mix(hash21(cell+vec2(0,1)),hash21(cell+vec2(1)),f.x),f.y);
}
void main() {
  vec3 direction=normalize(vDirection);
  float horizon=smoothstep(-.12,.62,direction.y);
  vec3 horizonBlue=vec3(.53,.76,.98);
  vec3 zenithBlue=vec3(.075,.28,.68);
  vec3 color=mix(horizonBlue,zenithBlue,horizon);

  // Two low-cost noise octaves form broad daylight clouds. Fade them near the
  // horizon and below it so the sky stays stable and inexpensive on mobile.
  vec2 cloudUv=direction.xz/max(direction.y+.34,.12);
  float cloudNoise=valueNoise(cloudUv*1.35)+.5*valueNoise(cloudUv*2.7+17.0);
  float clouds=smoothstep(.79,1.17,cloudNoise)*smoothstep(.02,.24,direction.y);
  float cloudLight=.78+.22*max(dot(direction,normalize(environment.sunDirectionIntensity.xyz)),0.0);
  color=mix(color,vec3(.93,.96,1.0)*cloudLight,clouds*.78);

  vec3 sunDirection=normalize(environment.sunDirectionIntensity.xyz);
  float sunDot=max(dot(direction,sunDirection),0.0);
  float sunDisc=smoothstep(cos(environment.sunColorAngularRadius.w*2.6),
                           cos(environment.sunColorAngularRadius.w),sunDot);
  float sunGlow=pow(sunDot,128.0)*.28;
  color+=environment.sunColorAngularRadius.rgb*(sunDisc*1.45+sunGlow);

  color=agxTonemap(color*environment.parameters.x);
  outColor=vec4(color,1);
}
