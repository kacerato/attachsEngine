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

vec3 sampleFxaa(vec2 uv, out vec3 unfiltered, out vec3 crossAverage) {
  vec2 px = post.texelFlags.xy;
  vec3 center = texture(sceneColor, uv).rgb;
  unfiltered = center;
  crossAverage = center;
  if (post.texelFlags.w < 0.5 && post.bloom.z <= 0.0) return center;
  vec3 north = texture(sceneColor, uv + vec2(0.0, -px.y)).rgb;
  vec3 south = texture(sceneColor, uv + vec2(0.0,  px.y)).rgb;
  vec3 west  = texture(sceneColor, uv + vec2(-px.x, 0.0)).rgb;
  vec3 east  = texture(sceneColor, uv + vec2( px.x, 0.0)).rgb;
  crossAverage = (north + south + west + east) * 0.25;
  if (post.texelFlags.w < 0.5) return center;
  float minimum = min(luma(center), min(min(luma(north), luma(south)), min(luma(west), luma(east))));
  float maximum = max(luma(center), max(max(luma(north), luma(south)), max(luma(west), luma(east))));
  float edge = smoothstep(0.025, 0.125, maximum - minimum);
  return mix(center, crossAverage, edge * 0.65);
}

vec3 bloomNeighborhood(vec2 uv) {
  if (post.texelFlags.z < 0.5 || post.bloom.y <= 0.0) return vec3(0.0);
  vec2 px = post.texelFlags.xy * 2.0;
  vec3 sum = vec3(0.0);
  const vec2 offsets[8] = vec2[8](
    vec2(-1,-1),vec2(0,-1),vec2(1,-1),vec2(-1,0),
    vec2(1,0),vec2(-1,1),vec2(0,1),vec2(1,1));
  for (int index = 0; index < 8; ++index) {
    vec3 value = texture(sceneColor, uv + offsets[index] * px).rgb;
    sum += value * smoothstep(post.bloom.x, post.bloom.x + 0.25, luma(value));
  }
  return sum * (post.bloom.y / 8.0);
}

vec3 linearToSrgb(vec3 color) {
  return mix(12.92 * color, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec3(0.0031308)));
}

void main() {
  vec2 sourceUv = clamp(vUv * post.sourceTransform.xy, post.texelFlags.xy * 0.5,
                        post.sourceTransform.xy - post.texelFlags.xy * 0.5);
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
