#include "dirt_road_frame.glsl"
precision mediump int;
precision highp float;
// Mesma precisao de varying declarada em dirt_road.vert; divergir aqui faria os
// dois estagios discordarem sobre a mesma interface.
layout(location=3) in highp vec2 vUv0;
layout(location=4) in highp vec2 vUv1;
layout(location=5) in mediump vec4 vColor;
layout(location=6) in mediump float vDither;
#include "lod_dither.glsl"
#include "impostor_view.glsl"
#include "material_uv_transform.glsl"

highp vec2 selectedCoverageUv() {
  // A mesma UV do passe de cor: recorte e cor precisam cair no mesmo texel.
  return aetherTransformUv(0u,(frame.materialFlags.y&3u)==1u?vUv1:vUv0);
}

void main() {
  highp vec2 dx=dFdx(vUv0),dy=dFdy(vUv0);
  // This must be bit-identical to the shade pass. Previously the prepass wrote
  // both LODs at full coverage while depth-EQUAL shading discarded a
  // complementary subset, leaving holes whenever their surfaces differed.
  if(aetherLodDitherDiscard(gl_FragCoord.xy,vDither)) discard;
  bool impostor=(frame.materialFlags.x&256u)!=0u && (frame.materialFlags.y>>16u)!=0u;
  highp vec2 uv=selectedCoverageUv();
  if(impostor) {
    uv=aetherImpostorViewUv(frame.materialFlags.x,vUv0,vUv1,vColor.a,gl_FragCoord.xy);
    uv=aetherImpostorSafeUv(uv,dx,dy,vec2(textureSize(BASE_MAP,0)),
        frame.materialFlags.y>>16u,float(textureQueryLevels(BASE_MAP)-1));
  }
  mediump float sampledAlpha=impostor?
      textureGrad(BASE_MAP,uv,dx,dy).a:texture(BASE_MAP,uv).a;
  mediump float alpha=sampledAlpha*frame.baseColorFactor.a*(impostor?1.0:vColor.a);
  mediump float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
  if(alpha<alphaCutoff) discard;
  // Sem saída de cor: este passe escreve somente a cobertura visível no depth.
}
