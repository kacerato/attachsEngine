#include "harness.h"
#include "resources/terrain_snow_coverage.h"
#include "resources/terrain_texture_policy.h"
#include "resources/terrain_surface_texture_refs.h"

#include "renderer/terrain_surface_table.h"
#include <cmath>
#include <fstream>
#include <iterator>
using namespace ae::resources;
AE_TEST(terrain_snow_real_checkpoint_v2_migrates_to_v3_with_neutral_new_defaults){
  std::ifstream file("tests/native/fixtures/terrain-surface-v2.txt",std::ios::binary);
  std::string fixture{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()},error;
  AE_EXPECT_TRUE(fixture.starts_with("AETERRAINSURFACE2 "),"actual checkpoint writer fixture retains original version");
  TerrainSurfaceAsset surface;AE_EXPECT_TRUE(deserializeTerrainSurface(fixture,surface,error),error.c_str());
  AE_EXPECT_EQ(surface.snow.mode,TerrainSnowMode::Off,"historical surface does not acquire Snow");AE_EXPECT_EQ(surface.textureResidency.mode,TerrainTextureResidencyMode::Original,"historical source quality remains Original");
  AE_EXPECT_EQ(surface.geo.scaleMeters,5.f,"real V2 Geo authoring survives");AE_EXPECT_EQ(surface.globalMaps[0].scaleX,-2.f,"V2 signed UV authoring survives");AE_EXPECT_EQ(surface.globalMaps[0].scaleY,0.f,"V2 zero UV authoring survives");
  AE_EXPECT_EQ(surface.layers[0].snowParticipation,1.f,"new layer participation is neutral");AE_EXPECT_EQ(surface.layers[0].snowPorosity,.4f,"source core porosity default");
  AE_EXPECT_EQ(surface.layers[0].maps[0].textureResidency.mode,TerrainTextureResidencyMode::InheritTerrain,"legacy maps inherit Original");
  std::string migrated,again;AE_EXPECT_TRUE(serializeTerrainSurface(surface,migrated,error),error.c_str());AE_EXPECT_TRUE(migrated.starts_with("AETERRAINSURFACE4 "),"migration declares current version");
  TerrainSurfaceAsset cold;AE_EXPECT_TRUE(deserializeTerrainSurface(migrated,cold,error),error.c_str());AE_EXPECT_TRUE(serializeTerrainSurface(cold,again,error),error.c_str());AE_EXPECT_EQ(again,migrated,"cold migration preserves every old/new field");
}
namespace {
TerrainSurfaceAsset snowTestSurface(){TerrainSurfaceAsset s;TerrainSurfaceLayer l;l.identity={1,1};s.layers.push_back(l);return s;}
}
AE_TEST(terrain_snow_normal_rgb_rg_ga_are_explicit_persistent_descriptor_views){
  auto surface=snowTestSurface();auto &snow=surface.snow;snow.mode=TerrainSnowMode::Simple;
  TerrainSurfaceMap *maps[]{&snow.diffuse,&snow.height,&snow.normal,&snow.smoothness,&snow.ao};
  for(ae::u32 i=0;i<5;++i)maps[i]->texture={41,i+1};
  for(const auto packing:{TerrainSurfaceChannel::NormalRGB,TerrainSurfaceChannel::NormalRG,TerrainSurfaceChannel::NormalGA}){
    snow.normal.channel=packing;std::string encoded,error;
    AE_EXPECT_TRUE(serializeTerrainSurface(surface,encoded,error),error.c_str());
    TerrainSurfaceAsset restored;AE_EXPECT_TRUE(deserializeTerrainSurface(encoded,restored,error),error.c_str());
    AE_EXPECT_EQ(restored.snow.normal.channel,packing,"normal interpretation survives the real V3 writer/reader");
    ae::renderer::TerrainSurfaceBinding binding;binding.surface=restored;
    std::vector<ae::u32> slots;
    for(const auto &ref:terrainSurfaceTextureRefs(restored)){
      binding.textures.push_back({ref.identity,ref.linear,ref.normal,ae::u32(slots.size()),ref.normalChannel,ref.residencyCap,ref.clampSampling});
      slots.push_back(ae::u32(slots.size())+3);
      if(ref.identity==snow.normal.texture)AE_EXPECT_EQ(ref.normalChannel,ae::u32(packing),"packing is part of the actual descriptor identity");
    }
    std::vector<float> rows;ae::u32 offset=0;
    AE_EXPECT_TRUE(ae::renderer::appendTerrainSurfaceTable(binding,slots,rows,offset,error),error.c_str());
    AE_EXPECT_TRUE(offset!=0&&!rows.empty(),"RGB/RG/GA all reach actual Snow table publication");
  }
  snow.normal.channel=TerrainSurfaceChannel::RGB;std::string error;
  AE_EXPECT_TRUE(!validTerrainSurface(surface,error),"color RGB is not silently accepted as decoded normal RGB");
}
AE_TEST(terrain_snow_v3_roundtrip_preserves_authored_fields_and_all_map_policies){
  auto surface=snowTestSurface();surface.textureResidency={TerrainTextureResidencyMode::Cap,1024};
  surface.layers[0].snowParticipation=.3f;surface.layers[0].snowPorosity=.6f;
  surface.layers[0].maps[0].textureResidency={TerrainTextureResidencyMode::Original,0};
  auto &s=surface.snow;s.upVector={1,0,0,.75f};s.uvScales={-3,0,.25f,-.5f};s.normalNoiseParams={.2f,.7f,.3f,.4f};s.heightAngleRange={-20,80,.1f,.9f};
  s.distanceResample={-.25f,.4f,3,77};s.parameters={.2f,.1f,.8f,.6f};s.sssTint={.1f,.2f,.3f,.4f};s.sparkleViewDependency=.8f;
  s.normal.textureResidency={TerrainTextureResidencyMode::Cap,512};s.sourcePropertiesJson="{\"preserved\":42}";s.sourceKeywordsJson="[\"TRACKS_PENDING\"]";
  s.normalNoiseEnabled=s.distanceResampleEnabled=s.perLayerEnabled=s.maskEnabled=s.worldSpaceUV=s.stochasticEnabled=s.triplanarEnabled=s.useGlobalLevel=s.useGlobalHeight=s.sssEnabled=s.rimEnabled=s.sparkleEnabled=true;
  std::string encoded,error;AE_EXPECT_TRUE(serializeTerrainSurface(surface,encoded,error),"disabled Snow retains every authored field and deferred flag");AE_EXPECT_TRUE(encoded.starts_with("AETERRAINSURFACE4 "),"new format explicit");
  TerrainSurfaceAsset restored;AE_EXPECT_TRUE(deserializeTerrainSurface(encoded,restored,error),"V3 parses complete authored tail");
  std::string again;AE_EXPECT_TRUE(serializeTerrainSurface(restored,again,error),"reserialized V3");AE_EXPECT_EQ(again,encoded,"all Snow/residency fields survive bit-exact textual roundtrip");
  auto before=again;AE_EXPECT_TRUE(!deserializeTerrainSurface(encoded.substr(0,encoded.size()-10),restored,error),"truncated residency coverage rejected");
  AE_EXPECT_TRUE(serializeTerrainSurface(restored,again,error),"old output remains valid");AE_EXPECT_EQ(again,before,"failed tail load is transactional");
}
AE_TEST(terrain_snow_gpu_directory_relocates_atomically_and_gates_controller){
  using namespace ae::renderer;TerrainSurfaceBinding binding;binding.surface=snowTestSurface();auto &s=binding.surface.snow;s.mode=TerrainSnowMode::Rich;
  TerrainSurfaceMap *maps[]={&s.diffuse,&s.height,&s.normal,&s.smoothness,&s.ao};
  for(ae::u32 i=0;i<5;++i){maps[i]->texture={9,i+1};binding.textures.push_back({maps[i]->texture,true,i==2,i,i==2?7u:0u,0});}
  s.diffuse.linear=true;std::array<ae::u32,5> slots{4,5,6,7,8};std::vector<float> rows;ae::u32 offset=0;std::string error;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,rows,offset,error),"Rich Snow publishes complete sources");
  const auto canonical=rows;auto relocated=rows;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,17,error),"new Snow absolute pointers relocate");
  const ae::usize root=ae::usize(canonical[5])-1,snow=ae::usize(canonical[root*4+7])-1;
  AE_EXPECT_EQ(relocated[root*4+7],canonical[root*4+7]+17,"optional Snow root pointer plus-one relocated");
  AE_EXPECT_EQ(relocated[snow*4+55],canonical[snow*4+55]+17,"layer controls relocate with Snow");
  auto corrupt=canonical;corrupt[snow*4+55]+=1;auto invalid=corrupt;AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(corrupt,17,error),"invalid controls extent rejected");AE_EXPECT_TRUE(corrupt==invalid,"invalid relocation leaves canonical copy untouched");
  s.useGlobalLevel=true;offset=999;rows=canonical;AE_EXPECT_TRUE(!appendTerrainSurfaceTable(binding,slots,rows,offset,error),"global authority unavailable is diagnosed");AE_EXPECT_TRUE(rows==canonical&&offset==999,"missing controller cannot publish partial table");
  s.useGlobalLevel=false;slots[2]=InvalidMapTexture;AE_EXPECT_TRUE(!appendTerrainSurfaceTable(binding,slots,rows,offset,error),"GA descriptor is mandatory");AE_EXPECT_TRUE(rows==canonical,"missing descriptor publication remains atomic");
}
AE_TEST(terrain_snow_source_coverage_simple_rich_and_mask_priority){
  TerrainSnowSettings snow;snow.mode=TerrainSnowMode::Simple;snow.amount=.25f;TerrainSnowCoverageInputs input;input.worldHeight=140;input.normalMatchesVisual=true;TerrainSnowCoverageResult result;
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"simple coverage accepts shared geometric inputs");AE_EXPECT_TRUE(std::abs(result.coverage-.125f)<1e-6,"source cubic coverage and eight multiplier are real");
  input.materialHeight=.9f;input.occlusion=.1f;snow.parameters[0]=1;
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"simple ignores rich material clearing inputs");AE_EXPECT_EQ(result.coverage,.125f,"simple uses Height0 and AO1");
  snow.mode=TerrainSnowMode::Rich;input.materialInputsAvailable=true;
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"rich evaluates actual height");AE_EXPECT_EQ(result.coverage,0.f,"rich material height clears the same region");
  snow.mode=TerrainSnowMode::Simple;snow.parameters[0]=0;snow.amount=.2f;snow.maskEnabled=true;input.maskAvailable=true;input.maskRG={.1f,.3f};
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"conflicting min/max mask is explicitly supported");AE_EXPECT_TRUE(std::abs(result.fade-.3f)<1e-6&&std::abs(result.coverage-.216f)<1e-6,"source mask minimum G wins over maximum R");
}
AE_TEST(terrain_snow_source_height_direction_erosion_and_controller_authority){
  TerrainSnowSettings snow;snow.mode=TerrainSnowMode::Rich;TerrainSnowCoverageInputs input;input.worldHeight=140;input.materialInputsAvailable=true;input.normalMatchesVisual=true;TerrainSnowCoverageResult result;
  snow.parameters[1]=.5f;input.occlusion=.5f;
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"AO erosion evaluated");AE_EXPECT_TRUE(std::abs(result.erosionClearing-.0625f)<1e-6,"source squares erosion");AE_EXPECT_TRUE(std::abs(result.coverage-.6489868164f)<1e-6,"squared erosion affects nonlinear coverage");
  snow.parameters[1]=0;snow.perLayerEnabled=true;input.weightedParticipation=.5f;
  AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"rich participation before nonlinear mask");AE_EXPECT_EQ(result.coverage,.125f,"weighted participation scales level, not final coverage");
  input.worldHeight=50;AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"height entry valid");AE_EXPECT_EQ(result.coverage,0.f,"start of source linear height fade");
  input.worldHeight=140;snow.perLayerEnabled=false;snow.upVector={1,0,0,0};AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"arbitrary unit snow direction");AE_EXPECT_EQ(result.coverage,.25f,"source snowDot floor remains visible at perpendicular normal");
  snow.upVector={0,1,0,0};snow.useGlobalLevel=true;result.coverage=.75f;AE_EXPECT_TRUE(!evaluateTerrainSnowCoverage(snow,input,result),"missing controller never silently substitutes local state");AE_EXPECT_EQ(result.coverage,.75f,"failed authority resolution is atomic");
  input.controller.available=true;input.controller.level=.25f;AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"bound controller has explicit authority");AE_EXPECT_EQ(result.coverage,.125f,"bound level is actually consumed");
  snow.upVector={0,9,0,0};AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"non-unit authored direction is normalized like the GPU");AE_EXPECT_EQ(result.coverage,.125f,"up magnitude does not alter coverage");
  snow.upVector={0,0,0,0};result.coverage=.75f;AE_EXPECT_TRUE(!evaluateTerrainSnowCoverage(snow,input,result),"zero up direction rejected");AE_EXPECT_EQ(result.coverage,.75f,"invalid direction remains atomic");
}
AE_TEST(terrain_texture_authored_caps_do_not_change_source_or_sampling_domain){
  TerrainTextureResidency original{TerrainTextureResidencyMode::Original,0},inherit,cap{TerrainTextureResidencyMode::Cap,1024};ae::u32 first=99;
  AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(4096,2048,13,original,inherit,first),"complete original chain");AE_EXPECT_EQ(first,0u,"Original never imposes authored loss");
  AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(4096,2048,13,cap,inherit,first),"explicit terrain cap");AE_EXPECT_EQ(first,2u,"4096x2048 cap1024 gives actual mip2 1024x512");
  AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(4096,2048,13,cap,original,first),"map Original overrides terrain cap");AE_EXPECT_EQ(first,0u,"explicit map identity override is independent");
  cap.maxSide=512;AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(1,256,9,original,cap,first),"thin Geo strip cap above source");AE_EXPECT_EQ(first,0u,"thin original is never enlarged or unnecessarily reduced");
  cap.maxSide=1024;AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(3000,1500,12,original,cap,first),"non-power-of-two mip chain");AE_EXPECT_EQ(first,2u,"actual integer-halved mip2 is750x375");
  cap.maxSide=768;first=44;AE_EXPECT_TRUE(!terrainTextureFirstAllowedMip(3000,1500,12,original,cap,first),"unknown cap is rejected");AE_EXPECT_EQ(first,44u,"invalid residency policy leaves applied bound unchanged");
}
AE_TEST(terrain_snow_short_height_interval_and_pending_puddle_correspondence){
  TerrainSnowSettings snow;snow.amount=.1f;snow.heightAngleRange={0,.0001f,0,1};
  TerrainSnowCoverageInputs input;input.worldHeight=.00005f;input.normalMatchesVisual=true;input.materialInputsAvailable=true;
  TerrainSnowCoverageResult result;
  for(const auto mode:{TerrainSnowMode::Simple,TerrainSnowMode::Rich}){
    snow.mode=mode;input.puddleHeight=0;
    AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"source submillimetre height interval evaluates");
    AE_EXPECT_TRUE(std::abs(result.fade-.005f)<1e-8,"source denominator has .001 minimum, not exact end-start");
    AE_EXPECT_TRUE(std::abs(result.coverage-.000001f)<1e-11,"source cubic coverage retains short-interval scale");
    AE_EXPECT_EQ(result.correspondence,mode==TerrainSnowMode::Simple?TerrainSnowCoverageCorrespondence::ExactSimpleInputs:TerrainSnowCoverageCorrespondence::ExactRichInputs,"zero puddle permits exact shared inputs");
    input.puddleHeight=.001f;
    AE_EXPECT_TRUE(evaluateTerrainSnowCoverage(snow,input,result),"pending puddle source computation remains available");
    AE_EXPECT_TRUE(std::abs(result.coverage-.000000512f)<1e-11,"puddle source clearing formula is retained");
    AE_EXPECT_EQ(result.correspondence,mode==TerrainSnowMode::Simple?TerrainSnowCoverageCorrespondence::EstimateSimple:TerrainSnowCoverageCorrespondence::EstimateRich,"nonzero puddle never claims unsupported visual correspondence");
  }
}
