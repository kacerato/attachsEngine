layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  // Transient frame data appended after the stable AEEN resource payload.
  // Rows transform a world-space direction into camera/view space.
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
} environment;
layout(set=1,binding=1) uniform sampler2D environmentMap;

// A UV do panorama permanece highp: sao 1024 px por eixo com costura horizontal,
// e o fract() perto da costura e exatamente onde fp16 produziria uma emenda
// visivel. A radiancia devolvida ja pode descer para mediump.
highp vec2 environmentUv(highp vec3 direction) {
  // Callers provide a normalized reflection direction. reflect() preserves
  // length for normalized N/V, avoiding a redundant reciprocal sqrt per pixel.
  highp float phi=atan(direction.z,direction.x)+environment.parameters.y;
  return vec2(fract(phi/(2.0*PI)+.5),acos(clamp(direction.y,-1.0,1.0))/PI);
}

mediump vec3 environmentRadiance(highp vec3 direction,mediump float lod) {
  return textureLod(environmentMap,environmentUv(direction),clamp(lod,0.0,environment.parameters.z)).rgb;
}

mediump vec3 toneMapEnvironment(mediump vec3 color) {
  color=max(color*environment.parameters.x,vec3(0));
  // Stable filmic curve with a soft shoulder for the HDR sun.
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
}
