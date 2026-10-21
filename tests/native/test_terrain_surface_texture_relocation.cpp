#include "harness.h"
#include "renderer/terrain_surface_texture_relocation.h"
#include "renderer/terrain_surface_table.h"
using namespace ae;
namespace {
renderer::TerrainSurfaceBinding localBinding(){
 renderer::TerrainSurfaceBinding b;resources::TerrainSurfaceLayer layer;layer.identity={99,1};b.surface.layers={layer};b.surface.flow.wetness=true;
 b.textures.push_back({{99,10},true,false,0});b.textures.push_back({{99,11},true,false,1});
 auto &fx=b.staticFx;fx.enabled=true;fx.textureIndex=2;fx.width=fx.height=1;fx.domain=b.terrainDomain;fx.identity={99,2};fx.revision=1;fx.rgbaSha256=std::string(64,'a');return b;
}
}
AE_TEST(terrain_static_fx_validation_offset_zero_preserves_local_indices){
 auto b=localBinding();std::string error;AE_EXPECT_TRUE(renderer::offsetTerrainSurfaceSourceTextureIndices(b,0,3,error),error.c_str());
 AE_EXPECT_EQ(b.textures[0].textureIndex,0u,"validation source0");AE_EXPECT_EQ(b.textures[1].textureIndex,1u,"validation source1");AE_EXPECT_EQ(b.staticFx.textureIndex,2u,"validation StaticFX local2");
}
AE_TEST(terrain_static_fx_package_offset_selects_same_descriptor_as_local_validation){
 auto local=localBinding();std::string error;u32 offset=0;std::vector<float> validation,global,oldBug;
 const std::array<u32,3> localSlots{11,12,77};const std::array<u32,7> packageSlots{201,202,203,204,11,12,77};
 AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(local,localSlots,validation,offset,error),error.c_str());
 auto moved=local;AE_EXPECT_TRUE(renderer::offsetTerrainSurfaceSourceTextureIndices(moved,4,3,error),error.c_str());
 AE_EXPECT_EQ(moved.textures[0].textureIndex,4u,"PBR first source translated");AE_EXPECT_EQ(moved.textures[1].textureIndex,5u,"PBR second source translated");AE_EXPECT_EQ(moved.staticFx.textureIndex,6u,"StaticFX translated into same complete package");
 AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(moved,packageSlots,global,offset,error),error.c_str());AE_EXPECT_EQ(global,validation,"validation and final table consume same actual StaticFX descriptor77");
 auto old=local;for(auto &t:old.textures)t.textureIndex+=4;
 AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(old,packageSlots,oldBug,offset,error),error.c_str());AE_EXPECT_TRUE(oldBug!=validation,"repro: old refs-only relocation selects built-in203 instead of StaticFX77");
}
AE_TEST(terrain_static_fx_invalid_source_or_overflow_rejects_atomically){
 auto b=localBinding();b.staticFx.textureIndex=3;std::string error;AE_EXPECT_TRUE(!renderer::offsetTerrainSurfaceSourceTextureIndices(b,4,3,error),"StaticFX outside source library rejected before mutation");AE_EXPECT_EQ(b.textures[0].textureIndex,0u,"no partial PBR relocation");AE_EXPECT_EQ(b.staticFx.textureIndex,3u,"invalid index unchanged");
 b=localBinding();AE_EXPECT_TRUE(!renderer::offsetTerrainSurfaceSourceTextureIndices(b,renderer::InvalidMapTexture-1,3,error),"overflow into invalid sentinel rejected");AE_EXPECT_EQ(b.textures[1].textureIndex,1u,"overflow atomic");AE_EXPECT_EQ(b.staticFx.textureIndex,2u,"StaticFX unchanged on overflow");
}
AE_TEST(terrain_disabled_static_fx_never_turns_placeholder_into_a_texture){
 renderer::TerrainSurfaceBinding b;std::string error;AE_EXPECT_TRUE(renderer::offsetTerrainSurfaceSourceTextureIndices(b,4,3,error),error.c_str());AE_EXPECT_EQ(b.staticFx.textureIndex,renderer::InvalidMapTexture,"disabled sentinel stays absent");
}
