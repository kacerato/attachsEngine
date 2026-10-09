#include "harness.h"
#include "resources/animation_clip_selection.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace ae;
using namespace ae::resources;
AE_TEST(animation_selection_retimes_and_retires_complete_poses_atomically) {
  AnimationClip source;source.name="Seleção universal";source.duration=3;
  AnimationChannel q;q.path=AnimationPath::Rotation;q.times={0,1,2,3};q.values={0,0,0,1,0,.5f,0,.8660254f,0,.8660254f,0,.5f,0,1,0,0};
  AnimationChannel p;p.times=q.times;p.values={0,0,0,1,2,3,2,4,6,3,6,9};source.channels={q,p};
  const AnimationClipBinding binding{0,{},"","Objeto"};AnimationClipAsset original;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(source,assetGuidFromSeed("selection-test"),{},{},{},{&binding,1},original,error),error.c_str());
  for(const auto mode:{AnimationRotationMode::Quaternion,AnimationRotationMode::ProgressiveQuaternion}) {
    auto asset=original;const auto trackId=asset.tracks.front().id;
    AE_EXPECT_TRUE(asset.changeRotationMode(trackId,mode,error),error.c_str());
    const auto *track=asset.track(trackId);
    const AnimationKeyAddress chosen[]{ {trackId,1,track->curves[1].keys[1].id}, {trackId,2,track->curves[2].keys[2].id}, {trackId,1,track->curves[1].keys[1].id} };
    std::vector<AnimationKeyAddress> expanded;
    AE_EXPECT_TRUE(expandAnimationKeySelection(asset,chosen,expanded,error),error.c_str());
    AE_EXPECT_EQ(expanded.size(),mode==AnimationRotationMode::Quaternion?8u:10u,"duplicates removed; quaternion and progress groups remain whole");
    const auto frontier=asset.nextId;
    AE_EXPECT_TRUE(transformAnimationKeyTimes(asset,chosen,0,1,.25,error),error.c_str());
    for(const auto &address:expanded) {
      const auto &keys=asset.track(address.track)->curves[address.component].keys;
      const auto key=std::find_if(keys.begin(),keys.end(),[&](const auto &k){return k.id==address.key;});
      AE_EXPECT_TRUE(key!=keys.end()&&(key->time==1.25f||key->time==2.25f),"each stable component ID moved with the pose");
    }
    AE_EXPECT_EQ(asset.nextId,frontier,"moving does not allocate replacement identities");
    const auto after=asset.serialize();
    AE_EXPECT_TRUE(!transformAnimationKeyTimes(asset,chosen,0,1,1,error)&&asset.serialize()==after,"collision with an unselected endpoint refuses the entire selection");
    AE_EXPECT_TRUE(!transformAnimationKeyTimes(asset,chosen,0,std::numeric_limits<double>::quiet_NaN(),0,error)&&asset.serialize()==after,"invalid time transformation leaves all data intact");
    AnimationClip compiled;float pose[4];
    AE_EXPECT_TRUE(asset.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],1.25f,pose)&&std::abs(pose[1]-.5f)<1e-5,"retimed pose is consumed by the real sampler");
    AE_EXPECT_TRUE(eraseAnimationKeySelection(asset,chosen,error),error.c_str());
    for(const auto &curve:asset.track(trackId)->curves)AE_EXPECT_EQ(curve.keys.size(),2u,"all selected pose components are retired together");
    const auto retained=asset.serialize();track=asset.track(trackId);
    const AnimationKeyAddress all[]{ {trackId,0,track->curves[0].keys.front().id},{trackId,0,track->curves[0].keys.back().id} };
    AE_EXPECT_TRUE(!eraseAnimationKeySelection(asset,all,error)&&asset.serialize()==retained,"deleting the entire property refuses atomically, preserving the last valid pose");
    const auto scalar=asset.tracks[1].id;const AnimationKeyAddress independent{scalar,0,asset.tracks[1].curves[0].keys[1].id};
    AE_EXPECT_TRUE(transformAnimationKeyTimes(asset,{&independent,1},0,1,.5,error),error.c_str());
    AE_EXPECT_TRUE(asset.track(scalar)->curves[0].keys[1].time==1.5f&&asset.track(scalar)->curves[1].keys[1].time==1,"scalar axes retain independent key times");
    const auto untouched=asset.serialize();const AnimationKeyAddress stale{trackId,0,0xfffffff};
    AE_EXPECT_TRUE(!eraseAnimationKeySelection(asset,{&stale,1},error)&&asset.serialize()==untouched,"stale key selection cannot delete another pose");
  }
}
