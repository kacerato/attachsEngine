// StaticFX ABI 1. RecipeFX RGBA8 independent channels, linear Clamp, one level.
// No derivatives or implicit LOD: CPU confirmation samples these exact bytes.
#ifndef TERRAIN_SURFACE_TIME_SECONDS
#define TERRAIN_SURFACE_TIME_SECONDS environment.materialDistanceParameters.w
#endif
#include "terrain_flow_water.glsl"
#include "terrain_flow_lava.glsl"
#include "terrain_flow_rain.glsl"
float terrainPbrApplyStaticFx(inout TerrainPBRResult material,uint root,vec3 localPosition,
    vec3 worldPosition,vec3 geometricNormal,float amounts[4],uint supported,float sum,TerrainFlowSamples flowSamples){
  if(terrainSurfaceRows[root].x<3.0)return 0.0;
  uint directory=uint(terrainSurfaceRows[root+3u].x+.5)-1u;
  uint snapshot=uint(terrainSurfaceRows[directory].z+.5),wetRow=uint(terrainSurfaceRows[directory+1u].z+.5),puddleRow=uint(terrainSurfaceRows[directory+2u].z+.5);
  vec4 record=terrainSurfaceRows[snapshot],domain=terrainSurfaceRows[snapshot+1u];
  vec2 uv=clamp((localPosition.xz-domain.xy)/domain.zw,vec2(0),vec2(1));
  vec4 raw=record.w<.5?vec4(0):terrainSurfaceRows[root].x>=4.0?flowSamples.raw:textureLod(TERRAIN_SURFACE_MAP(uint(record.x+.5)),uv,0.0);
  vec4 options=terrainSurfaceRows[wetRow],range=terrainSurfaceRows[wetRow+1u],height=terrainSurfaceRows[wetRow+2u],distanceOptions=terrainSurfaceRows[wetRow+3u],puddle=terrainSurfaceRows[puddleRow];
  uint flags=uint(puddle.w+.5);float wetness=0.0,puddleCoverage=0.0;
  if(options.x>.5){
    wetness=clamp(raw.r,range.x,range.y);
    if(options.y>.5){float waterline=height.x+sin(TERRAIN_SURFACE_TIME_SECONDS*height.z)*height.w;wetness=max(wetness,clamp((waterline-worldPosition.y)*height.y,0.0,1.0));}
    if(options.z>.5){float distanceToCamera=length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION);float fade=clamp((distanceToCamera-distanceOptions.x)/distanceOptions.z,0.0,1.0);wetness*=mix(distanceOptions.y,distanceOptions.w,fade);}
  }
  if(puddle.x>.5)puddleCoverage=clamp((raw.g*puddle.z-material.height)*puddle.y,0.0,1.0);
  float streamCoverage=terrainPbrStreamCoverage(flowSamples,worldPosition.y,material.height);
  float coverage=max(max(wetness,puddleCoverage),streamCoverage),porosity=options.w;
  if((flags&1u)!=0u){uint controls=uint(terrainSurfaceRows[snapshot+2u].x+.5);porosity=0.0;for(uint i=0u;i<supported;++i)porosity+=terrainSurfaceRows[controls+i].x*(amounts[i]/sum);}
  float foamStrength=1.0,foam=0.0,normalStrength=1.0;
  if(flowSamples.streamRow!=0u){
    uint row=flowSamples.streamRow;
    if((uint(terrainSurfaceRows[row].x+.5)&4u)!=0u){uint controls=uint(terrainSurfaceRows[snapshot+2u].x+.5);foamStrength=0;for(uint i=0u;i<supported;++i)foamStrength+=terrainSurfaceRows[controls+i].y*(amounts[i]/sum);}
    foam=terrainPbrStreamFoam(flowSamples,worldPosition.y,material.height,foamStrength,geometricNormal);
    material.color.rgb=mix(material.color.rgb,material.color.rgb*terrainSurfaceRows[row+4u].rgb*2.0,streamCoverage);
    normalStrength=terrainSurfaceRows[row+2u].x;
  }
  // Preserve source's nested lerp: the water BRDF is itself blended by coverage.
  float absorption=clamp((material.roughness-.5)/max(porosity,.001),0.0,1.0);
  float darkening=mix(1.0,.2,(1.0-material.metallic)*absorption);
  vec3 wetColor=material.color.rgb*mix(1.0,darkening,coverage)+vec3(foam);
  material.color.rgb=mix(material.color.rgb,wetColor,coverage);
  float wetRoughness=mix(material.roughness,.08,coverage)+foam;
  material.roughness=mix(material.roughness,wetRoughness,coverage);
  material.occlusion=mix(material.occlusion,1.0,coverage);
  material.normalWorld=terrainPbrSafeNormal(material.normalWorld,geometricNormal);
  vec3 waterNormal=geometricNormal;uint rainRow=terrainPbrFlowRecord(root,6u);
  if(rainRow!=0u)waterNormal=terrainPbrRainWorldNormal(rainRow,flowSamples,puddleCoverage,geometricNormal);
  material.normalWorld=terrainPbrSafeNormal(mix(material.normalWorld,waterNormal,puddleCoverage*normalStrength),geometricNormal);
  float lavaClearing=terrainPbrApplyLava(material,flowSamples,worldPosition,geometricNormal);
  float clearing=max(coverage,lavaClearing);
  if((flags&2u)!=0u)clearing=max(clearing,1.0-raw.r);
  return clearing;
}
