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
// Ver dirt_road_vertex.glsl: luzes aceitas que alcancam este desenho e as
// flags autorais (Receive Shadows, sol pelas mascaras).
layout(location=11) flat in highp uint vLightMask;
const uint DRAW_NO_SHADOWS=131072u; // renderer::DrawLightingNoShadows
const uint DRAW_NO_SUN=262144u;     // renderer::DrawLightingNoSun
layout(location=0) out vec4 outColor;
#include "temporal_masks.glsl"
layout(constant_id=0) const uint GPU_COST_ISOLATION=0u;
layout(constant_id=1) const uint MATERIAL_FEATURE_MASK=0xffffffffu;
// Projecao da sonda de ambiente resolvida em tempo de compilacao de pipeline.
// O ramo legado equirect custa atan/acos/fract e, sobretudo, registradores em
// TODO fragmento enquanto for um uniform -- o driver nao pode provar que ele e
// morto. Como spec constant ele desaparece do binario do perfil que usa AEEN v3.
// Zero e "decidir em runtime", preservando o comportamento anterior.
layout(constant_id=2) const uint ENVIRONMENT_PROJECTION=0u;
// Only solid, non-transitioning draws may omit the clipping contract.
layout(constant_id=3) const uint OPAQUE_NO_CLIP=0u;
layout(constant_id=4) const uint FULL_DETAIL_SAMPLING=0u;
// Only selected when every uploaded punctual light is a point without an
// allocated local shadow. Directional lighting and its cascades stay intact.
layout(constant_id=5) const uint UNSHADOWED_POINT_LIGHTING=0u;
const float PI=3.141592653589793;
const uint MATERIAL_IMPOSTOR=256u; // renderer::MapMaterialImpostor
const uint MATERIAL_WATER=512u; // renderer::MapMaterialWater
// Precisa casar com renderer::MaximumPunctualLights. Os dois lados leem a mesma
// memoria; divergir aqui leria alem do array ou apagaria luzes que couberam.
const int PUNCTUAL_LIGHT_LIMIT=16;
const int PUNCTUAL_LIGHT_BANK=8; // renderer::PunctualLightBankSize
#include "environment_lighting.glsl"
#include "probe_lighting.glsl"
// LOD cross-fade (see renderer::selectLodLevel): vDither==0 for every draw
// outside an active transition, so this is a no-op discard everywhere LOD is
// disabled or a group has only one level. The two draws sharing a
// transitioning lodGroupId carry signed complementary masks: outgoing uses
// +factor and keeps threshold>=factor; incoming uses -factor and keeps
// threshold<factor. The prior `1-factor` encoding made one mask a subset of
// the other, causing double shading/overdraw instead of a cross-fade.
#include "lod_dither.glsl"
#include "impostor_view.glsl"
#include "material_uv_transform.glsl"
#include "world_uv.glsl"
const uint MATERIAL_SHADING_EXTENSION=1048576u; // renderer::MapMaterialShadingExtension
const float AETHER_MEDIUMP_MAX=60000.0;          // abaixo do maior finito do fp16 (65504)
// PK-9: estado do Material por desenho que a BRDF consulta (uniforme no
// desenho). Fora da extensão valem os padrões do Lit e nada muda.
uint aetherShadingModel=0u;                // 0 GGX (Lit), 1 Blinn-Phong (Simple Lit)
mediump float aetherHighlightWeight=1.0;    // Specular Highlights
highp float aetherBlinnExponent=2.0;
mediump vec3 aetherSpecularSum=vec3(0.0);   // só o especular, para Preserve Specular
highp vec2 aetherParallaxOffset=vec2(0.0);  // Height Map, somado ao UV de cada mapa
// Multiply: a cena é HDR antes da exposição (o sol físico passa de 100), então
// o fator não pode ser a cor iluminada; é a tinta da superfície (cor base ×
// textura), um vidro colorido que escurece o que está atrás.
mediump vec3 aetherMultiplyTint=vec3(1.0);
highp vec2 selectedUv(uint slot) {
  uint set=(frame.materialFlags.y>>(slot*2))&3u;
  return set==2u?aetherTransformUv(slot,aetherWorldUv(vPosition,vNormal)):
                 aetherTransformUv(slot,set==1u?vUv1:vUv0)+aetherParallaxOffset;
}
// Color Mode dos shaders de partícula da URP (ParticlesInput.hlsl): como a cor
// do vértice entra na cor base. 0 multiplicar, 1 somar, 2 subtrair, 3 overlay,
// 4 cor (matiz e saturação do vértice, valor da textura), 5 diferença.
mediump vec3 aetherRgbToHsv(mediump vec3 c) {
  mediump vec4 k=vec4(0.0,-1.0/3.0,2.0/3.0,-1.0);
  mediump vec4 p=mix(vec4(c.bg,k.wz),vec4(c.gb,k.xy),step(c.b,c.g));
  mediump vec4 q=mix(vec4(p.xyw,c.r),vec4(c.r,p.yzx),step(p.x,c.r));
  mediump float d=q.x-min(q.w,q.y);
  return vec3(abs(q.z+(q.w-q.y)/(6.0*d+1.0e-4)),d/(q.x+1.0e-4),q.x);
}
mediump vec3 aetherHsvToRgb(mediump vec3 c) {
  mediump vec3 p=abs(fract(c.xxx+vec3(1.0,2.0/3.0,1.0/3.0))*6.0-3.0);
  return c.z*mix(vec3(1.0),clamp(p-1.0,0.0,1.0),c.y);
}
mediump vec4 aetherParticleColor(mediump vec4 albedo,mediump vec4 color,uint mode) {
  if(mode==1u) albedo.rgb+=color.rgb;
  else if(mode==2u) albedo.rgb-=color.rgb;
  else if(mode==3u) albedo.rgb=mix(1.0-2.0*(1.0-albedo.rgb)*(1.0-color.rgb),2.0*albedo.rgb*color.rgb,step(albedo.rgb,vec3(0.5)));
  else if(mode==4u) {mediump vec3 a=aetherRgbToHsv(albedo.rgb),b=aetherRgbToHsv(color.rgb);albedo.rgb=aetherHsvToRgb(vec3(b.x,b.y,a.z));}
  else if(mode==5u) albedo.rgb=abs(albedo.rgb-color.rgb);
  else albedo.rgb*=color.rgb;
  albedo.a*=color.a;
  albedo.rgb=max(albedo.rgb,vec3(0.0));
  return albedo;
}
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
// KHR_materials_specular weights the entire dielectric lobe, including F90.
// Weighting only F0 resurrects white grazing reflections on specular=0 assets.
mediump vec3 fresnel(mediump vec3 f0,mediump float f90,mediump float vh) {
  return f0+(vec3(f90)-f0)*pow5(1-clamp(vh,0.0,1.0));
}
#include "microfacet_brdf.glsl"
highp float distribution(highp float nh,highp float alpha) {
  return distributionGGX(nh,alpha); }
highp float visibility(highp float nv,highp float nl,highp float alpha) {
  return visibilitySmithGGX(nv,nl,alpha); }
mediump vec3 directLight(mediump vec3 n,mediump vec3 v,mediump vec3 l,mediump vec3 radiance,
                         mediump vec3 base,mediump vec3 f0,mediump float f90,mediump float metal,
                         mediump float rough) {
  // O hemisferio oposto contribui exatamente zero: o retorno inteiro e
  // multiplicado por nl. Sair aqui poupa normalize(v+l), GGX, visibilidade e
  // Fresnel numa fracao grande dos fragmentos de uma floresta, onde metade das
  // folhas esta de costas para o sol -- e nao muda um unico valor final.
  highp float nl=dot(n,l);
  if(nl<=0.0) return vec3(0.0);
  if(aetherShadingModel==1u) {
    // PK-9 Simple Lit: Lambert e Blinn-Phong normalizado ((e+8)/8π), para que
    // o mesmo sol ilumine com a mesma energia do Lit; f0 é a cor especular.
    highp vec3 hb=normalize(v+l);
    highp float nhb=max(dot(n,hb),0.0);
    highp float lobe=pow(nhb,aetherBlinnExponent)*(aetherBlinnExponent+8.0)/(8.0*PI)*aetherHighlightWeight;
    highp vec3 incoming=vec3(radiance)*nl;
    mediump vec3 specularTerm=vec3(min(vec3(f0)*lobe*incoming,vec3(AETHER_MEDIUMP_MAX)));
    aetherSpecularSum=min(aetherSpecularSum+specularTerm,vec3(AETHER_MEDIUMP_MAX));
    return min(vec3(base/PI)*incoming+vec3(specularTerm),vec3(AETHER_MEDIUMP_MAX));
  }
  highp float nv=max(dot(n,v),1e-4);mediump vec3 h=normalize(v+l);
  highp float nh=max(dot(n,h),0);mediump float lh=max(dot(l,h),0);
  mediump vec3 f=fresnel(f0,f90,lh);highp float alpha=rough*rough;
  mediump float fd90=.5+2*rough*lh*lh;
  mediump float fd=(1+(fd90-1)*pow5(1-float(nl)))*(1+(fd90-1)*pow5(1-float(nv)))/PI;
  // O pico do GGX com rugosidade mínima passa de 10^4 e, vezes a radiância do
  // sol, estoura o fp16 (65504): o brilho virava inf/NaN, um ponto preto. O
  // lóbulo fica em highp e só desce para mediump já limitado.
  highp float lobe=f90>0.0?distribution(nh,alpha)*visibility(nv,nl,alpha):0.0;
  // PK-9: Specular Highlights desligado tira só o lóbulo direto.
  mediump vec3 specularTerm=vec3(min(lobe*vec3(f)*aetherHighlightWeight*vec3(radiance)*nl,vec3(AETHER_MEDIUMP_MAX)));
  aetherSpecularSum=min(aetherSpecularSum+specularTerm,vec3(AETHER_MEDIUMP_MAX));
  return min((1-f)*(1-metal)*base*fd*radiance*float(nl)+specularTerm,vec3(AETHER_MEDIUMP_MAX));
}
// Luzes pontuais e spot da cena, com a mesma BRDF do sol -- nao ha um segundo
// modelo de iluminacao escondido aqui. O laco tem limite constante para o
// compilador desenrolar; `count` so decide onde parar.
//
// Sombra local (G6-B): o tile vem do atlas em quadtree (renderer/shadow_atlas.h).
// O pontual escolhe a face pelo eixo dominante da direcao luz->fragmento, na
// mesma ordem das faces do C++ (+X,-X,+Y,-Y,+Z,-Z). Fora do mapa conta como
// iluminado: e a borda do cone ou o fim do alcance, onde a luz ja e zero.
mediump float localShadowVisibility(highp vec4 shadow,highp vec3 position,mediump vec3 n,
                                    highp vec3 fromLight) {
  int first=int(shadow.x+0.5);
  if(shadow.x<-0.5||first>=int(environment.localShadowParameters.x+0.5)) return 1.0;
  int packed=int(shadow.y+0.5);
  bool soft=packed>=16;
  int count=soft?packed-16:packed;
  int tile=first;
  if(count==6) {
    highp vec3 a=abs(fromLight);
    if(a.x>=a.y&&a.x>=a.z) tile+=fromLight.x>=0.0?0:1;
    else if(a.y>=a.z) tile+=fromLight.y>=0.0?2:3;
    else tile+=fromLight.z>=0.0?4:5;
  }
  highp vec4 rect=environment.localShadowRect[tile];
  // Desvio ao longo da normal medido em texels do mapa, como o Normal Bias da
  // Unity: o mesmo numero serve para lampada de 2 m e holofote de 40 m.
  highp vec3 biased=position+n*(rect.w*shadow.w);
  highp vec4 clip=environment.localShadowViewProjection[tile]*vec4(biased,1.0);
  if(clip.w<=1e-5) return 1.0;
  highp vec3 ndc=clip.xyz/clip.w;
  if(abs(ndc.x)>1.0||abs(ndc.y)>1.0||ndc.z<0.0||ndc.z>1.0) return 1.0;
  highp vec2 uv=rect.xy+(ndc.xy*0.5+0.5)*rect.z;
  mediump float lit;
  if(soft) {
    // Suave: quatro buscas com compare de hardware em volta do texel, cada uma
    // ja filtrada 2x2 — 16 amostras efetivas sem sair do tile.
    highp vec2 texel=vec2(environment.localShadowParameters.y*0.75);
    highp vec2 low=rect.xy+texel,high=rect.xy+rect.zz-texel;
    lit=0.25*(texture(localShadowAtlas,vec3(clamp(uv+vec2(-texel.x,-texel.y),low,high),ndc.z))+
              texture(localShadowAtlas,vec3(clamp(uv+vec2( texel.x,-texel.y),low,high),ndc.z))+
              texture(localShadowAtlas,vec3(clamp(uv+vec2(-texel.x, texel.y),low,high),ndc.z))+
              texture(localShadowAtlas,vec3(clamp(uv+vec2( texel.x, texel.y),low,high),ndc.z)));
  } else {
    lit=texture(localShadowAtlas,vec3(uv,ndc.z));
  }
  return mix(1.0,lit,clamp(shadow.z,0.0,1.0));
}

// Uma linha do registro da vaga `slot`, nos dois bancos do bloco.
highp vec4 punctualRecord(int slot,int row) {
  return slot<PUNCTUAL_LIGHT_BANK?environment.punctualLights[slot*4+row]:
      environment.punctualLightsExtra[(slot-PUNCTUAL_LIGHT_BANK)*4+row];
}
mediump vec3 punctualLighting(highp vec3 position,mediump vec3 n,mediump vec3 v,
                              mediump vec3 base,mediump vec3 f0,mediump float f90,
                              mediump float metal,mediump float rough) {
  int count=int(environment.punctualLightParameters.x+0.5);
  mediump vec3 sum=vec3(0.0);
  for(int i=0;i<PUNCTUAL_LIGHT_LIMIT;++i) {
    if(i>=count) break;
    // A CPU ja provou que a esfera desta luz nao toca o desenho (ou que as
    // mascaras da luz recusam o objeto): nem a distancia precisa ser
    // calculada. Uniforme por desenho, sem divergencia.
    if(((vLightMask>>uint(i))&1u)==0u) continue;
    highp vec4 positionRange=punctualRecord(i,0);
    mediump vec4 colorIntensity=punctualRecord(i,1);
    mediump vec4 directionOffset=punctualRecord(i,2);
    highp vec4 shadow=punctualRecord(i,3);
    // Direcional adicional (lua, preenchimento): alcance negativo. Sem janela,
    // cone nem sombra -- a mesma BRDF do sol, com a direcao de emissao.
    if(UNSHADOWED_POINT_LIGHTING==0u && positionRange.w<0.0) {
      sum+=directLight(n,v,-directionOffset.xyz,colorIntensity.rgb,base,f0,f90,metal,rough);
      continue;
    }
    highp vec3 toLight=positionRange.xyz-position;
    highp float distanceSquared=dot(toLight,toLight);
    // Uma luz exatamente sobre a superficie dividiria por zero e pintaria o
    // fragmento de infinito. O piso e um centimetro quadrado.
    highp float safeSquared=max(distanceSquared,1e-4);
    mediump vec3 l=toLight*inversesqrt(safeSquared);
    // Janela suave de alcance: a luz chega a zero exatamente em `range`, sem o
    // degrau que um corte duro deixaria na borda da esfera de influencia.
    mediump float ratio=distanceSquared/max(positionRange.w*positionRange.w,1e-4);
    mediump float window=clamp(1.0-ratio*ratio,0.0,1.0);
    window*=window;
    if(window<=0.0) continue;
    // Cone do spot pre-calculado na CPU: escala e deslocamento em vez de dois
    // cossenos por fragmento. Pontual usa escala 0 e deslocamento 1, o que da
    // exatamente 1 em qualquer direcao -- mesma conta, sem desvio por tipo.
    mediump float cone=UNSHADOWED_POINT_LIGHTING!=0u?1.0:
        clamp(dot(directionOffset.xyz,-l)*colorIntensity.w+directionOffset.w,0.0,1.0);
    cone*=cone;
    // A sombra so e buscada onde a luz ainda chega: fora do cone ou do
    // alcance, o atlas nao tem o que dizer e a busca seria desperdicio.
    if(cone<=0.0) continue;
    mediump float visible=UNSHADOWED_POINT_LIGHTING!=0u||(vLightMask&DRAW_NO_SHADOWS)!=0u?1.0:
        localShadowVisibility(shadow,position,n,-toLight);
    mediump vec3 radiance=colorIntensity.rgb*(window*cone*visible/safeSquared);
    sum+=directLight(n,v,l,radiance,base,f0,f90,metal,rough);
  }
  return sum;
}
mediump vec3 shadeWater(highp vec3 position,mediump vec3 n) {
  highp vec3 eye=frame.cameraPositionNear.xyz;
  mediump vec3 v=environment.worldToViewRow0.w>0.0?-environment.worldToViewRow2.xyz:normalize(eye-position);
  mediump float nv=max(dot(n,v),0.001);
  mediump float ior=clamp(environment.waterOptics.x,1.0,2.0);
  mediump float f0=(ior-1.0)/(ior+1.0); f0*=f0;
  mediump float fresnelWeight=f0+(1.0-f0)*pow5(1.0-nv);
  mediump float rough=clamp(environment.waterOptics.y,0.025,1.0);
  mediump vec3 reflected=environmentRadiance(reflect(-v,n),
      rough*environment.parameters.z);

  // Beer-Lambert attenuation is evaluated from a bounded optical path. Until
  // scene depth is available this is an explicit deep-water approximation,
  // not a fake screen grab. Turbidity controls in-scattering independently.
  mediump float opticalPath=mix(0.45,4.0,1.0-nv);
  mediump vec3 transmission=exp(-environment.waterAbsorption.rgb*opticalPath);
  mediump vec3 body=mix(environment.waterDeepColorFoam.rgb,
                        environment.waterShallowColorDistance.rgb,transmission);
  body=mix(body,environment.waterShallowColorDistance.rgb,
           clamp(environment.waterOptics.z,0.0,1.0)*0.35);
  mediump vec3 color=mix(body,reflected,fresnelWeight);

  mediump vec3 l=environment.sunDirectionIntensity.xyz;
  mediump float nl=max(dot(n,l),0.0);
  mediump vec3 h=normalize(v+l);
  mediump float sunPower=mix(384.0,18.0,rough);
  mediump float sunSpec=pow(max(dot(n,h),0.0),sunPower)*nl;
  mediump vec3 sunRadiance=environment.sunColorAngularRadius.rgb*
                           environment.sunDirectionIntensity.w;
  highp float viewDepth=max(dot(environment.worldToViewRow2.xyz,position-eye),0.0);
  mediump float visibility=nl>0.0?directionalShadow(position,n,viewDepth):1.0;
  color+=sunRadiance*sunSpec*visibility;

  mediump float slope=length(n.xz)/max(n.y,0.05);
  mediump float foam=smoothstep(environment.waterOptics.w,
      min(environment.waterOptics.w+0.22,1.0),clamp(slope,0.0,1.0));
  foam*=clamp(environment.waterDeepColorFoam.w,0.0,1.0);
  return mix(color,vec3(0.92,0.97,1.0),foam);
}
// Saída da superfície. No transparente, PK-9: premultiplicado (difuso·a +
// especular), aditivo (cor·a, ou o especular inteiro com Preserve Specular) e
// multiplicar (mix(1, tinta, a): a tinta da superfície, sem luz, sobre o que está atrás).
void aetherWriteSurface(mediump vec3 color,mediump vec3 specular,mediump float alpha,uint flags,uint blendOutput,uint shadingBits) {
  bool transparent=(flags&1u)!=0u;
  specular=min(specular,color);
  if(transparent && blendOutput==1u) color=(color-specular)*alpha+specular;
  else if(transparent && blendOutput==2u) color=(shadingBits&4u)!=0u?(color-specular)*alpha+specular:color*alpha;
  if(transparent && blendOutput==3u) color=mix(vec3(1.0),aetherMultiplyTint,alpha);
  else color=toneMapEnvironment(color);
  if((frame.materialFlags.z&1u)!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                             greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,transparent?alpha:1.0);
  if(transparent) aetherBlendedTemporalMasks(alpha);
}
void main() {
  if(OPAQUE_NO_CLIP==0u && aetherLodDitherDiscard(gl_FragCoord.xy,vDither)) discard;
  uint flags=frame.materialFlags.x;
  uint isolation=GPU_COST_ISOLATION;
  aetherWriteTemporalMasks(0.0,0.0);
  // The specialized solid family has already validated water/impostor/LOD
  // eligibility against the effective material and uploaded instance. Fold
  // their shader programs too; leaving uniform-false paths here still costs
  // registers and instruction scheduling on the mobile compiler.
  bool generalSurface=OPAQUE_NO_CLIP==0u || UNSHADOWED_POINT_LIGHTING==0u;
  if(generalSurface && (flags&MATERIAL_WATER)!=0u) {
    aetherWaterTemporalMasks(1.0,0.0);
    mediump vec3 color=toneMapEnvironment(shadeWater(vPosition,normalize(vNormal)));
    if((frame.materialFlags.z&1u)!=0u)
      color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                greaterThan(color,vec3(.0031308)));
    outColor=vec4(color,1.0);
    return;
  }
  // Compute derivatives before any lane can discard at an alpha edge. Water
  // returned above without touching material textures or their derivatives.
  highp vec2 impostorDx=dFdx(vUv0),impostorDy=dFdy(vUv0);
  bool impostor=generalSurface && (flags&MATERIAL_IMPOSTOR)!=0u && (frame.materialFlags.y>>16u)!=0u;
  highp vec2 impostorUv=vUv0;
  if(impostor) {
    impostorUv=aetherImpostorViewUv(flags,vUv0,vUv1,vColor.a,gl_FragCoord.xy);
    impostorUv=aetherImpostorSafeUv(impostorUv,impostorDx,impostorDy,
        vec2(textureSize(BASE_MAP,0)),frame.materialFlags.y>>16u,
        float(textureQueryLevels(BASE_MAP)-1));
  }
  // PK-9: shader e opções do Material, só nos desenhos com a flag. As variantes
  // especializadas por textura não têm a flag na máscara e eliminam tudo isto.
  bool shadingExtension=generalSurface && !impostor && hasMaterialFeature(flags,MATERIAL_SHADING_EXTENSION) &&
                        aetherHasMaterialExtension();
  uint shaderModel=0u,workflow=0u,blendOutput=0u,shadingBits=3u;
  if(shadingExtension) {
    highp vec4 shaderRow=aetherMaterialRow(13u);
    shaderModel=uint(shaderRow.x+0.5);workflow=uint(shaderRow.y+0.5);
    blendOutput=uint(shaderRow.z+0.5);shadingBits=uint(shaderRow.w+0.5);
#ifdef OCCLUSION_MAP
    // Height Map (URP ParallaxOffset1Step): um passo de paralaxe no espaço
    // tangente, altura no canal G, centrada em meia amplitude. O UV da cor
    // base mede a altura; o deslocamento vale para todos os mapas.
    highp vec4 heightRow=aetherMaterialRow(15u);
    if(heightRow.x>=0.0) {
      mediump vec3 geometricNormal=normalize(vNormal);
      mediump vec3 tangentAxisH=vTangent.xyz-geometricNormal*dot(geometricNormal,vTangent.xyz);
      if(dot(tangentAxisH,tangentAxisH)>1.0e-6) {
        mediump vec3 th=normalize(tangentAxisH);
        mediump vec3 bh=cross(geometricNormal,th)*vTangent.w;
        highp vec3 toEye=normalize(frame.cameraPositionNear.xyz-vPosition);
        // O eixo b do normal map aponta para o "alto" da imagem, que é -v.
        highp vec3 viewTs=vec3(dot(toEye,th),-dot(toEye,bh),dot(toEye,geometricNormal));
        highp float amplitude=heightRow.y;
        highp float height=texture(OCCLUSION_MAP(uint(heightRow.x+0.5)),selectedUv(0)).g*amplitude-amplitude*0.5;
        viewTs.z+=0.42;
        aetherParallaxOffset=height*(viewTs.xy/viewTs.z);
      }
    }
#endif
  }
  mediump vec4 baseSample=impostor?
      textureGrad(BASE_MAP,impostorUv,impostorDx,impostorDy):texture(BASE_MAP,selectedUv(0));
  // R4: origem do alfa do material, antes do corte (igual à cobertura e à sombra).
  // G3a: o alfa cru da textura de cor é a suavidade quando o material diz
  // (Standard da Unity, "Smoothness Source: Albedo Alpha"); a origem do alfa
  // pode tê-lo trocado por 1 logo abaixo.
  mediump float baseTexelAlpha=baseSample.a;
  if(!impostor) baseSample.a=aetherAlphaFromSource(baseSample);
  mediump vec4 vertexColor=impostor?vec4(vColor.rgb,1.0):vColor;
  mediump vec4 base=baseSample*frame.baseColorFactor*vertexColor;
  if(shadingExtension) {
    // Particles Lit/Unlit: Color Mode da cor do vértice.
    if(shaderModel>=4u) base=aetherParticleColor(baseSample*frame.baseColorFactor,vertexColor,uint(aetherMaterialRow(20u).x+0.5));
    // Camera Fading: some perto da câmera, entre "de" e "até".
    if((shadingBits&16u)!=0u) {
      highp vec4 fadeRow=aetherMaterialRow(20u);
      base.a*=clamp((length(frame.cameraPositionNear.xyz-vPosition)-fadeRow.y)/max(fadeRow.z-fadeRow.y,1.0e-4),0.0,1.0);
    }
    // Alpha Clipping no Transparent.
    if((shadingBits&8u)!=0u && base.a<aetherMaterialRow(14u).w) discard;
    aetherMultiplyTint=clamp(base.rgb,0.0,1.0);
  }
  // O depth-only de coverage já descartou estes texels, mas discard no shader
  // impede early-Z em parte dos drivers tile-based. Reaplicar o mesmo cutoff
  // imediatamente após o único sample obrigatório evita executar normal, MR,
  // sombra e PBR em folhas que jamais podem produzir um pixel. O branch é
  // uniforme por material batch e preserva exatamente a cobertura do prepass.
  if(OPAQUE_NO_CLIP==0u && (flags&16u)!=0u) {
    mediump float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
    if(base.a<alphaCutoff) discard;
  }
  // Fully transparent atlas texels cannot affect the framebuffer. Rejecting
  // them before normal/PBR work preserves the exact 8-bit alpha result and is
  // particularly important for dense forest cards.
  if(OPAQUE_NO_CLIP==0u && (flags&1u)!=0u && base.a<(1.0/255.0)) discard;
  // BaseColorOnly keeps the exact alpha/depth coverage and geometry while
  // avoiding material/lighting fetches. The branch is uniform for the full run.
  if(isolation==3u) {
    vec3 color=base.rgb;
    if((frame.materialFlags.z&1u)!=0u)
      color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                greaterThan(color,vec3(.0031308)));
    outColor=vec4(color,(flags&1u)!=0u?base.a:1.0);
    if((flags&1u)!=0u) aetherBlendedTemporalMasks(base.a);
    return;
  }
#ifdef OCCLUSION_MAP
  // PK-9 Detail Inputs (URP Lit): máscara no canal A, com o UV da cor base;
  // Base Map de detalhe ×2 no espaço da Unity (cinza sRGB 0,5 é neutro: o
  // linear vezes unity_ColorSpaceDouble 4,5948), visibilidade como escala.
  mediump float detailMask=0.0;
  highp vec2 detailUv=vec2(0.0);
  if(shadingExtension) {
    highp vec4 detailRow=aetherMaterialRow(15u),detailNormalRow=aetherMaterialRow(16u);
    if(detailRow.w>=0.0 || detailNormalRow.x>=0.0) {
      detailMask=detailRow.z>=0.0?texture(OCCLUSION_MAP(uint(detailRow.z+0.5)),selectedUv(0)).a:1.0;
      highp vec3 point=vec3(detailNormalRow.w>0.5?vUv1:vUv0,1.0);
      detailUv=vec2(dot(aetherMaterialRow(17u).xyz,point),dot(aetherMaterialRow(18u).xyz,point))+aetherParallaxOffset;
      if(detailRow.w>=0.0) {
        mediump vec3 detailAlbedo=texture(OCCLUSION_MAP(uint(detailRow.w+0.5)),detailUv).rgb*4.59479380;
        detailAlbedo=max(mix(vec3(1.0),detailAlbedo,detailNormalRow.y),vec3(0.0));
        base.rgb*=mix(vec3(1.0),detailAlbedo,detailMask);
      }
    }
  }
#endif
  // PK-9 Unlit e Particles Unlit: cor base sem luz (a partícula soma emissão).
  if(shadingExtension && (shaderModel==3u || shaderModel==5u)) {
    mediump vec3 unlit=base.rgb;
    if(shaderModel==5u && hasMaterialFeature(flags,8u))
      unlit+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*frame.emissiveFactorStrength.a;
    aetherWriteSurface(unlit,vec3(0.0),base.a,flags,blendOutput,shadingBits);
    return;
  }
  highp vec3 eye=frame.cameraPositionNear.xyz;
  highp float cameraDistance=length(eye-vPosition);
  mediump float mrDetailWeight=materialDetailWeight(
      cameraDistance,environment.materialDistanceParameters.x);
  mediump vec4 mr=vec4(1);
  // Distance may reduce spatial detail, never change the BRDF into the scalar
  // defaults. White ORM turned textured conductors into rough bare metal and
  // erased occlusion as the camera crossed the quality band. Preserve the
  // filtered material statistics using the texture's mip chain instead.
  if(hasMaterialFeature(flags,4u)) {
    highp vec2 uv=selectedUv(2);
    // Evaluate implicit derivatives before the new per-pixel branch. A full
    // detail sample has weight one; its coarsest mip contributes exactly zero.
    mediump vec4 fine=mrDetailWeight>0.0?texture(MR_MAP,uv):vec4(1.0);
    if(FULL_DETAIL_SAMPLING!=0u && mrDetailWeight>=1.0) mr=fine;
    else {
      mediump vec4 coarse=textureLod(MR_MAP,uv,float(textureQueryLevels(MR_MAP)-1));
      mr=mrDetailWeight>0.0?mix(coarse,fine,mrDetailWeight):coarse;
    }
  }
  bool materialExtension=aetherHasMaterialExtension();
  // R4: canais do mapa escolhidos no material; sem extensão, o do glTF (G e B).
  uint roughnessChannel=materialExtension?uint(aetherMaterialRow(9u).x+0.5):1u;
  uint metallicChannel=materialExtension?uint(aetherMaterialRow(9u).y+0.5):2u;
  mediump float rough=mr[roughnessChannel]*frame.materialFactors.x;
  // G3a: suavidade (Standard da Unity) vira rugosidade = 1 - escala * canal, com
  // o fator guardado como 1 - escala. Sem textura o canal é 1 e sobra o fator.
  if(materialExtension) {
    uint roughnessSource=uint(aetherMaterialRow(10u).y+0.5);
    if(roughnessSource!=0u) {
      mediump float smoothness=roughnessSource==2u?(impostor?1.0:baseTexelAlpha):mr[roughnessChannel];
      rough=1.0-(1.0-frame.materialFactors.x)*smoothness;
    }
  }
  rough=clamp(rough,.07,1);
  mediump float metal=clamp(mr[metallicChannel]*frame.materialFactors.y,0,1);
  // R4: oclusão empacotada no canal R do mapa metal/rugosidade (ORM do glTF),
  // só quando o importador provou mesma textura e mesmo UV. Atenua a luz
  // ambiente e o reflexo do ambiente, nunca a luz direta. `mr` já volta a 1
  // longe da câmera, então a oclusão some junto com o detalhe do mapa.
  mediump float occlusion=((flags&16384u)!=0u && hasMaterialFeature(flags,4u))?mr.r:1.0;
  if(materialExtension) {
    // R4: origem, canal e força da oclusão escolhidos no material (T21).
    highp vec4 occlusionRow=aetherMaterialRow(8u);
    uint occlusionSource=uint(occlusionRow.y+0.5);
    uint occlusionChannel=uint(occlusionRow.z+0.5);
    mediump float sampledOcclusion=1.0;
    if(occlusionSource==1u && hasMaterialFeature(flags,4u)) sampledOcclusion=mr[occlusionChannel];
#ifdef OCCLUSION_MAP
    if(occlusionSource==2u && mrDetailWeight>0.0)
      sampledOcclusion=mix(1.0,texture(OCCLUSION_MAP(uint(occlusionRow.w+0.5)),selectedUv(2))[occlusionChannel],mrDetailWeight);
#endif
    occlusion=occlusionSource==0u?1.0:mix(1.0,sampledOcclusion,clamp(occlusionRow.x,0.0,1.0));
  }
  mediump vec3 n=normalize(vNormal);
  // For regular materials this is optional high-frequency detail. For an
  // impostor the atlas stores the geometry's replacement normal field, so it
  // remains at full weight at distance and prevents a whole crown from sharing
  // one upward-facing, over-bright normal.
  mediump float normalDetailWeight=impostor?1.0:
      materialDetailWeight(cameraDistance,environment.quality.x);
  // Tangente degenerada (zero) faria normalize() devolver NaN e a superfície
  // inteira ficar preta; sem base válida o detalhe do mapa normal é omitido.
  mediump vec3 tangentAxis=vTangent.xyz-n*dot(n,vTangent.xyz);
  bool worldNormal=((frame.materialFlags.y>>2u)&3u)==2u;
  if(hasMaterialFeature(flags,2u) && isolation!=1u && normalDetailWeight>0.0 &&
     (impostor || worldNormal || dot(tangentAxis,tangentAxis)>1.0e-6)) {
    mediump vec3 t=worldNormal?vec3(1,0,0):normalize(tangentAxis);
    mediump vec3 b=worldNormal?vec3(0,1,0):cross(n,t)*(impostor?1.0:vTangent.w);
    mediump vec3 detail=(impostor?
        textureGrad(NORMAL_MAP,impostorUv,impostorDx,impostorDy):
        texture(NORMAL_MAP,selectedUv(1))).xyz*2-1;
    // R4: convenção do mapa normal (T06): inverter Y para mapas no padrão DirectX.
    if(!impostor && materialExtension && aetherMaterialRow(9u).w>0.5) detail.y=-detail.y;
    // UV girada: o detalhe volta aos eixos da tangente da malha.
    if(!impostor) detail.xy=aetherUvTangentFrame(1u)*detail.xy;
    // Normalizing before and after an orthonormal TBN is redundant. Keep the
    // final normalization (same direction, one fewer reciprocal sqrt).
    detail=vec3(detail.xy*frame.materialFactors.z*normalDetailWeight,
                mix(1.0,detail.z,normalDetailWeight));
    n=impostor?normalize(detail):worldNormal?normalize(aetherWorldUvBasis(n)*detail):normalize(mat3(t,b,n)*detail);
  }
#ifdef OCCLUSION_MAP
  // PK-9 Detail Normal Map: reorientado sobre o normal já perturbado (a base
  // tangente acompanha n), na força do detalhe e pela máscara.
  if(shadingExtension && isolation!=1u && aetherMaterialRow(16u).x>=0.0 && detailMask>0.0 && dot(tangentAxis,tangentAxis)>1.0e-6) {
    highp vec4 detailNormalRow=aetherMaterialRow(16u);
    mediump vec3 detailTs=texture(OCCLUSION_MAP(uint(detailNormalRow.x+0.5)),detailUv).xyz*2.0-1.0;
    if(materialExtension && aetherMaterialRow(9u).w>0.5) detailTs.y=-detailTs.y;
    mediump vec3 td=vTangent.xyz-n*dot(n,vTangent.xyz);
    if(dot(td,td)>1.0e-6) {
      td=normalize(td);
      mediump vec3 bd=cross(n,td)*vTangent.w;
      mediump vec3 perturbed=normalize(td*detailTs.x*detailNormalRow.z+bd*detailTs.y*detailNormalRow.z+n*max(detailTs.z,1.0e-3));
      n=normalize(mix(n,perturbed,detailMask));
    }
  }
#endif
  // A subtração fica em highp: `eye` e `vPosition` são coordenadas de mundo e o
  // mapa se estende por centenas de unidades, onde fp16 já perde resolução. Só
  // o resultado normalizado, que vive em [-1,1], desce para mediump.
  mediump vec3 v=environment.worldToViewRow0.w>0.0?-environment.worldToViewRow2.xyz:normalize(eye-vPosition);
  mediump float dielectricSpecular=clamp(frame.materialFactors.w,0.0,1.0);
  mediump vec3 f0=mix(vec3(.04*dielectricSpecular),base.rgb,metal);
  mediump float f90=mix(dielectricSpecular,1.0,metal);
  if(shadingExtension) {
    aetherHighlightWeight=(shadingBits&1u)!=0u?1.0:0.0;
    // Workflow Specular (e Simple Lit): o RGB do binding 2 é o Specular Map,
    // vezes a cor especular; sem metálico. O Lit especular conserva energia
    // como a URP (difuso vezes 1 - refletividade); o Simple Lit não.
    if(workflow==1u || shaderModel==2u) {
      f0=aetherMaterialRow(14u).rgb*(hasMaterialFeature(flags,4u)?mr.rgb:vec3(1.0));
      f90=1.0;metal=0.0;
      if(shaderModel!=2u) base.rgb*=1.0-max(f0.r,max(f0.g,f0.b));
    }
    if(shaderModel==2u) {
      aetherShadingModel=1u;
      aetherBlinnExponent=exp2(10.0*(1.0-rough)+1.0);
    }
  }
  // Culling Mask/Rendering Layers do sol que recusam o objeto apagam a luz
  // direta dele aqui; Receive Shadows desligado pula a busca nas cascatas.
  mediump vec3 sunRadiance=(vLightMask&DRAW_NO_SUN)!=0u?vec3(0.0):
      environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  highp float viewDepth=max(dot(environment.worldToViewRow2.xyz,vPosition-eye),0.0);
  // A luz direta é exatamente zero no hemisfério oposto. Consultar 1/9/25
  // texels de sombra nesse caso só gasta banda; o branch remove trabalho sem
  // mudar um único valor final.
  mediump float sunVisibility=isolation!=5u && (vLightMask&(DRAW_NO_SHADOWS|DRAW_NO_SUN))==0u &&
      dot(n,environment.sunDirectionIntensity.xyz)>0.0?
      directionalShadow(vPosition,n,viewDepth):1.0;
  mediump vec3 color=directLight(n,v,environment.sunDirectionIntensity.xyz,sunRadiance,
                         base.rgb,f0,f90,metal,rough)*sunVisibility;
  aetherSpecularSum*=sunVisibility;
  if(isolation!=4u) color+=punctualLighting(vPosition,n,v,base.rgb,f0,f90,metal,rough);
  mediump float nv=max(dot(n,v),0.0);
  // PK-6: com Light Probes = Misturar sondas e um bake na cena, o ambiente
  // difuso vem do SH interpolado no tetraedro do objeto (E/π), no lugar do céu.
  // A mesma força do ambiente que escala o céu escala a sonda: uma sonda em
  // campo aberto devolve exatamente o ambiente do céu que ela substitui.
  mediump vec3 diffuseIrradiance=(astraLightProbesActive()?
      astraLightProbeIrradiance(n)*(environment.ambientColorStrength.w*0.31830988618):
      environmentAmbientDiffuse(n))*occlusion;
#ifdef OCCLUSION_MAP
  // External lightmaps contain linear indirect irradiance E, not reflected color.
  // Replace ambient diffuse to avoid counting indirect lighting twice; retain
  // dynamic direct lights and environment specular. E / pi is Lambertian diffuse.
  if(materialExtension && aetherMaterialRow(11u).x>0.5) {
    highp vec4 lm=aetherMaterialRow(11u),uv=aetherMaterialRow(12u);
    diffuseIrradiance=max(texture(OCCLUSION_MAP(uint(lm.y+0.5)),vUv1*uv.xy+uv.zw).rgb,vec3(0))*lm.z*0.31830988618;
  }
#endif
  color+=(1-metal)*base.rgb*diffuseIrradiance;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  mediump float specularDetailWeight=environment.quality.w>0.5?
      materialDetailWeight(cameraDistance,environment.quality.y):0.0;
  // PK-9: o Simple Lit não reflete o ambiente (URP); Environment Reflections
  // desligado troca a sonda pela cor do ambiente na direção do reflexo (o
  // _GlossyEnvironmentColor da URP).
  if(!(shadingExtension && shaderModel==2u) && isolation!=2u && f90>0.0 && environment.quality.w>0.5) {
    mediump vec3 reflection=reflect(-v,n);
    // Fade angular detail into the coarsest prefiltered probe, retaining
    // reflected energy. Roughness is continuous; it must not switch IBL off.
    mediump float probeLod=mix(environment.parameters.z,
        rough*environment.parameters.z,specularDetailWeight);
    // PK-6: as sondas de reflexo do desenho (box projection por pixel) e o céu
    // no que sobra dos pesos. A rugosidade efetiva segue o mesmo esmaecimento
    // de detalhe por distância que escolhe o mip do céu.
    // PK-9: Environment Reflections desligado troca as sondas pela cor do ambiente.
    mediump vec3 specularEnvironment=shadingExtension && (shadingBits&2u)==0u?
        environmentAmbientDiffuse(reflection):astraSpecularRadiance(vPosition,reflection,probeLod,
        mix(1.0,rough,specularDetailWeight));
    mediump vec2 integratedBrdf=environmentBrdf(nv,rough);
    // The split-sum LUT already integrates Schlick's angular term. Applying
    // Fresnel again double-counted it; scaling the result by the dielectric
    // weight a second time also incorrectly extinguished metallic reflections.
    mediump vec3 integratedSpecular=environment.parameters.w>1.5?
        f0*integratedBrdf.x+vec3(f90*integratedBrdf.y):
        fresnel(f0,f90,nv)*integratedBrdf.x;
    // G3a, oclusão especular: a sonda é o céu aberto, e sem estes termos todo
    // canto, beiral e interior reflete o céu -- o "tudo brilha". Dois fatores:
    // horizonte (Jimenez 2016): o normal map pode apontar o reflexo para dentro
    // da superfície geométrica, onde só há o próprio objeto;
    // AO -> especular (Lagarde 2014, "Moving Frostbite to PBR"): a oclusão do
    // material vale mais para reflexos rasantes e lisos do que multiplicada
    // direto, e chega a zero onde a AO é total.
    mediump float horizon=min(1.0+dot(reflection,normalize(vNormal)),1.0);
    mediump float specularOcclusion=horizon*horizon*
        clamp(pow(nv+occlusion,exp2(-16.0*rough-1.0))-1.0+occlusion,0.0,1.0);
    mediump vec3 environmentSpecular=specularEnvironment*integratedSpecular*specularOcclusion;
    color+=environmentSpecular;
    aetherSpecularSum+=environmentSpecular;
  }
  // PK-9 Clear Coat (Complex Lit): verniz dielétrico (F0 0,04) liso sobre a
  // camada de baixo, com o normal da geometria. A camada de baixo recebe o
  // que o verniz deixa passar; o verniz reflete o sol e o ambiente.
  if(shadingExtension && (shadingBits&32u)!=0u) {
    highp vec4 coatRow=aetherMaterialRow(19u);
    mediump float coatMask=clamp(coatRow.x,0.0,1.0),coatSmoothness=clamp(coatRow.y,0.0,1.0);
#ifdef OCCLUSION_MAP
    if(coatRow.z>=0.0) {
      mediump vec2 coatTexel=texture(OCCLUSION_MAP(uint(coatRow.z+0.5)),selectedUv(0)).rg;
      coatMask*=coatTexel.r;coatSmoothness*=coatTexel.g;
    }
#endif
    mediump vec3 coatNormal=normalize(vNormal);
    mediump float coatRough=clamp(1.0-coatSmoothness,.07,1.0);
    highp float coatNv=max(dot(coatNormal,v),1e-4);
    mediump float coatFresnel=.04+.96*pow5(1.0-float(coatNv));
    color*=1.0-coatMask*coatFresnel;
    aetherSpecularSum*=1.0-coatMask*coatFresnel;
    mediump vec3 coatLight=vec3(0.0);
    highp float coatNl=dot(coatNormal,environment.sunDirectionIntensity.xyz);
    if(coatNl>0.0) {
      mediump vec3 coatHalf=normalize(v+environment.sunDirectionIntensity.xyz);
      highp float coatAlpha=coatRough*coatRough;
      mediump float coatF=.04+.96*pow5(1.0-clamp(dot(v,coatHalf),0.0,1.0));
      coatLight+=vec3(min(distribution(max(dot(coatNormal,coatHalf),0.0),coatAlpha)*visibility(coatNv,coatNl,coatAlpha)*coatNl*
                 coatF*vec3(sunRadiance)*sunVisibility*aetherHighlightWeight,vec3(AETHER_MEDIUMP_MAX)));
    }
    if(environment.quality.w>0.5) {
      mediump vec2 coatBrdf=environmentBrdf(float(coatNv),coatRough);
      coatLight+=environmentRadiance(reflect(-v,coatNormal),coatRough*environment.parameters.z)*(.04*coatBrdf.x+coatBrdf.y)*occlusion;
    }
    color+=coatLight*coatMask;
    aetherSpecularSum+=coatLight*coatMask;
  }
  mediump float emissiveDetailWeight=materialDetailWeight(
      cameraDistance,environment.materialDistanceParameters.y);
  if(hasMaterialFeature(flags,8u) && emissiveDetailWeight>0.0)
    color+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*
        frame.emissiveFactorStrength.a*emissiveDetailWeight;
  // R4: isolar um dado do material na prévia do editor (a sessão só marca fora de Play).
  if(materialExtension) {
    uint isolate=uint(aetherMaterialRow(10u).x+0.5);
    if(isolate!=0u) {
      mediump vec3 shown=isolate==1u?vec3(occlusion):isolate==2u?vec3(rough):isolate==3u?vec3(metal):
                         isolate==4u?vec3(base.a):n*0.5+0.5;
      if((frame.materialFlags.z&1u)!=0u) shown=mix(12.92*shown,1.055*pow(shown,vec3(1.0/2.4))-.055,
                                                 greaterThan(shown,vec3(.0031308)));
      outColor=vec4(shown,1.0);
      return;
    }
  }
  aetherWriteSurface(color,aetherSpecularSum,base.a,flags,blendOutput,shadingBits);
}
