#version 450
#extension GL_GOOGLE_include_directive : require
precision highp float;
precision highp int;
layout(set=0,binding=0) uniform sampler2D textures[10];
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
#ifdef TEST_PBR
#define TERRAIN_SURFACE_CAMERA_POSITION ((test.mode==45u||test.mode==46u||test.mode==77u||test.mode==81u||test.mode==82u||test.mode==83u||test.mode==86u)?vec3(worldPosition.x,15.0,worldPosition.z):vec3(0,(test.mode==33u||test.mode==35u||test.mode==62u||test.mode==69u||test.mode==76u||test.mode==84u||test.mode==106u)?100.0:0.0,0))
#define TERRAIN_SURFACE_PBR
#define TERRAIN_SURFACE_MAP(index) textures[index]
#endif
#ifndef NO_CONTROL_MAP
#define TERRAIN_CONTROL_MAP(index) textures[index]
#endif
#include "material_uv_transform.glsl"
#include "terrain_surface.glsl"
void main() {
  frame.materialFlags=uvec4(0,0,test.mode==4u?0u:2u,0);
  vUv0=uv;
  if(test.mode==112u||test.mode==113u)vUv0=uv*4.0+vec2(test.mode==112u?-.00001:.00001,0);
  if(test.mode==1u)vUv0.x=0.0;
  if(test.mode==2u)vUv0.x=1.0;
  vUv1=vec2(0,test.mode==3u?-1.0:0.0);
  if(test.mode==23u||test.mode==24u)vUv1.y=1.0;
  vColor=vec4(1,0,0,0);
  if(test.mode==13u||test.mode==15u)vColor=vec4(.5,.5,0,0);
#ifdef TEST_PBR
  vec3 position=vec3(uv.x*2.0,0,uv.y*3.0),normal=vec3(0,1,0);
  if(test.mode==14u||test.mode==72u||test.mode==73u){position=vec3(uv.x*1.41421356,uv.x*1.41421356,uv.y*3.0);normal=normalize(vec3(-1,1,0));}
  if(test.mode==73u)normal=vec3(0,1,0);
  TerrainPBRResult pbr=terrainPBRSurface(position,normal);
  if(test.mode==9u||test.mode==14u||test.mode==16u||test.mode==17u||test.mode==22u||test.mode==29u||((test.mode>=50u&&test.mode<74u&&test.mode!=68u&&test.mode!=69u)||test.mode==79u||test.mode==94u||test.mode==115u))outColor=vec4(pbr.normalWorld*.5+.5,1);
  else if(test.mode==10u||test.mode==23u||test.mode==26u||test.mode==95u)outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);
  else if(test.mode==11u||test.mode==25u||test.mode==27u||test.mode==80u||test.mode==86u||test.mode==96u)outColor=vec4(pbr.emission,pbr.height);
  else outColor=pbr.color;
  if(!pbr.valid)outColor=vec4(1,0,1,1);
#else
  outColor=terrainSurface();
#endif
}
