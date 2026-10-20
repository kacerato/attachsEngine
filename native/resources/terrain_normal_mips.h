#pragma once
#include "resources/image_decode.h"
#include <algorithm>
#include <cmath>

namespace ae::resources {
// GA is a normal-only view. R/B scalar views must be built independently from
// the original packed image; filtering XYZ must never rewrite their source.
inline bool buildTerrainGaNormalMipChain(const DecodedImage &source,
    std::vector<u8> &out,u32 &levels) {
  if(!source.width || !source.height ||
     u64(source.width)*source.height*4!=source.rgba.size())return false;
  DecodedImage canonical=source;
  const auto encode=[](float value){return u8(std::lround(std::clamp(value*.5f+.5f,0.f,1.f)*255.f));};
  for(usize i=0;i<canonical.rgba.size();i+=4) {
    float x=source.rgba[i+3]/127.5f-1.f,y=source.rgba[i+1]/127.5f-1.f;
    float z=std::sqrt(std::max(0.f,1.f-x*x-y*y));
    const float length=std::sqrt(x*x+y*y+z*z);
    if(length>0){x/=length;y/=length;z/=length;}else{x=0;y=0;z=1;}
    canonical.rgba[i]=encode(x);canonical.rgba[i+1]=encode(y);
    canonical.rgba[i+2]=encode(z);canonical.rgba[i+3]=255;
  }
  std::vector<u8> chain;u32 count=0;
  if(!buildNormalMipChain(canonical,chain,count))return false;
  for(usize i=0;i<chain.size();i+=4) {
    const u8 x=chain[i],y=chain[i+1];
    chain[i]=0;chain[i+1]=y;chain[i+2]=0;chain[i+3]=x;
  }
  // Preserve exact authored GA at level zero, including out-of-disk XY. The
  // shader reconstructs/normalizes; lower levels contain normalized vectors.
  for(usize i=0;i<source.rgba.size();i+=4){chain[i+1]=source.rgba[i+1];chain[i+3]=source.rgba[i+3];}
  out=std::move(chain);levels=count;return true;
}
}
