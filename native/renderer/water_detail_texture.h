#pragma once
#include "core/base.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::renderer {
// Linear signed slope channels, complete mip chain. A self-contained fallback
// for water without an imported detail texture; never sample the albedo fallback.
inline bool buildWaterDetailTexture(u32 size,u32 seed,std::vector<u8> &output) {
  if(size<8 || size>256 || (size&(size-1))) return false;
  std::vector<u8> result(static_cast<usize>(size)*size*4);
  constexpr int frequencies[4][2]{{3,2},{-7,5},{11,-13},{-19,-17}};
  float phase[4];
  for(auto &value:phase) {seed=seed*1664525u+1013904223u;value=float(seed>>8)*(6.28318530718f/16777216.0f);}
  for(u32 y=0;y<size;++y) for(u32 x=0;x<size;++x) {
    float slope[2]{};
    for(u32 wave=0;wave<4;++wave) {
      const float kx=float(frequencies[wave][0]),kz=float(frequencies[wave][1]);
      const float s=.19f*std::cos(6.28318530718f*(kx*x+kz*y)/size+phase[wave])/std::hypot(kx,kz);
      slope[0]+=kx*s;slope[1]+=kz*s;
    }
    const usize index=(static_cast<usize>(y)*size+x)*4;
    for(u32 axis=0;axis<2;++axis) result[index+axis]=static_cast<u8>(std::clamp(std::lround((.5f+.5f*slope[axis])*255),0l,255l));
    result[index+2]=255;result[index+3]=255;
  }
  usize previous=0;
  for(u32 width=size;width>1;width/=2) {
    const u32 next=width/2;const usize start=result.size();result.resize(start+static_cast<usize>(next)*next*4);
    for(u32 y=0;y<next;++y) for(u32 x=0;x<next;++x) for(u32 channel=0;channel<4;++channel) {
      u32 sum=0;
      for(u32 dy=0;dy<2;++dy) for(u32 dx=0;dx<2;++dx)
        sum+=result[previous+((y*2+dy)*width+x*2+dx)*4+channel];
      result[start+(y*next+x)*4+channel]=static_cast<u8>((sum+2)/4);
    }
    previous=start;
  }
  output=std::move(result);return true;
}
}
