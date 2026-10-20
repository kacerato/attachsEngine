// Version 1 table layout is published by terrain_surface_binding.h. This
// consumer deliberately excludes the later noise/resampling/cluster modules.
#if defined(TERRAIN_SURFACE_PBR) && defined(TERRAIN_SURFACE_MAP)
layout(std430,set=1,binding=18) readonly buffer TerrainSurfaceTable { vec4 terrainSurfaceRows[]; };
float terrainNoise(vec2 p);
struct TerrainPBRResult {
  vec4 color;
  vec3 normalWorld;
  float roughness,metallic,occlusion;
  vec3 emission;
  bool valid;
  float height;
};
vec3 terrainPbrSafeNormal(vec3 n,vec3 fallback) {return dot(n,n)>1e-12?normalize(n):fallback;}
// The derivative frame follows increasing projected U/V in world space. This
// handles signed projections, mirrored UVs, rotations and nonuniform object
// scaling without assuming an object-to-world matrix exists in the fragment.
vec3 terrainPbrNormal(vec3 tangentNormal,vec2 uv,vec3 worldPosition,vec3 n) {
  vec3 px=dFdx(worldPosition),py=dFdy(worldPosition);
  vec2 ux=dFdx(uv),uy=dFdy(uv);
  float determinant=ux.x*uy.y-ux.y*uy.x;
  if(abs(determinant)<1e-12)return n;
  vec3 t=(px*uy.y-py*ux.y)/determinant;
  vec3 b=(py*ux.x-px*uy.x)/determinant;
  t=terrainPbrSafeNormal(t-n*dot(n,t),vec3(0));
  if(dot(t,t)<.5)return n;
  float handedness=dot(cross(n,t),b)<0.0?-1.0:1.0;
  b=cross(n,t)*handedness;
  return terrainPbrSafeNormal(t*tangentNormal.x+b*tangentNormal.y+n*tangentNormal.z,n);
}
vec2 terrainPbrUv(vec2 coordinates,vec4 transform,float angle) {
  float radiansAngle=radians(angle),c=cos(radiansAngle),s=sin(radiansAngle);
  // Offsets use projected meters, like TerrainLayer tileOffset. Translate in
  // that space before converting tile sizes to repeats and rotating UV axes.
  vec2 tiled=(coordinates+transform.zw)/max(transform.xy,vec2(.0001));
  return mat2(c,s,-s,c)*tiled;
}
vec4 terrainPbrFetch(uint mapRow,vec2 uv,float quality) {
  vec4 binding=terrainSurfaceRows[mapRow];
  if(binding.x<0.0)return vec4(1);
  uint slot=uint(binding.x+.5);
  vec2 dx=dFdx(uv),dy=dFdy(uv);
  if(quality<.5)return textureLod(TERRAIN_SURFACE_MAP(slot),uv,min(1.0,float(textureQueryLevels(TERRAIN_SURFACE_MAP(slot))-1)));
  vec4 value=textureGrad(TERRAIN_SURFACE_MAP(slot),uv,dx,dy);
  if(quality>1.5){
    value=vec4(0);
    for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)
      value+=textureGrad(TERRAIN_SURFACE_MAP(slot),uv+.25*(float(x)*dx+float(y)*dy),dx,dy)*.25;
  }
  return value;
}
float terrainPbrScalar(uint row,vec4 sampleValue) {
  vec4 binding=terrainSurfaceRows[row],remap=terrainSurfaceRows[row+1u];
  uint channel=uint(binding.y+.5);
  float scalar=channel<4u?sampleValue[channel]:dot(sampleValue.rgb,vec3(.2126,.7152,.0722));
  float value=mix(remap.x,remap.y,scalar);
  if(binding.z>.5)value=1.0-value;
  return value*binding.w;
}
vec4 terrainPbrColor(uint row,vec4 sampleValue) {
  vec4 binding=terrainSurfaceRows[row];
  vec4 value=binding.x<0.0?vec4(1):sampleValue;
  if(binding.y<3.5&&binding.x>=0.0)value=vec4(value[min(uint(binding.y+.5),3u)]);
  value=mix(terrainSurfaceRows[row+3u],terrainSurfaceRows[row+4u],value);
  if(binding.z>.5)value=vec4(1)-value;
  return value*binding.w*terrainSurfaceRows[row+2u];
}
TerrainPBRResult terrainPbrLayer(uint row,vec3 localPosition,vec3 worldPosition,vec3 worldNormal) {
  TerrainPBRResult result;result.color=vec4(0);result.normalWorld=vec3(0);result.roughness=0;result.metallic=0;result.occlusion=0;result.emission=vec3(0);result.height=0;result.valid=true;
  vec4 options=terrainSurfaceRows[row],transform=terrainSurfaceRows[row+1u],blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];
  vec3 n=worldNormal;
  if(options.w>.5){vec3 face=terrainPbrSafeNormal(cross(dFdx(worldPosition),dFdy(worldPosition)),n);n=dot(face,n)<0.0?-face:face;}
  vec3 p=options.y>1.5?worldPosition:localPosition;
  vec3 projectionNormal=terrainPbrSafeNormal(cross(dFdx(p),dFdy(p)),vec3(0,1,0));
  vec3 axes=pow(abs(projectionNormal),vec3(max(options.z,.01)));
  axes/=max(dot(axes,vec3(1)),1e-8);
  if(options.y<.5)axes=vec3(0,1,0);
  // All projections and derivatives are evaluated before weight branches.
  for(uint axis=0u;axis<3u;++axis){
    // Projection is uniform for the complete draw/layer. UV mode needs only
    // XZ, so omitting the other axes cannot make derivatives divergent.
    if(options.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 uv=terrainPbrUv(projected,transform,blend.x);
    uint maps=row+5u;
    vec4 albedo=terrainPbrColor(maps,terrainPbrFetch(maps,uv,1.0));
    uint normalRow=maps+5u;
    vec4 normalBinding=terrainSurfaceRows[normalRow];
    vec3 normal=n;
    if(normalBinding.x>=0.0){
      vec4 encoded=terrainPbrFetch(normalRow,uv,1.0);
      vec3 decoded=mix(terrainSurfaceRows[normalRow+3u].xyz,terrainSurfaceRows[normalRow+4u].xyz,encoded.xyz)*terrainSurfaceRows[normalRow+2u].xyz;
      if(normalBinding.z>.5)decoded=vec3(1)-decoded;
      decoded=decoded*2.0-1.0;
      if(normalBinding.y>5.5)decoded.z=sqrt(max(1.0-dot(decoded.xy,decoded.xy),0.0));
      decoded.xy*=normalBinding.w;
      if(normalBinding.w==0.0)decoded=vec3(0,0,1);
      normal=terrainPbrNormal(terrainPbrSafeNormal(decoded,vec3(0,0,1)),uv,worldPosition,n);
    }
    float weight=axes[axis];
    result.color+=albedo*weight;result.normalWorld+=normal*weight;
    result.height+=terrainPbrScalar(maps+10u,terrainPbrFetch(maps+10u,uv,height.z))*weight;
    result.occlusion+=terrainPbrScalar(maps+15u,terrainPbrFetch(maps+15u,uv,1.0))*weight;
    result.roughness+=terrainPbrScalar(maps+20u,terrainPbrFetch(maps+20u,uv,1.0))*weight;
    result.metallic+=terrainPbrScalar(maps+25u,terrainPbrFetch(maps+25u,uv,1.0))*weight;
    result.emission+=terrainPbrColor(maps+30u,terrainPbrFetch(maps+30u,uv,1.0)).rgb*weight;
  }
  float luminance=dot(result.color.rgb,vec3(.2126,.7152,.0722));
  result.color.rgb=mix(vec3(luminance),result.color.rgb,blend.y);
  result.color*=terrainSurfaceRows[row+4u];
  result.normalWorld=terrainPbrSafeNormal(result.normalWorld,n);
  return result;
}
TerrainPBRResult terrainPBRSurface(vec3 worldPosition,vec3 worldNormal) {
  TerrainPBRResult result;result.color=vec4(0);result.normalWorld=worldNormal;result.roughness=1;result.metallic=0;result.occlusion=1;result.emission=vec3(0);result.height=0;result.valid=false;
  if(!aetherHasMaterialExtension()||aetherMaterialRow(14u).z<.5)return result;
  uint offset=uint(aetherMaterialRow(14u).z+.5)-1u;
  vec4 header=terrainSurfaceRows[offset];uint count=min(uint(header.x+.5),32u);
  if(header.y!=1.0||count==0u)return result;
  vec4 weights=max(vColor,vec4(0));
#ifdef TERRAIN_CONTROL_MAP
  if(vUv1.y>=0.0&&aetherMaterialRow(13u).x>.5){vec4 control=aetherMaterialRow(13u);weights=max(textureLod(TERRAIN_CONTROL_MAP(uint(control.y+.5)),vUv0*control.zw+aetherMaterialRow(14u).xy,0.0),vec4(0));}
#endif
  vec3 localPosition=vec3(vUv0.x,vUv1.x,vUv0.y)*terrainSurfaceRows[offset+1u].x;
  TerrainPBRResult layers[4];float amounts[4];float peak=-1e20;
  // Current control storage is RGBA. Higher catalog layers require additional
  // control pages before they can receive weights; they are not invented here.
  uint supported=min(count,4u);
  for(uint i=0u;i<supported;++i){uint row=offset+2u+i*40u;amounts[i]=0.0;if(terrainSurfaceRows[row].x<.5)continue;layers[i]=terrainPbrLayer(row,localPosition,worldPosition,worldNormal);vec4 blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];amounts[i]=weights[i];if(amounts[i]>0.0)peak=max(peak,amounts[i]+layers[i].height*height.y);}
  float sum=0;
  for(uint i=0u;i<supported;++i){uint row=offset+2u+i*40u;if(terrainSurfaceRows[row].x<.5)continue;vec4 blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];if(blend.z>.5){float h=amounts[i]+layers[i].height*height.y;amounts[i]*=pow(clamp((h-peak+height.x)/max(height.x,.0001),0.0,1.0),max(blend.w,.0001));}sum+=amounts[i];}
  if(sum<1e-8){result.valid=true;result.color=vec4(0,0,0,1);return result;}
  result.normalWorld=vec3(0);result.roughness=0;result.metallic=0;result.occlusion=0;result.valid=true;
  for(uint i=0u;i<supported;++i){if(terrainSurfaceRows[offset+2u+i*40u].x<.5)continue;float w=amounts[i]/sum;result.color+=layers[i].color*w;result.normalWorld+=layers[i].normalWorld*w;result.roughness+=layers[i].roughness*w;result.metallic+=layers[i].metallic*w;result.occlusion+=layers[i].occlusion*w;result.emission+=layers[i].emission*w;result.height+=layers[i].height*w;}
  float variation=clamp(vUv1.y<0.0?-vUv1.y-1.0:vUv1.y,0.0,1.0);
  result.color.rgb*=1.0+variation*(terrainNoise(vUv0*.11)-.5)*.6;
  result.normalWorld=terrainPbrSafeNormal(result.normalWorld,worldNormal);result.roughness=clamp(result.roughness,.07,1.0);result.metallic=clamp(result.metallic,0.0,1.0);result.occlusion=clamp(result.occlusion,0.0,1.0);return result;
}
#endif
