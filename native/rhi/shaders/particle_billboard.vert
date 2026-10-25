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
#include "particle_orientation.glsl"

layout(location=0) in vec4 inPositionSize;
layout(location=1) in vec4 inVelocityRotation;
layout(location=2) in vec4 inColor;
layout(location=3) in uint inRandom;
layout(location=4) in float inFrame;  // PK-10a: quadro da folha de textura
layout(location=5) in vec2 inSizeYZ;  // G6a: altura e profundidade
layout(location=6) in vec4 inOrientation;  // G6a: quatérnio (snorm16)

layout(push_constant) uniform ParticleBatch {
  vec4 tint;     // rgb tinta x emissão, w tempo
  vec4 scroll;   // principal xy, ruído zw
  vec4 noise;    // escala, recorte, distorção, distância suave
  vec4 glow;     // brilho central, concentração, tamanho máximo na tela, escala do comprimento
  vec4 stretch;  // escala da velocidade, 1/largura, 1/altura, slot da textura de projeto
  uvec4 layers;  // principal, ruído, máscara, flags
  vec4 sheet;    // colunas, linhas da folha de textura
} batch;

layout(location=0) out vec2 vUv;
layout(location=1) out vec4 vColor;
layout(location=2) out vec2 vNoiseOffset;
layout(location=3) out float vViewDepth;
layout(location=4) out vec2 vSheetUv;

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
  float height = inSizeYZ.x;
  bool orthographic = temporalFrame.temporalParameters.x > 0.0;
  float focal = orthographic ? 1.0 / temporalFrame.temporalParameters.x : temporalFrame.temporalProjection.x;
  float centerDepth = orthographic ? 1.0 : max(temporalViewPosition(center, false).z, 1.0e-3);
  // Tamanho máximo na tela (fração da altura): limita o overdraw de partículas
  // que passam rente à câmera. Altura do NDC é 2. A altura acompanha a largura.
  if(batch.glow.z > 0.0) {
    float limited = min(size, batch.glow.z * 2.0 * centerDepth / focal);
    height *= size > 0.0 ? limited / size : 0.0;
    size = limited;
  }

  vec3 world;
  vec3 toCamera = orthographic ? -forward : normalize(camera - center);
  if((batch.layers.w & 16u) != 0u) {
    // Orientada (G6a): o quadrado no plano XY da partícula, girado pelo
    // quatérnio e levado à base do alinhamento.
    uint alignment = (batch.layers.w >> 5u) & 3u;
    mat3 basis = particleAlignmentBasis(alignment, right, up, toCamera, inVelocityRotation.xyz);
    vec3 local = vec3(corner.x * 0.5 * size, corner.y * 0.5 * height, 0.0);
    world = center + basis * particleRotate(normalize(inOrientation), local);
  } else if((batch.layers.w & 1u) != 0u) {
    // Esticado: o eixo longo segue a velocidade projetada no plano da câmera;
    // a cabeça fica na posição da partícula e a cauda para trás.
    vec3 velocity = inVelocityRotation.xyz;
    vec3 axis = velocity - toCamera * dot(velocity, toCamera);
    float planar = length(axis);
    axis = planar > 1.0e-4 ? axis / planar : up;
    vec3 side = normalize(cross(axis, toCamera));
    float lengthWorld = max(size * batch.glow.w + length(velocity) * batch.stretch.x, size * 0.25);
    world = center + side * (corner.x * 0.5 * size) + axis * ((corner.y - 1.0) * 0.5 * lengthWorld);
  } else {
    float c = cos(inVelocityRotation.w), s = sin(inVelocityRotation.w);
    vec2 scaled = corner * vec2(size, height) * 0.5;
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);
    world = center + right * rotated.x + up * rotated.y;
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
  // Linha (G6a): U do início (cauda) ao fim (cabeça) do trecho, V atravessa.
  if((batch.layers.w & 256u) != 0u) vUv = vec2(corner.y * 0.5 + 0.5, corner.x * 0.5 + 0.5);
  vColor = inColor;
  vNoiseOffset = vec2(float(inRandom & 0xffffu), float(inRandom >> 16u)) * (1.0 / 65535.0);
  vViewDepth = view.z;
  // Quadro N da grade: da esquerda para a direita, de cima para baixo, como a
  // Texture Sheet Animation da Unity.
  vec2 tiles = max(batch.sheet.xy, vec2(1.0));
  float frame = floor(inFrame + 0.5);
  float column = mod(frame, tiles.x);
  float row = min(floor(frame / tiles.x), tiles.y - 1.0);
  vSheetUv = (vec2(column, row) + vUv) / tiles;
}
