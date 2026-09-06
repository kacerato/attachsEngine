#ifdef AETHER_SPECTRAL_FRAGMENT
layout(set=1,binding=11) uniform sampler2D slope0;
layout(set=1,binding=12) uniform sampler2D slope1;
layout(set=1,binding=13) uniform sampler2D slope2;
layout(set=1,binding=14) uniform sampler2D slope3;
vec4 sampleWaterSurface(int cascade,vec2 position,uint n,vec2 dx,vec2 dy) {
  // FFT samples lie on grid nodes, textures on texel centres.
  vec2 uv=position+vec2(.5/float(n));
  if(cascade==0) return textureGrad(slope0,uv,dx,dy);
  if(cascade==1) return textureGrad(slope1,uv,dx,dy);
  if(cascade==2) return textureGrad(slope2,uv,dx,dy);
  return textureGrad(slope3,uv,dx,dy);
}
#endif

#ifndef AETHER_SPECTRAL_FRAGMENT
layout(std430,set=1,binding=7) readonly buffer Cascade0 { vec4 cascade0[]; };
layout(std430,set=1,binding=8) readonly buffer Cascade1 { vec4 cascade1[]; };
layout(std430,set=1,binding=9) readonly buffer Cascade2 { vec4 cascade2[]; };
layout(std430,set=1,binding=10) readonly buffer Cascade3 { vec4 cascade3[]; };
vec4 readWaterCascade(int cascade,uint address) {
  // Static bindings avoid requiring storage-buffer descriptor-array indexing.
  if(cascade==0) return cascade0[address];
  if(cascade==1) return cascade1[address];
  if(cascade==2) return cascade2[address];
  return cascade3[address];
}
vec4 sampleWaterCascade(int cascade,vec2 position,uint channel,uint n) {
  vec2 grid=fract(position)*float(n),fraction=fract(grid);
  uvec2 a=uvec2(floor(grid)),b=(a+1u)%n;
  vec4 aa=readWaterCascade(cascade,2u*(a.y*n+a.x)+channel);
  vec4 ba=readWaterCascade(cascade,2u*(a.y*n+b.x)+channel);
  vec4 ab=readWaterCascade(cascade,2u*(b.y*n+a.x)+channel);
  vec4 bb=readWaterCascade(cascade,2u*(b.y*n+b.x)+channel);
  return mix(mix(aa,ba,fraction.x),mix(ab,bb,fraction.x),fraction.y);
}
float readWaterFoam(int cascade,uint sampleIndex,uint n) {
  vec4 packed=readWaterCascade(cascade,2u*n*n+sampleIndex/4u);
  return packed[sampleIndex%4u];
}
float sampleWaterFoam(int cascade,vec2 position,uint n) {
  vec2 grid=fract(position)*float(n),fraction=fract(grid);
  uvec2 a=uvec2(floor(grid)),b=(a+1u)%n;
  return mix(mix(readWaterFoam(cascade,a.y*n+a.x,n),readWaterFoam(cascade,a.y*n+b.x,n),fraction.x),
             mix(readWaterFoam(cascade,b.y*n+a.x,n),readWaterFoam(cascade,b.y*n+b.x,n),fraction.x),fraction.y);
}
void addWaterSpectrum(vec2 position,float cellSize,inout float height,inout vec2 slope,
                     out vec2 horizontal,out vec3 horizontalDerivative,out float foam) {
  horizontal=vec2(0); horizontalDerivative=vec3(0);
  foam=0;
  for(int cascade=0;cascade<4;++cascade) {
    if(cascade>=int(environment.waterParameters.x)) break;
    // Provider-specific payload; analytical variant retains its original layout.
    vec4 parameters=environment.waterWaveShape[cascade]; // N, patch metres, scale, choppiness
    float shortest=environment.waterWaveMotion[cascade].x;
    float scale=parameters.z*smoothstep(2.0,5.0,shortest/max(cellSize,0.001));
    float c=environment.waterWaveMotion[cascade].y,s=environment.waterWaveMotion[cascade].z;
    mat2 rotation=mat2(c,s,-s,c);
    vec2 uv=transpose(rotation)*position/parameters.y;
    vec4 first=sampleWaterCascade(cascade,uv,0u,uint(parameters.x));
    vec4 second=sampleWaterCascade(cascade,uv,1u,uint(parameters.x));
    height+=first.x*scale;
    slope+=rotation*vec2(first.w,second.x)*scale;
    horizontal+=rotation*first.yz*(scale*parameters.w);
    vec3 d=second.yzw;
    horizontalDerivative+=vec3(c*c*d.x-2*c*s*d.y+s*s*d.z,
      c*s*(d.x-d.z)+(c*c-s*s)*d.y,s*s*d.x+2*c*s*d.y+c*c*d.z)*(scale*parameters.w);
    foam=max(foam,sampleWaterFoam(cascade,uv,uint(parameters.x))*
      smoothstep(2.0,5.0,shortest/max(cellSize,0.001)));
  }
}
#endif
