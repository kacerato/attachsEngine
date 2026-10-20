// STAGED PURE TESTS ONLY. No active target registration/build performed.
#include "harness.h"
#include "resources/terrain_surface_asset.h"
#include "resources/terrain_flow_coverage.h"
#include <cmath>
#include <limits>
using namespace ae::resources;
namespace {
bool closeFlow(float a,float b){return std::abs(a-b)<1e-5f;}
TerrainFlowCoverageInputs flowInput(){TerrainFlowCoverageInputs i;i.staticAvailable=true;return i;}
}
AE_TEST(staged_flow_defaults_are_neutral_and_source_vectors_retained) {
  TerrainFlowSettings s;TerrainFlowCoverageInputs in;TerrainFlowCoverageResult out;std::string error;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_EQ(out.correspondence,TerrainFlowCoverageCorrespondence::Off,"Off needs no mask producer");
  AE_EXPECT_EQ(s.streamUvScales[3],70.f,"reserved source component retained");
  AE_EXPECT_EQ(s.lavaParams[3],.275f,"source flow phase frequency retained");
  AE_EXPECT_EQ(s.simulation.resistance[0],.1f,"manager wins over shader default .5");
  s.specularFade=true;in.worldHeight=50;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.specularHeightFactor,.5f),"standalone specular height does not require an FX producer");
}
AE_TEST(staged_flow_static_channels_preserve_puddles_with_zero_lava) {
  TerrainFlowSettings s;s.wetness=true;s.puddles=true;s.streams=true;
  auto in=flowInput();in.staticRgba={.2f,.4f,.1f,0};in.materialHeight=.2f;
  TerrainFlowCoverageResult out;std::string error;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.wetness,.2f)&&closeFlow(out.puddle,1)&&closeFlow(out.stream,0),"independent RGBA channels");
  AE_EXPECT_EQ(out.snowClearing,1.f,"poça clears snow before Lava feature");
  s.lava=true;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_EQ(out.puddle,1.f,"original puddle stage retained separately");
  AE_EXPECT_TRUE(closeFlow(out.sourceSnowClearing,.2f),"reference retains original source accumulator ordering");
  AE_EXPECT_EQ(out.snowClearing,1.f,"Astra preserves independent puddle when Lava feature has zero coverage");
  in.staticRgba={.1f,.25f,.21f,.24f};s.lavaParams[0]=1;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.puddle,.3f)&&closeFlow(out.stream,.4f)&&closeFlow(out.lavaSnowClearing,.12f),"overlapping independent coverages remain available");
  AE_EXPECT_TRUE(closeFlow(out.snowClearing,.4f),"effective clearing is maximum, not additive");
  in.staticRgba[3]=.4f;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.snowClearing,.6f),"positive lava clearing can dominate overlap");
}
AE_TEST(staged_flow_dynamic_channels_trails_and_global_maximum) {
  TerrainFlowSettings s;s.streams=true;s.dynamicFlows=true;s.globalStreams=true;
  auto in=flowInput();in.dynamicAvailable=true;in.dynamicRgba={.8f,.6f,.1f,.2f};
  in.controller.streamAvailable=true;in.controller.streamMaximum=.5f;
  TerrainFlowCoverageResult out;std::string error;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.rawDynamic[2],.1f)&&closeFlow(out.processed[2],.15f),"water triples then applies maximum");
  AE_EXPECT_TRUE(closeFlow(out.dynamicWetTrail,.8f)&&closeFlow(out.dynamicBurnTrail,.6f),"RG are trails, not flow direction");
  in.dynamicRgba[2]=0;in.materialHeight=.1f;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.stream,.85f),"dynamic wet trail contributes after water has left");
}
AE_TEST(staged_flow_height_distance_tracks_and_snow_mask_order) {
  TerrainFlowSettings s;s.wetness=true;s.heightWetness=true;s.distanceWetness=true;
  s.heightWetnessParams={10,1,0,0};s.distanceWetnessParams={50,1,150,0};
  auto in=flowInput();in.worldHeight=9;in.cameraDistance=125;
  TerrainFlowCoverageResult out;std::string error;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.wetness,.5f),"distance Z is length150; midpoint125");
  s.wetnessMaskSnow=true;s.tracksEnabled=true;s.traxFxThresholds={.4f,0,0,0};in.tracksAvailable=true;in.traxBuffer=0;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_TRUE(closeFlow(out.processed[0],.4f)&&closeFlow(out.snowClearing,.6f),"snow uses processed FX.R after tracks, not height wetness");
}
AE_TEST(staged_flow_failure_is_atomic_and_correspondence_is_never_exact) {
  TerrainFlowSettings s;s.puddles=true;auto in=flowInput();
  in.staticRgba[1]=.2f;in.sampledMaskMatchesVisual=true;in.materialInputsMatchVisual=true;
  TerrainFlowCoverageResult out;std::string error;
  AE_EXPECT_TRUE(evaluateTerrainFlowCoverage(s,in,out,error),error.c_str());
  AE_EXPECT_EQ(out.correspondence,TerrainFlowCoverageCorrespondence::EstimateStaticInputs,"confirmations do not invent a shared visual producer");
  const float saved=out.puddle;s.globalPuddles=true;
  AE_EXPECT_TRUE(!evaluateTerrainFlowCoverage(s,in,out,error),"missing global authority refuses");
  AE_EXPECT_EQ(out.puddle,saved,"output untouched on failure");
  in.controller.puddleAvailable=true;in.controller.puddleMaximum=1;in.staticRgba[0]=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!evaluateTerrainFlowCoverage(s,in,out,error),"NaN channels rejected");
  AE_EXPECT_EQ(out.puddle,saved,"NaN failure atomic");
  s.distanceWetness=true;s.distanceWetnessParams[2]=0;
  AE_EXPECT_TRUE(!validTerrainFlowCoverageSettings(s,error),"zero length rejected before division");
}
