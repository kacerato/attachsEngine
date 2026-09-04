#version 450
#extension GL_GOOGLE_include_directive : require
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
layout(location=0) out vec4 outColor;

// O depth opaco permanece na memória do tile. subpassLoad lê somente o pixel
// corrente, suficiente para espessura, absorção e espuma de interseção sem a
// cópia full-resolution que uma textura de refração exigiria.
layout(input_attachment_index=0,set=1,binding=5) uniform subpassInput sceneDepthInput;
layout(set=1,binding=6) uniform sampler2D waterNormalTexture;

const float PI=3.141592653589793;
layout(constant_id=2) const uint ENVIRONMENT_PROJECTION=0u;
#include "environment_lighting.glsl"

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

void main() {
  highp vec3 eye=frame.cameraPositionNear.xyz;
  highp float nearPlane=frame.cameraPositionNear.w;
  highp float farPlane=uintBitsToFloat(frame.materialFlags.w);
  highp float waterDepth=viewDepthFromDevice(gl_FragCoord.z,nearPlane,farPlane);
  highp float opaqueDeviceDepth=subpassLoad(sceneDepthInput).r;
  highp float opaqueDepth=viewDepthFromDevice(opaqueDeviceDepth,nearPlane,farPlane);
  highp float thickness=clamp(opaqueDepth-waterDepth,0.0,80.0);

  highp float cameraDistance=length(eye-vPosition);
  mediump vec2 micro=microSlope(vPosition.xz,environment.waterParameters.z,cameraDistance);
  mediump vec3 n=normalize(vec3(vNormal.x-micro.x,vNormal.y,vNormal.z-micro.y));
  mediump vec3 v=normalize(eye-vPosition);
  mediump float nv=max(dot(n,v),0.001);
  mediump float ior=clamp(environment.waterOptics.x,1.0,2.0);
  mediump float f0=(ior-1.0)/(ior+1.0); f0*=f0;
  mediump float fresnelWeight=f0+(1.0-f0)*pow5(1.0-nv);
  mediump float rough=clamp(environment.waterOptics.y,0.025,1.0);
  mediump vec3 reflected=environmentRadiance(reflect(-v,n),rough*environment.parameters.z);

  mediump vec3 transmittance=exp(-environment.waterAbsorption.rgb*thickness);
  mediump float transmissionLuma=dot(transmittance,vec3(0.2126,0.7152,0.0722));
  mediump vec3 body=mix(environment.waterDeepColorFoam.rgb,
                        environment.waterShallowColorDistance.rgb,transmissionLuma);
  body=mix(body,environment.waterShallowColorDistance.rgb,
           clamp(environment.waterOptics.z,0.0,1.0)*0.25);

  mediump vec3 l=environment.sunDirectionIntensity.xyz;
  mediump float nl=max(dot(n,l),0.0);
  mediump vec3 h=normalize(v+l);
  mediump float sunPower=mix(512.0,24.0,rough);
  mediump float sunSpec=pow(max(dot(n,h),0.0),sunPower)*nl;
  mediump float shadow=nl>0.0?directionalShadow(vPosition,n,waterDepth):1.0;
  mediump vec3 sun=environment.sunColorAngularRadius.rgb*
                   environment.sunDirectionIntensity.w*sunSpec*shadow;

  mediump float geometricSlope=length(vNormal.xz)/max(vNormal.y,0.05);
  mediump float crestFoam=smoothstep(environment.waterOptics.w,
      min(environment.waterOptics.w+0.22,1.0),clamp(geometricSlope,0.0,1.0));
  // A interseção com terreno/objetos nasce da espessura real. Não depende de
  // nome de cena nem de uma textura pintada para esconder o encontro.
  mediump float shoreFoam=(1.0-smoothstep(0.06,1.35,thickness))*
                          step(opaqueDeviceDepth,0.99999);
  mediump float foam=clamp(max(crestFoam,shoreFoam)*environment.waterDeepColorFoam.w,0.0,1.0);

  mediump float opacity=clamp((1.0-transmissionLuma)*environment.waterAbsorption.w,
                              0.025,0.94);
  mediump float compositeAlpha=clamp(max(opacity,fresnelWeight),0.04,0.995);
  mediump vec3 premultiplied=body*opacity*(1.0-fresnelWeight)+
                             reflected*fresnelWeight+sun;
  premultiplied=mix(premultiplied,vec3(0.92,0.97,1.0)*compositeAlpha,foam);
  premultiplied=toneMapEnvironment(premultiplied);
  if((frame.materialFlags.z&1u)!=0u)
    premultiplied=mix(12.92*premultiplied,
        1.055*pow(premultiplied,vec3(1.0/2.4))-.055,
        greaterThan(premultiplied,vec3(.0031308)));
  outColor=vec4(premultiplied,compositeAlpha);
}
