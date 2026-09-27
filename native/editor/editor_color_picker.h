#pragma once
// Matemática da janela de cor (Unity 6000.0 Manual/InspectorColorPicker).
//
// A cor autoral é RGB LINEAR (é o que o renderer consome). A janela mostra e
// edita em sRGB, como a Unity: o quadrado SV, a matiz, as barras RGB 0–255 e
// 0–1 e o hexadecimal trabalham sobre a cor perceptual; só a gravação volta ao
// linear. Cor HDR = cor base (maior canal ≤ 1) × 2^intensidade, a "Intensity"
// da janela HDR da Unity.
#include "ui/ui_theme.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
namespace ae::editor {
inline float colorToSrgb(float v) {v=std::clamp(v,0.f,1.f);return v<=.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-.055f;}
inline float colorToLinear(float v) {return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
inline void pickerRgb(float h,float s,float v,float (&rgb)[3]) {
  h=h-std::floor(h);
  const float x=h*6;const int sector=static_cast<int>(x)%6;const float f=x-std::floor(x);
  const float p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
  const float table[6][3]={{v,t,p},{q,v,p},{p,v,t},{p,q,v},{t,p,v},{v,p,q}};
  std::copy(table[sector],table[sector]+3,rgb);
}
inline ui::UiColor pickerColor(float h,float s,float v,float alpha=1) {
  float rgb[3];pickerRgb(h,s,v,rgb);
  ui::UiColor color=static_cast<u32>(std::clamp(alpha,0.f,1.f)*255+.5f)<<24;
  for(u32 i=0;i<3;++i) color|=static_cast<u32>(rgb[i]*255+.5f)<<(16-8*i);
  return color;
}
// sRGB 0–1 → HSV, preservando a matiz quando a saturação some (senão arrastar
// até o cinza apagaria a matiz escolhida).
inline void srgbToHsv(const float (&c)[3],float &h,float &s,float &v) {
  v=std::max({c[0],c[1],c[2]});const float d=v-std::min({c[0],c[1],c[2]});s=v>0?d/v:0;
  if(d>0) {h=(v==c[0]?(c[1]-c[2])/d:v==c[1]?2+(c[2]-c[0])/d:4+(c[0]-c[1])/d)/6; if(h<0)h+=1;}
}
inline void pickerHsv(const float (&linear)[3],float &h,float &s,float &v) {
  float c[3];for(u32 i=0;i<3;++i)c[i]=colorToSrgb(linear[i]);
  h=0;srgbToHsv(c,h,s,v);
}
// Linear HDR → base LDR + intensidade (stops). Cor LDR tem intensidade 0.
inline void splitHdr(const float (&linear)[3],float (&base)[3],float &intensity) {
  const float peak=std::max({linear[0],linear[1],linear[2]});
  intensity=peak>1?std::log2(peak):0;
  const float scale=peak>1?1/peak:1;
  for(u32 i=0;i<3;++i) base[i]=linear[i]*scale;
}
// "#RRGGBB" ou "#RRGGBBAA" (a Unity aceita com e sem #), em sRGB.
inline bool parseColorHex(std::string_view text,float (&srgb)[3],float &alpha,bool withAlpha) {
  while(!text.empty() && text.front()==' ') text.remove_prefix(1);
  while(!text.empty() && text.back()==' ') text.remove_suffix(1);
  if(!text.empty() && text.front()=='#') text.remove_prefix(1);
  if(text.size()!=6 && !(withAlpha && text.size()==8)) return false;
  u32 channels[4]{0,0,0,255};
  for(u32 i=0;i<text.size()/2;++i) {
    u32 value=0;
    for(u32 k=0;k<2;++k) {
      const char c=text[i*2+k];
      const int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
      if(digit<0) return false;
      value=value*16+static_cast<u32>(digit);
    }
    channels[i]=value;
  }
  for(u32 i=0;i<3;++i) srgb[i]=channels[i]/255.f;
  if(text.size()==8) alpha=channels[3]/255.f;
  return true;
}
inline std::string formatColorHex(const float (&srgb)[3],float alpha,bool withAlpha) {
  char text[12];
  const auto byte=[](float v){return static_cast<unsigned>(std::clamp(v,0.f,1.f)*255+.5f);};
  if(withAlpha) std::snprintf(text,sizeof(text),"%02X%02X%02X%02X",byte(srgb[0]),byte(srgb[1]),byte(srgb[2]),byte(alpha));
  else std::snprintf(text,sizeof(text),"%02X%02X%02X",byte(srgb[0]),byte(srgb[1]),byte(srgb[2]));
  return text;
}
}
