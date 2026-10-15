// Terrain surface v1. The four bindings are sRGB albedos; COLOR_0 is a
// normalized splat vector, never vertex tint/opacity. UVs carry local XYZ so
// moving the terrain does not make the material swim through world space.
float terrainHash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float terrainNoise(vec2 p) {
  vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
  return mix(mix(terrainHash(i),terrainHash(i+vec2(1,0)),f.x),
             mix(terrainHash(i+vec2(0,1)),terrainHash(i+vec2(1,1)),f.x),f.y);
}
vec3 terrainTile(sampler2D tex,vec2 uv,vec2 dx,vec2 dy,float variation) {
  vec3 a=textureGrad(tex,uv,dx,dy).rgb;
  // Two independent scales/rotations; smooth macro mask avoids cell seams.
  vec2 turned=mat2(.8,-.6,.6,.8)*uv*.37+vec2(.27,.61);
  vec3 b=textureGrad(tex,turned,mat2(.8,-.6,.6,.8)*dx*.37,mat2(.8,-.6,.6,.8)*dy*.37).rgb;
  float mask=terrainNoise(uv*.11);
  return mix(a,b,variation*(.25+.5*mask))*(1.0+variation*(mask-.5)*.6);
}
vec3 terrainLayer(sampler2D tex,vec3 p,vec3 dx,vec3 dy,vec3 axes,float variation) {
  vec3 c=vec3(0);
  if(axes.x>.005)c+=terrainTile(tex,p.zy,dx.zy,dy.zy,variation)*axes.x;
  if(axes.y>.005)c+=terrainTile(tex,p.xz,dx.xz,dy.xz,variation)*axes.y;
  if(axes.z>.005)c+=terrainTile(tex,p.xy,dx.xy,dy.xy,variation)*axes.z;
  return c;
}
vec4 terrainSurface() {
  vec4 weights=max(vColor,vec4(0));weights/=max(dot(weights,vec4(1)),.001);
  // Derivatives of the stored local position supply projection weights in the
  // correct space even when the terrain root is rotated or scaled.
  vec3 p=vec3(vUv0.x,vUv1.x,vUv0.y);
  // Derivatives precede per-layer branches: explicit gradients keep mip
  // selection defined at splat/projection boundaries on mobile GPUs.
  vec3 dx=dFdx(p),dy=dFdy(p);
  vec3 localNormal=cross(dx,dy);
  localNormal=dot(localNormal,localNormal)>1e-16?normalize(localNormal):vec3(0,1,0);
  vec3 axes=pow(abs(localNormal),vec3(4));axes/=max(dot(axes,vec3(1)),.001);
  float variation=clamp(vUv1.y,0.0,1.0);
  vec3 color=vec3(0);
  if(weights.x>.001)color+=weights.x*terrainLayer(BASE_MAP,p,dx,dy,axes,variation);
  if(weights.y>.001)color+=weights.y*terrainLayer(NORMAL_MAP,p,dx,dy,axes,variation);
  if(weights.z>.001)color+=weights.z*terrainLayer(MR_MAP,p,dx,dy,axes,variation);
  if(weights.w>.001)color+=weights.w*terrainLayer(EMISSIVE_MAP,p,dx,dy,axes,variation);
  return vec4(color,1);
}
