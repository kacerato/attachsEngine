#pragma once
#include "core/base.h"
#include <vector>
namespace ae::test {
inline std::vector<u8> tiffFixture(bool little,u16 bits,u16 orientation=1) {
  // Owned 2x1 RGB + undefined extra, one strip. No external asset or encoder.
  constexpr u32 fields=12,ifd=8,bitsAt=ifd+2+fields*12+4,pixelsAt=bitsAt+8;
  std::vector<u8> out(pixelsAt+8*(bits/8));out[0]=out[1]=little?'I':'M';
  const auto half=[&](usize at,u16 value){for(u32 n=0;n<2;++n)out[at+n]=static_cast<u8>(value>>(little?n*8:(1-n)*8));};
  const auto word=[&](usize at,u32 value){for(u32 n=0;n<4;++n)out[at+n]=static_cast<u8>(value>>(little?n*8:(3-n)*8));};
  half(2,42);word(4,ifd);half(ifd,fields);u32 index=0;
  const auto field=[&](u16 tag,u16 type,u32 count,u32 value){const auto at=ifd+2+index++*12;half(at,tag);half(at+2,type);word(at+4,count);if(type==3&&count==1)half(at+8,static_cast<u16>(value));else word(at+8,value);};
  field(256,4,1,2);field(257,4,1,1);field(258,3,4,bitsAt);field(259,3,1,1);field(262,3,1,2);
  field(273,4,1,pixelsAt);field(274,3,1,orientation);field(277,3,1,4);field(278,4,1,1);
  field(279,4,1,8*(bits/8));field(284,3,1,1);field(338,3,1,0);
  for(u32 c=0;c<4;++c)half(bitsAt+c*2,bits);
  const u16 values[]{65535,0,32768,16448,0,65535,0,65535};
  for(u32 c=0;c<8;++c)if(bits==16)half(pixelsAt+c*2,values[c]);else out[pixelsAt+c]=static_cast<u8>((values[c]+128)/257);
  return out;
}
}
