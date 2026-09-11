#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(location=0) out vec3 vDirection;
// Triangulo de tela inteira, igual ao do ceu: a grade e um plano analitico, nao
// uma lista de segmentos. O fragmento intersecta o raio com o plano editorial e
// desenha a celula onde ele cair.
void main() {
  vec2 positions[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
  vec2 ndc=positions[gl_VertexIndex];
  vec2 cameraNdc=vec2(dot(frame.surfaceTransform.xz,ndc),dot(frame.surfaceTransform.yw,ndc));
  float focal=frame.materialFactors.w>0.0?frame.materialFactors.w:1.732050808;
  vec3 view=normalize(vec3(cameraNdc.x*frame.cameraFrame.x/focal,
                           -cameraNdc.y/focal,1));
  vDirection=dirtRoadCameraRotation()*view;
  // Profundidade 1: o fragmento escreve a sua propria, a partir do ponto de
  // interseccao. O que sai daqui so precisa cobrir a tela.
  gl_Position=vec4(ndc,1,1);
}
