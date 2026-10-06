precision highp float;
precision highp int;

layout(set=0,binding=0) uniform sampler2D sceneColor;
layout(set=0,binding=1) uniform sampler2D historyColor;
layout(set=0,binding=2) uniform sampler2D sceneDepth;
// Entradas temporais unificadas (temporal_projection.glsl): vetor RG16F
// atual -> anterior em UV da extensão, e as máscaras R8 do passe principal.
layout(set=0,binding=5) uniform sampler2D temporalVelocity;
layout(set=0,binding=6) uniform sampler2D reactiveMask;
layout(set=0,binding=7) uniform sampler2D compositionMask;
layout(set=0,binding=4,std430) readonly buffer ExposureState {
  float ev; float targetEv; float meteredEv; uint valid;
} automaticExposure;
layout(set=0,binding=3,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 sceneSky;
  vec4 sceneFogColorDensity;
  vec4 sceneFog;
  vec4 scenePost;
  vec4 sceneAo;
  vec4 sceneAoDetail;
  layout(offset=224) vec4 worldToViewRow0;
  layout(offset=240) vec4 worldToViewRow1;
  layout(offset=256) vec4 worldToViewRow2;
  layout(offset=3296) vec4 postViewport; // normalized physical xy/extent
  // zw: extensão renderizada / alocação das entradas temporais (profundidade,
  // vetor e máscaras), válida também quando a cor de entrada já foi ampliada.
  layout(offset=3472) vec4 temporalParameters;
} environment;
layout(push_constant) uniform PostPushConstants {
  // z bits: 1 bloom, 2 bicúbica, 4 FSR 1, 8 vetor, 16 cobertura estática,
  // 32 máscaras temporais, 64 só composição HDR (névoa+AO, antes do ampliador
  // temporal), 128 entrada já composta (depois do ampliador).
  vec4 texelFlags; // xy=1/source extent, z=bits acima, w=AA + debug*4
  vec4 bloom;      // threshold, intensity, sharpen, vignette intensity
  vec4 grade;      // contrast, saturation, encode sRGB, packed transform+feedback
  vec4 sourceTransform; // xy=active/source extent, z=focal length, w=display aspect
  vec4 currentCamera; // yaw, pitch, physical-NDC jitter x/y
  vec4 currentPositionNear; // camera xyz, near plane
  vec4 previousCamera; // previous yaw/pitch, current roll, previous roll
  vec4 previousPositionFar; // camera xyz, far plane
} post;
layout(location=0) in highp vec2 vUv;
layout(location=0) out vec4 outColor;
#if AETHER_TEMPORAL
layout(location=1) out vec4 outHistory;
#endif

float luma(vec3 value) { return dot(value, vec3(0.2126, 0.7152, 0.0722)); }

vec2 clampSourceUv(vec2 uv) {
  vec2 lower=environment.postViewport.xy*post.sourceTransform.xy+post.texelFlags.xy*0.5;
  vec2 upper=(environment.postViewport.xy+environment.postViewport.zw)*
      post.sourceTransform.xy-post.texelFlags.xy*0.5;
  return clamp(uv,lower,max(lower,upper));
}

float viewDistance(vec2 uv) {
  float depth=texture(sceneDepth,clampSourceUv(uv)).r;
  float nearPlane=post.currentPositionNear.w;
  float farPlane=post.previousPositionFar.w;
  if(post.sourceTransform.z<0.0) return mix(nearPlane,farPlane,depth);
  return nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
}

bool postFlag(float bit) { return mod(floor(post.texelFlags.z/bit),2.0)>0.5; }
vec2 temporalInputUv(vec2 uv) { return uv*environment.temporalParameters.zw; }

vec3 applySceneFog(vec2 sourceUv, vec3 color) {
  // Depois do ampliador temporal a névoa já está na cor: aplicá-la de novo
  // dobraria a densidade, e com profundidade de outra resolução.
  if(environment.sceneFog.x>0.5 && !postFlag(128.0)) {
    float viewDepth=viewDistance(sourceUv);
    float falloff=environment.sceneFog.w;
    float opticalDepth;
    if(falloff<=0.0) {
      // Exact legacy path: default/old scenes retain depth-linear exponential fog.
      opticalDepth=environment.sceneFogColorDensity.w*
          max(0.0,viewDepth-environment.sceneFog.y);
    } else {
      vec2 outputUv=sourceUv/max(post.sourceTransform.xy,vec2(1.0e-6));
      vec2 physicalNdc=(outputUv-environment.postViewport.xy)/
          max(environment.postViewport.zw,vec2(1.0e-6))*2.0-1.0;
      int packed=int(floor(post.grade.w));
      vec4 surface=vec4(float((packed>>0)&3)-1.0,float((packed>>2)&3)-1.0,
                        float((packed>>4)&3)-1.0,float((packed>>6)&3)-1.0);
      vec2 cameraNdc=vec2(dot(surface.xz,physicalNdc),dot(surface.yw,physicalNdc));
      float orthoHalfHeight=max(environment.worldToViewRow0.w,0.0);
      vec3 viewDirection;
      float rayOriginHeight=post.currentPositionNear.y;
      float rayDistance;
      if(orthoHalfHeight>0.0) {
        vec3 viewOffset=vec3(cameraNdc.x*orthoHalfHeight*post.sourceTransform.w,
                             -cameraNdc.y*orthoHalfHeight,0.0);
        rayOriginHeight+=dot(vec3(environment.worldToViewRow0.y,
                                 environment.worldToViewRow1.y,
                                 environment.worldToViewRow2.y),viewOffset);
        viewDirection=vec3(0.0,0.0,1.0);
        rayDistance=viewDepth;
      } else {
        float focal=max(abs(post.sourceTransform.z),1.0e-6);
        viewDirection=normalize(vec3(cameraNdc.x*post.sourceTransform.w/focal,
                                     -cameraNdc.y/focal,1.0));
        rayDistance=viewDepth/max(viewDirection.z,1.0e-6);
      }
      float worldDirectionY=dot(vec3(environment.worldToViewRow0.y,
                                     environment.worldToViewRow1.y,
                                     environment.worldToViewRow2.y),viewDirection);
      float segment=max(0.0,rayDistance-environment.sceneFog.y);
      float startHeight=rayOriginHeight+worldDirectionY*environment.sceneFog.y;
      float density=environment.sceneFogColorDensity.w;
      opticalDepth=0.0;
      if(segment>0.0&&density>0.0) {
        float endHeight=startHeight+worldDirectionY*segment;
        float change=abs(falloff*worldDirectionY*segment);
        // Integrate from the denser endpoint and work in log space. Clamping
        // two exponentials separately would cancel their saturation and make
        // a ray crossing from high altitude into dense low fog transparent.
        float factor=change<1.0e-3?1.0-change*0.5+change*change/6.0:
                     (1.0-exp(-change))/change;
        float logDepth=log(density)+log(segment)+log(factor)-
            falloff*(min(startHeight,endHeight)-environment.sceneFog.z);
        opticalDepth=exp(clamp(logDepth,-80.0,log(80.0)));
      }
    }
    float fog=1.0-exp(-opticalDepth);
    color=mix(color,environment.sceneFogColorDensity.rgb,clamp(fog,0.0,1.0));
  }
  return color;
}

vec3 sampleScene(vec2 uv) {
  vec2 sourceUv=clampSourceUv(uv);
  return applySceneFog(sourceUv, texture(sceneColor,sourceUv).rgb);
}

vec3 reconstructScene(vec2 uv) {
  // Catmull-Rom has four taps per axis. The two central, positive weights
  // share one bilinear fetch, reducing the separable 4x4 kernel to nine.
  // Clamp every fetch to the active rectangle: dynamic resolution leaves
  // unused texels in the allocation which must never leak into the image.
  vec2 pixel=uv/post.texelFlags.xy-0.5;
  vec2 base=floor(pixel);
  vec2 f=pixel-base;
  vec2 w0=f*(-0.5+f*(1.0-0.5*f));
  vec2 w1=1.0+f*f*(-2.5+1.5*f);
  vec2 w2=f*(0.5+f*(2.0-1.5*f));
  vec2 w3=f*f*(-0.5+0.5*f);
  vec2 middle=w1+w2;
  vec2 p0=(base-0.5)*post.texelFlags.xy;
  vec2 p1=(base+0.5+w2/middle)*post.texelFlags.xy;
  vec2 p2=(base+2.5)*post.texelFlags.xy;
  vec3 weightsX=vec3(w0.x,middle.x,w3.x);
  vec3 weightsY=vec3(w0.y,middle.y,w3.y);
  vec3 positionsX=vec3(p0.x,p1.x,p2.x);
  vec3 positionsY=vec3(p0.y,p1.y,p2.y);
  vec3 result=vec3(0),minimum=vec3(1e30),maximum=vec3(-1e30);
  for(int y=0;y<3;++y) for(int x=0;x<3;++x) {
    vec3 value=texture(sceneColor,clampSourceUv(vec2(positionsX[x],positionsY[y]))).rgb;
    result+=value*weightsX[x]*weightsY[y];
    minimum=min(minimum,value);maximum=max(maximum,value);
  }
  // Negative lobes restore detail but may ring around HDR highlights. Keep
  // reconstruction inside the source neighbourhood before tone mapping.
  return applySceneFog(uv,clamp(result,minimum,maximum));
}

vec3 aoViewPosition(vec2 uv,float depth) {
  vec2 outputUv=uv/max(post.sourceTransform.xy,vec2(1e-6));
  vec2 physicalNdc=(outputUv-environment.postViewport.xy)/
      max(environment.postViewport.zw,vec2(1e-6))*2.0-1.0;
  int packed=int(floor(post.grade.w));
  vec4 surface=vec4(float((packed>>0)&3)-1.0,float((packed>>2)&3)-1.0,
                   float((packed>>4)&3)-1.0,float((packed>>6)&3)-1.0);
  physicalNdc-=post.currentCamera.zw;
  vec2 ndc=vec2(dot(surface.xz,physicalNdc),dot(surface.yw,physicalNdc));
  float halfHeight=max(environment.worldToViewRow0.w,0.0);
  vec2 xy=vec2(ndc.x*post.sourceTransform.w,-ndc.y);
  return vec3(xy*(halfHeight>0.0?halfHeight:
      depth/max(abs(post.sourceTransform.z),1e-6)),depth);
}

float screenSpaceAmbientOcclusion(vec2 uv) {
  if(environment.sceneAo.x<0.5 || postFlag(128.0)) return 1.0;
  float rawDepth=texture(sceneDepth,clampSourceUv(uv)).r;
  float center=viewDistance(uv);
  vec3 position=aoViewPosition(uv,center);
  // Compare samples against the local tangent plane. Comparing only their
  // depth incorrectly occluded every sloping floor and changed its brightness
  // as the camera rotated. Evaluate derivatives before the sky branch.
  vec3 normal=cross(dFdx(position),dFdy(position));
  float normalLength=length(normal);
  if(rawDepth>=0.999999 || normalLength<1e-8) return 1.0;
  normal/=normalLength;
  if(dot(normal,-position)<0.0) normal=-normal;
  float radius=max(environment.sceneAo.y,0.05);
  float radiusPixels=clamp(radius*abs(post.sourceTransform.z)/
      max(center*post.texelFlags.y*2.0,1.0e-5),1.0,48.0);
  const vec2 directions[12]=vec2[12](
    vec2(1,0),vec2(.866,.5),vec2(.5,.866),vec2(0,1),
    vec2(-.5,.866),vec2(-.866,.5),vec2(-1,0),vec2(-.866,-.5),
    vec2(-.5,-.866),vec2(0,-1),vec2(.5,-.866),vec2(.866,-.5));
  float occlusion=0.0;
  // Rotação barata por pixel evita que as doze direções apareçam como raios
  // fixos, sem textura de ruído nem estado temporal adicional.
  float angle=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)*6.2831853;
  mat2 rotation=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
  for(int index=0;index<12;++index) {
    float ring=.35+.65*float((index%3)+1)/3.0;
    vec2 offset=rotation*directions[index]*post.texelFlags.xy*radiusPixels*ring;
    vec2 sampleUv=uv+offset;
    if(any(notEqual(sampleUv,clampSourceUv(sampleUv))) ||
       texture(sceneDepth,sampleUv).r>=0.999999) continue;
    vec3 delta=aoViewPosition(sampleUv,viewDistance(sampleUv))-position;
    float distance=length(delta);
    float height=dot(normal,delta);
    float range=max(0.0,1.0-distance/radius);
    float horizon=max(0.0,(height-environment.sceneAoDetail.x)/max(distance,1e-5));
    occlusion+=horizon*range;
  }
  float visibility=clamp(1.0-occlusion*(environment.sceneAo.z/12.0),0.0,1.0);
  return pow(visibility,max(environment.sceneAo.w,0.1));
}

vec3 sampleFxaa(vec2 uv, out vec3 unfiltered, out vec3 crossAverage) {
  vec2 px = post.texelFlags.xy;
  float aaMode=mod(post.texelFlags.w,4.0);
  vec3 center = sampleScene(uv);
  unfiltered = center;
  crossAverage = center;
  if (post.bloom.z > 0.0 || aaMode >= 2.0) {
    vec3 north = sampleScene(uv + vec2(0.0, -px.y));
    vec3 south = sampleScene(uv + vec2(0.0,  px.y));
    vec3 west  = sampleScene(uv + vec2(-px.x, 0.0));
    vec3 east  = sampleScene(uv + vec2( px.x, 0.0));
    crossAverage = (north + south + west + east) * 0.25;
  }
  if (aaMode < 0.5 || aaMode > 1.5) return center;

  // FXAA 3.x-style edge search. Reject non-edges first; unlike the old cross
  // average this does not blur foliage interiors and material texture detail.
  vec3 northWest = sampleScene(uv + vec2(-px.x, -px.y));
  vec3 northEast = sampleScene(uv + vec2( px.x, -px.y));
  vec3 southWest = sampleScene(uv + vec2(-px.x,  px.y));
  vec3 southEast = sampleScene(uv + vec2( px.x,  px.y));
  float lumaCenter = luma(center);
  float lumaNorthWest = luma(northWest);
  float lumaNorthEast = luma(northEast);
  float lumaSouthWest = luma(southWest);
  float lumaSouthEast = luma(southEast);
  float minimum = min(lumaCenter, min(min(lumaNorthWest, lumaNorthEast),
                                      min(lumaSouthWest, lumaSouthEast)));
  float maximum = max(lumaCenter, max(max(lumaNorthWest, lumaNorthEast),
                                      max(lumaSouthWest, lumaSouthEast)));
  float range = maximum - minimum;
  if (range < max(0.0312, maximum * 0.125)) return center;

  vec2 direction;
  direction.x = -((lumaNorthWest + lumaNorthEast) -
                  (lumaSouthWest + lumaSouthEast));
  direction.y =  ((lumaNorthWest + lumaSouthWest) -
                  (lumaNorthEast + lumaSouthEast));
  float directionReduce = max((lumaNorthWest + lumaNorthEast +
                               lumaSouthWest + lumaSouthEast) * (0.25 * 0.25),
                              1.0 / 128.0);
  float reciprocalMinimum = 1.0 /
      (min(abs(direction.x), abs(direction.y)) + directionReduce);
  direction = clamp(direction * reciprocalMinimum, vec2(-8.0), vec2(8.0)) * px;

  vec3 candidateNear = 0.5 * (
      sampleScene(uv + direction * (1.0 / 3.0 - 0.5)) +
      sampleScene(uv + direction * (2.0 / 3.0 - 0.5)));
  vec3 candidateFar = candidateNear * 0.5 + 0.25 * (
      sampleScene(uv + direction * -0.5) +
      sampleScene(uv + direction *  0.5));
  float farLuma = luma(candidateFar);
  return (farLuma < minimum || farLuma > maximum) ? candidateNear : candidateFar;
}

vec3 bloomNeighborhood(vec2 uv) {
  if (mod(post.texelFlags.z,2.0) < 0.5 || post.bloom.y <= 0.0) return vec3(0.0);
  vec2 px = post.texelFlags.xy * 2.0;
  vec3 sum = vec3(0.0);
  const vec2 offsets[8] = vec2[8](
    vec2(-1,-1),vec2(0,-1),vec2(1,-1),vec2(-1,0),
    vec2(1,0),vec2(-1,1),vec2(0,1),vec2(1,1));
  for (int index = 0; index < 8; ++index) {
    vec3 value = sampleScene(uv + offsets[index] * px);
    sum += value * smoothstep(post.bloom.x, post.bloom.x + 0.25, luma(value));
  }
  return sum * (post.bloom.y / 8.0);
}

vec3 linearToSrgb(vec3 color) {
  return mix(12.92 * color, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec3(0.0031308)));
}

#include "tone_mapping.glsl"
vec3 toneMap(vec3 color) {
  float adaptation=automaticExposure.valid!=0u?automaticExposure.ev:0.0;
  color=max(color*environment.parameters.x*exp2(environment.scenePost.x+adaptation),vec3(0.0));
  return astraToneMap(color,environment.scenePost.y);
}

vec3 displayLinearColor(vec3 color) {
  if(environment.scenePost.z>0.5) color=toneMap(color);
  float gray=luma(color);
  color=mix(vec3(gray),color,post.grade.y);
  color=(color-0.5)*post.grade.x+0.5;
  if(post.bloom.w>0.0) {
    vec2 centered=(vUv-environment.postViewport.xy)/environment.postViewport.zw*2.0-1.0;
    float vignette=1.0-smoothstep(0.25,1.25,dot(centered,centered));
    color*=mix(1.0,vignette,post.bloom.w);
  }
  return max(color,vec3(0));
}

#if AETHER_TEMPORAL
#include "temporal_clipping.glsl"
vec3 srgbToLinear(vec3 color) {
  return mix(color/12.92,pow((max(color,vec3(0))+.055)/1.055,vec3(2.4)),
             greaterThan(color,vec3(.04045)));
}
mat3 cameraRotation(float yaw, float pitch, float roll) {
  float cy=cos(yaw), sy=sin(yaw), cp=cos(pitch), sp=sin(pitch);
  float cr=cos(roll), sr=sin(roll);
  return mat3(cy,0,-sy, 0,1,0, sy,0,cy) *
         mat3(1,0,0, 0,cp,sp, 0,-sp,cp) *
         mat3(cr,sr,0, -sr,cr,0, 0,0,1);
}

vec4 unpackSurfaceTransform(int packed) {
  return vec4(float((packed >> 0) & 3) - 1.0,
              float((packed >> 2) & 3) - 1.0,
              float((packed >> 4) & 3) - 1.0,
              float((packed >> 6) & 3) - 1.0);
}

// Alpha in the resolved-history attachment stores logarithmic view depth.
// The presentation attachment keeps opaque alpha; only temporal reprojection
// reads this value. Logarithmic encoding gives the UNORM8 history useful
// precision across both nearby geometry and distant scenery.
float temporalEncodeDepth(float viewZ, float nearPlane, float farPlane) {
  return clamp(log2(max(viewZ / nearPlane, 1.0)) /
                   log2(max(farPlane / nearPlane, 1.0001)), 0.0, 1.0);
}

vec3 temporalResolve(vec2 sourceUv, vec3 current, float ao, vec3 bloomColor,
                     out float encodedDepth, out float historyContribution,
                     out float rejectedDepth, out vec2 motionUv) {
  // Mode 1 is spatial AA when history is invalid (first frame/cut/moving draw),
  // mode 3 has a valid copied resolve. Invalid frames are rendered without
  // jitter and seed the next history; they never sample uninitialized color.
  float depth = texture(sceneDepth, sourceUv).r;
  float nearPlane = post.currentPositionNear.w;
  float farPlane = post.previousPositionFar.w;
  float denominator = farPlane - depth * (farPlane - nearPlane);
  encodedDepth = depth >= 0.999999 ? 1.0 :
      temporalEncodeDepth(nearPlane * farPlane / max(denominator, 1.0e-6),
                          nearPlane, farPlane);
  historyContribution=0.0;
  rejectedDepth=0.0;
  motionUv=vec2(0.0);
  if (mod(post.texelFlags.w,4.0) < 2.5) return current;
  if (denominator <= 1.0e-6) return current;
  float viewZ = nearPlane * farPlane / denominator;

  int packed = int(floor(post.grade.w));
  vec4 surface = unpackSurfaceTransform(packed);
  vec2 physicalNdc = (vUv-environment.postViewport.xy)/environment.postViewport.zw *
      2.0 - 1.0 - post.currentCamera.zw;
  vec2 cameraNdc = vec2(dot(surface.xz, physicalNdc),
                        dot(surface.yw, physicalNdc));
  float focal=post.sourceTransform.z;
  vec3 viewPosition = vec3(cameraNdc.x * post.sourceTransform.w * viewZ / focal,
                           -cameraNdc.y * viewZ / focal, viewZ);
  vec3 worldPosition = post.currentPositionNear.xyz +
                       cameraRotation(post.currentCamera.x, post.currentCamera.y, post.previousCamera.z) * viewPosition;
  vec3 previousView = transpose(cameraRotation(post.previousCamera.x,
                                                 post.previousCamera.y, post.previousCamera.w)) *
                      (worldPosition - post.previousPositionFar.xyz);
  if(depth>=0.999999) {
    // The infinite sky responds to rotation only, never camera translation.
    previousView=transpose(cameraRotation(post.previousCamera.x,post.previousCamera.y,post.previousCamera.w))*
        cameraRotation(post.currentCamera.x,post.currentCamera.y,post.previousCamera.z)*viewPosition;
  }
  if (previousView.z <= nearPlane) return current;
  vec2 previousCameraNdc = vec2(previousView.x * focal /
                                    (post.sourceTransform.w * previousView.z),
                                -previousView.y * focal / previousView.z);
  vec2 previousPhysicalNdc = vec2(dot(surface.xy, previousCameraNdc),
                                  dot(surface.zw, previousCameraNdc)) +
                             post.currentCamera.zw;
  // History stores resolved output on a stable pixel grid. Reprojection is
  // previousStable-currentStable, anchored at this output pixel: add CURRENT
  // jitter to cancel its subtraction above. Previous jitter makes a static
  // camera oscillate by jPrevious-jCurrent on every Halton sample.
  vec2 previousLocalUv = previousPhysicalNdc * 0.5 + 0.5;
  if (any(lessThan(previousLocalUv, vec2(0.0))) ||
      any(greaterThan(previousLocalUv, vec2(1.0)))) return current;
  vec2 previousUv=environment.postViewport.xy+previousLocalUv*environment.postViewport.zw;
  // O vetor unificado manda: câmera e objetos saem do mesmo passe que as
  // bibliotecas consomem. A reprojeção só de câmera acima continua dando a
  // profundidade esperada; onde o vetor diverge dela, o pixel pertence a um
  // objeto em movimento, cuja profundidade anterior a câmera não descreve.
  bool objectMotion=false;
  if(postFlag(8.0)) {
    vec2 velocityUv=vUv+texture(temporalVelocity,sourceUv).xy;
    objectMotion=length((velocityUv-previousUv)/max(post.texelFlags.xy,vec2(1.0e-6)))>0.5;
    if(any(lessThan(velocityUv,environment.postViewport.xy))||
       any(greaterThan(velocityUv,environment.postViewport.xy+environment.postViewport.zw))) return current;
    previousUv=velocityUv;
  }
  motionUv=previousUv-vUv;

  // FSR runs this resolve at the current internal extent, in the top-left of
  // the full-size history allocation. The rest of the image is never read.
  vec2 historyScale=mod(floor(post.texelFlags.z/4.0),2.0)>0.5
      ? post.sourceTransform.xy/post.texelFlags.xy/vec2(textureSize(historyColor,0))
      : vec2(1.0);
  previousUv*=historyScale;
  vec2 historyHalfTexel=0.5/vec2(textureSize(historyColor,0));
  previousUv=clamp(previousUv,environment.postViewport.xy*historyScale+historyHalfTexel,
      (environment.postViewport.xy+environment.postViewport.zw)*historyScale-historyHalfTexel);
  // A color match is insufficient at disocclusions: a newly visible surface
  // may have nearly the same color as the old occluder. Reject its history
  // when the old pixel's depth differs from the reprojected surface depth.
  // Fetch alpha without bilinear filtering across a depth edge.
  ivec2 historyExtent=textureSize(historyColor,0);
  ivec2 historyPixel=clamp(ivec2(previousUv*vec2(historyExtent)),
                          ivec2(0),historyExtent-ivec2(1));
  float previousDepth=texelFetch(historyColor,historyPixel,0).a;
  float expectedDepth=depth>=0.999999 ? 1.0 :
      temporalEncodeDepth(previousView.z,nearPlane,farPlane);
  // ~0.02 in log depth tolerates 8-bit quantization and raster edge samples
  // while rejecting a different surface at materially different depth.
  // A stationary depth edge alternates foreground/background under projection
  // jitter. There is no newly exposed surface when camera and depth-writing
  // geometry are unchanged, so this mismatch must not discard its history.
  bool staticCoverage=mod(floor(post.texelFlags.z/16.0),2.0)>0.5;
  if(!staticCoverage && !objectMotion && abs(previousDepth-expectedDepth)>0.02) {
    rejectedDepth=1.0;
    return current;
  }
  vec3 history = texture(historyColor, previousUv).rgb;
  // An sRGB history view decodes in hardware; UNORM contains explicitly
  // encoded presentation bytes. Resolve and clip in display-linear color
  // in both cases, before the final output transfer function.
  if(post.grade.z>0.5) history=srgbToLinear(history);
  vec3 currentY=temporalYCoCg(current);
  vec3 lower=currentY,upper=currentY;
  for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
    vec3 value=displayLinearColor(sampleScene(sourceUv+vec2(x,y)*post.texelFlags.xy)*ao+bloomColor);
    vec3 converted=temporalYCoCg(value);
    lower=min(lower,converted);upper=max(upper,converted);
  }
  history=temporalRgb(temporalClipAabb(lower,upper,temporalYCoCg(history)));
  float historyWeight = fract(post.grade.w);
  float motionPixels = length((previousUv-vUv*historyScale)*vec2(textureSize(historyColor,0)));
  historyWeight *= exp2(-motionPixels * 0.035);
  // Transparentes e água mudam de cor sem escrever profundidade nem vetor.
  // As máscaras do passe principal dizem quanto; sem elas, a cobertura opaca
  // restante em sceneColor.a é a aproximação anterior.
  float reactive=postFlag(32.0)
      ? max(texture(reactiveMask,sourceUv).r,0.5*texture(compositionMask,sourceUv).r)
      : clamp(1.0-texture(sceneColor,sourceUv).a,0.0,1.0);
  historyWeight *= 1.0-reactive;
  historyWeight *= 1.0 - smoothstep(0.08, 0.35, abs(luma(history) - luma(current)));
  historyContribution=clamp(historyWeight, 0.0, 0.97);
  return mix(current, history, historyContribution);
}
#endif

float filmGrainNoise(vec2 pixel) {
  // A câmera e o jitter variam a semente sem exigir outro campo no ABI de
  // push constants. Sem TAA o padrão permanece estável quando a vista para,
  // evitando cintilação gratuita em aparelhos de atualização baixa.
  vec2 seed=post.currentCamera.xy*37.0+post.currentCamera.zw*8192.0;
  return fract(sin(dot(pixel+seed,vec2(12.9898,78.233)))*43758.5453);
}

// Diagnóstico temporal (G6-B): mostra o valor que o consumidor lê, não uma
// recomputação. Vale no TAA nativo e depois dos ampliadores temporais.
bool temporalDiagnostic(vec2 sourceUv, float encodedDepth, float historyContribution,
                        float rejectedDepth, out vec3 diagnostic) {
  int view=int(floor(post.texelFlags.w*0.25));
  if(view==0) return false;
  vec2 inputUv=temporalInputUv(vUv);
  vec2 texel=vec2(textureSize(sceneDepth,0));
  if(view==1) {
    float depth=texture(sceneDepth,inputUv).r;
    float nearPlane=post.currentPositionNear.w,farPlane=post.previousPositionFar.w;
    float viewZ=nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
    diagnostic=vec3(depth>=0.999999?1.0:clamp(log2(max(viewZ/nearPlane,1.0))/
                                                   log2(max(farPlane/nearPlane,1.0001)),0.0,1.0));
  } else if(view==2) diagnostic=vec3(0,historyContribution,0);
  else if(view==3) diagnostic=vec3(rejectedDepth,0,0);
  else if(view==4) {
    // Pixels de movimento em cor: 16 px por quadro satura. Azul fixo separa
    // "parado" (cinza-azulado) de ausência de dados.
    vec2 pixels=texture(temporalVelocity,inputUv).xy*texel/max(environment.temporalParameters.zw,vec2(1.0e-6));
    diagnostic=vec3(clamp(vec2(0.5)+pixels/32.0,0.0,1.0),0.5);
  } else if(view==5) {
    float reactive=postFlag(32.0)?texture(reactiveMask,inputUv).r:
        clamp(1.0-texture(sceneColor,sourceUv).a,0.0,1.0);
    diagnostic=vec3(reactive,0,reactive);
  } else diagnostic=vec3(0,postFlag(32.0)?texture(compositionMask,inputUv).r:0.0,
                         postFlag(32.0)?texture(compositionMask,inputUv).r:0.0);
  return true;
}

void main() {
  if(postFlag(64.0)) {
    // Composição antes do ampliador temporal: névoa e AO em HDR linear, na
    // resolução interna, alinhadas à profundidade que as produziu. Tonemap,
    // bloom, grade e vinheta ficam para depois da ampliação.
    vec2 sourceUv=clampSourceUv(vUv*post.sourceTransform.xy);
    outColor=vec4(sampleScene(sourceUv)*screenSpaceAmbientOcclusion(sourceUv),1.0);
    return;
  }
  if(any(lessThan(vUv,environment.postViewport.xy)) ||
     any(greaterThan(vUv,environment.postViewport.xy+environment.postViewport.zw))) {
    // UI is composited afterwards. Preserve the clear outside the camera;
    // neither reconstruction nor history may spread its edge across panels.
    vec3 background=texture(sceneColor,vUv*post.sourceTransform.xy).rgb;
    outColor=vec4(post.grade.z>.5?linearToSrgb(background):background,1);
#if AETHER_TEMPORAL
    outHistory=outColor;
#endif
    return;
  }
  vec2 sourceUv = clampSourceUv(vUv * post.sourceTransform.xy);
  vec3 unfiltered;
  vec3 crossAverage;
  vec3 center = sampleFxaa(sourceUv, unfiltered, crossAverage);
  if(mod(floor(post.texelFlags.z/2.0),2.0)>0.5) {
    // Preserve the chosen AA correction and reconstruct only the centre,
    // rather than multiplying every FXAA/bloom neighbourhood by nine taps.
    center=max(vec3(0),center+reconstructScene(sourceUv)-unfiltered);
  }
  float ao=screenSpaceAmbientOcclusion(sourceUv);
  center *= ao;
  vec3 bloomColor=bloomNeighborhood(sourceUv);
  vec3 color = center + bloomColor;
  color=displayLinearColor(color);
#if AETHER_TEMPORAL
  float encodedDepth;
  float historyContribution,rejectedDepth;
  vec2 motionUv;
  color = temporalResolve(sourceUv, color, ao, bloomColor,
                          encodedDepth, historyContribution, rejectedDepth,motionUv);
  outHistory=vec4(post.grade.z>.5?linearToSrgb(color):color,encodedDepth);
  // Editor diagnostics inspect the actual consumer values. The normal color
  // and depth still go into history, so changing view never poisons a frame.
  vec3 diagnostic;
  if(temporalDiagnostic(sourceUv,encodedDepth,historyContribution,rejectedDepth,diagnostic)) {
    outColor=vec4(diagnostic,1);
    return;
  }
#else
  vec3 diagnostic;
  if(temporalDiagnostic(sourceUv,0.0,0.0,0.0,diagnostic)) {
    outColor=vec4(post.grade.z>.5?linearToSrgb(diagnostic):diagnostic,1);
    return;
  }
#endif
  // Presentation detail never feeds back into temporal reconstruction.
  if (post.bloom.z > 0.0)
    color += (displayLinearColor(unfiltered*ao+bloomColor)-
              displayLinearColor(crossAverage*ao+bloomColor))*post.bloom.z;
  float grainIntensity=environment.sceneAoDetail.y;
  if(grainIntensity>0.0 && mod(floor(post.texelFlags.z/4.0),2.0)<0.5) {
    float response=0.35+0.65*(1.0-abs(luma(color)*2.0-1.0));
    color+=(filmGrainNoise(gl_FragCoord.xy)-0.5)*grainIntensity*response;
  }
  if (post.grade.z > 0.5) color = linearToSrgb(max(color,vec3(0)));
  outColor = vec4(max(color, vec3(0.0)), 1.0);
}
