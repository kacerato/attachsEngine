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
  layout(offset=4112) vec4 cloudVolume;     // fator de forma, erosão, histórico válido
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

// Densidade (0..1) num ponto a `p` km do observador, com altura relativa
// `height` na camada. `cheap` pula a erosão (marcha de luz).
float cloudVolumeDensity(vec3 p,float height,float lod,bool cheap) {
  vec2 weatherUv=cloudLayerUv(p.xz*1000.0);
  // Cobertura suave do mapa: onde há cúmulos e quão densos.
  float coverage=cloudCumulusFromMap(vec3(textureLod(cloudMap,weatherUv,lod+1.0).r,0.0,0.5));
  if(coverage<=0.0) return 0.0;
  // Perfil de cúmulo: base reta e rápida, topo que sobe onde a coluna é densa.
  float top=mix(0.4,1.0,coverage);
  float profile=smoothstep(0.0,0.07,height)*smoothstep(top,top*0.5,height);
  if(profile<=0.0) return 0.0;
  // Forma: o ruído 3D repete a cada metade do mapa e anda com o mesmo vento.
  vec3 world=vec3(p.x*1000.0-environment.cloudShape.z,p.y*1000.0,p.z*1000.0-environment.cloudShape.w);
  vec3 q=world*environment.cloudShape.x*2.0;
  vec4 n=textureLod(cloudShapeNoise,q,0.0);
  float fbm=n.g*0.625+n.b*0.25+n.a*0.125;
  float shape=clamp(remapValue(n.r,fbm-1.0,1.0,0.0,1.0),0.0,1.0);
  float base=mix(1.0,shape,environment.cloudVolume.x)*profile;
  float cloud=clamp(remapValue(base,1.0-coverage,1.0,0.0,1.0),0.0,1.0)*coverage;
  if(cloud<=0.0 || cheap) return cloud;
  // Erosão: rolos finos no topo, fiapos na base.
  vec3 e3=textureLod(cloudErosionNoise,q*5.0,0.0).rgb;
  float erosion=e3.r*0.625+e3.g*0.25+e3.b*0.125;
  erosion=mix(1.0-erosion,erosion,clamp(height*4.0,0.0,1.0));
  return clamp(remapValue(cloud,erosion*environment.cloudVolume.y*0.45,1.0,0.0,1.0),0.0,1.0);
}

void main() {
  vec3 viewDirection=normalize(vDirection);
  vec4 result=vec4(0.0);
  if(viewDirection.y>-0.002 && environment.cloudParameters.x>0.0) {
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
    float frameIndex=mod(floor(frame.cameraFrame.w*60.0),64.0);
    float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy+5.588238*frameIndex,vec2(0.06711056,0.00583715))));
    vec3 toSun=normalize(environment.sunDirectionIntensity.xyz);
    float daylight=smoothstep(-0.06,0.06,toSun.y);
    vec3 sunRadiance;
    vec3 ambient;
    if(environment.sceneSky.w>1.5) {
      sunRadiance=max(environment.sunColorAngularRadius.rgb,vec3(0.0))*max(environment.sunDirectionIntensity.w,0.0)*
          environment.atmosphereScattering.x*daylight;
      ambient=max(environment.atmosphereGroundIrradiance.rgb,vec3(0.0))*(1.0/PI)*environment.atmosphereScattering.x;
    } else {
      sunRadiance=max(environment.sunColorAngularRadius.rgb,vec3(0.0))*environment.sceneAoDetail.z*daylight*
          (environment.sunDirectionIntensity.w>0.0?PI:0.0);
      ambient=max(environment.ambientColorStrength.rgb,vec3(0.0))*environment.sceneAoDetail.z;
    }
    ambient*=environment.cloudTint.w;
    float extinction=mix(8.0,48.0,environment.cloudParameters.w);   // por km com densidade 1
    float cosine=dot(viewDirection,toSun);
    float phase=mix(henyeyGreenstein(cosine,0.8),henyeyGreenstein(cosine,-0.3),0.35)*4.0*PI;
    float lod=clamp(log2(max(startKm,1.0)/4.0),0.0,4.0);
    float transmittance=1.0;
    vec3 color=vec3(0.0);
    float weightedDistance=0.0;
    for(int i=0;i<64;++i) {
      if(i>=steps) break;
      float t=startKm+(float(i)+jitter)*stepKm;
      vec3 p=viewDirection*t;
      float height=(cloudPointHeightKm(p)-baseKm)/thicknessKm;
      if(height<0.0 || height>1.0) continue;
      float density=cloudVolumeDensity(p,height,lod,false);
      if(density<=0.003) continue;
      // Marcha de luz: 4 passos crescentes rumo ao sol, sem erosão.
      float optical=0.0;
      for(int j=0;j<4;++j) {
        float d=thicknessKm*(0.06+0.14*float(j));
        vec3 lp=p+toSun*d;
        float lh=(cloudPointHeightKm(lp)-baseKm)/thicknessKm;
        if(lh>1.0) break;
        optical+=cloudVolumeDensity(lp,clamp(lh,0.0,1.0),lod+1.0,true)*thicknessKm*0.14;
      }
      float beer=exp(-optical*extinction);
      float powder=1.0-exp(-optical*extinction*2.0-0.15);
      float sunLight=beer*mix(1.0,powder,0.65)*1.4;
      vec3 scattered=sunRadiance*sunLight*phase+ambient*mix(0.35,1.0,height);
      float sampleTransmittance=exp(-density*extinction*stepKm);
      float absorbed=transmittance*(1.0-sampleTransmittance);
      color+=absorbed*scattered;
      weightedDistance+=absorbed*t;
      transmittance*=sampleTransmittance;
      if(transmittance<0.02) break;
    }
    float alpha=1.0-transmittance;
    if(alpha>0.001) {
      float distanceKm=weightedDistance/alpha;
      // Névoa: longe, a nuvem some no céu (o céu atrás aparece pela opacidade).
      float fade=mix(1.0,0.1,1.0-exp(-distanceKm/55.0))*smoothstep(-0.002,0.03,viewDirection.y);
      float opacity=environment.cloudParameters.x*fade;
      result=vec4(color*environment.cloudTint.rgb*opacity,alpha*opacity);
    }
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
      result=mix(result,history,0.85);
    }
  }
  outCloud=result;
}
