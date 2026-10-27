// Corpo do material de partícula, incluído por particle_material.frag e, com
// PARTICLE_PROJECT_TEXTURES, por particle_material_project.frag (aparelhos com
// descritores bindless: a imagem do Material de projeto, PK-10a).
//
// Material de partícula (G5 P1): mistura ou aditivo com textura principal,
// ruído que recorta e deforma, máscara, rolagem de UV, emissão HDR, brilho
// central e partícula suave por profundidade.
//
// Inspirado nas PROPRIEDADES dos shaders Blend_CenterGlow/Add_CenterGlow do RPG
// VFX Bundle (o que o artista ajusta), com código próprio: nenhum trecho dos
// shaders do pacote foi copiado (licença da Asset Store, plano §11).
#define AETHER_TEMPORAL_SET 0
#define AETHER_TEMPORAL_BINDING 0
#include "temporal_projection.glsl"

layout(set=1, binding=0) uniform sampler2D particleAtlas;
layout(set=1, binding=1) uniform highp sampler2D sceneDepth;
// G6a: cópia da cor da cena antes do passe (meia resolução), para a Distorção.
layout(set=1, binding=2) uniform sampler2D sceneCopy;
#ifdef PARTICLE_PROJECT_TEXTURES
// O mesmo conjunto bindless das texturas da cena. O índice vem por lote
// (push constant), uniforme no desenho inteiro.
layout(set=2, binding=0) uniform sampler2D projectTextures[];
#endif

layout(push_constant) uniform ParticleBatch {
  vec4 tint;
  vec4 scroll;
  vec4 noise;
  vec4 glow;
  vec4 stretch;  // w: bits do slot bindless da textura de projeto (flag 8)
  uvec4 layers;
  vec4 sheet;    // colunas, linhas da folha de textura; z = força da Distorção (px a 1 m)
} batch;

layout(location=0) in vec2 vUv;
layout(location=1) in vec4 vColor;
layout(location=2) in vec2 vNoiseOffset;
layout(location=3) in float vViewDepth;
layout(location=4) in vec2 vSheetUv;
layout(location=0) out vec4 outColor;

// Atlas 4x4 de blocos de 128 com borda de 4 texels (particle_textures.h).
const float kTile = 128.0;
const float kGutter = 4.0;
const float kAtlas = 512.0;
const uint kNone = 0xffffffffu;

vec4 sampleTile(uint layer, vec2 uv, bool wrap) {
  vec2 local = wrap ? fract(uv) : clamp(uv, 0.0, 1.0);
  vec2 origin = vec2(float(layer % 4u), float(layer / 4u)) * kTile + kGutter;
  float content = (kTile - 2.0 * kGutter) / kAtlas;
  // Derivadas da coordenada contínua: o salto do fract não escolhe o mip menor.
  return textureGrad(particleAtlas, origin / kAtlas + local * content, dFdx(uv) * content, dFdy(uv) * content);
}

void main() {
  float time = batch.tint.w;
  // Folha de textura: o quadro da partícula já foi recortado no vértice.
  bool sheet = batch.sheet.x > 1.0 || batch.sheet.y > 1.0;
  vec2 uv = sheet ? vSheetUv : vUv;
  float noise = 1.0;
  bool hasNoise = batch.layers.y != kNone;
  if(hasNoise) {
    noise = sampleTile(batch.layers.y, uv * batch.noise.x + batch.scroll.zw * time + vNoiseOffset, true).r;
    uv += (noise - 0.5) * batch.noise.z;
  }
  // Linha: U passa de 1 (repetição pelo comprimento), então a imagem repete.
  bool scrolls = batch.scroll.x != 0.0 || batch.scroll.y != 0.0 || (hasNoise && batch.noise.z > 0.0) || (batch.layers.w & 256u) != 0u;
  vec4 shape;
#ifdef PARTICLE_PROJECT_TEXTURES
  if((batch.layers.w & 8u) != 0u)
    shape = texture(projectTextures[floatBitsToUint(batch.stretch.w)], uv + batch.scroll.xy * time);
  else
#endif
  shape = sampleTile(batch.layers.x, uv + batch.scroll.xy * time, scrolls || sheet);
  float alpha = shape.a * vColor.a;
  if(hasNoise) alpha *= mix(1.0, noise, batch.noise.y);
  if(batch.layers.z != kNone) alpha *= sampleTile(batch.layers.z, vUv, false).r;
  vec3 rgb = shape.rgb * vColor.rgb * batch.tint.rgb;
  if(batch.glow.x > 0.0) {
    // Miolo denso esquenta: mais intenso e puxado para o branco, como chama.
    float core = pow(clamp(shape.a * noise, 0.0, 1.0), batch.glow.y) * batch.glow.x;
    float peak = max(rgb.r, max(rgb.g, rgb.b));
    rgb = rgb * (1.0 + core) + vec3(peak * core * 0.35);
  }
  if((batch.layers.w & 4u) != 0u) {
    // Partícula suave: esmaece quando a superfície da cena está perto atrás dela.
    float stored = texture(sceneDepth, gl_FragCoord.xy * batch.stretch.yz).r;
    float nearPlane = temporalFrame.temporalProjection.z, farPlane = temporalFrame.temporalProjection.w;
    float sceneZ = temporalFrame.temporalParameters.x > 0.0
        ? nearPlane + stored * (farPlane - nearPlane)
        : nearPlane * farPlane / max(farPlane - stored * (farPlane - nearPlane), 1.0e-6);
    alpha *= clamp((sceneZ - vViewDepth) / batch.noise.w, 0.0, 1.0);
  }
  alpha = clamp(alpha, 0.0, 1.0);
  if((batch.layers.w & 512u) != 0u) {
    // Distorção: a imagem é um mapa normal (xy em [0,1]). A do Material chega
    // decodificada como sRGB e volta à codificação original aqui; a forma
    // própria vira uma lente que empurra do centro para fora.
    vec2 bend;
    if((batch.layers.w & 8u) != 0u) {
      vec3 c = clamp(shape.rgb, 0.0, 1.0);
      vec3 encoded = mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
      bend = encoded.xy * 2.0 - 1.0;
    } else {
      bend = (uv - 0.5) * 2.0 * shape.a;
    }
    // Força em pixels a 1 m; cai com a distância, como a refração de perto.
    vec2 offset = bend * batch.sheet.z * vColor.a / max(vViewDepth, 0.25) * batch.stretch.yz;
    vec3 behind = texture(sceneCopy, gl_FragCoord.xy * batch.stretch.yz + offset).rgb;
    // Onde o normal é plano não há o que entortar: some sem marcar a borda.
    float weight = clamp(length(bend) * 6.0, 0.0, 1.0) * (hasNoise ? mix(1.0, noise, batch.noise.y) : 1.0);
    if((batch.layers.w & 4u) != 0u) {
      float stored = texture(sceneDepth, gl_FragCoord.xy * batch.stretch.yz).r;
      float nearPlane = temporalFrame.temporalProjection.z, farPlane = temporalFrame.temporalProjection.w;
      float sceneZ = temporalFrame.temporalParameters.x > 0.0
          ? nearPlane + stored * (farPlane - nearPlane)
          : nearPlane * farPlane / max(farPlane - stored * (farPlane - nearPlane), 1.0e-6);
      weight *= clamp((sceneZ - vViewDepth) / batch.noise.w, 0.0, 1.0);
    }
    outColor = vec4(behind, clamp(weight, 0.0, 1.0));
    return;
  }
  // Aditivo: a mistura é ONE,ONE na cor e preserva o alfa do destino.
  outColor = (batch.layers.w & 2u) != 0u ? vec4(rgb * alpha, 0.0) : vec4(rgb, alpha);
}
