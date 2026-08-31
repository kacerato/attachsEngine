#version 450
#extension GL_GOOGLE_include_directive : require

// Passe de sombra: só profundidade, do ponto de vista do sol.
//
// Reutiliza exatamente o mesmo layout de vértice/instância do passe principal
// para que os mesmos buffers já ligados sirvam sem rebind. Atributos que a
// sombra não usa continuam declarados porque o estado de vertex input do
// pipeline precisa casar com os bindings; o compilador descarta o que não é
// lido.
layout(push_constant) uniform ShadowPushConstants {
  mat4 lightViewProjection;   // mundo -> clip da cascata
  vec4 alphaCutoffUvSlot;     // x=cutoff, y=slot de UV, zw reservados
  uvec4 baseTextureIndex;     // x = indice bindless da base color
} shadow;

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

layout(location=0) out highp vec2 vUv;
layout(location=1) out mediump float vAlpha;

void main() {
  // Posicao permanece highp em toda a cadeia: a cascata distante cobre centenas
  // de unidades de mundo por texel e fp16 aqui produziria degraus na sombra.
  highp vec4 world = inModel * vec4(inPosition, 1.0);
  vUv = shadow.alphaCutoffUvSlot.y > 0.5 ? inUv1 : inUv0;
  vAlpha = (inColor * inTint).a;
  gl_Position = shadow.lightViewProjection * world;
}
