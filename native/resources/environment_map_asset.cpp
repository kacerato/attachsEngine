#include "resources/environment_map_asset.h"

#include "core/sha256.h"
#include "resources/image_decode.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>

namespace ae::resources {
namespace {
constexpr float Pi=3.14159265358979323846f;
constexpr u32 CacheMagic=0x434D4541; // AEMC
using Vec3=std::array<float,3>;
struct FloatImage {u32 width=0,height=0;std::vector<float> rgb;};

Vec3 add(Vec3 a,const Vec3 &b){for(u32 i=0;i<3;++i){a[i]+=b[i];}return a;}
Vec3 mul(Vec3 a,float s){for(float &v:a){v*=s;}return a;}
float dot(const Vec3&a,const Vec3&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 cross(const Vec3&a,const Vec3&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
Vec3 normalize(Vec3 v){const float l=std::sqrt(dot(v,v));return l>1e-12f?mul(v,1.0f/l):Vec3{};}
bool powerOfTwo(u32 value){return value&&!(value&(value-1));}
float radicalInverse(u32 bits){
  bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xaaaaaaaau)>>1u);
  bits=((bits&0x33333333u)<<2u)|((bits&0xccccccccu)>>2u);
  bits=((bits&0x0f0f0f0fu)<<4u)|((bits&0xf0f0f0f0u)>>4u);
  bits=((bits&0x00ff00ffu)<<8u)|((bits&0xff00ff00u)>>8u);
  return static_cast<float>(bits)*2.3283064365386963e-10f;
}

u16 floatToHalf(float value) {
  const u32 bits=std::bit_cast<u32>(value);const u32 sign=(bits>>16u)&0x8000u;
  const u32 magnitude=bits&0x7fffffffu;
  if(magnitude>=0x47800000u)return static_cast<u16>(sign|0x7bffu);
  if(magnitude<0x33000000u)return static_cast<u16>(sign);
  if(magnitude<0x38800000u) {
    const u32 mantissa=(magnitude&0x7fffffu)|0x800000u;
    const u32 shift=113u-(magnitude>>23u);
    return static_cast<u16>(sign+((mantissa+(1u<<(shift+12u)))>>(shift+13u)));
  }
  return static_cast<u16>(sign+((magnitude+0xc8000000u+0x1000u)>>13u));
}

Vec3 sample(const FloatImage &image,const Vec3 &direction) {
  const Vec3 d=normalize(direction);
  const float u=(std::atan2(d[2],d[0])/(2*Pi)+.5f)*image.width-.5f;
  const float v=std::acos(std::clamp(d[1],-1.0f,1.0f))/Pi*image.height-.5f;
  const i32 xi=static_cast<i32>(std::floor(u)),yi=static_cast<i32>(std::floor(v));
  const float tx=u-std::floor(u),ty=v-std::floor(v);
  const auto at=[&](i32 x,i32 y,u32 c){
    x%=static_cast<i32>(image.width);if(x<0)x+=image.width;
    y=std::clamp(y,0,static_cast<i32>(image.height)-1);
    return image.rgb[(static_cast<usize>(y)*image.width+static_cast<u32>(x))*3+c];};
  Vec3 result{};
  for(u32 c=0;c<3;++c)result[c]=(at(xi,yi,c)*(1-tx)+at(xi+1,yi,c)*tx)*(1-ty)+
      (at(xi,yi+1,c)*(1-tx)+at(xi+1,yi+1,c)*tx)*ty;
  return result;
}

Vec3 sample(const DecodedHdrImage &image,const Vec3 &direction) {
  const Vec3 d=normalize(direction);
  const float u=(std::atan2(d[2],d[0])/(2*Pi)+.5f)*image.width-.5f;
  const float v=std::acos(std::clamp(d[1],-1.0f,1.0f))/Pi*image.height-.5f;
  const i32 xi=static_cast<i32>(std::floor(u)),yi=static_cast<i32>(std::floor(v));
  const float tx=u-std::floor(u),ty=v-std::floor(v);
  const auto at=[&](i32 x,i32 y,u32 c){x%=static_cast<i32>(image.width);if(x<0)x+=image.width;
    y=std::clamp(y,0,static_cast<i32>(image.height)-1);
    return image.rgba[(static_cast<usize>(y)*image.width+static_cast<u32>(x))*4+c];};
  Vec3 result{};
  for(u32 c=0;c<3;++c)
    result[c]=(at(xi,yi,c)*(1-tx)+at(xi+1,yi,c)*tx)*(1-ty)+
        (at(xi,yi+1,c)*(1-tx)+at(xi+1,yi+1,c)*tx)*ty;
  return result;
}

Vec3 directionFromEquirect(u32 x,u32 y,u32 width,u32 height) {
  const float phi=((x+.5f)/width-.5f)*2*Pi,theta=(y+.5f)/height*Pi,s=std::sin(theta);
  return {s*std::cos(phi),std::cos(theta),s*std::sin(phi)};
}

Vec3 octaDirection(u32 x,u32 y,u32 size) {
  float px=(x+.5f)/size*2-1,pz=(y+.5f)/size*2-1,py=1-std::abs(px)-std::abs(pz);
  if(py<0){const float ox=px;px=(1-std::abs(pz))*(ox<0?-1:1);pz=(1-std::abs(ox))*(pz<0?-1:1);}
  return normalize({px,py,pz});
}

bool appendHalf(renderer::Rgba16fMipChain &out,const Vec3 &rgb) {
  for(float value:rgb)if(!std::isfinite(value)||value<0||value>65504)return false;
  out.texels.push_back(floatToHalf(rgb[0]));out.texels.push_back(floatToHalf(rgb[1]));
  out.texels.push_back(floatToHalf(rgb[2]));out.texels.push_back(floatToHalf(1));return true;
}

bool makePanorama(const DecodedHdrImage &source,u32 width,EnvironmentMapCancel cancel,
                  FloatImage &linear,renderer::Rgba16fMipChain &out) {
  linear.width=width;linear.height=width/2;linear.rgb.resize(static_cast<usize>(width)*linear.height*3);
  for(u32 y=0;y<linear.height;++y){if(cancel.cancelled())return false;for(u32 x=0;x<width;++x){
    const Vec3 color=sample(source,directionFromEquirect(x,y,width,linear.height));
    for(u32 c=0;c<3;++c)linear.rgb[(static_cast<usize>(y)*width+x)*3+c]=color[c];
  }}
  // Longitude collapses at the poles. Averaging the first/last target rows
  // prevents a source seam from becoming a bright radial streak.
  for(u32 y:{0u,linear.height-1}) {
    Vec3 average{};
    for(u32 x=0;x<width;++x)
      for(u32 c=0;c<3;++c)
        average[c]+=linear.rgb[(static_cast<usize>(y)*width+x)*3+c]/width;
    for(u32 x=0;x<width;++x)
      for(u32 c=0;c<3;++c)
        linear.rgb[(static_cast<usize>(y)*width+x)*3+c]=average[c];
  }
  out.width=width;out.height=linear.height;out.levels=1;
  for(u32 size=width;size>1;size/=2)++out.levels;
  out.texels.reserve(static_cast<usize>(out.expectedHalfCount()));
  for(usize p=0;p<static_cast<usize>(linear.width)*linear.height;++p)
    if(!appendHalf(out,{linear.rgb[p*3],linear.rgb[p*3+1],linear.rgb[p*3+2]}))return false;
  const FloatImage *previous=&linear;FloatImage level;
  for(u32 mip=1;mip<out.levels;++mip){
    FloatImage next{std::max(1u,previous->width/2),std::max(1u,previous->height/2),{}};
    next.rgb.resize(static_cast<usize>(next.width)*next.height*3);
    for(u32 y=0;y<next.height;++y){if(cancel.cancelled())return false;for(u32 x=0;x<next.width;++x)
      for(u32 c=0;c<3;++c){float sum=0;for(u32 oy=0;oy<2;++oy)for(u32 ox=0;ox<2;++ox){
        const u32 sx=std::min(x*2+ox,previous->width-1),sy=std::min(y*2+oy,previous->height-1);
        sum+=previous->rgb[(static_cast<usize>(sy)*previous->width+sx)*3+c];}
        next.rgb[(static_cast<usize>(y)*next.width+x)*3+c]=sum*.25f;}}
    for(usize p=0;p<static_cast<usize>(next.width)*next.height;++p)
      if(!appendHalf(out,{next.rgb[p*3],next.rgb[p*3+1],next.rgb[p*3+2]}))return false;
    level=std::move(next);previous=&level;
  }
  return true;
}

bool projectSh(const FloatImage &source,renderer::DiffuseIrradianceSh9 &out,EnvironmentMapCancel cancel){
  constexpr float bands[3]{Pi,2*Pi/3,Pi/4};double coefficients[9][3]{};
  for(u32 y=0;y<source.height;++y){if(cancel.cancelled())return false;
    const double theta=(y+.5)*Pi/source.height,s=std::sin(theta),cy=std::cos(theta);
    const double edge0=static_cast<double>(y)*Pi/source.height,edge1=static_cast<double>(y+1)*Pi/source.height;
    const double solid=(std::cos(edge0)-std::cos(edge1))*2*Pi/source.width;
    for(u32 x=0;x<source.width;++x){const double phi=((x+.5)/source.width-.5)*2*Pi;
      const double dx=s*std::cos(phi),dz=s*std::sin(phi);
      const double basis[9]{.2820947918,.4886025119*cy,.4886025119*dz,.4886025119*dx,
        1.0925484306*dx*cy,1.0925484306*cy*dz,.3153915653*(3*cy*cy-1),
        1.0925484306*dx*dz,.5462742153*(dx*dx-dz*dz)};
      for(u32 i=0;i<9;++i)for(u32 c=0;c<3;++c)
        coefficients[i][c]+=source.rgb[(static_cast<usize>(y)*source.width+x)*3+c]*basis[i]*solid;
    }}
  for(u32 i=0;i<9;++i) {
    for(u32 c=0;c<3;++c) {
      const u32 band=i==0?0:i<=3?1:2;
      const double value=coefficients[i][c]*bands[band];
      if(!std::isfinite(value))return false;
      out[i][c]=static_cast<float>(value);
    }
    out[i][3]=0;
  }
  return true;
}

bool bakeSpecular(const FloatImage &source,const EnvironmentMapImportSettings &settings,
                  EnvironmentMapCancel cancel,renderer::Rgba16fMipChain &out){
  out.width=out.height=settings.specularSize;out.levels=1;
  for(u32 size=settings.specularSize;size>1;size/=2)++out.levels;
  out.texels.reserve(static_cast<usize>(out.expectedHalfCount()));
  for(u32 mip=0,size=settings.specularSize;mip<out.levels;++mip,size=std::max(1u,size/2)){
    const float roughness=out.levels>1?static_cast<float>(mip)/(out.levels-1):0;
    const u32 samples=mip?settings.specularSamples:1;
    for(u32 y=0;y<size;++y){if(cancel.cancelled())return false;for(u32 x=0;x<size;++x){
      const Vec3 n=octaDirection(x,y,size);Vec3 total{};float weight=0;
      Vec3 helper=std::abs(n[1])>.999f?Vec3{1,0,0}:Vec3{0,1,0};
      const Vec3 tangent=normalize(cross(helper,n)),bitangent=cross(n,tangent);
      const float alpha=std::max(.001f,roughness*roughness);
      for(u32 i=0;i<samples;++i){
        if((i&31u)==0&&cancel.cancelled())return false;
        const float xiX=static_cast<float>(i)/samples,xiY=radicalInverse(i);
        const float cosine=std::sqrt((1-xiY)/(1+(alpha*alpha-1)*xiY));
        const float sine=std::sqrt(std::max(0.0f,1-cosine*cosine)),azimuth=2*Pi*xiX;
        Vec3 half=add(add(mul(tangent,std::cos(azimuth)*sine),mul(bitangent,std::sin(azimuth)*sine)),mul(n,cosine));
        const Vec3 light=add(mul(half,2*dot(n,half)),mul(n,-1));const float noL=std::max(dot(n,light),0.0f);
        total=add(total,mul(sample(source,light),noL));weight+=noL;
      }if(!appendHalf(out,mul(total,1/std::max(weight,1e-8f))))return false;
    }}
  }
  return true;
}

bool bakeBrdf(const EnvironmentMapImportSettings &settings,EnvironmentMapCancel cancel,
              renderer::Rgba16fMipChain &out){
  out.width=out.height=settings.brdfSize;out.levels=1;
  out.texels.reserve(static_cast<usize>(settings.brdfSize)*settings.brdfSize*4);
  for(u32 y=0;y<settings.brdfSize;++y){if(cancel.cancelled())return false;const float rough=(y+.5f)/settings.brdfSize;
    const float alpha=rough*rough,alpha2=alpha*alpha;
    for(u32 x=0;x<settings.brdfSize;++x){const float noV=(x+.5f)/settings.brdfSize;
      const Vec3 view{std::sqrt(std::max(0.0f,1-noV*noV)),0,noV};float scale=0,bias=0;
      for(u32 i=0;i<settings.brdfSamples;++i){
        if((i&31u)==0&&cancel.cancelled())return false;
        const float xiX=static_cast<float>(i)/settings.brdfSamples,xiY=radicalInverse(i);
        const float cosine=std::sqrt((1-xiY)/(1+(alpha2-1)*xiY));const float sine=std::sqrt(std::max(0.0f,1-cosine*cosine));
        const Vec3 half{std::cos(2*Pi*xiX)*sine,std::sin(2*Pi*xiX)*sine,cosine};const float voH=std::max(dot(view,half),0.0f);
        const Vec3 light=add(mul(half,2*voH),mul(view,-1));const float noL=std::max(light[2],0.0f);
        const float visibility=.5f/std::max(noL*std::sqrt(noV*noV*(1-alpha2)+alpha2)+
            noV*std::sqrt(noL*noL*(1-alpha2)+alpha2),1e-6f);
        const float weight=4*visibility*noL*voH/std::max(cosine,1e-6f),fresnel=std::pow(1-voH,5);
        scale+=(1-fresnel)*weight;bias+=fresnel*weight;
      }if(!appendHalf(out,{scale/settings.brdfSamples,bias/settings.brdfSamples,0}))return false;
    }
  }
  return true;
}

struct Writer{std::vector<u8>&out;void raw(const void*p,usize n){auto*b=static_cast<const u8*>(p);out.insert(out.end(),b,b+n);}
  void u32v(u32 v){for(u32 i=0;i<4;++i)out.push_back(static_cast<u8>(v>>(8*i)));}void u64v(u64 v){for(u32 i=0;i<8;++i)out.push_back(static_cast<u8>(v>>(8*i)));}
  void u16v(u16 v){out.push_back(static_cast<u8>(v));out.push_back(static_cast<u8>(v>>8u));}
  void text(std::string_view s){u32v(static_cast<u32>(s.size()));raw(s.data(),s.size());}};
struct Reader{std::span<const u8>in;usize at=0;bool ok=true;u32 u32v(){if(!ok||in.size()-at<4){ok=false;return 0;}u32 v=0;for(u32 i=0;i<4;++i)v|=u32(in[at++])<<(8*i);return v;}
  u64 u64v(){if(!ok||in.size()-at<8){ok=false;return 0;}u64 v=0;for(u32 i=0;i<8;++i)v|=u64(in[at++])<<(8*i);return v;}
  u16 u16v(){if(!ok||in.size()-at<2){ok=false;return 0;}const u16 v=static_cast<u16>(in[at])|static_cast<u16>(in[at+1])<<8u;at+=2;return v;}
  std::string text(){u32 n=u32v();if(!ok||n>in.size()-at){ok=false;return{};}std::string s(reinterpret_cast<const char*>(in.data()+at),n);at+=n;return s;}};
void writeChain(Writer&w,const renderer::Rgba16fMipChain&c){w.u32v(c.width);w.u32v(c.height);w.u32v(c.levels);w.u64v(c.texels.size());for(u16 v:c.texels)w.u16v(v);}
bool readChain(Reader&r,u64 budget,renderer::Rgba16fMipChain&c){c.width=r.u32v();c.height=r.u32v();c.levels=r.u32v();const u64 count=r.u64v();
  if(!r.ok||count>budget/2||count>(r.in.size()-r.at)/2)return false;
  c.texels.resize(static_cast<usize>(count));
  for(u16&v:c.texels){v=r.u16v();}
  return r.ok&&c.valid();}
} // namespace

bool EnvironmentMapImportSettings::valid() const noexcept{return panoramaWidth>=64&&panoramaWidth<=4096&&powerOfTwo(panoramaWidth)&&
  specularSize>=16&&specularSize<=1024&&powerOfTwo(specularSize)&&brdfSize>=16&&brdfSize<=512&&powerOfTwo(brdfSize)&&
  specularSamples>=16&&specularSamples<=1024&&brdfSamples>=32&&brdfSamples<=2048;}
bool EnvironmentMapImportLimits::valid() const noexcept{return maximumSourceBytes&&maximumSourceDimension&&maximumSourcePixels&&maximumDecodedBytes&&maximumWorkingBytes&&maximumOutputBytes;}

std::string environmentMapCacheKey(std::string_view sourceHash,const EnvironmentMapImportSettings&s,const EnvironmentMapImportLimits&l){
  const std::string key="environment-map|schema="+std::to_string(EnvironmentMapCacheSchema)+"|importer="+
    std::to_string(EnvironmentMapImporterRevision)+"|source="+std::string(sourceHash)+"|panorama="+std::to_string(s.panoramaWidth)+
    "|specular="+std::to_string(s.specularSize)+"|brdf="+std::to_string(s.brdfSize)+"|ss="+std::to_string(s.specularSamples)+
    "|bs="+std::to_string(s.brdfSamples)+"|sourceBytes="+std::to_string(l.maximumSourceBytes)+"|dimension="+
    std::to_string(l.maximumSourceDimension)+"|pixels="+std::to_string(l.maximumSourcePixels)+"|decoded="+
    std::to_string(l.maximumDecodedBytes)+"|working="+std::to_string(l.maximumWorkingBytes)+"|output="+std::to_string(l.maximumOutputBytes);
  return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(key.data()),key.size()));
}

std::string environmentMapCacheRelativePath(std::string_view key) {
  return ".astra/cache/environments/"+std::string(key)+".aemc";
}

std::string writeEnvironmentMapImportSettings(const EnvironmentMapImportSettings&s) {
  return "AEM_IMPORT 1 "+std::to_string(s.panoramaWidth)+' '+std::to_string(s.specularSize)+' '+
      std::to_string(s.brdfSize)+' '+std::to_string(s.specularSamples)+' '+std::to_string(s.brdfSamples);
}

bool readEnvironmentMapImportSettings(std::string_view text,EnvironmentMapImportSettings&settings) {
  std::istringstream input{std::string(text)};std::string magic;u32 version=0;
  EnvironmentMapImportSettings candidate;
  if(!(input>>magic>>version>>candidate.panoramaWidth>>candidate.specularSize>>candidate.brdfSize>>
       candidate.specularSamples>>candidate.brdfSamples)||magic!="AEM_IMPORT"||version!=1||!candidate.valid())return false;
  input>>std::ws;
  if(!input.eof())return false;
  settings=candidate;return true;
}

bool importRadianceEnvironmentMap(std::span<const u8> source,const EnvironmentMapImportSettings&s,
 const EnvironmentMapImportLimits&l,EnvironmentMapCancel cancel,renderer::SharedEnvironmentMap&out,std::string&diagnostic){
  out.reset();diagnostic.clear();if(!s.valid()||!l.valid()){diagnostic="Configuração de importação HDRI inválida.";return false;}
  renderer::Rgba16fMipChain panoramaPlan{s.panoramaWidth,s.panoramaWidth/2,1,{}};
  renderer::Rgba16fMipChain specularPlan{s.specularSize,s.specularSize,1,{}};
  for(u32 size=s.panoramaWidth;size>1;size/=2)++panoramaPlan.levels;
  for(u32 size=s.specularSize;size>1;size/=2)++specularPlan.levels;
  const u64 plannedBytes=(panoramaPlan.expectedHalfCount()+specularPlan.expectedHalfCount()+
                          static_cast<u64>(s.brdfSize)*s.brdfSize*4u)*2u;
  if(!plannedBytes||plannedBytes>l.maximumOutputBytes){
    diagnostic="A receita HDRI excede o orçamento de saída antes da decodificação.";return false;
  }
  if(cancel.cancelled()){diagnostic="Importação HDRI cancelada.";return false;}
  HdrImageDecodeLimits decode{l.maximumSourceDimension,l.maximumSourcePixels,l.maximumSourceBytes,l.maximumDecodedBytes};
  DecodedHdrImage image;if(!decodeRadianceHdrRgba32f(source,decode,image,diagnostic))return false;
  if(image.width!=image.height*2u){diagnostic="O ambiente Radiance HDR deve usar projeção equiretangular 2:1.";return false;}
  const u64 sourceBytes=static_cast<u64>(image.rgba.size())*sizeof(float);
  const u64 linearBytes=static_cast<u64>(s.panoramaWidth)*(s.panoramaWidth/2u)*3u*sizeof(float);
  if(sourceBytes+linearBytes+linearBytes/4u+plannedBytes>l.maximumWorkingBytes){
    diagnostic="A fonte e os derivados HDRI excedem o orçamento de memória de trabalho.";return false;
  }
  auto candidate=std::make_shared<renderer::EnvironmentMapResource>();FloatImage linear;
  if(!makePanorama(image,s.panoramaWidth,cancel,linear,candidate->panorama)){diagnostic=cancel.cancelled()?"Importação HDRI cancelada.":"Radiância excede RGBA16F.";return false;}
  if(!projectSh(linear,candidate->description.diffuseIrradianceSh,cancel)||
     !bakeSpecular(linear,s,cancel,candidate->specular)||!bakeBrdf(s,cancel,candidate->brdf)){
    diagnostic=cancel.cancelled()?"Importação HDRI cancelada.":"Falha numérica ao derivar iluminação HDRI.";return false;}
  const u64 outputBytes=(candidate->panorama.texels.size()+candidate->specular.texels.size()+candidate->brdf.texels.size())*2ull;
  if(outputBytes>l.maximumOutputBytes){diagnostic="Derivados HDRI excedem o orçamento de saída.";return false;}
  auto&d=candidate->description;d.specularProjection=renderer::EnvironmentProjection::Octahedral;
  d.specularWidth=candidate->specular.width;d.specularHeight=candidate->specular.height;d.specularMipLevels=candidate->specular.levels;
  d.brdfWidth=candidate->brdf.width;d.brdfHeight=candidate->brdf.height;d.brdfMipLevels=candidate->brdf.levels;
  d.flags=renderer::EnvironmentMapPrefilteredGgx|renderer::EnvironmentMapSplitSumBrdf|renderer::EnvironmentMapDiffuseIrradianceSh9;
  candidate->sourceHash=Sha256::hex(source);candidate->cacheKey=environmentMapCacheKey(candidate->sourceHash,s,l);
  if(!candidate->valid()){diagnostic="Derivados HDRI ficaram inconsistentes.";return false;}
  out=std::move(candidate);return true;
}

bool writeEnvironmentMapCache(const renderer::EnvironmentMapResource&r,std::vector<u8>&out){out.clear();if(!r.valid())return false;Writer w{out};
  w.u32v(CacheMagic);w.u32v(EnvironmentMapCacheSchema);w.text(r.sourceHash);w.text(r.cacheKey);w.u32v(static_cast<u32>(r.description.specularProjection));
  w.u32v(r.description.flags);
  for(const auto&coefficient:r.description.diffuseIrradianceSh)
    for(float value:coefficient)w.u32v(std::bit_cast<u32>(value));
  writeChain(w,r.panorama);writeChain(w,r.specular);writeChain(w,r.brdf);
  const std::string checksum=Sha256::hex(out);w.raw(checksum.data(),checksum.size());return true;}

bool readEnvironmentMapCache(std::span<const u8>bytes,std::string_view expectedKey,const EnvironmentMapImportLimits&limits,
 renderer::SharedEnvironmentMap&out){out.reset();if(!limits.valid()||bytes.size()<64||bytes.size()>limits.maximumOutputBytes+4096)return false;
  const auto body=bytes.first(bytes.size()-64);const std::string_view stored(reinterpret_cast<const char*>(bytes.data()+body.size()),64);
  if(Sha256::hex(body)!=stored)return false;
  Reader r{body};
  if(r.u32v()!=CacheMagic||r.u32v()!=EnvironmentMapCacheSchema)return false;
  auto candidate=std::make_shared<renderer::EnvironmentMapResource>();
  candidate->sourceHash=r.text();candidate->cacheKey=r.text();if(!r.ok||candidate->cacheKey!=expectedKey)return false;
  candidate->description.specularProjection=static_cast<renderer::EnvironmentProjection>(r.u32v());candidate->description.flags=r.u32v();
  for(auto&coefficient:candidate->description.diffuseIrradianceSh)for(float&value:coefficient)value=std::bit_cast<float>(r.u32v());
  if(!readChain(r,limits.maximumOutputBytes,candidate->panorama)||!readChain(r,limits.maximumOutputBytes,candidate->specular)||
     !readChain(r,limits.maximumOutputBytes,candidate->brdf)||r.at!=body.size())return false;
  auto&d=candidate->description;d.specularWidth=candidate->specular.width;d.specularHeight=candidate->specular.height;d.specularMipLevels=candidate->specular.levels;
  d.brdfWidth=candidate->brdf.width;d.brdfHeight=candidate->brdf.height;d.brdfMipLevels=candidate->brdf.levels;
  const u64 payload=(candidate->panorama.texels.size()+candidate->specular.texels.size()+candidate->brdf.texels.size())*2ull;
  if(payload>limits.maximumOutputBytes||!candidate->valid())return false;
  out=std::move(candidate);return true;}
} // namespace ae::resources
