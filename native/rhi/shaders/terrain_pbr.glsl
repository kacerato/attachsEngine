// Version 1 table layout is published by terrain_surface_binding.h. This
// optional advanced v1 directory adds M05/M06 noises, M07 resampling and M08 clusters.
#if defined(TERRAIN_SURFACE_PBR) && defined(TERRAIN_SURFACE_MAP)
layout(std430,set=1,binding=18) readonly buffer TerrainSurfaceTable { vec4 terrainSurfaceRows[]; };
// Advanced-v1 transport matches terrain_surface_abi in terrain_surface_table.h.
const uint terrainNoiseRows=7u,terrainNoiseCount=5u;
const uint terrainAdvancedHeaderRows=2u,terrainLayerDirectoryRows=2u;
const uint terrainAdvancedGlobalRows=terrainAdvancedHeaderRows+terrainNoiseCount*terrainNoiseRows;
const uint terrainAdvancedLayerRows=terrainLayerDirectoryRows+terrainNoiseCount*terrainNoiseRows;
const uint terrainLayerExtraRows=3u,terrainAntiEntryRows=3u*terrainNoiseRows;
const uint terrainFeatureM05=1u,terrainFeatureM06=2u,terrainFeatureAntiTile=4u,terrainFeatureM07=8u,terrainFeatureM08=16u,terrainFeatureGeo=32u,terrainFeatureGlobals=64u;
const uint terrainModuleRows=3u,terrainModuleLayerRows=2u,terrainGeoRows=14u,terrainGlobalRecordRows=29u;
const uint terrainClusterHeaderRows=2u,terrainClusterRows=terrainClusterHeaderRows+terrainNoiseRows,terrainVariantRows=36u;
const uint terrainResampleHeaderRows=2u,terrainResampleRows=terrainResampleHeaderRows+terrainNoiseRows;
float terrainNoise(vec2 p);
vec2 terrainPbrRefractionUv=vec2(0);
#ifndef TERRAIN_SURFACE_CAMERA_POSITION
#define TERRAIN_SURFACE_CAMERA_POSITION frame.cameraPositionNear.xyz
#endif
struct TerrainPBRResult {
  vec4 color;
  vec3 normalWorld;
  float roughness,metallic,occlusion;
  vec3 emission;
  vec3 transmission;
  bool valid;
  float height;
};
vec3 terrainPbrSafeNormal(vec3 n,vec3 fallback) {return dot(n,n)>1e-12?normalize(n):fallback;}
// The derivative frame follows increasing projected U/V in world space. This
// handles signed projections, mirrored UVs, rotations and nonuniform object
// scaling without assuming an object-to-world matrix exists in the fragment.
vec3 terrainPbrNormalGrad(vec3 tangentNormal,vec2 ux,vec2 uy,vec3 px,vec3 py,vec3 n) {
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
vec3 terrainPbrNormal(vec3 tangentNormal,vec2 uv,vec3 worldPosition,vec3 n) {
  return terrainPbrNormalGrad(tangentNormal,dFdx(uv),dFdy(uv),dFdx(worldPosition),dFdy(worldPosition),n);
}
vec3 terrainPbrGeometricNormal(vec4 options,vec3 worldPosition,vec3 worldNormal){
  vec3 n=terrainPbrSafeNormal(worldNormal,vec3(0,1,0));
  if(options.w>.5){vec3 face=terrainPbrSafeNormal(cross(dFdx(worldPosition),dFdy(worldPosition)),n);n=dot(face,n)<0.0?-face:face;}
  return n;
}
void terrainPbrProjection(vec4 options,vec3 localPosition,vec3 worldPosition,vec3 worldNormal,out vec3 p,out vec3 n,out vec3 axes){
  n=terrainPbrGeometricNormal(options,worldPosition,worldNormal);
  p=options.y>1.5?worldPosition:localPosition;
  vec3 projectedNormal=terrainPbrSafeNormal(cross(dFdx(p),dFdy(p)),vec3(0,1,0));
  axes=pow(abs(projectedNormal),vec3(max(options.z,.01)));
  axes/=max(dot(axes,vec3(1)),1e-8);if(options.y<.5)axes=vec3(0,1,0);
}
vec2 terrainPbrUv(vec2 coordinates,vec4 transform,float angle) {
  float radiansAngle=radians(angle),c=cos(radiansAngle),s=sin(radiansAngle);
  // Offsets use projected meters, like TerrainLayer tileOffset. Translate in
  // that space before converting tile sizes to repeats and rotating UV axes.
  vec2 tiled=(coordinates+transform.zw)/max(transform.xy,vec2(.0001));
  return mat2(c,s,-s,c)*tiled;
}
vec4 terrainPbrFetchGrad(uint mapRow,vec2 uv,vec2 dx,vec2 dy,float quality) {
  vec4 binding=terrainSurfaceRows[mapRow];
  if(binding.x<0.0)return vec4(1);
  uint slot=uint(binding.x+.5);
  if(quality<.5)return textureLod(TERRAIN_SURFACE_MAP(slot),uv,min(1.0,float(textureQueryLevels(TERRAIN_SURFACE_MAP(slot))-1)));
  vec4 value=textureGrad(TERRAIN_SURFACE_MAP(slot),uv,dx,dy);
  if(quality>1.5){
    value=vec4(0);
    for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)
      value+=textureGrad(TERRAIN_SURFACE_MAP(slot),uv+.25*(float(x)*dx+float(y)*dy),dx,dy)*.25;
  }
  return value;
}
vec4 terrainPbrFetch(uint mapRow,vec2 uv,float quality) {
  return terrainPbrFetchGrad(mapRow,uv,dFdx(uv),dFdy(uv),quality);
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
// M05 scale is frequency in repeats/metre, independent of layer tile sizes.
// Projection principles: Unity Shader Graph 17.0 Triplanar Node:
// https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Triplanar-Node.html
vec3 terrainPbrNoiseModulation(uint row,bool distant,float participation,vec3 localPosition,vec3 worldPosition,float axisContrast) {
  vec4 noise=terrainSurfaceRows[row];
  if(noise.x<.5||noise.w==0.0||participation==0.0)return vec3(1);
  vec4 fade=terrainSurfaceRows[row+1u];
  float distanceToCamera=length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION);
  float amount=smoothstep(fade.x,fade.y,distanceToCamera);
  amount=distant?amount:1.0-amount;
  // Fade is spatially varying: retain derivative-safe sampling even where it
  // reaches zero. Enabled/strength/participation are uniform draw properties.
  vec3 p=noise.y>1.5?worldPosition:localPosition;
  vec3 normal=terrainPbrSafeNormal(cross(dFdx(p),dFdy(p)),vec3(0,1,0));
  vec3 axes=pow(abs(normal),vec3(max(axisContrast,.01)));
  axes/=max(dot(axes,vec3(1)),1e-8);
  if(noise.y<.5)axes=vec3(0,1,0);
  vec3 value=vec3(0);
  for(uint axis=0u;axis<3u;++axis){
    if(noise.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 uv=projected*noise.z;
    vec4 sampled=terrainPbrFetch(row+2u,uv,1.0);
    vec3 mapped=terrainSurfaceRows[row+2u].y>3.5?terrainPbrColor(row+2u,sampled).rgb:vec3(terrainPbrScalar(row+2u,sampled));
    value+=mapped*axes[axis];
  }
  return max(vec3(0),vec3(1)+(value-vec3(.5))*2.0*noise.w*participation*amount);
}
// RNM in one geometric frame. Quaternion form avoids division by 1+A.z
// in Unity Shader Graph 17.0 Normal Blend; antipodal normals use a fixed axis.
// https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Blend-Node.html
vec3 terrainPbrRnm(vec3 base,vec3 detail){
  base=terrainPbrSafeNormal(base,vec3(0,0,1));
  vec4 q=base.z<-.999999?vec4(1,0,0,0):normalize(vec4(-base.y,base.x,0,1.0+base.z));
  return terrainPbrSafeNormal(detail+2.0*cross(q.xyz,cross(q.xyz,detail)+q.w*detail),base);
}
void terrainPbrNoiseNormal(inout vec3 accumulated,uint row,float participation,vec3 localPosition,vec3 worldPosition,vec3 geometricNormal,float axisContrast){
  vec4 noise=terrainSurfaceRows[row],map=terrainSurfaceRows[row+2u];
  if(noise.x<.5||noise.w==0.0||participation==0.0||map.w==0.0||map.x<0.0)return;
  geometricNormal=terrainPbrSafeNormal(geometricNormal,vec3(0,1,0));
  vec4 fade=terrainSurfaceRows[row+1u];
  float amount=1.0-smoothstep(fade.x,fade.y,length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION));
  vec3 p=noise.y>1.5?worldPosition:localPosition;
  vec3 projectionNormal=terrainPbrSafeNormal(cross(dFdx(p),dFdy(p)),vec3(0,1,0));
  vec3 axes=pow(abs(projectionNormal),vec3(max(axisContrast,.01)));
  axes/=max(dot(axes,vec3(1)),1e-8);if(noise.y<.5)axes=vec3(0,1,0);
  vec3 detailWorld=vec3(0);
  float strength=map.w*noise.w*participation*amount;
  for(uint axis=0u;axis<3u;++axis){
    if(noise.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 uv=projected*noise.z;
    vec4 rawNormal=terrainPbrFetch(row+2u,uv,1.0);vec3 encoded=map.y>6.5?vec3(rawNormal.ag,1):rawNormal.xyz;
    vec3 decoded=mix(terrainSurfaceRows[row+5u].xyz,terrainSurfaceRows[row+6u].xyz,encoded)*terrainSurfaceRows[row+4u].xyz;
    if(map.z>.5)decoded=vec3(1)-decoded;
    decoded=decoded*2.0-1.0;
    if(map.y>5.5)decoded.z=sqrt(max(1.0-dot(decoded.xy,decoded.xy),0.0));
    // Unity Shader Graph 17.0 Normal Strength: zero is exactly flat.
    decoded=vec3(decoded.xy*strength,mix(1.0,decoded.z,clamp(strength,0.0,1.0)));
    detailWorld+=terrainPbrNormal(terrainPbrSafeNormal(decoded,vec3(0,0,1)),uv,worldPosition,geometricNormal)*axes[axis];
  }
  detailWorld=terrainPbrSafeNormal(detailWorld,geometricNormal);
  // A deterministic orthonormal frame shared by base and every noise avoids
  // mixing tangent coordinates from different triplanar projections.
  vec3 helper=abs(geometricNormal.z)<.999?vec3(0,0,1):vec3(0,1,0);
  vec3 t=normalize(cross(helper,geometricNormal)),b=cross(geometricNormal,t);
  vec3 base=vec3(dot(accumulated,t),dot(accumulated,b),dot(accumulated,geometricNormal));
  vec3 detail=vec3(dot(detailWorld,t),dot(detailWorld,b),dot(detailWorld,geometricNormal));
  vec3 combined=terrainPbrRnm(base,detail);
  accumulated=terrainPbrSafeNormal(t*combined.x+b*combined.y+geometricNormal*combined.z,geometricNormal);
}
void terrainPbrApplyNoises(inout TerrainPBRResult layer,uint advanced,uint index,vec3 localPosition,vec3 worldPosition,vec3 geometricNormal,float axisContrast){
  vec4 header=terrainSurfaceRows[advanced];
  if(header.x!=1.0||index>=uint(header.y+.5))return;
  uint globalRow=uint(header.z+.5),directory=uint(header.w+.5)+index*terrainAdvancedLayerRows;
  vec4 entry=terrainSurfaceRows[directory];
  uint localRow=uint(entry.x+.5);
  vec4 participation=terrainSurfaceRows[uint(entry.y+.5)];
  vec3 modulation=terrainPbrNoiseModulation(globalRow,false,participation.x,localPosition,worldPosition,axisContrast);
  modulation*=terrainPbrNoiseModulation(globalRow+terrainNoiseRows,true,participation.y,localPosition,worldPosition,axisContrast);
  modulation*=terrainPbrNoiseModulation(localRow,false,1.0,localPosition,worldPosition,axisContrast);
  modulation*=terrainPbrNoiseModulation(localRow+terrainNoiseRows,true,1.0,localPosition,worldPosition,axisContrast);
  uint features=uint(terrainSurfaceRows[advanced+1u].w+.5);
  if((features&terrainFeatureM06)!=0u){
    vec4 normalParticipation=terrainSurfaceRows[uint(entry.w+.5)];
    for(uint i=0u;i<3u;++i){
      terrainPbrNoiseNormal(layer.normalWorld,globalRow+2u*terrainNoiseRows+i*terrainNoiseRows,normalParticipation[i],localPosition,worldPosition,geometricNormal,axisContrast);
      terrainPbrNoiseNormal(layer.normalWorld,localRow+2u*terrainNoiseRows+i*terrainNoiseRows,1.0,localPosition,worldPosition,geometricNormal,axisContrast);
    }
    vec4 antiBinding=terrainSurfaceRows[uint(entry.z+.5)];
    if((features&terrainFeatureAntiTile)!=0u&&antiBinding.y>.5){
      uint antiRow=uint(antiBinding.x+.5);
      vec4 antiParticipation=terrainSurfaceRows[uint(entry.z+.5)+1u];
      terrainPbrNoiseNormal(layer.normalWorld,antiRow,antiParticipation.x,localPosition,worldPosition,geometricNormal,axisContrast);
      modulation*=terrainPbrNoiseModulation(antiRow+terrainNoiseRows,false,antiParticipation.y,localPosition,worldPosition,axisContrast);
      modulation*=terrainPbrNoiseModulation(antiRow+2u*terrainNoiseRows,true,antiParticipation.z,localPosition,worldPosition,axisContrast);
    }
  }
  layer.color.rgb*=modulation;
}
vec3 terrainPbrMappedNormalGrad(uint normalRow,vec2 uv,vec2 dx,vec2 dy,vec3 px,vec3 py,vec3 n){
    vec4 normalBinding=terrainSurfaceRows[normalRow];
    vec3 normal=n;
    if(normalBinding.x>=0.0){
      vec4 encoded=terrainPbrFetchGrad(normalRow,uv,dx,dy,1.0);
      if(normalBinding.y>6.5)encoded.xyz=vec3(encoded.ag,1);
      vec3 decoded=mix(terrainSurfaceRows[normalRow+3u].xyz,terrainSurfaceRows[normalRow+4u].xyz,encoded.xyz)*terrainSurfaceRows[normalRow+2u].xyz;
      if(normalBinding.z>.5)decoded=vec3(1)-decoded;
      decoded=decoded*2.0-1.0;
      if(normalBinding.y>5.5)decoded.z=sqrt(max(1.0-dot(decoded.xy,decoded.xy),0.0));
      decoded.xy*=normalBinding.w;
      if(normalBinding.w==0.0)decoded=vec3(0,0,1);
      normal=terrainPbrNormalGrad(terrainPbrSafeNormal(decoded,vec3(0,0,1)),dx,dy,px,py,n);
    }
    return normal;
}
vec3 terrainPbrMappedNormal(uint normalRow,vec2 uv,vec3 worldPosition,vec3 n){
  return terrainPbrMappedNormalGrad(normalRow,uv,dFdx(uv),dFdy(uv),dFdx(worldPosition),dFdy(worldPosition),n);
}
TerrainPBRResult terrainPbrLayer(uint row,vec3 localPosition,vec3 worldPosition,vec3 worldNormal) {
  TerrainPBRResult result;result.color=vec4(0);result.normalWorld=vec3(0);result.roughness=0;result.metallic=0;result.occlusion=0;result.emission=vec3(0);result.transmission=vec3(0);result.height=0;result.valid=true;
  vec4 options=terrainSurfaceRows[row],transform=terrainSurfaceRows[row+1u],blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];
  vec3 p,n,axes;terrainPbrProjection(options,localPosition,worldPosition,worldNormal,p,n,axes);
  // All projections and derivatives are evaluated before weight branches.
  for(uint axis=0u;axis<3u;++axis){
    // Projection is uniform for the complete draw/layer. UV mode needs only
    // XZ, so omitting the other axes cannot make derivatives divergent.
    if(options.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 uv=terrainPbrUv(projected,transform,blend.x)+terrainPbrRefractionUv;
    uint maps=row+5u;
    vec4 albedo=terrainPbrColor(maps,terrainPbrFetch(maps,uv,1.0));
    uint normalRow=maps+5u;
    vec3 normal=terrainPbrMappedNormal(normalRow,uv,worldPosition,n);
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
// M07 changes only the selected albedo/normal channels and optionally the
// near/far height comparison. Far UVs multiply the complete layer transform.
void terrainPbrResample(inout TerrainPBRResult layer,uint layerRow,uint resampleRow,vec3 localPosition,vec3 worldPosition,vec3 worldNormal){
  vec4 options=terrainSurfaceRows[resampleRow],distanceOptions=terrainSurfaceRows[resampleRow+1u];
  if((options.x<.5&&options.y<.5&&options.z<.5)||options.w==1.0)return;
  float amount=distanceOptions.z<.5?1.0:smoothstep(distanceOptions.x,distanceOptions.y,length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION));
  if(distanceOptions.z>.5&&distanceOptions.z<1.5){
    // Disabled legacy height-only noise is a neutral perturbation, preserving
    // compatibility with assets accepted before M07 (equivalent to Crossfade).
    vec3 modulation=terrainPbrNoiseModulation(resampleRow+terrainResampleHeaderRows,false,1.0,localPosition,worldPosition,terrainSurfaceRows[layerRow].z);
    float perturbation=dot(modulation-vec3(1),vec3(.2126,.7152,.0722));
    amount=clamp(amount+perturbation*amount*(1.0-amount),0.0,1.0);
  }
  vec4 projection=terrainSurfaceRows[layerRow],transform=terrainSurfaceRows[layerRow+1u],blend=terrainSurfaceRows[layerRow+2u],height=terrainSurfaceRows[layerRow+3u];
  vec3 p,n,axes;terrainPbrProjection(projection,localPosition,worldPosition,worldNormal,p,n,axes);
  vec4 farColor=vec4(0);vec3 farNormal=vec3(0);float farHeight=0.0;
  uint maps=layerRow+5u;
  // Sampling remains derivative-safe throughout the spatial distance fade.
  for(uint axis=0u;axis<3u;++axis){
    if(projection.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 uv=terrainPbrUv(projected,transform,blend.x)*options.w+terrainPbrRefractionUv;
    if(options.x>.5)farColor+=terrainPbrColor(maps,terrainPbrFetch(maps,uv,1.0))*axes[axis];
    if(options.y>.5)farNormal+=terrainPbrMappedNormal(maps+5u,uv,worldPosition,n)*axes[axis];
    if(options.z>.5)farHeight+=terrainPbrScalar(maps+10u,terrainPbrFetch(maps+10u,uv,height.z))*axes[axis];
  }
  if(options.z>.5){
    float nearWeight=1.0-amount,farWeight=amount;
    float nearScore=nearWeight+layer.height*height.y,farScore=farWeight+farHeight*height.y;
    float peak=max(nearScore,farScore),width=max(height.x,.0001),contrast=max(blend.w,.0001);
    nearWeight*=pow(clamp((nearScore-peak+width)/width,0.0,1.0),contrast);
    farWeight*=pow(clamp((farScore-peak+width)/width,0.0,1.0),contrast);
    float total=nearWeight+farWeight;
    if(total>1e-8)amount=farWeight/total;
    layer.height=mix(layer.height,farHeight,amount);
  }
  if(options.x>.5){
    float luminance=dot(farColor.rgb,vec3(.2126,.7152,.0722));
    farColor.rgb=mix(vec3(luminance),farColor.rgb,blend.y);
    farColor*=terrainSurfaceRows[layerRow+4u];
    layer.color=mix(layer.color,farColor,amount);
  }
  if(options.y>.5)layer.normalWorld=terrainPbrSafeNormal(mix(layer.normalWorld,terrainPbrSafeNormal(farNormal,n),amount),n);
}
// M08 contribution budgets are total budgets, never variants times stochastic
// samples. High uses shared triangular-lattice vertices. Medium uses a scalar
// lattice; Low stochastic uses one fixed variant with a continuous UV warp.
// Inspired by UnityLabs' historical prototype; no histogram-preservation claim:
// https://github.com/UnityLabs/procedural-stochastic-texturing
struct TerrainClusterPlan {uvec3 variant;vec3 weight;vec2 shift[3];uint count;};
vec2 terrainPbrClusterHash(vec2 vertex){
  return fract(sin(vec2(dot(vertex,vec2(127.1,311.7)),dot(vertex,vec2(269.5,183.3))))*43758.5453);
}
TerrainClusterPlan terrainPbrClusterPlan(vec2 grid,float selector,uint variants,uint quality,bool stochastic,float contrast,float noiseBoost){
  TerrainClusterPlan plan;plan.variant=uvec3(0);plan.weight=vec3(0);plan.count=quality+1u;
  for(uint i=0u;i<3u;++i)plan.shift[i]=vec2(0);
  if(!stochastic){
    if(quality==0u){plan.variant.x=min(uint(clamp(selector,0.0,1.0)*float(variants)),variants-1u);plan.weight.x=1.0;}
    else if(quality==1u){
      float index=selector*float(variants-1u);uint first=min(uint(floor(index)),variants-1u);
      plan.variant=uvec3(first,min(first+1u,variants-1u),0u);
      float t=smoothstep(0.0,1.0,fract(index));plan.weight=vec3(1.0-t,t,0);
    }else{
      plan.count=variants;
      for(uint i=0u;i<variants;++i){plan.variant[i]=i;float center=float(i)/max(float(variants-1u),1.0);plan.weight[i]=1.0/(1.0+abs(selector-center)*float(variants*variants));}
    }
  }else if(quality==0u){plan.variant.x=0u;plan.weight.x=1.0;}
  else{
    vec2 vertex[3];
    if(quality==1u){
      float position=dot(grid,vec2(.754877666,.569840296))+selector;
      float cell=floor(position),t=smoothstep(0.0,1.0,fract(position));
      vertex[0]=vec2(cell,0);vertex[1]=vec2(cell+1.0,0);vertex[2]=vec2(0);
      plan.weight=vec3(1.0-t,t,0);
    }else{
      vec2 cell=floor(grid),f=fract(grid);
      if(f.x+f.y<=1.0){vertex[0]=cell;vertex[1]=cell+vec2(1,0);vertex[2]=cell+vec2(0,1);plan.weight=vec3(1.0-f.x-f.y,f.x,f.y);}
      else{vertex[0]=cell+vec2(1,1);vertex[1]=cell+vec2(0,1);vertex[2]=cell+vec2(1,0);plan.weight=vec3(f.x+f.y-1.0,1.0-f.x,1.0-f.y);}
    }
    for(uint i=0u;i<plan.count;++i){
      vec2 random=terrainPbrClusterHash(vertex[i]);
      plan.variant[i]=min(uint(random.x*float(variants)),variants-1u);plan.shift[i]=(random-.5)*2.0;
      float center=float(plan.variant[i])/max(float(variants-1u),1.0);
      // Positive affinity preserves shared-edge continuity and survives large
      // contrast/noiseBoost without dropping every contribution to zero.
      plan.weight[i]/=1.0+abs(selector-center)*noiseBoost;
    }
  }
  float peak=max(max(plan.weight.x,plan.weight.y),plan.weight.z);
  plan.weight=pow(max(plan.weight/max(peak,1e-30),vec3(0)),vec3(max(contrast,.0001)));
  plan.weight/=max(dot(plan.weight,vec3(1)),1e-30);
  return plan;
}
vec2 terrainPbrRotate(vec2 value,float angle){float a=radians(angle),c=cos(a),s=sin(a);return mat2(c,s,-s,c)*value;}
// All UV/world gradients are captured before choosing cells/variants. The
// selected maps, near/far UVs and normal frames use explicit gradients only.
TerrainPBRResult terrainPbrCluster(uint layerRow,uint clusterRow,uint resampleRow,vec3 localPosition,vec3 worldPosition,vec3 worldNormal){
  vec4 cluster=terrainSurfaceRows[clusterRow],settings=terrainSurfaceRows[clusterRow+1u];
  vec4 projection=terrainSurfaceRows[layerRow],layerTransform=terrainSurfaceRows[layerRow+1u],blend=terrainSurfaceRows[layerRow+2u],height=terrainSurfaceRows[layerRow+3u];
  vec3 p,n,axes;terrainPbrProjection(projection,localPosition,worldPosition,worldNormal,p,n,axes);
  vec3 px=dFdx(worldPosition),py=dFdy(worldPosition);
  vec3 selectionModulation=terrainPbrNoiseModulation(clusterRow+terrainClusterHeaderRows,false,1.0,localPosition,worldPosition,projection.z);
  float selectionDelta=dot(selectionModulation-vec3(1),vec3(.2126,.7152,.0722));
  uint variants=cluster.x>.5?uint(cluster.z+.5):1u,quality=uint(cluster.w+.5);
  uint variantsRow=uint(settings.z+.5);
  float gridScale=cluster.x>.5?terrainSurfaceRows[variantsRow].x:max(layerTransform.x,layerTransform.y);
  vec4 resample=vec4(0),distanceOptions=vec4(0);float farAmount=0.0;
  if(resampleRow!=0u){resample=terrainSurfaceRows[resampleRow];distanceOptions=terrainSurfaceRows[resampleRow+1u];
    farAmount=distanceOptions.z<.5?1.0:smoothstep(distanceOptions.x,distanceOptions.y,length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION));
    if(distanceOptions.z>.5&&distanceOptions.z<1.5){vec3 modulation=terrainPbrNoiseModulation(resampleRow+terrainResampleHeaderRows,false,1.0,localPosition,worldPosition,projection.z);farAmount=clamp(farAmount+dot(modulation-vec3(1),vec3(.2126,.7152,.0722))*farAmount*(1.0-farAmount),0.0,1.0);}
  }
  bool sampleFar=resampleRow!=0u&&resample.w!=1.0&&(resample.x>.5||resample.y>.5||resample.z>.5);
  TerrainPBRResult result;result.color=vec4(0);result.normalWorld=vec3(0);result.roughness=0;result.metallic=0;result.occlusion=0;result.emission=vec3(0);result.transmission=vec3(0);result.height=0;result.valid=true;
  vec4 farColor=vec4(0);vec3 farNormal=vec3(0);float farHeight=0.0;
  for(uint axis=0u;axis<3u;++axis){
    if(projection.y<.5&&axis!=1u)continue;
    vec2 projected=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 coordinates=projected+layerTransform.zw;
    float procedural=.5+.5*sin(dot(coordinates/max(gridScale,.0001),vec2(.61,.47)))*cos(dot(coordinates/max(gridScale,.0001),vec2(-.31,.79)));
    float selector=clamp(.5+(procedural-.5)*(1.0+settings.y)+selectionDelta*.5,0.0,1.0);
    if(cluster.y<.5)selector=pow(selector,max(settings.x,.0001));
    if(cluster.y>.5&&quality==0u){
      // One fetch cannot blend arbitrary variants seamlessly. Low stochastic
      // keeps variant zero and continuously warps its metre coordinates.
      coordinates+=sin(coordinates/max(gridScale,.0001)+vec2(selector,-selector))*gridScale*.25/(1.0+settings.x);
    }
    vec2 dx=dFdx(coordinates),dy=dFdy(coordinates);
    TerrainClusterPlan plan=terrainPbrClusterPlan(coordinates/max(gridScale,.0001),selector,variants,quality,cluster.y>.5,settings.x,settings.y);
    for(uint contribution=0u;contribution<plan.count;++contribution){
      float weight=axes[axis]*plan.weight[contribution];if(weight<=0.0)continue;
      uint variantRow=variantsRow+plan.variant[contribution]*terrainVariantRows;
      vec4 transform=cluster.x>.5?terrainSurfaceRows[variantRow]:vec4(1,0,0,0);
      vec2 tile=cluster.x>.5?vec2(transform.x):layerTransform.xy;
      uint maps=cluster.x>.5?variantRow+1u:layerRow+5u;
      vec2 shifted=coordinates+transform.yz+plan.shift[contribution]*tile;
      float angle=blend.x+transform.w;
      vec2 uv=terrainPbrRotate(shifted/max(tile,vec2(.0001)),angle)+terrainPbrRefractionUv;
      vec2 ux=terrainPbrRotate(dx/max(tile,vec2(.0001)),angle),uy=terrainPbrRotate(dy/max(tile,vec2(.0001)),angle);
      result.color+=terrainPbrColor(maps,terrainPbrFetchGrad(maps,uv,ux,uy,1.0))*weight;
      result.normalWorld+=terrainPbrMappedNormalGrad(maps+5u,uv,ux,uy,px,py,n)*weight;
      result.height+=terrainPbrScalar(maps+10u,terrainPbrFetchGrad(maps+10u,uv,ux,uy,height.z))*weight;
      result.occlusion+=terrainPbrScalar(maps+15u,terrainPbrFetchGrad(maps+15u,uv,ux,uy,1.0))*weight;
      result.roughness+=terrainPbrScalar(maps+20u,terrainPbrFetchGrad(maps+20u,uv,ux,uy,1.0))*weight;
      result.metallic+=terrainPbrScalar(maps+25u,terrainPbrFetchGrad(maps+25u,uv,ux,uy,1.0))*weight;
      result.emission+=terrainPbrColor(maps+30u,terrainPbrFetchGrad(maps+30u,uv,ux,uy,1.0)).rgb*weight;
      if(sampleFar){
        vec2 farUv=uv*resample.w,farDx=ux*resample.w,farDy=uy*resample.w;
        if(resample.x>.5)farColor+=terrainPbrColor(maps,terrainPbrFetchGrad(maps,farUv,farDx,farDy,1.0))*weight;
        if(resample.y>.5)farNormal+=terrainPbrMappedNormalGrad(maps+5u,farUv,farDx,farDy,px,py,n)*weight;
        if(resample.z>.5)farHeight+=terrainPbrScalar(maps+10u,terrainPbrFetchGrad(maps+10u,farUv,farDx,farDy,height.z))*weight;
      }
    }
  }
  result.normalWorld=terrainPbrSafeNormal(result.normalWorld,n);
  if(sampleFar){
    if(resample.z>.5){float nearWeight=1.0-farAmount,farWeight=farAmount;float nearScore=nearWeight+result.height*height.y,farScore=farWeight+farHeight*height.y;float peak=max(nearScore,farScore),width=max(height.x,.0001),contrast=max(blend.w,.0001);nearWeight*=pow(clamp((nearScore-peak+width)/width,0.0,1.0),contrast);farWeight*=pow(clamp((farScore-peak+width)/width,0.0,1.0),contrast);float total=nearWeight+farWeight;if(total>1e-8)farAmount=farWeight/total;result.height=mix(result.height,farHeight,farAmount);}
    if(resample.x>.5)result.color=mix(result.color,farColor,farAmount);
    if(resample.y>.5)result.normalWorld=terrainPbrSafeNormal(mix(result.normalWorld,terrainPbrSafeNormal(farNormal,n),farAmount),n);
  }
  float luminance=dot(result.color.rgb,vec3(.2126,.7152,.0722));result.color.rgb=mix(vec3(luminance),result.color.rgb,blend.y);result.color*=terrainSurfaceRows[layerRow+4u];
  return result;
}
// M09/M10 sample shared global inputs once, then apply layer participation.
// Unity SG17 Sample Texture 2D / Blend / Position; source units and channels
// are recorded in M09-M10-SOURCE-CONTRACT.md. Global fades/ranges are linear.
struct TerrainModuleSamples {
  vec4 geoColor;vec4 geoNormal;vec2 geoUv;float geoStrength;
  vec4 raw[4];vec4 saom[4];vec2 uv[4];vec2 dx[4];vec2 dy[4];vec4 strength;
  vec3 px;vec3 py;
};
float terrainPbrRamp(float start,float end,float value){return end>start?clamp((value-start)/(end-start),0.0,1.0):(value>=end?1.0:0.0);}
float terrainPbrCurve(uint root,float time){
  vec4 curve=terrainSurfaceRows[root+1u];uint count=uint(curve.z+.5);if(count<2u)return 0.0;
  float point=clamp(time,0.0,1.0)*float(count-1u);uint a=min(uint(point),count-1u),b=min(a+1u,count-1u),offset=uint(curve.y+.5);
  return mix(terrainSurfaceRows[offset+a/4u][a%4u],terrainSurfaceRows[offset+b/4u][b%4u],fract(point));
}
TerrainModuleSamples terrainPbrSampleModules(uint root,vec3 localPosition,vec3 worldPosition){
  TerrainModuleSamples samples;samples.geoColor=vec4(1);samples.geoNormal=vec4(.5,.5,1,1);samples.geoUv=vec2(0);samples.geoStrength=0;samples.strength=vec4(0);
  samples.px=dFdx(worldPosition);samples.py=dFdy(worldPosition);
  for(uint role=0u;role<4u;++role){samples.raw[role]=vec4(1);samples.saom[role]=vec4(1);samples.uv[role]=vec2(0);samples.dx[role]=vec2(0);samples.dy[role]=vec2(0);}
  vec4 directory=terrainSurfaceRows[root];
  if(directory.y>.5){
    uint row=uint(directory.y+.5);vec4 g=terrainSurfaceRows[row],position=terrainSurfaceRows[row+1u],curve=terrainSurfaceRows[row+3u];
    float vertical=(worldPosition.y+position.y)/position.x;
    if(position.w>.5){float a=radians(curve.z);float coordinate=dot((worldPosition.xz+vec2(curve.y))/curve.x,vec2(cos(a),sin(a)));vertical+=terrainPbrCurve(root,coordinate);}
    samples.geoUv=vec2(0,vertical);
    if(g.z>0.0){
      uint map=row+4u;samples.geoColor=terrainPbrColor(map,terrainPbrFetch(map,samples.geoUv,1.0));
      samples.geoStrength=g.z*samples.geoColor.a;
      if(position.z>.5){vec4 bounds=terrainSurfaceRows[row+2u];samples.geoStrength*=terrainPbrRamp(bounds.x,bounds.y,worldPosition.y)*(1.0-terrainPbrRamp(bounds.z,bounds.w,worldPosition.y));}
      if(g.x>1.5&&g.w>0.0&&terrainSurfaceRows[row+9u].w>0.0)samples.geoNormal=terrainPbrFetch(row+9u,samples.geoUv,1.0);
    }
  }
  if(directory.z>.5){
    uint first=uint(directory.z+.5);vec4 domain=terrainSurfaceRows[root+2u];vec2 globalUv=(localPosition.xz-domain.xy)/domain.zw;
    float cameraDistance=length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION);
    for(uint role=0u;role<4u;++role){
      uint row=first+role*terrainGlobalRecordRows;vec4 g=terrainSurfaceRows[row],fade=terrainSurfaceRows[row+1u],transform=terrainSurfaceRows[row+2u];
      samples.uv[role]=(globalUv+transform.zw)*transform.xy;
      samples.dx[role]=dFdx(samples.uv[role]);samples.dy[role]=dFdy(samples.uv[role]);
      float strength=g.z*mix(fade.z,fade.w,terrainPbrRamp(fade.x,fade.y,cameraDistance));
      if(g.x<.5||strength==0.0)continue;samples.strength[role]=strength;
      uint map=row+4u;
      if(role==2u){
        for(uint channel=0u;channel<4u;++channel){
          uint channelMap=map+5u+channel*5u;float slot=terrainSurfaceRows[channelMap].x;bool reused=false;
          for(uint prior=0u;prior<channel;++prior)if(terrainSurfaceRows[map+5u+prior*5u].x==slot){samples.saom[channel]=samples.saom[prior];reused=true;break;}
          if(!reused)samples.saom[channel]=terrainPbrFetchGrad(channelMap,samples.uv[role],samples.dx[role],samples.dy[role],1.0);
        }
      }else samples.raw[role]=terrainPbrFetchGrad(map,samples.uv[role],samples.dx[role],samples.dy[role],1.0);
    }
  }
  return samples;
}
vec3 terrainPbrColorBlend(vec3 base,vec3 value,uint mode){
  if(mode==0u)return base*value*2.0;
  if(mode==1u)return mix(2.0*base*value,vec3(1)-2.0*(vec3(1)-base)*(vec3(1)-value),step(vec3(.5),base));
  if(mode==2u)return dot(base,vec3(1))>dot(value,vec3(1))?base:value;
  return value;
}
vec3 terrainPbrDecodedNormal(uint map,vec4 sampleValue,float strength){
  if(terrainSurfaceRows[map].y>6.5)sampleValue.xyz=vec3(sampleValue.ag,1);
  vec3 decoded=mix(terrainSurfaceRows[map+3u].xyz,terrainSurfaceRows[map+4u].xyz,sampleValue.xyz)*terrainSurfaceRows[map+2u].xyz;
  if(terrainSurfaceRows[map].z>.5)decoded=vec3(1)-decoded;
  decoded=decoded*2.0-1.0;if(terrainSurfaceRows[map].y>5.5)decoded.z=sqrt(max(1.0-dot(decoded.xy,decoded.xy),0.0));
  strength*=terrainSurfaceRows[map].w;
  return terrainPbrSafeNormal(vec3(decoded.xy*strength,mix(1.0,decoded.z,clamp(strength,0.0,1.0))),vec3(0,0,1));
}
vec3 terrainPbrStableDetail(vec3 tangentNormal,vec3 n){
  vec3 helper=abs(n.x)<.999?vec3(1,0,0):vec3(0,0,1);vec3 t=normalize(helper-n*dot(helper,n)),b=cross(n,t);
  return terrainPbrSafeNormal(t*tangentNormal.x+b*tangentNormal.y+n*tangentNormal.z,n);
}
vec3 terrainPbrCombineNormal(vec3 base,vec3 detail,vec3 n){
  vec3 helper=abs(n.z)<.999?vec3(0,0,1):vec3(0,1,0);vec3 t=normalize(cross(helper,n)),b=cross(n,t);
  vec3 a=vec3(dot(base,t),dot(base,b),dot(base,n)),d=vec3(dot(detail,t),dot(detail,b),dot(detail,n));vec3 result=terrainPbrRnm(a,d);
  return terrainPbrSafeNormal(t*result.x+b*result.y+n*result.z,n);
}
void terrainPbrApplyModules(inout TerrainPBRResult layer,uint root,uint index,TerrainModuleSamples samples,vec3 n){
  vec4 directory=terrainSurfaceRows[root];uint controls=uint(directory.w+.5)+index*terrainModuleLayerRows;
  if(directory.y>.5){
    uint row=uint(directory.y+.5);vec4 g=terrainSurfaceRows[row],participation=terrainSurfaceRows[controls];
    float amount=samples.geoStrength*participation.x;
    if(participation.y>.5){float geoFilter=clamp((abs(participation.z)-layer.height)*terrainSurfaceRows[row+3u].w+.5,0.0,1.0);if(participation.z<0.0)geoFilter=1.0-geoFilter;amount*=max(1.0-participation.w,geoFilter);}
    layer.color.rgb=mix(layer.color.rgb,terrainPbrColorBlend(layer.color.rgb,samples.geoColor.rgb,uint(g.y+.5)),clamp(amount,0.0,1.0));
    if(g.x>1.5&&g.w>0.0&&amount>0.0){vec3 detail=terrainPbrStableDetail(terrainPbrDecodedNormal(row+9u,samples.geoNormal,g.w*amount),n);layer.normalWorld=terrainPbrCombineNormal(layer.normalWorld,detail,n);}
  }
  if(directory.z>.5){
    uint first=uint(directory.z+.5);vec4 participation=terrainSurfaceRows[controls+1u];
    for(uint role=0u;role<4u;++role){
      float amount=samples.strength[role]*participation[role];if(amount==0.0)continue;
      uint row=first+role*terrainGlobalRecordRows,map=row+4u;vec4 g=terrainSurfaceRows[row];
      if(role==0u){vec4 color=terrainPbrColor(map,samples.raw[role]);layer.color.rgb=mix(layer.color.rgb,terrainPbrColorBlend(layer.color.rgb,color.rgb,uint(terrainSurfaceRows[row+3u].x+.5)),clamp(amount*color.a,0.0,1.0));}
      else if(role==1u){
        bool reoriented=g.w<.5;vec3 decoded=terrainPbrDecodedNormal(map,samples.raw[role],reoriented?amount:1.0);
        float determinant=samples.dx[role].x*samples.dy[role].y-samples.dx[role].y*samples.dy[role].x;
        vec3 detail=abs(determinant)<1e-12?terrainPbrStableDetail(decoded,n):terrainPbrNormalGrad(decoded,samples.dx[role],samples.dy[role],samples.px,samples.py,n);
        layer.normalWorld=reoriented?terrainPbrCombineNormal(layer.normalWorld,detail,n):terrainPbrSafeNormal(mix(layer.normalWorld,detail,amount),n);
      }else if(role==2u){
        uint smoothness=map+5u,ao=map+10u,metallic=map+15u,mask=map+20u;
        float blend=amount*terrainPbrScalar(mask,samples.saom[3]);
        layer.roughness=clamp(mix(layer.roughness,1.0-terrainPbrScalar(smoothness,samples.saom[0]),blend),0.0,1.0);
        layer.occlusion=clamp(mix(layer.occlusion,terrainPbrScalar(ao,samples.saom[1]),blend),0.0,1.0);layer.metallic=clamp(mix(layer.metallic,terrainPbrScalar(metallic,samples.saom[2]),blend),0.0,1.0);
      }else layer.emission=max(vec3(0),mix(layer.emission,terrainPbrColor(map,samples.raw[role]).rgb,amount));
    }
  }
}
#include "terrain_snow.glsl"
#include "terrain_static_fx.glsl"
TerrainPBRResult terrainPBRSurface(vec3 worldPosition,vec3 worldNormal) {
  TerrainPBRResult result;result.color=vec4(0);result.normalWorld=worldNormal;result.roughness=1;result.metallic=0;result.occlusion=1;result.emission=vec3(0);result.transmission=vec3(0);result.height=0;result.valid=false;
  if(!aetherHasMaterialExtension()||aetherMaterialRow(14u).z<.5)return result;
  uint offset=uint(aetherMaterialRow(14u).z+.5)-1u;
  vec4 header=terrainSurfaceRows[offset];uint count=min(uint(header.x+.5),32u);
  if(header.y!=1.0||count==0u)return result;
  vec4 weights=max(vColor,vec4(0));
#ifdef TERRAIN_CONTROL_MAP
  if(vUv1.y>=0.0&&aetherMaterialRow(13u).x>.5){vec4 control=aetherMaterialRow(13u);weights=max(textureLod(TERRAIN_CONTROL_MAP(uint(control.y+.5)),vUv0*control.zw+aetherMaterialRow(14u).xy,0.0),vec4(0));}
#endif
  vec3 localPosition=vec3(vUv0.x,vUv1.x,vUv0.y)*terrainSurfaceRows[offset+1u].x;
  uint moduleRoot=terrainSurfaceRows[offset+1u].y>.5?uint(terrainSurfaceRows[offset+1u].y+.5)-1u:0u;
  terrainPbrRefractionUv=vec2(0);
  TerrainFlowSamples flowSamples=terrainPbrPrepareFlow(moduleRoot,localPosition,worldPosition,terrainPbrSafeNormal(worldNormal,vec3(0,1,0)));
  TerrainModuleSamples moduleSamples;if(moduleRoot!=0u)moduleSamples=terrainPbrSampleModules(moduleRoot,localPosition,worldPosition);
  TerrainPBRResult layers[4];float amounts[4];float peak=-1e20;
  // Current control storage is RGBA. Higher catalog layers require additional
  // control pages before they can receive weights; they are not invented here.
  uint supported=min(count,4u);
  for(uint i=0u;i<supported;++i){
    uint row=offset+2u+i*40u;amounts[i]=0.0;if(terrainSurfaceRows[row].x<.5)continue;
    uint advanced=0u,resampleRow=0u,clusterRow=0u;
    if(header.z>.5){advanced=uint(header.z+.5)-1u;uint features=uint(terrainSurfaceRows[advanced+1u].w+.5);uint directory=uint(terrainSurfaceRows[advanced].w+.5)+i*terrainAdvancedLayerRows;
      if((features&terrainFeatureM07)!=0u)resampleRow=uint(terrainSurfaceRows[directory+1u].z+.5);
      if((features&terrainFeatureM08)!=0u)clusterRow=uint(terrainSurfaceRows[directory+1u].w+.5);
    }
    bool useCluster=clusterRow!=0u&&(terrainSurfaceRows[clusterRow].x>.5||terrainSurfaceRows[clusterRow].y>.5);
    if(useCluster)layers[i]=terrainPbrCluster(row,clusterRow,resampleRow,localPosition,worldPosition,worldNormal);
    else{layers[i]=terrainPbrLayer(row,localPosition,worldPosition,worldNormal);if(resampleRow!=0u)terrainPbrResample(layers[i],row,resampleRow,localPosition,worldPosition,worldNormal);}
    if(header.z>.5)terrainPbrApplyNoises(layers[i],advanced,i,localPosition,worldPosition,terrainPbrGeometricNormal(terrainSurfaceRows[row],worldPosition,worldNormal),terrainSurfaceRows[row].z);
    if(moduleRoot!=0u)terrainPbrApplyModules(layers[i],moduleRoot,i,moduleSamples,terrainPbrGeometricNormal(terrainSurfaceRows[row],worldPosition,worldNormal));
    vec4 blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];amounts[i]=weights[i];if(amounts[i]>0.0)peak=max(peak,amounts[i]+layers[i].height*height.y);
  }

  float sum=0;
  for(uint i=0u;i<supported;++i){uint row=offset+2u+i*40u;if(terrainSurfaceRows[row].x<.5)continue;vec4 blend=terrainSurfaceRows[row+2u],height=terrainSurfaceRows[row+3u];if(blend.z>.5){float h=amounts[i]+layers[i].height*height.y;amounts[i]*=pow(clamp((h-peak+height.x)/max(height.x,.0001),0.0,1.0),max(blend.w,.0001));}sum+=amounts[i];}
  bool emptyWeights=sum<1e-8;float sampleSum=max(sum,1e-8);
  // Refraction is a uniform authored capability; no coverage branch encloses
  // texture derivatives. Keep the first evaluation's color/height/weights.
  if(flowSamples.streamRow!=0u&&terrainSurfaceRows[flowSamples.streamRow+1u].x!=0.0){
    float earlyHeight=0;for(uint i=0u;i<supported;++i)if(terrainSurfaceRows[offset+2u+i*40u].x>.5)earlyHeight+=layers[i].height*amounts[i]/sampleSum;
    terrainPbrRefractionUv=terrainPbrStreamRefraction(flowSamples,earlyHeight,worldPosition.y);
    for(uint i=0u;i<supported;++i){uint row=offset+2u+i*40u;if(terrainSurfaceRows[row].x<.5)continue;
      uint advanced=0u,resampleRow=0u,clusterRow=0u;
      if(header.z>.5){advanced=uint(header.z+.5)-1u;uint features=uint(terrainSurfaceRows[advanced+1u].w+.5),directory=uint(terrainSurfaceRows[advanced].w+.5)+i*terrainAdvancedLayerRows;
        if((features&terrainFeatureM07)!=0u)resampleRow=uint(terrainSurfaceRows[directory+1u].z+.5);if((features&terrainFeatureM08)!=0u)clusterRow=uint(terrainSurfaceRows[directory+1u].w+.5);}
      bool useCluster=clusterRow!=0u&&(terrainSurfaceRows[clusterRow].x>.5||terrainSurfaceRows[clusterRow].y>.5);TerrainPBRResult refracted;
      if(useCluster)refracted=terrainPbrCluster(row,clusterRow,resampleRow,localPosition,worldPosition,worldNormal);
      else{refracted=terrainPbrLayer(row,localPosition,worldPosition,worldNormal);if(resampleRow!=0u)terrainPbrResample(refracted,row,resampleRow,localPosition,worldPosition,worldNormal);}
      if(header.z>.5)terrainPbrApplyNoises(refracted,advanced,i,localPosition,worldPosition,terrainPbrGeometricNormal(terrainSurfaceRows[row],worldPosition,worldNormal),terrainSurfaceRows[row].z);
      if(moduleRoot!=0u)terrainPbrApplyModules(refracted,moduleRoot,i,moduleSamples,terrainPbrGeometricNormal(terrainSurfaceRows[row],worldPosition,worldNormal));
      refracted.color=layers[i].color;refracted.height=layers[i].height;layers[i]=refracted;
    }
    terrainPbrRefractionUv=vec2(0);
  }
  if(emptyWeights){result.valid=true;result.color=vec4(0,0,0,1);return result;}
  result.normalWorld=vec3(0);result.roughness=0;result.metallic=0;result.occlusion=0;result.valid=true;
  for(uint i=0u;i<supported;++i){if(terrainSurfaceRows[offset+2u+i*40u].x<.5)continue;float w=amounts[i]/sum;result.color+=layers[i].color*w;result.normalWorld+=layers[i].normalWorld*w;result.roughness+=layers[i].roughness*w;result.metallic+=layers[i].metallic*w;result.occlusion+=layers[i].occlusion*w;result.emission+=layers[i].emission*w;result.height+=layers[i].height*w;}
  float flowSnowClearing=0.0;
  if(moduleRoot!=0u)flowSnowClearing=terrainPbrApplyStaticFx(result,moduleRoot,localPosition,worldPosition,terrainPbrSafeNormal(worldNormal,vec3(0,1,0)),amounts,supported,sum,flowSamples);
  if(moduleRoot!=0u&&terrainSurfaceRows[moduleRoot+1u].w>.5){
    uint snow=uint(terrainSurfaceRows[moduleRoot+1u].w+.5)-1u,controls=uint(terrainSurfaceRows[snow+13u].w+.5);float participation=0.0,porosity=0.0;
    for(uint i=0u;i<supported;++i){float w=amounts[i]/sum;participation+=terrainSurfaceRows[controls+i].x*w;porosity+=terrainSurfaceRows[controls+i].y*w;}
    result.normalWorld=terrainPbrSafeNormal(result.normalWorld,worldNormal);
    terrainPbrApplySnow(result,snow,terrainSurfaceRows[moduleRoot+2u],localPosition,worldPosition,terrainPbrSafeNormal(worldNormal,vec3(0,1,0)),participation,porosity,flowSnowClearing);
  }
  if(moduleRoot!=0u)terrainPbrApplySpecularHeight(result,moduleRoot,worldPosition.y);
  float variation=clamp(vUv1.y<0.0?-vUv1.y-1.0:vUv1.y,0.0,1.0);
  result.color.rgb*=1.0+variation*(terrainNoise(vUv0*.11)-.5)*.6;
  result.normalWorld=terrainPbrSafeNormal(result.normalWorld,worldNormal);result.roughness=clamp(result.roughness,.07,1.0);result.metallic=clamp(result.metallic,0.0,1.0);result.occlusion=clamp(result.occlusion,0.0,1.0);return result;
}
#endif
