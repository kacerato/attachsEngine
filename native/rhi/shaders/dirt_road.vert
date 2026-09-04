#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
  vec4 quality;
  mat4 shadowViewProjection[4];
  vec4 shadowSplitDepths;
  vec4 shadowParameters;
  vec4 shadowWorldUnitsPerTexel;
  vec4 shadowFilterParameters; // xy=PCF radii, zw=temporal jitter in physical NDC
} environment;
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inTangent;
layout(location=3) in vec2 inUv0;
layout(location=4) in vec2 inUv1;
layout(location=5) in vec4 inColor;
layout(location=6) in mat4 inModel;
layout(location=10) in vec4 inTint;
layout(location=11) in vec4 inNormalColumn0;
layout(location=12) in vec4 inNormalColumn1;
layout(location=13) in vec4 inNormalColumn2;
// Precisao dos varyings (item 2.2.5). Cada varying e gravado na memoria de tile
// e reinterpolado por fragmento: em fp16 esse trafego cai pela metade, e sao 12
// floats por pixel entre normal, tangente, cor e dither.
//
// vPosition e vUv permanecem highp e nao por simetria: vPosition alimenta o
// vetor de visao em coordenadas de mundo, num mapa de centenas de unidades, e as
// UV enderecam texturas de ate 4096 px, onde a mantissa do fp16 (~2048 passos em
// [0,1]) ja nao resolve um texel.
layout(location=0) out highp vec3 vPosition;
layout(location=1) out mediump vec3 vNormal;
layout(location=2) out mediump vec4 vTangent;
layout(location=3) out highp vec2 vUv0;
layout(location=4) out highp vec2 vUv1;
layout(location=5) out mediump vec4 vColor;
// LOD dither cross-fade factor (see renderer::selectLodLevel /
// PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md): [-1,1], 0 = fully opaque, positive =
// outgoing mask, negative = incoming complementary mask. Piggybacks
// on GpuMeshInstance.normalColumns[7] (column 1's .w), which
// buildNormalMatrix leaves at 0 unconditionally -- column 0's .w is the only
// one carrying real data (tangent handedness sign), so this slot never grew
// the 128-byte instance struct or its vertex binding stride.
layout(location=6) out mediump float vDither;
const uint MATERIAL_IMPOSTOR=256u; // renderer::MapMaterialImpostor
void main() {
  // Posicao, view e clip permanecem highp em toda a cadeia: fp16 em coordenada
  // de mundo ou de clip produz tremor de vertice e z-fighting visiveis, e a
  // subtracao relativa a camera e exatamente o caso de cancelamento catastrofico.
  // Impostor de folhagem: o quad chega em espaco local, centrado na origem, e
  // gira em torno de Y para encarar a camera ANTES da matriz de mundo. O bake
  // v2 guarda varias vistas azimutais; os 16 bits superiores de materialFlags.y
  // descrevem o macro-tile e UV1 guarda sua origem no atlas.
  //
  // Normal e tangente giram com a posicao. Sem isso o quad encararia a camera
  // mas continuaria iluminado como se ainda encarasse a direcao em que foi
  // assado, e a folhagem distante alternaria entre clara e escura conforme o
  // giro -- defeito mais visivel que a troca de LOD que o impostor resolve.
  highp vec3 modelPosition=inPosition;
  highp vec3 modelNormal=inNormal;
  highp vec3 modelTangent=inTangent.xyz;
  highp vec2 resolvedUv0=inUv0;
  highp vec2 resolvedUv1=inUv1;
  mediump float impostorViewBlend=0.0;
  bool impostor=(frame.materialFlags.x&MATERIAL_IMPOSTOR)!=0u;
  if(impostor) {
    highp vec3 worldCenter=inModel[3].xyz;
    highp vec2 planarToObject=worldCenter.xz-frame.cameraPositionNear.xz;
    highp float planarLength=length(planarToObject);
    highp vec2 forward=planarLength>1.0e-5?planarToObject/planarLength:
        vec2(sin(frame.cameraFrame.y),cos(frame.cameraFrame.y));
    highp vec3 worldForward=vec3(forward.x,0.0,forward.y);
    highp vec3 worldRight=vec3(forward.y,0.0,-forward.x);
    highp mat3 faceCamera=mat3(worldRight,vec3(0.0,1.0,0.0),worldForward);
    modelPosition=faceCamera*inPosition;
    modelNormal=faceCamera*inNormal;
    modelTangent=faceCamera*inTangent.xyz;

    uint metadata=frame.materialFlags.y>>16u;
    uint viewColumns=(metadata>>8u)&15u;
    uint viewRows=(metadata>>12u)&15u;
    if(viewColumns>0u && viewRows>0u) {
      highp vec2 macroScale=exp2(-vec2(float(metadata&15u),float((metadata>>4u)&15u)));
      uint viewCount=viewColumns*viewRows;
      highp float azimuth=atan(worldForward.x,worldForward.z);
      highp float viewPosition=fract(azimuth/6.28318530718+1.0)*float(viewCount);
      uint firstView=uint(floor(viewPosition))%viewCount;
      uint secondView=(firstView+1u)%viewCount;
      highp vec2 frameScale=macroScale/vec2(float(viewColumns),float(viewRows));
      highp vec2 firstCell=vec2(float(firstView%viewColumns),float(firstView/viewColumns));
      highp vec2 secondCell=vec2(float(secondView%viewColumns),float(secondView/viewColumns));
      // Keep exact-edge fragments in this view's cell when the fragment shader
      // clamps the footprint at the chosen resident mip level.
      highp vec2 localUv=clamp(inUv0,vec2(0.00001),vec2(0.99999));
      resolvedUv0=inUv1+(firstCell+localUv)*frameScale;
      resolvedUv1=inUv1+(secondCell+localUv)*frameScale;
      impostorViewBlend=fract(viewPosition);
    }
  }
  highp vec3 worldPosition=(inModel*vec4(modelPosition,1)).xyz;
  vPosition=worldPosition;
  highp mat3 linear=mat3(inModel);
  highp mat3 normalMatrix=mat3(inNormalColumn0.xyz,inNormalColumn1.xyz,inNormalColumn2.xyz);
  vNormal=normalize(normalMatrix*modelNormal);
  vTangent=vec4(normalize(linear*modelTangent),inTangent.w*inNormalColumn0.w);
  vUv0=resolvedUv0; vUv1=resolvedUv1; vColor=inColor*inTint;
  if(impostor && (frame.materialFlags.y>>16u)!=0u) vColor.a=impostorViewBlend;
  vDither=inNormalColumn1.w;
  highp vec3 relative=worldPosition-frame.cameraPositionNear.xyz;
  highp vec3 view=vec3(dot(environment.worldToViewRow0.xyz,relative),
                 dot(environment.worldToViewRow1.xyz,relative),
                 dot(environment.worldToViewRow2.xyz,relative));
  highp float farPlane=uintBitsToFloat(frame.materialFlags.w);
  highp float nearPlane=frame.cameraPositionNear.w;
  highp vec2 xy=vec2(view.x*1.732050808/frame.cameraFrame.x,-view.y*1.732050808);
  highp vec2 projected=vec2(dot(frame.surfaceTransform.xy,xy),
                            dot(frame.surfaceTransform.zw,xy));
  projected += environment.shadowFilterParameters.zw * view.z;
  gl_Position=vec4(projected,
      (farPlane*view.z-nearPlane*farPlane)/(farPlane-nearPlane),view.z);
}
