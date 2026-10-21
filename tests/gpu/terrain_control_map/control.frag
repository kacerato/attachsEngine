#version 450
#extension GL_GOOGLE_include_directive : require
precision highp float;
precision highp int;
layout(set=0,binding=0) uniform sampler2D textures[14];
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
#define TERRAIN_SURFACE_CAMERA_POSITION ((test.mode==45u||test.mode==46u||test.mode==77u||test.mode==81u||test.mode==82u||test.mode==83u||test.mode==86u||test.mode==152u||test.mode==168u||test.mode==179u)?vec3(worldPosition.x,15.0,worldPosition.z):vec3(0,(test.mode==33u||test.mode==35u||test.mode==62u||test.mode==69u||test.mode==76u||test.mode==84u||test.mode==106u||test.mode==153u)?100.0:0.0,0))
#define TERRAIN_SURFACE_TIME_SECONDS (test.mode==288u?.125:test.mode==279u?.25:((test.mode==272u||test.mode==273u||test.mode==274u)?1.0:0.0))
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
  if(test.mode==250u)vUv0=vec2(-96);
  if(test.mode==251u)vUv0=vec2(32);
  if(test.mode==230u||test.mode==231u||test.mode==233u||test.mode==234u)vUv0=vec2(test.mode==230u||test.mode==233u?-32:32,-32);
  if(test.mode==232u)vUv0=vec2(-96,-96);
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
  if(test.mode==135u||test.mode==136u)position.y=.5;
  if(test.mode==145u)position.y=-.5;
  if(test.mode==189u)position.y=uv.y;
  if(test.mode==235u)position.y=.00005;
  if(test.mode>=255u&&test.mode<=257u||test.mode>=261u&&test.mode<=263u||test.mode==278u||test.mode==279u||test.mode>=271u&&test.mode<=274u)normal=normalize(vec3(.2,.979795897,0));
  if(test.mode==297u||test.mode==301u)position.y=1.5;
  if(test.mode==298u)position.y=2.5;
  if(test.mode==300u)position.y=.00005;
  // Old Flow fixtures authored identity LOCAL XZ but used world XZ scaled2/3.
  // Make geometry consistent without changing UVs, source normals or pixels.
  if(test.mode>=252u&&test.mode<=283u)position.xz=uv;
  if(test.mode>=302u&&test.mode<=309u){
    vec3 local=vec3(uv.x*2.0,-.2041241452319315*uv.x*2.0,uv.y*3.0);
    vec3 localNormal=normalize(vec3(.2,.9797958971132712,0));
    mat3 transform=mat3(1);
    if(test.mode==303u)transform=mat3(vec3(0,0,-1),vec3(0,1,0),vec3(1,0,0));
    if(test.mode==304u)transform=mat3(vec3(2,0,0),vec3(0,3,0),vec3(0,0,.5));
    if(test.mode==305u)transform=mat3(vec3(1,0,0),vec3(0,.8660254037844386,.5),vec3(0,-.5,.8660254037844386));
    if(test.mode==306u){local.y=0;localNormal=vec3(0,1,0);transform=mat3(vec3(0,1,0),vec3(-1,0,0),vec3(0,0,1));}
    if(test.mode==307u)transform=mat3(vec3(0,0,1),vec3(0,1,0),vec3(-1,0,0));
    if(test.mode==308u)transform=mat3(vec3(0,0,-2),vec3(0,3,0),vec3(.5,0,0));
    if(test.mode==309u){local.y=-.5*local.x+.25*local.z;localNormal=normalize(vec3(.5,1,-.25));transform=mat3(vec3(1.5,0,0),vec3(0,.8660254037844386,.5),vec3(0,-.25,.4330127018922193));}
    position=transform*local;normal=normalize(transpose(inverse(transform))*localNormal);
    vUv0=local.xz;vUv1.x=local.y;
    uint offset=uint(aetherMaterialRow(14u).z+.5)-1u,root=uint(terrainSurfaceRows[offset+1u].y+.5)-1u;
    TerrainFlowSamples prepared=terrainPbrPrepareFlow(root,local,position,normal);
    outColor=vec4(prepared.flowDir*.25+.5,0,1);return;
  }
  if(test.mode==310u||test.mode==311u){
    position.y=test.mode==310u?5.0:1.5;
    uint offset=uint(aetherMaterialRow(14u).z+.5)-1u,root=uint(terrainSurfaceRows[offset+1u].y+.5)-1u;
    vec3 local=vec3(vUv0.x,vUv1.x,vUv0.y)*terrainSurfaceRows[offset+1u].x;
    TerrainFlowSamples prepared=terrainPbrPrepareFlow(root,local,position,normal);
    vec2 refract=terrainPbrStreamRefraction(prepared,0.0,position.y);
    outColor=vec4(vec2(.5)+refract*2.0,0,1);return;
  }
  TerrainPBRResult pbr=terrainPBRSurface(position,normal);
  if(test.mode>=284u&&test.mode<=295u){outColor=vec4(pbr.normalWorld*.5+.5,1);return;}
  if(test.mode>=296u){outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);return;}
  if(test.mode==253u){outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);return;}
  if(test.mode==267u||test.mode==277u){outColor=vec4(pbr.emission,1);return;}
  if(test.mode>=271u&&test.mode<=274u){outColor=vec4(pbr.normalWorld*.5+.5,1);return;}
  if(test.mode>=261u&&test.mode<=263u||test.mode==278u||test.mode==279u){outColor=vec4(vec3(pbr.roughness),1);return;}
  if(test.mode==239u||test.mode==241u){outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);return;}
  if(test.mode==235u){outColor=vec4(vec3((1.0-pbr.color.g)*100000.0),1);return;}
  if(test.mode==212u||test.mode==213u||test.mode==220u||test.mode==221u||test.mode==236u||test.mode==237u){outColor=vec4(pbr.normalWorld*.5+.5,1);return;}
  if(test.mode==214u||test.mode==226u||test.mode==227u){outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);return;}
  if(test.mode==215u){outColor=vec4(pbr.transmission,1);return;}
  if(test.mode==216u){outColor=vec4(pbr.emission,1);return;}
  if(test.mode==9u||test.mode==14u||test.mode==16u||test.mode==17u||test.mode==22u||test.mode==29u||((test.mode>=50u&&test.mode<74u&&test.mode!=68u&&test.mode!=69u)||test.mode==79u||test.mode==94u||test.mode==115u||test.mode>=128u&&test.mode<=132u||test.mode>=162u&&test.mode<=169u||test.mode==192u))outColor=vec4(pbr.normalWorld*.5+.5,1);
  else if(test.mode==10u||test.mode==23u||test.mode==26u||test.mode==95u||test.mode>=170u&&test.mode<=175u||test.mode==181u)outColor=vec4(pbr.roughness,pbr.metallic,pbr.occlusion,1);
  else if(test.mode==11u||test.mode==25u||test.mode==27u||test.mode==80u||test.mode==86u||test.mode==96u||test.mode>=176u&&test.mode<=180u)outColor=vec4(pbr.emission,pbr.height);
  else outColor=pbr.color;
  if(!pbr.valid)outColor=vec4(1,0,1,1);
#else
  outColor=terrainSurface();
#endif
}
