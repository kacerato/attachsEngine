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

// Estrelas: uma em ~3% das células de uma grade na direção, com brilho em lei de
// potência (poucas fortes, muitas fracas), cor de temperatura e cintilação lenta.
// Só aparecem com o sol abaixo do horizonte e onde o céu já está escuro.
vec3 celestialStars(vec3 direction,vec3 sunDirection,vec3 sky,float pixelAngle,float time) {
  float intensity=environment.celestialStars.x;
  if(intensity<=0.0 || direction.y<=-0.02) return vec3(0.0);
  // Como no céu da Unity: nenhuma estrela com o sol acima do horizonte; elas
  // acendem ao longo do crepúsculo civil (sol até -6 graus, y ~ -0,10).
  float night=smoothstep(-0.01,-0.10,sunDirection.y);
  float darkness=clamp(1.0-dot(sky,vec3(0.2126,0.7152,0.0722))*10.0,0.0,1.0);
  float horizon=smoothstep(-0.02,0.18,direction.y);
  float visibility=night*darkness*horizon;
  if(visibility<=0.0) return vec3(0.0);
  const float cells=170.0;
  vec3 cell=floor(direction*cells);
  float presence=celestialHash13(cell);
  if(presence>0.032) return vec3(0.0);
  vec3 jitter=celestialHash33(cell);
  vec3 star=normalize((cell+0.5+(jitter-0.5)*0.55)/cells);
  float angle=acos(clamp(dot(direction,star),-1.0,1.0));
  float size=max(pixelAngle*0.9,2.0e-4);
  float shape=exp(-(angle*angle)/(size*size));
  float magnitude=0.25+3.5*pow(jitter.x,7.0);
  float twinkle=0.78+0.22*sin(time*(1.3+jitter.y*3.0)+presence*120.0);
  vec3 color=mix(vec3(1.0,0.80,0.60),vec3(0.72,0.84,1.0),jitter.z);
  return color*magnitude*shape*twinkle*visibility*intensity;
}
