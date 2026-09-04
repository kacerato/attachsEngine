#include "dirt_road_frame.glsl"
precision mediump int;
precision highp float;
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
layout(constant_id=1) const uint MATERIAL_FEATURE_MASK=0xffffffffu;
// Projecao da sonda de ambiente resolvida em tempo de compilacao de pipeline.
// O ramo legado equirect custa atan/acos/fract e, sobretudo, registradores em
// TODO fragmento enquanto for um uniform -- o driver nao pode provar que ele e
// morto. Como spec constant ele desaparece do binario do perfil que usa AEEN v3.
// Zero e "decidir em runtime", preservando o comportamento anterior.
layout(constant_id=2) const uint ENVIRONMENT_PROJECTION=0u;
const float PI=3.141592653589793;
const uint MATERIAL_IMPOSTOR=256u; // renderer::MapMaterialImpostor
#include "environment_lighting.glsl"
// LOD cross-fade (see renderer::selectLodLevel): vDither==0 for every draw
// outside an active transition, so this is a no-op discard everywhere LOD is
// disabled or a group has only one level. The two draws sharing a
// transitioning lodGroupId carry signed complementary masks: outgoing uses
// +factor and keeps threshold>=factor; incoming uses -factor and keeps
// threshold<factor. The prior `1-factor` encoding made one mask a subset of
// the other, causing double shading/overdraw instead of a cross-fade.
#include "lod_dither.glsl"
highp vec2 selectedUv(uint slot) { return ((frame.materialFlags.y>>(slot*2))&3u)==1u?vUv1:vUv0; }
bool hasMaterialFeature(uint flags,uint feature) {
  uint selected=MATERIAL_FEATURE_MASK==0xffffffffu?flags:MATERIAL_FEATURE_MASK;
  return (selected&feature)!=0u;
}
mediump float materialDetailWeight(highp float cameraDistance, highp float maximumDistance) {
  if(maximumDistance<=0.0) return 1.0;
  mediump float band=clamp(environment.materialDistanceParameters.z,0.0,0.5);
  if(band<=0.0) return cameraDistance<=maximumDistance?1.0:0.0;
  return 1.0-smoothstep(maximumDistance*(1.0-band),maximumDistance,cameraDistance);
}
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
  // O hemisferio oposto contribui exatamente zero: o retorno inteiro e
  // multiplicado por nl. Sair aqui poupa normalize(v+l), GGX, visibilidade e
  // Fresnel numa fracao grande dos fragmentos de uma floresta, onde metade das
  // folhas esta de costas para o sol -- e nao muda um unico valor final.
  highp float nl=dot(n,l);
  if(nl<=0.0) return vec3(0.0);
  highp float nv=max(dot(n,v),1e-4);mediump vec3 h=normalize(v+l);
  highp float nh=max(dot(n,h),0);mediump float lh=max(dot(l,h),0);
  mediump vec3 f=fresnel(f0,lh);highp float alpha=rough*rough;
  mediump float fd90=.5+2*rough*lh*lh;
  mediump float fd=(1+(fd90-1)*pow5(1-float(nl)))*(1+(fd90-1)*pow5(1-float(nv)))/PI;
  mediump float specular=float(distribution(nh,alpha)*visibility(nv,nl,alpha));
  return ((1-f)*(1-metal)*base*fd+specular*f)*radiance*float(nl);
}
void main() {
  if(aetherLodDitherDiscard(gl_FragCoord.xy,vDither)) discard;
  uint flags=frame.materialFlags.x;
  uint isolation=GPU_COST_ISOLATION;
  mediump vec4 baseSample=texture(BASE_MAP,selectedUv(0));
  mediump vec4 base=baseSample*frame.baseColorFactor*vColor;
  // O depth-only de coverage já descartou estes texels, mas discard no shader
  // impede early-Z em parte dos drivers tile-based. Reaplicar o mesmo cutoff
  // imediatamente após o único sample obrigatório evita executar normal, MR,
  // sombra e PBR em folhas que jamais podem produzir um pixel. O branch é
  // uniforme por material batch e preserva exatamente a cobertura do prepass.
  if((flags&16u)!=0u) {
    mediump float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
    if(base.a<alphaCutoff) discard;
  }
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
  highp vec3 eye=frame.cameraPositionNear.xyz;
  highp float cameraDistance=length(eye-vPosition);
  mediump float mrDetailWeight=materialDetailWeight(
      cameraDistance,environment.materialDistanceParameters.x);
  mediump vec3 mr=vec3(1);
  if(hasMaterialFeature(flags,4u) && mrDetailWeight>0.0)
    mr=mix(vec3(1),texture(MR_MAP,selectedUv(2)).rgb,mrDetailWeight);
  mediump float rough=clamp(mr.g*frame.materialFactors.x,.07,1);
  mediump float metal=clamp(mr.b*frame.materialFactors.y,0,1);
  mediump vec3 n=normalize(vNormal);
  // A single-view foliage impostor has no per-pixel normal field: using the
  // camera-facing quad normal directly makes the entire distant crown brighten
  // and darken as the camera yaws. Treat it as a coarse canopy volume instead.
  // The small view-facing contribution retains shape while world-up dominates,
  // producing stable distant lighting without an extra texture fetch.
  if((flags&MATERIAL_IMPOSTOR)!=0u)
    n=normalize(mix(vec3(0.0,1.0,0.0),n,0.18));
  mediump float normalDetailWeight=materialDetailWeight(cameraDistance,environment.quality.x);
  if(hasMaterialFeature(flags,2u) && isolation!=1u && normalDetailWeight>0.0) {
    mediump vec3 t=normalize(vTangent.xyz-n*dot(n,vTangent.xyz));
    mediump vec3 b=cross(n,t)*vTangent.w;
    mediump vec3 detail=texture(NORMAL_MAP,selectedUv(1)).xyz*2-1;
    // Normalizing before and after an orthonormal TBN is redundant. Keep the
    // final normalization (same direction, one fewer reciprocal sqrt).
    detail=vec3(detail.xy*frame.materialFactors.z*normalDetailWeight,
                mix(1.0,detail.z,normalDetailWeight));
    n=normalize(mat3(t,b,n)*detail);
  }
  // A subtração fica em highp: `eye` e `vPosition` são coordenadas de mundo e o
  // mapa se estende por centenas de unidades, onde fp16 já perde resolução. Só
  // o resultado normalizado, que vive em [-1,1], desce para mediump.
  mediump vec3 v=normalize(eye-vPosition);
  mediump vec3 f0=mix(vec3(.04*frame.materialFactors.w),base.rgb,metal);
  mediump vec3 sunRadiance=environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  highp float viewDepth=max(dot(environment.worldToViewRow2.xyz,vPosition-eye),0.0);
  // A luz direta é exatamente zero no hemisfério oposto. Consultar 1/9/25
  // texels de sombra nesse caso só gasta banda; o branch remove trabalho sem
  // mudar um único valor final.
  mediump float sunVisibility=dot(n,environment.sunDirectionIntensity.xyz)>0.0?
      directionalShadow(vPosition,n,viewDepth):1.0;
  mediump vec3 color=directLight(n,v,environment.sunDirectionIntensity.xyz,sunRadiance,
                         base.rgb,f0,metal,rough)*sunVisibility;
  mediump float nv=max(dot(n,v),0.0);
  // A single global hemispherical irradiance keeps upward-facing foliage tied
  // to the sky while giving downward/vertical surfaces a neutral ground bounce.
  // This is constant-time ALU: no extra pass, draw call or texture fetch.
  mediump float skyWeight=environment.quality.z>0.5?clamp(n.y*.5+.5,0.0,1.0):1.0;
  mediump vec3 ambientIrradiance=mix(environment.groundColorSaturation.rgb,
                              environment.ambientColorStrength.rgb,skyWeight);
  // AEEN has carried this authored control since v3. Applying it to irradiance
  // (rather than grading the final image) removes colored shadow casts while
  // preserving direct-sun and material color. Values above one deliberately
  // allow stylized environments.
  mediump float ambientLuminance=dot(ambientIrradiance,vec3(.2126,.7152,.0722));
  ambientIrradiance=mix(vec3(ambientLuminance),ambientIrradiance,
                        clamp(environment.groundColorSaturation.w,0.0,2.0));
  color+=(1-metal)*base.rgb*ambientIrradiance*environment.ambientColorStrength.w;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  mediump float specularDetailWeight=environment.quality.w>0.5?
      materialDetailWeight(cameraDistance,environment.quality.y):0.0;
  if(isolation!=2u && specularDetailWeight>0.0 && (metal>.01 || rough<.65)) {
    mediump vec3 reflection=reflect(-v,n);
    mediump vec3 specularEnvironment=environmentRadiance(reflection,rough*environment.parameters.z);
    mediump vec3 environmentFresnel=fresnel(f0,nv);
    mediump vec2 integratedBrdf=environmentBrdf(nv,rough);
    color+=specularEnvironment*(environmentFresnel*integratedBrdf.x+integratedBrdf.y)*
        frame.materialFactors.w*specularDetailWeight;
  }
  mediump float emissiveDetailWeight=materialDetailWeight(
      cameraDistance,environment.materialDistanceParameters.y);
  if(hasMaterialFeature(flags,8u) && emissiveDetailWeight>0.0)
    color+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*
        frame.emissiveFactorStrength.a*emissiveDetailWeight;
  color=toneMapEnvironment(color);
  if((frame.materialFlags.z&1u)!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                             greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,base.a);
}
