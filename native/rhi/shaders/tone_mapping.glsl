#ifndef ASTRA_TONE_MAPPING_GLSL
#define ASTRA_TONE_MAPPING_GLSL
// Godot 4.4-stable, MIT. Pinned source and license accompany the shader.
#include "../../third_party/godot_agx/agx.glsl"

// Neutral da Unity (Color.hlsl, NeutralTonemap): curva de Hable com branco em
// 5,3, a mesma do Tonemapping Neutral do URP/HDRP.
highp vec3 astraNeutralCurve(highp vec3 x) {
  const highp float a=0.2,b=0.29,c=0.24,d=0.272,e=0.02,f=0.3;
  return ((x*(a*x+c*b)+d*e)/(x*(a*x+b)+d*f))-e/f;
}

// Input and output are linear sRGB. Transfer to display sRGB happens once,
// after grading. IDs: 0 Reinhard (legado), 1 ACES, 2 AgX, 3 None, 4 Neutral.
highp vec3 astraToneMap(highp vec3 color, highp float mode) {
  color=max(color,vec3(0.0));
  if(mode<0.5) return color/(vec3(1.0)+color);
  if(mode<1.5)
    return clamp((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);
  if(mode<2.5) return max(tonemap_agx(color),vec3(0.0));
  if(mode<3.5) return min(color,vec3(1.0));
  highp float whiteScale=1.0/astraNeutralCurve(vec3(5.3)).x;
  return clamp(astraNeutralCurve(color*whiteScale)*whiteScale,0.0,1.0);
}
#endif
