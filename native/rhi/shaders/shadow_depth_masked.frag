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
  vec4 uvRow0;
  vec4 uvRow1;
} shadow;
layout(location=0) in highp vec2 vUv;
layout(location=1) in mediump float vAlpha;
void main() {
  // O indice vem de push constant aplicada uma vez por lote agrupado por
  // material, portanto e dinamicamente uniforme (mesma justificativa de
  // dirt_road.frag).
  mediump vec4 texel = texture(textures[shadow.baseTextureIndex.x], vUv);
  // R4: origem do alfa escolhida no material (a mesma do passe de cor).
  uint source = uint(shadow.alphaCutoffUvSlot.z + 0.5);
  mediump float sampled = source == 1u ? 1.0 : source == 2u ? dot(texel.rgb, vec3(.2126, .7152, .0722)) : texel.a;
  mediump float alpha = sampled * vAlpha;
  if (alpha < shadow.alphaCutoffUvSlot.x) discard;
}
