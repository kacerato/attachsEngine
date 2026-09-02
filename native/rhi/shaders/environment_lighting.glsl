layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters; // exposure, rotation radians, maximum LOD, reserved
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  // Transient frame data appended after the stable AEEN resource payload.
  // Rows transform a world-space direction into camera/view space.
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
  vec4 quality; // normal distance, specular distance, hemispheric, specular probe
  mat4 shadowViewProjection[4];
  vec4 shadowSplitDepths;
  vec4 shadowParameters;
  vec4 shadowWorldUnitsPerTexel;
  vec4 shadowFilterParameters; // near PCF radius, far PCF radius, reserved
  vec4 materialDistanceParameters; // MR map, emissive map, fade-band ratio, reserved
} environment;
layout(set=1,binding=1) uniform sampler2D environmentMap;
layout(set=1,binding=2) uniform sampler2D shadowAtlas;
layout(set=1,binding=3) uniform sampler2D environmentSpecularMap;
layout(set=1,binding=4) uniform sampler2D environmentBrdfLut;

// A UV do panorama permanece highp: sao 1024 px por eixo com costura horizontal,
// e o fract() perto da costura e exatamente onde fp16 produziria uma emenda
// visivel. A radiancia devolvida ja pode descer para mediump.
highp vec2 environmentUv(highp vec3 direction) {
  // Callers provide a normalized reflection direction. reflect() preserves
  // length for normalized N/V, avoiding a redundant reciprocal sqrt per pixel.
  highp float phi=atan(direction.z,direction.x)+environment.parameters.y;
  return vec2(fract(phi/(2.0*PI)+.5),acos(clamp(direction.y,-1.0,1.0))/PI);
}

mediump vec3 environmentRadiance(highp vec3 direction,mediump float lod) {
  if(environment.parameters.w<0.5)
    return textureLod(environmentMap,environmentUv(direction),
                      clamp(lod,0.0,environment.parameters.z)).rgb;
  // Octahedral projection is homogeneous: reflect() already returns a unit
  // direction, but even a slightly non-unit vector maps correctly without a
  // reciprocal sqrt. This replaces atan+acos in the hot PBR fragment path.
  highp vec2 folded=direction.xz/
      max(abs(direction.x)+abs(direction.y)+abs(direction.z),1e-8);
  if(direction.y<0.0)
    folded=(1.0-abs(folded.yx))*mix(vec2(-1.0),vec2(1.0),
                                    greaterThanEqual(folded,vec2(0.0)));
  highp vec2 uv=folded*0.5+0.5;
  return textureLod(environmentSpecularMap,uv,clamp(lod,0.0,environment.parameters.z)).rgb;
}

mediump vec2 environmentBrdf(mediump float noV,mediump float roughness) {
  return environment.parameters.w>1.5
      ?texture(environmentBrdfLut,vec2(clamp(noV,0.0,1.0),clamp(roughness,0.0,1.0))).rg
      :vec2(.25+.75*(1.0-roughness),0.0);
}

mediump vec3 toneMapEnvironment(mediump vec3 color) {
  color=max(color*environment.parameters.x,vec3(0));
  // Stable filmic curve with a soft shoulder for the HDR sun.
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
}

mediump float directionalShadow(highp vec3 worldPosition, mediump vec3 normal,
                                highp float viewDepth) {
  int cascadeCount=int(environment.shadowParameters.y+0.5);
  if(cascadeCount<=0) return 1.0;
  if(viewDepth>environment.shadowSplitDepths[cascadeCount-1]) return 1.0;
  int cascade=0;
  while(cascade<cascadeCount-1 && viewDepth>environment.shadowSplitDepths[cascade])
    ++cascade;
  highp vec3 offsetPosition=worldPosition+normal*
      (environment.shadowParameters.w*environment.shadowWorldUnitsPerTexel[cascade]);
  highp vec4 clip=environment.shadowViewProjection[cascade]*vec4(offsetPosition,1.0);
  highp vec3 projected=clip.xyz/clip.w;
  highp vec2 localUv=projected.xy*0.5+0.5;
  if(projected.z<=0.0 || projected.z>=1.0 || any(lessThan(localUv,vec2(0.0))) ||
     any(greaterThan(localUv,vec2(1.0)))) return 1.0;
  highp vec2 tile=vec2(float(cascade&1),float(cascade>>1));
  highp float grid=cascadeCount>1?2.0:1.0;
  highp vec2 atlasUv=(localUv+tile)/grid;
  highp float cascadeRatio=cascadeCount>1?float(cascade)/float(cascadeCount-1):0.0;
  int radius=int(mix(environment.shadowFilterParameters.x,
                     environment.shadowFilterParameters.y,cascadeRatio)+0.5);
  mediump float visible=0.0;
  mediump float samples=0.0;
  for(int y=-2;y<=2;++y) for(int x=-2;x<=2;++x) {
    if(abs(x)>radius || abs(y)>radius) continue;
    highp float stored=texture(shadowAtlas,atlasUv+vec2(x,y)*environment.shadowParameters.x).r;
    visible+=projected.z<=stored?1.0:0.0;
    samples+=1.0;
  }
  return visible/max(samples,1.0);
}
