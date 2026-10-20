#include "editor/editor_terrain_fx_controller.h"
#include "harness.h"
using namespace ae;
AE_TEST(terrain_fx_editor_budget_refuses_before_allocating_and_preserves_source) {
  resources::TerrainFxAsset asset;asset.width=asset.height=2048;std::string error;
  resources::TerrainFxLimits limits;
  AE_EXPECT_TRUE(!editor::TerrainFxEditorController::preflight(asset,error,limits),"96MiB refuses224MiB editor aggregate before allocation");
  limits.maximumWorkingBytes=192ull<<20;
  AE_EXPECT_TRUE(!editor::TerrainFxEditorController::preflight(asset,error,limits),"192MiB still refuses2048");
  limits.maximumWorkingBytes=256ull<<20;
  AE_EXPECT_TRUE(editor::TerrainFxEditorController::preflight(asset,error,limits),"explicit256MiB permits estimated aggregate");
  AE_EXPECT_EQ(asset.width,2048u,"budget never reduces source dimensions");
}
AE_TEST(terrain_fx_editor_atomic_independent_rgba_checkpoint_and_cancel) {
  for(u32 channel=0;channel<4;++channel){
    resources::TerrainFxAsset asset;asset.enabled=true;asset.width=asset.height=16;
    editor::TerrainFxEditorController editor;resources::TerrainFxStroke brush;brush.channel=resources::TerrainFxChannel(channel);brush.radiusMeters=16;brush.target=.8;brush.flow=8;
    editor::TerrainFxEditToken token{7,1,2};std::string error;
    AE_EXPECT_TRUE(editor.begin(asset,brush,false,token,{},error) && editor.sample(token,0,0,.1,1,error),"core raster receives captured channel/time");
    AE_EXPECT_TRUE(asset.strokes.empty(),"live gesture never mutates recipe asset");
    editor::TerrainFxPreparedCommit commit;AE_EXPECT_TRUE(editor.prepareCommit(token,commit,error),"checkpoint prepared atomically");
    std::array<float,4> value{};AE_EXPECT_TRUE(resources::sampleTerrainFxBytes(commit.bitmap->pixels,16,16,.5,.5,value),"preview samples exact checkpoint bytes");
    AE_EXPECT_TRUE(value[channel]>0,"selected channel changes");
    for(u32 other=0;other<4;++other)if(other!=channel)AE_EXPECT_EQ(value[other],0.f,"RGBA never normalize or borrow weights");
    AE_EXPECT_TRUE(commit.after->bakedStrokeCount==1 && commit.after->strokes.size()==1,"history retains full baked authoring");
    editor.cancel();AE_EXPECT_TRUE(!editor.accept(commit),"cancel rejects prepared publication");
    AE_EXPECT_TRUE(asset.seedPixels.empty() && asset.strokes.empty(),"cancel preserves original bytes/log");
  }
}
AE_TEST(terrain_fx_editor_stale_checkpoint_and_revision_are_rejected) {
  resources::TerrainFxAsset asset;asset.enabled=true;asset.width=asset.height=16;
  editor::TerrainFxEditorController controller;resources::TerrainFxStroke brush;std::string error;editor::TerrainFxEditToken token{1,2,3};
  AE_EXPECT_TRUE(controller.begin(asset,brush,false,token,{},error) && controller.sample(token,0,0,.1,1,error),"begin actual gesture");
  editor::TerrainFxPreparedCommit old;
  AE_EXPECT_TRUE(controller.prepareCommit(token,old,error) && controller.sample(token,1,1,.1,1,error),"new sample follows prepared checkpoint");
  AE_EXPECT_TRUE(!controller.accept(old),"stale checkpoint cannot discard later samples");
  AE_EXPECT_TRUE(!controller.sample({1,2,4},2,2,.1,1,error) && !controller.active(),"changed document revision cancels atomically");
}
AE_TEST(terrain_fx_editor_erase_targets_zero_without_replacing_authored_target) {
  resources::TerrainFxAsset asset;asset.enabled=true;asset.width=asset.height=16;asset.seedPixels.assign(16*16*4,255);
  editor::TerrainFxEditorController controller;resources::TerrainFxStroke brush;brush.target=.7;brush.radiusMeters=16;editor::TerrainFxEditToken token{1,1,1};std::string error;
  AE_EXPECT_TRUE(controller.begin(asset,brush,true,token,{},error) && controller.sample(token,0,0,.1,1,error),"erase uses real core interpolation");
  editor::TerrainFxPreparedCommit commit;AE_EXPECT_TRUE(controller.prepareCommit(token,commit,error),"erase prepares snapshot");
  AE_EXPECT_EQ(commit.after->strokes.back().target,0.,"erase snapshot uses target zero");
  AE_EXPECT_EQ(brush.target,.7,"caller target stays prepared");
}
