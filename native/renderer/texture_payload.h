#pragma once
#include "rhi/memory_allocator.h"
#include <span>
#include <cstring>
#include <algorithm>
#include <limits>

namespace ae::renderer {
struct TexturePayload {
  rhi::ImageDesc description{};
  u64 payloadBytes=0;
};
struct TextureResidentRange {
  rhi::ImageDesc description{};
  u32 baseMip=0;
  u64 byteOffset=0; // relativo ao início do payload, depois do cabeçalho AETX
  u64 byteSize=0;
  bool valid() const noexcept {return byteSize!=0 && description.mipLevels!=0;}
};
// AETX v1 header is explicit little-endian, never a dump of a compiler struct.
inline bool decodeTextureHeader(std::span<const u8> bytes, u64 fileSize, TexturePayload &out) {
  if (bytes.size()<32) return false;
  auto word=[&](usize offset) { return static_cast<u32>(bytes[offset]) |
    static_cast<u32>(bytes[offset+1])<<8 | static_cast<u32>(bytes[offset+2])<<16 |
    static_cast<u32>(bytes[offset+3])<<24; };
  if (word(0)!=0x58544541 || word(4)!=1) return false;
  TexturePayload data;
  auto &d=data.description;
  d.width=word(8); d.height=word(12); d.mipLevels=word(20);
  if (d.width>16384 || d.height>16384) return false;
  switch(word(16)) {
  case 1:d.format=VK_FORMAT_ASTC_6x6_SRGB_BLOCK;break;
  case 2:d.format=VK_FORMAT_ASTC_6x6_UNORM_BLOCK;break;
  case 3:d.format=VK_FORMAT_R8G8B8A8_SRGB;break;
  case 4:d.format=VK_FORMAT_R8G8B8A8_UNORM;break;
  case 5:d.format=VK_FORMAT_R16G16B16A16_SFLOAT;break;
  default:return false;
  }
  d.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  d.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  data.payloadBytes=static_cast<u64>(word(24))|(static_cast<u64>(word(28))<<32);
  if (data.payloadBytes==0 || data.payloadBytes!=rhi::sampledChainByteSize(d) ||
      data.payloadBytes>512ull*1024*1024 || fileSize!=32+data.payloadBytes) return false;
  out=data; return true;
}
inline u32 chooseResidentMip(const rhi::ImageDesc &description,u32 maxDimension,u64 budget,
                             u32 minimumMip=0) {
  auto d=description;
  for(u32 mip=0;mip<description.mipLevels;++mip) {
    if(mip>=minimumMip && d.width<=maxDimension && d.height<=maxDimension &&
       rhi::sampledChainByteSize(d)<=budget) return mip;
    d.width=std::max(1u,d.width/2);d.height=std::max(1u,d.height/2);--d.mipLevels;
  }
  return description.mipLevels; // no supported resident chain
}
// Seleciona uma cauda contígua da cadeia sem alterar o recurso fonte. O bias é
// um limite de residência: 1 começa no segundo mip (metade em cada dimensão).
inline TextureResidentRange chooseResidentRange(const rhi::ImageDesc &source,u32 maxDimension,
                                                 u64 budget,u32 minimumMip=0) {
  TextureResidentRange range;
  const u32 base=chooseResidentMip(source,maxDimension,budget,minimumMip);
  if(base==source.mipLevels) return range;
  range.description=source;range.baseMip=base;
  for(u32 mip=0;mip<base;++mip) {
    const u64 bytes=rhi::sampledMipByteSize(range.description.format,range.description.width,
                                            range.description.height);
    if(bytes==0 || bytes>std::numeric_limits<u64>::max()-range.byteOffset) return {};
    range.byteOffset+=bytes;
    range.description.width=std::max(1u,range.description.width/2);
    range.description.height=std::max(1u,range.description.height/2);
    --range.description.mipLevels;
  }
  range.byteSize=rhi::sampledChainByteSize(range.description);
  return range;
}
} // namespace ae::renderer
