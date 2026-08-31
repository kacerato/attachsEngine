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
void main() {
  // Posicao, view e clip permanecem highp em toda a cadeia: fp16 em coordenada
  // de mundo ou de clip produz tremor de vertice e z-fighting visiveis, e a
  // subtracao relativa a camera e exatamente o caso de cancelamento catastrofico.
  highp vec3 worldPosition=(inModel*vec4(inPosition,1)).xyz;
  vPosition=worldPosition;
  highp mat3 linear=mat3(inModel);
  highp mat3 normalMatrix=mat3(inNormalColumn0.xyz,inNormalColumn1.xyz,inNormalColumn2.xyz);
  vNormal=normalize(normalMatrix*inNormal);
  vTangent=vec4(normalize(linear*inTangent.xyz),inTangent.w*inNormalColumn0.w);
  vUv0=inUv0; vUv1=inUv1; vColor=inColor*inTint; vDither=inNormalColumn1.w;
  highp vec3 relative=worldPosition-frame.cameraPositionNear.xyz;
  highp vec3 view=vec3(dot(environment.worldToViewRow0.xyz,relative),
                 dot(environment.worldToViewRow1.xyz,relative),
                 dot(environment.worldToViewRow2.xyz,relative));
  highp float farPlane=uintBitsToFloat(frame.materialFlags.w);
  highp float nearPlane=frame.cameraPositionNear.w;
  highp vec2 xy=vec2(view.x*1.732050808/frame.cameraFrame.x,-view.y*1.732050808);
  gl_Position=vec4(dot(frame.surfaceTransform.xy,xy),dot(frame.surfaceTransform.zw,xy),
      (farPlane*view.z-nearPlane*farPlane)/(farPlane-nearPlane),view.z);
}
