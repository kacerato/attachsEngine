#version 450

layout(push_constant) uniform RuntimeHudPushConstants {
  vec4 centerHalfSize;
  vec4 displaySize;
  vec4 surfaceTransform;
  uvec4 parameters;
} hud;

layout(location=0) out vec2 localPosition;

void main() {
  vec2 positions[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),
                            vec2(-1,-1),vec2(1,1),vec2(-1,1));
  vec2 sourcePosition=positions[gl_VertexIndex];
  mat2 displayToFramebuffer=mat2(abs(hud.surfaceTransform.x),abs(hud.surfaceTransform.z),
                                 abs(hud.surfaceTransform.y),abs(hud.surfaceTransform.w));
  // HUD positions already arrive in Android's visible coordinate system. We
  // only map which swapchain axis stores each display axis; the compositor
  // owns the rotation sign. Applying the signed world transform here rotates
  // the overlay a second time (bottom-left becomes top and glyphs mirror).
  localPosition=sourcePosition;
  vec2 pixel=hud.centerHalfSize.xy+localPosition*hud.centerHalfSize.zw;
  vec2 displayNdc=vec2(pixel.x*2.0/hud.displaySize.x-1.0,
                       1.0-pixel.y*2.0/hud.displaySize.y);
  // World geometry uses the signed matrix. Display-space controls use only
  // its axis mapping so both landscape sides retain bottom-left/top-left.
  vec2 framebufferNdc=displayToFramebuffer*displayNdc;
  gl_Position=vec4(framebufferNdc,0.0,1.0);
}
