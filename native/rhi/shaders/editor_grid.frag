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

layout(location=0) in vec3 vRay;
layout(location=1) in vec3 vOriginOffset;
layout(location=0) out vec4 outColor;

// UMA familia de linhas: as perpendiculares a um eixo, de passo `spacing`.
//
// Separar as duas familias em vez de tratar a grade como um todo e o que
// resolve o angulo rasante. Perto do horizonte a celula mede centenas de pixels
// numa direcao e fracoes de pixel na outra; medir a grade por um unico numero
// obrigava a escolher entre apagar as linhas que ainda sao nitidas ou manter as
// que ja viraram ruido. Aqui cada familia responde pelo proprio rodape.
//
// `derivative` e a variacao do mundo por pixel no eixo (fwidth): sem ela a linha
// some ao longe e engorda de perto. `spacing/derivative` e o tamanho da celula
// em pixels; abaixo de ~2 px o `fract` alterna dentro do mesmo pixel e o que
// aparece e moire -- faixas diagonais em angulos que NAO existem na grade, que
// leem como uma grade torta. Por isso a familia desaparece sozinha ali.
float lineFamily(float position, float derivative, float spacing, float widthPixels) {
  float width=max(derivative/spacing,1e-8);
  float distanceToLine=abs(fract(position/spacing-0.5)-0.5)/width;
  float line=1.0-smoothstep(widthPixels-0.5,widthPixels+0.5,distanceToLine);
  return line*smoothstep(2.0,6.0,1.0/width);
}

float grid(vec2 position, vec2 derivative, float spacing, float widthPixels) {
  return max(lineFamily(position.x,derivative.x,spacing,widthPixels),
             lineFamily(position.y,derivative.y,spacing,widthPixels));
}

void main() {
  // Raio CRU, como veio do vertice. A intersecao com o plano e invariante a
  // escala do raio (`travel` escala pelo inverso), entao normalizar aqui seria
  // trabalho sem efeito -- e normalizar no vertice era o defeito.
  vec3 direction=vRay;
  vec3 origin=frame.cameraPositionNear.xyz+vOriginOffset;
  float planeHeight=frame.baseColorFactor.w;
  // Raio paralelo ao plano nunca cruza: descartar e a resposta certa, e nao um
  // denominador enorme que produziria uma linha explosiva no horizonte.
  if(abs(direction.y)<1e-7) discard;
  float travel=(planeHeight-origin.y)/direction.y;
  if(travel<=0.0) discard;
  vec3 hit=origin+direction*travel;

  // A MESMA profundidade do vertice da cena, a partir da MESMA linha de
  // worldToView. Qualquer divergencia aqui coloca a grade sistematicamente na
  // frente ou atras da geometria que ela deveria acompanhar.
  float viewZ=dot(environment.worldToViewRow2.xyz,hit-frame.cameraPositionNear.xyz);
  float nearPlane=frame.cameraPositionNear.w;
  float farPlane=uintBitsToFloat(frame.materialFlags.w);
  if(viewZ<=nearPlane || viewZ>=farPlane) discard;
  // Empurrao relativo minimo, a favor da geometria no empate. O chao e a base de
  // um objeto apoiado nele sao coplanares por construcao; sem desempate a linha
  // de contato cintila. Por ser relativo ele acompanha a perda de precisao com a
  // distancia e nao descola a grade do chao perto da camera.
  float biased=viewZ*1.0005;
  gl_FragDepth=uintBitsToFloat(frame.textureIndices.w)>0.0?
    (biased-nearPlane)/(farPlane-nearPlane):
    ((farPlane*biased-nearPlane*farPlane)/(farPlane-nearPlane))/biased;

  vec2 position=hit.xz;
  vec2 derivative=fwidth(position);
  float minorSpacing=frame.emissiveFactorStrength.x;
  float majorSpacing=frame.emissiveFactorStrength.y;
  float minorWeight=frame.emissiveFactorStrength.z;
  float fadeDistance=frame.emissiveFactorStrength.w;

  float minor=grid(position,derivative,minorSpacing,0.5)*minorWeight;
  float major=grid(position,derivative,majorSpacing,0.7);
  // Eixos do mundo: sempre a mesma linha, nunca acompanhando a camera. Eles nao
  // se repetem, entao nao ha moire a evitar -- so a espessura em pixels.
  vec2 axisDistance=abs(position)/max(derivative,vec2(1e-8));
  float axisX=1.0-smoothstep(0.7,1.7,axisDistance.y); // linha sobre z=0
  float axisZ=1.0-smoothstep(0.7,1.7,axisDistance.x); // linha sobre x=0

  float distanceFade=1.0-smoothstep(fadeDistance*0.35,fadeDistance,length(hit-origin));
  vec3 minorColor=frame.materialFactors.xyz;
  vec3 majorColor=minorColor*1.35;
  vec3 axisXColor=vec3(0.85,0.29,0.33);
  vec3 axisZColor=vec3(0.31,0.51,0.90);

  float lineAlpha=max(max(minor*0.35,major*0.55),max(axisX,axisZ));
  vec3 color=mix(minorColor,majorColor,major);
  color=mix(color,axisXColor,axisX);
  color=mix(color,axisZColor,axisZ);
  float alpha=lineAlpha*distanceFade*frame.cameraFrame.w;
  // Nenhum `discard` DEPOIS de escrever `gl_FragDepth`.
  //
  // MEDIDO no aparelho: com os descartes por alfa aqui, a grade atravessava as
  // faces da geometria; trocando a saida por magenta opaco e sem esses
  // descartes, a mesma cena ocluia perfeitamente. O descarte tardio faz este
  // driver resolver o teste de profundidade com um valor que nao e o escrito.
  //
  // Alfa zero ja nao pinta nada -- o descarte era so economia, e custava a
  // correcao. Os descartes que sobram acontecem ANTES da escrita e sao
  // obrigatorios: raio paralelo ao plano, atras da camera ou fora do alcance.
  if((frame.materialFlags.z&1u)!=0u)
    color=mix(12.92*color,1.055*pow(max(color,vec3(0)),vec3(1.0/2.4))-.055,
              greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,alpha);
}
