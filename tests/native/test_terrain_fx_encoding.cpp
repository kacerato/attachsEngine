#include "harness.h"
#include "resources/terrain_fx_field.h"
#include "core/sha256.h"
using namespace ae::resources;
AE_TEST(terrain_fx_seed_codec_preserves_uniform_painted_and_noise_pixels_exactly){
  for(ae::u32 pattern=0;pattern<3;++pattern){TerrainFxAsset a;a.width=a.height=32;a.seedPixels.assign(32*32*4,77);
    if(pattern==1)for(ae::usize i=20*4;i<30*4;++i)a.seedPixels[i]=ae::u8(i&255);
    if(pattern==2)for(ae::usize i=0;i<a.seedPixels.size();++i)a.seedPixels[i]=ae::u8((i*37+11)&255);
    const auto hash=ae::Sha256::hex(a.seedPixels);std::string text,error;AE_EXPECT_TRUE(serializeTerrainFxAsset(a,text,error),error.c_str());
    if(pattern<2)AE_EXPECT_TRUE(text.size()<1024,"uniform/painted map uses deterministic lossless pixel runs");
    else AE_EXPECT_TRUE(text.size()>8192,"noise falls back to raw rather than expanding RLE");
    TerrainFxAsset cold;AE_EXPECT_TRUE(deserializeTerrainFxAsset(text,cold,error),error.c_str());AE_EXPECT_TRUE(cold.seedPixels==a.seedPixels,"decoded authoring bytes identical for every pattern");AE_EXPECT_EQ(ae::Sha256::hex(cold.seedPixels),hash,"content hash independent of storage encoding");
    std::string again;AE_EXPECT_TRUE(serializeTerrainFxAsset(cold,again,error),error.c_str());AE_EXPECT_EQ(again,text,"codec choice/run segmentation deterministic");
  }
}
AE_TEST(terrain_fx_seed_codec_rejects_unknown_zero_overrun_and_truncated_runs_atomically){
  TerrainFxAsset a;a.width=a.height=5;a.seedPixels.assign(100,77);std::string text,error;
  AE_EXPECT_TRUE(serializeTerrainFxAsset(a,text,error),error.c_str());const std::string header=" 1 100 8 ";auto p=text.find(header);AE_EXPECT_TRUE(p!=std::string::npos,"encoding1 explicitly declares decoded100 and encoded8 bytes");
  std::vector<std::string> invalid;
  auto unknown=text;unknown.replace(p,header.size()," 2 100 8 ");invalid.push_back(unknown);
  auto zero=text;zero.replace(p+header.size(),8,"00000000");invalid.push_back(zero);
  auto overflow=text;overflow.replace(p+header.size(),8,"1a000000");invalid.push_back(overflow);
  invalid.push_back(text.substr(0,p+header.size()+15));
  auto oversized=text;oversized.replace(p,header.size()," 1 18446744073709551615 8 ");invalid.push_back(oversized);
  TerrainFxAsset output=a;output.width=77;
  for(const auto &bad:invalid){AE_EXPECT_TRUE(!deserializeTerrainFxAsset(bad,output,error),"bad codec/count/expansion/truncation rejected within quotas");AE_EXPECT_EQ(output.width,77u,"failed codec load cannot mutate current asset");}
}
