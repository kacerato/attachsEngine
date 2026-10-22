// PK-6: sondas de reflexo e de luz da Unity no fragmento PBR.
//
// O renderer decide na CPU, POR DESENHO, como a Unity decide por Mesh Renderer:
// as duas sondas de reflexo de maior importância que contêm o centro dos
// limites e os pesos delas (renderer/reflection_probes.cpp), e o SH L2 da
// interpolação no tetraedro de sondas de luz (renderer/light_probes.cpp). O
// fragmento só lê esse registro pelo índice do desenho (gl_InstanceIndex, que é
// o drawIndex em todos os caminhos de desenho do mapa) e faz a parte que é por
// pixel: a box projection e a busca no atlas.
//
// Os bindings 24 e 25 ficam longe do fim da lista para não colidir com quem
// acrescentar descritores depois do 18.
struct AstraDrawProbe {
  // Irradiância E (cosseno já convoluído) em SH L2 RGB; sh[0].w=1 quando vale.
  vec4 sh[9];
  // x: palavra das sondas de reflexo (encodeReflectionProbeBlend); yzw reservados.
  // highp: o arquivo que inclui declara mediump int, e a palavra usa 24 bits.
  highp uvec4 reflection;
};
layout(std430,set=1,binding=24) readonly buffer AstraProbeBlock {
  // x: bit 0 sondas de reflexo, bit 1 sondas de luz; y: sondas do quadro.
  highp uvec4 header;
  vec4 boxMin[8];     // xyz mínimo da caixa, w intensidade
  vec4 boxMax[8];     // xyz máximo, w>0.5 com box projection
  vec4 capture[8];    // xyz ponto de captura, w mip base no atlas
  vec4 parameters[8]; // x último mip, y 1 sem HDR (corta em 1), zw reservados
  AstraDrawProbe draws[];
} astraProbes;
layout(set=1,binding=25) uniform mediump sampler2DArray reflectionProbeAtlas;
layout(location=12) flat in highp uint vDrawIndex;

bool astraLightProbesActive() {
  return (astraProbes.header.x&2u)!=0u && astraProbes.draws[vDrawIndex].sh[0].w>0.5;
}
mediump vec3 astraLightProbeIrradiance(mediump vec3 n) {
  mediump float x=n.x,y=n.y,z=n.z;
  mediump float basis[9]=float[9](
      0.2820947918,0.4886025119*y,0.4886025119*z,0.4886025119*x,
      1.0925484306*x*y,1.0925484306*y*z,0.3153915653*(3.0*y*y-1.0),
      1.0925484306*x*z,0.5462742153*(x*x-z*z));
  mediump vec3 e=vec3(0.0);
  for(int i=0;i<9;++i) e+=astraProbes.draws[vDrawIndex].sh[i].rgb*basis[i];
  return max(e,vec3(0.0));
}

highp vec2 astraOctahedralUv(highp vec3 d) {
  highp vec2 folded=d.xz/max(abs(d.x)+abs(d.y)+abs(d.z),1e-8);
  if(d.y<0.0) folded=(1.0-abs(folded.yx))*mix(vec2(-1.0),vec2(1.0),greaterThanEqual(folded,vec2(0.0)));
  return folded*0.5+0.5;
}
// Box projection (o mesmo cálculo de renderer::boxProjectReflection): o raio
// refletido sai do ponto até a parede da caixa, e é esse ponto de parede, visto
// do centro de captura, que a sonda viu.
highp vec3 astraBoxProject(uint layer,highp vec3 position,highp vec3 direction) {
  if(astraProbes.boxMax[layer].w<0.5) return direction;
  highp vec3 lo=astraProbes.boxMin[layer].xyz,hi=astraProbes.boxMax[layer].xyz;
  if(any(lessThan(position,lo))||any(greaterThan(position,hi))) return direction;
  highp vec3 safe=mix(vec3(1e-6),direction,greaterThan(abs(direction),vec3(1e-6)));
  highp vec3 planes=mix(lo,hi,greaterThan(direction,vec3(0.0)));
  highp vec3 t=(planes-position)/safe;
  t=mix(vec3(1e30),t,greaterThan(abs(direction),vec3(1e-6)));
  highp float hit=min(min(t.x,t.y),t.z);
  return position+direction*hit-astraProbes.capture[layer].xyz;
}
mediump vec3 astraReflectionProbeSample(uint layer,highp vec3 position,highp vec3 direction,mediump float roughness) {
  highp vec3 d=astraBoxProject(layer,position,direction);
  mediump float base=astraProbes.capture[layer].w,last=astraProbes.parameters[layer].x;
  mediump float lod=mix(base,last,clamp(roughness,0.0,1.0));
  mediump vec3 c=textureLod(reflectionProbeAtlas,vec3(astraOctahedralUv(d),float(layer)),lod).rgb;
  if(astraProbes.parameters[layer].y>0.5) c=min(c,vec3(1.0));
  return c*astraProbes.boxMin[layer].w;
}
// Reflexo especular do ambiente com as sondas do desenho. Sem sonda (ou com o
// desenho em "Desligado"), é exatamente o environmentRadiance de antes.
mediump vec3 astraSpecularRadiance(highp vec3 position,highp vec3 direction,mediump float lod,mediump float roughness) {
  if((astraProbes.header.x&1u)==0u) return environmentRadiance(direction,lod);
  highp uint word=astraProbes.draws[vDrawIndex].reflection.x;
  mediump float w0=float((word>>8u)&255u)/255.0,w1=float((word>>16u)&255u)/255.0;
  if(w0+w1<=0.0) return environmentRadiance(direction,lod);
  mediump vec3 c=astraReflectionProbeSample(word&15u,position,direction,roughness)*w0;
  if(w1>0.0) c+=astraReflectionProbeSample((word>>4u)&15u,position,direction,roughness)*w1;
  mediump float sky=1.0-w0-w1;
  if(sky>0.004) c+=environmentRadiance(direction,lod)*sky;
  return c;
}
