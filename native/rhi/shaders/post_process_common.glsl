precision highp float;
precision highp int;

layout(set=0,binding=0) uniform sampler2D sceneColor;
layout(set=0,binding=1) uniform sampler2D historyColor;
layout(set=0,binding=2) uniform sampler2D sceneDepth;
// Entradas temporais unificadas (temporal_projection.glsl): vetor RG16F
// atual -> anterior em UV da extensão, e as máscaras R8 do passe principal.
layout(set=0,binding=5) uniform sampler2D temporalVelocity;
layout(set=0,binding=6) uniform sampler2D reactiveMask;
layout(set=0,binding=7) uniform sampler2D compositionMask;
// G2 no G1: bloom em cadeia de mips (nível 1/2 já acumulado) e AO em 1/2.
layout(set=0,binding=8) uniform sampler2D bloomChain;
layout(set=0,binding=9) uniform sampler2D halfAmbientOcclusion;
// G2: LUT 32³ da gradação de cor (renderer/color_grading.cpp), RGBA16F linear,
// indexada por sqrt(cor do monitor). Só é lida com o bit 65536.
layout(set=0,binding=10) uniform highp sampler3D gradingLut;
// PK-5: resultado da profundidade de campo em 1/2 (rgb e alfa do bokeh) e a
// textura de sujeira da lente (Bloom › Lens Dirt).
layout(set=0,binding=11) uniform sampler2D depthOfFieldResult;
layout(set=0,binding=12) uniform sampler2D bloomDirt;
layout(set=0,binding=4,std430) readonly buffer ExposureState {
  float ev; float targetEv; float meteredEv; uint valid;
} automaticExposure;
layout(set=0,binding=3,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
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
  layout(offset=224) vec4 worldToViewRow0;
  layout(offset=240) vec4 worldToViewRow1;
  layout(offset=256) vec4 worldToViewRow2;
  layout(offset=3296) vec4 postViewport; // normalized physical xy/extent
  // zw: extensão renderizada / alocação das entradas temporais (profundidade,
  // vetor e máscaras), válida também quando a cor de entrada já foi ampliada.
  layout(offset=3472) vec4 temporalParameters;
  // PK-5: Color Adjustments (filtro rgb, matiz em graus) e Bloom (tom rgb,
  // espalhamento; brilho máximo). Ver DirtRoadFrameUniform.
  layout(offset=4112) vec4 postColorAdjust;
  layout(offset=4128) vec4 postBloomTint;
  layout(offset=4144) vec4 postBloomDetail; // clamp, sujeira, filtro bicúbico, reservado
  // PK-5 (depois do V-2, 4256): o resto do pós da URP. Ver DirtRoadFrameUniform.
  layout(offset=4256) vec4 postLensAxis;    // centro xy (URP: center*2-1), multiplicadores xy
  layout(offset=4272) vec4 postLensShape;   // theta, sigma, 1/escala, intensidade*100
  layout(offset=4288) vec4 postChroma;      // intensidade*0,05, liga distorção, reservados
  layout(offset=4304) vec4 postVignette;    // cor rgb, arredondada (proporção ou 1)
  layout(offset=4320) vec4 postVignetteShape; // centro xy, intensidade*3, suavidade*5
  layout(offset=4336) vec4 postMotion;      // intensidade, limite, iterações, reservado
  layout(offset=4352) vec4 postDof;         // modo (1 gauss, 2 bokeh), a, b, raio máximo
  layout(offset=4368) vec4 postDofDetail;   // amostras do bokeh, 1/aspecto, alta qualidade, reservado
  layout(offset=4384) vec4 postBloomDirt;   // escala xy, deslocamento xy da sujeira
  layout(offset=4400) vec4 postBokeh[22];   // até 43 amostras do bokeh (xy, zw)
} environment;
layout(push_constant) uniform PostPushConstants {
  // z bits: 1 bloom, 2 bicúbica, 4 FSR 1, 8 vetor, 16 cobertura estática,
  // 32 máscaras temporais, 64 só composição HDR (névoa+AO, antes do ampliador
  // temporal), 128 entrada já composta (depois do ampliador), 65536 LUT de
  // gradação (contraste e saturação passam a vir dela).
  vec4 texelFlags; // xy=1/source extent, z=bits acima, w=AA + debug*4
  vec4 bloom;      // threshold, intensity, sharpen, vignette intensity
  vec4 grade;      // contrast, saturation, encode sRGB, packed transform+feedback
  vec4 sourceTransform; // xy=active/source extent, z=focal length, w=display aspect
  vec4 currentCamera; // yaw, pitch, physical-NDC jitter x/y
  vec4 currentPositionNear; // camera xyz, near plane
  vec4 previousCamera; // previous yaw/pitch, current roll, previous roll
  vec4 previousPositionFar; // camera xyz, far plane
} post;
layout(location=0) in highp vec2 vUv;
layout(location=0) out vec4 outColor;
#if AETHER_TEMPORAL
layout(location=1) out vec4 outHistory;
#endif

float luma(vec3 value) { return dot(value, vec3(0.2126, 0.7152, 0.0722)); }

// Leitura da cena presa aos texels que a vista desenhou. A borda da vista vem
// da superfície girada (physicalSceneViewport) e cai em fração de texel (ex.:
// 100,6 logo abaixo da barra do editor). A rasterização só cobre texels com o
// centro dentro dela; o texel 100 fica com o fundo da câmera. O limite antigo
// (borda + meio texel = 101,1) misturava 40% dele na primeira linha: a emenda
// escura sob o Play, branca quando o fundo é o céu da noite. O limite agora é
// o centro do primeiro e do último texel desenhados (post_sample_bounds.h).
vec2 clampSourceUv(vec2 uv) {
  vec2 size=1.0/max(post.texelFlags.xy,vec2(1.0e-8));
  vec2 start=environment.postViewport.xy*post.sourceTransform.xy*size;
  vec2 end=(environment.postViewport.xy+environment.postViewport.zw)*post.sourceTransform.xy*size;
  vec2 lower=(ceil(start-0.5)+0.5)*post.texelFlags.xy;
  vec2 upper=(floor(end-0.5)+0.5)*post.texelFlags.xy;
  return clamp(uv,lower,max(lower,upper));
}

float viewDistanceFromDepth(float depth) {
  float nearPlane=post.currentPositionNear.w;
  float farPlane=post.previousPositionFar.w;
  if(post.sourceTransform.z<0.0) return mix(nearPlane,farPlane,depth);
  return nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
}
float viewDistance(vec2 uv) {
  return viewDistanceFromDepth(texture(sceneDepth,clampSourceUv(uv)).r);
}

bool postFlag(float bit) { return mod(floor(post.texelFlags.z/bit),2.0)>0.5; }
vec2 temporalInputUv(vec2 uv) { return uv*environment.temporalParameters.zw; }

// PK-5: a coordenada local da câmera (0..1 dentro do Viewport Rect) que a
// distorção da lente, a vinheta e a sujeira usam, como o uv do UberPost.
vec2 gLocalUv=vec2(0.5);
vec2 gResolveUv=vec2(0.5);
vec2 localFromOutput(vec2 uv) { return (uv-environment.postViewport.xy)/max(environment.postViewport.zw,vec2(1.0e-6)); }
vec2 outputFromLocal(vec2 uv) { return environment.postViewport.xy+uv*environment.postViewport.zw; }
// Lens Distortion da URP (PostProcessing/Common.hlsl, DistortUV).
vec2 distortUv(vec2 uv) {
  if(environment.postChroma.y<0.5) return uv;
  uv=(uv-0.5)*environment.postLensShape.z+0.5;
  vec2 ruv=environment.postLensAxis.zw*(uv-0.5-environment.postLensAxis.xy);
  float ru=max(length(ruv),1.0e-5);
  if(environment.postLensShape.w>0.0) {
    float wu=ru*environment.postLensShape.x;
    ru=tan(wu)/(ru*environment.postLensShape.y);
  } else {
    ru=environment.postLensShape.x*atan(ru*environment.postLensShape.y)/ru;
  }
  return uv+ruv*(ru-1.0);
}
// Profundidade lida pelos efeitos de lente: na entrada já ampliada (128) a
// profundidade é a da resolução interna.
float effectRawDepth(vec2 sourceUv) {
  return texture(sceneDepth,postFlag(128.0)?temporalInputUv(sourceUv):clampSourceUv(sourceUv)).r;
}

vec3 applySceneFog(vec2 sourceUv, vec3 color) {
  // Depois do ampliador temporal a névoa já está na cor: aplicá-la de novo
  // dobraria a densidade, e com profundidade de outra resolução.
  // 512: a cor de entrada já recebeu a névoa por texel no pré-passe (256).
  if(environment.sceneFog.x>0.5 && !postFlag(128.0) && !postFlag(512.0)) {
    float rawDepth=texture(sceneDepth,clampSourceUv(sourceUv)).r;
    float viewDepth=viewDistanceFromDepth(rawDepth);
    float falloff=environment.sceneFog.w;
    float opticalDepth;
    // PK-1: Fog Mode da janela Lighting. x = 1 exponencial, 2 exponencial ao
    // quadrado, 3 linear (Start/End; a linha da densidade carrega o End).
    // +4 = a neblina também cobre o céu ("Acompanhar o céu", o visual antigo);
    // sem isso o Skybox fica limpo, como na Unity (o céu não recebe Fog).
    float fogMode=environment.sceneFog.x;
    bool fogsSky=fogMode>4.5;
    if(fogsSky) fogMode-=4.0;
    else if(rawDepth>=0.99999) return color;
    if(fogMode>2.5) {
      float start=environment.sceneFog.y,end=environment.sceneFogColorDensity.w;
      float linearFog=clamp((viewDepth-start)/max(end-start,1.0e-4),0.0,1.0);
      return mix(color,environment.sceneFogColorDensity.rgb,linearFog);
    }
    if(falloff<=0.0) {
      // Exact legacy path: default/old scenes retain depth-linear exponential fog.
      opticalDepth=environment.sceneFogColorDensity.w*
          max(0.0,viewDepth-environment.sceneFog.y);
    } else {
      vec2 outputUv=sourceUv/max(post.sourceTransform.xy,vec2(1.0e-6));
      vec2 physicalNdc=(outputUv-environment.postViewport.xy)/
          max(environment.postViewport.zw,vec2(1.0e-6))*2.0-1.0;
      int packed=int(floor(post.grade.w));
      vec4 surface=vec4(float((packed>>0)&3)-1.0,float((packed>>2)&3)-1.0,
                        float((packed>>4)&3)-1.0,float((packed>>6)&3)-1.0);
      vec2 cameraNdc=vec2(dot(surface.xz,physicalNdc),dot(surface.yw,physicalNdc));
      float orthoHalfHeight=max(environment.worldToViewRow0.w,0.0);
      vec3 viewDirection;
      float rayOriginHeight=post.currentPositionNear.y;
      float rayDistance;
      if(orthoHalfHeight>0.0) {
        vec3 viewOffset=vec3(cameraNdc.x*orthoHalfHeight*post.sourceTransform.w,
                             -cameraNdc.y*orthoHalfHeight,0.0);
        rayOriginHeight+=dot(vec3(environment.worldToViewRow0.y,
                                 environment.worldToViewRow1.y,
                                 environment.worldToViewRow2.y),viewOffset);
        viewDirection=vec3(0.0,0.0,1.0);
        rayDistance=viewDepth;
      } else {
        float focal=max(abs(post.sourceTransform.z),1.0e-6);
        viewDirection=normalize(vec3(cameraNdc.x*post.sourceTransform.w/focal,
                                     -cameraNdc.y/focal,1.0));
        rayDistance=viewDepth/max(viewDirection.z,1.0e-6);
      }
      float worldDirectionY=dot(vec3(environment.worldToViewRow0.y,
                                     environment.worldToViewRow1.y,
                                     environment.worldToViewRow2.y),viewDirection);
      float segment=max(0.0,rayDistance-environment.sceneFog.y);
      float startHeight=rayOriginHeight+worldDirectionY*environment.sceneFog.y;
      float density=environment.sceneFogColorDensity.w;
      opticalDepth=0.0;
      if(segment>0.0&&density>0.0) {
        float endHeight=startHeight+worldDirectionY*segment;
        float change=abs(falloff*worldDirectionY*segment);
        // Integrate from the denser endpoint and work in log space. Clamping
        // two exponentials separately would cancel their saturation and make
        // a ray crossing from high altitude into dense low fog transparent.
        float factor=change<1.0e-3?1.0-change*0.5+change*change/6.0:
                     (1.0-exp(-change))/change;
        float logDepth=log(density)+log(segment)+log(factor)-
            falloff*(min(startHeight,endHeight)-environment.sceneFog.z);
        opticalDepth=exp(clamp(logDepth,-80.0,log(80.0)));
      }
    }
    if(fogMode>1.5) opticalDepth*=opticalDepth;
    float fog=1.0-exp(-opticalDepth);
    color=mix(color,environment.sceneFogColorDensity.rgb,clamp(fog,0.0,1.0));
  }
  return color;
}

vec3 sampleScene(vec2 uv) {
  vec2 sourceUv=clampSourceUv(uv);
  return applySceneFog(sourceUv, texture(sceneColor,sourceUv).rgb);
}

vec3 reconstructScene(vec2 uv) {
  // Catmull-Rom has four taps per axis. The two central, positive weights
  // share one bilinear fetch, reducing the separable 4x4 kernel to nine.
  // Clamp every fetch to the active rectangle: dynamic resolution leaves
  // unused texels in the allocation which must never leak into the image.
  vec2 pixel=uv/post.texelFlags.xy-0.5;
  vec2 base=floor(pixel);
  vec2 f=pixel-base;
  vec2 w0=f*(-0.5+f*(1.0-0.5*f));
  vec2 w1=1.0+f*f*(-2.5+1.5*f);
  vec2 w2=f*(0.5+f*(2.0-1.5*f));
  vec2 w3=f*f*(-0.5+0.5*f);
  vec2 middle=w1+w2;
  vec2 p0=(base-0.5)*post.texelFlags.xy;
  vec2 p1=(base+0.5+w2/middle)*post.texelFlags.xy;
  vec2 p2=(base+2.5)*post.texelFlags.xy;
  vec3 weightsX=vec3(w0.x,middle.x,w3.x);
  vec3 weightsY=vec3(w0.y,middle.y,w3.y);
  vec3 positionsX=vec3(p0.x,p1.x,p2.x);
  vec3 positionsY=vec3(p0.y,p1.y,p2.y);
  vec3 result=vec3(0),minimum=vec3(1e30),maximum=vec3(-1e30);
  for(int y=0;y<3;++y) for(int x=0;x<3;++x) {
    vec3 value=texture(sceneColor,clampSourceUv(vec2(positionsX[x],positionsY[y]))).rgb;
    result+=value*weightsX[x]*weightsY[y];
    minimum=min(minimum,value);maximum=max(maximum,value);
  }
  // Negative lobes restore detail but may ring around HDR highlights. Keep
  // reconstruction inside the source neighbourhood before tone mapping.
  return applySceneFog(uv,clamp(result,minimum,maximum));
}

vec3 aoViewPosition(vec2 uv,float depth) {
  vec2 outputUv=uv/max(post.sourceTransform.xy,vec2(1e-6));
  vec2 physicalNdc=(outputUv-environment.postViewport.xy)/
      max(environment.postViewport.zw,vec2(1e-6))*2.0-1.0;
  int packed=int(floor(post.grade.w));
  vec4 surface=vec4(float((packed>>0)&3)-1.0,float((packed>>2)&3)-1.0,
                   float((packed>>4)&3)-1.0,float((packed>>6)&3)-1.0);
  physicalNdc-=post.currentCamera.zw;
  vec2 ndc=vec2(dot(surface.xz,physicalNdc),dot(surface.yw,physicalNdc));
  float halfHeight=max(environment.worldToViewRow0.w,0.0);
  vec2 xy=vec2(ndc.x*post.sourceTransform.w,-ndc.y);
  return vec3(xy*(halfHeight>0.0?halfHeight:
      depth/max(abs(post.sourceTransform.z),1e-6)),depth);
}

float screenSpaceAmbientOcclusion(vec2 uv) {
  if(environment.sceneAo.x<0.5 || postFlag(128.0)) return 1.0;
  float rawDepth=texture(sceneDepth,clampSourceUv(uv)).r;
  float center=viewDistanceFromDepth(rawDepth);
  vec3 position=aoViewPosition(uv,center);
  // Compare samples against the local tangent plane. Comparing only their
  // depth incorrectly occluded every sloping floor and changed its brightness
  // as the camera rotated. Evaluate derivatives before the sky branch.
  vec3 normal=cross(dFdx(position),dFdy(position));
  float normalLength=length(normal);
  if(rawDepth>=0.999999 || normalLength<1e-8) return 1.0;
  normal/=normalLength;
  if(dot(normal,-position)<0.0) normal=-normal;
  float radius=max(environment.sceneAo.y,0.05);
  float radiusPixels=clamp(radius*abs(post.sourceTransform.z)/
      max(center*post.texelFlags.y*2.0,1.0e-5),1.0,48.0);
  const vec2 directions[12]=vec2[12](
    vec2(1,0),vec2(.866,.5),vec2(.5,.866),vec2(0,1),
    vec2(-.5,.866),vec2(-.866,.5),vec2(-1,0),vec2(-.866,-.5),
    vec2(-.5,-.866),vec2(0,-1),vec2(.5,-.866),vec2(.866,-.5));
  float occlusion=0.0;
  // Rotação barata por pixel evita que as doze direções apareçam como raios
  // fixos, sem textura de ruído nem estado temporal adicional.
  float angle=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)*6.2831853;
  mat2 rotation=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
  for(int index=0;index<12;++index) {
    float ring=.35+.65*float((index%3)+1)/3.0;
    vec2 offset=rotation*directions[index]*post.texelFlags.xy*radiusPixels*ring;
    vec2 sampleUv=uv+offset;
    if(any(notEqual(sampleUv,clampSourceUv(sampleUv)))) continue;
    float sampleDepth=texture(sceneDepth,sampleUv).r;
    if(sampleDepth>=0.999999)continue;
    vec3 delta=aoViewPosition(sampleUv,viewDistanceFromDepth(sampleDepth))-position;
    float distance=length(delta);
    float height=dot(normal,delta);
    float range=max(0.0,1.0-distance/radius);
    float horizon=max(0.0,(height-environment.sceneAoDetail.x)/max(distance,1e-5));
    occlusion+=horizon*range;
  }
  float visibility=clamp(1.0-occlusion*(environment.sceneAo.z/12.0),0.0,1.0);
  return pow(visibility,max(environment.sceneAo.w,0.1));
}

// Alvos em 1/2 (bloom, DoF, AO): com a resolução dinâmica abaixo do teto só o
// canto (ceil(interna/2)) é escrito neste quadro; o resto da alocação guarda
// um quadro em escala maior ou lixo. A leitura da cena se prende ao retângulo
// com meio texel DA CENA, mas meio texel do alvo em 1/2 são dois da cena: na
// última coluna e linha o bilinear puxava metade do texel não escrito e o
// bloom acendia uma faixa branca na borda da tela. Limites em uv do alvo, meio
// texel dele para dentro (renderer/post_sample_bounds.h, testado no host).
vec2 halfActiveExtent() {
  vec2 internal=floor(post.sourceTransform.xy/max(post.texelFlags.xy,vec2(1.0e-8))+0.5);
  return max(floor((internal+1.0)*0.5),vec2(1.0));
}
vec2 clampHalfUv(vec2 uv,vec2 size) {
  vec2 written=min(halfActiveExtent(),size);
  return clamp(uv,vec2(0.5)/size,(written-0.5)/size);
}
// AO calculada em 1/2 (bit 2048) e reconstruída aqui com as quatro amostras
// do filtro bilinear, ponderadas pela profundidade para não vazar em bordas.
float upsampledAmbientOcclusion(vec2 uv) {
  if(environment.sceneAo.x<0.5 || postFlag(128.0)) return 1.0;
  vec2 size=vec2(textureSize(halfAmbientOcclusion,0));
  // A partir da última coluna ativa o peso dos texels de fora é zero.
  vec2 pixel=clamp(uv*size-0.5,vec2(0.0),min(halfActiveExtent(),size)-1.0);
  vec2 base=floor(pixel);
  vec2 f=pixel-base;
  vec4 ao=textureGather(halfAmbientOcclusion,(base+1.0)/size,0); // (0,1)(1,1)(1,0)(0,0)
  vec2 offsets[4]=vec2[4](vec2(0,1),vec2(1,1),vec2(1,0),vec2(0,0));
  float bilinear[4]=float[4]((1.0-f.x)*f.y,f.x*f.y,f.x*(1.0-f.y),(1.0-f.x)*(1.0-f.y));
  float center=viewDistance(uv);
  float sum=0.0,weightSum=0.0,nearest=1e30,nearestAo=1.0;
  for(int index=0;index<4;++index) {
    float depth=viewDistance((base+offsets[index]+0.5)/size);
    float relative=abs(depth-center)/max(center,1.0e-3);
    float weight=bilinear[index]*max(0.0,1.0-relative*20.0);
    sum+=ao[index]*weight;weightSum+=weight;
    if(relative<nearest){nearest=relative;nearestAo=ao[index];}
  }
  return weightSum>1.0e-4?sum/weightSum:nearestAo;
}

vec3 sampleFxaa(vec2 uv, out vec3 unfiltered, out vec3 crossAverage) {
  vec2 px = post.texelFlags.xy;
  float aaMode=mod(post.texelFlags.w,4.0);
  vec3 center = sampleScene(uv);
  unfiltered = center;
  crossAverage = center;
  if (post.bloom.z > 0.0 || aaMode >= 2.0) {
    vec3 north = sampleScene(uv + vec2(0.0, -px.y));
    vec3 south = sampleScene(uv + vec2(0.0,  px.y));
    vec3 west  = sampleScene(uv + vec2(-px.x, 0.0));
    vec3 east  = sampleScene(uv + vec2( px.x, 0.0));
    crossAverage = (north + south + west + east) * 0.25;
  }
  if (aaMode < 0.5 || aaMode > 1.5) return center;

  // FXAA 3.x-style edge search. Reject non-edges first; unlike the old cross
  // average this does not blur foliage interiors and material texture detail.
  vec3 northWest = sampleScene(uv + vec2(-px.x, -px.y));
  vec3 northEast = sampleScene(uv + vec2( px.x, -px.y));
  vec3 southWest = sampleScene(uv + vec2(-px.x,  px.y));
  vec3 southEast = sampleScene(uv + vec2( px.x,  px.y));
  float lumaCenter = luma(center);
  float lumaNorthWest = luma(northWest);
  float lumaNorthEast = luma(northEast);
  float lumaSouthWest = luma(southWest);
  float lumaSouthEast = luma(southEast);
  float minimum = min(lumaCenter, min(min(lumaNorthWest, lumaNorthEast),
                                      min(lumaSouthWest, lumaSouthEast)));
  float maximum = max(lumaCenter, max(max(lumaNorthWest, lumaNorthEast),
                                      max(lumaSouthWest, lumaSouthEast)));
  float range = maximum - minimum;
  if (range < max(0.0312, maximum * 0.125)) return center;

  vec2 direction;
  direction.x = -((lumaNorthWest + lumaNorthEast) -
                  (lumaSouthWest + lumaSouthEast));
  direction.y =  ((lumaNorthWest + lumaSouthWest) -
                  (lumaNorthEast + lumaSouthEast));
  float directionReduce = max((lumaNorthWest + lumaNorthEast +
                               lumaSouthWest + lumaSouthEast) * (0.25 * 0.25),
                              1.0 / 128.0);
  float reciprocalMinimum = 1.0 /
      (min(abs(direction.x), abs(direction.y)) + directionReduce);
  direction = clamp(direction * reciprocalMinimum, vec2(-8.0), vec2(8.0)) * px;

  vec3 candidateNear = 0.5 * (
      sampleScene(uv + direction * (1.0 / 3.0 - 0.5)) +
      sampleScene(uv + direction * (2.0 / 3.0 - 0.5)));
  vec3 candidateFar = candidateNear * 0.5 + 0.25 * (
      sampleScene(uv + direction * -0.5) +
      sampleScene(uv + direction *  0.5));
  float farLuma = luma(candidateFar);
  return (farLuma < minimum || farLuma > maximum) ? candidateNear : candidateFar;
}

const float kBloomLevels=5.0;
// Bloom › High Quality Filtering: B-spline cúbica em 4 leituras bilineares
// (Sigg e Hadwiger, GPU Gems 2 cap. 20), como o SampleTexture2DBicubic da URP.
// As quatro leituras também ficam no retângulo ativo do alvo em 1/2 (a pegada
// 4x4 passa 1,5 texel da borda).
vec4 sampleHalfBicubic(sampler2D source,vec2 uv) {
  vec2 size=vec2(textureSize(source,0));
  uv=clampHalfUv(uv,size);
  vec2 pixel=uv*size-0.5;
  vec2 f=fract(pixel);
  vec2 base=floor(pixel);
  vec2 w0=(1.0/6.0)*(1.0-f)*(1.0-f)*(1.0-f);
  vec2 w1=(1.0/6.0)*(4.0-6.0*f*f+3.0*f*f*f);
  vec2 w2=(1.0/6.0)*(1.0+3.0*f+3.0*f*f-3.0*f*f*f);
  vec2 w3=(1.0/6.0)*f*f*f;
  vec2 g0=w0+w1,g1=w2+w3;
  vec2 h0=clampHalfUv((w1/g0-1.0+base+0.5)/size,size),h1=clampHalfUv((w3/g1+1.0+base+0.5)/size,size);
  return g0.y*(g0.x*texture(source,vec2(h0.x,h0.y))+g1.x*texture(source,vec2(h1.x,h0.y)))+
         g1.y*(g0.x*texture(source,vec2(h0.x,h1.y))+g1.x*texture(source,vec2(h1.x,h1.y)));
}
vec3 sampleBloomChain(vec2 uv) {
  return environment.postBloomDetail.z<0.5?texture(bloomChain,clampHalfUv(uv,vec2(textureSize(bloomChain,0)))).rgb:
                                           sampleHalfBicubic(bloomChain,uv).rgb;
}
vec3 bloomNeighborhood(vec2 uv) {
  if (mod(post.texelFlags.z,2.0) < 0.5 || post.bloom.y <= 0.0) return vec3(0.0);
  // 1024: a cadeia já espalhou o brilho por cinco oitavas; cada nível somou
  // a subida do seguinte, por isso a média divide pelo número de níveis.
  if (postFlag(1024.0))
    return sampleBloomChain(uv)*environment.postBloomTint.rgb*(post.bloom.y/kBloomLevels);
  // Scatter: o anel de 8 amostras abre de 2 texels (0, o bloom de antes) a 8.
  vec2 px = post.texelFlags.xy * 2.0 * exp2(2.0*clamp(environment.postBloomTint.w,0.0,1.0));
  vec3 sum = vec3(0.0);
  const vec2 offsets[8] = vec2[8](
    vec2(-1,-1),vec2(0,-1),vec2(1,-1),vec2(-1,0),
    vec2(1,0),vec2(-1,1),vec2(0,1),vec2(1,1));
  for (int index = 0; index < 8; ++index) {
    vec3 value = min(sampleScene(uv + offsets[index] * px),vec3(environment.postBloomDetail.x));
    sum += value * smoothstep(post.bloom.x, post.bloom.x + 0.25, luma(value));
  }
  return sum * environment.postBloomTint.rgb * (post.bloom.y / 8.0);
}

vec3 linearToSrgb(vec3 color) {
  return mix(12.92 * color, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec3(0.0031308)));
}

#include "tone_mapping.glsl"
// Giro de matiz em HSV linear, como o Hue Shift do LutBuilderHdr da Unity.
vec3 rgbToHsv(vec3 c) {
  vec4 k=vec4(0.0,-1.0/3.0,2.0/3.0,-1.0);
  vec4 p=mix(vec4(c.bg,k.wz),vec4(c.gb,k.xy),step(c.b,c.g));
  vec4 q=mix(vec4(p.xyw,c.r),vec4(c.r,p.yzx),step(p.x,c.r));
  float d=q.x-min(q.w,q.y);
  return vec3(abs(q.z+(q.w-q.y)/(6.0*d+1.0e-4)),d/(q.x+1.0e-4),q.x);
}
vec3 hsvToRgb(vec3 c) {
  vec3 p=abs(fract(c.xxx+vec3(1.0,2.0/3.0,1.0/3.0))*6.0-3.0);
  return c.z*mix(vec3(1.0),clamp(p-1.0,0.0,1.0),c.y);
}
vec3 toneMap(vec3 color) {
  float adaptation=automaticExposure.valid!=0u?automaticExposure.ev:0.0;
  color=max(color*environment.parameters.x*exp2(environment.scenePost.x+adaptation),vec3(0.0));
  // Color Adjustments antes da curva, na ordem da Unity: filtro, depois matiz.
  color*=environment.postColorAdjust.rgb;
  if(abs(environment.postColorAdjust.w)>0.01) {
    vec3 hsv=rgbToHsv(color);
    hsv.x=fract(hsv.x+environment.postColorAdjust.w/360.0);
    color=hsvToRgb(hsv);
  }
  return astraToneMap(color,environment.scenePost.y);
}

// Vignette da URP (UberPost, ApplyVignette): distância ao centro vezes
// intensidade*3, eixo x pela proporção quando arredondada, queda pela
// suavidade*5 e a cor nas bordas. Lê o uv local distorcido pela lente.
vec3 applyVignette(vec3 color) {
  if(post.bloom.w>0.0) {
    vec2 distance=abs(gLocalUv-environment.postVignetteShape.xy)*environment.postVignetteShape.z;
    distance.x*=environment.postVignette.w;
    float factor=pow(clamp(1.0-dot(distance,distance),0.0,1.0),environment.postVignetteShape.w);
    color*=mix(environment.postVignette.rgb,vec3(1.0),factor);
  }
  return max(color,vec3(0));
}

vec3 displayLinearColor(vec3 color) {
  if(environment.scenePost.z>0.5) color=toneMap(color);
  // Com a LUT (bit 65536), saturação, contraste e vinheta vêm depois da
  // resolução temporal, em applyGradingLut, na mesma ordem: o histórico e a
  // vizinhança do TAA ficam no mesmo espaço sem nove leituras 3D.
  if(postFlag(65536.0)) return max(color,vec3(0));
  float gray=luma(color);
  color=mix(vec3(gray),color,post.grade.y);
  color=(color-0.5)*post.grade.x+0.5;
  return applyVignette(color);
}

// Uma leitura 3D por pixel. sqrt dá às sombras a resolução que um índice linear
// de 32 passos não daria; meia célula de borda alinha com os centros.
vec3 applyGradingLut(vec3 color) {
  if(!postFlag(65536.0)) return color;
  vec3 coordinate=sqrt(clamp(color,vec3(0.0),vec3(1.0)))*(31.0/32.0)+0.5/32.0;
  return applyVignette(textureLod(gradingLut,coordinate,0.0).rgb);
}

#if AETHER_TEMPORAL
#include "temporal_clipping.glsl"
vec3 srgbToLinear(vec3 color) {
  return mix(color/12.92,pow((max(color,vec3(0))+.055)/1.055,vec3(2.4)),
             greaterThan(color,vec3(.04045)));
}
mat3 cameraRotation(float yaw, float pitch, float roll) {
  float cy=cos(yaw), sy=sin(yaw), cp=cos(pitch), sp=sin(pitch);
  float cr=cos(roll), sr=sin(roll);
  return mat3(cy,0,-sy, 0,1,0, sy,0,cy) *
         mat3(1,0,0, 0,cp,sp, 0,-sp,cp) *
         mat3(cr,sr,0, -sr,cr,0, 0,0,1);
}

vec4 unpackSurfaceTransform(int packed) {
  return vec4(float((packed >> 0) & 3) - 1.0,
              float((packed >> 2) & 3) - 1.0,
              float((packed >> 4) & 3) - 1.0,
              float((packed >> 6) & 3) - 1.0);
}

// Alpha in the resolved-history attachment stores logarithmic view depth.
// The presentation attachment keeps opaque alpha; only temporal reprojection
// reads this value. Logarithmic encoding gives the UNORM8 history useful
// precision across both nearby geometry and distant scenery.
float temporalEncodeDepth(float viewZ, float nearPlane, float farPlane) {
  return clamp(log2(max(viewZ / nearPlane, 1.0)) /
                   log2(max(farPlane / nearPlane, 1.0001)), 0.0, 1.0);
}

vec3 temporalResolve(vec2 sourceUv, vec3 current, float ao, vec3 bloomColor,
                     out float encodedDepth, out float historyContribution,
                     out float rejectedDepth, out vec2 motionUv) {
  // Mode 1 is spatial AA when history is invalid (first frame/cut/moving draw),
  // mode 3 has a valid copied resolve. Invalid frames are rendered without
  // jitter and seed the next history; they never sample uninitialized color.
  float depth = texture(sceneDepth, sourceUv).r;
  float nearPlane = post.currentPositionNear.w;
  float farPlane = post.previousPositionFar.w;
  float denominator = farPlane - depth * (farPlane - nearPlane);
  encodedDepth = depth >= 0.999999 ? 1.0 :
      temporalEncodeDepth(nearPlane * farPlane / max(denominator, 1.0e-6),
                          nearPlane, farPlane);
  historyContribution=0.0;
  rejectedDepth=0.0;
  motionUv=vec2(0.0);
  if (mod(post.texelFlags.w,4.0) < 2.5) return current;
  if (denominator <= 1.0e-6) return current;
  float viewZ = nearPlane * farPlane / denominator;

  int packed = int(floor(post.grade.w));
  vec4 surface = unpackSurfaceTransform(packed);
  // PK-5: com distorção de lente o pixel mostra o ponto de gResolveUv; a
  // reprojeção parte dele e o deslocamento volta para este pixel no fim.
  vec2 baseUv = gResolveUv;
  vec2 physicalNdc = (baseUv-environment.postViewport.xy)/environment.postViewport.zw *
      2.0 - 1.0 - post.currentCamera.zw;
  vec2 cameraNdc = vec2(dot(surface.xz, physicalNdc),
                        dot(surface.yw, physicalNdc));
  float focal=post.sourceTransform.z;
  vec3 viewPosition = vec3(cameraNdc.x * post.sourceTransform.w * viewZ / focal,
                           -cameraNdc.y * viewZ / focal, viewZ);
  vec3 worldPosition = post.currentPositionNear.xyz +
                       cameraRotation(post.currentCamera.x, post.currentCamera.y, post.previousCamera.z) * viewPosition;
  vec3 previousView = transpose(cameraRotation(post.previousCamera.x,
                                                 post.previousCamera.y, post.previousCamera.w)) *
                      (worldPosition - post.previousPositionFar.xyz);
  if(depth>=0.999999) {
    // The infinite sky responds to rotation only, never camera translation.
    previousView=transpose(cameraRotation(post.previousCamera.x,post.previousCamera.y,post.previousCamera.w))*
        cameraRotation(post.currentCamera.x,post.currentCamera.y,post.previousCamera.z)*viewPosition;
  }
  if (previousView.z <= nearPlane) return current;
  vec2 previousCameraNdc = vec2(previousView.x * focal /
                                    (post.sourceTransform.w * previousView.z),
                                -previousView.y * focal / previousView.z);
  vec2 previousPhysicalNdc = vec2(dot(surface.xy, previousCameraNdc),
                                  dot(surface.zw, previousCameraNdc)) +
                             post.currentCamera.zw;
  // History stores resolved output on a stable pixel grid. Reprojection is
  // previousStable-currentStable, anchored at this output pixel: add CURRENT
  // jitter to cancel its subtraction above. Previous jitter makes a static
  // camera oscillate by jPrevious-jCurrent on every Halton sample.
  vec2 previousLocalUv = previousPhysicalNdc * 0.5 + 0.5;
  if (any(lessThan(previousLocalUv, vec2(0.0))) ||
      any(greaterThan(previousLocalUv, vec2(1.0)))) return current;
  vec2 previousUv=environment.postViewport.xy+previousLocalUv*environment.postViewport.zw;
  // O vetor unificado manda: câmera e objetos saem do mesmo passe que as
  // bibliotecas consomem. A reprojeção só de câmera acima continua dando a
  // profundidade esperada; onde o vetor diverge dela, o pixel pertence a um
  // objeto em movimento, cuja profundidade anterior a câmera não descreve.
  bool objectMotion=false;
  if(postFlag(8.0)) {
    vec2 velocityUv=baseUv+texture(temporalVelocity,sourceUv).xy;
    objectMotion=length((velocityUv-previousUv)/max(post.texelFlags.xy,vec2(1.0e-6)))>0.5;
    if(any(lessThan(velocityUv,environment.postViewport.xy))||
       any(greaterThan(velocityUv,environment.postViewport.xy+environment.postViewport.zw))) return current;
    previousUv=velocityUv;
  }
  previousUv+=vUv-baseUv;
  motionUv=previousUv-vUv;

  // FSR runs this resolve at the current internal extent, in the top-left of
  // the full-size history allocation. The rest of the image is never read.
  vec2 historyScale=mod(floor(post.texelFlags.z/4.0),2.0)>0.5
      ? post.sourceTransform.xy/post.texelFlags.xy/vec2(textureSize(historyColor,0))
      : vec2(1.0);
  previousUv*=historyScale;
  vec2 historyHalfTexel=0.5/vec2(textureSize(historyColor,0));
  previousUv=clamp(previousUv,environment.postViewport.xy*historyScale+historyHalfTexel,
      (environment.postViewport.xy+environment.postViewport.zw)*historyScale-historyHalfTexel);
  // A color match is insufficient at disocclusions: a newly visible surface
  // may have nearly the same color as the old occluder. Reject its history
  // when the old pixel's depth differs from the reprojected surface depth.
  // Fetch alpha without bilinear filtering across a depth edge.
  ivec2 historyExtent=textureSize(historyColor,0);
  ivec2 historyPixel=clamp(ivec2(previousUv*vec2(historyExtent)),
                          ivec2(0),historyExtent-ivec2(1));
  float previousDepth=texelFetch(historyColor,historyPixel,0).a;
  float expectedDepth=depth>=0.999999 ? 1.0 :
      temporalEncodeDepth(previousView.z,nearPlane,farPlane);
  // ~0.02 in log depth tolerates 8-bit quantization and raster edge samples
  // while rejecting a different surface at materially different depth.
  // A stationary depth edge alternates foreground/background under projection
  // jitter. There is no newly exposed surface when camera and depth-writing
  // geometry are unchanged, so this mismatch must not discard its history.
  bool staticCoverage=mod(floor(post.texelFlags.z/16.0),2.0)>0.5;
  if(!staticCoverage && !objectMotion && abs(previousDepth-expectedDepth)>0.02) {
    rejectedDepth=1.0;
    return current;
  }
  vec3 history = texture(historyColor, previousUv).rgb;
  // An sRGB history view decodes in hardware; UNORM contains explicitly
  // encoded presentation bytes. Resolve and clip in display-linear color
  // in both cases, before the final output transfer function.
  if(post.grade.z>0.5) history=srgbToLinear(history);
  vec3 currentY=temporalYCoCg(current);
  vec3 lower=currentY,upper=currentY;
  for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
    vec3 value=displayLinearColor(sampleScene(sourceUv+vec2(x,y)*post.texelFlags.xy)*ao+bloomColor);
    vec3 converted=temporalYCoCg(value);
    lower=min(lower,converted);upper=max(upper,converted);
  }
  history=temporalRgb(temporalClipAabb(lower,upper,temporalYCoCg(history)));
  float historyWeight = fract(post.grade.w);
  float motionPixels = length((previousUv-vUv*historyScale)*vec2(textureSize(historyColor,0)));
  historyWeight *= exp2(-motionPixels * 0.035);
  // Transparentes e água mudam de cor sem escrever profundidade nem vetor.
  // As máscaras do passe principal dizem quanto; sem elas, a cobertura opaca
  // restante em sceneColor.a é a aproximação anterior.
  float reactive=postFlag(32.0)
      ? max(texture(reactiveMask,sourceUv).r,0.5*texture(compositionMask,sourceUv).r)
      : clamp(1.0-texture(sceneColor,sourceUv).a,0.0,1.0);
  historyWeight *= 1.0-reactive;
  historyWeight *= 1.0 - smoothstep(0.08, 0.35, abs(luma(history) - luma(current)));
  historyContribution=clamp(historyWeight, 0.0, 0.97);
  return mix(current, history, historyContribution);
}
#endif

// --- PK-5: Motion Blur, Chromatic Aberration e Depth of Field -------------
mat3 cameraBasis(float yaw, float pitch, float roll) {
  float cy=cos(yaw), sy=sin(yaw), cp=cos(pitch), sp=sin(pitch);
  float cr=cos(roll), sr=sin(roll);
  return mat3(cy,0,-sy, 0,1,0, sy,0,cy) *
         mat3(1,0,0, 0,cp,sp, 0,-sp,cp) *
         mat3(cr,sr,0, -sr,cr,0, 0,0,1);
}
// GetCameraVelocity do CameraMotionBlur da URP: reprojeta o ponto da
// profundidade pela câmera anterior e devolve o deslocamento em NDC local
// (anterior - atual), limitado ao Clamp. Só a câmera (Camera Only).
vec2 cameraMotionNdc(vec2 localUv,vec2 sourceUv) {
  float focal=post.sourceTransform.z;
  if(focal<=0.0) return vec2(0.0);
  float rawDepth=effectRawDepth(sourceUv);
  float viewZ=viewDistanceFromDepth(rawDepth);
  int packed=int(floor(post.grade.w));
  vec4 surface=vec4(float((packed>>0)&3)-1.0,float((packed>>2)&3)-1.0,
                    float((packed>>4)&3)-1.0,float((packed>>6)&3)-1.0);
  vec2 physicalNdc=localUv*2.0-1.0-post.currentCamera.zw;
  vec2 cameraNdc=vec2(dot(surface.xz,physicalNdc),dot(surface.yw,physicalNdc));
  vec3 viewPosition=vec3(cameraNdc.x*post.sourceTransform.w*viewZ/focal,-cameraNdc.y*viewZ/focal,viewZ);
  mat3 current=cameraBasis(post.currentCamera.x,post.currentCamera.y,post.previousCamera.z);
  mat3 previousBasis=cameraBasis(post.previousCamera.x,post.previousCamera.y,post.previousCamera.w);
  vec3 previousView;
  if(rawDepth>=0.999999) previousView=transpose(previousBasis)*(current*viewPosition);
  else previousView=transpose(previousBasis)*(post.currentPositionNear.xyz+current*viewPosition-post.previousPositionFar.xyz);
  if(previousView.z<=post.currentPositionNear.w) return vec2(0.0);
  vec2 previousCameraNdc=vec2(previousView.x*focal/(post.sourceTransform.w*previousView.z),-previousView.y*focal/previousView.z);
  vec2 previousPhysicalNdc=vec2(dot(surface.xy,previousCameraNdc),dot(surface.zw,previousCameraNdc))+post.currentCamera.zw;
  vec2 velocity=(previousPhysicalNdc-(physicalNdc+post.currentCamera.zw))*environment.postMotion.x;
  float len=length(velocity);
  return len>0.0?velocity*(min(len,environment.postMotion.y)/len):vec2(0.0);
}
float interleavedGradientNoise(vec2 pixel) {
  return fract(52.9829189*fract(dot(pixel,vec2(0.06711056,0.00583715))));
}
// DoMotionBlur da URP: 2 leituras por iteração, com o deslocamento sorteado
// por pixel; Low/Medium/High = 2/3/4 iterações.
vec3 applyMotionBlur(vec3 center,vec2 localUv,vec2 sourceUv) {
  if(environment.postMotion.x<=0.0) return center;
  vec2 velocity=cameraMotionNdc(localUv,sourceUv);
  if(dot(velocity,velocity)<1.0e-10) return center;
  // Mesmo uso da URP: o deslocamento em NDC soma direto ao uv.
  vec2 step=velocity*environment.postViewport.zw*post.sourceTransform.xy;
  float randomValue=interleavedGradientNoise(gl_FragCoord.xy);
  int iterations=int(environment.postMotion.z+0.5);
  float inverseCount=1.0/float(iterations*2);
  vec3 sum=vec3(0.0);
  for(int i=0;i<4;++i) {
    if(i>=iterations) break;
    float offsetNegative=(float(i)+0.5)-(randomValue-0.5);
    float offsetPositive=(float(i)+0.5)+(randomValue-0.5);
    sum+=sampleScene(sourceUv-offsetNegative*inverseCount*step);
    sum+=sampleScene(sourceUv+offsetPositive*inverseCount*step);
  }
  return sum*inverseCount;
}
// Chromatic Aberration da URP (as 3 leituras do UberPost): verde e azul
// deslocados para as bordas proporcionalmente ao quadrado da distância.
vec3 applyChromaticAberration(vec3 center,vec2 localUv) {
  float amount=environment.postChroma.x;
  if(amount<=0.0) return center;
  vec2 coords=2.0*localUv-1.0;
  vec2 delta=(-coords*dot(coords,coords)*amount)/3.0;
  vec2 green=outputFromLocal(distortUv(localUv+delta))*post.sourceTransform.xy;
  vec2 blue=outputFromLocal(distortUv(localUv+2.0*delta))*post.sourceTransform.xy;
  return vec3(center.r,sampleScene(clampSourceUv(green)).g,sampleScene(clampSourceUv(blue)).b);
}
// CoC em fração da altura da tela. Gaussiano (URP): só o fundo, 0 a 1 entre
// Início e Fim. Bokeh (PPv2/URP): (d - foco) * coeficiente / d, sinal negativo
// na frente do foco.
float circleOfConfusion(float viewZ) {
  if(environment.postDof.x<1.5) return clamp((viewZ-environment.postDof.y)/max(environment.postDof.z-environment.postDof.y,1.0e-4),0.0,1.0);
  // URP BokehDepthOfField (FragCoC): (1 - foco/d) * maxCoC, limitado a ±1 e
  // escalado pelo raio máximo do bokeh.
  float coc=(viewZ-environment.postDof.y)*environment.postDof.z/max(viewZ,1.0e-4);
  return clamp(coc,-1.0,1.0)*environment.postDof.w;
}
// Composição do DoF sobre a cena (FragComposition do PPv2 no Bokeh; mistura
// pelo CoC no Gaussiano, com leitura bicúbica opcional).
vec3 applyDepthOfField(vec3 color,vec2 sourceUv,vec2 halfUv) {
  if(!postFlag(2097152.0)) return color;
  float coc=circleOfConfusion(viewDistanceFromDepth(effectRawDepth(sourceUv)));
  if(environment.postDof.x<1.5) {
    // High Quality Sampling: a leitura bicúbica tira o serrilhado da meia resolução.
    vec3 blurred=environment.postDofDetail.z>0.5?sampleHalfBicubic(depthOfFieldResult,halfUv).rgb:
        texture(depthOfFieldResult,clampHalfUv(halfUv,vec2(textureSize(depthOfFieldResult,0)))).rgb;
    return mix(color,blurred,coc);
  }
  vec4 dof=texture(depthOfFieldResult,clampHalfUv(halfUv,vec2(textureSize(depthOfFieldResult,0))));
  float texel=1.0/max(float(textureSize(sceneColor,0).y),1.0);
  float farAlpha=smoothstep(texel*2.0,texel*4.0,coc);
  return mix(color,dof.rgb,farAlpha+dof.a-farAlpha*dof.a);
}

float filmGrainNoise(vec2 pixel) {
  // A câmera e o jitter variam a semente sem exigir outro campo no ABI de
  // push constants. Sem TAA o padrão permanece estável quando a vista para,
  // evitando cintilação gratuita em aparelhos de atualização baixa.
  vec2 seed=post.currentCamera.xy*37.0+post.currentCamera.zw*8192.0;
  return fract(sin(dot(pixel+seed,vec2(12.9898,78.233)))*43758.5453);
}

// Diagnóstico temporal (G6-B): mostra o valor que o consumidor lê, não uma
// recomputação. Vale no TAA nativo e depois dos ampliadores temporais.
bool temporalDiagnostic(vec2 sourceUv, float encodedDepth, float historyContribution,
                        float rejectedDepth, out vec3 diagnostic) {
  int view=int(floor(post.texelFlags.w*0.25));
  if(view==0) return false;
  vec2 inputUv=temporalInputUv(vUv);
  vec2 texel=vec2(textureSize(sceneDepth,0));
  if(view==1) {
    float depth=texture(sceneDepth,inputUv).r;
    float nearPlane=post.currentPositionNear.w,farPlane=post.previousPositionFar.w;
    float viewZ=nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
    diagnostic=vec3(depth>=0.999999?1.0:clamp(log2(max(viewZ/nearPlane,1.0))/
                                                   log2(max(farPlane/nearPlane,1.0001)),0.0,1.0));
  } else if(view==2) diagnostic=vec3(0,historyContribution,0);
  else if(view==3) diagnostic=vec3(rejectedDepth,0,0);
  else if(view==4) {
    // Pixels de movimento em cor: 16 px por quadro satura. Azul fixo separa
    // "parado" (cinza-azulado) de ausência de dados.
    vec2 pixels=texture(temporalVelocity,inputUv).xy*texel/max(environment.temporalParameters.zw,vec2(1.0e-6));
    diagnostic=vec3(clamp(vec2(0.5)+pixels/32.0,0.0,1.0),0.5);
  } else if(view==5) {
    float reactive=postFlag(32.0)?texture(reactiveMask,inputUv).r:
        clamp(1.0-texture(sceneColor,sourceUv).a,0.0,1.0);
    diagnostic=vec3(reactive,0,reactive);
  } else diagnostic=vec3(0,postFlag(32.0)?texture(compositionMask,inputUv).r:0.0,
                         postFlag(32.0)?texture(compositionMask,inputUv).r:0.0);
  return true;
}

void main() {
  if(postFlag(2048.0)) {
    // AO em 1/2: mesma integração, um quarto dos pixels. Os texels da
    // profundidade continuam os de resolução cheia (post.texelFlags).
    outColor=vec4(screenSpaceAmbientOcclusion(clampSourceUv(vUv*post.sourceTransform.xy)),0.0,0.0,1.0);
    return;
  }
  if(postFlag(8192.0)) {
    // Bloom, descida (filtro duplo de Kawase, ARM 2015): 4 diagonais a um
    // texel da fonte, cada uma média bilinear 2x2, mais o centro com peso 4.
    // 16384 é o pré-filtro: limiar suave e média de Karis contra vagalumes.
    vec2 uv=clampSourceUv(vUv*post.sourceTransform.xy);
    vec2 texel=post.texelFlags.xy;
    vec3 taps[5]=vec3[5](texture(sceneColor,uv).rgb,
        texture(sceneColor,clampSourceUv(uv+vec2(-texel.x,-texel.y))).rgb,
        texture(sceneColor,clampSourceUv(uv+vec2( texel.x,-texel.y))).rgb,
        texture(sceneColor,clampSourceUv(uv+vec2(-texel.x, texel.y))).rgb,
        texture(sceneColor,clampSourceUv(uv+vec2( texel.x, texel.y))).rgb);
    const float weights[5]=float[5](4.0,1.0,1.0,1.0,1.0);
    vec3 sum=vec3(0.0);float total=0.0;
    bool prefilter=postFlag(16384.0);
    for(int index=0;index<5;++index) {
      vec3 value=taps[index];float weight=weights[index];
      if(prefilter) {
        value=min(value,vec3(environment.postBloomDetail.x));
        float brightness=luma(value);
        value*=smoothstep(post.bloom.x,post.bloom.x+0.25,brightness);
        weight/=1.0+luma(value);
      }
      sum+=value*weight;total+=weight;
    }
    outColor=vec4(sum/max(total,1.0e-6),1.0);
    return;
  }
  if(postFlag(32768.0)) {
    // Bloom, subida: tenda de 8 amostras sobre o nível de baixo, somada ao
    // nível atual pela mistura aditiva da pipeline.
    vec2 uv=clampSourceUv(vUv*post.sourceTransform.xy);
    vec2 half_=post.texelFlags.xy*0.5;
    vec3 sum=texture(sceneColor,clampSourceUv(uv+vec2(-half_.x*2.0,0.0))).rgb+
             texture(sceneColor,clampSourceUv(uv+vec2( half_.x*2.0,0.0))).rgb+
             texture(sceneColor,clampSourceUv(uv+vec2(0.0,-half_.y*2.0))).rgb+
             texture(sceneColor,clampSourceUv(uv+vec2(0.0, half_.y*2.0))).rgb+
             2.0*(texture(sceneColor,clampSourceUv(uv+vec2(-half_.x, half_.y))).rgb+
                  texture(sceneColor,clampSourceUv(uv+vec2( half_.x, half_.y))).rgb+
                  texture(sceneColor,clampSourceUv(uv+vec2( half_.x,-half_.y))).rgb+
                  texture(sceneColor,clampSourceUv(uv+vec2(-half_.x,-half_.y))).rgb);
    outColor=vec4(sum/12.0,1.0);
    return;
  }
  if(postFlag(131072.0)) {
    // DoF, pré-filtro em 1/2 (FragPrefilter do PPv2): quatro leituras da cena
    // com o CoC de cada uma, pesadas para não vazar o foco nem cintilar.
    vec2 uv=clampSourceUv(vUv*post.sourceTransform.xy);
    vec2 texel=post.texelFlags.xy*0.5;
    vec2 offsets[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));
    vec3 sum=vec3(0.0);float total=0.0;float cocMin=1.0e9,cocMax=-1.0e9;
    for(int index=0;index<4;++index) {
      vec2 tap=clampSourceUv(uv+offsets[index]*texel);
      vec3 value=sampleScene(tap);
      float coc=circleOfConfusion(viewDistanceFromDepth(texture(sceneDepth,tap).r));
      float weight=(environment.postDof.x<1.5?1.0:abs(coc))/(max(value.r,max(value.g,value.b))+1.0);
      sum+=value*weight;total+=weight;
      cocMin=min(cocMin,coc);cocMax=max(cocMax,coc);
    }
    vec3 average=sum/max(total,1.0e-4);
    float coc=-cocMin>cocMax?cocMin:cocMax;
    if(environment.postDof.x>1.5) average*=smoothstep(0.0,post.texelFlags.y*2.0,abs(coc));
    outColor=vec4(average,coc);
    return;
  }
  if(postFlag(262144.0)) {
    // DoF Gaussiano (URP GaussianDepthOfField, Blur): 9 pesos em 5 leituras
    // bilineares na direção do passe, raio = CoC do centro * raio máximo.
    vec2 uv=vUv*post.sourceTransform.xy;
    vec4 center=texture(sceneColor,uv);
    vec2 offset=post.texelFlags.xy*post.bloom.xy*center.a*environment.postDof.w;
    const float offsets[5]=float[5](-3.23076923,-1.38461538,0.0,1.38461538,3.23076923);
    const float weights[5]=float[5](0.07027027,0.31621622,0.22702703,0.31621622,0.07027027);
    vec4 sum=vec4(0.0);
    for(int index=0;index<5;++index) {
      vec4 tap=texture(sceneColor,uv+offsets[index]*offset);
      float weight=weights[index]*clamp(1.0-(center.a-tap.a),0.0,1.0);
      sum+=vec4(tap.rgb,1.0)*weight;
    }
    outColor=vec4(sum.rgb/max(sum.a,1.0e-4),center.a);
    return;
  }
  if(postFlag(524288.0)) {
    // DoF Bokeh (FragBlur do PPv2 com o kernel de lâminas da URP): fundo pelo
    // menor CoC (centro ou amostra), frente pelo CoC da amostra; alfa = frente.
    vec2 uv=vUv*post.sourceTransform.xy;
    vec4 center=texture(sceneColor,uv);
    vec4 background=vec4(0.0),foreground=vec4(0.0);
    float margin=post.texelFlags.y*2.0;
    int count=int(environment.postDofDetail.x+0.5);
    for(int index=0;index<44;++index) {
      if(index>=count) break;
      vec4 pair=environment.postBokeh[index>>1];
      vec2 kernel=(index&1)==0?pair.xy:pair.zw;
      vec2 displacement=kernel*environment.postDof.w;
      float distance=length(displacement);
      vec4 tap=texture(sceneColor,uv+vec2(displacement.x*environment.postDofDetail.y,displacement.y)*post.sourceTransform.xy);
      float backgroundCoc=max(min(center.a,tap.a),0.0);
      float backgroundWeight=clamp((backgroundCoc-distance+margin)/margin,0.0,1.0);
      float foregroundWeight=clamp((-tap.a-distance+margin)/margin,0.0,1.0);
      foregroundWeight*=step(post.texelFlags.y,-tap.a);
      background+=vec4(tap.rgb,1.0)*backgroundWeight;
      foreground+=vec4(tap.rgb,1.0)*foregroundWeight;
    }
    background.rgb/=background.a+(background.a==0.0?1.0:0.0);
    foreground.rgb/=foreground.a+(foreground.a==0.0?1.0:0.0);
    float alpha=clamp(foreground.a*3.14159265/max(float(count),1.0),0.0,1.0);
    outColor=vec4(mix(background.rgb,foreground.rgb,alpha),alpha);
    return;
  }
  if(postFlag(1048576.0)) {
    // DoF Bokeh, pós-filtro: tenda de 9 pesos em 4 leituras bilineares.
    vec2 uv=vUv*post.sourceTransform.xy;
    vec4 duv=vec4(post.texelFlags.xy,-post.texelFlags.x,0.0)*0.5;
    outColor=(texture(sceneColor,uv-duv.xy)+texture(sceneColor,uv-duv.zy)+
              texture(sceneColor,uv+duv.zy)+texture(sceneColor,uv+duv.xy))*0.25;
    return;
  }
  if(postFlag(256.0)) {
    // Pré-passe de névoa: avaliada uma vez por texel na resolução interna.
    // Sem ele, FXAA, bloom e reconstrução repetem profundidade + log/exp em
    // cada amostra (até ~22 por pixel); o passe principal lê esta cor com 512.
    vec2 sourceUv=clampSourceUv(vUv*post.sourceTransform.xy);
    outColor=vec4(sampleScene(sourceUv),1.0);
    return;
  }
  if(postFlag(64.0)) {
    // Composição antes do ampliador temporal: névoa e AO em HDR linear, na
    // resolução interna, alinhadas à profundidade que as produziu. Tonemap,
    // bloom, grade e vinheta ficam para depois da ampliação.
    vec2 sourceUv=clampSourceUv(vUv*post.sourceTransform.xy);
    outColor=vec4(sampleScene(sourceUv)*screenSpaceAmbientOcclusion(sourceUv),1.0);
    return;
  }
  if(any(lessThan(vUv,environment.postViewport.xy)) ||
     any(greaterThan(vUv,environment.postViewport.xy+environment.postViewport.zw))) {
    // UI is composited afterwards. Preserve the clear outside the camera;
    // neither reconstruction nor history may spread its edge across panels.
    vec3 background=texture(sceneColor,vUv*post.sourceTransform.xy).rgb;
    outColor=vec4(post.grade.z>.5?linearToSrgb(background):background,1);
#if AETHER_TEMPORAL
    outHistory=outColor;
#endif
    return;
  }
  // PK-5: a lente distorce o uv de toda leitura da cena (URP uvDistorted).
  gLocalUv=distortUv(localFromOutput(vUv));
  gResolveUv=outputFromLocal(gLocalUv);
  vec2 sourceUv = clampSourceUv(gResolveUv * post.sourceTransform.xy);
  vec3 unfiltered;
  vec3 crossAverage;
  vec3 center = sampleFxaa(sourceUv, unfiltered, crossAverage);
  if(mod(floor(post.texelFlags.z/2.0),2.0)>0.5) {
    // Preserve the chosen AA correction and reconstruct only the centre,
    // rather than multiplying every FXAA/bloom neighbourhood by nine taps.
    center=max(vec3(0),center+reconstructScene(sourceUv)-unfiltered);
  }
  center=applyChromaticAberration(center,localFromOutput(vUv));
  center=applyDepthOfField(center,sourceUv,gResolveUv*post.sourceTransform.xy);
  center=applyMotionBlur(center,gLocalUv,sourceUv);
  float ao=postFlag(4096.0)?upsampledAmbientOcclusion(sourceUv):screenSpaceAmbientOcclusion(sourceUv);
  center *= ao;
  vec3 bloomColor=bloomNeighborhood(sourceUv);
  // Lens Dirt: a sujeira acende onde o bloom passa, como na URP.
  if(environment.postBloomDetail.y>0.0)
    bloomColor+=texture(bloomDirt,gLocalUv*environment.postBloomDirt.xy+environment.postBloomDirt.zw).rgb*
                environment.postBloomDetail.y*bloomColor;
  vec3 color = center + bloomColor;
  color=displayLinearColor(color);
#if AETHER_TEMPORAL
  float encodedDepth;
  float historyContribution,rejectedDepth;
  vec2 motionUv;
  color = temporalResolve(sourceUv, color, ao, bloomColor,
                          encodedDepth, historyContribution, rejectedDepth,motionUv);
  outHistory=vec4(post.grade.z>.5?linearToSrgb(color):color,encodedDepth);
  // Editor diagnostics inspect the actual consumer values. The normal color
  // and depth still go into history, so changing view never poisons a frame.
  vec3 diagnostic;
  if(temporalDiagnostic(sourceUv,encodedDepth,historyContribution,rejectedDepth,diagnostic)) {
    outColor=vec4(diagnostic,1);
    return;
  }
#else
  vec3 diagnostic;
  if(temporalDiagnostic(sourceUv,0.0,0.0,0.0,diagnostic)) {
    outColor=vec4(post.grade.z>.5?linearToSrgb(diagnostic):diagnostic,1);
    return;
  }
#endif
  // Presentation detail never feeds back into temporal reconstruction.
  if (post.bloom.z > 0.0)
    color += (displayLinearColor(unfiltered*ao+bloomColor)-
              displayLinearColor(crossAverage*ao+bloomColor))*post.bloom.z;
  color=applyGradingLut(color);
  float grainIntensity=environment.sceneAoDetail.y;
  if(grainIntensity>0.0 && mod(floor(post.texelFlags.z/4.0),2.0)<0.5) {
    float response=0.35+0.65*(1.0-abs(luma(color)*2.0-1.0));
    color+=(filmGrainNoise(gl_FragCoord.xy)-0.5)*grainIntensity*response;
  }
  if (post.grade.z > 0.5) color = linearToSrgb(max(color,vec3(0)));
  outColor = vec4(max(color, vec3(0.0)), 1.0);
}
