#version 450
// Partícula de malha (G6a; Unity Particle System Renderer, Render Mode Mesh).
//
// A malha do lote vem por vértice (posição e UV, ParticleMeshVertex) e a
// partícula por instância, com o mesmo layout do billboard
// (renderer/particles/particle_simulation.h). O vértice é escalado pelos três
// tamanhos, girado pelo quatérnio da partícula e levado à base do alinhamento;
// a projeção é a da cena, como no billboard. O fragmento é o mesmo material.
#define AETHER_TEMPORAL_SET 0
#define AETHER_TEMPORAL_BINDING 0
#include "temporal_projection.glsl"
#include "particle_orientation.glsl"

layout(location=0) in vec3 inMeshPosition;
layout(location=1) in vec2 inMeshUv;
layout(location=2) in vec4 inPositionSize;
layout(location=3) in vec4 inVelocityRotation;
layout(location=4) in vec4 inColor;
layout(location=5) in uint inRandom;
layout(location=6) in float inFrame;
layout(location=7) in vec2 inSizeYZ;
layout(location=8) in vec4 inOrientation;

layout(push_constant) uniform ParticleBatch {
  vec4 tint;
  vec4 scroll;
  vec4 noise;
  vec4 glow;
  vec4 stretch;
  uvec4 layers;
  vec4 sheet;
} batch;

layout(location=0) out vec2 vUv;
layout(location=1) out vec4 vColor;
layout(location=2) out vec2 vNoiseOffset;
layout(location=3) out float vViewDepth;
layout(location=4) out vec2 vSheetUv;

void main() {
  vec3 right = temporalFrame.temporalCurrentView[0].xyz;
  vec3 up = temporalFrame.temporalCurrentView[1].xyz;
  vec3 forward = temporalFrame.temporalCurrentView[2].xyz;
  vec3 camera = vec3(temporalFrame.temporalCurrentView[0].w, temporalFrame.temporalCurrentView[1].w,
                     temporalFrame.temporalCurrentView[2].w);
  vec3 center = inPositionSize.xyz;
  bool orthographic = temporalFrame.temporalParameters.x > 0.0;
  float focal = orthographic ? 1.0 / temporalFrame.temporalParameters.x : temporalFrame.temporalProjection.x;
  vec3 toCamera = orthographic ? -forward : normalize(camera - center);

  uint alignment = (batch.layers.w >> 5u) & 3u;
  mat3 basis = particleAlignmentBasis(alignment, right, up, toCamera, inVelocityRotation.xyz);
  vec3 scaled = inMeshPosition * vec3(inPositionSize.w, inSizeYZ.x, inSizeYZ.y);
  vec3 world = center + basis * particleRotate(normalize(inOrientation), scaled);

  vec3 view = temporalViewPosition(world, false);
  float nearPlane = temporalFrame.temporalProjection.z, farPlane = temporalFrame.temporalProjection.w;
  vec2 xy = vec2(view.x * focal / temporalFrame.temporalProjection.y, -view.y * focal);
  vec2 projected = vec2(dot(temporalFrame.temporalSurface.xy, xy), dot(temporalFrame.temporalSurface.zw, xy));
  float clipW = orthographic ? 1.0 : view.z;
  projected += temporalFrame.temporalJitter.xy * clipW;
  gl_Position = vec4(projected,
      orthographic ? (view.z - nearPlane) / (farPlane - nearPlane)
                   : (farPlane * view.z - nearPlane * farPlane) / (farPlane - nearPlane), clipW);
  vUv = inMeshUv;
  vColor = inColor;
  vNoiseOffset = vec2(float(inRandom & 0xffffu), float(inRandom >> 16u)) * (1.0 / 65535.0);
  vViewDepth = view.z;
  vec2 tiles = max(batch.sheet.xy, vec2(1.0));
  float frame = floor(inFrame + 0.5);
  float column = mod(frame, tiles.x);
  float row = min(floor(frame / tiles.x), tiles.y - 1.0);
  vSheetUv = (vec2(column, row) + fract(inMeshUv)) / tiles;
}
