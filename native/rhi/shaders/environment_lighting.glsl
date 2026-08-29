layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
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

vec3 toneMapEnvironment(vec3 color) {
  color=max(color*environment.parameters.x,vec3(0));
  // Stable filmic curve with a soft shoulder for the HDR sun.
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
}
