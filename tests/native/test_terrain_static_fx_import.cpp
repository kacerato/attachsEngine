#include "harness.h"
#include "resources/terrain_static_fx_import.h"
#include <cmath>
using namespace ae;
using namespace ae::resources;
namespace {
TerrainFxField sampleField(){TerrainFxField f;f.width=f.height=5;f.values.resize(25,{.75f,.625f,.5f,.875f});return f;}
}
AE_TEST(terrain_static_fx_snapshot_keeps_independent_original_channels_and_identity){
  auto field=sampleField();renderer::SharedAuthoringTexture texture;renderer::TerrainStaticFxBinding binding;std::string error;
  AE_EXPECT_TRUE(makeTerrainStaticFxTexture(field,8,texture,binding,error),error.c_str());
  AE_EXPECT_TRUE(texture->levels==1&&texture->mipChain.size()==100&&!texture->srgb,"mip zero is immutable and cannot be replaced by a streamed coarse mip");
  AE_EXPECT_TRUE(texture->mipChain[0]==191&&texture->mipChain[1]==159&&texture->mipChain[2]==128&&texture->mipChain[3]==223,"RGBA quantities never normalize against each other");
  std::array<float,4> sampled{};AE_EXPECT_TRUE(sampleTerrainFxBytes(texture->mipChain,5,5,-1,2,sampled),"CPU samples actual published linear Clamp bytes");
  AE_EXPECT_TRUE(std::abs(sampled[0]-191.f/255.f)<1e-6f,"outside domain clamps exactly");
  renderer::SharedAuthoringTexture other;renderer::TerrainStaticFxBinding same;
  AE_EXPECT_TRUE(makeTerrainStaticFxTexture(field,14,other,same,error),error.c_str());
  AE_EXPECT_TRUE(same.identity==binding.identity&&same.revision==binding.revision,"source-local texture relocation cannot change content identity");
  field.values[0][0]=0;
  AE_EXPECT_TRUE(makeTerrainStaticFxTexture(field,14,other,same,error),error.c_str());
  AE_EXPECT_TRUE(same.identity!=binding.identity&&same.rgbaSha256!=binding.rgbaSha256,"actual pixel edit produces a new immutable snapshot");
}
AE_TEST(terrain_static_fx_snapshot_rejects_mutated_pixels_sampler_domain_and_metadata){
  auto field=sampleField();renderer::SharedAuthoringTexture texture;renderer::TerrainStaticFxBinding binding;std::string error;
  AE_EXPECT_TRUE(makeTerrainStaticFxTexture(field,8,texture,binding,error),error.c_str());
  auto changed=*texture;changed.mipChain[0]^=1;
  AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(binding,changed,binding.domain,error),"pixel corruption cannot publish");
  changed=*texture;changed.srgb=true;AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(binding,changed,binding.domain,error),"sRGB data cannot publish");
  changed=*texture;changed.samplerFlags|=renderer::AuthoringTextureRepeatU;
  AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(binding,changed,binding.domain,error),"repeating mask cannot publish");
  changed=*texture;changed.samplerFlags=renderer::encodeGltfTextureSampler();
  AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(binding,changed,binding.domain,error),"mip filtered data cannot claim original mip sampling");
  auto wrong=binding;wrong.revision++;
  AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(wrong,*texture,binding.domain,error),"revision is bound to pixels and domain");
  auto domain=binding.domain;domain[2]*=2;
  AE_EXPECT_TRUE(!validateTerrainStaticFxTexture(binding,*texture,domain,error),"size change requires explicit rebase");
}
AE_TEST(terrain_static_fx_snapshot_budget_failure_preserves_previous_publication){
  auto field=sampleField();renderer::SharedAuthoringTexture texture;renderer::TerrainStaticFxBinding binding;std::string error;
  AE_EXPECT_TRUE(makeTerrainStaticFxTexture(field,8,texture,binding,error),error.c_str());
  auto previous=texture;auto identity=binding.identity;TerrainFxLimits tiny;tiny.maximumWorkingBytes=499;
  AE_EXPECT_TRUE(!makeTerrainStaticFxTexture(field,9,texture,binding,error,tiny),"insufficient budget fails before publication");
  AE_EXPECT_TRUE(texture==previous&&binding.identity==identity&&binding.textureIndex==8,"no downsize, partial texture or new identity on failure");
}
