layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 sceneSky;
  vec4 sceneFogColorDensity;
  vec4 sceneFog;
  vec4 scenePost;
  vec4 sceneAo;
  vec4 sceneAoDetail;
  // Transient frame data appended after the stable AEEN resource payload.
  // Rows transform a world-space direction into camera/view space.
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
  vec4 quality; // normal distance, specular distance, hemispheric, specular probe
  mat4 shadowViewProjection[4];
  vec4 shadowSplitDepths;
  vec4 shadowParameters;
  vec4 shadowWorldUnitsPerTexel;
  vec4 shadowFilterParameters; // near PCF radius, far PCF radius, reserved
  vec4 shadowTransitionParameters; // cascade blend, final distance fade, camera focal length, reserved
  vec4 materialDistanceParameters; // MR map, emissive map, fade-band ratio, reserved
  vec4 waterParameters; // wave count, base height, time, reserved
  vec4 waterOptics; // IOR, roughness, turbidity, foam threshold
  vec4 waterDeepColorFoam;
  vec4 waterShallowColorDistance;
  vec4 waterAbsorption;
  vec4 waterWaveShape[8]; // direction.xz, amplitude, wave number
  vec4 waterWaveMotion[8]; // speed, steepness, phase, reserved
  vec4 waterInteractionParameters; // active count, reserved
  vec4 waterInteractionShape[8]; // center.xz, start time, amplitude
  vec4 waterInteractionMotion[8]; // wavelength, speed, decay, duration
  // centro x, centro z, lado da area em metros, resolucao da grade. Resolucao
  // zero significa que nao ha ondulacao e o vertice pula a leitura inteira.
  vec4 waterRippleArea;
  vec4 waterSurfaceDetail; // foam elevation/coverage, micro height/wavelength
  // Luzes pontuais e spot. `x` e quantas valem neste quadro. O array e o mesmo
  // `PunctualLight` do C++, quatro vec4 por luz: posicao+alcance, cor*intensidade
  // +escala do cone, direcao+deslocamento do cone, e a sombra local (primeiro
  // tile, tiles+16 se suave, forca, desvio na normal). Estes membros ficam SO
  // nesta copia do bloco: o vertice nao le luz, e std140 permite que um estagio
  // declare menos membros no fim do mesmo buffer.
  vec4 punctualLightParameters;
  vec4 punctualLights[32];
  // Atlas de sombra local (G6-B): quantos tiles valem e o inverso do lado; por
  // tile, a matriz da luz e o retangulo normalizado (w = texel no alcance).
  vec4 localShadowParameters;
  mat4 localShadowViewProjection[16];
  vec4 localShadowRect[16];
  // AEEN v4 diffuse irradiance, copied at load time. Appending preserves every
  // existing UBO offset. RGB stores cosine-convolved real SH L2; W is padding.
  vec4 diffuseIrradianceSh[9];
  // G4b, Cloud Layer (cloud_layer.glsl): fim do bloco, depois das linhas do céu.
  layout(offset=3536) vec4 cloudParameters; // opacidade, cobertura, cirros, densidade
  layout(offset=3552) vec4 cloudShape;      // 1/tamanho, altitude, vento x, z
  layout(offset=3568) vec4 cloudTint;       // tom*2^EV, luz do céu
  layout(offset=3584) vec4 cloudShadow;     // força da sombra, reservados
} environment;
layout(set=1,binding=1) uniform sampler2D environmentMap;
// sampler2DShadow: o compare e o filtro bilinear 2x2 saem numa unica busca de
// hardware. Ver createShadowResources -- o sampler correspondente e criado com
// compareEnable e filtro linear; sem isso este declarador seria invalido.
layout(set=1,binding=2) uniform sampler2DShadow shadowAtlas;
layout(set=1,binding=3) uniform sampler2D environmentSpecularMap;
layout(set=1,binding=4) uniform sampler2D environmentBrdfLut;
// Atlas das luzes locais, com o mesmo sampler de compare das cascatas.
layout(set=1,binding=17) uniform sampler2DShadow localShadowAtlas;
// Mapa de nuvens repetível (renderer/cloud_layer.cpp), o mesmo do céu.
layout(set=1,binding=18) uniform mediump sampler2D cloudMap;
#include "cloud_layer.glsl"

highp vec3 environmentSourceDirection(highp vec3 direction) {
  // Same positive longitude offset used by the visible panorama.
  float c=cos(environment.parameters.y),s=sin(environment.parameters.y);
  return vec3(c*direction.x-s*direction.z,direction.y,
              s*direction.x+c*direction.z);
}

mediump vec3 environmentDiffuseIrradiance(mediump vec3 direction) {
  direction=environmentSourceDirection(direction);
  direction=normalize(direction);
  mediump float x=direction.x,y=direction.y,z=direction.z;
  mediump float basis[9]=float[9](
      0.2820947918,
      0.4886025119*y,
      0.4886025119*z,
      0.4886025119*x,
      1.0925484306*x*y,
      1.0925484306*y*z,
      0.3153915653*(3.0*y*y-1.0),
      1.0925484306*x*z,
      0.5462742153*(x*x-z*z));
  mediump vec3 irradiance=vec3(0.0);
  for(int coefficient=0;coefficient<9;++coefficient)
    irradiance+=environment.diffuseIrradianceSh[coefficient].rgb*basis[coefficient];
  // Truncating at L2 can ring slightly around a very bright sun/horizon.
  // Negative irradiance is non-physical and would subtract material energy.
  return max(irradiance,vec3(0.0));
}

mediump vec3 environmentAmbientDiffuse(mediump vec3 normal) {
  // The cooked coefficients contain irradiance E. Lambertian diffuse uses
  // E/pi; the existing strength remains the common authoring/volume scale.
  if(environment.diffuseIrradianceSh[0].w>0.5 &&
     environment.quality.z>0.5 && environment.sceneSky.w<0.5)
    return environmentDiffuseIrradiance(normal)*
        (environment.ambientColorStrength.w/PI);

  // AEEN v1-v3, atmosphere, and the Constant quality tier retain their exact
  // hemispheric fallback. No coefficient is fabricated for legacy assets.
  mediump float skyWeight=environment.quality.z>0.5?
      clamp(normal.y*0.5+0.5,0.0,1.0):1.0;
  mediump vec3 ambient=mix(environment.groundColorSaturation.rgb,
                           environment.ambientColorStrength.rgb,skyWeight);
  mediump float luminance=dot(ambient,vec3(0.2126,0.7152,0.0722));
  ambient=mix(vec3(luminance),ambient,
              clamp(environment.groundColorSaturation.w,0.0,2.0));
  return ambient*environment.ambientColorStrength.w;
}

// A UV do panorama permanece highp: sao 1024 px por eixo com costura horizontal,
// e o fract() perto da costura e exatamente onde fp16 produziria uma emenda
// visivel. A radiancia devolvida ja pode descer para mediump.
highp vec2 environmentUv(highp vec3 direction) {
  // Callers provide a normalized reflection direction. reflect() preserves
  // length for normalized N/V, avoiding a redundant reciprocal sqrt per pixel.
  highp float phi=atan(direction.z,direction.x)+environment.parameters.y;
  return vec2(fract(phi/(2.0*PI)+.5),acos(clamp(direction.y,-1.0,1.0))/PI);
}

// ENVIRONMENT_PROJECTION: 0 = decide em runtime (legado), 1 = sempre
// equiretangular, 2 = sempre octaedrico. Ver a declaracao em
// dirt_road_shading.glsl -- a constante existe para que o ramo nao usado saia
// do binario em vez de custar registradores em todo fragmento.
mediump vec3 environmentRadianceUnscaled(highp vec3 direction,mediump float lod) {
  if(ENVIRONMENT_PROJECTION==1u ||
     (ENVIRONMENT_PROJECTION==0u && environment.parameters.w<0.5))
    return textureLod(environmentMap,environmentUv(direction),
                      clamp(lod,0.0,environment.parameters.z)).rgb;
  // Octahedral projection is homogeneous: reflect() already returns a unit
  // direction, but even a slightly non-unit vector maps correctly without a
  // reciprocal sqrt. This replaces atan+acos in the hot PBR fragment path.
  direction=environmentSourceDirection(direction);
  highp vec2 folded=direction.xz/
      max(abs(direction.x)+abs(direction.y)+abs(direction.z),1e-8);
  if(direction.y<0.0)
    folded=(1.0-abs(folded.yx))*mix(vec2(-1.0),vec2(1.0),
                                    greaterThanEqual(folded,vec2(0.0)));
  highp vec2 uv=folded*0.5+0.5;
  return textureLod(environmentSpecularMap,uv,clamp(lod,0.0,environment.parameters.z)).rgb;
}

// `sceneAoDetail.w` e a escala do reflexo do ambiente: a razao sol autorado / sol
// do recurso (quando o sol da cena esta em lux) vezes o Reflexo indireto do
// volume da camera. 1 no legado. Todo reflexo do ambiente passa por aqui.
mediump vec3 environmentRadiance(highp vec3 direction,mediump float lod) {
  return environmentRadianceUnscaled(direction,lod)*environment.sceneAoDetail.w;
}

mediump vec2 environmentBrdf(mediump float noV,mediump float roughness) {
  return environment.parameters.w>1.5
      ?texture(environmentBrdfLut,vec2(clamp(noV,0.0,1.0),clamp(roughness,0.0,1.0))).rg
      :vec2(.25+.75*(1.0-roughness),0.0);
}

#include "tone_mapping.glsl"
highp vec3 toneMapEnvironment(highp vec3 color) {
  // O intermediário HDR preserva radiância para bloom/neblina e aplica a
  // curva uma única vez no passe final. Prévias sem pós mantêm o caminho inline.
  if(environment.scenePost.z>0.5) return max(color,vec3(0));
  color=max(color*environment.parameters.x*exp2(environment.scenePost.x),vec3(0));
  return astraToneMap(color,environment.scenePost.y);
}

mediump float sampleDirectionalShadowCascade(highp vec3 worldPosition,
                                              mediump vec3 normal,
                                              int cascade,
                                              int cascadeCount) {
  highp vec3 offsetPosition=worldPosition+normal*
      (environment.shadowParameters.w*environment.shadowWorldUnitsPerTexel[cascade]);
  highp vec4 clip=environment.shadowViewProjection[cascade]*vec4(offsetPosition,1.0);
  highp vec3 projected=clip.xyz/clip.w;
  highp vec2 localUv=projected.xy*0.5+0.5;
  if(projected.z<=0.0 || projected.z>=1.0 || any(lessThan(localUv,vec2(0.0))) ||
     any(greaterThan(localUv,vec2(1.0)))) return 1.0;
  highp vec2 tile=vec2(float(cascade&1),float(cascade>>1));
  highp float grid=cascadeCount>1?2.0:1.0;
  highp vec2 atlasUv=(localUv+tile)/grid;
  highp float texel=environment.shadowParameters.x;
  // sampler2DShadow with linear filtering evaluates a 2x2 comparison footprint.
  // Confining its center to half a texel inside this tile prevents a cascade
  // from sampling the neighboring atlas tile at its guard-band edge.
  highp vec2 tileMinimum=tile/grid+vec2(texel*0.5);
  highp vec2 tileMaximum=(tile+1.0)/grid-vec2(texel*0.5);
  atlasUv=clamp(atlasUv,tileMinimum,tileMaximum);
  highp float cascadeRatio=cascadeCount>1?float(cascade)/float(cascadeCount-1):0.0;
  int radius=int(mix(environment.shadowFilterParameters.x,
                     environment.shadowFilterParameters.y,cascadeRatio)+0.5);
  // Nenhum laco de contagem dinamica, e nenhum grid 5x5 com `continue`.
  //
  // O corpo anterior percorria 25 iteracoes para tomar 9 amostras na cascata
  // proxima e 1 na distante, com desvio dinamico que o compilador so podia
  // predicar. Trocar por um laco de limites dinamicos foi pior ainda: a
  // contagem de voltas deixa de ser conhecida e o compilador nao desenrola, o
  // que serializa buscas de textura dependentes.
  //
  // Cada busca agora e um PCF 2x2 de hardware, ja filtrado. Isso muda o
  // calculo do kernel: quatro buscas em +-0,5 texel cobrem a mesma vizinhanca
  // 3x3 que nove buscas NAO filtradas cobriam, com pesos bilineares em vez de
  // degraus. Menos da metade das buscas para a mesma suavidade -- nao e perda
  // de qualidade, e a qualidade que o hardware ja estava pronto para dar.
  if(radius<=0) return texture(shadowAtlas,vec3(atlasUv,projected.z));
  if(radius==1) {
    highp vec2 offset=vec2(texel*0.5);
    mediump float sum=texture(shadowAtlas,vec3(clamp(atlasUv+vec2(-offset.x,-offset.y),tileMinimum,tileMaximum),projected.z));
    sum+=texture(shadowAtlas,vec3(clamp(atlasUv+vec2( offset.x,-offset.y),tileMinimum,tileMaximum),projected.z));
    sum+=texture(shadowAtlas,vec3(clamp(atlasUv+vec2(-offset.x, offset.y),tileMinimum,tileMaximum),projected.z));
    sum+=texture(shadowAtlas,vec3(clamp(atlasUv+vec2( offset.x, offset.y),tileMinimum,tileMaximum),projected.z));
    return sum*0.25;
  }
  // Kernel largo (UltraSoft): a soma de 3x3 buscas filtradas espacadas de um
  // texel e, exatamente, uma soma ponderada dos 4x4 texels em volta: todas as
  // buscas compartilham a mesma fracao f dentro do texel, e por eixo os pesos
  // somam (1-f, 1, 1, f). Quatro textureGather com comparacao devolvem esses
  // 16 resultados -- 4 buscas no lugar de 9, mesmo valor final. Medido na Vila
  // em S (POCO F7): a busca da sombra direcional era 20% do passe opaco.
  highp float atlasSize=1.0/texel;
  highp vec2 texelPosition=atlasUv*atlasSize-0.5;
  highp vec2 base=floor(texelPosition);
  mediump vec2 f=texelPosition-base;
  // O bloco 4x4 (base-1 .. base+2) fica dentro do tile, como o centro de cada
  // busca filtrada ficava; na borda a fracao deixa de casar com o bloco, mas
  // ali a cascata ja esta na faixa de guarda que nenhum fragmento visivel usa.
  highp vec2 tileStart=tile/grid*atlasSize,tileEnd=(tile+1.0)/grid*atlasSize;
  base=clamp(base,tileStart+1.0,tileEnd-3.0);
  // Cada gather devolve (i0,j1),(i1,j1),(i1,j0),(i0,j0) do 2x2 em volta do ponto.
  mediump vec4 g00=textureGather(shadowAtlas,(base+vec2(0.0,0.0))*texel,projected.z);
  mediump vec4 g10=textureGather(shadowAtlas,(base+vec2(2.0,0.0))*texel,projected.z);
  mediump vec4 g01=textureGather(shadowAtlas,(base+vec2(0.0,2.0))*texel,projected.z);
  mediump vec4 g11=textureGather(shadowAtlas,(base+vec2(2.0,2.0))*texel,projected.z);
  mediump vec2 low=1.0-f;
  // Linhas e colunas internas pesam 1; as externas, 1-f e f.
  mediump float visible=
      g00.w*low.x*low.y+g00.z*low.y+g00.x*low.x+g00.y+
      g10.w*low.y+g10.z*f.x*low.y+g10.x+g10.y*f.x+
      g01.w*low.x+g01.z+g01.x*low.x*f.y+g01.y*f.y+
      g11.w+g11.z*f.x+g11.x*f.y+g11.y*f.x*f.y;
  return visible*(1.0/9.0);
}

mediump float directionalCascadeShadow(highp vec3 worldPosition, mediump vec3 normal,
                                       highp float viewDepth) {
  int cascadeCount=int(environment.shadowParameters.y+0.5);
  if(cascadeCount<=0) return 1.0;
  highp float shadowEnd=environment.shadowSplitDepths[cascadeCount-1];
  if(viewDepth>shadowEnd) return 1.0;
  int cascade=0;
  while(cascade<cascadeCount-1 && viewDepth>environment.shadowSplitDepths[cascade])
    ++cascade;

  mediump float visibility=sampleDirectionalShadowCascade(
      worldPosition,normal,cascade,cascadeCount);
  highp float intervalStart=cascade>0?environment.shadowSplitDepths[cascade-1]:0.0;
  highp float intervalEnd=environment.shadowSplitDepths[cascade];
  highp float interval=max(intervalEnd-intervalStart,1e-4);
  if(cascade<cascadeCount-1 && environment.shadowTransitionParameters.x>0.0) {
    highp float width=interval*environment.shadowTransitionParameters.x;
    highp float start=intervalEnd-width;
    if(viewDepth>start) {
      mediump float nextVisibility=sampleDirectionalShadowCascade(
          worldPosition,normal,cascade+1,cascadeCount);
      mediump float blend=smoothstep(start,intervalEnd,viewDepth);
      visibility=mix(visibility,nextVisibility,blend);
    }
  } else if(cascade==cascadeCount-1 && environment.shadowTransitionParameters.y>0.0) {
    highp float width=interval*environment.shadowTransitionParameters.y;
    highp float start=intervalEnd-width;
    visibility=mix(visibility,1.0,smoothstep(start,intervalEnd,viewDepth));
  }
  return visibility;
}

// Sol que chega à superfície: sombra das cascatas vezes a das nuvens.
mediump float directionalShadow(highp vec3 worldPosition, mediump vec3 normal,
                                highp float viewDepth) {
  return directionalCascadeShadow(worldPosition,normal,viewDepth)*cloudShadowVisibility(worldPosition);
}
