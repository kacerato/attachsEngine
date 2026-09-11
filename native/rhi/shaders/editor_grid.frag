#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"

// A grade do chao como plano analitico, COM profundidade.
//
// Ela existe aqui e nao na lista de interface por um motivo concreto: desenhada
// depois da cena, sem teste de profundidade, a grade aparecia por cima de
// qualquer objeto -- uma caixa opaca sobre a origem nao escondia os eixos. Este
// passe escreve `gl_FragDepth` a partir do ponto de intersecao, entao a mesma
// comparacao que ordena a cena ordena a grade.
//
// O plano tambem resolve o piscar do zoom: nao ha lista de segmentos para
// trocar de uma vez quando a decada muda. Duas celulas coexistem e a fina perde
// peso continuamente, enquanto `fwidth` mantem a espessura constante em pixels.

// Os vetores que levam mundo para camera vivem no bloco de ambiente, o mesmo
// que o vertice da cena usa. Reimplementar a projecao aqui com outra convencao
// faria a grade ficar atras ou na frente do que ela deveria acompanhar.
// set=0 e so ate worldToViewRow2: e o mesmo conjunto de descritores que o ceu
// usa, e declarar membros que este passe nao le so faria a validacao do driver
// exigir um layout maior sem motivo.
layout(set=0,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
} environment;

layout(location=0) in vec3 vDirection;
layout(location=0) out vec4 outColor;

// Cobertura de uma familia de linhas de passo `spacing`, com espessura constante
// em pixels. `fwidth` da a derivada do mundo por pixel: sem ela, a linha some ao
// longe e engorda de perto.
float coverage(vec2 position, vec2 derivative, float spacing, float widthPixels) {
  vec2 scaled=position/spacing;
  vec2 width=derivative/spacing;
  vec2 distanceToLine=abs(fract(scaled-0.5)-0.5)/max(width,vec2(1e-8));
  float line=min(distanceToLine.x,distanceToLine.y);
  return 1.0-smoothstep(widthPixels-0.5,widthPixels+0.5,line);
}

void main() {
  vec3 direction=normalize(vDirection);
  vec3 origin=frame.cameraPositionNear.xyz;
  float planeHeight=frame.baseColorFactor.w;
  // Raio paralelo ao plano nunca cruza: descartar e a resposta certa, e nao um
  // denominador enorme que produziria uma linha explosiva no horizonte.
  if(abs(direction.y)<1e-5) discard;
  float travel=(planeHeight-origin.y)/direction.y;
  if(travel<=0.0) discard;
  vec3 hit=origin+direction*travel;

  vec3 relative=hit-origin;
  vec3 view=vec3(dot(environment.worldToViewRow0.xyz,relative),
                 dot(environment.worldToViewRow1.xyz,relative),
                 dot(environment.worldToViewRow2.xyz,relative));
  float nearPlane=frame.cameraPositionNear.w;
  float farPlane=uintBitsToFloat(frame.materialFlags.w);
  if(view.z<=nearPlane || view.z>=farPlane) discard;
  // A MESMA projecao do vertice da cena. Qualquer divergencia aqui coloca a
  // grade sistematicamente na frente ou atras da geometria coplanar.
  //
  // Empurrao relativo, a favor da geometria no empate. O alcance de
  // profundidade da camera editorial vai de centimetros a quilometros e o chao
  // atras de um objeto cai no mesmo valor quantizado da face desse objeto com
  // frequencia. Por ser relativo, ele some junto com a distancia e nao afasta a
  // grade do chao perto da camera.
  //
  // MEDIDO: o valor sozinho NAO elimina o vazamento nas faces verticais (ver o
  // relatorio de validacao). Aumenta-lo dez vezes mudou pouco, o que diz que o
  // residuo nao e so quantizacao -- e por isso ele esta documentado como aberto
  // em vez de escondido atras de um numero maior.
  float biased=view.z*1.005;
  gl_FragDepth=((farPlane*biased-nearPlane*farPlane)/(farPlane-nearPlane))/biased;

  vec2 position=hit.xz;
  vec2 derivative=fwidth(position);
  float minorSpacing=frame.emissiveFactorStrength.x;
  float majorSpacing=frame.emissiveFactorStrength.y;
  float minorWeight=frame.emissiveFactorStrength.z;
  float fadeDistance=frame.emissiveFactorStrength.w;

  float minor=coverage(position,derivative,minorSpacing,0.5)*minorWeight;
  float major=coverage(position,derivative,majorSpacing,0.6);
  // Eixos do mundo: sempre a mesma linha, nunca acompanhando a camera.
  vec2 axisDistance=abs(position)/max(derivative,vec2(1e-8));
  float axisX=1.0-smoothstep(0.6,1.6,axisDistance.y); // linha sobre z=0
  float axisZ=1.0-smoothstep(0.6,1.6,axisDistance.x); // linha sobre x=0

  // Celula menor que um pixel nao e informacao: e a faixa branca do horizonte.
  // Some antes de virar ruido, e some primeiro a familia mais fina.
  float minorPixels=minorSpacing/max(max(derivative.x,derivative.y),1e-8);
  minor*=smoothstep(1.5,4.0,minorPixels);
  float majorPixels=majorSpacing/max(max(derivative.x,derivative.y),1e-8);
  major*=smoothstep(1.5,4.0,majorPixels);

  float distanceFade=1.0-smoothstep(fadeDistance*0.35,fadeDistance,length(hit-origin));
  vec3 minorColor=frame.materialFactors.xyz;
  vec3 majorColor=minorColor*1.35;
  vec3 axisXColor=vec3(0.85,0.29,0.33);
  vec3 axisZColor=vec3(0.31,0.51,0.90);

  float lineAlpha=max(max(minor*0.35,major*0.55),max(axisX,axisZ));
  if(lineAlpha<=0.0) discard;
  vec3 color=mix(minorColor,majorColor,major);
  color=mix(color,axisXColor,axisX);
  color=mix(color,axisZColor,axisZ);
  float alpha=lineAlpha*distanceFade*frame.cameraFrame.w;
  if(alpha<=0.002) discard;
  if((frame.materialFlags.z&1u)!=0u)
    color=mix(12.92*color,1.055*pow(max(color,vec3(0)),vec3(1.0/2.4))-.055,
              greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,alpha);
}
