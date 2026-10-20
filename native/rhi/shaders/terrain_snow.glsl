// M11/M12 table contract: sixteen headers, eight five-row map records, then
// one participation/porosity row per layer. Included after the PBR helpers.
const uint terrainSnowHeaderRows=16u,terrainSnowMapRows=5u;
vec4 terrainSnowSample(uint row,uint role,vec3 p,vec3 px,vec3 py,vec3 axes,vec4 scale,float multiplier,uint flags,float stochasticScale,float contrast){
  uint map=row+terrainSnowHeaderRows+role*terrainSnowMapRows;vec4 value=vec4(0);
  for(uint axis=0u;axis<3u;++axis){
    if((flags&64u)==0u&&axis!=1u)continue;
    if(axes[axis]<=0.0)continue;
    vec2 q=axis==0u?p.zy:axis==1u?p.xz:p.xy;
    vec2 qx=axis==0u?px.zy:axis==1u?px.xz:px.xy,qy=axis==0u?py.zy:axis==1u?py.xz:py.xy;
    vec2 uv=q*scale.xy+scale.zw,dx=qx*scale.xy,dy=qy*scale.xy;
    // Continuous deterministic warp: gradients deliberately exclude lattice
    // selection/warp derivatives so a cell boundary cannot inflate the mip.
    if((flags&32u)!=0u){vec2 cell=q*stochasticScale;uv+=vec2(terrainNoise(cell),terrainNoise(cell+vec2(37.2,91.7)))*contrast;}
    value+=terrainPbrFetchGrad(map,uv*multiplier,dx*multiplier,dy*multiplier,1.0)*axes[axis];
  }
  return value;
}
void terrainPbrApplySnow(inout TerrainPBRResult material,uint row,vec4 domain,vec3 localPosition,vec3 worldPosition,vec3 geometricNormal,float participation,float porosity,float flowSnowClearing){
  vec4 header=terrainSurfaceRows[row];uint flags=uint(header.z+.5);bool rich=header.y>1.5;
  vec4 core=terrainSurfaceRows[row+3u],uvScale=terrainSurfaceRows[row+4u],heightAngle=terrainSurfaceRows[row+5u],parameters=terrainSurfaceRows[row+6u];
  vec3 up=terrainPbrSafeNormal(terrainSurfaceRows[row+1u].xyz,vec3(0,1,0));
  vec3 p=(flags&16u)!=0u?worldPosition:localPosition;
  // Domain UV is independent of material tiling. World UV uses frequency.
  if((flags&16u)==0u)p.xz=(p.xz-domain.xy)/domain.zw;
  vec3 px=dFdx(p),py=dFdy(p),wx=dFdx(worldPosition),wy=dFdy(worldPosition);
  vec2 maskUv=(localPosition.xz-domain.xy)/domain.zw,maskDx=dFdx(maskUv),maskDy=dFdy(maskUv);
  vec3 axes=vec3(0,1,0);
  if((flags&64u)!=0u){axes=pow(abs(geometricNormal),vec3(4));axes/=max(dot(axes,vec3(1)),1e-8);}
  vec4 diffuse=terrainSnowSample(row,0u,p,px,py,axes,uvScale,1.0,flags,core.y,core.z);
  vec4 rawHeight=terrainSnowSample(row,1u,p,px,py,axes,uvScale,1.0,flags,core.y,core.z);
  vec4 rawSmooth=terrainSnowSample(row,3u,p,px,py,axes,uvScale,1.0,flags,core.y,core.z);
  vec4 rawAo=terrainSnowSample(row,4u,p,px,py,axes,uvScale,1.0,flags,core.y,core.z);
  if((flags&2u)!=0u){vec4 resample=terrainSurfaceRows[row+8u];float blend=terrainPbrRamp(resample.z,resample.w,length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION))*resample.y;
    diffuse=mix(diffuse,terrainSnowSample(row,0u,p,px,py,axes,uvScale,resample.x,flags,core.y,core.z),blend);
    rawHeight=mix(rawHeight,terrainSnowSample(row,1u,p,px,py,axes,uvScale,resample.x,flags,core.y,core.z),blend);
    rawSmooth=mix(rawSmooth,terrainSnowSample(row,3u,p,px,py,axes,uvScale,resample.x,flags,core.y,core.z),blend);
    rawAo=mix(rawAo,terrainSnowSample(row,4u,p,px,py,axes,uvScale,resample.x,flags,core.y,core.z),blend);
  }
  uint maps=row+terrainSnowHeaderRows;
  vec4 snowColor=terrainPbrColor(maps,diffuse)*terrainSurfaceRows[row+2u];
  float snowHeight=clamp(terrainPbrScalar(maps+5u,rawHeight),0.0,1.0),snowSmooth=clamp(terrainPbrScalar(maps+15u,rawSmooth),0.0,1.0),snowAo=clamp(terrainPbrScalar(maps+20u,rawAo),0.0,1.0);
  float level=core.x;if(rich&&(flags&4u)!=0u)level*=participation;
  float d=max(level*.5,dot(material.normalWorld,up));
  float heightFade=clamp((worldPosition.y-heightAngle.x)/max(heightAngle.y-heightAngle.x,.001),0.0,1.0);
  float angle=clamp(max(0.0,(d-heightAngle.z)*6.0)*(1.0-max(0.0,(d-heightAngle.w)*6.0)),0.0,1.0);
  float fade=clamp(level*heightFade*angle-flowSnowClearing,0.0,1.0);
  if((flags&8u)!=0u){vec4 mask=terrainPbrColor(maps+30u,terrainPbrFetchGrad(maps+30u,clamp(maskUv,vec2(0),vec2(1)),maskDx,maskDy,1.0));fade=max(min(fade,mask.r),mask.g);}
  float h=clamp((rich?material.height:0.0)-(1.0-parameters.x),0.0,1.0),e=clamp((rich?material.occlusion:1.0)*parameters.y,0.0,1.0);e*=e;
  float m=clamp(fade-h-e,0.0,1.0),coverage=clamp(m*m*m*clamp(d-(h+e)*.5,0.0,1.0)*8.0,0.0,1.0),normalWeight=coverage*coverage;
  float wet=clamp(parameters.w*4.0*fade-snowSmooth*.5,0.0,1.0);
  float wetDarkening=mix(1.0,.4,clamp((material.roughness-.5)/max(porosity,.001),0.0,1.0));
  material.color.rgb*=mix(1.0,wetDarkening,wet);
  material.normalWorld=terrainPbrSafeNormal(mix(material.normalWorld,geometricNormal,wet),geometricNormal);
  material.roughness=mix(material.roughness,.2,wet);
  vec3 normalWorld=vec3(0);
  for(uint axis=0u;axis<3u;++axis){if((flags&64u)==0u&&axis!=1u)continue;
    vec3 oneAxis=vec3(0);oneAxis[axis]=1.0;
    vec4 axisNormal=terrainSnowSample(row,2u,p,px,py,oneAxis,uvScale,1.0,flags,core.y,core.z);
    vec3 decoded=terrainPbrDecodedNormal(maps+10u,axisNormal,1.0);
    if((flags&2u)!=0u){vec4 resample=terrainSurfaceRows[row+8u];float blend=terrainPbrRamp(resample.z,resample.w,length(worldPosition-TERRAIN_SURFACE_CAMERA_POSITION))*resample.y;
      vec3 farNormal=terrainPbrDecodedNormal(maps+10u,terrainSnowSample(row,2u,p,px,py,oneAxis,uvScale,resample.x,flags,core.y,core.z),1.0);
      decoded=terrainPbrSafeNormal(mix(decoded,farNormal,blend),vec3(0,0,1));}
    if((flags&1u)!=0u){vec4 n=terrainSurfaceRows[row+7u];vec4 noise=terrainSnowSample(row,5u,p,px,py,oneAxis,uvScale,n.x,flags,core.y,core.z);decoded=terrainPbrRnm(decoded,terrainPbrDecodedNormal(maps+25u,noise,n.y));}
    vec2 dx=(axis==0u?px.zy:axis==1u?px.xz:px.xy)*uvScale.xy,dy=(axis==0u?py.zy:axis==1u?py.xz:py.xy)*uvScale.xy;
    normalWorld+=terrainPbrNormalGrad(decoded,dx,dy,wx,wy,geometricNormal)*axes[axis];}
  material.normalWorld=terrainPbrSafeNormal(mix(material.normalWorld,terrainPbrSafeNormal(normalWorld,geometricNormal),normalWeight),geometricNormal);
  material.color.rgb=mix(material.color.rgb,snowColor.rgb,coverage);material.occlusion=mix(material.occlusion,snowAo,coverage);material.metallic=mix(material.metallic,.01,coverage);material.height=mix(material.height,snowHeight,coverage);
  float smoothness=mix(1.0-material.roughness,snowSmooth*parameters.z,coverage);
  smoothness=mix(smoothness,clamp(.65-snowSmooth,0.0,1.0)*parameters.z,coverage);material.roughness=clamp(1.0-smoothness,.07,1.0);
  vec3 view=terrainPbrSafeNormal(TERRAIN_SURFACE_CAMERA_POSITION-worldPosition,geometricNormal);
  if((flags&512u)!=0u){vec4 sss=terrainSurfaceRows[row+9u];material.transmission=max(sss.rgb,vec3(0))*exp(-max(sss.a*2.0*snowHeight,0.0))*normalWeight;}
  if((flags&1024u)!=0u)material.emission+=max(terrainSurfaceRows[row+10u].rgb,vec3(0))*pow(max(1.0-clamp(dot(view,material.normalWorld),0.0,1.0),1e-8),core.w)*coverage;
  if((flags&2048u)!=0u){vec4 sparkle=terrainSurfaceRows[row+12u],noiseParams=terrainSurfaceRows[row+13u];
    vec3 fromEye=worldPosition-TERRAIN_SURFACE_CAMERA_POSITION;float distanceToEye=length(fromEye);
    float band=floor(log2(.3*distanceToEye+3.0)/.3785116),bandDepth=max(.1*pow(1.3,band)-.2,1e-6),density=sparkle.w*.12/bandDepth;
    vec3 direction=distanceToEye>1e-6?fromEye/distanceToEye:-geometricNormal;
    vec3 viewCell=direction*bandDepth;viewCell=sign(viewCell)*fract(abs(viewCell));
    vec3 position=worldPosition*density+noiseParams.z*terrainPbrSafeNormal(viewCell,vec3(0));
    vec3 cell=floor(position),center=cell/max(density,1e-6);
    uint noiseMap=maps+35u;vec2 noiseUv=noiseParams.x*.12/bandDepth*center.xz+center.y;
    vec4 rawNoise=textureLod(TERRAIN_SURFACE_MAP(uint(terrainSurfaceRows[noiseMap].x+.5)),noiseUv,0.0);
    vec3 noise=terrainPbrColor(noiseMap,rawNoise).rgb*noiseParams.y;
    vec3 delta=position-cell+.5*fract(noise)-.75;
    float facing=dot(direction,geometricNormal);vec3 tangent=direction-facing*geometricNormal;
    float tangentSquared=dot(tangent,tangent);
    if(tangentSquared>1e-8)delta+=(abs(facing)-1.0)*dot(delta,tangent)*tangent/tangentSquared;
    float threshold=sparkle.z*.001,distSquared=dot(delta,delta);
    float star=density>0.0&&threshold>0.0?max(1.0-distSquared/max(threshold,1e-8),0.0):0.0;
    star*=sparkle.y*coverage;
    vec3 color=max(terrainSurfaceRows[row+11u].rgb,vec3(0))*star;
    material.color.rgb+=color;material.emission+=color*sparkle.x;material.roughness=clamp(material.roughness-star,.07,1.0);
  }
}
