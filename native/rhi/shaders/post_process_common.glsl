precision highp float;
precision highp int;

layout(set=0,binding=0) uniform sampler2D sceneColor;
layout(set=0,binding=1) uniform sampler2D historyColor;
layout(set=0,binding=2) uniform sampler2D sceneDepth;
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
} environment;
layout(push_constant) uniform PostPushConstants {
  vec4 texelFlags; // xy=1/source extent, z=bloom, w=AA mode/history state
  vec4 bloom;      // threshold, intensity, sharpen, vignette intensity
  vec4 grade;      // contrast, saturation, encode sRGB, packed transform+feedback
  vec4 sourceTransform; // xy=active/source extent, z=focal length, w=display aspect
  vec4 currentCamera; // yaw, pitch, physical-NDC jitter x/y
  vec4 currentPositionNear; // camera xyz, near plane
  vec4 previousCamera; // yaw, pitch, previous physical-NDC jitter x/y
  vec4 previousPositionFar; // camera xyz, far plane
} post;
layout(location=0) in highp vec2 vUv;
layout(location=0) out vec4 outColor;

float luma(vec3 value) { return dot(value, vec3(0.2126, 0.7152, 0.0722)); }

vec2 clampSourceUv(vec2 uv) {
  return clamp(uv, post.texelFlags.xy * 0.5,
               post.sourceTransform.xy - post.texelFlags.xy * 0.5);
}

float viewDistance(vec2 uv) {
  float depth=texture(sceneDepth,clampSourceUv(uv)).r;
  float nearPlane=post.currentPositionNear.w;
  float farPlane=post.previousPositionFar.w;
  if(post.sourceTransform.z<0.0) return mix(nearPlane,farPlane,depth);
  return nearPlane*farPlane/max(farPlane-depth*(farPlane-nearPlane),1.0e-6);
}

vec3 sampleScene(vec2 uv) {
  vec2 sourceUv=clampSourceUv(uv);
  vec3 color=texture(sceneColor,sourceUv).rgb;
  if(environment.sceneFog.x>0.5) {
    float distance=max(0.0,viewDistance(sourceUv)-environment.sceneFog.y);
    float fog=1.0-exp(-environment.sceneFogColorDensity.w*distance);
    color=mix(color,environment.sceneFogColorDensity.rgb,clamp(fog,0.0,1.0));
  }
  return color;
}

float screenSpaceAmbientOcclusion(vec2 uv) {
  if(environment.sceneAo.x<0.5) return 1.0;
  float rawDepth=texture(sceneDepth,clampSourceUv(uv)).r;
  if(rawDepth>=0.999999) return 1.0;
  float center=viewDistance(uv);
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
    float sampleDistance=viewDistance(uv+offset);
    float delta=center-sampleDistance;
    float range=max(0.0,1.0-abs(delta)/radius);
    occlusion+=step(environment.sceneAoDetail.x,delta)*range;
  }
  float visibility=clamp(1.0-occlusion*(environment.sceneAo.z/12.0),0.0,1.0);
  return pow(visibility,max(environment.sceneAo.w,0.1));
}

vec3 sampleFxaa(vec2 uv, out vec3 unfiltered, out vec3 crossAverage) {
  vec2 px = post.texelFlags.xy;
  vec3 center = sampleScene(uv);
  unfiltered = center;
  crossAverage = center;
  if (post.bloom.z > 0.0 || post.texelFlags.w >= 2.0) {
    vec3 north = sampleScene(uv + vec2(0.0, -px.y));
    vec3 south = sampleScene(uv + vec2(0.0,  px.y));
    vec3 west  = sampleScene(uv + vec2(-px.x, 0.0));
    vec3 east  = sampleScene(uv + vec2( px.x, 0.0));
    crossAverage = (north + south + west + east) * 0.25;
  }
  if (post.texelFlags.w < 0.5 || post.texelFlags.w > 1.5) return center;

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
  if (post.texelFlags.z < 0.5 || post.bloom.y <= 0.0) return vec3(0.0);
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

vec3 toneMap(vec3 color) {
  color=max(color*environment.parameters.x*exp2(environment.scenePost.x),vec3(0.0));
  if(environment.scenePost.y<0.5) return color/(vec3(1.0)+color);
  return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
}

#if AETHER_TEMPORAL
mat3 cameraRotation(float yaw, float pitch) {
  float cy=cos(yaw), sy=sin(yaw), cp=cos(pitch), sp=sin(pitch);
  return mat3(cy,0,-sy, 0,1,0, sy,0,cy) *
         mat3(1,0,0, 0,cp,sp, 0,-sp,cp);
}

vec4 unpackSurfaceTransform(int packed) {
  return vec4(float((packed >> 0) & 3) - 1.0,
              float((packed >> 2) & 3) - 1.0,
              float((packed >> 4) & 3) - 1.0,
              float((packed >> 6) & 3) - 1.0);
}

vec3 temporalResolve(vec2 sourceUv, vec3 current, vec3 unfiltered, vec3 crossAverage) {
  // Mode 2 is temporal with invalid history (first frame/cut), mode 3 has a
  // valid copied resolve. Keeping this state explicit avoids sampling an image
  // that is still in UNDEFINED layout.
  if (post.texelFlags.w < 2.5) return current;
  float depth = texture(sceneDepth, sourceUv).r;
  float nearPlane = post.currentPositionNear.w;
  float farPlane = post.previousPositionFar.w;
  float denominator = farPlane - depth * (farPlane - nearPlane);
  if (denominator <= 1.0e-6) return current;
  float viewZ = nearPlane * farPlane / denominator;

  int packed = int(floor(post.grade.w));
  vec4 surface = unpackSurfaceTransform(packed);
  vec2 physicalNdc = vUv * 2.0 - 1.0 - post.currentCamera.zw;
  vec2 cameraNdc = vec2(dot(surface.xz, physicalNdc),
                        dot(surface.yw, physicalNdc));
  float focal=post.sourceTransform.z;
  vec3 viewPosition = vec3(cameraNdc.x * post.sourceTransform.w * viewZ / focal,
                           -cameraNdc.y * viewZ / focal, viewZ);
  vec3 worldPosition = post.currentPositionNear.xyz +
                       cameraRotation(post.currentCamera.x, post.currentCamera.y) * viewPosition;
  vec3 previousView = transpose(cameraRotation(post.previousCamera.x,
                                                 post.previousCamera.y)) *
                      (worldPosition - post.previousPositionFar.xyz);
  if (previousView.z <= nearPlane) return current;
  vec2 previousCameraNdc = vec2(previousView.x * focal /
                                    (post.sourceTransform.w * previousView.z),
                                -previousView.y * focal / previousView.z);
  vec2 previousPhysicalNdc = vec2(dot(surface.xy, previousCameraNdc),
                                  dot(surface.zw, previousCameraNdc)) +
                             post.previousCamera.zw;
  vec2 previousUv = previousPhysicalNdc * 0.5 + 0.5;
  if (any(lessThan(previousUv, vec2(0.0))) ||
      any(greaterThan(previousUv, vec2(1.0)))) return current;

  vec3 history = texture(historyColor, previousUv).rgb;
  // Variance-style clipping from the current cross prevents disocclusion
  // trails without a full 3x3 neighborhood. Motion and luma disagreement then
  // reduce feedback continuously instead of toggling history frame-to-frame.
  vec3 extent = max(vec3(0.035), abs(unfiltered - crossAverage) * 2.0 +
                                  abs(current - unfiltered) * 0.25);
  history = clamp(history, current - extent, current + extent);
  float historyWeight = fract(post.grade.w);
  float motionPixels = length((previousUv - vUv) / max(post.texelFlags.xy, vec2(1.0e-6)));
  historyWeight *= exp2(-motionPixels * 0.035);
  historyWeight *= 1.0 - smoothstep(0.08, 0.35, abs(luma(history) - luma(current)));
  return mix(current, history, clamp(historyWeight, 0.0, 0.97));
}
#endif

float filmGrainNoise(vec2 pixel) {
  // A câmera e o jitter variam a semente sem exigir outro campo no ABI de
  // push constants. Sem TAA o padrão permanece estável quando a vista para,
  // evitando cintilação gratuita em aparelhos de atualização baixa.
  vec2 seed=post.currentCamera.xy*37.0+post.currentCamera.zw*8192.0;
  return fract(sin(dot(pixel+seed,vec2(12.9898,78.233)))*43758.5453);
}

void main() {
  vec2 sourceUv = clampSourceUv(vUv * post.sourceTransform.xy);
  vec3 unfiltered;
  vec3 crossAverage;
  vec3 center = sampleFxaa(sourceUv, unfiltered, crossAverage);
  center *= screenSpaceAmbientOcclusion(sourceUv);
  vec3 color = center + bloomNeighborhood(sourceUv);
  if (post.bloom.z > 0.0) color += (unfiltered-crossAverage)*post.bloom.z;
  if(environment.scenePost.z>0.5) color=toneMap(color);
  float gray = luma(color);
  color = mix(vec3(gray), color, post.grade.y);
  color = (color - 0.5) * post.grade.x + 0.5;
  if (post.bloom.w > 0.0) {
    vec2 centered = vUv * 2.0 - 1.0;
    float vignette = smoothstep(1.25, 0.25, dot(centered, centered));
    color *= mix(1.0, vignette, post.bloom.w);
  }
  if (post.grade.z > 0.5) color = linearToSrgb(color);
#if AETHER_TEMPORAL
  color = temporalResolve(sourceUv, color, unfiltered, crossAverage);
#endif
  float grainIntensity=environment.sceneAoDetail.y;
  if(grainIntensity>0.0) {
    float response=0.35+0.65*(1.0-abs(luma(color)*2.0-1.0));
    color+=(filmGrainNoise(gl_FragCoord.xy)-0.5)*grainIntensity*response;
  }
  outColor = vec4(max(color, vec3(0.0)), 1.0);
}
