// Adapted source Rain capability: R shape/lifetime, GB signed normal, A phase.
// All inputs raw linear; four samples, no simulation/controller ownership.
vec3 terrainPbrRainWorldNormal(uint row,TerrainFlowSamples prepared,float puddleCoverage,vec3 geometricNormal){
 vec4 params=terrainSurfaceRows[row+1u];float intensity=params.x;
 if(intensity<=0.0||puddleCoverage<=0.0)return geometricNormal;
 vec2 uv=prepared.domainUv*params.y,dx=prepared.dx*params.y,dy=prepared.dy*params.y;
 vec4 times=fract((vec4(TERRAIN_SURFACE_TIME_SECONDS)*vec4(1,.85,.93,1.13)+vec4(0,.2,.45,.7))*1.6);
 vec2 offsets[4]=vec2[4](vec2(.25,0),vec2(-.55,.3),vec2(.6,.85),vec2(.5,-.75));
 vec2 ripple=vec2(0);
 for(uint n=0u;n<4u;++n){
  float weight=intensity-float(n)*.25;
  vec4 raw=terrainPbrFlowData(row+2u,uv+offsets[n],dx,dy);
  float lifetime=fract(raw.a+times[n]),phase=lifetime-1.0+raw.r;
  float amplitude=clamp(.2+weight*.8-lifetime,0.0,1.0)*raw.r*sin(clamp(phase*9.0,0.0,3.0)*3.14159265359);
  ripple+=(raw.gb*2.0-1.0)*amplitude*clamp(weight*4.0,0.0,1.0);
 }
 // Source blends normalized XY by intensity squared, then applies the puddle
 // mask here and again at the final water-normal blend in DoStreams.
 vec2 xy=normalize(vec3(ripple,1)).xy*intensity*intensity*puddleCoverage;
 return terrainPbrNormalGrad(vec3(xy,1),prepared.dx,prepared.dy,prepared.px,prepared.py,geometricNormal);
}
void terrainPbrApplySpecularHeight(inout TerrainPBRResult material,uint root,float worldHeight){
 uint row=terrainPbrFlowRecord(root,7u);if(row==0u)return;
 vec4 bounds=terrainSurfaceRows[row+1u];float fade=clamp((worldHeight-bounds.x)/max(bounds.y-bounds.x,.0001),0.0,1.0);
 // Actual source consumes Metallic and Smoothness, not a separate F0 weight.
 material.metallic*=fade;material.roughness=1.0-(1.0-material.roughness)*fade;
}
