#version 450
#extension GL_GOOGLE_include_directive : require
const float PI=3.141592653589793;
layout(set=0,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
} environment;
layout(set=0,binding=1) uniform sampler2D environmentMap;
layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outColor;
vec2 environmentUv(vec3 direction) {
  direction=normalize(direction);
  return vec2(fract((atan(direction.z,direction.x)+environment.parameters.y)/(2.0*PI)+.5),
              acos(clamp(direction.y,-1.0,1.0))/PI);
}
void main() {
  vec3 color=textureLod(environmentMap,environmentUv(vDirection),0).rgb*environment.parameters.x;
  color=clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
  outColor=vec4(color,1);
}
