#include "resources/terrain_surface_import.h"
#include "resources/terrain_surface_texture_refs.h"
#include "resources/terrain_png_integrity.h"
#include "resources/image_decode.h"
#include "resources/shared_decoded_texture.h"
#include "resources/terrain_normal_mips.h"
#include <algorithm>
#include <cmath>
#include <map>
namespace ae::resources {
bool importTerrainSurface(const TerrainSurfaceAsset &surface,std::span<const TerrainSurfaceImage> images,float meters,
    u64 budget,std::vector<renderer::SharedAuthoringTexture> &out,renderer::TerrainSurfaceBinding &binding,std::string &error,
    std::array<float,4> domain) {
  if(!validTerrainSurface(surface,error))return false;
  if(!std::isfinite(meters) || meters<=0 || meters>256){error="Invalid terrain surface meter scale.";return false;}
  for(float value:domain)if(!std::isfinite(value)||std::abs(value)>1e8f){error="Invalid terrain domain.";return false;}
  if(domain[2]<=0||domain[3]<=0){error="Terrain domain extent must be positive.";return false;}
  std::map<AssetGuid,const TerrainSurfaceImage *> sources;
  for(const auto &image:images)if(!image.identity.valid() || !sources.emplace(image.identity,&image).second) {
    error="Invalid/duplicate terrain surface image identity.";return false;
  }
  std::vector<renderer::SharedAuthoringTexture> textures;renderer::TerrainSurfaceBinding next;next.surface=surface;next.localMetersPerUv=meters;next.terrainDomain=domain;
  u64 used=0;
  for(const auto &ref:terrainSurfaceTextureRefs(surface)) {
    const auto found=sources.find(ref.identity);
    if(found==sources.end() || !validTerrainPng(found->second->bytes())) {
      error="Terrain surface requires original PNG for every texture reference.";return false;
    }
    u32 width=0,height=0;ImageDecodeLimits limits;
    if(!readImageDimensions(found->second->bytes(),limits,width,height)){error="Invalid terrain surface PNG dimensions.";return false;}
    u64 required=0;for(u32 w=width,h=height;;w=std::max(1u,w/2),h=std::max(1u,h/2)) {
      required+=u64(w)*h*4;if(w==1 && h==1)break;
    }
    if(used>budget || required>budget-used){error="Full terrain surface texture mip chains exceed budget.";return false;}
    DecodedImage decoded;if(!decodeImageRgba8(found->second->bytes(),limits,decoded,error))return false;
    const auto original=ref.normalRG?decoded.rgba:std::vector<u8>{};
    if(ref.normalRG)for(usize i=0;i<decoded.rgba.size();i+=4) {
      const float x=decoded.rgba[i]/127.5f-1,y=decoded.rgba[i+1]/127.5f-1;
      decoded.rgba[i+2]=u8(std::lround((std::sqrt(std::max(0.f,1-x*x-y*y))*.5f+.5f)*255));
    }
    renderer::AuthoringTexture made;made.width=width;made.height=height;made.srgb=!ref.linear;
    made.samplerFlags=renderer::encodeGltfTextureSampler()|(ref.clampSampling?0u:renderer::AuthoringTextureRepeatU|renderer::AuthoringTextureRepeatV);
    const bool built=ref.normalChannel==7?buildTerrainGaNormalMipChain(decoded,made.mipChain,made.levels):ref.normal?buildNormalMipChain(decoded,made.mipChain,made.levels):buildMipChain(decoded,made.srgb,made.mipChain,made.levels);
    if(!built){error="Terrain surface mip generation failed.";return false;}
    if(ref.normalRG)std::copy(original.begin(),original.end(),made.mipChain.begin());
    TerrainTextureResidency choice{ref.residencyCap?TerrainTextureResidencyMode::Cap:TerrainTextureResidencyMode::Original,ref.residencyCap};
    if(!terrainTextureFirstAllowedMip(width,height,made.levels,choice,{},made.minimumResidentMip)){error="Invalid resolved terrain texture ceiling.";return false;}
    used+=made.mipChain.size();next.textures.push_back({ref.identity,ref.linear,ref.normal,u32(textures.size()),ref.normalChannel,ref.residencyCap,ref.clampSampling});
    textures.push_back(shareDecodedTexture(std::move(made)));
  }
  out=std::move(textures);binding=std::move(next);error.clear();return true;
}
}
