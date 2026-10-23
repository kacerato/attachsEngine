#version 450
#extension GL_GOOGLE_include_directive : require
// G4d: Volumetric Clouds (HDRP 17), no modelo de Schneider, "The Real-time
// Volumetric Cloudscapes of Horizon Zero Dawn" (SIGGRAPH 2015), base das
// nuvens do HDRP:
//  * casca esférica entre a Altitude e a Altitude + Espessura (raio da Terra);
//  * cobertura do mapa 2D (o mesmo do Cloud Layer e da sombra no chão);
//  * forma por ruído 3D Perlin-Worley + Worley e erosão por Worley em
//    frequência alta; perfil vertical de cúmulo (base reta, topo arredondado);
//  * luz do sol por marcha curta rumo ao sol, Beer + Powder e fase
//    Henyey-Greenstein de dois lóbulos; ambiente por altura;
//  * névoa do horizonte pela distância.
// Roda em 1/4 da resolução em cada eixo (1/16 dos pixels) e acumula no tempo
// com reprojeção pela rotação da câmera, como o modo de desempenho do HDRP.
// Saída pré-multiplicada: rgb = luz da nuvem, a = cobertura; o céu compõe.
#include "dirt_road_frame.glsl"
const float PI=3.141592653589793;
layout(set=0,binding=0,std140) uniform CloudFrameBlock {
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
  layout(offset=3248) vec4 atmosphereScattering;
  layout(offset=3312) vec4 atmosphereGroundIrradiance;
  layout(offset=3328) vec4 temporalCurrentView[3];
  layout(offset=3376) vec4 temporalPreviousView[3];
  layout(offset=3424) vec4 temporalProjection;
  layout(offset=3456) vec4 temporalSurface;
  layout(offset=3472) vec4 temporalParameters;
  layout(offset=3536) vec4 cloudParameters; // opacidade, cobertura, cirros, densidade
  layout(offset=3552) vec4 cloudShape;      // 1/tamanho, altitude, vento x, z
  layout(offset=3568) vec4 cloudTint;       // tom*2^EV, luz do céu
  layout(offset=3584) vec4 cloudShadow;     // sombra, espessura, tipo, passos
  layout(offset=4256) vec4 cloudVolume;     // fator de forma, erosão, histórico válido
} environment;
layout(set=0,binding=18) uniform mediump sampler2D cloudMap;
layout(set=0,binding=20) uniform mediump sampler2D cloudHistory;
layout(set=0,binding=21) uniform mediump sampler3D cloudShapeNoise;
layout(set=0,binding=22) uniform mediump sampler3D cloudErosionNoise;
layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outCloud;
#include "cloud_layer.glsl"

float remapValue(float v,float lo,float hi,float newLo,float newHi) {
  return newLo+(v-lo)/max(hi-lo,1e-5)*(newHi-newLo);
}
float henyeyGreenstein(float cosine,float g) {
  float g2=g*g;
  return (1.0-g2)/(4.0*PI*pow(max(1.0+g2-2.0*g*cosine,1e-4),1.5));
}

// Mancha de cobertura do mapa 2D. O mapa da camada 2D tem ruas ao longo de z;
// a média com uma cópia girada 60° (e noutra escala) deixa as manchas redondas.
float cloudVolumePatch(vec2 weatherUv,float lod) {
  vec2 rotated=vec2(weatherUv.x*0.5-weatherUv.y*0.866,weatherUv.x*0.866+weatherUv.y*0.5)*0.77+vec2(0.31);
  float m=(textureLod(cloudMap,weatherUv,lod).r+textureLod(cloudMap,rotated,lod).r)*0.5;
  return cloudCumulusFromMap(vec3(0.5+(m-0.5)*1.6,0.0,0.5));
}

// Densidade (0..1) num ponto a `p` km do observador, com altura relativa
// `height` na camada. `noiseLod` é o mip dos ruídos 3D pela área do pixel (sem
// mip, o ruído longe vira moiré). `cheap` pula a erosão (marcha de luz).
// Modelo de Schneider (Horizon Zero Dawn), base das Volumetric Clouds do HDRP.
// Perfil de altura e ambiente topo/base portados do UniStorm 5.3.1 (Black
// Horizon Studios, licença company do dono do projeto), arquivo
// Assets/UniStorm Weather System/Resources/Clouds/cloudsInclude.cginc,
// funções cloudMapBase, cloudGradient e o ambiente de renderCloudsInternal;
// adaptado ao ruído 3D e à casca esférica da Astra.
float cloudVolumeDensity(vec3 p,float height,float lod,float noiseLod,bool cheap) {
  float patchValue=cloudVolumePatch(cloudLayerUv(p.xz*1000.0),lod);
  if(patchValue<=0.0) return 0.0;
  // Cobertura local: o ruído 3D recorta cúmulos separados dentro da mancha.
  float coverage=patchValue*(0.12+0.45*environment.cloudParameters.y);
  // UniStorm cloudGradient: sobe de 0 a 5% da camada e cai de 80% a 120%.
  float profile=clamp(height/0.05,0.0,1.0)-clamp((height-0.8)/0.4,0.0,1.0);
  if(profile<=0.0) return 0.0;
  vec3 world=vec3(p.x*1000.0-environment.cloudShape.z,p.y*1000.0,p.z*1000.0-environment.cloudShape.w);
  vec3 q=world*environment.cloudShape.x*2.0;
  vec4 n=textureLod(cloudShapeNoise,q,noiseLod);
  float fbm=n.g*0.625+n.b*0.25+n.a*0.125;
  float shape=clamp(remapValue(n.r,fbm-1.0,1.0,0.0,1.0),0.0,1.0);
  // UniStorm cloudMapBase: corta h² + (1-h)^36 da forma — cúpula arredondada no
  // alto, base plana embaixo — e reestica o que sobra.
  float cut=(height*height+pow(1.0-height,36.0))*0.9;
  shape=clamp(remapValue(shape-cut,0.0,1.0-cut,0.0,1.0),0.0,1.0);
  float base=mix(1.0,shape,environment.cloudVolume.x)*profile;
  float cloud=clamp(remapValue(base,1.0-coverage,1.0,0.0,1.0),0.0,1.0)*coverage;
  if(cloud<=0.0 || cheap) return cloud;
  // Erosão por Worley 6x mais fino: couve-flor no topo, fiapos na base.
  vec3 e3=textureLod(cloudErosionNoise,q*6.0,noiseLod+3.585).rgb;
  float erosion=e3.r*0.625+e3.g*0.25+e3.b*0.125;
  erosion=mix(1.0-erosion,erosion,clamp(height*4.0,0.0,1.0));
  return clamp(remapValue(cloud,erosion*environment.cloudVolume.y*0.5,1.0,0.0,1.0),0.0,1.0);
}

void main() {
  vec3 viewDirection=normalize(vDirection);
  // Saída sem unidade, somada pela opacidade de cada amostra: r = luz do sol
  // (transmitância rumo ao sol x fase), g = altura (topo 1, base 0), b =
  // distância (km), a = cobertura. O céu converte em cor com as mesmas
  // radiâncias da camada 2D.
  // Alvo em meia resolução; cada quadro marcha 1 pixel de cada bloco 2x2 (os
  // outros três vêm do histórico): o custo de 1/16 dos pixels da tela com a
  // nitidez de meia resolução, como o modo de desempenho do HDRP.
  float frameIndex=mod(floor(frame.cameraFrame.w*60.0),64.0);
  ivec2 cell=ivec2(gl_FragCoord.xy)&1;
  int phaseIndex=int(mod(frameIndex,4.0));
  bool march=environment.cloudVolume.z<0.5 || (cell.x+2*cell.y)==phaseIndex;
  vec4 result=vec4(0.0);
  if(march && viewDirection.y>-0.002 && environment.cloudParameters.x>0.0) {
    float y=max(viewDirection.y,0.0);
    float baseKm=environment.cloudShape.y*0.001;
    float thicknessKm=max(environment.cloudShadow.y,200.0)*0.001;
    float startKm=cloudShellDistanceKm(baseKm,y);
    float endKm=cloudShellDistanceKm(baseKm+thicknessKm,y);
    // Raios rasantes cruzam dezenas de km de casca; além de 40 km é névoa.
    float pathKm=min(endKm-startKm,40.0);
    int steps=int(clamp(environment.cloudShadow.w,8.0,64.0));
    float stepKm=pathKm/float(steps);
    // Ruído de gradiente intercalado que muda a cada quadro: o acúmulo
    // temporal soma posições diferentes dentro do passo.
    float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy+5.588238*frameIndex,vec2(0.06711056,0.00583715))));
    vec3 toSun=normalize(environment.sunDirectionIntensity.xyz);
    float extinction=mix(8.0,48.0,environment.cloudParameters.w);   // por km com densidade 1
    float cosine=dot(viewDirection,toSun);
    float phase=mix(henyeyGreenstein(cosine,0.8),henyeyGreenstein(cosine,-0.3),0.35)*4.0*PI;
    // Mip do mapa pela área do pixel neste alvo, como a camada 2D faz.
    float focal=frame.materialFactors.w>0.0?frame.materialFactors.w:1.732050808;
    float pixelAngle=2.0/(focal*float(textureSize(cloudHistory,0).y));
    float texelKm=1.0/(environment.cloudShape.x*1000.0*256.0);
    float noiseTexelKm=1.0/(environment.cloudShape.x*1000.0*2.0*64.0);
    float transmittance=1.0;
    vec3 sums=vec3(0.0);
    for(int i=0;i<64;++i) {
      if(i>=steps) break;
      float t=startKm+(float(i)+jitter)*stepKm;
      vec3 p=viewDirection*t;
      float height=(cloudPointHeightKm(p)-baseKm)/thicknessKm;
      if(height<0.0 || height>1.0) continue;
      float lod=clamp(log2(max(t*pixelAngle,1e-6)/texelKm)-1.0,0.0,2.0);
      float noiseLod=max(log2(max(t*pixelAngle,1e-6)/noiseTexelKm),0.0);
      float density=cloudVolumeDensity(p,height,lod,noiseLod,false);
      if(density<=0.003) continue;
      // Marcha de luz: 4 passos crescentes rumo ao sol, sem erosão.
      float optical=0.0;
      for(int j=0;j<4;++j) {
        float d=thicknessKm*(0.06+0.14*float(j));
        vec3 lp=p+toSun*d;
        float lh=(cloudPointHeightKm(lp)-baseKm)/thicknessKm;
        if(lh>1.0) break;
        optical+=cloudVolumeDensity(lp,clamp(lh,0.0,1.0),lod+1.0,noiseLod+1.0,true)*thicknessKm*0.14;
      }
      // Beer com espalhamento múltiplo (três oitavas, Wrenninge) e Powder suave.
      float od=optical*extinction;
      float beer=exp(-od)+0.5*exp(-od*0.25)+0.25*exp(-od*0.0625);
      float powder=1.0-exp(-density*extinction*0.08-0.05);
      float sunLight=beer*(0.6+0.4*powder)*(0.5+0.5*phase);
      // Ambiente por altura: o céu converte em topo azul-claro e base cinza-azulada.
      float ambientLight=height;
      float sampleTransmittance=exp(-density*extinction*stepKm);
      float absorbed=transmittance*(1.0-sampleTransmittance);
      sums+=absorbed*vec3(sunLight,ambientLight,t);
      transmittance*=sampleTransmittance;
      if(transmittance<0.02) break;
    }
    result=vec4(sums,1.0-transmittance);
  }
  // Acúmulo temporal: o mesmo ponto do céu no quadro anterior, pela rotação da
  // câmera (as nuvens estão longe; a translação não muda a direção).
  if(environment.cloudVolume.z>0.5) {
    vec3 far=viewDirection*50000.0;
    vec3 current=vec3(dot(environment.temporalCurrentView[0].xyz,far),dot(environment.temporalCurrentView[1].xyz,far),
                      dot(environment.temporalCurrentView[2].xyz,far));
    vec3 previous=vec3(dot(environment.temporalPreviousView[0].xyz,far),dot(environment.temporalPreviousView[1].xyz,far),
                       dot(environment.temporalPreviousView[2].xyz,far));
    float focal=environment.temporalProjection.x;
    vec2 currentXy=vec2(current.x*focal/environment.temporalProjection.y,-current.y*focal);
    vec2 previousXy=vec2(previous.x*focal/environment.temporalProjection.y,-previous.y*focal);
    vec2 currentNdc=vec2(dot(environment.temporalSurface.xy,currentXy),dot(environment.temporalSurface.zw,currentXy))/max(current.z,1e-3);
    vec2 previousNdc=vec2(dot(environment.temporalSurface.xy,previousXy),dot(environment.temporalSurface.zw,previousXy))/max(previous.z,1e-3);
    vec2 uv=gl_FragCoord.xy/vec2(textureSize(cloudHistory,0));
    vec2 previousUv=uv+(previousNdc-currentNdc)*0.5;
    if(previous.z>0.0 && all(greaterThanEqual(previousUv,vec2(0.0))) && all(lessThanEqual(previousUv,vec2(1.0)))) {
      vec4 history=textureLod(cloudHistory,previousUv,0.0);
      // Pixel marchado neste quadro pesa mais; os outros só reprojetam.
      result=march?mix(result,history,0.6):history;
    }
  }
  outCloud=result;
}
