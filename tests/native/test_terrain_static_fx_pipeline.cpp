#include "harness.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_static_fx_import.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/terrain_textures.generated.h"
#include "resources/terrain_surface_texture_refs.h"
#include <algorithm>
using namespace ae;
using namespace ae::resources;
namespace {
std::array<std::vector<u8>,4> originals(){return {{{std::begin(terrainTexture0),std::end(terrainTexture0)},
 {std::begin(terrainTexture1),std::end(terrainTexture1)},{std::begin(terrainTexture2),std::end(terrainTexture2)},
 {std::begin(terrainTexture3),std::end(terrainTexture3)}}};}
TerrainRecipe painted(){TerrainRecipe r;r.cells=8;r.controlCells=8;r.height=0;
 TerrainSurfaceLayer layer;layer.identity={19,1};layer.name="FX";r.surface.layers={layer};
 r.fx.enabled=true;r.fx.width=r.fx.height=5;r.fx.seedPixels.resize(100);
 for(usize n=0;n<100;n+=4){r.fx.seedPixels[n]=51;r.fx.seedPixels[n+1]=102;r.fx.seedPixels[n+2]=153;r.fx.seedPixels[n+3]=204;}
 r.surface.flow.wetness=r.surface.flow.puddles=true;return r;}
}
AE_TEST(terrain_static_fx_glb_import_and_cache_keep_one_original_snapshot){
 auto recipe=painted();TerrainBuild build;std::string error;std::vector<u8> glb;
 AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());
 AE_EXPECT_TRUE(writeTerrainGlb(build,originals(),{},glb,error),error.c_str());
 GltfImport model;GltfImportLimits limits;limits.maximumTextureDimension=1;
 AE_EXPECT_TRUE(importGlb(glb,limits,{},model),model.diagnostic.c_str());
 const auto &fx=model.terrainSurfaces[0].staticFx;
 AE_EXPECT_TRUE(fx.enabled&&fx.width==5&&fx.height==5,"generic import cap does not silently resize authored FX");
 AE_EXPECT_TRUE(model.textures[fx.textureIndex]->mipChain==recipe.fx.seedPixels,"every independent channel reaches the actual importer unchanged");
 for(usize m=0;m<4;++m)AE_EXPECT_TRUE(model.terrainSurfaces[m].staticFx.textureIndex==fx.textureIndex,"four material slots share one immutable snapshot");
 std::vector<u8> cache;AE_EXPECT_TRUE(writeImportCache(model,"fx-pipeline",cache),"cache writer accepts validated pixel snapshot");
 GltfImport cold;AE_EXPECT_TRUE(readImportCache(cache,"fx-pipeline",cold),"cold cache load validates metadata and pixel hash");
 const auto &restored=cold.terrainSurfaces[0].staticFx;
 AE_EXPECT_TRUE(restored.identity==fx.identity&&restored.revision==fx.revision&&restored.rgbaSha256==fx.rgbaSha256,"cache conserves derived identity");
 AE_EXPECT_TRUE(cold.textures[restored.textureIndex]->mipChain==recipe.fx.seedPixels,"cold cache keeps original bytes");
 TerrainRecipe authored;AE_EXPECT_TRUE(terrainRecipeFromGlb(glb,authored,error),error.c_str());
 AE_EXPECT_TRUE(authored.fx.seedPixels==recipe.fx.seedPixels,"one authoring owner survives GLB reedit");
}
AE_TEST(terrain_static_fx_corrupt_glb_hash_and_cache_metadata_are_rejected){
 auto recipe=painted();TerrainBuild build;std::string error;std::vector<u8> glb;
 AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());
 AE_EXPECT_TRUE(writeTerrainGlb(build,originals(),{},glb,error),error.c_str());
 GltfImport good;AE_EXPECT_TRUE(importGlb(glb,{},{},good),good.diagnostic.c_str());
 const auto hash=good.terrainSurfaces[0].staticFx.rgbaSha256;
 auto bad=glb;auto at=std::search(bad.begin(),bad.end(),hash.begin(),hash.end());
 AE_EXPECT_TRUE(at!=bad.end(),"real GLB carries exact pixel hash");*at=*at=='0'?'1':'0';
 GltfImport rejected;AE_EXPECT_TRUE(!importGlb(bad,{},{},rejected),"changed GLB pixel hash refuses entire import");
 std::vector<u8> cache;AE_EXPECT_TRUE(writeImportCache(good,"fx-corrupt",cache),"valid cache fixture");
 at=std::search(cache.begin(),cache.end(),hash.begin(),hash.end());AE_EXPECT_TRUE(at!=cache.end(),"cache carries exact pixel hash");*at=*at=='0'?'1':'0';
 AE_EXPECT_TRUE(!readImportCache(cache,"fx-corrupt",rejected),"changed cache snapshot is rejected");
}
AE_TEST(terrain_static_fx_export_refuses_unavailable_consumer_before_replacing_output){
 auto recipe=painted();recipe.surface.flow.streams=true;TerrainBuild build;std::string error;
 AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());std::vector<u8> output{4,3,2,1};
 AE_EXPECT_TRUE(!writeTerrainGlb(build,originals(),{},output,error),"Streams flag cannot publish without its visual consumer");
 AE_EXPECT_EQ(output,std::vector<u8>({4,3,2,1}),"previous source survives rejected publication");
 recipe=painted();recipe.fx.enabled=false;AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());
 AE_EXPECT_TRUE(!writeTerrainGlb(build,originals(),{},output,error),"Wetness cannot publish without the enabled pixel producer");
}
AE_TEST(terrain_flow_packed_sources_keep_raw_foam_heat_and_lifetime_channels){
 auto recipe=painted();auto png=originals()[0];const AssetGuid packed{19,2};
 recipe.surface.flow.streamNormal.texture=packed;
 const auto refs=terrainSurfaceTextureRefs(recipe.surface);
 const auto ref=std::find_if(refs.begin(),refs.end(),[&](const auto &v){return v.identity==packed;});
 AE_EXPECT_TRUE(ref!=refs.end()&&ref->linear&&!ref->normal&&!ref->normalRG&&ref->normalChannel==0,"packed RG normal/B foam is retained as raw data");
 TerrainBuild build;std::string error;AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());
 std::vector<u8> glb;const std::array<TerrainSurfaceImage,1> source{{{packed,png}}};
 AE_EXPECT_TRUE(writeTerrainGlb(build,originals(),source,glb,error),error.c_str());
 GltfImport model;AE_EXPECT_TRUE(importGlb(glb,{},{},model),model.diagnostic.c_str());
 AE_EXPECT_TRUE(model.terrainSurfaceImages.size()==1&&*model.terrainSurfaceImages[0].encoded==png,"disabled future Flow source original stays inside exported authoring source");
 AE_EXPECT_EQ(model.terrainSurfaces[0].surface.flow.streamNormal.texture,packed,"packed source identity remains editable");
}
AE_TEST(terrain_static_fx_2048_original_survives_real_glb_decode_and_cold_cache){
 auto recipe=painted();recipe.fx.width=recipe.fx.height=2048;
 recipe.fx.seedPixels.resize(usize(2048)*2048*4);
 for(usize n=0;n<recipe.fx.seedPixels.size();++n)recipe.fx.seedPixels[n]=u8((n*37+11)&255);
 TerrainFxLimits budget;budget.maximumWorkingBytes=256ull<<20;TerrainBuild build;std::string error;
 AE_EXPECT_TRUE(updateTerrainWithFxFrame(recipe,nullptr,build,error,{}, {},budget),error.c_str());
 std::vector<u8> glb;AE_EXPECT_TRUE(writeTerrainGlb(build,originals(),{},glb,error),error.c_str());
 GltfImportLimits limits;limits.maximumTextureDimension=1;GltfImport model;
 AE_EXPECT_TRUE(importGlb(glb,limits,{},model),model.diagnostic.c_str());
 const auto &fx=model.terrainSurfaces[0].staticFx;
 AE_EXPECT_TRUE(fx.width==2048&&fx.height==2048,"complete authoring resolution survives real image decoder despite generic cap1");
 AE_EXPECT_TRUE(model.textures[fx.textureIndex]->mipChain==recipe.fx.seedPixels,"all sixteen MiB of independent RGBA survive publication");
 std::vector<u8> cache;AE_EXPECT_TRUE(writeImportCache(model,"fx-2048",cache),"actual full-resolution import cache write");
 GltfImport cold;AE_EXPECT_TRUE(readImportCache(cache,"fx-2048",cold),"cold full-resolution cache load");
 const auto &restored=cold.terrainSurfaces[0].staticFx;
 AE_EXPECT_TRUE(cold.textures[restored.textureIndex]->mipChain==recipe.fx.seedPixels&&restored.identity==fx.identity,"no hidden downsize or identity drift on cold reload");
}
