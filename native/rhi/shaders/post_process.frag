#version 450

precision highp float;
precision highp int;

layout(set=0,binding=0) uniform sampler2D sceneColor;
layout(push_constant) uniform PostPushConstants {
  vec4 texelFlags; // xy=1/source extent, z=bloom, w=fxaa
  vec4 bloom;      // threshold, intensity, sharpen, vignette intensity
  vec4 grade;      // contrast, saturation, encode sRGB, reserved
  vec4 sourceTransform; // xy=active/source extent, z=render scale
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
  if (post.bloom.z > 0.0) {
    vec3 north = sampleScene(uv + vec2(0.0, -px.y));
    vec3 south = sampleScene(uv + vec2(0.0,  px.y));
    vec3 west  = sampleScene(uv + vec2(-px.x, 0.0));
    vec3 east  = sampleScene(uv + vec2( px.x, 0.0));
    crossAverage = (north + south + west + east) * 0.25;
  }
  if (post.texelFlags.w < 0.5) return center;

  // FXAA 3.x-style edge search. The old implementation blended every high
  // contrast pixel toward a four-tap cross, which softened foliage interiors,
  // texturing and specular highlights as if the whole image had a blur pass.
  // This path first rejects non-edges, then searches along the edge normal and
  // only blends when the candidate luma remains inside the local envelope.
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
  // Relative threshold preserves low-contrast material detail; the absolute
  // floor prevents noise in dark regions from becoming an expensive edge.
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

void main() {
  vec2 sourceUv = clampSourceUv(vUv * post.sourceTransform.xy);
  vec3 unfiltered;
  vec3 crossAverage;
  vec3 center = sampleFxaa(sourceUv, unfiltered, crossAverage);
  vec3 color = center + bloomNeighborhood(sourceUv);
  if (post.bloom.z > 0.0) {
    color += (unfiltered-crossAverage)*post.bloom.z;
  }
  float gray = luma(color);
  color = mix(vec3(gray), color, post.grade.y);
  color = (color - 0.5) * post.grade.x + 0.5;
  if (post.bloom.w > 0.0) {
    vec2 centered = vUv * 2.0 - 1.0;
    float vignette = smoothstep(1.25, 0.25, dot(centered, centered));
    color *= mix(1.0, vignette, post.bloom.w);
  }
  if (post.grade.z > 0.5) color = linearToSrgb(color);
  outColor = vec4(max(color, vec3(0.0)), 1.0);
}
