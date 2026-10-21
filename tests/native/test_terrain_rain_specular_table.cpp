#include "harness.h"
#include "renderer/terrain_surface_table.h"
#include "resources/terrain_flow_coverage.h"
using namespace ae;
namespace {
renderer::TerrainSurfaceBinding rainBinding(){
 renderer::TerrainSurfaceBinding b;resources::TerrainSurfaceLayer layer;layer.identity={92,1};b.surface.layers={layer};
 auto &f=b.surface.flow;f.puddles=f.rainDrops=true;f.rainDropTexture.texture={92,11};f.rainDropTexture.channel=resources::TerrainSurfaceChannel::RGB;f.rainIntensityScale={.75f,150,17,-19};
 auto &fx=b.staticFx;fx.enabled=true;fx.textureIndex=0;fx.width=fx.height=1;fx.domain=b.terrainDomain;fx.identity={92,2};fx.revision=1;fx.rgbaSha256=std::string(64,'a');
 b.textures.push_back({f.rainDropTexture.texture,true,false,1,0,0,false});return b;
}
}
AE_TEST(rain_records_keep_rgba_raw_remaps_residency_and_atomic_relocation){
 auto b=rainBinding();auto &map=b.surface.flow.rainDropTexture;map.factor=.75f;map.invert=true;map.colorFactor={1,.5f,.25f,1};map.colorRemapMin={.1f,.2f,.3f,.4f};map.colorRemapMax={.6f,.7f,.8f,.9f};map.textureResidency.mode=resources::TerrainTextureResidencyMode::Cap;map.textureResidency.maxSide=256;b.textures[0].residencyCap=256;
 std::array<u32,2> slots{12,0};std::vector<float> table;u32 offset=0;std::string error;
 auto invalidCap=b;invalidCap.surface.flow.rainDropTexture.textureResidency.maxSide=128;
 AE_EXPECT_TRUE(!renderer::appendTerrainSurfaceTable(invalidCap,slots,table,offset,error),"128 is not an authored residency choice; it must not silently resize or publish");
 AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(b,slots,table,offset,error),error.c_str());usize root=usize(table[5])-1,directory=usize(table[root*4+12])-1,row=usize(table[(directory+3)*4+2]),raw=row+2;
 AE_EXPECT_EQ(table[root*4],5.f,"rain introduces root5");AE_EXPECT_EQ(table[(directory+3)*4],6.f,"rain type6");AE_EXPECT_EQ(table[raw*4],0.f,"actual raw descriptor0 is valid");AE_EXPECT_EQ(table[raw*4+1],float(resources::TerrainSurfaceChannel::RGB),"rain is rawRGBA, no conventional normal filter");AE_EXPECT_EQ(table[raw*4+2],1.f,"inversion carried");AE_EXPECT_EQ(table[raw*4+3],.75f,"factor carried");
 for(usize n=0;n<4;++n){AE_EXPECT_EQ(table[(raw+2)*4+n],map.colorFactor[n],"RGBA factors");AE_EXPECT_EQ(table[(raw+3)*4+n],map.colorRemapMin[n],"RGBA remap min");AE_EXPECT_EQ(table[(raw+4)*4+n],map.colorRemapMax[n],"RGBA remap max");AE_EXPECT_EQ(table[(row+1)*4+n],b.surface.flow.rainIntensityScale[n],"all rain vector components retained");}
 auto moved=table;AE_EXPECT_TRUE(renderer::relocateTerrainSurfaceTable(moved,7,error),error.c_str());std::vector<float> direct(28,0);AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(b,slots,direct,offset,error),error.c_str());AE_EXPECT_EQ(moved,std::vector<float>(direct.begin()+28,direct.end()),"rain pointers relocate exactly");
 auto bad=table;bad[(directory+3)*4+2]+=1;auto saved=bad;AE_EXPECT_TRUE(!renderer::relocateTerrainSurfaceTable(bad,7,error),"corrupt rain pointer rejects");AE_EXPECT_EQ(bad,saved,"rejection atomic");
 b.textures[0].residencyCap=0;AE_EXPECT_TRUE(!renderer::appendTerrainSurfaceTable(b,slots,direct,offset,error),"cap mismatch cannot resolve the wrong raw role");
}
AE_TEST(specular_only_has_no_fx_descriptor_and_matches_cpu_height_factor){
 renderer::TerrainSurfaceBinding b;resources::TerrainSurfaceLayer layer;layer.identity={92,1};b.surface.layers={layer};b.surface.flow.specularFade=true;b.surface.flow.specularFades={0,100,17,-19};
 std::vector<float> table;u32 offset=0;std::string error;AE_EXPECT_TRUE(renderer::diagnoseStaticTerrainFlowPublication(b,error),error.c_str());AE_EXPECT_TRUE(renderer::appendTerrainSurfaceTable(b,{},table,offset,error),error.c_str());
 usize root=usize(table[5])-1,directory=usize(table[root*4+12])-1,snapshot=usize(table[directory*4+2]),spec=usize(table[(directory+3)*4+2]);
 AE_EXPECT_EQ(table[root*4],5.f,"spec-only root5");AE_EXPECT_EQ(table[snapshot*4],-1.f,"literal absent texture");AE_EXPECT_EQ(table[snapshot*4+1],0.f,"no phantom width");AE_EXPECT_EQ(table[snapshot*4+2],0.f,"no phantom height");AE_EXPECT_EQ(table[snapshot*4+3],0.f,"snapshot flag disabled");AE_EXPECT_EQ(table[(directory+3)*4],7.f,"specular type7");
 for(usize n=0;n<4;++n)AE_EXPECT_EQ(table[(spec+1)*4+n],b.surface.flow.specularFades[n],"all spec vector components retained");
 auto moved=table;AE_EXPECT_TRUE(renderer::relocateTerrainSurfaceTable(moved,13,error),error.c_str());AE_EXPECT_EQ(moved[snapshot*4],-1.f,"relocation never turns placeholder into descriptor");
 auto bad=table;bad[snapshot*4]=0;auto saved=bad;AE_EXPECT_TRUE(!renderer::relocateTerrainSurfaceTable(bad,13,error),"disabled snapshot cannot acquire a valid index");AE_EXPECT_EQ(bad,saved,"placeholder validation atomic");
 resources::TerrainFlowCoverageInputs in;in.worldHeight=50;resources::TerrainFlowCoverageResult coverage;AE_EXPECT_TRUE(resources::evaluateTerrainFlowCoverage(b.surface.flow,in,coverage,error),error.c_str());AE_EXPECT_EQ(coverage.specularHeightFactor,.5f,"existing CPU accepts spec-only without static producer");
 std::string serialized;AE_EXPECT_TRUE(resources::serializeTerrainSurface(b.surface,serialized,error),error.c_str());resources::TerrainSurfaceAsset cold;AE_EXPECT_TRUE(resources::deserializeTerrainSurface(serialized,cold,error),error.c_str());AE_EXPECT_EQ(cold.flow.specularFades,b.surface.flow.specularFades,"existing Surface4 keeps spec fields");
}
AE_TEST(rain_requires_puddles_raw_source_and_local_authority){
 auto b=rainBinding();std::string error;AE_EXPECT_TRUE(renderer::diagnoseStaticTerrainFlowPublication(b,error),error.c_str());
 auto bad=b;bad.surface.flow.puddles=false;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"source keyword requires puddles");
 bad=b;bad.surface.flow.rainDropTexture.texture={};AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"no fake ripple texture");
 bad=b;bad.surface.flow.globalRain=true;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"global rain controller still has no producer");
 bad=b;bad.surface.flow.dynamicFlows=true;AE_EXPECT_TRUE(!renderer::diagnoseStaticTerrainFlowPublication(bad,error),"dynamic integration remains independently gated");
}
