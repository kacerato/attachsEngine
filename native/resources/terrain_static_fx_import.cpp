#include "resources/terrain_static_fx_import.h"
#include "resources/shared_decoded_texture.h"
#include "core/sha256.h"
#include <algorithm>
#include <bit>
#include <cmath>
namespace ae::resources {
namespace {
std::string snapshotKey(const renderer::TerrainStaticFxBinding &binding) {
  // Canonical fixed little-endian metadata, independent of native padding.
  std::vector<u8> seed(binding.rgbaSha256.begin(),binding.rgbaSha256.end());
  const auto word=[&](u32 value){for(u32 n=0;n<4;++n)seed.push_back(u8(value>>(n*8)));};
  word(binding.width);word(binding.height);
  for(float value:binding.domain)word(std::bit_cast<u32>(value));
  return Sha256::hex(seed);
}
void assignIdentity(renderer::TerrainStaticFxBinding &binding) {
  const auto key=snapshotKey(binding);
  binding.identity=assetGuidFromSeed("terrain-static-fx-v1:"+key);
  binding.revision=0;
  for(usize n=0;n<16;++n){const char c=key[n];binding.revision=(binding.revision<<4)|u64(c<='9'?c-'0':c-'a'+10);}
  if(!binding.revision)binding.revision=1;
}
}
bool validateTerrainStaticFxTexture(const renderer::TerrainStaticFxBinding &binding,
    const renderer::AuthoringTexture &texture,std::array<float,4> domain,std::string &error) {
  const auto fail=[&](const char *message){error=message;return false;};
  if(!binding.enabled||binding.textureIndex==renderer::InvalidMapTexture||!binding.identity.valid()||!binding.revision||
     !binding.width||!binding.height||binding.width>2048||binding.height>2048||binding.domain!=domain||
     !binding.linear||!binding.clampSampling)return fail("Invalid static FX snapshot metadata.");
  for(float value:domain)if(!std::isfinite(value)||std::abs(value)>1e8f)return fail("Invalid static FX domain.");
  if(domain[2]<=0||domain[3]<=0)return fail("Invalid static FX extent.");
  const u32 wrap=renderer::AuthoringTextureRepeatU|renderer::AuthoringTextureRepeatV|
    renderer::AuthoringTextureMirrorU|renderer::AuthoringTextureMirrorV;
  if(!texture.valid()||texture.format!=renderer::AuthoringTextureRgba8||texture.srgb||
     texture.width!=binding.width||texture.height!=binding.height||texture.minimumResidentMip||texture.firstLevel||texture.levels!=1||
     (texture.samplerFlags&wrap))return fail("Static FX requires original linear Clamp RGBA8.");
  const auto sampler=renderer::decodeTextureSampler(texture.samplerFlags);
  if(!sampler.minLinear||!sampler.magLinear||sampler.mipEnabled)return fail("Static FX requires linear filtering at original mip zero.");
  const usize bytes=usize(texture.width)*texture.height*4;
  if(texture.mipChain.size()!=bytes||
     Sha256::hex(std::span<const u8>(texture.mipChain.data(),bytes))!=binding.rgbaSha256)
    return fail("Static FX pixel hash or original byte count mismatch.");
  auto expected=binding;assignIdentity(expected);
  if(binding.identity!=expected.identity||binding.revision!=expected.revision)return fail("Static FX identity mismatch.");
  error.clear();return true;
}
bool makeTerrainStaticFxTexture(const TerrainFxField &field,u32 index,
    renderer::SharedAuthoringTexture &out,renderer::TerrainStaticFxBinding &binding,std::string &error,
    const TerrainFxLimits &limits) {
  if(index==renderer::InvalidMapTexture){error="Missing static FX texture slot.";return false;}
  std::vector<u8> pixels;if(!quantizeTerrainFx(field,pixels,error,limits))return false;
  renderer::TerrainStaticFxBinding next;next.enabled=true;next.width=field.width;next.height=field.height;
  next.domain={float(field.domain.originX),float(field.domain.originZ),float(field.domain.sizeX),float(field.domain.sizeZ)};
  next.textureIndex=index;next.rgbaSha256=Sha256::hex(pixels);next.linear=true;next.clampSampling=true;assignIdentity(next);
  renderer::AuthoringTexture made;made.width=field.width;made.height=field.height;made.srgb=false;
  made.levels=1;made.mipChain=std::move(pixels);
  made.samplerFlags=renderer::encodeGltfTextureSampler(9729,9729)|renderer::AuthoringTextureNoAnisotropy;
  if(!validateTerrainStaticFxTexture(next,made,next.domain,error))return false;
  out=shareDecodedTexture(std::move(made));binding=std::move(next);error.clear();return true;
}
}
