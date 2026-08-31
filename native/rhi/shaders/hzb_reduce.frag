#version 450
// GPU counterpart of ae::renderer::buildHzbPyramid's per-level reduction
// (native/renderer/hzb_visibility.cpp): exact 2x2 max via texelFetch, no
// filtering. Run once per Hi-Z pyramid level above the base (see
// hzb_reduce_first.frag for the real-resolution -> base-level pass).
layout(set = 0, binding = 0) uniform sampler2D previousLevel;

layout(push_constant) uniform HzbReducePushConstants {
  uint previousWidth;
  uint previousHeight;
} pushConstants;

layout(location = 0) out float outMaxDepth;

void main() {
  uint x0 = uint(gl_FragCoord.x) * 2u;
  uint y0 = uint(gl_FragCoord.y) * 2u;
  uint x1 = min(x0 + 1u, pushConstants.previousWidth - 1u);
  uint y1 = min(y0 + 1u, pushConstants.previousHeight - 1u);

  float value = texelFetch(previousLevel, ivec2(x0, y0), 0).r;
  value = max(value, texelFetch(previousLevel, ivec2(x1, y0), 0).r);
  value = max(value, texelFetch(previousLevel, ivec2(x0, y1), 0).r);
  value = max(value, texelFetch(previousLevel, ivec2(x1, y1), 0).r);
  outMaxDepth = value;
}
