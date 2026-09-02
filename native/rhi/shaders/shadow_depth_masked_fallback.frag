#version 450
#extension GL_GOOGLE_include_directive : require
precision mediump int;
precision highp float;
layout(set=0,binding=0) uniform sampler2D baseMap;
layout(push_constant) uniform ShadowPushConstants {
  mat4 lightViewProjection;
  vec4 alphaCutoffUvSlot;
  uvec4 baseTextureIndex;
} shadow;
layout(location=0) in highp vec2 vUv;
layout(location=1) in mediump float vAlpha;
void main() {
  mediump float alpha = texture(baseMap, vUv).a * vAlpha;
  if (alpha < shadow.alphaCutoffUvSlot.x) discard;
}
