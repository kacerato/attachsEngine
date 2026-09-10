layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
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
} environment;
layout(set=1,binding=1) uniform sampler2D environmentMap;
// sampler2DShadow: o compare e o filtro bilinear 2x2 saem numa unica busca de
// hardware. Ver createShadowResources -- o sampler correspondente e criado com
// compareEnable e filtro linear; sem isso este declarador seria invalido.
layout(set=1,binding=2) uniform sampler2DShadow shadowAtlas;
layout(set=1,binding=3) uniform sampler2D environmentSpecularMap;
layout(set=1,binding=4) uniform sampler2D environmentBrdfLut;

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
mediump vec3 environmentRadiance(highp vec3 direction,mediump float lod) {
  if(ENVIRONMENT_PROJECTION==1u ||
     (ENVIRONMENT_PROJECTION==0u && environment.parameters.w<0.5))
    return textureLod(environmentMap,environmentUv(direction),
                      clamp(lod,0.0,environment.parameters.z)).rgb;
  // Octahedral projection is homogeneous: reflect() already returns a unit
  // direction, but even a slightly non-unit vector maps correctly without a
  // reciprocal sqrt. This replaces atan+acos in the hot PBR fragment path.
  highp vec2 folded=direction.xz/
      max(abs(direction.x)+abs(direction.y)+abs(direction.z),1e-8);
  if(direction.y<0.0)
    folded=(1.0-abs(folded.yx))*mix(vec2(-1.0),vec2(1.0),
                                    greaterThanEqual(folded,vec2(0.0)));
  highp vec2 uv=folded*0.5+0.5;
  return textureLod(environmentSpecularMap,uv,clamp(lod,0.0,environment.parameters.z)).rgb;
}

mediump vec2 environmentBrdf(mediump float noV,mediump float roughness) {
  return environment.parameters.w>1.5
      ?texture(environmentBrdfLut,vec2(clamp(noV,0.0,1.0),clamp(roughness,0.0,1.0))).rg
      :vec2(.25+.75*(1.0-roughness),0.0);
}

mediump vec3 toneMapEnvironment(mediump vec3 color) {
  color=max(color*environment.parameters.x,vec3(0));
  // Stable filmic curve with a soft shoulder for the HDR sun.
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
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
  // Kernel largo (UltraSoft): grid 3x3 de buscas filtradas, espacadas de um
  // texel, cobrindo a vizinhanca 4x4 que o 5x5 nao filtrado cobria.
  mediump float visible=0.0;
  for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    visible+=texture(shadowAtlas,vec3(clamp(atlasUv+vec2(x,y)*texel,
                                            tileMinimum,tileMaximum),projected.z));
  return visible*(1.0/9.0);
}

mediump float directionalShadow(highp vec3 worldPosition, mediump vec3 normal,
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
