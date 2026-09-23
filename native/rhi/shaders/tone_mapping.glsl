#ifndef ASTRA_TONE_MAPPING_GLSL
#define ASTRA_TONE_MAPPING_GLSL
// Godot 4.4-stable, MIT. Pinned source and license accompany the shader.
#include "../../third_party/godot_agx/agx.glsl"

// Input and output are linear sRGB. Transfer to display sRGB happens once,
// after grading. IDs 0/1 preserve the appearance of existing Astra scenes.
highp vec3 astraToneMap(highp vec3 color, highp float mode) {
  color=max(color,vec3(0.0));
  if(mode<0.5) return color/(vec3(1.0)+color);
  if(mode<1.5)
    return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
  return max(tonemap_agx(color),vec3(0.0));
}
#endif
