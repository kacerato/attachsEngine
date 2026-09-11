#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
const float PI=3.141592653589793;
layout(set=0,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
} environment;
layout(set=0,binding=1) uniform sampler2D environmentMap;
layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outColor;
vec2 environmentUv(vec3 direction) {
  direction=normalize(direction);
  return vec2(fract((atan(direction.z,direction.x)+environment.parameters.y)/
                    (2.0*PI)+0.5),
              acos(clamp(direction.y,-1.0,1.0))/PI);
}
vec3 linearToSrgb(vec3 color) {
  return mix(12.92*color,1.055*pow(max(color,vec3(0)),vec3(1.0/2.4))-.055,
             greaterThan(color,vec3(.0031308)));
}
void main() {
  // The panorama is authored in display-referred sRGB and sampled through an
  // sRGB Vulkan format, so the texture unit supplies linear color. Keeping the
  // visible sky to one filtered lookup avoids the full-screen procedural-cloud
  // cost measured on mobile. Only world direction is used: camera translation
  // cannot move the infinitely distant sky or introduce parallax.
  vec3 visibleDirection=normalize(vDirection);
  if((frame.materialFlags.x&1u)!=0u && visibleDirection.y<0.0) {
    // A grade oceanica acompanha XZ da camera, mas continua finita. Raios
    // quase paralelos podem passar alem da ultima aresta e revelar a metade
    // inferior do panorama como uma faixa horizontal. Nessa cunha, refletir a
    // direcao acima do horizonte e a continuacao optica correta do ambiente
    // que a superficie distante representaria. O bit so e publicado quando a
    // cena possui MapMaterialWaterCameraGrid; lagos finitos e cenas terrestres
    // continuam usando o panorama integral.
    visibleDirection.y=-visibleDirection.y;
  }
  vec3 color=textureLod(environmentMap,environmentUv(visibleDirection),0.0).rgb;
  if((frame.materialFlags.z&1u)!=0u) color=linearToSrgb(color);
  outColor=vec4(color,1);
}
