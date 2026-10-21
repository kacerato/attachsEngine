// G4: sol, lua e estrelas do céu procedural e do céu físico.
//
// Referências de estudo: escurecimento de limbo solar (Neckel & Labs 1994, lei
// aproximada I(mu) = 1 - u(1 - mu), com u maior no azul); disco lunar como esfera
// iluminada pela direção real do sol, de modo que a fase sai da geometria e não
// de uma máscara; campo de estrelas por células hash na direção (técnica comum
// de céus procedurais, ex. o Sky da Godot 4 e o Physically Based Sky da HDRP).
//
// Quem inclui precisa declarar environment.celestialParameters/celestialStars
// e definir PI. `pixelAngle` é o tamanho angular do pixel, calculado fora de
// qualquer desvio (derivadas dentro de ramos divergentes são indefinidas).

float celestialHash13(vec3 p) {
  p=fract(p*0.1031);
  p+=dot(p,p.zyx+31.32);
  return fract((p.x+p.y)*p.z);
}
vec3 celestialHash33(vec3 p) {
  p=fract(p*vec3(0.1031,0.1030,0.0973));
  p+=dot(p,p.yxz+33.33);
  return fract((p.xxy+p.yxx)*p.zyx);
}
float celestialHash12(vec2 p) {
  vec3 p3=fract(vec3(p.xyx)*0.1031);
  p3+=dot(p3,p3.yzx+33.33);
  return fract((p3.x+p3.y)*p3.z);
}
float celestialNoise(vec2 p) {
  vec2 i=floor(p),f=fract(p);
  f=f*f*(3.0-2.0*f);
  return mix(mix(celestialHash12(i),celestialHash12(i+vec2(1,0)),f.x),
             mix(celestialHash12(i+vec2(0,1)),celestialHash12(i+vec2(1,1)),f.x),f.y);
}

// Disco solar com limbo: a borda escurece e esquenta, o que faz o disco ler
// como esfera brilhante em vez de círculo chapado. Retorna o fator de radiância.
vec3 celestialSunDisc(float angle,float radius,float pixelAngle) {
  float edge=max(pixelAngle*1.5,radius*0.08);
  float coverage=1.0-smoothstep(radius-edge,radius+edge,angle);
  float r=clamp(angle/radius,0.0,1.0);
  float mu=sqrt(max(1.0-r*r,0.0));
  return (1.0-vec3(0.38,0.52,0.70)*(1.0-mu))*coverage;
}

// Auréola: espalhamento para a frente em volta do disco. Um núcleo que cai em
// poucos raios solares e uma cauda larga. Em HDR, o bloom a espalha na tela.
float celestialSunGlare(float angle,float cosine,float radius) {
  float outside=max(angle-radius,0.0);
  // Calibrado no POCO (Vila, 19/10/2026): cauda em cos^6 lavava meio céu em
  // bege; o halo precisa cair à metade em ~3°, não em ~27°.
  float core=exp(-outside/(radius*1.3))*0.45;
  float tail=pow(max(cosine,0.0),400.0)*0.05+pow(max(cosine,0.0),40.0)*0.004;
  return (core+tail)*environment.celestialParameters.x;
}

// Direção da lua pela fase: gira a direção do sol em torno do eixo horizontal
// perpendicular a ele. Fase 0,5 é exatamente oposta (lua cheia).
vec3 celestialMoonDirection(vec3 sunDirection) {
  // V-2: com uma direcional Lua (Hora do dia › Lua), o disco fica onde ela
  // está, como o Celestial Body do HDRP desenha a lua na direção da luz.
  if(environment.skyMoon.w>0.5) return normalize(environment.skyMoon.xyz);
  vec3 axis=cross(sunDirection,vec3(0.0,1.0,0.0));
  axis=dot(axis,axis)<1.0e-6?vec3(1.0,0.0,0.0):normalize(axis);
  float angle=6.2831853*environment.celestialParameters.y;
  return normalize(sunDirection*cos(angle)+cross(axis,sunDirection)*sin(angle));
}

// Lua: esfera de albedo baixo iluminada pelo sol, com mares escuros, luz cinérea
// na parte não iluminada e um halo fraco proporcional à parte acesa.
// `coverage` volta com a fração do pixel coberta pelo disco (esconde estrelas).
vec3 celestialMoon(vec3 direction,vec3 sunDirection,float pixelAngle,out float coverage) {
  coverage=0.0;
  float intensity=environment.celestialParameters.w;
  float radius=max(environment.celestialParameters.z,1.0e-4);
  if(intensity<=0.0) return vec3(0.0);
  vec3 moon=celestialMoonDirection(sunDirection);
  float cosine=clamp(dot(direction,moon),-1.0,1.0);
  float angle=acos(cosine);
  // Fração iluminada vista da Terra: 0 na nova, 1 na cheia.
  float lit=0.5*(1.0-dot(moon,sunDirection));
  vec3 moonTint=vec3(1.0,0.96,0.90);
  float halo=exp(-max(angle-radius,0.0)/(radius*2.5))*0.05*lit;
  if(angle>radius*1.6) return moonTint*halo*intensity;
  float edge=max(pixelAngle*1.5,radius*0.05);
  coverage=1.0-smoothstep(radius-edge,radius+edge,angle);
  vec3 right=cross(moon,vec3(0.0,1.0,0.0));
  right=dot(right,right)<1.0e-6?vec3(1.0,0.0,0.0):normalize(right);
  vec3 up=cross(right,moon);
  vec2 p=vec2(dot(direction,right),dot(direction,up))/sin(radius);
  float z=sqrt(max(1.0-dot(p,p),0.0));
  // A face visível aponta de volta para o observador.
  vec3 normal=normalize(right*p.x+up*p.y-moon*z);
  float sunlit=max(dot(normal,sunDirection),0.0);
  float maria=smoothstep(0.48,0.72,celestialNoise(p*2.3+vec2(3.1,1.7))*0.65+
                                  celestialNoise(p*5.1+vec2(7.3,2.9))*0.35);
  float detail=0.90+0.10*celestialNoise(p*14.0);
  float albedo=0.16*(1.0-0.42*maria)*detail;
  float earthshine=0.012;
  vec3 surface=moonTint*albedo*(sunlit*7.0+earthshine);
  return (surface*coverage+moonTint*halo)*intensity;
}

// Estrelas (V-2), no lugar do campo uniforme do G4: o céu do Physically Based
// Sky da HDRP é um mapa de emissão do espaço (catálogo real, Via Láctea), e os
// ciclos de dia da Unity giram esse mapa com a hora ("Space Rotation"). Aqui o
// mesmo céu sai de células hash na ESFERA CELESTE: a direção do mundo passa
// pelas linhas de environment.skyStarRotation (renderer/time_of_day.h), então
// as estrelas nascem a leste e giram em torno do polo da latitude.
//
// Contagem e brilho pela lei de Pogson: N(<m) cresce 10^(0,6 m) até o limite
// de visão (m 6,5, ~6000 no céu inteiro), fluxo 10^(-0,4 m). Quase todas são
// fracas; poucas dezenas brilham. O tamanho cresce só nas fortes, a cor segue
// a temperatura (azuladas e alaranjadas raras), a cintilação é discreta e
// aumenta perto do horizonte, onde a extinção também apaga as fracas.
vec3 celestialHash3(vec3 p) { return celestialHash33(p); }
float celestialNoise3(vec3 p) {
  vec3 i=floor(p),f=fract(p);
  f=f*f*(3.0-2.0*f);
  float a=celestialHash13(i),b=celestialHash13(i+vec3(1,0,0));
  float c=celestialHash13(i+vec3(0,1,0)),d=celestialHash13(i+vec3(1,1,0));
  float e=celestialHash13(i+vec3(0,0,1)),g=celestialHash13(i+vec3(1,0,1));
  float h=celestialHash13(i+vec3(0,1,1)),k=celestialHash13(i+vec3(1,1,1));
  return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,k,f.x),f.y),f.z);
}

// Uma camada de estrelas: células de `cells` por raio, `presence` delas
// ocupadas, magnitudes de `brightest` a `faintest`.
vec3 celestialStarLayer(vec3 celestial,float cells,float presence,float brightest,float faintest,
                        float pixelAngle,float time,float horizonTwinkle) {
  vec3 cell=floor(celestial*cells);
  float occupied=celestialHash13(cell+17.0);
  if(occupied>presence) return vec3(0.0);
  vec3 jitter=celestialHash3(cell);
  vec3 star=normalize((cell+0.5+(jitter-0.5)*0.7)/cells);
  float angle=acos(clamp(dot(celestial,star),-1.0,1.0));
  // Pogson: u uniforme dá N(<m) ~ 10^(0,6 m) entre as magnitudes da camada.
  float u=max(occupied/presence,1.0e-4);
  float magnitude=min(faintest+log(u)/(0.6*2.302585),faintest);
  magnitude=max(magnitude,brightest);
  float flux=exp2(-1.3287712*magnitude); // 10^(-0,4 m)
  // As fortes ocupam um pouco mais que o pixel; as fracas, o pixel.
  float size=pixelAngle*(0.65+0.9*clamp(sqrt(flux)*0.7,0.0,1.4));
  float shape=exp(-(angle*angle)/(size*size));
  float twinkle=1.0-horizonTwinkle*(0.5+0.5*sin(time*(2.1+jitter.y*4.0)+occupied*911.0));
  float temperature=jitter.z;
  vec3 color=temperature<0.12?vec3(0.70,0.80,1.0):
             (temperature>0.90?vec3(1.0,0.78,0.58):vec3(1.0,0.96,0.90));
  return color*flux*shape*twinkle;
}

// Via Láctea: faixa em torno do plano galáctico (polo norte galáctico em
// AR 192,9°, Dec +27,1°; centro em AR 266,4°, Dec -28,9°), mais clara no
// bojo, com nuvens e a faixa escura de poeira por ruído.
vec3 celestialMilkyWay(vec3 celestial) {
  const vec3 galacticPole=vec3(-0.8676,-0.1981,0.4560);
  const vec3 galacticCenter=vec3(-0.0548,-0.8728,-0.4834);
  float latitude=dot(celestial,galacticPole);
  float band=exp(-latitude*latitude/(0.17*0.17));
  if(band<0.01) return vec3(0.0);
  float bulge=0.45+0.55*pow(max(dot(celestial,galacticCenter),0.0),3.0);
  float clouds=celestialNoise3(celestial*7.0)*0.6+celestialNoise3(celestial*17.0)*0.4;
  float dust=smoothstep(0.035,0.0,abs(latitude+0.02*(celestialNoise3(celestial*11.0)-0.5)))*
             smoothstep(0.35,0.75,celestialNoise3(celestial*9.0+3.7));
  float glow=band*bulge*(0.35+0.65*clouds)*(1.0-0.75*dust);
  return vec3(1.0,0.94,0.86)*glow;
}

vec3 celestialStars(vec3 direction,vec3 sunDirection,vec3 sky,float pixelAngle,float time) {
  float intensity=environment.celestialStars.x;
  if(intensity<=0.0 || direction.y<=-0.02) return vec3(0.0);
  // Como no céu da Unity: nenhuma estrela com o sol acima do horizonte; elas
  // acendem ao longo do crepúsculo (sol até ~-10 graus, y ~ -0,17).
  float night=smoothstep(-0.02,-0.17,sunDirection.y);
  float darkness=clamp(1.0-dot(sky,vec3(0.2126,0.7152,0.0722))*10.0,0.0,1.0);
  // Extinção: perto do horizonte o ar apaga e avermelha as estrelas.
  float airmass=smoothstep(-0.02,0.30,direction.y);
  float visibility=night*darkness*airmass;
  if(visibility<=0.0) return vec3(0.0);
  vec3 celestial=direction;
  if(environment.skyStarRotation[0].w>0.5)
    celestial=normalize(vec3(dot(environment.skyStarRotation[0].xyz,direction),
                             dot(environment.skyStarRotation[1].xyz,direction),
                             dot(environment.skyStarRotation[2].xyz,direction)));
  float twinkle=0.10+0.25*(1.0-airmass);
  // Visíveis a olho nu (~6000 no céu inteiro) e um véu fraco que dá textura.
  vec3 stars=celestialStarLayer(celestial,120.0,0.040,-1.4,6.5,pixelAngle,time,twinkle)+
             celestialStarLayer(celestial,240.0,0.018,5.0,8.0,pixelAngle,time,twinkle);
  // Escala da cena com o meio-dia em ~EV100 0: a magnitude 0 fica ~200 vezes
  // acima do chão ao luar, uma de magnitude 6,5 abaixo dele, e a Via Láctea
  // no nível das estrelas mais fracas, como no céu real à mesma exposição.
  vec3 milkyWay=celestialMilkyWay(celestial)*2.5e-5;
  return (stars*0.012+milkyWay)*visibility*intensity*mix(vec3(1.0,0.82,0.66),vec3(1.0),airmass);
}
