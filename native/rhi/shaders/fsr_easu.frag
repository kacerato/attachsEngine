#version 450
#extension GL_GOOGLE_include_directive : require

// Adapter for AMD FidelityFX Super Resolution 1.0.2 EASU. The upstream
// implementation and its MIT license are pinned in third_party/fidelityfx_fsr1.
layout(set = 0, binding = 0) uniform sampler2D sourceImage;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform FsrPushConstants {
  vec4 sourceSize;   // active width/height, allocated width/height
  vec4 outputSize;   // native width/height, sharpen amount, source sampler is sRGB
  vec4 viewport;     // normalized x, y, width, height in source and destination
  vec4 presentation;// grain seed x/y, intensity, output attachment is sRGB
} fsr;

#define A_GPU 1
#define A_GLSL 1
#include "../../third_party/fidelityfx_fsr1/ffx_a.h"
#define FSR_EASU_F 1

ivec2 astraSourceMinimum;
ivec2 astraSourceMaximum;
ivec2 astraSourceAllocated;

vec3 astraLinearToSrgb(vec3 color) {
  return mix(12.92 * color,
             1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec3(0.0031308)));
}

vec4 astraLinearToSrgb(vec4 color) {
  return mix(12.92 * color,
             1.055 * pow(max(color, vec4(0.0)), vec4(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec4(0.0031308)));
}

vec4 astraFsrSource(ivec2 pixel) {
  pixel = clamp(pixel, astraSourceMinimum, astraSourceMaximum - ivec2(1));
  vec4 value = texelFetch(sourceImage, pixel, 0);
  // An sRGB image view is decoded by the sampler. EASU deliberately operates
  // after tone mapping in the perceptual display domain, so restore that curve.
  if (fsr.outputSize.w > 0.5) value.rgb = astraLinearToSrgb(value.rgb);
  return value;
}

// AMD's reference path uses textureGather. Fetch the same four footprint
// texels explicitly so every tap can be clamped to the active camera viewport;
// this prevents stale allocation pixels or editor panels bleeding at its edge.
AF4 astraFsrGather(AF2 uv, int channel) {
  ivec2 base = ivec2(floor(uv * vec2(astraSourceAllocated) - vec2(0.5)));
  if (all(greaterThanEqual(base, astraSourceMinimum)) &&
      all(lessThan(base + ivec2(1), astraSourceMaximum))) {
    vec4 gathered;
    if (channel == 0) gathered = textureGather(sourceImage, uv, 0);
    else if (channel == 1) gathered = textureGather(sourceImage, uv, 1);
    else gathered = textureGather(sourceImage, uv, 2);
    if (fsr.outputSize.w > 0.5) gathered = astraLinearToSrgb(gathered);
    return AF4(gathered);
  }
  vec4 p0 = astraFsrSource(base + ivec2(0, 1));
  vec4 p1 = astraFsrSource(base + ivec2(1, 1));
  vec4 p2 = astraFsrSource(base + ivec2(1, 0));
  vec4 p3 = astraFsrSource(base);
  return AF4(p0[channel], p1[channel], p2[channel], p3[channel]);
}

vec3 astraUnscaledPixel(ivec2 pixel, ivec2 nativeSize, ivec2 activeSize) {
  ivec2 sourcePixel = ivec2(floor((vec2(pixel) + vec2(0.5)) *
                                  vec2(activeSize) / vec2(nativeSize)));
  astraSourceMinimum = ivec2(0);
  astraSourceMaximum = activeSize;
  return astraFsrSource(sourcePixel).rgb;
}

AF4 FsrEasuRF(AF2 p) { return astraFsrGather(p, 0); }
AF4 FsrEasuGF(AF2 p) { return astraFsrGather(p, 1); }
AF4 FsrEasuBF(AF2 p) { return astraFsrGather(p, 2); }

#include "../../third_party/fidelityfx_fsr1/ffx_fsr1.h"

void main() {
  ivec2 nativeSize = max(ivec2(fsr.outputSize.xy + vec2(0.5)), ivec2(1));
  ivec2 activeSize = clamp(ivec2(fsr.sourceSize.xy + vec2(0.5)), ivec2(1),
                           max(ivec2(fsr.sourceSize.zw + vec2(0.5)), ivec2(1)));
  astraSourceAllocated = max(ivec2(fsr.sourceSize.zw + vec2(0.5)), activeSize);
  ivec2 pixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), nativeSize - ivec2(1));
  vec4 region = vec4(clamp(fsr.viewport.xy, vec2(0.0), vec2(1.0)),
                     clamp(fsr.viewport.zw, vec2(0.0), vec2(1.0)));
  region.zw = min(region.zw, vec2(1.0) - region.xy);
  if (any(lessThanEqual(region.zw, vec2(0.0)))) {
    outColor = vec4(astraUnscaledPixel(pixel, nativeSize, activeSize), 1.0);
    return;
  }
  ivec2 outputMinimum = ivec2(floor(region.xy * vec2(nativeSize)));
  outputMinimum = clamp(outputMinimum, ivec2(0), nativeSize - ivec2(1));
  ivec2 outputMaximum = ivec2(ceil((region.xy + region.zw) * vec2(nativeSize)));
  outputMaximum = clamp(outputMaximum, outputMinimum + ivec2(1), nativeSize);
  if (any(lessThan(pixel, outputMinimum)) || any(greaterThanEqual(pixel, outputMaximum))) {
    outColor = vec4(astraUnscaledPixel(pixel, nativeSize, activeSize), 1.0);
    return;
  }

  astraSourceMinimum = ivec2(floor(region.xy * vec2(activeSize)));
  astraSourceMinimum = clamp(astraSourceMinimum, ivec2(0), activeSize - ivec2(1));
  astraSourceMaximum = ivec2(ceil((region.xy + region.zw) * vec2(activeSize)));
  astraSourceMaximum = clamp(astraSourceMaximum, astraSourceMinimum + ivec2(1), activeSize);
  ivec2 inputExtent = astraSourceMaximum - astraSourceMinimum;
  ivec2 outputExtent = outputMaximum - outputMinimum;
  AU4 con0, con1, con2, con3;
  FsrEasuConOffset(con0, con1, con2, con3,
                   float(inputExtent.x), float(inputExtent.y),
                   float(astraSourceAllocated.x), float(astraSourceAllocated.y),
                   float(outputExtent.x), float(outputExtent.y),
                   float(astraSourceMinimum.x), float(astraSourceMinimum.y));
  AF3 color;
  FsrEasuF(color, AU2(pixel - outputMinimum), con0, con1, con2, con3);
  outColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
