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
  layout(offset=3248) vec4 atmosphereScattering;
  layout(offset=3264) vec4 atmosphereGeometry;
  layout(offset=3280) vec4 atmosphereDetail;
  // Irradiância hemisférica incidente no solo, integrada na CPU e atualizada
  // apenas quando atmosfera ou sol mudam. w fica reservado para evolução ABI.
  layout(offset=3312) vec4 atmosphereGroundIrradiance;
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
#include "tone_mapping.glsl"

vec2 raySphere(vec3 origin,vec3 direction,float radius) {
  // Solve in radius-normalized coordinates. At Earth scale the unnormalized
  // b^2 term is ~4e7 and loses the few units that distinguish horizon rays.
  vec3 normalizedOrigin=origin/radius;
  float b=dot(normalizedOrigin,direction);
  float radialDistance=length(normalizedOrigin);
  float c=(radialDistance-1.0)*(radialDistance+1.0);
  float discriminant=b*b-c;
  float tangentTolerance=4.0e-7*max(1.0,max(abs(b*b),abs(c)));
  if(discriminant < -tangentTolerance) return vec2(-1.0);
  float root=sqrt(max(discriminant,0.0));
  return vec2(-b-root,-b+root)*radius;
}

vec2 atmosphereDensity(vec3 position,float planetRadius,vec2 scaleHeights) {
  float height=max(length(position)-planetRadius,0.0);
  return exp(-height/max(scaleHeights,vec2(0.001)));
}
float cubicDistribution(float value) { return value*value*value; }

vec2 opticalDepthToAtmosphere(vec3 origin,vec3 direction,float planetRadius,
                              float atmosphereRadius,vec2 scaleHeights,int steps) {
  vec2 interval=raySphere(origin,direction,atmosphereRadius);
  float distance=max(interval.y,0.0);
  vec2 depth=vec2(0.0);
  for(int index=0;index<8;++index) {
    if(index>=steps) break;
    float segmentStart=cubicDistribution(float(index)/float(steps))*distance;
    float segmentEnd=cubicDistribution(float(index+1)/float(steps))*distance;
    float sampleDistance=(segmentStart+segmentEnd)*0.5;
    vec3 samplePosition=origin+direction*sampleDistance;
    depth+=atmosphereDensity(samplePosition,planetRadius,scaleHeights)*
           (segmentEnd-segmentStart);
  }
  return depth;
}

vec3 integrateSingleScattering(vec3 origin,vec3 direction,float distance,
                               int viewSteps,int lightSteps,float planetRadius,
                               float atmosphereRadius,vec2 scaleHeights,
                               vec3 betaRayleigh,vec3 betaAerosol,
                               vec3 betaAerosolExtinction,vec3 sunDirection,
                               out vec2 integratedDepth) {
  float cosine=clamp(dot(direction,sunDirection),-1.0,1.0);
  float rayleighPhase=3.0*(1.0+cosine*cosine)/(16.0*PI);
  float anisotropy=clamp(environment.atmosphereScattering.w,0.0,0.95);
  float g2=anisotropy*anisotropy;
  float mieDenominator=max(1.0+g2-2.0*anisotropy*cosine,0.001);
  float aerosolPhase=3.0*(1.0-g2)*(1.0+cosine*cosine)/
                     (8.0*PI*(2.0+g2)*pow(mieDenominator,1.5));
  vec2 viewDepth=vec2(0.0);
  vec3 scattered=vec3(0.0);
  for(int index=0;index<16;++index) {
    if(index>=viewSteps) break;
    float segmentStart=cubicDistribution(float(index)/float(viewSteps))*distance;
    float segmentEnd=cubicDistribution(float(index+1)/float(viewSteps))*distance;
    float segmentLength=segmentEnd-segmentStart;
    vec3 samplePosition=origin+direction*((segmentStart+segmentEnd)*0.5);
    vec2 density=atmosphereDensity(samplePosition,planetRadius,scaleHeights);
    vec2 segmentDepth=density*segmentLength;
    viewDepth+=segmentDepth*0.5;

    vec3 sampleUp=normalize(samplePosition);
    vec2 groundShadow=raySphere(samplePosition+sampleUp*0.001,sunDirection,planetRadius);
    if(groundShadow.x<=0.0) {
      vec2 sunDepth=opticalDepthToAtmosphere(samplePosition,sunDirection,planetRadius,
                                             atmosphereRadius,scaleHeights,lightSteps);
      vec3 extinction=betaRayleigh*(viewDepth.x+sunDepth.x)+
                      betaAerosolExtinction*(viewDepth.y+sunDepth.y);
      scattered+=exp(-extinction)*(betaRayleigh*density.x*rayleighPhase+
                                   betaAerosol*density.y*aerosolPhase)*segmentLength;
    }
    viewDepth+=segmentDepth*0.5;
  }
  integratedDepth=viewDepth;
  return scattered;
}

vec3 physicalAtmosphere(vec3 viewDirection) {
  float intensity=environment.atmosphereScattering.x;
  float airDensity=max(environment.atmosphereScattering.y,0.0);
  float aerosolDensity=max(environment.atmosphereScattering.z,0.0);
  float planetRadius=max(environment.atmosphereGeometry.x,1.0);
  float observerHeight=clamp(environment.atmosphereGeometry.y,0.0,
                             max(environment.atmosphereDetail.x,1.0));
  vec2 scaleHeights=max(environment.atmosphereGeometry.zw,vec2(0.001));
  float atmosphereRadius=planetRadius+max(environment.atmosphereDetail.x,1.0);
  float groundAlbedo=clamp(environment.atmosphereDetail.y,0.0,1.0);
  bool highQuality=environment.atmosphereDetail.z>0.5;
  int viewSteps=highQuality?16:8;
  int lightSteps=highQuality?8:4;

  vec3 origin=vec3(0.0,planetRadius+observerHeight,0.0);
  vec2 atmosphereHit=raySphere(origin,viewDirection,atmosphereRadius);
  if(atmosphereHit.y<=0.0) return vec3(0.0);
  float end=atmosphereHit.y;
  vec2 planetHit=raySphere(origin,viewDirection,planetRadius);
  bool hitsGround=planetHit.x>0.0;
  if(hitsGround) end=min(end,planetHit.x);
  float viewDistance=max(end,0.0);

  // Standard sea-level scattering coefficients, expressed per kilometre.
  // Author controls scale density rather than entering wavelength constants.
  vec3 betaRayleigh=vec3(0.005802,0.013558,0.033100)*airDensity;
  vec3 betaAerosol=vec3(0.021000)*aerosolDensity;
  vec3 betaAerosolExtinction=betaAerosol*1.10;
  float sunIntensity=max(environment.sunDirectionIntensity.w,0.0);
  if(sunIntensity<=0.0) return vec3(0.0);
  vec3 sunDirection=normalize(environment.sunDirectionIntensity.xyz);
  vec3 sunColor=max(environment.sunColorAngularRadius.rgb,vec3(0.0));
  vec2 viewDepth;
  vec3 scattered=integrateSingleScattering(origin,viewDirection,viewDistance,
      viewSteps,lightSteps,planetRadius,atmosphereRadius,scaleHeights,
      betaRayleigh,betaAerosol,betaAerosolExtinction,sunDirection,viewDepth);

  vec3 color=scattered*sunColor*sunIntensity*intensity;
  if(hitsGround) {
    vec3 groundPosition=origin+viewDirection*end;
    vec3 groundNormal=normalize(groundPosition);
    vec3 groundOrigin=groundPosition+groundNormal*0.001;
    float illumination=max(dot(groundNormal,sunDirection),0.0);
    vec3 directIrradiance=vec3(0.0);
    if(illumination>0.0) {
      vec2 groundSunDepth=opticalDepthToAtmosphere(groundOrigin,sunDirection,planetRadius,
                                                   atmosphereRadius,scaleHeights,lightSteps);
      vec3 groundSunTransmittance=exp(-(betaRayleigh*groundSunDepth.x+
                                       betaAerosolExtinction*groundSunDepth.y));
      directIrradiance=sunColor*sunIntensity*groundSunTransmittance*illumination;
    }

    // A fronteira difusa usa irradiância direta + hemisférica antes do BRDF
    // Lambertiano. A integral do céu é pré-calculada e cacheada na CPU para
    // não executar três integrações atmosféricas extras por pixel inferior.
    vec3 skyIrradiance=max(environment.atmosphereGroundIrradiance.rgb,vec3(0.0));
    vec3 viewTransmittance=exp(-(betaRayleigh*viewDepth.x+
                                betaAerosolExtinction*viewDepth.y));
    color+=vec3(groundAlbedo)*(directIrradiance+skyIrradiance)/PI*
           viewTransmittance*intensity;
  }

  // The authored disc is occluded by the planet and attenuated by the air.
  // It uses the current sun intensity directly, without the legacy sky scale.
  if(!hitsGround) {
    float cosine=clamp(dot(viewDirection,sunDirection),-1.0,1.0);
    float angularDistance=acos(cosine);
    float radius=max(environment.sceneSky.y,0.0001);
    float edge=max(fwidth(angularDistance)*1.5,radius*0.10);
    float disc=1.0-smoothstep(radius-edge,radius+edge,angularDistance);
    vec2 sunDepth=opticalDepthToAtmosphere(origin,sunDirection,planetRadius,
                                           atmosphereRadius,scaleHeights,lightSteps);
    vec3 sunTransmittance=exp(-(betaRayleigh*sunDepth.x+
                               betaAerosolExtinction*sunDepth.y));
    color+=sunColor*sunIntensity*sunTransmittance*disc*environment.sceneSky.z;
  }
  return max(color,vec3(0.0));
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
  if(environment.sceneSky.w>1.5) {
    color=physicalAtmosphere(visibleDirection);
  } else if(environment.sceneSky.w>0.5) {
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
  // HDRI e o céu legado usam a proporção relativa skyScale. O modo físico já
  // integra a intensidade atual do sol e não pode recebê-la uma segunda vez.
  if(environment.sceneSky.w<1.5) color*=environment.sceneAoDetail.z;
  if(environment.scenePost.z<0.5)
    color=astraToneMap(color*environment.parameters.x*exp2(environment.scenePost.x),environment.scenePost.y);
  if((frame.materialFlags.z&1u)!=0u) color=linearToSrgb(color);
  outColor=vec4(color,1);
}
