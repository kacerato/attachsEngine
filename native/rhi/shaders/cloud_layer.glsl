// G4b: Cloud Layer (HDRP 17) para mobile. Quem inclui declara, no bloco de
// ambiente, environment.cloudParameters/cloudShape/cloudTint/cloudShadow
// (offset 3536, ver DirtRoadFrameUniform) e o sampler `cloudMap` (binding 18).
//
// Uma camada plana na Altitude: o céu acha o ponto da camada pela direção de
// vista e o chão acha o mesmo ponto pela direção do sol, então a sombra cai sob
// a nuvem desenhada. O mapa é RGBA8 repetível (renderer/cloud_layer.cpp):
// R cúmulos, G cirros, B detalhe. A regra de cobertura é cloudLayerAlpha da CPU.

// Só os cúmulos (canais R e B), sem a opacidade.
mediump float cloudCumulusFromMap(mediump vec3 map) {
  mediump float coverage=environment.cloudParameters.y;
  mediump float shape=map.r-(map.b-0.5)*0.12;
  mediump float threshold=0.9-1.2*coverage;
  return smoothstep(threshold,threshold+0.3,shape)*smoothstep(0.0,0.06,coverage);
}

mediump float cloudLayerAlphaFromMap(mediump vec3 map) {
  mediump float coverage=environment.cloudParameters.y;
  mediump float shape=map.r-(map.b-0.5)*0.12;
  mediump float threshold=0.9-1.2*coverage;
  mediump float cumulus=smoothstep(threshold,threshold+0.3,shape)*smoothstep(0.0,0.06,coverage);
  mediump float cirrus=smoothstep(0.52,0.9,map.g)*environment.cloudParameters.z*0.6;
  return clamp(cumulus+cirrus*(1.0-cumulus),0.0,1.0)*environment.cloudParameters.x;
}

// uv em repetições do mapa para um ponto XZ do mundo sobre a camada.
highp vec2 cloudLayerUv(highp vec2 worldXz) {
  return (worldXz-environment.cloudShape.zw)*environment.cloudShape.x;
}

mediump float cloudLayerAlphaAt(highp vec2 uv,mediump float lod) {
  // O detalhe corrói a borda em 3x a frequência do mapa.
  mediump vec3 map=textureLod(cloudMap,uv,lod).rgb;
  map.b=textureLod(cloudMap,uv*3.0+vec2(0.37,0.61),lod).b;
  return cloudLayerAlphaFromMap(map);
}

// Fração da luz do sol que passa pela camada até `worldPosition`.
mediump float cloudShadowVisibility(highp vec3 worldPosition) {
  mediump float strength=environment.cloudShadow.x;
  if(strength<=0.0 || environment.cloudParameters.x<=0.0) return 1.0;
  highp vec3 toSun=normalize(environment.sunDirectionIntensity.xyz);
  if(toSun.y<=0.02) return 1.0;
  highp float climb=max(environment.cloudShape.y-worldPosition.y,0.0)/toSun.y;
  highp vec2 hit=worldPosition.xz+toSun.xz*climb;
  // Mip 2: a sombra de uma nuvem a 2 km nunca tem borda de texel.
  return 1.0-cloudLayerAlphaAt(cloudLayerUv(hit),2.0)*strength;
}

#ifdef CLOUD_LAYER_SKY
// Distância (km) do observador no chão até a casca de raio R+h, pela forma
// estável de renderer::cloudShellDistance: no zênite dá h, no horizonte
// ~sqrt(2Rh). É a curvatura do planeta que encolhe as nuvens até a faixa
// densa do horizonte, como nas nuvens do HDRP.
float cloudShellDistanceKm(float heightKm,float viewSine) {
  const float R=6371.0;
  float c=2.0*R*heightKm+heightKm*heightKm;
  return c/(R*viewSine+sqrt(R*R*viewSine*viewSine+c));
}
// Altura (km) acima do chão de um ponto a `p` km do observador, sem
// cancelamento: (|p|^2+2R p.y)/(|R up + p|+R).
float cloudPointHeightKm(vec3 p) {
  const float R=6371.0;
  float n=dot(p,p)+2.0*R*p.y;
  return n/(sqrt(R*R+n)+R);
}

// Composição sobre o céu já desenhado (sol, lua e estrelas ficam atrás).
// `sunRadiance` é a luz do sol que chega à camada na escala deste céu, e
// `skyAmbient` a luz difusa do céu nessa mesma escala.
//
// A camada vai da Altitude até Altitude+Espessura. Seis passos atravessam a
// casca: o mapa diz quanto de nuvem há em cada coluna e o perfil vertical
// sobe mais onde ela é densa (base reta, topo alto, como um cúmulo). Perto do
// horizonte o raio cruza a casca de lado e enxerga a lateral das nuvens.
vec3 cloudLayerComposite(vec3 sky,vec3 viewDirection,vec3 toSun,vec3 sunRadiance,vec3 skyAmbient,
                         float pixelAngle) {
  float opacity=environment.cloudParameters.x;
  if(opacity<=0.0 || viewDirection.y<=-0.002) return sky;
  float y=max(viewDirection.y,0.0);
  float baseKm=environment.cloudShape.y*0.001;
  float thicknessKm=max(environment.cloudShadow.y,0.0)*0.001;
  float startKm=cloudShellDistanceKm(baseKm,y);
  float endKm=cloudShellDistanceKm(baseKm+thicknessKm,y);
  // Raios rasantes cruzam dezenas de km de casca: seis passos só cobrem os
  // primeiros 24 km, onde ainda há forma visível; o resto é névoa.
  float pathKm=min(endKm-startKm,24.0);
  const int steps=6;
  float stepKm=pathKm/float(steps);
  float toUv=environment.cloudShape.x*1000.0;           // km -> repetições do mapa
  float texelKm=1.0/(toUv*256.0);
  float lod=clamp(log2(max(startKm*pixelAngle,1e-6)/texelKm),0.0,6.0);

  // Luz: dois passos no mapa rumo ao sol (sombra lateral das vizinhas) e o
  // quanto de nuvem há acima de cada amostra (topo claro, base escura).
  // Com o sol baixo a luz vem de lado e a altura pesa pouco.
  vec2 entryUv=cloudLayerUv(viewDirection.xz*startKm*1000.0);
  vec2 sunStepUv=toSun.xz/max(toSun.y,0.15)*max(thicknessKm,0.3)*0.5*toUv;
  float side=(cloudLayerAlphaAt(entryUv+sunStepUv,lod+1.0)+
              cloudLayerAlphaAt(entryUv+sunStepUv*2.0,lod+1.5))/max(opacity,1e-3);
  float density=mix(0.6,3.0,environment.cloudParameters.w);
  float extinction=density*4.0/max(thicknessKm,0.2);   // por km
  float lowSun=1.0-clamp(toSun.y*4.0,0.0,1.0);
  float cosine=dot(viewDirection,toSun);
  const float g=0.6;
  float phase=(1.0-g*g)/pow(max(1.0+g*g-2.0*g*cosine,1e-3),1.5)*0.0796;
  const vec3 luma=vec3(0.2126,0.7152,0.0722);
  vec3 sunGrey=vec3(dot(sunRadiance,luma));
  vec3 ambient=mix(vec3(dot(skyAmbient,luma)),skyAmbient,0.5)*environment.cloudTint.w;

  float transmittance=1.0;
  vec3 color=vec3(0.0);
  for(int i=0;i<steps;++i) {
    float t=startKm+(float(i)+0.5)*stepKm;
    vec3 p=viewDirection*t;
    vec2 sampleUv=cloudLayerUv(p.xz*1000.0);
    mediump vec3 map=textureLod(cloudMap,sampleUv,lod).rgb;
    map.b=textureLod(cloudMap,sampleUv*3.0+vec2(0.37,0.61),lod).b;
    float shape=cloudCumulusFromMap(map);
    float top=pow(clamp(shape,0.0,1.0),0.6);
    // Folha plana (Espessura 0): a coluna inteira é a própria cobertura.
    float height=thicknessKm>0.01?clamp((cloudPointHeightKm(p)-baseKm)/thicknessKm,0.0,1.0):0.5*top;
    float local=clamp((top-height)*6.0,0.0,1.0)*smoothstep(0.02,0.12,shape);
    if(local<=0.0) continue;
    float above=max(top-height,0.0)*max(thicknessKm,0.3);
    float sunTransmittance=exp(-extinction*0.25*(above/max(toSun.y,0.15)*(1.0-lowSun)+side*0.3));
    float lift=mix(mix(0.35,1.0,height),1.0,lowSun);
    vec3 multiple=mix(sunRadiance,sunGrey,0.5)*0.3*(1.0-sunTransmittance*0.5);
    vec3 lit=sunRadiance*(sunTransmittance*lift+phase*(1.0-local*0.6)*2.0)+multiple+
             ambient*(0.55+0.45*height);
    // Espessura 0: uma amostra só, opacidade direta do mapa.
    float sampleT=thicknessKm>0.01?exp(-local*extinction*stepKm):1.0-local;
    color+=transmittance*(1.0-sampleT)*lit;
    transmittance*=sampleT;
    if(thicknessKm<=0.01 || transmittance<0.02) break;
  }
  // Cirros: véu fino 6 km acima da base, atrás dos cúmulos e iluminado direto.
  float cirrusKm=cloudShellDistanceKm(baseKm+6.0,y);
  vec2 cirrusUv=cloudLayerUv(viewDirection.xz*cirrusKm*1000.0)*0.5+vec2(0.21,0.67);
  float cirrusLod=clamp(log2(max(cirrusKm*pixelAngle,1e-6)/(texelKm*2.0)),0.0,6.0);
  float cirrus=smoothstep(0.52,0.9,textureLod(cloudMap,cirrusUv,cirrusLod).g)*
               environment.cloudParameters.z*0.5*opacity*(1.0-exp(-y*12.0));
  vec3 cirrusColor=(sunRadiance*(0.6+phase*2.0)+ambient)*environment.cloudTint.rgb;
  sky=mix(sky,mix(cirrusColor,sky,1.0-exp(-cirrusKm/60.0)),cirrus);
  float alpha=(1.0-transmittance)*opacity;
  if(alpha<=0.001) return sky;
  vec3 cloud=color/max(1.0-transmittance,1e-3)*environment.cloudTint.rgb;
  // Longe, a nuvem se perde na névoa do horizonte, que é o próprio céu.
  float haze=1.0-exp(-startKm/38.0);
  cloud=mix(cloud,sky,haze*0.8);
  float horizon=smoothstep(-0.002,0.03,viewDirection.y);
  return mix(sky,cloud,alpha*horizon);
}
#endif
