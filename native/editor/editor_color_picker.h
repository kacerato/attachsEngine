#pragma once
#include "ui/ui_theme.h"
#include <algorithm>
#include <cmath>
namespace ae::editor {
inline float colorToSrgb(float v) {v=std::clamp(v,0.f,1.f);return v<=.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-.055f;}
inline float colorToLinear(float v) {return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
inline void pickerRgb(float h,float s,float v,float (&rgb)[3]) {
  const float x=h*6;const int sector=static_cast<int>(x)%6;const float f=x-std::floor(x);
  const float p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
  const float table[6][3]={{v,t,p},{q,v,p},{p,v,t},{p,q,v},{t,p,v},{v,p,q}};
  std::copy(table[sector],table[sector]+3,rgb);
}
inline ui::UiColor pickerColor(float h,float s,float v) {
  float rgb[3];pickerRgb(h,s,v,rgb);ui::UiColor color=0xff000000u;
  for(u32 i=0;i<3;++i) color|=static_cast<u32>(rgb[i]*255+.5f)<<(16-8*i);
  return color;
}
inline void pickerHsv(const float (&linear)[3],float &h,float &s,float &v) {
  float c[3];for(u32 i=0;i<3;++i)c[i]=colorToSrgb(linear[i]);
  v=std::max({c[0],c[1],c[2]});const float d=v-std::min({c[0],c[1],c[2]});s=v>0?d/v:0;h=0;
  if(d>0) {h=(v==c[0]?(c[1]-c[2])/d:v==c[1]?2+(c[2]-c[0])/d:4+(c[0]-c[1])/d)/6; if(h<0)h+=1;}
}
}
