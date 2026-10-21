// G4b: Cloud Layer (HDRP 17) para mobile. Quem inclui declara, no bloco de
// ambiente, environment.cloudParameters/cloudShape/cloudTint/cloudShadow
// (offset 3536, ver DirtRoadFrameUniform) e o sampler `cloudMap` (binding 18).
//
// Uma camada plana na Altitude: o céu acha o ponto da camada pela direção de
// vista e o chão acha o mesmo ponto pela direção do sol, então a sombra cai sob
// a nuvem desenhada. O mapa é RGBA8 repetível (renderer/cloud_layer.cpp):
// R cúmulos, G cirros, B detalhe. A regra de cobertura é cloudLayerAlpha da CPU.

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
// Composição sobre o céu já desenhado (sol, lua e estrelas ficam atrás).
// `sunRadiance` é a luz do sol que chega à camada na escala deste céu, e
// `skyAmbient` a luz difusa do céu nessa mesma escala.
vec3 cloudLayerComposite(vec3 sky,vec3 viewDirection,vec3 toSun,vec3 sunRadiance,vec3 skyAmbient) {
  if(environment.cloudParameters.x<=0.0 || viewDirection.y<=0.0) return sky;
  float altitude=environment.cloudShape.y;
  // Curvatura barata: a camada desce para o horizonte em vez de ir ao infinito.
  float slope=max(viewDirection.y,0.0)+0.035;
  float distanceToLayer=altitude/slope;
  vec2 uv=cloudLayerUv(viewDirection.xz*distanceToLayer);
  // Nível do mip pela distância: perto do horizonte o mapa se comprime.
  float lod=clamp(log2(distanceToLayer/max(altitude,1.0))*0.9,0.0,5.0);
  float alpha=cloudLayerAlphaAt(uv,lod);
  if(alpha<=0.001) return sky;

  // Raymarching 2D do HDRP: 3 passos rumo ao sol sobre o mapa acumulam
  // densidade; Density escurece o miolo. Sol baixo atravessa mais nuvem.
  vec2 sunStep=toSun.xz/max(toSun.y,0.18)*(altitude*0.06)*environment.cloudShape.x;
  float optical=0.0;
  for(int i=1;i<=3;++i) optical+=cloudLayerAlphaAt(uv+sunStep*float(i),lod+0.5);
  float density=mix(0.3,2.5,environment.cloudParameters.w);
  float transmittance=exp(-optical*density*0.4);
  // Borda prateada: espalhamento para a frente (Henyey-Greenstein, g=0,6)
  // pesa mais onde a nuvem é fina.
  float cosine=dot(viewDirection,toSun);
  float g=0.6;
  float phase=(1.0-g*g)/pow(max(1.0+g*g-2.0*g*cosine,1e-3),1.5)*0.0796;
  float silver=phase*(1.0-alpha*0.7)*3.0;
  // Dispersão múltipla: a luz que atravessa o miolo sai difusa e quase sem cor.
  const vec3 luma=vec3(0.2126,0.7152,0.0722);
  vec3 multiple=mix(sunRadiance,vec3(dot(sunRadiance,luma)),0.5)*0.3*(1.0-transmittance*0.5);
  vec3 lit=sunRadiance*(transmittance*0.75+silver)+multiple;
  vec3 ambient=mix(vec3(dot(skyAmbient,luma)),skyAmbient,0.5)*environment.cloudTint.w;
  vec3 cloud=(lit+ambient)*environment.cloudTint.rgb;
  // Longe, a nuvem se perde na própria névoa do céu.
  float haze=1.0-exp(-distanceToLayer/(altitude*14.0));
  float horizon=smoothstep(0.0,0.08,viewDirection.y);
  cloud=mix(cloud,sky,haze*0.75);
  return mix(sky,cloud,alpha*horizon);
}
#endif
