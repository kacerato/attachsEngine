// Source packing is RG distortion, B heat blend, A crust; all raw linear.
vec4 terrainPbrLavaData(uint row,vec2 uv,vec2 dx,vec2 dy){
 vec4 options=terrainSurfaceRows[row];uint flags=uint(options.x+.5);uint map=row+9u;
 if((flags&4u)==0u)return terrainPbrFlowData(map,uv,dx,dy);
 float size=max(options.z,.0001);vec2 grid=uv/size,cell=floor(grid),f=fract(grid),v0,v1,v2;vec3 weights;
 if(f.x+f.y<=1.0){v0=cell;v1=cell+vec2(1,0);v2=cell+vec2(0,1);weights=vec3(1.0-f.x-f.y,f.x,f.y);}else{v0=cell+vec2(1);v1=cell+vec2(0,1);v2=cell+vec2(1,0);weights=vec3(f.x+f.y-1.0,1.0-f.x,1.0-f.y);}
 vec4 a=terrainPbrFlowData(map,uv+(terrainPbrClusterHash(v0)-.5)*size,dx,dy),b=terrainPbrFlowData(map,uv+(terrainPbrClusterHash(v1)-.5)*size,dx,dy),c=terrainPbrFlowData(map,uv+(terrainPbrClusterHash(v2)-.5)*size,dx,dy);
 // Astra shared triangular lattice, height affinity from source R. Explicit
 // three contributions; no histogram-preserving or vendor bit-parity claim.
 vec3 affinity=max(vec3(.0001),weights*exp2(clamp((vec3(a.r,b.r,c.r)-.5)*options.y*8.0,vec3(-32),vec3(32))));affinity/=dot(affinity,vec3(1));
 return a*affinity.x+b*affinity.y+c*affinity.z;
}
float terrainPbrApplyLava(inout TerrainPBRResult material,TerrainFlowSamples prepared,vec3 worldPosition,vec3 geometricNormal){
 if(prepared.lavaRow==0u)return 0;
 uint row=prepared.lavaRow;vec4 options=terrainSurfaceRows[row],params=terrainSurfaceRows[row+1u],distortion=terrainSurfaceRows[row+2u],scale=terrainSurfaceRows[row+6u];
 float level=prepared.raw.a*params.y;if((uint(options.x+.5)&2u)!=0u)level*=terrainPbrFlowHeight(worldPosition.y,terrainSurfaceRows[row+7u]);
 float gap=level-material.height,blend=clamp(gap*params.x,0.0,1.0);
 // Explicit gradients were captured in prepare; dry fragments need no fetch.
 if(blend<=0.0)return 0.0;
 vec2 uv=prepared.domainUv*scale.xy,dx=prepared.dx*scale.xy,dy=prepared.dy*scale.xy,a,b,da,db;float phase,distPhase;
 terrainPbrFlowPhases(uv,prepared.flowDir,params.w,params.z*mix(.1,1.0,clamp(gap,0.0,1.0)),a,b,phase);
 terrainPbrFlowPhases(uv*distortion.z,prepared.flowDir,distortion.y,distortion.x,da,db,distPhase);
 // Sample gradients are precomputed outside spatial coverage branches.
 vec4 disturbance=mix(terrainPbrLavaData(row,da*.51,dx,dy),terrainPbrLavaData(row,db,dx,dy),distPhase)*distortion.x*2.0-distortion.x;
 vec4 raw=mix(terrainPbrLavaData(row,a*1.1+disturbance.xy,dx,dy),terrainPbrLavaData(row,b+disturbance.zw,dx,dy),phase);
 vec3 edgeColor=terrainSurfaceRows[row+3u].rgb,color=mix(terrainSurfaceRows[row+4u].rgb,terrainSurfaceRows[row+5u].rgb,raw.b);
 float edge=clamp((1.0-clamp(gap*params.x*.5,0.0,1.0))*3.0,0.0,1.0),crust=clamp(raw.a*distortion.w*clamp(gap*2.0,0.0,1.0),0.0,1.0);
 color=mix(color*(1.0-crust),edgeColor,edge);
 material.color.rgb=mix(material.color.rgb,color,blend);
 vec2 xy=raw.rg*2.0-1.0;vec3 tangent=vec3(xy,sqrt(max(0.0,1.0-dot(xy,xy))));
 vec3 normal=terrainPbrNormalGrad(tangent,prepared.dx,prepared.dy,prepared.px,prepared.py,geometricNormal);
 material.normalWorld=terrainPbrSafeNormal(mix(material.normalWorld,normal,blend),geometricNormal);
 material.roughness=mix(material.roughness,.7,blend*crust);
 material.emission=mix(material.emission,color*blend*options.w,blend);
 material.emission+=edgeColor*.3*(blend>0.0?1.0:0.0)*(clamp((level*1.2-material.height)*params.x,0.0,1.0)-blend);
 return clamp(blend*3.0,0.0,1.0);
}
