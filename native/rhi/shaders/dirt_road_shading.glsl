#include "dirt_road_frame.glsl"
// A precisao dos varyings precisa casar com dirt_road.vert.
layout(location=0) in highp vec3 vPosition;
layout(location=1) in mediump vec3 vNormal;
layout(location=2) in mediump vec4 vTangent;
layout(location=3) in highp vec2 vUv0;
layout(location=4) in highp vec2 vUv1;
layout(location=5) in mediump vec4 vColor;
layout(location=6) in mediump float vDither;
layout(location=0) out vec4 outColor;
layout(constant_id=0) const uint GPU_COST_ISOLATION=0u;
const float PI=3.141592653589793;
#include "environment_lighting.glsl"
// LOD cross-fade (see renderer::selectLodLevel): vDither==0 for every draw
// outside an active transition, so this is a no-op discard everywhere LOD is
// disabled or a group has only one level. The two draws sharing a
// transitioning lodGroupId carry signed complementary masks: outgoing uses
// +factor and keeps threshold>=factor; incoming uses -factor and keeps
// threshold<factor. The prior `1-factor` encoding made one mask a subset of
// the other, causing double shading/overdraw instead of a cross-fade.
const float BAYER4X4[16]=float[16](
  0.0/16.0,8.0/16.0,2.0/16.0,10.0/16.0,
  12.0/16.0,4.0/16.0,14.0/16.0,6.0/16.0,
  3.0/16.0,11.0/16.0,1.0/16.0,9.0/16.0,
  15.0/16.0,7.0/16.0,13.0/16.0,5.0/16.0);
bool ditherDiscard(highp vec2 fragCoord,mediump float dither) {
  if(dither==0.0) return false;
  ivec2 cell=ivec2(fragCoord)&3;
  mediump float threshold=BAYER4X4[cell.y*4+cell.x];
  return dither>0.0?threshold<dither:threshold>=-dither;
}
highp vec2 selectedUv(uint slot) { return ((frame.materialFlags.y>>(slot*2))&3u)==1u?vUv1:vUv0; }
// Precisão explícita (item 2.2.5 do plano). Até aqui o fragmento inteiro rodava
// em highp por omissão: em Adreno/Mali a ALU fp16 roda ao dobro da taxa e ocupa
// metade dos registradores, e mais registradores livres significam mais waves em
// voo para esconder latência de textura.
//
// A divisão NÃO é "tudo mediump". Cor, normal e parâmetros de material vivem em
// [0,1] e cabem folgados em fp16. A numérica do GGX não: `rough` é limitado a
// 0,07, então `alpha*alpha` vale 2,4e-5 — abaixo do menor normal do fp16
// (6,1e-5). Em mediump esse termo vira subnormal ou zero e o brilho especular
// desaparece nas superfícies mais lisas. Por isso `distribution`, `visibility` e
// os produtos escalares que as alimentam permanecem highp.
mediump float pow5(mediump float value) {
  mediump float squared=value*value;return squared*squared*value; }
mediump vec3 fresnel(mediump vec3 f0,mediump float vh) { return f0+(1-f0)*pow5(1-vh); }
highp float distribution(highp float nh,highp float alpha) {
  highp float a2=alpha*alpha,d=nh*nh*(a2-1)+1;return a2/(PI*d*d); }
highp float visibility(highp float nv,highp float nl,highp float alpha) {
  highp float a2=alpha*alpha;return .5/max(
  nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2),1e-6); }
mediump vec3 directLight(mediump vec3 n,mediump vec3 v,mediump vec3 l,mediump vec3 radiance,
                         mediump vec3 base,mediump vec3 f0,mediump float metal,
                         mediump float rough) {
  highp float nv=max(dot(n,v),1e-4),nl=max(dot(n,l),0);mediump vec3 h=normalize(v+l);
  highp float nh=max(dot(n,h),0);mediump float lh=max(dot(l,h),0);
  mediump vec3 f=fresnel(f0,lh);highp float alpha=rough*rough;
  mediump float fd90=.5+2*rough*lh*lh;
  mediump float fd=(1+(fd90-1)*pow5(1-float(nl)))*(1+(fd90-1)*pow5(1-float(nv)))/PI;
  mediump float specular=float(distribution(nh,alpha)*visibility(nv,nl,alpha));
  return ((1-f)*(1-metal)*base*fd+specular*f)*radiance*float(nl);
}
void main() {
  if(ditherDiscard(gl_FragCoord.xy,vDither)) discard;
  uint flags=frame.materialFlags.x;
  uint isolation=GPU_COST_ISOLATION;
  mediump vec4 baseSample=texture(BASE_MAP,selectedUv(0));
  mediump vec4 base=baseSample*frame.baseColorFactor*vColor;
  // Fully transparent atlas texels cannot affect the framebuffer. Rejecting
  // them before normal/PBR work preserves the exact 8-bit alpha result and is
  // particularly important for dense forest cards.
  if((flags&1u)!=0u && base.a<(1.0/255.0)) discard;
  // BaseColorOnly keeps the exact alpha/depth coverage and geometry while
  // avoiding material/lighting fetches. The branch is uniform for the full run.
  if(isolation==3u) {
    vec3 color=base.rgb;
    if((frame.materialFlags.z&1u)!=0u)
      color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                greaterThan(color,vec3(.0031308)));
    outColor=vec4(color,base.a);
    return;
  }
  mediump vec3 mr=(flags&4u)!=0u?texture(MR_MAP,selectedUv(2)).rgb:vec3(1);
  mediump float rough=clamp(mr.g*frame.materialFactors.x,.07,1);
  mediump float metal=clamp(mr.b*frame.materialFactors.y,0,1);
  mediump vec3 n=normalize(vNormal);
  if((flags&2u)!=0u && isolation!=1u) {
    mediump vec3 t=normalize(vTangent.xyz-n*dot(n,vTangent.xyz));
    mediump vec3 b=cross(n,t)*vTangent.w;
    mediump vec3 detail=texture(NORMAL_MAP,selectedUv(1)).xyz*2-1;
    // Normalizing before and after an orthonormal TBN is redundant. Keep the
    // final normalization (same direction, one fewer reciprocal sqrt).
    detail.xy*=frame.materialFactors.z;n=normalize(mat3(t,b,n)*detail);
  }
  // A subtração fica em highp: `eye` e `vPosition` são coordenadas de mundo e o
  // mapa se estende por centenas de unidades, onde fp16 já perde resolução. Só
  // o resultado normalizado, que vive em [-1,1], desce para mediump.
  highp vec3 eye=frame.cameraPositionNear.xyz;
  mediump vec3 v=normalize(eye-vPosition);
  mediump vec3 f0=mix(vec3(.04*frame.materialFactors.w),base.rgb,metal);
  mediump vec3 sunRadiance=environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  mediump vec3 color=directLight(n,v,environment.sunDirectionIntensity.xyz,sunRadiance,
                         base.rgb,f0,metal,rough);
  mediump float nv=max(dot(n,v),0.0);
  // A single global hemispherical irradiance keeps upward-facing foliage tied
  // to the sky while giving downward/vertical surfaces a neutral ground bounce.
  // This is constant-time ALU: no extra pass, draw call or texture fetch.
  mediump float skyWeight=clamp(n.y*.5+.5,0.0,1.0);
  mediump vec3 ambientIrradiance=mix(environment.groundColorSaturation.rgb,
                             environment.ambientColorStrength.rgb,skyWeight);
  color+=(1-metal)*base.rgb*ambientIrradiance*environment.ambientColorStrength.w;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  if(isolation!=2u && (metal>.01 || rough<.65)) {
    mediump vec3 reflection=reflect(-v,n);
    mediump vec3 specularEnvironment=environmentRadiance(reflection,rough*environment.parameters.z);
    mediump vec3 environmentFresnel=fresnel(f0,nv);
    color+=specularEnvironment*environmentFresnel*(.25+.75*(1.0-rough))*frame.materialFactors.w;
  }
  if((flags&8u)!=0u) color+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*frame.emissiveFactorStrength.a;
  color=toneMapEnvironment(color);
  if((frame.materialFlags.z&1u)!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                             greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,base.a);
}
