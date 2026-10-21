#include "harness.h"
#include "renderer/terrain_surface_table.h"
using namespace ae;
namespace {
renderer::TerrainSurfaceBinding fxBinding(){
 renderer::TerrainSurfaceBinding b;resources::TerrainSurfaceLayer layer;layer.identity={71,1};layer.snowPorosity=.6f;layer.flow.foam=1.5f;b.surface.layers={layer};
 b.surface.flow.wetness=true;b.surface.flow.puddles=true;b.surface.flow.perTexPorosity=true;
 auto &fx=b.staticFx;fx.enabled=true;fx.textureIndex=0;fx.width=fx.height=1;fx.domain=b.terrainDomain;fx.identity={71,2};fx.revision=1;fx.rgbaSha256=std::string(64,'a');return b;
}
}
AE_TEST(static_fx_table_relocates_all_pointers_and_preserves_descriptor){
 using namespace renderer;auto b=fxBinding();std::array<u32,1> slots{12};std::vector<float> table;u32 offset=0;std::string error;
 AE_EXPECT_TRUE(appendTerrainSurfaceTable(b,slots,table,offset,error),error.c_str());
 const usize root=usize(table[5])-1,directory=usize(table[root*4+12])-1,snapshot=usize(table[directory*4+2]);
 AE_EXPECT_EQ(table[root*4],3.f,"Flow uses version three module directory");AE_EXPECT_EQ(table[snapshot*4],12.f,"actual snapshot descriptor transported");
 auto moved=table;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(moved,7,error),error.c_str());
 std::vector<float> direct(28,0);AE_EXPECT_TRUE(appendTerrainSurfaceTable(b,slots,direct,offset,error),error.c_str());
 AE_EXPECT_EQ(moved,std::vector<float>(direct.begin()+28,direct.end()),"all directories controls and extents relocate atomically");
 for(usize index:{root*4+12,directory*4+2,snapshot*4+8,snapshot*4+11}){auto bad=table;bad[index]+=1;auto saved=bad;AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(bad,7,error),"corrupt Flow pointer rejected");AE_EXPECT_EQ(bad,saved,"rejection never mutates");}
}
AE_TEST(static_fx_publication_refuses_missing_snapshot_and_unimplemented_flags){
 auto original=fxBinding();std::string error;AE_EXPECT_TRUE(renderer::diagnoseStaticTerrainFlowPublication(original,error),error.c_str());
 auto b=original;b.staticFx.enabled=false;b.staticFx.textureIndex=renderer::InvalidMapTexture;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"active Flow requires actual Recipe snapshot");
 b=original;b.surface.flow.wetness=b.surface.flow.puddles=false;b.surface.flow.wetnessMaskSnow=true;b.staticFx.enabled=false;b.staticFx.textureIndex=renderer::InvalidMapTexture;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"maskSnow alone still requires actual snapshot");
 b=original;b.staticFx.domain[2]+=1;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"domain mismatch rejected");
 b=original;b.staticFx.rgbaSha256[0]='Z';AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"noncanonical hash rejected");
 for(bool resources::TerrainFlowSettings::*field:{&resources::TerrainFlowSettings::dynamicFlows,&resources::TerrainFlowSettings::globalWetness,&resources::TerrainFlowSettings::tracksEnabled}){b=original;b.surface.flow.*field=true;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"unimplemented enabled flag rejected");}
 b=original;b.surface.flow.streamNormal.texture={71,3};AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(b,error),"even disabled authoring references must resolve");
}
AE_TEST(static_fx_table_missing_descriptor_is_atomic){
 auto b=fxBinding();std::vector<float> table(4,17);auto before=table;std::string error;u32 offset=29;std::array<u32,1> slots{renderer::InvalidMapTexture};
 AE_EXPECT_TRUE(!renderer::appendTerrainSurfaceTable(b,slots,table,offset,error),"missing descriptor rejects publication");AE_EXPECT_EQ(table,before,"table untouched");AE_EXPECT_EQ(offset,29u,"offset untouched");
}
