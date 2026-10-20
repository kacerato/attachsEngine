#version 450
// Partícula billboard e billboard esticado (G5 P1).
//
// Um quadrado por instância, sem buffer de vértices: os seis cantos saem de
// gl_VertexIndex e a instância traz centro, tamanho, velocidade, giro, cor e a
// semente da partícula (renderer/particles/particle_simulation.h).
//
// A projeção é a mesma do vértice da cena (dirt_road_vertex.glsl), lida do
// contrato temporal do UBO do quadro: mesma base de câmera, focal, pré-rotação
// da superfície e jitter. Sem isso a partícula oscilaria meio pixel contra a
// geometria com TAA ligado.
#define AETHER_TEMPORAL_SET 0
#define AETHER_TEMPORAL_BINDING 0
#include "temporal_projection.glsl"

layout(location=0) in vec4 inPositionSize;
layout(location=1) in vec4 inVelocityRotation;
layout(location=2) in vec4 inColor;
layout(location=3) in uint inRandom;

layout(push_constant) uniform ParticleBatch {
  vec4 tint;     // rgb tinta x emissão, w tempo
  vec4 scroll;   // principal xy, ruído zw
  vec4 noise;    // escala, recorte, distorção, distância suave
  vec4 glow;     // brilho central, concentração, tamanho máximo na tela, escala do comprimento
  vec4 stretch;  // escala da velocidade, 1/largura, 1/altura, reservado
  uvec4 layers;  // principal, ruído, máscara, flags
} batch;

layout(location=0) out vec2 vUv;
layout(location=1) out vec4 vColor;
layout(location=2) out vec2 vNoiseOffset;
layout(location=3) out float vViewDepth;

const vec2 kCorners[6] = vec2[](vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0),
                                vec2(-1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));

void main() {
  vec2 corner = kCorners[gl_VertexIndex];
  vec3 right = temporalFrame.temporalCurrentView[0].xyz;
  vec3 up = temporalFrame.temporalCurrentView[1].xyz;
  vec3 forward = temporalFrame.temporalCurrentView[2].xyz;
  vec3 camera = vec3(temporalFrame.temporalCurrentView[0].w, temporalFrame.temporalCurrentView[1].w,
                     temporalFrame.temporalCurrentView[2].w);
  vec3 center = inPositionSize.xyz;
  float size = inPositionSize.w;
  bool orthographic = temporalFrame.temporalParameters.x > 0.0;
  float focal = orthographic ? 1.0 / temporalFrame.temporalParameters.x : temporalFrame.temporalProjection.x;
  float centerDepth = orthographic ? 1.0 : max(temporalViewPosition(center, false).z, 1.0e-3);
  // Tamanho máximo na tela (fração da altura): limita o overdraw de partículas
  // que passam rente à câmera. Altura do NDC é 2.
  if(batch.glow.z > 0.0) size = min(size, batch.glow.z * 2.0 * centerDepth / focal);

  vec3 world;
  if((batch.layers.w & 1u) != 0u) {
    // Esticado: o eixo longo segue a velocidade projetada no plano da câmera;
    // a cabeça fica na posição da partícula e a cauda para trás.
    vec3 velocity = inVelocityRotation.xyz;
    vec3 toCamera = orthographic ? -forward : normalize(camera - center);
    vec3 axis = velocity - toCamera * dot(velocity, toCamera);
    float planar = length(axis);
    axis = planar > 1.0e-4 ? axis / planar : up;
    vec3 side = normalize(cross(axis, toCamera));
    float lengthWorld = max(size * batch.glow.w + length(velocity) * batch.stretch.x, size * 0.25);
    world = center + side * (corner.x * 0.5 * size) + axis * ((corner.y - 1.0) * 0.5 * lengthWorld);
  } else {
    float c = cos(inVelocityRotation.w), s = sin(inVelocityRotation.w);
    vec2 rotated = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    world = center + (right * rotated.x + up * rotated.y) * (0.5 * size);
  }

  vec3 view = temporalViewPosition(world, false);
  float nearPlane = temporalFrame.temporalProjection.z, farPlane = temporalFrame.temporalProjection.w;
  vec2 xy = vec2(view.x * focal / temporalFrame.temporalProjection.y, -view.y * focal);
  vec2 projected = vec2(dot(temporalFrame.temporalSurface.xy, xy), dot(temporalFrame.temporalSurface.zw, xy));
  float clipW = orthographic ? 1.0 : view.z;
  projected += temporalFrame.temporalJitter.xy * clipW;
  gl_Position = vec4(projected,
      orthographic ? (view.z - nearPlane) / (farPlane - nearPlane)
                   : (farPlane * view.z - nearPlane * farPlane) / (farPlane - nearPlane), clipW);
  vUv = vec2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);
  vColor = inColor;
  vNoiseOffset = vec2(float(inRandom & 0xffffu), float(inRandom >> 16u)) * (1.0 / 65535.0);
  vViewDepth = view.z;
}
