#include "harness.h"
#include "renderer/terrain_surface_table.h"
using namespace ae;
namespace {
renderer::TerrainSurfaceBinding flowBinding(){
 renderer::TerrainSurfaceBinding b;resources::TerrainSurfaceLayer layer;layer.identity={82,1};layer.flow.foam=1.5f;b.surface.layers={layer};
 auto &flow=b.surface.flow;flow.streams=flow.lava=true;flow.streamNormal.texture={82,11};flow.streamNormal.channel=resources::TerrainSurfaceChannel::NormalRG;flow.lavaDiffuse.texture={82,12};flow.lavaDiffuse.channel=resources::TerrainSurfaceChannel::RGB;
 auto &fx=b.staticFx;fx.enabled=true;fx.textureIndex=0;fx.width=fx.height=1;fx.domain=b.terrainDomain;fx.identity={82,2};fx.revision=1;fx.rgbaSha256=std::string(64,'a');
 b.textures.push_back({flow.streamNormal.texture,true,false,1,0,0,false});b.textures.push_back({flow.lavaDiffuse.texture,true,false,2,0,0,false});return b;
}
}
AE_TEST(streams_lava_typed_records_raw_packing_and_atomic_relocation){
 using namespace renderer;std::array<u32,3> slots{5,9,11};std::string error;
 for(u32 features=1;features<=3;++features){auto b=flowBinding();b.surface.flow.streams=(features&1)!=0;b.surface.flow.lava=(features&2)!=0;
  std::vector<float> table;u32 offset=0;AE_EXPECT_TRUE(appendTerrainSurfaceTable(b,slots,table,offset,error),error.c_str());
  usize root=usize(table[5])-1,directory=usize(table[root*4+12])-1,entries=3+((features&1)?1:0)+((features&2)?1:0);
  AE_EXPECT_EQ(table[root*4],4.f,"Streams/Lava declare module version four");AE_EXPECT_EQ(table[root*4+13],float(entries),"only real consumers add typed records");
  usize last=usize(table[(directory+entries-1)*4+2]);AE_EXPECT_EQ(table[(directory+entries-1)*4],(features&2)?5.f:4.f,"type4 stream / type5 lava");
  usize snapshot=usize(table[directory*4+2]),controls=usize(table[snapshot*4+8]);AE_EXPECT_EQ(table[controls*4+1],1.5f,"source foam up to two preserved");
  usize map=last+((features&2)?9:6);AE_EXPECT_EQ(table[map*4],(features&2)?11.f:9.f,"real raw linear descriptor");AE_EXPECT_EQ(table[map*4+1],float(resources::TerrainSurfaceChannel::RGB),"normal components remain alongside independent foam/heat/crust");
  auto moved=table;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(moved,13,error),error.c_str());std::vector<float> direct(52,0);AE_EXPECT_TRUE(appendTerrainSurfaceTable(b,slots,direct,offset,error),error.c_str());AE_EXPECT_EQ(moved,std::vector<float>(direct.begin()+52,direct.end()),"all typed pointers relocate as direct publication");
  for(usize index:{root*4+13,(directory+entries-1)*4+2,(directory+entries-1)*4+3}){auto bad=table;bad[index]+=1;auto saved=bad;AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(bad,13,error),"malformed directory rejects");AE_EXPECT_EQ(bad,saved,"malformed relocation atomic");}
 }
}
AE_TEST(streams_lava_publication_keeps_dynamic_global_gates){
 auto b=flowBinding();std::string error;AE_EXPECT_TRUE(renderer::diagnoseStaticTerrainFlowPublication(b,error),error.c_str());
 for(bool resources::TerrainFlowSettings::*field:{&resources::TerrainFlowSettings::dynamicFlows,&resources::TerrainFlowSettings::globalStreams,&resources::TerrainFlowSettings::tracksEnabled}){auto bad=b;bad.surface.flow.*field=true;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"no fake producer capability");}
 auto bad=b;bad.textures[0].textureIndex=renderer::InvalidMapTexture;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"missing packed source rejects");
 bad=b;bad.staticFx.enabled=false;bad.staticFx.textureIndex=renderer::InvalidMapTexture;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"Streams/Lava require actual quantity producer");
}
