#version 450
#extension GL_GOOGLE_include_directive : require

// Adapter for AMD FidelityFX Super Resolution 1.0.2 RCAS. Input is the
// perceptual UNORM result of EASU; film grain is applied after sharpening.
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
#define FSR_RCAS_F 1

ivec2 astraOutputMinimum;
ivec2 astraOutputMaximum;

AF4 FsrRcasLoadF(ASU2 pixel) {
  return AF4(texelFetch(sourceImage,
                        clamp(ivec2(pixel), astraOutputMinimum,
                              astraOutputMaximum - ivec2(1)), 0));
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

#include "../../third_party/fidelityfx_fsr1/ffx_fsr1.h"

vec3 astraSrgbToLinear(vec3 color) {
  return mix(color / 12.92,
             pow((max(color, vec3(0.0)) + 0.055) / 1.055, vec3(2.4)),
             greaterThan(color, vec3(0.04045)));
}

vec3 astraLinearToSrgb(vec3 color) {
  return mix(12.92 * color,
             1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
             greaterThan(color, vec3(0.0031308)));
}

float astraLuma(vec3 color) { return dot(color, vec3(0.2126, 0.7152, 0.0722)); }

float astraFilmGrain(vec2 pixel) {
  return fract(sin(dot(pixel + fsr.presentation.xy,
                       vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 astraPresentCopy(vec3 perceptual) {
  return fsr.presentation.w > 0.5
      ? astraSrgbToLinear(clamp(perceptual, vec3(0.0), vec3(1.0)))
      : perceptual;
}

void main() {
  ivec2 nativeSize = max(ivec2(fsr.outputSize.xy + vec2(0.5)), ivec2(1));
  vec4 region = vec4(clamp(fsr.viewport.xy, vec2(0.0), vec2(1.0)),
                     clamp(fsr.viewport.zw, vec2(0.0), vec2(1.0)));
  region.zw = min(region.zw, vec2(1.0) - region.xy);
  ivec2 pixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), nativeSize - ivec2(1));
  if (any(lessThanEqual(region.zw, vec2(0.0)))) {
    outColor = vec4(astraPresentCopy(texelFetch(sourceImage, pixel, 0).rgb), 1.0);
    return;
  }
  astraOutputMinimum = ivec2(floor(region.xy * vec2(nativeSize)));
  astraOutputMinimum = clamp(astraOutputMinimum, ivec2(0), nativeSize - ivec2(1));
  astraOutputMaximum = ivec2(ceil((region.xy + region.zw) * vec2(nativeSize)));
  astraOutputMaximum = clamp(astraOutputMaximum, astraOutputMinimum + ivec2(1), nativeSize);
  if (any(lessThan(pixel, astraOutputMinimum)) || any(greaterThanEqual(pixel, astraOutputMaximum))) {
    outColor = vec4(astraPresentCopy(texelFetch(sourceImage, pixel, 0).rgb), 1.0);
    return;
  }

  vec3 perceptual;
  float amount = clamp(fsr.outputSize.z, 0.0, 1.0);
  if (amount <= 0.0) {
    perceptual = texelFetch(sourceImage, pixel, 0).rgb;
  } else {
    // AMD converts stops to gain with exp2(-stops). Inverting that mapping makes
    // the public 0..1 intensity continuous: zero tends to identity, one is the
    // strongest RCAS setting, and 0.25 is exactly two stops.
    AU4 con;
    FsrRcasCon(con, -log2(max(amount, 1.0e-6)));
    FsrRcasF(perceptual.r, perceptual.g, perceptual.b, AU2(pixel), con);
  }

  vec3 displayLinear = astraSrgbToLinear(clamp(perceptual, vec3(0.0), vec3(1.0)));
  float grain = clamp(fsr.presentation.z, 0.0, 1.0);
  if (grain > 0.0) {
    float response = 0.35 + 0.65 * (1.0 - abs(astraLuma(displayLinear) * 2.0 - 1.0));
    displayLinear += (astraFilmGrain(gl_FragCoord.xy) - 0.5) * grain * response;
  }
  displayLinear = max(displayLinear, vec3(0.0));
  outColor = vec4(fsr.presentation.w > 0.5
                    ? displayLinear
                    : astraLinearToSrgb(displayLinear), 1.0);
}
