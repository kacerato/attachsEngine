#include "resources/tiff_decode.h"
#include <algorithm>
#include <array>
#include <map>
#include <limits>
#include <cmath>
#include <cstring>

namespace ae::resources {
namespace {
struct Field {u16 type=0;u32 count=0;usize offset=0;};
struct Directory {
  std::span<const u8> bytes;bool little=false;std::map<u16,Field> fields;
  u16 half(usize at) const {return little?static_cast<u16>(bytes[at]|bytes[at+1]<<8):static_cast<u16>(bytes[at]<<8|bytes[at+1]);}
  u32 word(usize at) const {
    u32 value=0;for(u32 n=0;n<4;++n)value|=static_cast<u32>(bytes[at+n])<<(little?n*8:(3-n)*8);return value;
  }
  bool range(u64 at,u64 size) const {return at<=bytes.size()&&size<=bytes.size()-at;}
  bool value(u16 tag,u32 index,u32 &out) const {
    const auto found=fields.find(tag);if(found==fields.end())return false;
    const auto &field=found->second;if(index>=field.count)return false;
    const auto at=field.offset+index*(field.type==3?2:field.type==4?4:1);
    if(field.type==3)out=half(at);else if(field.type==4)out=word(at);else if(field.type==1)out=bytes[at];else return false;
    return true;
  }
  bool scalar(u16 tag,u32 fallback,u32 &out) const {
    const auto found=fields.find(tag);if(found==fields.end()){out=fallback;return true;}
    return found->second.count==1&&value(tag,0,out);
  }
};
bool matrixSrgb(std::span<const u8> bytes) {
  // Restricted ICC v2 matrix/TRC recognition. Names/descriptions are never
  // evidence of colorimetry. LUT/perceptual transforms require an actual CMM.
  Directory d{bytes,false,{}};
  if(bytes.size()<132||d.word(0)!=bytes.size()||bytes[8]!=2||
     std::memcmp(bytes.data()+12,"mntrRGB XYZ ",12)||std::memcmp(bytes.data()+36,"acsp",4)||d.word(64)!=1)return false;
  const u32 count=d.word(128);if(count>128||!d.range(132,static_cast<u64>(count)*12))return false;
  std::map<u32,std::pair<u32,u32>> tags;
  for(u32 n=0;n<count;++n) {
    const auto at=132+n*12;const u32 tag=d.word(at),offset=d.word(at+4),size=d.word(at+8);
    if(offset<132+count*12||!d.range(offset,size)||!tags.emplace(tag,std::pair(offset,size)).second)return false;
    if(tag==0x41324230||tag==0x41324231||tag==0x41324232||tag==0x42324130||tag==0x42324131||tag==0x42324132)return false;
  }
  const auto white=tags.find(0x77747074);if(white==tags.end()||white->second.second!=20||d.word(white->second.first)!=0x58595a20)return false;
  constexpr double whites[2][3]{{.9642,1,.8249},{.950455927,1,1.089057751}};
  bool validWhite=false;
  for(const auto &candidate:whites) {
    bool match=true;for(u32 c=0;c<3;++c)match=match&&std::abs(static_cast<i32>(d.word(white->second.first+8+c*4))/65536.0-candidate[c])<.0002;
    validWhite=validWhite||match;
  }
  if(!validWhite)return false;
  if(const auto black=tags.find(0x626b7074);black!=tags.end()) {
    if(black->second.second!=20||d.word(black->second.first)!=0x58595a20)return false;
    for(u32 c=0;c<3;++c)if(d.word(black->second.first+8+c*4)!=0)return false;
  }
  constexpr u32 primaries[]{0x7258595a,0x6758595a,0x6258595a};
  // ICC sRGB characterization, D50 PCS, columns R/G/B. Tolerance includes
  // legacy v2 Bradford coefficients; output is the existing RGBA8 contract.
  constexpr double expected[3][3]{{.436030342570117,.222438466210245,.013897440074263},
      {.385101860087134,.716942745571917,.097076381494207},{.143067806654203,.060618777416563,.713926257896652}};
  for(u32 p=0;p<3;++p) {
    const auto found=tags.find(primaries[p]);if(found==tags.end()||found->second.second!=20)return false;
    const auto at=found->second.first;if(d.word(at)!=0x58595a20)return false;
    for(u32 c=0;c<3;++c)if(std::abs(static_cast<i32>(d.word(at+8+c*4))/65536.0-expected[p][c])>.0002)return false;
  }
  constexpr u32 curves[]{0x72545243,0x67545243,0x62545243};
  for(const auto tag:curves) {
    const auto found=tags.find(tag);if(found==tags.end()||found->second.second<12)return false;
    const auto at=found->second.first;if(d.word(at)!=0x63757276)return false;
    const u32 samples=d.word(at+8);
    if(samples<256||samples>65536||found->second.second!=12+samples*2)return false;
    for(u32 n=0;n<samples;++n) {
      const double encoded=static_cast<double>(n)/(samples-1);
      const double linear=encoded<=.04045?encoded/12.92:std::pow((encoded+.055)/1.055,2.4);
      if(std::abs(d.half(at+12+n*2)-linear*65535)>.51)return false;
    }
  }
  return true;
}
bool directory(std::span<const u8> bytes,const ImageDecodeLimits &limits,Directory &d,TiffInfo &out,std::string &error) {
  out={};error.clear();d={bytes,false,{}};
  const auto fail=[&](const char *message){error=message;return false;};
  if(!isTiff(bytes)||bytes.size()>limits.maximumEncodedBytes)return fail("TIFF inválido ou maior que o limite de bytes.");
  d.little=bytes[0]=='I';const auto offset=d.word(4);
  if(offset<8||!d.range(offset,2))return fail("Diretório TIFF fora da fonte.");
  const auto count=d.half(offset);
  if(!count||count>1024||!d.range(offset+2ull,count*12ull+4))return fail("Diretório TIFF truncado ou com campos demais.");
  for(u32 n=0;n<count;++n) {
    const usize at=offset+2+n*12;const u16 tag=d.half(at),type=d.half(at+2);const u32 elements=d.word(at+4);
    static constexpr u8 sizes[]{0,1,1,2,4,8,1,1,2,4,8,4,8};
    if(!type||type>=std::size(sizes)||!elements)return fail("Campo TIFF inválido.");
    const u64 size=static_cast<u64>(elements)*sizes[type];const usize data=size<=4?at+8:d.word(at+8);
    if(!d.range(data,size)||!d.fields.emplace(tag,Field{type,elements,data}).second)return fail("Campo TIFF truncado ou duplicado.");
  }
  if(d.word(offset+2+count*12)!=0)return fail("TIFF com várias páginas requer seleção de subimagem; este perfil não as combina.");
  u32 width=0,height=0,bits=0,samples=0,compression=0,photo=0,planar=0,orientation=0,predictor=0,format=0;
  if(!d.scalar(256,0,width)||!d.scalar(257,0,height)||!d.scalar(277,1,samples)||
     !d.scalar(259,1,compression)||!d.scalar(262,0xffffffff,photo)||!d.scalar(284,1,planar)||
     !d.scalar(274,1,orientation)||!d.scalar(317,1,predictor)||!d.scalar(339,1,format))return fail("Campos escalares TIFF inválidos.");
  if(!width||!height||width>limits.maximumDimension||height>limits.maximumDimension||
     static_cast<u64>(width)*height>limits.maximumPixels||static_cast<u64>(width)*height>std::numeric_limits<usize>::max()/4)
    return fail("TIFF excede o limite de dimensões.");
  if(photo!=2||samples<3||samples>4||planar!=1||predictor!=1||format!=1||orientation<1||orientation>8)
    return fail("Este perfil TIFF requer RGB inteiro, canais intercalados e orientação válida.");
  if(compression!=1)return fail("Compressão TIFF ainda não integrada neste perfil; fonte preservada.");
  const auto found=d.fields.find(258);
  if(found==d.fields.end()||found->second.count!=samples||!d.value(258,0,bits)||(bits!=8&&bits!=16))return fail("TIFF requer 8 ou 16 bits uniformes por canal.");
  for(u32 n=1;n<samples;++n){u32 current=0;if(!d.value(258,n,current)||current!=bits)return fail("TIFF com precisões diferentes por canal não representável.");}
  if(d.fields.contains(322)||d.fields.contains(323)||d.fields.contains(324)||d.fields.contains(325))return fail("TIFF em tiles ainda não integrado neste perfil.");
  u32 extra=0;
  if(samples==4) {
    if(!d.scalar(338,0,extra)||extra>2)return fail("Semântica do canal extra TIFF inválida.");
  } else if(d.fields.contains(338))return fail("Canal extra TIFF declarado sem amostra correspondente.");
  if(extra==1)return fail("Alfa TIFF associado requer conversão de premultiplicação; este perfil recusa para preservar a aparência.");
  out.width=width;out.height=height;out.bits=static_cast<u16>(bits);out.samples=static_cast<u16>(samples);
  out.orientation=static_cast<u16>(orientation);out.extra=static_cast<u16>(extra);out.littleEndian=d.little;out.undefinedExtra=samples==4&&extra==0;
  if(const auto icc=d.fields.find(34675);icc!=d.fields.end()) {
    if(icc->second.type!=7||icc->second.count>4u<<20)return fail("Perfil ICC TIFF inválido ou maior que 4 MiB.");
    out.colorProfile=bytes.subspan(icc->second.offset,icc->second.count);
    out.matrixSrgb=matrixSrgb(out.colorProfile);
  }
  return true;
}
}
bool isTiff(std::span<const u8> bytes) {
  return bytes.size()>=8&&((bytes[0]=='I'&&bytes[1]=='I'&&bytes[2]==42&&bytes[3]==0)||
      (bytes[0]=='M'&&bytes[1]=='M'&&bytes[2]==0&&bytes[3]==42));
}
bool inspectTiff(std::span<const u8> bytes,const ImageDecodeLimits &limits,TiffInfo &out,std::string &diagnostic) {
  Directory d;return directory(bytes,limits,d,out,diagnostic);
}
bool tiffPreparationLosses(const TiffInfo &info,const TiffDecodePolicy &policy,std::vector<std::string> &losses,std::string &diagnostic) {
  losses.clear();diagnostic.clear();
  const auto fail=[&](const char *message){diagnostic=message;return false;};
  if(static_cast<u8>(policy.undefinedExtra)>static_cast<u8>(TiffUndefinedExtra::Alpha))return fail("Política TIFF inválida.");
  if(info.undefinedExtra&&policy.undefinedExtra==TiffUndefinedExtra::Reject)return fail("TIFF possui quarto canal indefinido. Escolha ignorar ou interpretar como alfa na receita.");
  if(info.bits==16&&!policy.quantize16)return fail("TIFF de 16 bits exige autorização de quantização para RGBA8 na receita.");
  if(!info.colorProfile.empty()&&!info.matrixSrgb&&!policy.ignoreColorProfile)return fail("TIFF possui ICC não reconhecido como matriz/TRC sRGB. A conversão de cor não está integrada; autorize usar os valores sem conversão ou forneça sRGB convertido.");
  if(info.undefinedExtra)losses.push_back(policy.undefinedExtra==TiffUndefinedExtra::Alpha?"Quarto canal TIFF indefinido interpretado como alfa por escolha autoral.":"Quarto canal TIFF indefinido ignorado; original preservado.");
  if(info.bits==16)losses.push_back("Canais TIFF de 16 bits quantizados para 8 bits por arredondamento; original preservado.");
  if(!info.colorProfile.empty()&&!info.matrixSrgb)losses.push_back("Perfil ICC TIFF preservado somente na fonte; valores usados sem transformação de cor por escolha autoral.");
  return true;
}
bool decodeTiffRgba8(std::span<const u8> bytes,const ImageDecodeLimits &limits,const TiffDecodePolicy &policy,
                    DecodedImage &out,std::vector<std::string> &losses,std::string &diagnostic,const std::atomic<bool> *cancel) {
  out={};losses.clear();Directory d;TiffInfo info;
  const auto fail=[&](const char *message){out={};losses.clear();diagnostic=message;return false;};
  if(!directory(bytes,limits,d,info,diagnostic))return false;
  if(!tiffPreparationLosses(info,policy,losses,diagnostic))return false;
  u32 rows=0;if(!d.scalar(278,info.height,rows)||!rows)return fail("TIFF com tamanho de faixa inválido.");
  const u64 stripCount=(static_cast<u64>(info.height)+rows-1)/rows;
  const auto offsets=d.fields.find(273),counts=d.fields.find(279);
  if(stripCount>65536||offsets==d.fields.end()||counts==d.fields.end()||offsets->second.count!=stripCount||counts->second.count!=stripCount)return fail("Faixas TIFF ausentes ou incompatíveis.");
  const u64 rowBytes=static_cast<u64>(info.width)*info.samples*(info.bits/8);
  std::vector<std::pair<u32,u32>> strips;strips.reserve(static_cast<usize>(stripCount));
  for(u32 n=0;n<stripCount;++n) {
    u32 at=0,size=0;const auto actualRows=std::min<u64>(rows,info.height-static_cast<u64>(n)*rows);
    if(!d.value(273,n,at)||!d.value(279,n,size)||size!=actualRows*rowBytes||!d.range(at,size))return fail("Faixa TIFF truncada ou de tamanho divergente.");
    strips.emplace_back(at,size);
  }
  auto sorted=strips;std::sort(sorted.begin(),sorted.end());
  for(usize n=1;n<sorted.size();++n)if(static_cast<u64>(sorted[n-1].first)+sorted[n-1].second>sorted[n].first)return fail("Faixas TIFF sobrepostas.");
  if(cancel&&cancel->load(std::memory_order_relaxed))return fail("Importação TIFF cancelada.");
  out.width=info.orientation>=5?info.height:info.width;out.height=info.orientation>=5?info.width:info.height;
  out.rgba.resize(static_cast<usize>(out.width)*out.height*4);
  const bool alpha=info.samples==4&&(info.extra==2||policy.undefinedExtra==TiffUndefinedExtra::Alpha);
  for(u32 y=0;y<info.height;++y) {
    if(cancel&&cancel->load(std::memory_order_relaxed))return fail("Importação TIFF cancelada.");
    for(u32 x=0;x<info.width;++x) {
      const usize at=strips[y/rows].first+static_cast<usize>(y%rows)*rowBytes+static_cast<usize>(x)*info.samples*(info.bits/8);
      u32 dx=x,dy=y;
      switch(info.orientation) {
      case 2:dx=info.width-1-x;break;
      case 3:dx=info.width-1-x;dy=info.height-1-y;break;
      case 4:dy=info.height-1-y;break;
      case 5:dx=y;dy=x;break;
      case 6:dx=info.height-1-y;dy=x;break;
      case 7:dx=info.height-1-y;dy=info.width-1-x;break;
      case 8:dx=y;dy=info.width-1-x;break;
      default:break;
      }
      const usize target=(static_cast<usize>(dy)*out.width+dx)*4;
      for(u32 c=0;c<4;++c)out.rgba[target+c]=c==3&&!alpha?255:info.bits==8?bytes[at+c]:static_cast<u8>((static_cast<u32>(d.half(at+c*2))+128)/257);
    }
  }
  diagnostic.clear();return true;
}
}
