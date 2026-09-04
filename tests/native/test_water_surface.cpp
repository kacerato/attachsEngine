#include "harness.h"
#include "renderer/water_surface.h"
#include <cmath>

using namespace ae::renderer;

namespace {
WaterProfile profile() {
  WaterProfile p{};p.waveCount=2;
  p.waves[0]={{1,0},0.5f,8.0f,1.2f,0.4f,0.0f};
  p.waves[1]={{0,1},0.15f,2.0f,2.0f,0.2f,0.3f};
  return p;
}
}

AE_TEST(Water_profile_validates_authoring_contract) {
  AE_EXPECT_EQ(validateWaterProfile(defaultOceanWaterProfile()),WaterValidationError::None,
               "fallback ocean profile must satisfy the public contract");
  auto p=profile();AE_EXPECT_EQ(validateWaterProfile(p),WaterValidationError::None,"valid");
  p.waves[0].direction={2,0};
  AE_EXPECT_EQ(validateWaterProfile(p),WaterValidationError::Wave,"normalized direction");
  p=profile();p.clipmapLevels=MaximumWaterClipmapLevels+1;
  AE_EXPECT_EQ(validateWaterProfile(p),WaterValidationError::Clipmap,"bounded clipmap");
}

AE_TEST(Water_sampling_is_deterministic_unit_normal_and_time_variant) {
  const auto p=profile();const WaterVec2 point{3.25f,-7.0f};
  const auto a=sampleWaterSurface(p,point,1.5f),b=sampleWaterSurface(p,point,1.5f);
  AE_EXPECT_EQ(a.height,b.height,"bit stable query");
  const float length=std::sqrt(a.normal.x*a.normal.x+a.normal.y*a.normal.y+a.normal.z*a.normal.z);
  AE_EXPECT_TRUE(std::abs(length-1.0f)<1e-5f,"unit normal");
  AE_EXPECT_TRUE(sampleWaterSurface(p,point,1.6f).height!=a.height,"animated surface");
  AE_EXPECT_TRUE(std::abs(maximumWaterDisplacement(p)-0.65f)<1e-6f,
                 "visibility extent is the sum of authored amplitudes");
}

AE_TEST(Water_exclusion_supports_feathered_circle_and_rotated_box) {
  WaterExclusionVolume circle{};circle.halfExtent={2,2};circle.feather=1;
  AE_EXPECT_EQ(waterCoverage(circle,{0,0}),0.0f,"circle interior");
  AE_EXPECT_EQ(waterCoverage(circle,{3,0}),1.0f,"circle exterior");
  AE_EXPECT_TRUE(waterCoverage(circle,{2.5f,0})>0.0f &&
                 waterCoverage(circle,{2.5f,0})<1.0f,"circle feather");
  WaterExclusionVolume box{WaterExclusionShape::Box,{0,0},{3,1},1.5707963f,0};
  AE_EXPECT_EQ(waterCoverage(box,{0,2}),0.0f,"rotation respected");
}

AE_TEST(Water_clipmap_is_grid_stable_bounded_and_has_skirts) {
  auto p=profile();WaterPatchPlan a{},b{};
  AE_EXPECT_TRUE(planWaterClipmap(p,{4.1f,8.1f},a),"first plan");
  AE_EXPECT_TRUE(planWaterClipmap(p,{4.2f,8.2f},b),"subcell plan");
  AE_EXPECT_EQ(a.count,76u,"4 center plus six 12-patch rings");
  AE_EXPECT_EQ(a.count,b.count,"stable count");
  for(ae::u32 i=0;i<a.count;++i) {
    AE_EXPECT_EQ(a.patches[i].origin.x,b.patches[i].origin.x,"stable x");
    AE_EXPECT_EQ(a.patches[i].origin.y,b.patches[i].origin.y,"stable z");
    AE_EXPECT_TRUE(a.patches[i].skirtDepth>0.0f,"crack skirt");
  }
}

AE_TEST(Water_pipeline_falls_back_by_capability_not_scene_name) {
  auto p=profile();p.reflection=WaterReflection::ScreenSpace;
  auto resolved=resolveWaterPipeline(p,{});
  AE_EXPECT_EQ(resolved.reflection,WaterReflection::Environment,"SSR fallback");
  AE_EXPECT_TRUE(resolved.usedFallback,"fallback is explicit");
  WaterCapabilities caps{true,true,true,true,false};
  resolved=resolveWaterPipeline(p,caps);
  AE_EXPECT_EQ(resolved.reflection,WaterReflection::ScreenSpace,"SSR retained");
  AE_EXPECT_TRUE(resolved.refraction&&resolved.spectralSimulation,"independent capabilities");
}

AE_TEST(Water_interactions_are_bounded_time_variant_and_reusable) {
  WaterInteractionField field;
  WaterImpulse impulse{};
  impulse.center={2.0f,-1.0f}; impulse.startTime=3.0f; impulse.amplitude=0.6f;
  AE_EXPECT_TRUE(field.addImpulse(impulse),"valid impulse");
  AE_EXPECT_EQ(field.activeCount(2.9f),0u,"future impulse inactive");
  AE_EXPECT_EQ(field.activeCount(3.5f),1u,"impulse active");
  const auto first=field.sample({4.0f,-1.0f},3.4f);
  const auto second=field.sample({4.0f,-1.0f},3.8f);
  AE_EXPECT_TRUE(first.height!=second.height,"expanding ring changes over time");
  AE_EXPECT_TRUE(first.normal.y>0.0f,"interaction returns upward unit normal");
  impulse.wavelength=0.01f;
  AE_EXPECT_TRUE(!field.addImpulse(impulse),"invalid wavelength rejected");
}
