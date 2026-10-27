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

#ifdef CLOUD_LAYER_SKY
// Composição sobre o céu já desenhado (sol, lua e estrelas ficam atrás).
// `sunRadiance` é a luz do sol que chega à camada na escala deste céu, e
// `skyAmbient` a luz difusa do céu nessa mesma escala.
//
// A camada vai da Altitude até Altitude+Espessura. O mapa diz quanto de
// nuvem há em cada coluna e a coluna sobe até essa fração da espessura (base
// reta, topo alto onde é densa, como um cúmulo). Perto do horizonte o raio
// cruza a casca de lado e enxerga a lateral das nuvens.
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
  const int steps=8;
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

  // Relevo: o raio para na primeira amostra que entra no perfil do cúmulo
  // (altura relativa abaixo da densidade da coluna), com refinamento linear
  // entre a última amostra fora e a primeira dentro, como um parallax
  // occlusion. Acima da cabeça a opacidade é a do mapa 2D (a cobertura não
  // muda com a espessura); de lado o raio encontra a parede das colunas altas.
  // Um deslocamento por pixel esconde os degraus que restam.
  float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(0.06711056,0.00583715))));
  float previousGap=1.0,previousShape=0.0,previousHeight=0.0,previousTop=0.0;
  float hitShape=0.0,hitHeight=0.0,hitTop=1.0;
  bool hit=false;
  for(int i=0;i<steps;++i) {
    float t=startKm+(float(i)+jitter)*stepKm;
    vec3 p=viewDirection*t;
    // Forma suave (mip +1,5, sem erosão) para a altura da coluna: paredes
    // lisas, sem as "penas" que o detalhe de alta frequência riscava nelas.
    vec2 sampleUv=cloudLayerUv(p.xz*1000.0);
    float shape=cloudCumulusFromMap(vec3(textureLod(cloudMap,sampleUv,lod+2.5).r,0.0,0.5));
    // Núcleo denso chega ao topo, bordas ficam baixas: topo fofo, base reta.
    float top=pow(smoothstep(0.15,0.85,shape),0.7);
    float height=thicknessKm>0.01?clamp((cloudPointHeightKm(p)-baseKm)/thicknessKm,0.0,1.0):0.0;
    float gap=height-top;
    if(gap<=0.0 && top>0.04) {
      // Bissecção entre a última amostra fora e esta: a altura do acerto sai
      // contínua e a luz por altura não forma degraus nas laterais.
      float lowT=i==0?startKm:t-stepKm,highT=t;
      if(i>0) for(int k=0;k<3;++k) {
        float midT=0.5*(lowT+highT);
        vec3 m=viewDirection*midT;
        float mShape=cloudCumulusFromMap(vec3(textureLod(cloudMap,cloudLayerUv(m.xz*1000.0),lod+2.5).r,0.0,0.5));
        float mHeight=clamp((cloudPointHeightKm(m)-baseKm)/thicknessKm,0.0,1.0);
        if(mHeight<=pow(smoothstep(0.15,0.85,mShape),0.7)) highT=midT; else lowT=midT;
      }
      vec3 h=viewDirection*highT;
      float hShape=cloudCumulusFromMap(vec3(textureLod(cloudMap,cloudLayerUv(h.xz*1000.0),lod+2.5).r,0.0,0.5));
      hitHeight=thicknessKm>0.01?clamp((cloudPointHeightKm(h)-baseKm)/thicknessKm,0.0,1.0):0.0;
      hitTop=max(pow(smoothstep(0.15,0.85,hShape),0.7),1e-3);
      // Opacidade com o detalhe, só no ponto de acerto.
      vec2 hitUv=cloudLayerUv(h.xz*1000.0);
      mediump vec3 map=textureLod(cloudMap,hitUv,lod).rgb;
      map.b=textureLod(cloudMap,hitUv*3.0+vec2(0.37,0.61),lod).b;
      // A borda vem da forma suave; o detalhe só texturiza (sem cortinas nas paredes).
      hitShape=hShape*mix(0.8,1.15,cloudCumulusFromMap(map));
      hit=true;
      break;
    }
    previousGap=gap;previousShape=shape;previousHeight=height;previousTop=top;
    if(thicknessKm<=0.01) break;
  }
  float alpha=hit?hitShape*opacity:0.0;
  vec3 cloud=sky;
  if(hit) {
    float relative=clamp(hitHeight/hitTop,0.0,1.0);        // 0 base, 1 topo da coluna
    float above=max(hitTop-hitHeight,0.0)*max(thicknessKm,0.3)+hitShape*0.15;
    float sunTransmittance=exp(-extinction*0.25*(above/max(toSun.y,0.15)*(1.0-lowSun)+side*0.3));
    float lift=mix(mix(0.3,1.0,relative),1.0,lowSun);
    vec3 multiple=mix(sunRadiance,sunGrey,0.5)*0.3*(1.0-sunTransmittance*0.5);
    vec3 lit=sunRadiance*(sunTransmittance*lift+phase*(1.0-hitShape*0.6)*2.0)+multiple+
             ambient*(0.55+0.45*relative);
    cloud=lit*environment.cloudTint.rgb;
  }
  // Cirros: véu fino 6 km acima da base, atrás dos cúmulos e iluminado direto.
  float cirrusKm=cloudShellDistanceKm(baseKm+6.0,y);
  vec2 cirrusUv=cloudLayerUv(viewDirection.xz*cirrusKm*1000.0)*0.5+vec2(0.21,0.67);
  float cirrusLod=clamp(log2(max(cirrusKm*pixelAngle,1e-6)/(texelKm*2.0)),0.0,6.0);
  float cirrus=smoothstep(0.52,0.9,textureLod(cloudMap,cirrusUv,cirrusLod).g)*
               environment.cloudParameters.z*0.5*opacity*(1.0-exp(-y*12.0));
  vec3 cirrusColor=(sunRadiance*(0.6+phase*2.0)+ambient)*environment.cloudTint.rgb;
  sky=mix(sky,mix(cirrusColor,sky,1.0-exp(-cirrusKm/60.0)),cirrus);
  if(alpha<=0.001) return sky;
  // Longe, a nuvem se perde na névoa do horizonte, que é o próprio céu.
  float haze=1.0-exp(-startKm/38.0);
  cloud=mix(cloud,sky,haze*0.8);
  float horizon=smoothstep(-0.002,0.03,viewDirection.y);
  return mix(sky,cloud,alpha*horizon);
}
#endif
