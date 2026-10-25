#version 450
#extension GL_GOOGLE_include_directive : require
// G4d: reconstrução das Volumetric Clouds em meia resolução (HZD: o raymarch
// atualiza 1/16 dos pixels por quadro e a reprojeção preenche o resto).
// cloud_volume.frag marcha em 1/4 da resolução com todos os pixels ativos; a
// cada quadro o raio sai de um dos 4 subpixels do bloco 2x2 desta meia
// resolução (fase em cloudVolume.w). Aqui:
//  * o pixel daquela fase recebe a amostra nova, misturada ao histórico para
//    tirar o ruído do passo;
//  * os outros três seguem o histórico reprojetado pela rotação da câmera,
//    contido entre o mínimo e o máximo da vizinhança 3x3 da marcha (sem
//    rastro quando a nuvem passa ou o céu abre);
//  * sem histórico, upsample bilateral da marcha: os vizinhos com cobertura
//    parecida com a do texel mais próximo pesam mais, e a borda não borra.
// Em 4 quadros a meia resolução inteira foi marchada.
#include "dirt_road_frame.glsl"
layout(set=0,binding=0,std140) uniform CloudResolveBlock {
  layout(offset=3328) vec4 temporalCurrentView[3];
  layout(offset=3376) vec4 temporalPreviousView[3];
  layout(offset=3424) vec4 temporalProjection;
  layout(offset=3456) vec4 temporalSurface;
  layout(offset=4752) vec4 cloudVolume;     // fator de forma, erosão, histórico válido, fase do subpixel
} environment;
layout(set=0,binding=20) uniform mediump sampler2D cloudHistory;
layout(set=0,binding=23) uniform mediump sampler2D cloudMarch;
layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outCloud;

// Upsample bilateral: os 4 texels da marcha em volta, bilineares, com peso
// pela semelhança de cobertura ao texel do subpixel mais próximo.
vec4 bilateralUpsample(vec2 marchPosition,ivec2 marchSize,float centerCoverage) {
  vec2 base=marchPosition-0.5;
  ivec2 origin=ivec2(floor(base));
  vec2 f=base-vec2(origin);
  vec4 sum=vec4(0.0);
  float weights=0.0;
  for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
    ivec2 texel=clamp(origin+ivec2(x,y),ivec2(0),marchSize-1);
    vec4 value=texelFetch(cloudMarch,texel,0);
    float bilinear=(x==0?1.0-f.x:f.x)*(y==0?1.0-f.y:f.y);
    float similar=1.0/(1.0+abs(value.a-centerCoverage)*16.0);
    float w=bilinear*similar+1e-4;
    sum+=value*w;weights+=w;
  }
  return sum/weights;
}

void main() {
  ivec2 pixel=ivec2(gl_FragCoord.xy);
  ivec2 marchSize=textureSize(cloudMarch,0);
  ivec2 marchTexel=min(pixel/2,marchSize-1);
  int phase=int(environment.cloudVolume.w+0.5)&3;
  ivec2 subpixel=pixel-marchTexel*2;
  bool fresh=subpixel.x==(phase&1) && subpixel.y==(phase>>1);
  vec4 current=texelFetch(cloudMarch,marchTexel,0);
  vec4 low=current,high=current;
  for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
    if(x==0&&y==0) continue;
    vec4 value=texelFetch(cloudMarch,clamp(marchTexel+ivec2(x,y),ivec2(0),marchSize-1),0);
    low=min(low,value);high=max(high,value);
  }
  vec4 upsampled=bilateralUpsample(gl_FragCoord.xy*0.5,marchSize,current.a);
  vec4 result=fresh?current:upsampled;
  if(environment.cloudVolume.z>0.5) {
    // O mesmo ponto do céu no quadro anterior, pela rotação da câmera (as
    // nuvens estão longe; a translação não muda a direção).
    vec3 far=normalize(vDirection)*50000.0;
    vec3 now=vec3(dot(environment.temporalCurrentView[0].xyz,far),dot(environment.temporalCurrentView[1].xyz,far),
                  dot(environment.temporalCurrentView[2].xyz,far));
    vec3 before=vec3(dot(environment.temporalPreviousView[0].xyz,far),dot(environment.temporalPreviousView[1].xyz,far),
                     dot(environment.temporalPreviousView[2].xyz,far));
    float focal=environment.temporalProjection.x;
    vec2 nowXy=vec2(now.x*focal/environment.temporalProjection.y,-now.y*focal);
    vec2 beforeXy=vec2(before.x*focal/environment.temporalProjection.y,-before.y*focal);
    vec2 nowNdc=vec2(dot(environment.temporalSurface.xy,nowXy),dot(environment.temporalSurface.zw,nowXy))/max(now.z,1e-3);
    vec2 beforeNdc=vec2(dot(environment.temporalSurface.xy,beforeXy),dot(environment.temporalSurface.zw,beforeXy))/max(before.z,1e-3);
    vec2 uv=gl_FragCoord.xy/vec2(textureSize(cloudHistory,0));
    vec2 previousUv=uv+(beforeNdc-nowNdc)*0.5;
    if(before.z>0.0 && all(greaterThanEqual(previousUv,vec2(0.0))) && all(lessThanEqual(previousUv,vec2(1.0)))) {
      // A vizinhança da marcha contém o histórico; uma folga pequena guarda o
      // detalhe de meia resolução que só o histórico tem.
      vec4 margin=(high-low)*0.1+vec4(0.002);
      vec4 history=clamp(textureLod(cloudHistory,previousUv,0.0),low-margin,high+margin);
      // Amostra nova: metade do histórico tira o ruído do passo. Os outros
      // subpixels seguem o histórico e puxam um pouco do upsample para nunca
      // ficarem presos num quadro antigo.
      result=fresh?mix(current,history,0.5):mix(history,upsampled,0.12);
    }
  }
  outCloud=result;
}
