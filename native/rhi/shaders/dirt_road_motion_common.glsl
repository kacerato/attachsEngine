#include "dirt_road_frame.glsl"
precision mediump int;
precision highp float;
layout(early_fragment_tests) in;
layout(location=0) in highp vec3 vPosition;
layout(location=1) in mediump vec3 vNormal;
layout(location=3) in highp vec2 vUv0;
layout(location=4) in highp vec2 vUv1;
layout(location=5) in mediump vec4 vColor;
layout(location=6) in mediump float vDither;
layout(location=10) in highp vec3 vMotionWorldDelta;
// RG16F, contrato de temporal_projection.glsl: atual -> anterior, UV da
// extensão renderizada, sem jitter.
layout(location=0) out highp vec2 outMotion;
#include "temporal_projection.glsl"
#include "lod_dither.glsl"
#include "material_uv_transform.glsl"
#include "world_uv.glsl"

void main() {
  if(aetherLodDitherDiscard(gl_FragCoord.xy,vDither)) discard;
  // Opaque and alpha-clipped rigid geometry share the same pass. Match the
  // color pass's UV, alpha source and cutoff before publishing a vector.
  if((frame.materialFlags.x&16u)!=0u) {
    uint set=frame.materialFlags.y&3u;
    highp vec2 uv=aetherTransformUv(0u,set==2u?
        aetherWorldUv(vPosition,vNormal):set==1u?vUv1:vUv0);
    mediump float alpha=aetherAlphaFromSource(texture(BASE_MAP,uv))*
        frame.baseColorFactor.a*vColor.a;
    if(alpha<float((frame.materialFlags.y>>8u)&255u)/255.0) discard;
  }
  // vMotionWorldDelta é o deslocamento de mundo desde o quadro anterior do
  // mesmo vértice: pose rígida anterior ou, com skin, a posição deformada
  // anterior. A câmera anterior entra aqui, na projeção.
  outMotion=temporalVelocity(vPosition,vPosition-vMotionWorldDelta);
}
