precision highp float;
precision highp int;

layout(set=0,binding=0) uniform sampler2D sceneColor;
#if AETHER_TEMPORAL
layout(set=0,binding=1) uniform sampler2D historyColor;
layout(set=0,binding=2) uniform sampler2D sceneDepth;
#endif
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

vec3 sampleScene(vec2 uv) { return texture(sceneColor, clampSourceUv(uv)).rgb; }

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

void main() {
  vec2 sourceUv = clampSourceUv(vUv * post.sourceTransform.xy);
  vec3 unfiltered;
  vec3 crossAverage;
  vec3 center = sampleFxaa(sourceUv, unfiltered, crossAverage);
  vec3 color = center + bloomNeighborhood(sourceUv);
  if (post.bloom.z > 0.0) color += (unfiltered-crossAverage)*post.bloom.z;
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
  outColor = vec4(max(color, vec3(0.0)), 1.0);
}
