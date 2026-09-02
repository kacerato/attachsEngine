#include "dirt_road_frame.glsl"
precision mediump int;
precision highp float;
// Mesma precisao de varying declarada em dirt_road.vert; divergir aqui faria os
// dois estagios discordarem sobre a mesma interface.
layout(location=3) in highp vec2 vUv0;
layout(location=4) in highp vec2 vUv1;
layout(location=5) in mediump vec4 vColor;

highp vec2 selectedCoverageUv() {
  return (frame.materialFlags.y&3u)==1u?vUv1:vUv0;
}

void main() {
  mediump float alpha=texture(BASE_MAP,selectedCoverageUv()).a*
              frame.baseColorFactor.a*vColor.a;
  mediump float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
  if(alpha<alphaCutoff) discard;
  // Sem saída de cor: este passe escreve somente a cobertura visível no depth.
}
