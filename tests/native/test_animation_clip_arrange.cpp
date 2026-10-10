#include "harness.h"
#include "resources/animation_clip_arrange.h"
#include "core/rotation_math.h"
#include <cmath>
#include <algorithm>
using namespace ae;
using namespace ae::resources;
namespace {
bool seed(std::string_view name,float duration,std::string_view path,AnimationClipAsset &out,std::string &error) {
  AnimationClip source;source.name=name;source.duration=duration;AnimationChannel channel;channel.times={0};channel.values={0,0,0};source.channels={channel};
  const AnimationClipBinding binding{0,{},std::string(path),std::string(name)};
  return authorAnimationClip(source,assetGuidFromSeed(name),{},{},{},{&binding,1},out,error);
}
AnimationKeyAddress at(const AnimationClipAsset &clip,u64 track,u32 component,float time) {
  for(const auto &key:clip.track(track)->curves[component].keys)if(key.time==time)return {track,component,key.id};
  return {};
}
}
AE_TEST(animation_arrange_preserves_group_relations_and_refuses_collisions_atomically) {
  AnimationClipAsset clip;std::string error;u64 a=0,b=0;
  AE_EXPECT_TRUE(seed("arrange",6,"A",clip,error),error.c_str());a=clip.tracks.front().id;
  const float zero[]{0,0,0};
  AE_EXPECT_TRUE(clip.addTrack({0,{},"B","B"},AnimationPath::Translation,AnimationRotationMode::Quaternion,zero,b,error),error.c_str());
  for(const float time:{.5f,2.f,3.f,5.f}) {const float value[]{time,2,3};AE_EXPECT_TRUE(clip.putPose(a,time,value,error),error.c_str());}
  for(const float time:{1.f,3.f,5.f}) {const float value[]{time,4,5};AE_EXPECT_TRUE(clip.putPose(b,time,value,error),error.c_str());}
  AnimationCue cue;cue.name="Apoio";cue.time=.5f;u64 c=0,d=0;
  AE_EXPECT_TRUE(clip.putCue(cue,c,error),error.c_str());cue.time=3;AE_EXPECT_TRUE(clip.putCue(cue,d,error),error.c_str());
  const AnimationKeyAddress keys[]{at(clip,a,0,.5f),at(clip,a,0,2),at(clip,a,0,3),at(clip,b,0,1),at(clip,b,0,3)};
  const u64 cues[]{c,d};const auto frontier=clip.nextId;
  AE_EXPECT_TRUE(arrangeAnimationSelection(clip,keys,cues,AnimationArrange::AlignStart,1,0,error),error.c_str());
  AE_EXPECT_TRUE(at(clip,a,0,1).key==keys[0].key&&at(clip,a,0,2.5f).key==keys[1].key&&at(clip,b,0,3).key==keys[4].key&&clip.cue(d)->time==3.5f,"each track and cue block retains its internal timing and IDs");
  AE_EXPECT_TRUE(arrangeAnimationSelection(clip,keys,cues,AnimationArrange::Distribute,1,4,error),error.c_str());
  AE_EXPECT_TRUE(at(clip,a,0,2).key==keys[1].key&&at(clip,b,0,3).key==keys[4].key&&clip.cue(c)->time==1&&clip.cue(d)->time==4&&clip.nextId==frontier,"coincident instants distribute together without allocating IDs");
  const auto before=clip.serialize();
  AE_EXPECT_TRUE(!arrangeAnimationSelection(clip,keys,cues,AnimationArrange::AlignStart,0,0,error)&&clip.serialize()==before,"collision with the unselected zero key preserves every channel and cue");
  const u64 invalid[]{c,c};AE_EXPECT_TRUE(!arrangeAnimationSelection(clip,keys,invalid,AnimationArrange::AlignEnd,4,0,error)&&clip.serialize()==before,"duplicate cue selection is refused before mutation");
  u64 rotation=0;const float identity[]{0,0,0,1},turn[]{0,0,.70710678f,.70710678f};
  AE_EXPECT_TRUE(clip.addTrack({0,{},"Rotação","Rotação"},AnimationPath::Rotation,AnimationRotationMode::ProgressiveQuaternion,identity,rotation,error)&&clip.putPose(rotation,1,turn,error)&&clip.putPose(rotation,3,identity,error),error.c_str());
  const AnimationKeyAddress quaternion[]{at(clip,rotation,2,1),at(clip,rotation,0,3)};
  AE_EXPECT_TRUE(arrangeAnimationSelection(clip,quaternion,{},AnimationArrange::Distribute,2,4,error),error.c_str());
  for(u32 component=0;component<5;++component)AE_EXPECT_TRUE(at(clip,rotation,component,2).key&&at(clip,rotation,component,4).key,"quaternion selection expands to every synchronized component and progression");
  AE_EXPECT_TRUE(clip.valid(&error),error.c_str());
}
AE_TEST(animation_closed_cycles_repeat_layers_and_half_open_cues_then_cold_compile) {
  AnimationClipAsset clip;std::string error;u64 track=0,layer=0,overlay=0;
  AE_EXPECT_TRUE(seed("closed-cycles",4,"",clip,error),error.c_str());track=clip.tracks.front().id;
  const float zero[]{0,0,0},peak[]{2,0,0},open[]{1,0,0};
  AE_EXPECT_TRUE(clip.putPose(track,1,peak,error)&&clip.putPose(track,2,open,error)&&clip.putPose(track,4,zero,error),error.c_str());
  AE_EXPECT_TRUE(clip.addLayer("Correção",AnimationAuthorBlend::Additive,layer,error)&&clip.addLayerTrack(layer,track,true,overlay,error)&&clip.configureLayer(layer,"Correção",AnimationAuthorBlend::Additive,.5f,2,false,false,error),error.c_str());
  std::vector<u64> cues;
  for(const float time:{0.f,1.f,2.f,3.f}) {AnimationCue cue;cue.name="Passo";cue.kind=AnimationCueKind::Event;cue.time=time;u64 id=0;AE_EXPECT_TRUE(clip.putCue(cue,id,error),error.c_str());cues.push_back(id);}
  const auto before=clip.serialize();
  AE_EXPECT_TRUE(!repeatAnimationRange(clip,0,2,2,.001,error)&&clip.serialize()==before&&error.find("Fronteira aberta")!=std::string::npos,"open seam is refused with no ripple or partial copies");
  AE_EXPECT_TRUE(closeAnimationRange(clip,0,2,error)&&repeatAnimationRange(clip,0,2,2,.001,error),error.c_str());
  AE_EXPECT_TRUE(clip.duration==8&&clip.layer(layer)->referenceTime==6&&clip.cues.size()==8&&clip.cue(cues[2])->time==6&&clip.cue(cues[3])->time==7,"keys, additive reference and original final cues share the ripple clock");
  for(usize i=0;i<8;++i)AE_EXPECT_TRUE(clip.cues[i].time==float(i),"half-open cycle emits exactly one cue at each seam");
  for(const auto id:{track,overlay})for(const float time:{.5f,2.5f,4.5f}) {
    AnimationCurveSample sample;AE_EXPECT_TRUE(sampleValidatedAnimationCurve(clip.track(id)->curves[0],time,sample)&&std::abs(sample.value-1)<1e-5,"each repeated layer retains its sampled motion");
  }
  AnimationClipAsset cold;AnimationClip runtime;
  AE_EXPECT_TRUE(AnimationClipAsset::deserialize(clip.serialize(),cold,&error)&&cold.compile(runtime,&error)&&runtime.cues.size()==8,"reopened clip compiles to the real runtime resource");
  const auto stable=clip.serialize();AE_EXPECT_TRUE(!repeatAnimationRange(clip,0,2,65,.001,error)&&clip.serialize()==stable,"copy budget is atomic");
}
