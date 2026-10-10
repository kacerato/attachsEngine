#pragma once
#include "core/base.h"
#include <span>
#include <string>

namespace ae::resources {
// stb_image v2.30 supplies pixel decoding. This preflight closes its permissive
// truncated-TGA path and records orientation/alpha which stb does not honor.
// Supported source family: truecolor24/32 and grayscale8/16, raw or RLE.
// Indexed/15/16bit truecolor and interleaved images remain explicit refusals.
struct TgaImageInfo {
  u32 width=0,height=0,pixelBytes=0;
  usize pixelsOffset=0;
  bool rle=false,rightOrigin=false,hasAlpha=false;
};
inline bool readTgaInfo(std::span<const u8> bytes,TgaImageInfo &out) {
  out={};if(bytes.size()<18||bytes[1]!=0)return false;
  const auto type=bytes[2];const bool grey=type==3||type==11;
  if(type!=2&&type!=3&&type!=10&&type!=11)return false;
  // A non-indexed image must not smuggle a color table into the byte stream.
  for(usize i=3;i<8;++i)if(bytes[i])return false;
  const u32 width=bytes[12]|(static_cast<u32>(bytes[13])<<8);
  const u32 height=bytes[14]|(static_cast<u32>(bytes[15])<<8);
  const u32 depth=bytes[16],attributes=bytes[17]&15;
  if(!width||!height||(bytes[17]&0xc0))return false;
  if(grey?(depth!=8&&depth!=16):(depth!=24&&depth!=32))return false;
  if(attributes!=0&&attributes!=8)return false;
  if(attributes==8&&((grey&&depth!=16)||(!grey&&depth!=32)))return false;
  const usize offset=18u+bytes[0];if(offset>bytes.size())return false;
  out={width,height,depth/8,offset,type>=10,(bytes[17]&0x10)!=0,attributes==8};return true;
}
inline bool validateTgaPixels(std::span<const u8> bytes,const TgaImageInfo &info,std::string &diagnostic) {
  diagnostic.clear();
  const auto fail=[&](const char *message){diagnostic=message;return false;};
  if(!info.width||!info.height||!info.pixelBytes||info.pixelsOffset>bytes.size())return fail("Cabeçalho TGA inválido.");
  const u64 count=static_cast<u64>(info.width)*info.height;usize cursor=info.pixelsOffset;
  if(!info.rle) {
    if(count>(bytes.size()-cursor)/info.pixelBytes)return fail("TGA truncado antes do último pixel.");
    cursor+=static_cast<usize>(count)*info.pixelBytes;
  } else {
    u64 written=0;
    while(written<count) {
      if(cursor>=bytes.size())return fail("Pacote RLE do TGA ausente.");
      const u8 header=bytes[cursor++];const u64 run=static_cast<u64>(header&127)+1;
      if(run>count-written)return fail("Pacote RLE excede as dimensões do TGA.");
      const u64 required=(header&128)?info.pixelBytes:run*info.pixelBytes;
      if(required>bytes.size()-cursor)return fail("Pacote RLE do TGA truncado.");
      cursor+=static_cast<usize>(required);written+=run;
    }
  }
  // TGA 2.0 may declare premultiplied alpha or color correction in its extension.
  // Do not silently ignore those semantic transforms.
  constexpr char signature[]="TRUEVISION-XFILE.";
  if(bytes.size()>=26) {
    const usize footer=bytes.size()-26;bool matched=true;
    for(usize i=0;i<17;++i)matched&=bytes[footer+8+i]==static_cast<u8>(signature[i]);
    matched&=bytes.back()==0;
    if(matched) {
      if(cursor>footer)return fail("Pixels TGA invadem o rodapé do arquivo.");
      u32 extension=0;for(u32 i=0;i<4;++i)extension|=static_cast<u32>(bytes[footer+i])<<(i*8);
      if(extension) {
        if(extension<cursor||extension>footer||footer-extension<495)return fail("Extensão TGA inválida ou truncada.");
        if(bytes[extension]!=0xef||bytes[extension+1]!=1)return fail("Tamanho de extensão TGA não suportado.");
        const u8 alpha=bytes[extension+494];
        if(alpha>3)return fail("TGA com alfa premultiplicado ou semântica desconhecida requer conversão explícita.");
        if((alpha>=2)!=info.hasAlpha)return fail("Declarações de alfa divergentes no cabeçalho e na extensão TGA.");
        // Gamma metadata does not change the explicit TextureProfile. Color
        // correction tables would, so refuse instead of discarding them.
        for(u32 i=482;i<486;++i)if(bytes[extension+i])return fail("Tabela de correção de cor TGA ainda não suportada.");
      }
    }
  }
  return true;
}
} // namespace ae::resources
