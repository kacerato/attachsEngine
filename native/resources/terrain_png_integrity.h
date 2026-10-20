#pragma once
#include "core/base.h"
#include <algorithm>
#include <iterator>
#include <span>
namespace ae::resources {
// Validate the original container before retaining/exporting it. stb_image
// validates decoded pixels but deliberately skips PNG chunk CRCs.
inline bool validTerrainPng(std::span<const u8> bytes,u64 maximumBytes=64ull*1048576) {
  constexpr u8 signature[]{137,80,78,71,13,10,26,10};
  if(bytes.size()>maximumBytes || bytes.size()<8 || !std::equal(std::begin(signature),std::end(signature),bytes.begin()))return false;
  const auto be=[&](usize at){return u32(bytes[at])<<24|u32(bytes[at+1])<<16|u32(bytes[at+2])<<8|u32(bytes[at+3]);};
  bool header=false,data=false,dataEnded=false,palette=false;
  u8 color=0,depth=0;u64 dataBytes=0;
  usize at=8;
  while(at<bytes.size()) {
    if(bytes.size()-at<12)return false;
    const u32 length=be(at),type=be(at+4);
    if(length>0x7fffffffu || length>bytes.size()-at-12)return false;
    for(usize n=at+4;n<at+8;++n)
      if(!((bytes[n]>='A'&&bytes[n]<='Z')||(bytes[n]>='a'&&bytes[n]<='z')))return false;
    if(bytes[at+6]&32)return false; // PNG reserved chunk-type bit must be zero.
    u32 crc=0xffffffffu;
    for(usize n=at+4;n<at+8+length;++n) {
      crc^=bytes[n];for(u32 bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&u32(-i32(crc&1)));
    }
    if((crc^0xffffffffu)!=be(at+8+length))return false;
    if(!header) {
      if(type!=0x49484452u || length!=13 || be(at+8)==0 || be(at+12)==0 || be(at+8)>0x7fffffffu || be(at+12)>0x7fffffffu)return false;
      depth=bytes[at+16];color=bytes[at+17];
      const bool validDepth=color==0?(depth==1||depth==2||depth==4||depth==8||depth==16):
        color==3?(depth==1||depth==2||depth==4||depth==8):
        (color==2||color==4||color==6)?(depth==8||depth==16):false;
      if(!validDepth || bytes[at+18]!=0 || bytes[at+19]!=0 || bytes[at+20]>1)return false;
      header=true;
    } else if(type==0x49484452u)return false;
    if(type==0x504c5445u) {
      if(palette || data || color==0 || color==4 || length==0 || length%3 || length>768 ||
         (color==3 && length/3>(1u<<depth)))return false;
      palette=true;
    }
    if(type==0x49444154u) {if(dataEnded || (color==3&&!palette))return false;data=true;dataBytes+=length;}
    else if(data)dataEnded=true;
    if(type==0x49454e44u)return length==0 && data && dataBytes>=6 && at+12==bytes.size();
    if((bytes[at+4]&32)==0 && type!=0x49484452u && type!=0x504c5445u && type!=0x49444154u)return false;
    at+=usize(length)+12;
  }
  return false;
}
}
