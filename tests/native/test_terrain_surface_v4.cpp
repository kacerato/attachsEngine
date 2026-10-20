// Full SurfaceV4 persistence and real historical writer migration.
#include "harness.h"
#include "resources/terrain_surface_asset.h"
#include "resources/terrain_surface_map_visitor.h"
#include <fstream>
#include <iterator>
#include <limits>
using namespace ae::resources;
namespace {
TerrainSurfaceAsset flowSurface(){TerrainSurfaceAsset a;TerrainSurfaceLayer l;l.identity=assetGuidFromSeed("v4-layer");l.snowPorosity=.23f;a.layers={l};return a;}
auto flowVectors(const TerrainFlowSettings &v){return std::array{&v.wetnessParams,&v.puddleParams,&v.heightWetnessParams,&v.distanceWetnessParams,&v.streamFlowParams,&v.streamNormalFoam,&v.streamUvScales,&v.streamTint,&v.streamFades,&v.lavaParams,&v.lavaParams2,&v.lavaEdgeColor,&v.lavaColorLow,&v.lavaColorHighlight,&v.lavaUvScale,&v.lavaFades,&v.rainIntensityScale,&v.specularFades,&v.traxFxThresholds};}
auto flowFlags(const TerrainFlowSettings &v){return std::array{v.wetness,v.puddles,v.streams,v.lava,v.perTexPorosity,v.perTexFoam,v.dynamicFlows,v.wetnessMaskSnow,v.heightWetness,v.rainDrops,v.globalWetness,v.globalRain,v.globalStreams,v.globalPuddles,v.heightDampStreams,v.heightDampLava,v.specularFade,v.distanceWetness,v.lavaStochastic,v.tracksEnabled,v.simulation.enabled};}
}
AE_TEST(staged_surface_v4_flow_roundtrip_preserves_all_groups_and_maps) {
  auto a=flowSurface();auto &v=a.flow;
  v.wetness=v.puddles=v.streams=v.lava=v.perTexPorosity=v.perTexFoam=v.dynamicFlows=v.wetnessMaskSnow=true;
  v.heightWetness=v.rainDrops=v.globalWetness=v.globalRain=v.globalStreams=v.globalPuddles=true;
  v.heightDampStreams=v.heightDampLava=v.specularFade=v.distanceWetness=v.lavaStochastic=v.tracksEnabled=true;
  v.sourceRainMode=TerrainFlowRainMode::StreamsAndWetness;v.controller=assetGuidFromSeed("controller");
  v.globalPorosity=.31f;v.streamBlend=18;v.streamMaximum=.8f;v.lavaDisplacementScale=.3f;
  v.lavaEmissiveMultiplier=7;v.lavaStochasticContrast=.41f;v.lavaStochasticSize=1.2f;
  v.wetnessParams[2]=123;v.streamUvScales[3]=-17;v.lavaUvScale[2]=39;v.rainIntensityScale[3]=57;
  v.sourcePropertiesJson="{\"porosity\":0.23,\"unmapped\":17}";v.sourceKeywordsJson="[\"_STREAMS\",\"future\"]";
  v.sourceTextureEnvsJson="{\"_StreamNormal_ST\":[2,3,4,5]}";
  v.simulation.enabled=true;v.simulation.evaporation={.02f,.07f};v.simulation.strength={.3f,.8f};
  v.simulation.speed={2,3};v.simulation.resistance={.6f,.9f};v.simulation.wetnessEvaporation=.4f;
  v.simulation.burnEvaporation=.7f;v.simulation.maximumEmitters=53;v.simulation.maximumColliders=31;
  v.simulation.sourcePropertiesJson="{\"manager\":true}";a.layers[0].flow.foam=1.7f;
  unsigned mapIndex=0;visitTerrainFlowMaps(a,[&](auto &m){m.texture=assetGuidFromSeed("flowmap"+std::to_string(mapIndex++));m.factor=.6f;m.invert=true;m.textureResidency={TerrainTextureResidencyMode::Cap,512};});
  std::string text,error;AE_EXPECT_TRUE(serializeTerrainSurface(a,text,error),error.c_str());
  AE_EXPECT_TRUE(text.starts_with("AETERRAINSURFACE4 "),"new version tag");
  TerrainSurfaceAsset b;AE_EXPECT_TRUE(deserializeTerrainSurface(text,b,error),error.c_str());
  std::string again;AE_EXPECT_TRUE(serializeTerrainSurface(b,again,error),error.c_str());AE_EXPECT_EQ(again,text,"canonical whole-document roundtrip");
  AE_EXPECT_TRUE(flowFlags(a.flow)==flowFlags(b.flow),"all feature flags survive");
  auto va=flowVectors(a.flow),vb=flowVectors(b.flow);for(size_t i=0;i<va.size();++i)AE_EXPECT_TRUE(*va[i]==*vb[i],"all vectors/reserved components survive");
  AE_EXPECT_EQ(b.flow.controller,v.controller,"controller identity");AE_EXPECT_EQ(b.flow.sourceRainMode,v.sourceRainMode,"unused source enum retained");
  AE_EXPECT_EQ(b.flow.sourcePropertiesJson,v.sourcePropertiesJson,"porosity/unknown source properties retained");
  AE_EXPECT_EQ(b.flow.sourceTextureEnvsJson,v.sourceTextureEnvsJson,"original texture ST retained");
  AE_EXPECT_EQ(b.flow.sourceKeywordsJson,v.sourceKeywordsJson,"unknown keywords retained");
  AE_EXPECT_EQ(b.flow.lavaEmissiveMultiplier,7.f,"scalar survives");AE_EXPECT_EQ(b.flow.globalPorosity,.31f,"global default preserved");
  AE_EXPECT_EQ(b.flow.simulation.maximumEmitters,ae::u32(53),"manager fields survive");AE_EXPECT_EQ(b.flow.simulation.burnEvaporation,.7f,"manager evaporation survives");
  AE_EXPECT_EQ(b.layers[0].snowPorosity,.23f,"shared porosity has exactly the old authority");AE_EXPECT_EQ(b.layers[0].flow.foam,1.7f,"new layer foam survives");
  std::vector<AssetGuid> expected,actual;visitTerrainFlowMaps(a,[&](auto &m){expected.push_back(m.texture);});
  visitTerrainFlowMaps(b,[&](auto &m){actual.push_back(m.texture);AE_EXPECT_EQ(m.textureResidency.maxSide,ae::u32(512),"Flow residency lives in V4 tail");});
  AE_EXPECT_TRUE(actual==expected&&actual.size()==5,"every map identity remains exact");
}
AE_TEST(staged_surface_v4_disabled_data_resolution_and_publication_are_separate) {
  auto a=flowSurface();a.flow.streamControl.texture=assetGuidFromSeed("disabled-source");
  a.flow.streamFades={5,1,-7,-10}; // no active height consumer: retain authored data.
  std::string error,text;AE_EXPECT_TRUE(serializeTerrainSurface(a,text,error),error.c_str());
  std::vector<TerrainSurfaceDiagnostic> diagnostics;
  AE_EXPECT_TRUE(!resolveTerrainSurface(a,[](AssetGuid){return false;},diagnostics,error),"disabled references remain discoverable");
  AE_EXPECT_TRUE(!diagnostics.empty()&&diagnostics.back().location=="flow.staticControl","located missing Flow reference");
  a.flow.wetness=true;TerrainFlowConsumerCapabilities caps;
  AE_EXPECT_TRUE(!diagnoseTerrainFlowPublication(a,[](AssetGuid){return true;},caps,diagnostics,error),"active model cannot publish with no consumer");
  bool found=false;for(const auto &d:diagnostics)found=found||d.location=="unavailable.consumer:flow.wetness";
  AE_EXPECT_TRUE(found,"missing consumer is explicit");
  caps.wetness=true;AE_EXPECT_TRUE(diagnoseTerrainFlowPublication(a,[](AssetGuid){return true;},caps,diagnostics,error),error.c_str());
  auto previous=a;a.flow.streamBlend=std::numeric_limits<float>::quiet_NaN();
  text="sentinel";AE_EXPECT_TRUE(!serializeTerrainSurface(a,text,error),"nonfinite model rejected");AE_EXPECT_EQ(text,std::string("sentinel"),"serialization atomic");
  AE_EXPECT_TRUE(serializeTerrainSurface(previous,text,error),error.c_str());text.pop_back();text.resize(text.size()/2);
  AE_EXPECT_TRUE(!deserializeTerrainSurface(text,previous,error),"truncated V4 rejected");AE_EXPECT_TRUE(previous.flow.wetness,"failed reader retains destination");
}
AE_TEST(staged_surface_v4_reads_frozen_real_v3_and_preserves_entire_old_prefix) {
  // Generated by the frozen legacy C++ writer, never handwritten/substituted.
  std::ifstream file("tests/native/fixtures/terrain-surface-v3.txt",std::ios::binary);
  std::string old{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
  AE_EXPECT_TRUE(old.starts_with("AETERRAINSURFACE3 "),"generate/freeze real V3 fixture before this test");
  TerrainSurfaceAsset a;std::string error,next;
  AE_EXPECT_TRUE(deserializeTerrainSurface(old,a,error),error.c_str());
  AE_EXPECT_TRUE(!a.flow.wetness&&!a.flow.puddles&&!a.flow.streams&&!a.flow.lava,"V3 migrates Flow neutral");
  AE_EXPECT_EQ(a.layers[0].snowPorosity,.2f,"V3 shared porosity retained without default overwrite");
  AE_EXPECT_TRUE(serializeTerrainSurface(a,next,error),error.c_str());
  const std::string oldTag="AETERRAINSURFACE3 ",newTag="AETERRAINSURFACE4 ";
  AE_EXPECT_EQ(next.substr(newTag.size(),old.size()-oldTag.size()),old.substr(oldTag.size()),"legacy body/V2/V3 map coverage remains byte-for-byte prefix");
  size_t oldCount=0,allCount=0;visitTerrainSurfaceMapsV3(a,[&](auto &){++oldCount;});visitTerrainSurfaceMaps(a,[&](auto &){++allCount;});
  AE_EXPECT_EQ(allCount,oldCount+5,"Flow maps are outside the frozen V3 residency coverage");
}
AE_TEST(staged_surface_v4_preserves_approved_snow_normal_packing_choices) {
  for(const auto packing:{TerrainSurfaceChannel::NormalRGB,TerrainSurfaceChannel::NormalRG,TerrainSurfaceChannel::NormalGA}) {
    auto a=flowSurface();a.snow.mode=TerrainSnowMode::Simple;
    for(auto *m:{&a.snow.diffuse,&a.snow.height,&a.snow.normal,&a.snow.smoothness,&a.snow.ao})
      m->texture=assetGuidFromSeed("snow-source-packing");
    a.snow.normal.channel=packing;
    std::string text,error;
    AE_EXPECT_TRUE(validTerrainSurface(a,error),error.c_str());
    AE_EXPECT_TRUE(serializeTerrainSurface(a,text,error),error.c_str());
    TerrainSurfaceAsset b;AE_EXPECT_TRUE(deserializeTerrainSurface(text,b,error),error.c_str());
    AE_EXPECT_EQ(b.snow.normal.channel,packing,"V4 does not reinstate the obsolete GA-only restriction");
    AE_EXPECT_TRUE(!b.flow.streams&&!b.flow.wetness,"packing refinement does not enable Flow consumers");
  }
}
