#version 450
// GPU counterpart of ae::renderer::buildHzbBaseLevelBlockMax (native/renderer/
// hzb_visibility.cpp) -- keep the integer block math identical between the
// two so the CPU function stays a valid reference/validation oracle for this
// shader once hardware is available. Reduces the real (arbitrary, non-power-
// of-two) depth resolution down to the Hi-Z pyramid's base level by taking
// the MAX depth within each output texel's exact source footprint -- a naive
// 2x2 tap here would silently subsample the real resolution and understate
// occluder depth (a conservative-culling correctness bug, not just quality).
layout(set = 0, binding = 0) uniform sampler2D sourceDepth;

layout(push_constant) uniform HzbReduceFirstPushConstants {
  uint sourceWidth;
  uint sourceHeight;
  uint destWidth;
  uint destHeight;
} pushConstants;

layout(location = 0) out float outMaxDepth;

void main() {
  uint destX = uint(gl_FragCoord.x);
  uint destY = uint(gl_FragCoord.y);
  uint x0 = (destX * pushConstants.sourceWidth) / pushConstants.destWidth;
  uint x1 = max(x0 + 1u, ((destX + 1u) * pushConstants.sourceWidth) / pushConstants.destWidth);
  uint y0 = (destY * pushConstants.sourceHeight) / pushConstants.destHeight;
  uint y1 = max(y0 + 1u, ((destY + 1u) * pushConstants.sourceHeight) / pushConstants.destHeight);
  x1 = min(x1, pushConstants.sourceWidth);
  y1 = min(y1, pushConstants.sourceHeight);

  float maxDepth = texelFetch(sourceDepth, ivec2(x0, y0), 0).r;
  for (uint y = y0; y < y1; ++y) {
    for (uint x = x0; x < x1; ++x) {
      maxDepth = max(maxDepth, texelFetch(sourceDepth, ivec2(x, y), 0).r);
    }
  }
  outMaxDepth = maxDepth;
}
