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
const uint MATERIAL_WATER=512u; // renderer::MapMaterialWater
// Precisa casar com renderer::MaximumPunctualLights. Os dois lados leem a mesma
// memoria; divergir aqui leria alem do array ou apagaria luzes que couberam.
const int PUNCTUAL_LIGHT_LIMIT=8;
#include "environment_lighting.glsl"
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
highp vec2 selectedUv(uint slot) {
  uint set=(frame.materialFlags.y>>(slot*2))&3u;
  return aetherTransformUv(slot,set==2u?aetherWorldUv(vPosition,vNormal):set==1u?vUv1:vUv0);
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
  highp float nv=max(dot(n,v),1e-4);mediump vec3 h=normalize(v+l);
  highp float nh=max(dot(n,h),0);mediump float lh=max(dot(l,h),0);
  mediump vec3 f=fresnel(f0,f90,lh);highp float alpha=rough*rough;
  mediump float fd90=.5+2*rough*lh*lh;
  mediump float fd=(1+(fd90-1)*pow5(1-float(nl)))*(1+(fd90-1)*pow5(1-float(nv)))/PI;
  mediump float specular=f90>0.0?
      float(distribution(nh,alpha)*visibility(nv,nl,alpha)):0.0;
  return ((1-f)*(1-metal)*base*fd+specular*f)*radiance*float(nl);
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

mediump vec3 punctualLighting(highp vec3 position,mediump vec3 n,mediump vec3 v,
                              mediump vec3 base,mediump vec3 f0,mediump float f90,
                              mediump float metal,mediump float rough) {
  int count=int(environment.punctualLightParameters.x+0.5);
  mediump vec3 sum=vec3(0.0);
  for(int i=0;i<PUNCTUAL_LIGHT_LIMIT;++i) {
    if(i>=count) break;
    highp vec4 positionRange=environment.punctualLights[i*4];
    mediump vec4 colorIntensity=environment.punctualLights[i*4+1];
    mediump vec4 directionOffset=environment.punctualLights[i*4+2];
    highp vec4 shadow=environment.punctualLights[i*4+3];
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
    mediump float cone=clamp(dot(directionOffset.xyz,-l)*colorIntensity.w+directionOffset.w,0.0,1.0);
    cone*=cone;
    // A sombra so e buscada onde a luz ainda chega: fora do cone ou do
    // alcance, o atlas nao tem o que dizer e a busca seria desperdicio.
    if(cone<=0.0) continue;
    mediump float visible=localShadowVisibility(shadow,position,n,-toLight);
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
void main() {
  if(aetherLodDitherDiscard(gl_FragCoord.xy,vDither)) discard;
  uint flags=frame.materialFlags.x;
  uint isolation=GPU_COST_ISOLATION;
  if((flags&MATERIAL_WATER)!=0u) {
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
  bool impostor=(flags&MATERIAL_IMPOSTOR)!=0u && (frame.materialFlags.y>>16u)!=0u;
  highp vec2 impostorUv=vUv0;
  if(impostor) {
    impostorUv=aetherImpostorViewUv(flags,vUv0,vUv1,vColor.a,gl_FragCoord.xy);
    impostorUv=aetherImpostorSafeUv(impostorUv,impostorDx,impostorDy,
        vec2(textureSize(BASE_MAP,0)),frame.materialFlags.y>>16u,
        float(textureQueryLevels(BASE_MAP)-1));
  }
  mediump vec4 baseSample=impostor?
      textureGrad(BASE_MAP,impostorUv,impostorDx,impostorDy):texture(BASE_MAP,selectedUv(0));
  // R4: origem do alfa do material, antes do corte (igual à cobertura e à sombra).
  if(!impostor) baseSample.a=aetherAlphaFromSource(baseSample);
  mediump vec4 vertexColor=impostor?vec4(vColor.rgb,1.0):vColor;
  mediump vec4 base=baseSample*frame.baseColorFactor*vertexColor;
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
  mediump vec4 mr=vec4(1);
  if(hasMaterialFeature(flags,4u) && mrDetailWeight>0.0)
    mr=mix(vec4(1),texture(MR_MAP,selectedUv(2)),mrDetailWeight);
  bool materialExtension=aetherHasMaterialExtension();
  // R4: canais do mapa escolhidos no material; sem extensão, o do glTF (G e B).
  uint roughnessChannel=materialExtension?uint(aetherMaterialRow(9u).x+0.5):1u;
  uint metallicChannel=materialExtension?uint(aetherMaterialRow(9u).y+0.5):2u;
  mediump float rough=clamp(mr[roughnessChannel]*frame.materialFactors.x,.07,1);
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
  // A subtração fica em highp: `eye` e `vPosition` são coordenadas de mundo e o
  // mapa se estende por centenas de unidades, onde fp16 já perde resolução. Só
  // o resultado normalizado, que vive em [-1,1], desce para mediump.
  mediump vec3 v=environment.worldToViewRow0.w>0.0?-environment.worldToViewRow2.xyz:normalize(eye-vPosition);
  mediump float dielectricSpecular=clamp(frame.materialFactors.w,0.0,1.0);
  mediump vec3 f0=mix(vec3(.04*dielectricSpecular),base.rgb,metal);
  mediump float f90=mix(dielectricSpecular,1.0,metal);
  mediump vec3 sunRadiance=environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  highp float viewDepth=max(dot(environment.worldToViewRow2.xyz,vPosition-eye),0.0);
  // A luz direta é exatamente zero no hemisfério oposto. Consultar 1/9/25
  // texels de sombra nesse caso só gasta banda; o branch remove trabalho sem
  // mudar um único valor final.
  mediump float sunVisibility=dot(n,environment.sunDirectionIntensity.xyz)>0.0?
      directionalShadow(vPosition,n,viewDepth):1.0;
  mediump vec3 color=directLight(n,v,environment.sunDirectionIntensity.xyz,sunRadiance,
                         base.rgb,f0,f90,metal,rough)*sunVisibility;
  color+=punctualLighting(vPosition,n,v,base.rgb,f0,f90,metal,rough);
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
  color+=(1-metal)*base.rgb*ambientIrradiance*environment.ambientColorStrength.w*occlusion;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  mediump float specularDetailWeight=environment.quality.w>0.5?
      materialDetailWeight(cameraDistance,environment.quality.y):0.0;
  if(isolation!=2u && f90>0.0 && specularDetailWeight>0.0 && (metal>.01 || rough<.65)) {
    mediump vec3 reflection=reflect(-v,n);
    mediump vec3 specularEnvironment=environmentRadiance(reflection,rough*environment.parameters.z);
    mediump vec2 integratedBrdf=environmentBrdf(nv,rough);
    // The split-sum LUT already integrates Schlick's angular term. Applying
    // Fresnel again double-counted it; scaling the result by the dielectric
    // weight a second time also incorrectly extinguished metallic reflections.
    mediump vec3 integratedSpecular=environment.parameters.w>1.5?
        f0*integratedBrdf.x+vec3(f90*integratedBrdf.y):
        fresnel(f0,f90,nv)*integratedBrdf.x;
    color+=specularEnvironment*integratedSpecular*specularDetailWeight*occlusion;
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
  color=toneMapEnvironment(color);
  if((frame.materialFlags.z&1u)!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                             greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,base.a);
}
