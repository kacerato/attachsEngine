#version 450
#extension GL_GOOGLE_include_directive : require
precision highp float;
precision highp int;
layout(set=0,binding=0) uniform sampler2D textures[5];
layout(push_constant) uniform Test { uint mode; } test;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;
struct Frame { uvec4 materialFlags; };
Frame frame;
vec2 vUv0,vUv1;
vec4 vColor;
#define BASE_MAP textures[0]
#define NORMAL_MAP textures[1]
#define MR_MAP textures[2]
#define EMISSIVE_MAP textures[3]
#ifndef NO_CONTROL_MAP
#define TERRAIN_CONTROL_MAP(index) textures[index]
#endif
#include "material_uv_transform.glsl"
#include "terrain_surface.glsl"
void main() {
  frame.materialFlags=uvec4(0,0,test.mode==4u?0u:2u,0);
  vUv0=uv;
  if(test.mode==1u)vUv0.x=0.0;
  if(test.mode==2u)vUv0.x=1.0;
  vUv1=vec2(0,test.mode==3u?-1.0:0.0);
  vColor=vec4(1,0,0,0);
  outColor=terrainSurface();
}
