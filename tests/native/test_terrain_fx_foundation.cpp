#include "harness.h"
#include "resources/terrain_fx_field.h"
#include <cmath>
#include <limits>
using namespace ae::resources;
namespace {
TerrainFxAsset fx(){TerrainFxAsset a;a.enabled=true;a.width=a.height=5;a.domain={-2.5,-2.5,5,5};return a;}
TerrainFxStroke stamp(){TerrainFxStroke s;s.identity=1;s.radiusMeters=.49;return s;}
bool near(float a,float b){return std::abs(a-b)<1e-6f;}
}
AE_TEST(terrain_fx_target_time_pressure_and_channels_are_independent){
  auto a=fx();a.seedPixels.resize(100);for(ae::usize i=0;i<25;++i){a.seedPixels[i*4]=51;a.seedPixels[i*4+1]=102;a.seedPixels[i*4+2]=153;a.seedPixels[i*4+3]=204;}
  TerrainFxField before,after;TerrainFxDirtyRect dirty;std::string error;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,before,dirty,error),error.c_str());
  auto s=stamp();s.flow=8;s.sampleSeconds=.0625;s.pressure=.5;a.strokes={s};
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},&before,after,dirty,error),error.c_str());
  AE_EXPECT_TRUE(near(after.values[12][0],.4f),"source Target alpha=8*.0625*.5=.25, lerp(.2,1,.25)=.4");
  AE_EXPECT_TRUE(near(after.values[12][1],.4f)&&near(after.values[12][2],.6f)&&near(after.values[12][3],.8f),"other FX channels remain unchanged and their sum is not normalized");
  AE_EXPECT_EQ(dirty,(TerrainFxDirtyRect{2,2,3,3}),"dirty texel is exact and half-open");
  s.identity=2;s.channel=TerrainFxChannel::Lava;s.target=0;s.flow=65536;s.sampleSeconds=1;a.strokes.push_back(s);
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},&after,before,dirty,error),error.c_str());AE_EXPECT_EQ(before.values[12][3],0.f,"target0 erases only selected Lava channel");
  AE_EXPECT_EQ(before.values[12][0],after.values[12][0],"erase cannot repaint Wetness");
}
AE_TEST(terrain_fx_brush_shape_rotation_aspect_hardness_and_falloff_have_effect){
  auto a=fx();auto s=stamp();s.radiusMeters=1.5;s.flow=1;s.sampleSeconds=1;s.shape=TerrainFxBrushShape::Box;s.hardness=1;a.strokes={s};
  TerrainFxField box,circle,ellipse,falloff;TerrainFxDirtyRect dirty;std::string error;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,box,dirty,error),error.c_str());AE_EXPECT_EQ(box.values[6][0],1.f,"box includes corner(-1,-1)");
  a.strokes[0].shape=TerrainFxBrushShape::Circle;a.strokes[0].radiusMeters=1.1;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,circle,dirty,error),error.c_str());AE_EXPECT_EQ(circle.values[6][0],0.f,"circle excludes same corner");
  a.strokes[0].aspect=2;a.strokes[0].rotationDegrees=90;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,ellipse,dirty,error),error.c_str());AE_EXPECT_EQ(ellipse.values[2][0],1.f,"rotated elongated brush reaches z=-2");AE_EXPECT_EQ(ellipse.values[10][0],0.f,"rotated brush no longer reaches x=-2");
  a.strokes[0].aspect=1;a.strokes[0].rotationDegrees=0;a.strokes[0].radiusMeters=2;a.strokes[0].hardness=0;a.strokes[0].falloff=2;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,falloff,dirty,error),error.c_str());AE_EXPECT_TRUE(near(falloff.values[13][0],.25f),"distance.5 with power2 yields Target influence.25");
  a.strokes[0].hardness=.5;a.strokes[0].opacity=.5;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,falloff,dirty,error),error.c_str());AE_EXPECT_EQ(falloff.values[13][0],.5f,"hard inner region and opacity both affect brush");
}
AE_TEST(terrain_fx_replay_roundtrip_and_restore_are_deterministic){
  auto a=fx();a.enabled=false;a.sourceImage={9,3};a.sourcePropertiesJson="{\"original\":true}";a.seedPixels.assign(100,64);
  for(ae::u32 i=0;i<4;++i){auto s=stamp();s.identity=i+1;s.gesture=77;s.channel=TerrainFxChannel(i);s.flow=2;s.pressure=.75;s.target=.8;s.sampleSeconds=.25;a.strokes.push_back(s);}
  std::string text,again,error;AE_EXPECT_TRUE(serializeTerrainFxAsset(a,text,error),error.c_str());TerrainFxAsset cold;
  AE_EXPECT_TRUE(deserializeTerrainFxAsset(text,cold,error),error.c_str());AE_EXPECT_TRUE(serializeTerrainFxAsset(cold,again,error),error.c_str());AE_EXPECT_EQ(text,again,"version1 canonical save/reopen includes timing,pressure,seed,provenance and disabled authoring");
  TerrainFxField before,after;TerrainFxDirtyRect dirty;
  AE_EXPECT_TRUE(rasterTerrainFx(a,{},nullptr,before,dirty,error),error.c_str());AE_EXPECT_TRUE(rasterTerrainFx(cold,{},&before,after,dirty,error),error.c_str());AE_EXPECT_TRUE(before.values==after.values&&dirty.empty(),"cold replay is bit exact and produces no upload dirty region");
  cold.strokes.pop_back();AE_EXPECT_TRUE(rasterTerrainFx(cold,{},&before,after,dirty,error),error.c_str());AE_EXPECT_TRUE(after.values[12][3]<before.values[12][3],"undo via immutable stroke prefix restores previous Lava state");
  cold.strokes=a.strokes;AE_EXPECT_TRUE(rasterTerrainFx(cold,{},&after,before,dirty,error),error.c_str());AE_EXPECT_TRUE(near(before.values[12][3],float(64.0/255+(.8-64.0/255)*.375)),"redo replays captured sampleSeconds rather than current frame time");
}
AE_TEST(terrain_fx_checkpoint_preserves_painted_bytes_when_geometry_slope_changes){
  auto a=fx();auto s=stamp();s.slopeFilter=true;s.slopeMin=.25;s.slopeMax=.75;s.flow=1;s.sampleSeconds=1;s.hardness=1;a.strokes={s};
  std::vector<float> slope(25,.5f);TerrainFxField field;TerrainFxDirtyRect dirty;std::string error;
  AE_EXPECT_TRUE(rasterTerrainFx(a,slope,nullptr,field,dirty,error),error.c_str());std::vector<ae::u8> original;
  AE_EXPECT_TRUE(quantizeTerrainFx(field,original,error),error.c_str());TerrainFxAsset saved;
  AE_EXPECT_TRUE(checkpointTerrainFx(a,field,saved,error),error.c_str());AE_EXPECT_EQ(saved.bakedStrokeCount,1u,"checkpoint declares exact immutable prefix");AE_EXPECT_TRUE(saved.strokes==a.strokes,"checkpoint never discards source authoring properties");
  std::string text;AE_EXPECT_TRUE(serializeTerrainFxAsset(saved,text,error),error.c_str());TerrainFxAsset cold;AE_EXPECT_TRUE(deserializeTerrainFxAsset(text,cold,error),error.c_str());
  std::fill(slope.begin(),slope.end(),.99f);TerrainFxField reopened;
  AE_EXPECT_TRUE(rasterTerrainFx(cold,{},nullptr,reopened,dirty,error),"all filtered strokes already baked: no historical geometry input required");
  std::vector<ae::u8> restored;AE_EXPECT_TRUE(quantizeTerrainFx(reopened,restored,error),error.c_str());AE_EXPECT_TRUE(restored==original,"later sculpt cannot retroactively change published paint");
  auto tail=s;tail.identity=2;tail.channel=TerrainFxChannel::Puddle;cold.strokes.push_back(tail);
  AE_EXPECT_TRUE(rasterTerrainFx(cold,slope,&reopened,field,dirty,error),error.c_str());AE_EXPECT_EQ(field.values[12][1],0.f,"new tail uses current geometry slope");AE_EXPECT_EQ(field.values[12][0],1.f,"old Wetness paint remains untouched");
  for(ae::u32 change=0;change<5;++change){auto incompatible=saved;
    if(change==0)incompatible.strokes[0].pressure=.25;
    if(change==1)incompatible.domain.sizeX=6;
    if(change==2)incompatible.sourceImage={77,88};
    if(change==3)incompatible.seedPixels[0]^=1;
    if(change==4){incompatible.width=1;incompatible.height=25;}
    AE_EXPECT_TRUE(!validTerrainFxAsset(incompatible,error),"checkpoint rejects changed prefix/domain/base identity/bytes or shape without explicit rebase");
  }
}
AE_TEST(terrain_fx_slope_inputs_work_limits_and_cancellation_are_transactional){
  auto a=fx();auto s=stamp();s.slopeFilter=true;s.slopeMin=.25;s.slopeMax=.75;s.hardness=1;s.flow=1;s.sampleSeconds=1;a.strokes={s};
  TerrainFxField out;out.width=77;TerrainFxDirtyRect dirty{1,2,3,4};std::string error;
  AE_EXPECT_TRUE(!rasterTerrainFx(a,{},nullptr,out,dirty,error),"missing slope producer rejected");AE_EXPECT_EQ(out.width,77u,"failed job cannot replace field");AE_EXPECT_EQ(dirty,(TerrainFxDirtyRect{1,2,3,4}),"failed job cannot replace dirty region");
  std::vector<float> slope(25,.5f);AE_EXPECT_TRUE(rasterTerrainFx(a,slope,nullptr,out,dirty,error),error.c_str());AE_EXPECT_EQ(out.values[12][0],1.f,"normalized inverse slope inside range paints");
  slope[12]=.8f;TerrainFxField filtered;AE_EXPECT_TRUE(rasterTerrainFx(a,slope,&out,filtered,dirty,error),error.c_str());AE_EXPECT_EQ(filtered.values[12][0],0.f,"same stroke outside slope filter preserves seed");
  auto saved=filtered.values;auto oldDirty=dirty;
  AE_EXPECT_TRUE(!rasterTerrainFx(a,slope,&out,filtered,dirty,error,{},[](double progress){return progress<1;}),"cancel before publication rejects complete worker result");AE_EXPECT_TRUE(filtered.values==saved&&dirty==oldDirty,"late cancel is atomic");
  TerrainFxLimits limits;limits.maximumRasterSamples=25;
  AE_EXPECT_TRUE(!rasterTerrainFx(a,slope,&out,filtered,dirty,error,limits),"bounded raster work rejects instead of silently omitting strokes");AE_EXPECT_TRUE(filtered.values==saved,"over-budget job cannot mutate publication");
}
AE_TEST(terrain_fx_bytes_match_linear_clamp_and_mip_dirty_ancestry){
  const std::vector<ae::u8> bytes{0,64,128,255,255,128,64,0};std::array<float,4> result{};
  AE_EXPECT_TRUE(sampleTerrainFxBytes(bytes,2,1,0,0,result),"UVzero clamps");AE_EXPECT_EQ(result[0],0.f,"zero does not wrap opposite edge");AE_EXPECT_EQ(result[3],1.f,"Lava independent from wet");
  AE_EXPECT_TRUE(sampleTerrainFxBytes(bytes,2,1,1,1,result),"UVone clamps");AE_EXPECT_EQ(result[0],1.f,"one selects last texel");
  AE_EXPECT_TRUE(sampleTerrainFxBytes(bytes,2,1,.5,.5,result),"linear center interpolation");AE_EXPECT_EQ(result[0],.5f,"half blend matches GPU linear Clamp");
  auto before=result;AE_EXPECT_TRUE(!sampleTerrainFxBytes(bytes,2,1,std::numeric_limits<double>::infinity(),0,result),"nonfinite query rejected");AE_EXPECT_TRUE(result==before,"failed query is atomic");
  AE_EXPECT_EQ(terrainFxMipDirtyRect({3,5,4,6},8,8,1),(TerrainFxDirtyRect{1,2,2,3}),"mip1 includes every box-filter parent");
  AE_EXPECT_EQ(terrainFxMipDirtyRect({3,5,4,6},8,8,3),(TerrainFxDirtyRect{0,0,1,1}),"tail mip dirty reaches last pixel");
}
AE_TEST(terrain_fx_validation_metadata_and_quantization_reject_without_truncation){
  auto a=fx();a.strokes={stamp()};std::string text="protected",error;
  auto bad=a;bad.strokes.push_back(bad.strokes[0]);AE_EXPECT_TRUE(!serializeTerrainFxAsset(bad,text,error),"duplicate stroke IDs rejected");AE_EXPECT_EQ(text,std::string("protected"),"writer failure leaves caller text");
  AE_EXPECT_TRUE(serializeTerrainFxAsset(a,text,error),error.c_str());TerrainFxAsset cold=a;cold.width=77;
  for(const auto &invalid:{std::string("AETERRAINFX2 0"),text+" extra",text.substr(0,text.size()/2),std::string("AETERRAINFX1 1 -1 5")}){
    AE_EXPECT_TRUE(!deserializeTerrainFxAsset(invalid,cold,error),"unknown/truncated/trailing/negative input rejected");AE_EXPECT_EQ(cold.width,77u,"reader failure leaves caller asset");}
  TerrainFxLimits limits;limits.maximumMetadataBytes=4;AE_EXPECT_TRUE(!serializeTerrainFxAsset(a,text,error,limits),"explicit metadata budget fails atomically");
  TerrainFxField field;field.width=field.height=1;field.values={{{1,1,1,1}}};std::vector<ae::u8> bytes;
  AE_EXPECT_TRUE(quantizeTerrainFx(field,bytes,error),error.c_str());AE_EXPECT_TRUE(bytes==std::vector<ae::u8>({255,255,255,255}),"all four channels remain full simultaneously");
  field.values[0][2]=std::numeric_limits<float>::quiet_NaN();auto applied=bytes;
  AE_EXPECT_TRUE(!quantizeTerrainFx(field,bytes,error),"NaN field cannot reach GPU bytes");AE_EXPECT_TRUE(bytes==applied,"byte publication is atomic");
}
