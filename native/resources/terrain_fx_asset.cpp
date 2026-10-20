#include "resources/terrain_fx_asset.h"
#include "core/sha256.h"
#include "resources/terrain_text_reader.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <type_traits>
namespace ae::resources {
namespace {
bool bounded(double v,double a,double b){return std::isfinite(v)&&v>=a&&v<=b;}
template<class F>void strokeFields(TerrainFxStroke &v,F f){
  f(v.identity,v.gesture,v.x,v.z,v.radiusMeters,v.aspect,v.rotationDegrees,
    v.flow,v.falloff,v.pressure,v.sampleSeconds,v.target,v.hardness,v.opacity,v.slopeMin,v.slopeMax);
}
template<class T>bool unsignedValue(std::istream &s,T &out){
  s>>std::ws;const int c=s.peek();if(c<'0'||c>'9')return false;
  return bool(s>>out);
}
}
std::string terrainFxPrefixHash(const TerrainFxAsset &a,u32 count,std::string_view baseHash){
  if(count>a.strokes.size())return {};
  std::ostringstream w;w.imbue(std::locale::classic());w<<std::setprecision(std::numeric_limits<double>::max_digits10);
  w<<"TerrainFxPrefix1 "<<a.width<<' '<<a.height<<' '<<a.domain.originX<<' '<<a.domain.originZ<<' '<<a.domain.sizeX<<' '<<a.domain.sizeZ<<' '
   <<a.sourceImage.high<<' '<<a.sourceImage.low<<' '<<std::quoted(a.sourcePropertiesJson)<<' '<<baseHash<<' '<<count<<' ';
  for(u32 i=0;i<count;++i){auto s=a.strokes[i];w<<u32(s.channel)<<' '<<u32(s.shape)<<' '<<s.slopeFilter<<' ';strokeFields(s,[&](const auto &...v){((w<<v<<' '),...);});}
  const auto text=w.str();return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));
}
bool validTerrainFxAsset(const TerrainFxAsset &a,std::string &error,const TerrainFxLimits &limits){
  const auto fail=[&](const char *s){error=s;return false;};
  if(!a.width||!a.height||a.width>limits.maximumDimension||a.height>limits.maximumDimension||
     u64(a.width)*a.height>limits.maximumPixels)return fail("FX dimensions exceed the explicit grid budget");
  const u64 pixels=u64(a.width)*a.height;
  if(pixels>limits.maximumWorkingBytes/20)return fail("FX authoring field and byte snapshot exceed working budget");
  if(!bounded(a.domain.originX,-1e8,1e8)||!bounded(a.domain.originZ,-1e8,1e8)||
     !bounded(a.domain.sizeX,.000001,1e8)||!bounded(a.domain.sizeZ,.000001,1e8))return fail("Invalid FX terrain domain");
  if((!a.seedPixels.empty()&&a.seedPixels.size()!=pixels*4)||(a.sourceImage.valid()&&a.seedPixels.empty()))return fail("FX source requires a complete immutable seed bitmap");
  if(a.sourcePropertiesJson.size()>65536||a.sourcePropertiesJson.find('\0')!=std::string::npos)return fail("FX provenance exceeds 64 KiB or contains NUL");
  if(a.strokes.size()>limits.maximumStrokes)return fail("FX stroke budget exceeded; no strokes were truncated");
  if(a.bakedStrokeCount>a.strokes.size()||(a.bakedStrokeCount&&a.seedPixels.empty()))return fail("FX checkpoint requires a complete bitmap and matching stroke prefix");
  std::set<u64> ids;
  for(const auto &s:a.strokes){
    if(!s.identity||!ids.insert(s.identity).second||u32(s.channel)>3||u32(s.shape)>1)return fail("Invalid or duplicate FX stroke identity/channel/shape");
    if(!bounded(s.x,-1e8,1e8)||!bounded(s.z,-1e8,1e8)||!bounded(s.radiusMeters,.000001,1e8)||
       !bounded(s.aspect,.0001,65536)||!bounded(s.rotationDegrees,-36000,36000))return fail("Invalid FX brush transform");
    if(!bounded(s.flow,0,65536)||!bounded(s.falloff,.0001,64)||!bounded(s.pressure,0,1)||
       !bounded(s.sampleSeconds,0,60)||!bounded(s.target,0,1)||!bounded(s.hardness,0,1)||!bounded(s.opacity,0,1)||
       !bounded(s.slopeMin,0,1)||!bounded(s.slopeMax,0,1)||s.slopeMax<s.slopeMin)return fail("Invalid FX Target/time/pressure/falloff/slope parameter");
  }
  const bool checkpoint=!a.checkpointPrefixHash.empty()||!a.checkpointBaseHash.empty()||!a.checkpointPixelsHash.empty();
  const auto hash=[](std::string_view value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string_view::npos;};
  if(a.bakedStrokeCount&&!checkpoint)return fail("FX baked prefix is missing its checkpoint identity");
  if(checkpoint){
    if(a.seedPixels.empty()||!hash(a.checkpointBaseHash)||!hash(a.checkpointPrefixHash)||!hash(a.checkpointPixelsHash))return fail("Invalid FX checkpoint identity/hash");
    if(a.checkpointPixelsHash!=Sha256::hex(a.seedPixels)||a.checkpointPrefixHash!=terrainFxPrefixHash(a,a.bakedStrokeCount,a.checkpointBaseHash))return fail("FX checkpoint is incompatible with prefix, domain, resolution, source or bitmap; explicit rebase required");
  }
  error.clear();return true;
}
bool serializeTerrainFxAsset(const TerrainFxAsset &a,std::string &out,std::string &error,const TerrainFxLimits &limits){
  if(!validTerrainFxAsset(a,error,limits))return false;
  u64 runs=0;
  for(usize i=0;i<a.seedPixels.size();i+=4)if(i==0||std::memcmp(a.seedPixels.data()+i,a.seedPixels.data()+i-4,4)!=0)++runs;
  const u32 encoding=runs*8<a.seedPixels.size()?1u:0u; // deterministic pixel RLE only when smaller
  const u64 encodedBytes=encoding?runs*8:a.seedPixels.size();
  if(encodedBytes>limits.maximumMetadataBytes/2){error="FX encoded bitmap metadata exceeds byte budget";return false;}
  std::ostringstream w;w.imbue(std::locale::classic());w<<std::setprecision(std::numeric_limits<double>::max_digits10);
  w<<"AETERRAINFX1 "<<a.enabled<<' '<<a.width<<' '<<a.height<<' '<<a.domain.originX<<' '<<a.domain.originZ<<' '<<a.domain.sizeX<<' '<<a.domain.sizeZ<<' '
   <<a.sourceImage.high<<' '<<a.sourceImage.low<<' '<<std::quoted(a.sourcePropertiesJson)<<' '<<std::quoted(a.checkpointBaseHash)<<' '<<std::quoted(a.checkpointPrefixHash)<<' '<<std::quoted(a.checkpointPixelsHash)<<' '<<a.bakedStrokeCount<<' '<<encoding<<' '<<a.seedPixels.size()<<' '<<encodedBytes<<' ';
  constexpr char hex[]="0123456789abcdef";
  const auto emit=[&](u8 b){w<<hex[b>>4]<<hex[b&15];};
  if(!encoding){for(u8 b:a.seedPixels)emit(b);}
  else for(usize i=0;i<a.seedPixels.size();){usize end=i+4;while(end<a.seedPixels.size()&&std::memcmp(a.seedPixels.data()+i,a.seedPixels.data()+end,4)==0)end+=4;
    const u32 count=u32((end-i)/4);for(u32 shift=0;shift<32;shift+=8)emit(u8(count>>shift));for(u32 c=0;c<4;++c)emit(a.seedPixels[i+c]);i=end;}
  w<<" ; "<<a.strokes.size()<<' ';
  for(auto s:a.strokes){
    w<<u32(s.channel)<<' '<<u32(s.shape)<<' '<<s.slopeFilter<<' ';
    strokeFields(s,[&](const auto &...v){((w<<v<<' '),...);});
    if(w.tellp()<0||u64(w.tellp())>limits.maximumMetadataBytes){error="FX metadata exceeds byte budget";return false;}
  }
  auto text=std::move(w).str();if(text.size()>limits.maximumMetadataBytes){error="FX metadata exceeds byte budget";return false;}
  out=std::move(text);error.clear();return true;
}
bool deserializeTerrainFxAsset(std::string_view text,TerrainFxAsset &out,std::string &error,const TerrainFxLimits &limits){
  const auto fail=[&](const char *s){error=s;return false;};
  if(text.size()>limits.maximumMetadataBytes||text.find('\0')!=std::string_view::npos)return fail("FX metadata exceeds byte budget or contains NUL");
  terrain_detail::TextBuffer buffer(text);std::istream r(&buffer);r.imbue(std::locale::classic());std::string magic;r>>magic;
  if(magic!="AETERRAINFX1")return fail("Unsupported FX asset version");
  TerrainFxAsset a;u32 enabled=0;
  if(!unsignedValue(r,enabled)||enabled>1||!unsignedValue(r,a.width)||!unsignedValue(r,a.height))return fail("Invalid FX header");a.enabled=enabled!=0;
  if(!(r>>a.domain.originX>>a.domain.originZ>>a.domain.sizeX>>a.domain.sizeZ)||!unsignedValue(r,a.sourceImage.high)||!unsignedValue(r,a.sourceImage.low))return fail("Invalid FX domain/source identity");
  r>>std::ws;if(r.peek()!='"'||!(r>>std::quoted(a.sourcePropertiesJson)))return fail("Invalid FX source provenance");
  if(a.sourcePropertiesJson.size()>65536)return fail("FX source provenance exceeds 64 KiB");
  for(auto *hash:{&a.checkpointBaseHash,&a.checkpointPrefixHash,&a.checkpointPixelsHash}){
    r>>std::ws;if(r.peek()!='"'||!(r>>std::quoted(*hash))||hash->size()>64)return fail("Invalid FX checkpoint hash metadata");
  }
  u64 bytes=0,encodedBytes=0;u32 encoding=0;
  if(!unsignedValue(r,a.bakedStrokeCount)||!unsignedValue(r,encoding)||!unsignedValue(r,bytes)||!unsignedValue(r,encodedBytes)||encoding>1)return fail("Invalid FX checkpoint/bitmap encoding or size");
  if(!a.width||!a.height||a.width>limits.maximumDimension||a.height>limits.maximumDimension||u64(a.width)*a.height>limits.maximumPixels||
     u64(a.width)*a.height>limits.maximumWorkingBytes/20||(bytes&&bytes!=u64(a.width)*a.height*4)||encodedBytes>limits.maximumMetadataBytes/2||
     (encoding==0&&encodedBytes!=bytes)||(encoding==1&&(!bytes||!encodedBytes||encodedBytes%8||encodedBytes>=bytes)))return fail("FX bitmap dimension/encoding/byte budget exceeded");
  r>>std::ws;if(encodedBytes>buffer.remaining()/2)return fail("Truncated FX encoded bitmap before allocation");
  a.seedPixels.resize(usize(bytes));
  const auto digit=[](int c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  const auto readByte=[&](u8 &b){const int x=digit(r.get()),y=digit(r.get());if(x<0||y<0)return false;b=u8(x*16+y);return true;};
  if(!encoding){for(auto &b:a.seedPixels)if(!readByte(b))return fail("Invalid/truncated FX raw bitmap hex");}
  else{usize written=0;for(u64 record=0;record<encodedBytes/8;++record){u32 count=0;std::array<u8,4> pixel{};
      for(u32 shift=0;shift<32;shift+=8){u8 b=0;if(!readByte(b))return fail("Truncated FX RLE count");count|=u32(b)<<shift;}
      for(auto &b:pixel)if(!readByte(b))return fail("Truncated FX RLE pixel");
      if(!count||u64(count)>(bytes-written)/4)return fail("FX RLE expansion exceeds declared grid");
      for(u32 i=0;i<count;++i)for(u8 b:pixel)a.seedPixels[written++]=b;
    }if(written!=bytes)return fail("FX RLE does not cover the complete declared grid");}
  r>>std::ws;if(r.get()!=';')return fail("Missing FX bitmap terminator");
  u32 count=0;if(!unsignedValue(r,count)||count>limits.maximumStrokes)return fail("FX stroke count exceeds budget");
  // A stroke needs many numeric tokens; reject tiny malicious payloads before resize.
  if(count>text.size()/20)return fail("Truncated FX stroke count");
  a.strokes.resize(count);bool ok=true;
  for(auto &s:a.strokes){u32 channel=0,shape=0,slope=0;
    if(!unsignedValue(r,channel)||!unsignedValue(r,shape)||!unsignedValue(r,slope)||channel>3||shape>1||slope>1)return fail("Invalid FX stroke enum/filter");
    s.channel=TerrainFxChannel(channel);s.shape=TerrainFxBrushShape(shape);s.slopeFilter=slope!=0;
    strokeFields(s,[&](auto &...v){const auto read=[&](auto &x){using T=std::remove_reference_t<decltype(x)>;if constexpr(std::is_unsigned_v<T>)ok=unsignedValue(r,x)&&ok;else ok=bool(r>>x)&&ok;};(read(v),...);});
    if(!ok)return fail("Invalid/truncated FX stroke fields");
  }
  r>>std::ws;if(!r.eof())return fail("Trailing FX metadata tokens");
  if(!validTerrainFxAsset(a,error,limits))return false;
  out=std::move(a);error.clear();return true;
}
}
