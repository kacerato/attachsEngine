#include "dirt_road_frame.glsl"

precision mediump int;
precision highp float;

layout(location=0) in highp vec3 vPosition;
layout(location=1) in mediump vec3 vNormal;
layout(location=2) in mediump vec4 vTangent;
layout(location=3) in highp vec2 vUv0;
layout(location=4) in highp vec2 vUv1;
layout(location=5) in mediump vec4 vColor;
layout(location=6) in mediump float vDither;
#ifdef AETHER_SPECTRAL_WATER
layout(location=7) in mediump float vSpectralFoam;
layout(location=8) in highp vec3 vSpectralCoordinates;
#endif
layout(location=0) out vec4 outColor;

// O depth opaco permanece na memória do tile. subpassLoad lê somente o pixel
// corrente, suficiente para espessura, absorção e espuma de interseção sem a
// cópia full-resolution que uma textura de refração exigiria.
layout(input_attachment_index=0,set=1,binding=5) uniform subpassInput sceneDepthInput;
layout(set=1,binding=6) uniform sampler2D waterNormalTexture;

const float PI=3.141592653589793;
layout(constant_id=2) const uint ENVIRONMENT_PROJECTION=0u;
#include "environment_lighting.glsl"
#include "microfacet_brdf.glsl"
#ifdef AETHER_SPECTRAL_WATER
#define AETHER_SPECTRAL_FRAGMENT 1
#include "water_spectral_sampling.glsl"
#endif

mediump float pow5(mediump float value) {
  mediump float squared=value*value; return squared*squared*value;
}

highp float viewDepthFromDevice(highp float depth,highp float nearPlane,highp float farPlane) {
  return nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-5);
}

// Frequências menores que um vértice são detalhe de normal, não geometria. A
// cadeia mip filtra automaticamente o espectro; duas projeções giradas quebram
// repetição sem o custo e o aliasing de vários sin/cos por fragmento.
mediump vec2 microSlope(highp vec2 xz,highp float timeSeconds,highp float cameraDistance) {
  const highp mat2 rotation=mat2(0.819152,0.573576,-0.573576,0.819152);
  highp vec2 uv0=xz*0.031+vec2(0.021,-0.014)*timeSeconds;
  highp vec2 uv1=rotation*xz*0.057+vec2(-0.017,0.026)*timeSeconds;
  mediump vec2 first=texture(waterNormalTexture,uv0).rg*2.0-1.0;
  mediump vec2 second=rotation*(texture(waterNormalTexture,uv1).rg*2.0-1.0);
  mediump float distanceFade=1.0-smoothstep(260.0,1300.0,cameraDistance);
  return (first*0.105+second*0.065)*distanceFade*environment.waterParameters.w;
}

// Atribuição de custo: um GPU tile-deferred colapsa timestamps por subpasse, e a
// única leitura honesta do que o fragmento da água gasta é remover um termo por
// vez. O desvio é uniforme no draw inteiro, então o corpo removido não executa.
// Ver WaterCostIsolation; o modo zero é o caminho de produção.
#define WATER_ISOLATION int(environment.waterInteractionParameters.y+0.5)
#define WATER_ISOLATION_NO_SHADOW 1
#define WATER_ISOLATION_NO_SPECTRAL_DETAIL 2
#define WATER_ISOLATION_NO_MICRO_NORMAL 3
#define WATER_ISOLATION_NO_REFLECTION 4
#define WATER_ISOLATION_FLAT 5

void main() {
  int isolation=WATER_ISOLATION;
  if(isolation==WATER_ISOLATION_FLAT) {
    // Geometria, profundidade e blend permanecem; só o sombreamento sai.
    outColor=vec4(vec3(0.05,0.18,0.26)*0.72,0.72);
    return;
  }
  highp vec3 eye=frame.cameraPositionNear.xyz;
  highp float nearPlane=frame.cameraPositionNear.w;
  highp float farPlane=uintBitsToFloat(frame.materialFlags.w);
  highp float waterDepth=viewDepthFromDevice(gl_FragCoord.z,nearPlane,farPlane);
  highp float opaqueDeviceDepth=subpassLoad(sceneDepthInput).r;
  highp float opaqueDepth=viewDepthFromDevice(opaqueDeviceDepth,nearPlane,farPlane);
  highp float cameraDistance=length(eye-vPosition);
  // Depth is axial, Beer-Lambert needs distance along the camera ray.
  // Missing opaque geometry represents deep water, not a transparent sky floor.
  highp float thickness=max(opaqueDepth-waterDepth,0.0)*
      (cameraDistance/max(waterDepth,nearPlane));
  thickness=min(thickness,farPlane);
  mediump vec2 micro=isolation==WATER_ISOLATION_NO_MICRO_NORMAL?vec2(0.0):
      microSlope(vPosition.xz,environment.waterParameters.z,cameraDistance);
#ifdef AETHER_SPECTRAL_WATER
  if(isolation!=WATER_ISOLATION_NO_SPECTRAL_DETAIL) {
    // Geometry filtering must not erase wavelengths still resolvable by pixels.
    // Use parameter-space derivatives before any divergent cascade selection.
    highp float pixelSpacing=max(length(dFdx(vSpectralCoordinates.xy)),length(dFdy(vSpectralCoordinates.xy)));
    for(int cascade=0;cascade<4;++cascade) {
      if(cascade>=int(environment.waterParameters.x)) break;
      highp vec4 parameters=environment.waterWaveShape[cascade];
      highp float shortest=environment.waterWaveMotion[cascade].x;
      highp float geometricWeight=smoothstep(2.0,5.0,shortest/max(vSpectralCoordinates.z,.001));
      highp float pixelWeight=smoothstep(2.0,5.0,shortest/max(pixelSpacing,.001));
      highp float weight=parameters.z*(1.0-geometricWeight)*pixelWeight;
      if(weight<=.00001) continue;
      highp float c=environment.waterWaveMotion[cascade].y,s=environment.waterWaveMotion[cascade].z;
      highp mat2 rotation=mat2(c,s,-s,c);
      highp vec2 uv=transpose(rotation)*vSpectralCoordinates.xy/parameters.y;
      micro+=rotation*sampleWaterSlope(cascade,uv,uint(parameters.x))*weight;
    }
  }
#endif
  mediump vec3 n=normalize(vec3(vNormal.x-micro.x,vNormal.y,vNormal.z-micro.y));
  mediump vec3 v=normalize(eye-vPosition);
  mediump float nv=max(dot(n,v),0.001);
  mediump float ior=clamp(environment.waterOptics.x,1.0,2.0);
  mediump float f0=(ior-1.0)/(ior+1.0); f0*=f0;
  mediump float fresnelWeight=f0+(1.0-f0)*pow5(1.0-nv);
  mediump float rough=clamp(environment.waterOptics.y,0.025,1.0);
  if(environment.parameters.w>1.5 && isolation!=WATER_ISOLATION_NO_REFLECTION) {
    mediump vec2 integratedBrdf=environmentBrdf(nv,rough);
    fresnelWeight=clamp(f0*integratedBrdf.x+integratedBrdf.y,0.0,1.0);
  }
  mediump vec3 reflected=isolation==WATER_ISOLATION_NO_REFLECTION?vec3(0.0):
      environmentRadiance(reflect(-v,n),rough*environment.parameters.z);

  // Opacity scales optical density, not coverage: a deep column must converge
  // to opaque instead of permanently leaking (1-opacity) of the background.
  mediump vec3 transmittance=exp(-environment.waterAbsorption.rgb*
      (thickness*clamp(environment.waterAbsorption.w,0.0,1.0)));
  mediump float transmissionLuma=dot(transmittance,vec3(0.2126,0.7152,0.0722));
  mediump vec3 body=mix(environment.waterDeepColorFoam.rgb,
                        environment.waterShallowColorDistance.rgb,transmissionLuma);
  body=mix(body,environment.waterShallowColorDistance.rgb,
           clamp(environment.waterOptics.z,0.0,1.0)*0.25);

  mediump vec3 l=environment.sunDirectionIntensity.xyz;
  mediump float nl=max(dot(n,l),0.0);
  mediump vec3 h=normalize(v+l);
  highp float alpha=rough*rough;
  highp float nh=max(dot(n,h),0.0);
  mediump float solarFresnel=f0+(1.0-f0)*pow5(1.0-max(dot(v,h),0.0));
  highp float sunSpec=distributionGGX(nh,alpha)*visibilitySmithGGX(nv,nl,alpha)*
                      solarFresnel*nl;
  mediump float shadow=(nl>0.0 && isolation!=WATER_ISOLATION_NO_SHADOW)?
      directionalShadow(vPosition,n,waterDepth):1.0;
  mediump vec3 sun=environment.sunColorAngularRadius.rgb*
                   environment.sunDirectionIntensity.w*sunSpec*shadow;

#ifdef AETHER_SPECTRAL_WATER
  mediump float crestFoam=clamp(vSpectralFoam,0.0,1.0);
#else
  mediump float geometricSlope=length(vNormal.xz)/max(vNormal.y,0.05);
  mediump float crestFoam=smoothstep(environment.waterOptics.w,
      min(environment.waterOptics.w+0.22,1.0),clamp(geometricSlope,0.0,1.0));
#endif
  // A interseção com terreno/objetos nasce da espessura real. Não depende de
  // nome de cena nem de uma textura pintada para esconder o encontro.
  mediump float shoreFoam=(1.0-smoothstep(0.06,1.35,thickness))*
                          step(opaqueDeviceDepth,0.99999);
  mediump float foam=clamp(max(crestFoam,shoreFoam)*environment.waterDeepColorFoam.w,0.0,1.0);

  mediump float opacity=clamp(1.0-transmissionLuma,0.0,1.0);
  mediump float compositeAlpha=clamp(max(opacity+fresnelWeight*(1.0-opacity),foam),0.0,1.0);
  mediump vec3 premultiplied=body*opacity*(1.0-fresnelWeight)+
                             reflected*fresnelWeight+sun;
  premultiplied=mix(premultiplied,vec3(0.92,0.97,1.0)*compositeAlpha,foam);
  // Nonlinear transfer functions act on straight color. Applying them to
  // premultiplied radiance makes a thin surface brighter as alpha decreases.
  mediump vec3 surfaceColor=toneMapEnvironment(premultiplied/max(compositeAlpha,1.0e-5));
  if((frame.materialFlags.z&1u)!=0u)
    surfaceColor=mix(12.92*surfaceColor,
        1.055*pow(surfaceColor,vec3(1.0/2.4))-.055,
        greaterThan(surfaceColor,vec3(.0031308)));
  outColor=vec4(surfaceColor*compositeAlpha,compositeAlpha);
}
