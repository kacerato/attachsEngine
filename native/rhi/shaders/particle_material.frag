#version 450
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

layout(push_constant) uniform ParticleBatch {
  vec4 tint;
  vec4 scroll;
  vec4 noise;
  vec4 glow;
  vec4 stretch;
  uvec4 layers;
} batch;

layout(location=0) in vec2 vUv;
layout(location=1) in vec4 vColor;
layout(location=2) in vec2 vNoiseOffset;
layout(location=3) in float vViewDepth;
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
  vec2 uv = vUv;
  float noise = 1.0;
  bool hasNoise = batch.layers.y != kNone;
  if(hasNoise) {
    noise = sampleTile(batch.layers.y, uv * batch.noise.x + batch.scroll.zw * time + vNoiseOffset, true).r;
    uv += (noise - 0.5) * batch.noise.z;
  }
  bool scrolls = batch.scroll.x != 0.0 || batch.scroll.y != 0.0 || (hasNoise && batch.noise.z > 0.0);
  vec4 shape = sampleTile(batch.layers.x, uv + batch.scroll.xy * time, scrolls);
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
  // Aditivo: a mistura é ONE,ONE na cor e preserva o alfa do destino.
  outColor = (batch.layers.w & 2u) != 0u ? vec4(rgb * alpha, 0.0) : vec4(rgb, alpha);
}
