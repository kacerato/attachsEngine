// Shared global tonemap operator. Used by every consumer of scene-referred
// linear HDR color (dirt_road_sky.frag, environment_lighting.glsl) so the
// visible sky and shaded materials always compress highlights the same way.
//
// AgX minimal fit (Troy Sobotka's Blender AgX view transform, public
// polynomial approximation of the log/contrast + inset/outset matrices).
// Replaces a naive ACES-fit curve that desaturated bright, saturated color
// (e.g. the procedural sky's blue) more than AgX's softer highlight
// shoulder does. Output is linear display-referred color in [0,1]; the
// caller applies the sRGB OETF afterward when the target requires it, same
// as the curve it replaces.
const mat3 AGX_INSET = mat3(
  0.856627153315983, 0.0951212405381588, 0.0482516061458583,
  0.137318972929847, 0.761241990602591, 0.101439036467562,
  0.11189821299995, 0.0767994186031903, 0.811302368396859);
const mat3 AGX_OUTSET = mat3(
  1.1271005818144368, -0.11060664309660323, -0.016493938717834573,
  -0.1413297634984383, 1.157823702216272, -0.016493938717834257,
  -0.14132976349843826, -0.11060664309660294, 1.2519364065950405);
const float AGX_MIN_EV = -12.47393;
const float AGX_MAX_EV = 4.026069;

vec3 agxContrastApprox(vec3 x) {
  vec3 x2 = x * x, x4 = x2 * x2;
  return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + .4298 * x2 + .1191 * x - .00232;
}

vec3 agxTonemap(vec3 color) {
  color = AGX_INSET * max(color, vec3(0));
  color = log2(max(color, vec3(1e-10)));
  color = clamp((color - AGX_MIN_EV) / (AGX_MAX_EV - AGX_MIN_EV), 0.0, 1.0);
  color = agxContrastApprox(color);
  color = AGX_OUTSET * color;
  return pow(max(color, vec3(0)), vec3(2.2));
}
