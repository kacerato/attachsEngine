#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(location=0) out vec3 vRay;
layout(location=1) out vec3 vOriginOffset;

// O raio de camera de cada pixel, reconstruido invertendo EXATAMENTE a projecao
// que o vertice da cena aplica (dirt_road_vertex.glsl):
//
//   xy       = (view.x*focal/aspect, -view.y*focal)
//   projetado= M * xy                 , M = [[st.x,st.y],[st.z,st.w]]
//   ndc      = projetado/view.z + jitter
//
// Invertendo, com M ortogonal (a transformacao de superficie e uma rotacao de
// multiplo de 90 graus, entao M^-1 = M^T):
//
//   c        = M^T * (ndc - jitter)
//   raio     = (c.x*aspect/focal, -c.y/focal, 1)
void main() {
  vec2 positions[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
  vec2 ndc=positions[gl_VertexIndex];
  // O deslocamento temporal entra DEPOIS da divisao por view.z na cena, entao
  // aqui ele sai ANTES de desfazer a projecao. Sem isto a grade anda meio pixel
  // por quadro contra a geometria e o resolve temporal a borra.
  vec2 sampled=ndc-frame.baseColorFactor.xy;
  vec2 cameraNdc=vec2(dot(frame.surfaceTransform.xz,sampled),dot(frame.surfaceTransform.yw,sampled));
  float focal=frame.materialFactors.w>0.0?frame.materialFactors.w:1.732050808;
  // SEM `normalize` aqui, e este e o ponto todo.
  //
  // O raio de camera (x,y,1) e AFIM em NDC, logo interpola exato entre os tres
  // vertices. O versor NAO e: normalizar nos vertices e interpolar devolve uma
  // direcao que nao e o raio de pixel nenhum. Com o triangulo de tela inteira,
  // cujos vertices ficam em NDC 3 -- muito fora da tela --, os tres versores tem
  // comprimentos originais bem diferentes e o erro no MEIO da tela chega a
  // dezenas de graus: o plano do chao passava a ser intersectado por raios que
  // nao eram os da camera, e a grade saia torta e deslocada do horizonte da
  // cena. Interpolar o raio cru e normalizar no fragmento (quando precisar) e a
  // unica forma correta.
  vRay=dirtRoadCameraRotation(frame.baseColorFactor.z)*vec3(cameraNdc.x*frame.cameraFrame.x/focal,
                                     -cameraNdc.y/focal,1.0);
  vOriginOffset=vec3(0);
  float halfHeight=uintBitsToFloat(frame.textureIndices.w);
  if(halfHeight>0.0) {
    mat3 rotation=dirtRoadCameraRotation(frame.baseColorFactor.z);
    vRay=rotation*vec3(0,0,1);
    vOriginOffset=rotation*vec3(cameraNdc.x*frame.cameraFrame.x*halfHeight,-cameraNdc.y*halfHeight,0);
  }
  // Profundidade 1: o fragmento escreve a sua propria, a partir do ponto de
  // interseccao. O que sai daqui so precisa cobrir a tela.
  gl_Position=vec4(ndc,1,1);
}
