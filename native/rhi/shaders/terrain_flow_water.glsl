// Adapted capabilities from supplied Streams3.9.41. No private source copied.
struct TerrainFlowSamples {
 uint streamRow,lavaRow;vec4 raw;vec3 px,py;vec2 domainUv,flowDir,dx,dy;vec3 normalFoam;vec2 foamUv0,foamUv1,foamDx,foamDy;float foamInterp;
};
uint terrainPbrFlowRecord(uint root,uint type){
 if(root==0u||terrainSurfaceRows[root].x<3.0)return 0u;
 uint directory=uint(terrainSurfaceRows[root+3u].x+.5)-1u,count=uint(terrainSurfaceRows[root+3u].y+.5);
 for(uint n=3u;n<count;++n)if(uint(terrainSurfaceRows[directory+n].x+.5)==type)return uint(terrainSurfaceRows[directory+n].z+.5);
 return 0u;
}
void terrainPbrFlowPhases(vec2 uv,vec2 direction,float speed,float amplitude,out vec2 a,out vec2 b,out float blend){
 float phase=fract(TERRAIN_SURFACE_TIME_SECONDS*speed);a=uv-direction*amplitude*phase;b=uv-direction*amplitude*fract(phase+.5);blend=abs(.5-phase)*2.0;
}
float terrainPbrFlowHeight(float h,vec4 window){return clamp((h-window.x)/max(window.y-window.x,.0001),0.0,1.0)*(1.0-clamp((h-window.z)/max(window.w-window.z,.0001),0.0,1.0));}
vec4 terrainPbrFlowData(uint map,vec2 uv,vec2 dx,vec2 dy){return clamp(terrainPbrColor(map,terrainPbrFetchGrad(map,uv,dx,dy,1.0)),vec4(0),vec4(1));}
// Source direction is horizontal WORLD XZ, while phase UVs are LOCAL XZ.
// Pull back through the horizontal position Jacobian once. A Gram projection
// of the horizontal vector would shorten identity sloped-terrain amplitudes.
vec2 terrainPbrFlowLocalDirection(vec3 n,vec3 px,vec3 py,vec2 localDx,vec2 localDy){
 vec2 slope=n.xz;float magnitude=length(slope);vec2 unit=magnitude>1e-8?slope/magnitude:vec2(0);
 // Source3.9.41 explicitly uses normal.z, not the vertical normal.y.
 vec2 worldFlow=mix(slope,unit,clamp(max(.1,n.z),0.0,1.0));
 vec2 a=px.xz,b=py.xz;float determinant=a.x*b.y-a.y*b.x;
 float scale=length(a)*length(b);
 // Vertical/rank-deficient horizontal projections have no unique inverse.
 if(isnan(determinant)||isinf(determinant)||isnan(scale)||isinf(scale)||scale<=1e-30||abs(determinant)<=1e-8*scale)return vec2(0);
 vec2 weights=vec2(worldFlow.x*b.y-worldFlow.y*b.x,a.x*worldFlow.y-a.y*worldFlow.x)/determinant;
 vec2 localFlow=localDx*weights.x+localDy*weights.y;
 if(any(isnan(localFlow))||any(isinf(localFlow)))return vec2(0);
 return localFlow;
}
TerrainFlowSamples terrainPbrPrepareFlow(uint root,vec3 localPosition,vec3 worldPosition,vec3 geometricNormal){
 TerrainFlowSamples prepared;prepared.px=dFdx(worldPosition);prepared.py=dFdy(worldPosition);prepared.streamRow=0u;prepared.lavaRow=0u;prepared.raw=vec4(0);prepared.domainUv=vec2(0);prepared.flowDir=vec2(0);prepared.dx=vec2(0);prepared.dy=vec2(0);prepared.normalFoam=vec3(0);prepared.foamUv0=vec2(0);prepared.foamUv1=vec2(0);prepared.foamInterp=0;prepared.foamDx=vec2(0);prepared.foamDy=vec2(0);
 if(root==0u||terrainSurfaceRows[root].x<4.0)return prepared;
 uint directory=uint(terrainSurfaceRows[root+3u].x+.5)-1u,snapshot=uint(terrainSurfaceRows[directory].z+.5);vec4 domain=terrainSurfaceRows[snapshot+1u];
 prepared.domainUv=clamp((localPosition.xz-domain.xy)/domain.zw,vec2(-1e8),vec2(1e8));
 if((uint(terrainSurfaceRows[snapshot].w+.5)&1u)!=0u)prepared.raw=textureLod(TERRAIN_SURFACE_MAP(uint(terrainSurfaceRows[snapshot].x+.5)),clamp(prepared.domainUv,vec2(0),vec2(1)),0.0);
 // Derivatives are computed uniformly before any spatial coverage branch.
 prepared.flowDir=terrainPbrFlowLocalDirection(geometricNormal,prepared.px,prepared.py,dFdx(localPosition.xz),dFdy(localPosition.xz));
 prepared.streamRow=terrainPbrFlowRecord(root,4u);prepared.lavaRow=terrainPbrFlowRecord(root,5u);
 // Gradients exist before any coverage-dependent branching.
 prepared.dx=dFdx(prepared.domainUv);prepared.dy=dFdy(prepared.domainUv);
 if(prepared.streamRow!=0u){uint row=prepared.streamRow;vec4 flow=terrainSurfaceRows[row+1u],scale=terrainSurfaceRows[row+3u];vec2 a,b;float blend;
  terrainPbrFlowPhases(prepared.domainUv*scale.xy,prepared.flowDir,flow.y,flow.z,a,b,blend);
  vec3 raw=mix(terrainPbrFlowData(row+6u,a,prepared.dx*scale.xy,prepared.dy*scale.xy),terrainPbrFlowData(row+6u,b,prepared.dx*scale.xy,prepared.dy*scale.xy),blend).rgb;
  prepared.normalFoam=vec3(raw.rg*2.0-1.0,raw.b);
  terrainPbrFlowPhases(prepared.domainUv*1.75+prepared.normalFoam.xy*prepared.normalFoam.z,prepared.flowDir,flow.y/3.0,flow.z/3.0,prepared.foamUv0,prepared.foamUv1,prepared.foamInterp);
  prepared.foamDx=dFdx(prepared.foamUv0);prepared.foamDy=dFdy(prepared.foamUv0);
 }
 return prepared;
}
vec2 terrainPbrStreamRefraction(TerrainFlowSamples prepared,float materialHeight,float worldHeight){
 if(prepared.streamRow==0u)return vec2(0);
 uint row=prepared.streamRow;vec4 options=terrainSurfaceRows[row],flow=terrainSurfaceRows[row+1u];uint flags=uint(options.x+.5);
 // Source applies max once in ProcessFX and once again in refraction.
 float level=prepared.raw.b*options.y*options.y;
 if((flags&2u)!=0u)level*=terrainPbrFlowHeight(worldHeight,terrainSurfaceRows[row+5u]);
 float blend=clamp((level-materialHeight)*options.z,0.0,1.0);
 return prepared.normalFoam.xy*level*blend*blend*mix(1.0,materialHeight,flow.w)*flow.x;
}
float terrainPbrStreamCoverage(TerrainFlowSamples prepared,float worldHeight,float materialHeight){
 if(prepared.streamRow==0u)return 0;
 vec4 options=terrainSurfaceRows[prepared.streamRow];float level=prepared.raw.b*options.y;
 if((uint(options.x+.5)&2u)!=0u)level*=terrainPbrFlowHeight(worldHeight,terrainSurfaceRows[prepared.streamRow+5u]);
 return clamp((level-materialHeight)*options.z,0.0,1.0);
}
float terrainPbrStreamFoam(TerrainFlowSamples prepared,float worldHeight,float materialHeight,float foamStrength,vec3 geometricNormal){
 if(prepared.streamRow==0u)return 0;
 uint row=prepared.streamRow;vec4 options=terrainSurfaceRows[row];float level=prepared.raw.b*options.y;
 if((uint(options.x+.5)&2u)!=0u)level*=terrainPbrFlowHeight(worldHeight,terrainSurfaceRows[row+5u]);
 float gap=level-materialHeight;if(gap<=0.0)return 0.0;
 // Explicit gradients keep foam sampling defined at a moving water boundary.
 float a=terrainPbrFlowData(row+6u,prepared.foamUv0,prepared.foamDx,prepared.foamDy).b,b=terrainPbrFlowData(row+6u,prepared.foamUv1,prepared.foamDx,prepared.foamDy).b;
 // abs(gap)+height == level on this positive-coverage branch.
 float foam=mix(a,b,prepared.foamInterp)*level*(1.0-clamp(gap*1.5,0.0,1.0));
 foam=foam*foam*terrainSurfaceRows[row+2u].y*min(length(geometricNormal.xz)*18.0,1.0)*foamStrength;
 return terrainPbrStreamCoverage(prepared,worldHeight,materialHeight)>0.0?foam:0.0;
}
