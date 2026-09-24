// Contrato temporal único do renderer (G6-B).
//
// Todas as entradas temporais -- vetor de movimento, máscaras, jitter e poses
// de câmera -- saem desta convenção, consumida igualmente pelo TAA nativo,
// pelo Arm ASR e pelo AMD FSR 2:
//
// * Espaço: UV da extensão renderizada (0..1 sobre renderWidth x renderHeight,
//   já com a rotação de pré-transformação da superfície aplicada), a mesma
//   imagem normalizada que o pós lê como `vUv`.
// * Direção: atual -> anterior. `anterior = atual + vetor`.
// * Jitter: nenhum. As duas posições são calculadas sem o deslocamento de
//   subpixel; o jitter do quadro vai separado para quem precisa dele.
// * Unidade para bibliotecas: pixels = vetor * extensão renderizada (é o
//   `motionVectorScale` do FSR 2/Arm ASR).
//
// O bloco reaproveita o UBO do quadro (DirtRoadFrameUniform); os membros são
// declarados por offset explícito, como no pós, porque cada estágio lê apenas
// a fatia de que precisa da mesma memória.
#ifndef AETHER_TEMPORAL_PROJECTION_GLSL
#define AETHER_TEMPORAL_PROJECTION_GLSL

#ifndef AETHER_TEMPORAL_SET
#define AETHER_TEMPORAL_SET 1
#define AETHER_TEMPORAL_BINDING 0
#endif
layout(set=AETHER_TEMPORAL_SET,binding=AETHER_TEMPORAL_BINDING,std140) uniform TemporalFrameBlock {
  layout(offset=3296) vec4 postViewport;          // xy/extent normalizados da vista na extensão
  layout(offset=3328) vec4 temporalCurrentView[3];  // linhas world->view; w = posição da câmera
  layout(offset=3376) vec4 temporalPreviousView[3]; // idem no quadro anterior
  layout(offset=3424) vec4 temporalProjection;      // focal vertical, aspecto, near, far
  layout(offset=3440) vec4 temporalJitter;          // atual xy, anterior zw, NDC físico
  layout(offset=3456) vec4 temporalSurface;         // pré-transformação xx xy yx yy
  layout(offset=3472) vec4 temporalParameters;      // meia altura ortográfica, histórico válido
} temporalFrame;

highp vec3 temporalViewPosition(highp vec3 world, bool previous) {
  highp vec3 relative;
  if(previous) {
    relative=world-vec3(temporalFrame.temporalPreviousView[0].w,
                        temporalFrame.temporalPreviousView[1].w,
                        temporalFrame.temporalPreviousView[2].w);
    return vec3(dot(temporalFrame.temporalPreviousView[0].xyz,relative),
                dot(temporalFrame.temporalPreviousView[1].xyz,relative),
                dot(temporalFrame.temporalPreviousView[2].xyz,relative));
  }
  relative=world-vec3(temporalFrame.temporalCurrentView[0].w,
                      temporalFrame.temporalCurrentView[1].w,
                      temporalFrame.temporalCurrentView[2].w);
  return vec3(dot(temporalFrame.temporalCurrentView[0].xyz,relative),
              dot(temporalFrame.temporalCurrentView[1].xyz,relative),
              dot(temporalFrame.temporalCurrentView[2].xyz,relative));
}

// Projeção idêntica à do vértice da cena (dirt_road_vertex.glsl), sem jitter.
highp vec2 temporalProjectView(highp vec3 view) {
  highp float halfHeight=temporalFrame.temporalParameters.x;
  bool orthographic=halfHeight>0.0;
  highp float focal=orthographic?1.0/halfHeight:temporalFrame.temporalProjection.x;
  highp vec2 xy=vec2(view.x*focal/temporalFrame.temporalProjection.y,-view.y*focal);
  highp vec2 projected=vec2(dot(temporalFrame.temporalSurface.xy,xy),dot(temporalFrame.temporalSurface.zw,xy));
  return orthographic?projected:projected/max(view.z,1.0e-6);
}

highp vec2 temporalRenderUvFromNdc(highp vec2 physicalNdc) {
  return temporalFrame.postViewport.xy+(physicalNdc*0.5+0.5)*temporalFrame.postViewport.zw;
}

// Vetor atual -> anterior de um ponto de mundo que se moveu de `previousWorld`
// para `currentWorld`, em UV da extensão renderizada.
highp vec2 temporalVelocity(highp vec3 currentWorld, highp vec3 previousWorld) {
  highp vec2 current=temporalProjectView(temporalViewPosition(currentWorld,false));
  highp vec2 previous=temporalProjectView(temporalViewPosition(previousWorld,true));
  return (previous-current)*0.5*temporalFrame.postViewport.zw;
}

#endif
