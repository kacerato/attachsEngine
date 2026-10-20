#include "resources/terrain_fx_field.h"
#include "core/sha256.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace ae::resources {
namespace {
constexpr double Pi=3.14159265358979323846;
TerrainFxDirtyRect strokeBounds(const TerrainFxAsset &a,const TerrainFxStroke &s){
  const double angle=s.rotationDegrees*Pi/180,c=std::abs(std::cos(angle)),sn=std::abs(std::sin(angle));
  const double rx=s.radiusMeters*s.aspect,rz=s.radiusMeters,ex=c*rx+sn*rz,ez=sn*rx+c*rz;
  const auto index=[](double value,u32 size){return u32(std::clamp(value,0.0,double(size)));};
  return {index(std::ceil((s.x-ex-a.domain.originX)/a.domain.sizeX*a.width-.5),a.width),
    index(std::ceil((s.z-ez-a.domain.originZ)/a.domain.sizeZ*a.height-.5),a.height),
    index(std::floor((s.x+ex-a.domain.originX)/a.domain.sizeX*a.width-.5)+1,a.width),
    index(std::floor((s.z+ez-a.domain.originZ)/a.domain.sizeZ*a.height-.5)+1,a.height)};
}
bool validField(const TerrainFxField &f,const TerrainFxLimits &limits){
  return std::isfinite(f.domain.originX)&&std::isfinite(f.domain.originZ)&&std::isfinite(f.domain.sizeX)&&std::isfinite(f.domain.sizeZ)&&
    std::abs(f.domain.originX)<=1e8&&std::abs(f.domain.originZ)<=1e8&&f.domain.sizeX>=.000001&&f.domain.sizeZ>=.000001&&f.domain.sizeX<=1e8&&f.domain.sizeZ<=1e8&&
    f.width&&f.height&&f.width<=limits.maximumDimension&&f.height<=limits.maximumDimension&&
    u64(f.width)*f.height<=limits.maximumPixels&&u64(f.width)*f.height<=limits.maximumWorkingBytes/20&&f.values.size()==u64(f.width)*f.height;
}
u8 byte(float value){return u8(std::lround(std::clamp(double(value),0.0,1.0)*255));}
}
bool rasterTerrainFx(const TerrainFxAsset &asset,std::span<const float> slope,const TerrainFxField *previous,
    TerrainFxField &out,TerrainFxDirtyRect &dirty,std::string &error,const TerrainFxLimits &limits,const std::function<bool(double)> &progress){
  if(!validTerrainFxAsset(asset,error,limits))return false;
  const usize pixels=usize(asset.width)*asset.height;
  const bool requiresSlope=std::any_of(asset.strokes.begin()+asset.bakedStrokeCount,asset.strokes.end(),[](const auto &s){return s.slopeFilter;});
  if(requiresSlope&&slope.size()!=pixels){error="Filtered FX strokes require the matching normalized slope field";return false;}
  if(!slope.empty()){
    if(slope.size()!=pixels){error="FX slope field dimensions do not match";return false;}
    for(float value:slope)if(!std::isfinite(value)||value<0||value>1){error="FX slope input must be normalized and finite";return false;}
  }
  u64 visits=pixels;
  for(usize index=asset.bakedStrokeCount;index<asset.strokes.size();++index){const auto &s=asset.strokes[index];const auto rect=strokeBounds(asset,s);if(rect.empty())continue;const u64 count=u64(rect.x1-rect.x0)*(rect.z1-rect.z0);
    if(visits>limits.maximumRasterSamples||count>limits.maximumRasterSamples-visits){error="FX raster work exceeds budget; no strokes were truncated";return false;}visits+=count;}
  if(visits>limits.maximumRasterSamples){error="FX grid initialization exceeds raster budget";return false;}
  if(progress&&!progress(0)){error="FX raster cancelled";return false;}
  TerrainFxField next;next.width=asset.width;next.height=asset.height;next.domain=asset.domain;next.values.resize(pixels);
  for(usize i=0;i<pixels;++i)for(usize channel=0;channel<4;++channel)next.values[i][channel]=asset.seedPixels.empty()?0.f:float(asset.seedPixels[i*4+channel])/255.f;
  u64 done=pixels;
  for(usize index=asset.bakedStrokeCount;index<asset.strokes.size();++index){const auto &s=asset.strokes[index];
    const auto rect=strokeBounds(asset,s);if(rect.empty())continue;
    const double angle=s.rotationDegrees*Pi/180,c=std::cos(angle),sn=std::sin(angle);
    for(u32 z=rect.z0;z<rect.z1;++z){
      if(progress&&!progress(double(done)/double(visits))){error="FX raster cancelled";return false;}
      const double worldZ=asset.domain.originZ+(double(z)+.5)/asset.height*asset.domain.sizeZ;
      for(u32 x=rect.x0;x<rect.x1;++x){
        const usize i=usize(z)*asset.width+x;
        if(s.slopeFilter&&(slope[i]<s.slopeMin||slope[i]>s.slopeMax))continue;
        const double worldX=asset.domain.originX+(double(x)+.5)/asset.width*asset.domain.sizeX;
        const double dx=worldX-s.x,dz=worldZ-s.z,u=(c*dx+sn*dz)/(s.radiusMeters*s.aspect),v=(-sn*dx+c*dz)/s.radiusMeters;
        const double distance=s.shape==TerrainFxBrushShape::Circle?std::sqrt(u*u+v*v):std::max(std::abs(u),std::abs(v));
        if(distance>1)continue;
        const double falloff=s.hardness>=1?1.0:distance<=s.hardness?1.0:std::pow(std::clamp((1.0-distance)/(1.0-s.hardness),0.0,1.0),s.falloff);
        const double amount=std::clamp(falloff*s.sampleSeconds*s.flow*s.pressure*s.opacity,0.0,1.0);
        auto &value=next.values[i][u32(s.channel)];value=float(std::clamp(double(value)+(s.target-double(value))*amount,0.0,1.0));
      }
      done+=rect.x1-rect.x0;
    }
  }
  TerrainFxDirtyRect changed;
  const bool compatible=previous&&previous->width==next.width&&previous->height==next.height&&previous->domain==next.domain&&previous->values.size()==next.values.size();
  if(!compatible)changed={0,0,next.width,next.height};
  else for(u32 z=0;z<next.height;++z)for(u32 x=0;x<next.width;++x){const usize i=usize(z)*next.width+x;
    if(next.values[i]==previous->values[i])continue;
    if(changed.empty())changed={x,z,x+1,z+1};else{changed.x0=std::min(changed.x0,x);changed.z0=std::min(changed.z0,z);changed.x1=std::max(changed.x1,x+1);changed.z1=std::max(changed.z1,z+1);}}
  if(progress&&!progress(1)){error="FX raster cancelled before publication";return false;}
  out=std::move(next);dirty=changed;error.clear();return true;
}
bool quantizeTerrainFx(const TerrainFxField &field,std::vector<u8> &out,std::string &error,const TerrainFxLimits &limits){
  if(!validField(field,limits)){error="Invalid FX field or byte snapshot budget";return false;}
  for(const auto &sample:field.values)for(float value:sample)if(!std::isfinite(value)||value<0||value>1){error="Invalid FX channel value; channels must be independent [0,1]";return false;}
  std::vector<u8> bytes(field.values.size()*4);
  for(usize i=0;i<field.values.size();++i)for(usize c=0;c<4;++c)bytes[i*4+c]=byte(field.values[i][c]);
  out=std::move(bytes);error.clear();return true;
}
bool sampleTerrainFxBytes(std::span<const u8> bytes,u32 width,u32 height,double u,double v,std::array<float,4> &out){
  if(!width||!height||u64(width)*height>std::numeric_limits<usize>::max()/4||bytes.size()!=u64(width)*height*4||!std::isfinite(u)||!std::isfinite(v))return false;
  // Clamp UV before multiplying so arbitrarily large finite coordinates do
  // not overflow the integer conversion. Edge texels extend to infinity.
  const double x=std::clamp(u,0.0,1.0)*width-.5,z=std::clamp(v,0.0,1.0)*height-.5;
  const double ix=std::floor(x),iz=std::floor(z),tx=x-ix,tz=z-iz;
  const auto cx=[&](double index){return u32(std::clamp(index,0.0,double(width-1)));};
  const auto cz=[&](double index){return u32(std::clamp(index,0.0,double(height-1)));};
  const u32 x0=cx(ix),x1=cx(ix+1),z0=cz(iz),z1=cz(iz+1);std::array<float,4> next;
  for(usize c=0;c<4;++c){const auto at=[&](u32 a,u32 b){return double(bytes[(usize(b)*width+a)*4+c])/255;};
    next[c]=float((at(x0,z0)*(1-tx)+at(x1,z0)*tx)*(1-tz)+(at(x0,z1)*(1-tx)+at(x1,z1)*tx)*tz);}
  out=next;return true;
}
bool checkpointTerrainFx(const TerrainFxAsset &asset,const TerrainFxField &field,TerrainFxAsset &out,std::string &error,const TerrainFxLimits &limits){
  if(!validTerrainFxAsset(asset,error,limits))return false;
  if(field.width!=asset.width||field.height!=asset.height||field.domain!=asset.domain){error="FX checkpoint does not match the authored grid/domain";return false;}
  std::vector<u8> bytes;if(!quantizeTerrainFx(field,bytes,error,limits))return false;
  auto next=asset;next.seedPixels=std::move(bytes);next.bakedStrokeCount=u32(next.strokes.size());
  next.checkpointBaseHash=asset.checkpointBaseHash.empty()?Sha256::hex(asset.seedPixels):asset.checkpointBaseHash;
  next.checkpointPrefixHash=terrainFxPrefixHash(next,next.bakedStrokeCount,next.checkpointBaseHash);
  next.checkpointPixelsHash=Sha256::hex(next.seedPixels);
  out=std::move(next);error.clear();return true;
}
TerrainFxDirtyRect terrainFxMipDirtyRect(TerrainFxDirtyRect rect,u32 width,u32 height,u32 mip){
  if(!width||!height||rect.empty()||rect.x1>width||rect.z1>height)return {};
  // Box mip ancestry; dirty child pixels update every parent they contribute
  // to. Callers expand the base rect first if their filter has a wider halo.
  while(mip--&&(width>1||height>1)){rect.x0/=2;rect.z0/=2;rect.x1=(rect.x1+1)/2;rect.z1=(rect.z1+1)/2;width=std::max(1u,width/2);height=std::max(1u,height/2);
    rect.x0=std::min(rect.x0,width);rect.z0=std::min(rect.z0,height);rect.x1=std::min(rect.x1,width);rect.z1=std::min(rect.z1,height);}
  return rect;
}
}
