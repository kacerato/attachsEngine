#include "dirt_road_frame.glsl"
layout(location=3) in vec2 vUv0;
layout(location=4) in vec2 vUv1;
layout(location=5) in vec4 vColor;

vec2 selectedCoverageUv() {
  return (frame.materialFlags.y&3u)==1u?vUv1:vUv0;
}

void main() {
  float alpha=texture(BASE_MAP,selectedCoverageUv()).a*
              frame.baseColorFactor.a*vColor.a;
  float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
  if(alpha<alphaCutoff) discard;
  // Sem saída de cor: este passe escreve somente a cobertura visível no depth.
}
