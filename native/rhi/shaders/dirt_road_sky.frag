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
  vec4 sceneSky;
  vec4 sceneFogColorDensity;
  vec4 sceneFog;
  vec4 scenePost;
  vec4 sceneAo;
  vec4 sceneAoDetail;
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
vec3 acesToneMap(vec3 color) {
  color=max(color,vec3(0.0));
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
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
  if(environment.sceneSky.w>0.5) {
    float altitude=visibleDirection.y;
    float height=clamp(altitude,0.0,1.0);
    float horizonBand=exp(-abs(altitude)*7.0);
    vec3 upper=mix(environment.skyHorizonCloudDensity.rgb,
                   environment.skyZenithCloudCoverage.rgb,
                   pow(height,0.38));
    vec3 lower=mix(environment.groundColorSaturation.rgb,
                   environment.skyHorizonCloudDensity.rgb,
                   smoothstep(-0.32,0.025,altitude));
    vec3 atmosphereSky=altitude>=0.0?upper:lower;
    vec3 sunDirection=normalize(environment.sunDirectionIntensity.xyz);
    float alignment=clamp(dot(visibleDirection,sunDirection),-1.0,1.0);
    // Aproximação analítica de espalhamento. Mantém uma única passagem e não
    // exige LUT, mas acrescenta a separação visual que faltava entre zênite,
    // horizonte e halo solar no viewport mobile.
    float rayleighPhase=0.0596831*(1.0+alignment*alignment);
    const float mieG=0.76;
    float mieDenominator=max(0.02,1.0+mieG*mieG-2.0*mieG*alignment);
    float miePhase=0.119366*(1.0-mieG*mieG)*(1.0+alignment*alignment)/
                   ((2.0+mieG*mieG)*pow(mieDenominator,1.5));
    float airMass=1.0/(0.12+pow(max(height+0.12,0.001),0.42));
    float opticalDepth=1.0-exp(-airMass*(0.045+0.18*environment.sceneSky.x));
    vec3 sunTint=environment.sunColorAngularRadius.rgb;
    atmosphereSky+=sunTint*(rayleighPhase*(0.35+0.65*horizonBand)+
                            miePhase*(0.15+1.85*horizonBand))*opticalDepth;
    atmosphereSky=mix(atmosphereSky,
                      environment.skyHorizonCloudDensity.rgb+sunTint*0.08,
                      horizonBand*0.14*environment.sceneSky.x);
    float angularDistance=acos(alignment);
    float radius=max(environment.sceneSky.y,0.0001);
    float edge=max(fwidth(angularDistance)*1.5,radius*0.10);
    float disc=1.0-smoothstep(radius-edge,radius+edge,angularDistance);
    float corona=pow(max(alignment,0.0),256.0)*(0.3+0.7*horizonBand);
    atmosphereSky+=sunTint*(disc*environment.sceneSky.z+
                            corona*environment.sceneSky.z*0.18);
    color=mix(color,atmosphereSky,clamp(environment.sceneSky.x,0.0,1.0));
  }
  // O ceu visivel acompanha o sol autorado em lux (ver environmentRadiance).
  color*=environment.sceneAoDetail.z;
  if(environment.scenePost.z<0.5)
    color=acesToneMap(color*environment.parameters.x*exp2(environment.scenePost.x));
  if((frame.materialFlags.z&1u)!=0u) color=linearToSrgb(color);
  outColor=vec4(color,1);
}
