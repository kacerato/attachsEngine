#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(location=0) out vec3 vDirection;
void main() {
  vec2 positions[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
  vec2 ndc=positions[gl_VertexIndex];
  vec2 cameraNdc=vec2(dot(frame.surfaceTransform.xz,ndc),dot(frame.surfaceTransform.yw,ndc));
  float focal=frame.materialFactors.w>0.0?frame.materialFactors.w:1.732050808;
  // SEM `normalize` aqui: o raio de camera (x,y,1) e afim em NDC e interpola
  // exato; o versor nao e. Normalizar nos vertices de um triangulo de tela
  // inteira -- cujos cantos ficam em NDC 3 -- e interpolar devolve, no meio da
  // tela, uma direcao dezenas de graus fora da que o pixel realmente enxerga.
  // O ceu e um gradiente e tolerava o erro; o fragmento normaliza.
  vDirection=dirtRoadCameraRotation(frame.materialFactors.x)*vec3(cameraNdc.x*frame.cameraFrame.x/focal,
                                           -cameraNdc.y/focal,1.0);
  if(uintBitsToFloat(frame.textureIndices.w)>0.0)
    vDirection=dirtRoadCameraRotation(frame.materialFactors.x)*vec3(0,0,1);
  gl_Position=vec4(ndc,1,1);
}
