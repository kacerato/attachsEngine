#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
precision mediump int;
precision highp float;

// Recorte de folhagem no mapa de sombra. Sem isto, cada cartao de folha projeta
// um retangulo solido: a copa vira um bloco preto no chao e a sombra fica pior
// que nao ter sombra nenhuma.
layout(set=0,binding=0) uniform sampler2D textures[];
layout(push_constant) uniform ShadowPushConstants {
  mat4 lightViewProjection;
  vec4 alphaCutoffUvSlot;
  uvec4 baseTextureIndex;
} shadow;
layout(location=0) in highp vec2 vUv;
layout(location=1) in mediump float vAlpha;
void main() {
  // O indice vem de push constant aplicada uma vez por lote agrupado por
  // material, portanto e dinamicamente uniforme (mesma justificativa de
  // dirt_road.frag).
  mediump float alpha = texture(textures[shadow.baseTextureIndex.x], vUv).a * vAlpha;
  if (alpha < shadow.alphaCutoffUvSlot.x) discard;
}
