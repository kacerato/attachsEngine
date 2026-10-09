#include "harness.h"
#include "resources/animation_clip_asset.h"
#include "resources/animation_clip_bake.h"
#include "core/rotation_math.h"
#include <cmath>
#include <limits>
using namespace ae;
using namespace ae::resources;
AE_TEST(animation_euler_near_selects_equivalent_branches_and_gimbal_freedom) {
  const float poses[][3]{{20,89,35},{20,90,35},{20,91,35},{20,-90,35},{380,450,395},{170,135,-155}};
  for(const auto &angles:poses) {
    float q[4],lifted[3];rotationQuaternionXYZ(angles,q);
    AE_EXPECT_TRUE(rotationEulerXYZNear(q,angles,lifted),"nearest equivalent XYZ branch");
    for(u32 c=0;c<3;++c)AE_EXPECT_TRUE(std::abs(lifted[c]-angles[c])<.01,"explicit seed retains equivalent turns and gimbal X/Z split");
  }
}
AE_TEST(animation_clip_bake_weighted_curves_steps_reduction_and_atomic_refusal) {
  AnimationClip input;input.name="Bake";input.duration=2;
  AnimationChannel p;p.times={0,2};p.values={0,0,0,1,1,1};input.channels={p};
  const AnimationClipBinding binding{0,{},"","Objeto"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("bake-curves"),{},{},{},{&binding,1},asset,error),error.c_str());
  const auto track=asset.tracks[0].id;
  auto &curve=asset.tracks[0].curves[0];curve.keys[0].incoming=curve.keys[0].outgoing=AnimationTangentMode::Free;
  curve.keys[0].inSlope=curve.keys[0].outSlope=3;curve.keys[0].weightedOut=true;curve.keys[0].outWeight=.8f;
  curve.keys[1].incoming=curve.keys[1].outgoing=AnimationTangentMode::Free;
  curve.keys[1].inSlope=curve.keys[1].outSlope=-.5f;curve.keys[1].weightedIn=true;curve.keys[1].inWeight=.2f;
  auto &step=asset.tracks[0].curves[1];for(auto &k:step.keys)k.incoming=k.outgoing=AnimationTangentMode::NextConstant;
  AnimationClip source;AE_EXPECT_TRUE(asset.compile(source),"source valid");const auto before=asset.serialize();
  AnimationBakeSettings settings;settings.tolerance=.001;AnimationBakeReport report;
  auto denied=asset;settings.maximumFrames=2;
  AE_EXPECT_TRUE(!bakeAnimationClipTrack(denied,track,settings,report,error)&&denied.serialize()==before,"budget refusal changes neither keys nor ID frontier");
  settings.maximumFrames=16384;
  AE_EXPECT_TRUE(!bakeAnimationClipTrack(denied,track,settings,report,error,[]{return true;})&&denied.serialize()==before,"cancellation has no partial bake");
  AE_EXPECT_TRUE(bakeAnimationClipTrack(asset,track,settings,report,error),error.c_str());
  AnimationClip result;AE_EXPECT_TRUE(asset.compile(result),"baked channel compiles");
  AE_EXPECT_TRUE(report.maximumError<=settings.tolerance&&report.verifiedSamples>report.sampledFrames,"actual source/candidate verification, including between frames");
  AE_EXPECT_TRUE(report.outputKeys<report.sampledFrames*3,"reduces sampled keys");
  for(u32 i=0;i<=4000;++i) {
    const float t=2.f*i/4000;float a[3],b[3];
    AE_EXPECT_TRUE(sampleValidatedAnimationChannel(source.channels[0],t,a)&&sampleValidatedAnimationChannel(result.channels[0],t,b),"runtime samples");
    for(u32 c=0;c<3;++c)AE_EXPECT_TRUE(std::abs(a[c]-b[c])<.0011,"weighted overshoot and reversed step retained");
  }
  float a[3],b[3];const float next=std::nextafter(0.f,1.f);
  AE_EXPECT_TRUE(sampleValidatedAnimationChannel(source.channels[0],next,a)&&sampleValidatedAnimationChannel(result.channels[0],next,b)&&a[1]==b[1],"step remains at the next representable time");
  AE_EXPECT_EQ(asset.tracks[0].curves[0].keys.front().id,source.channels[0].curves[0].keys.front().id,"retained key preserves identity");
}
AE_TEST(animation_clip_bake_euler_turns_convert_to_real_quaternion_without_aliasing) {
  AnimationClip input;input.duration=1;AnimationChannel e;e.path=AnimationPath::Rotation;e.rotationMode=AnimationRotationMode::Euler;
  for(u32 c=0;c<3;++c) {
    AnimationCurve curve;AnimationCurveKey a,b;a.id=1+c*2;b.id=2+c*2;b.time=1;b.value=c==1?720:0;
    a.incoming=a.outgoing=b.incoming=b.outgoing=AnimationTangentMode::Linear;curve.keys={a,b};e.curves.push_back(curve);
  }
  input.channels={e};const AnimationClipBinding binding{0,{},"","Rotor"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("bake-turns"),{},{},{},{&binding,1},asset,error),error.c_str());
  const auto id=asset.tracks[0].id;AnimationClip source;AE_EXPECT_TRUE(asset.compile(source),"Euler source");
  AnimationBakeSettings settings;settings.sampleRate=2;settings.rotation=0;settings.tolerance=.01;AnimationBakeReport report;
  AE_EXPECT_TRUE(bakeAnimationClipTrack(asset,id,settings,report,error),error.c_str());
  AE_EXPECT_TRUE(report.sampledFrames>3&&asset.tracks[0].rotationMode==AnimationRotationMode::Quaternion,"adaptive raw travel prevents aliasing two whole turns");
  AnimationClip result;AE_EXPECT_TRUE(asset.compile(result),"quaternion bake compiles");
  for(u32 i=0;i<=1000;++i) {
    const float t=i/1000.f;float a[4],b[4];
    AE_EXPECT_TRUE(sampleValidatedAnimationChannel(source.channels[0],t,a)&&sampleValidatedAnimationChannel(result.channels[0],t,b),"rotation sampling");
    AE_EXPECT_TRUE(rotationDistanceDegrees(a,b)<.011,"continuous turn path sampled independently");
  }
  const auto text=asset.serialize();settings.rotation=1;
  AE_EXPECT_TRUE(!bakeAnimationClipTrack(asset,id,settings,report,error)&&asset.serialize()==text,"ambiguous quaternion to Euler is explicit refusal");
  settings.rotation=2;AE_EXPECT_TRUE(bakeAnimationClipTrack(asset,id,settings,report,error)&&asset.valid(),"progressive output has a real cumulative distance channel");
}
AE_TEST(animation_clip_quaternion_group_edits_preserve_identity_and_reject_partial_topology) {
  AnimationClip input;input.name="Giro";input.duration=2;
  AnimationChannel channel;channel.path=AnimationPath::Rotation;channel.times={0,2};channel.values={0,0,0,1,0,.8660254f,0,.5f};input.channels={channel};
  const AnimationClipBinding binding{0,{},"","Rotor"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("quat-group"),{},{},{},{&binding,1},asset,error),error.c_str());
  auto &track=asset.tracks[0];const auto trackId=track.id;AnimationCurveKey inserted;inserted.time=1;inserted.value=.5f;
  inserted.incoming=inserted.outgoing=AnimationTangentMode::Linear;u64 id=0;
  AE_EXPECT_TRUE(asset.putKey(trackId,1,inserted,id,error)&&asset.valid(),"a quaternion insertion creates four synchronized keys");
  const auto *edited=asset.track(trackId);u64 ids[4];for(u32 c=0;c<4;++c)ids[c]=edited->curves[c].keys[1].id;
  auto moved=edited->curves[1].keys[1];moved.time=.75f;
  AE_EXPECT_TRUE(asset.putKey(trackId,1,moved,id,error),"retime the group");
  edited=asset.track(trackId);for(u32 c=0;c<4;++c)AE_EXPECT_TRUE(edited->curves[c].keys[1].time==.75f&&edited->curves[c].keys[1].id==ids[c],"retiming preserves each component ID");
  const auto before=asset.serialize();moved.time=2;
  AE_EXPECT_TRUE(!asset.putKey(trackId,1,moved,id,error)&&asset.serialize()==before,"group collision rolls back every component");
  AE_EXPECT_TRUE(asset.eraseKey(trackId,1,ids[1],error)&&asset.valid(),"group deletion leaves no orphan channel keys");
  edited=asset.track(trackId);for(const auto &curve:edited->curves)AE_EXPECT_EQ(curve.keys.size(),2u,"all four keys removed");
  auto huge=asset;for(auto &curve:huge.tracks[0].curves)for(auto &key:curve.keys)key.value*=1e30f;
  AnimationClip compiled;float q[4];
  AE_EXPECT_TRUE(huge.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],1,q),"large finite authored quaternions normalize without float overflow");
  AE_EXPECT_TRUE(std::abs(q[1]-.5f)<1e-5&&std::abs(q[3]-.8660254f)<1e-5,"no silent identity fallback");
}
AE_TEST(animation_clip_crop_retime_and_reverse_preserve_quaternion_geodesics_and_cubic_channels) {
  AnimationClip input;input.name="Corte com rotação";input.duration=3;
  AnimationChannel q;q.path=AnimationPath::Rotation;q.times={0,1,3};q.values={0,0,0,2,0,1,0,0,0,0,0,-3};
  AnimationChannel p;p.times={0,3};p.interpolation=AnimationInterpolation::CubicSpline;p.values={0,0,0,0,1,2,2,0,0,-1,0,0,4,1,2,0,0,0};
  input.channels={q,p};const AnimationClipBinding binding{0,{},"","Objeto"};AnimationClipAsset original;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("crop-rotation"),{},{},{},{&binding,1},original,error),error.c_str());
  auto cropped=original;AE_EXPECT_TRUE(cropped.crop(.2f,2.7f,error),error.c_str());AnimationClip source,result;
  AE_EXPECT_TRUE(original.compile(source)&&cropped.compile(result),"compile edited channels");
  for(u32 i=0;i<=200;++i)for(u32 channel=0;channel<2;++channel) {
    float a[4],b[4];const float time=result.duration*i/200;
    AE_EXPECT_TRUE(sampleAnimationChannel(source.channels[channel],time+.2f,a)&&sampleAnimationChannel(result.channels[channel],time,b),"sample both curves");
    if(!channel) {float dot=0;for(u32 c=0;c<4;++c)dot+=a[c]*b[c];AE_EXPECT_TRUE(std::abs(std::abs(dot)-1)<2e-5,"trim keeps the quaternion geodesic even with nonunit source keys");}
    else for(u32 c=0;c<3;++c)AE_EXPECT_TRUE(std::abs(a[c]-b[c])<3e-5,"cubic handles retain their function");
  }
  auto reversed=original;AE_EXPECT_TRUE(reversed.reverse(error)&&reversed.compile(result),error.c_str());
  for(u32 i=0;i<=100;++i) {
    float a[4],b[4];const float t=3.f*i/100;
    AE_EXPECT_TRUE(sampleAnimationChannel(source.channels[0],3-t,a)&&sampleAnimationChannel(result.channels[0],t,b),"reverse quaternion sample");
    float dot=0;for(u32 c=0;c<4;++c)dot+=a[c]*b[c];AE_EXPECT_TRUE(std::abs(std::abs(dot)-1)<2e-5,"reverse retains rotation, sign independent");
  }
  auto scaled=original;AE_EXPECT_TRUE(scaled.retime(2.5,error)&&scaled.compile(result),error.c_str());
  for(u32 i=0;i<=100;++i) {
    float a[4],b[4];const float t=3.f*i/100;
    AE_EXPECT_TRUE(sampleAnimationChannel(source.channels[1],t,a)&&sampleAnimationChannel(result.channels[1],t*2.5f,b),"retime cubic sample");
    for(u32 c=0;c<3;++c)AE_EXPECT_TRUE(std::abs(a[c]-b[c])<3e-5,"retime scales slopes and duration together");
  }
  const auto before=scaled.serialize();AE_EXPECT_TRUE(!scaled.crop(-1,1,error)&&!scaled.retime(0,error)&&scaled.serialize()==before,"invalid operations preserve the entire clip");
}
namespace {
using Mode=AnimationTangentMode;
AnimationCurve weighted() {
  AnimationCurve c;
  AnimationCurveKey a;a.id=1;a.time=0;a.value=0;a.incoming=a.outgoing=Mode::Free;a.inSlope=a.outSlope=3;
  a.weightedOut=true;a.outWeight=.8f;
  AnimationCurveKey b;b.id=2;b.time=2;b.value=1;b.incoming=b.outgoing=Mode::Free;b.inSlope=b.outSlope=-.5f;
  b.weightedIn=true;b.inWeight=.2f;c.keys={a,b};return c;
}
bool close(double a,double b,double tolerance=1e-5) {return std::abs(a-b)<=tolerance;}
AnimationClip imported() {
  AnimationClip c;c.name="Mecanismo";c.duration=2;
  AnimationChannel p;p.node=0;p.times={0,2};p.values={0,1,2,4,5,6};
  AnimationChannel q;q.node=1;q.path=AnimationPath::Rotation;q.times={0,2};q.values={0,0,0,1,0,.8660254f,0,.5f};
  AnimationChannel w;w.node=1;w.path=AnimationPath::Weights;w.weightCount=2;w.interpolation=AnimationInterpolation::CubicSpline;
  w.times={0,2};w.values={0,0,.1f,.2f,.3f,-.1f,.2f,.1f,.7f,.4f,0,0};
  c.channels={p,q,w};return c;
}
bool authored(AnimationClipAsset &out,std::string &error) {
  const AnimationClipBinding bindings[]{{0,assetGuidFromSeed("node0"),"","Mecanismo"},
    {0,assetGuidFromSeed("node1"),"Braço/Junta","Junta"}};
  return authorAnimationClip(imported(),assetGuidFromSeed("editable"),assetGuidFromSeed("source"),assetGuidFromSeed("original"),
    std::string(64,'a'),bindings,out,error);
}
}
AE_TEST(animation_clip_weighted_split_retime_reverse_preserve_function) {
  auto curve=weighted(),original=curve;u64 next=3,id=0;
  AE_EXPECT_TRUE(splitAnimationCurve(curve,.73f,next,id),"split weighted Bezier");
  AE_EXPECT_EQ(id,3u,"stable new key");AE_EXPECT_EQ(next,4u,"allocator advances once");
  for(u32 i=0;i<=400;++i) {
    AnimationCurveSample a,b;const double t=2.0*i/400;
    AE_EXPECT_TRUE(sampleAnimationCurve(original,t,a)&&sampleAnimationCurve(curve,t,b),"both curves sample");
    AE_EXPECT_TRUE(close(a.value,b.value,2e-6),"subdivision preserves complete weighted curve");
  }
  AE_EXPECT_TRUE(retimeAnimationCurve(curve,3,.25),"retime handles and slopes");
  for(u32 i=0;i<=100;++i) {
    AnimationCurveSample a,b;const double t=2.0*i/100;
    AE_EXPECT_TRUE(sampleAnimationCurve(original,t,a)&&sampleAnimationCurve(curve,t*3+.25,b),"retimed samples");
    AE_EXPECT_TRUE(close(a.value,b.value,2e-6),"retime preserves trajectory");
  }
  AE_EXPECT_TRUE(reverseAnimationCurve(curve,.25,6.25),"reverse weighted handles");
  for(u32 i=0;i<=100;++i) {
    AnimationCurveSample a,b;const double t=2.0*i/100;
    AE_EXPECT_TRUE(sampleAnimationCurve(original,t,a)&&sampleAnimationCurve(curve,6.25-3*t,b),"reverse samples");
    AE_EXPECT_TRUE(close(a.value,b.value,2e-6),"reverse preserves time-reversed function");
  }
}
AE_TEST(animation_clip_reverse_step_holds_next_value_and_preserves_boundaries) {
  auto curve=weighted();curve.keys[0].incoming=curve.keys[0].outgoing=Mode::Constant;
  curve.keys[1].incoming=curve.keys[1].outgoing=Mode::Constant;
  auto original=curve;AE_EXPECT_TRUE(reverseAnimationCurve(curve,0,2),"reverse step");
  for(u32 i=0;i<=200;++i) {
    AnimationCurveSample a,b;const double t=2.0*i/200;
    AE_EXPECT_TRUE(sampleAnimationCurve(original,t,a)&&sampleAnimationCurve(curve,2-t,b),"steps sample");
    AE_EXPECT_TRUE(close(a.value,b.value),"holding left after reverse would be incorrect");
  }
  u64 next=3,id=0;AE_EXPECT_TRUE(splitAnimationCurve(curve,.5f,next,id),"split reversed step");
  AnimationCurveSample value;AE_EXPECT_TRUE(sampleAnimationCurve(curve,.25,value),"next-hold segment samples");
  AE_EXPECT_TRUE(close(value.value,0),"next-hold preserved after insertion");
}
AE_TEST(animation_clip_crop_preserves_weighted_function_and_clamped_ranges) {
  auto original=weighted(),curve=original;u64 next=3;
  AE_EXPECT_TRUE(cropAnimationCurve(curve,.37f,1.65f,next),"crop with two exact weighted subdivisions");
  for(u32 i=0;i<=200;++i) {
    AnimationCurveSample a,b;const double t=(1.65f-.37f)*i/200;
    AE_EXPECT_TRUE(sampleAnimationCurve(original,t+.37f,a)&&sampleAnimationCurve(curve,t,b),"crop samples");
    AE_EXPECT_TRUE(close(a.value,b.value,3e-6),"trim changes no retained motion");
  }
  curve=original;next=3;
  AE_EXPECT_TRUE(cropAnimationCurve(curve,2.5f,4,next),"entire range beyond last key");
  AnimationCurveSample sample;AE_EXPECT_TRUE(sampleAnimationCurve(curve,.7,sample)&&close(sample.value,1),"clamped source tail becomes held pose");
  curve=original;next=3;
  AE_EXPECT_TRUE(cropAnimationCurve(curve,.8f,.8f,next)&&curve.keys.size()==1&&curve.keys[0].time==0,"zero-duration clip keeps one key");
  AE_EXPECT_TRUE(sampleAnimationCurve(original,.8f,sample)&&close(curve.keys[0].value,sample.value),"static snapshot preserves value");
  const auto before=curve.keys[0];const auto frontier=next;
  AE_EXPECT_TRUE(!cropAnimationCurve(curve,2,1,next),"reversed range is invalid");
  AE_EXPECT_TRUE(curve.keys[0].id==before.id&&curve.keys[0].value==before.value&&next==frontier,"failed crop is atomic");
}
AE_TEST(animation_clip_component_times_and_clamped_auto_are_effective) {
  AnimationChannel channel;channel.curves.resize(3);
  for(u32 c=0;c<3;++c) {
    AnimationCurveKey a;a.id=1+3*c;a.time=0;a.value=0;
    AnimationCurveKey b;b.id=2+3*c;b.time=.1f*(c+1);b.value=1;
    AnimationCurveKey d;d.id=3+3*c;d.time=2;d.value=1.1f;channel.curves[c].keys={a,b,d};
  }
  float value[4];AE_EXPECT_TRUE(sampleAnimationChannel(channel,.15f,value),"independent component times");
  AE_EXPECT_TRUE(value[0]>=1&&value[1]<1&&value[2]<value[1],"each component uses its own keys");
  for(u32 i=0;i<=500;++i) {
    AE_EXPECT_TRUE(sampleAnimationChannel(channel,2.f*i/500,value),"clamped auto samples");
    for(u32 c=0;c<3;++c)AE_EXPECT_TRUE(value[c]>=-1e-6&&value[c]<=1.100001,"clamped monotone segment does not overshoot");
  }
}
AE_TEST(animation_clip_import_compile_roundtrip_preserves_sampling_and_ids) {
  AnimationClipAsset asset;std::string error;AE_EXPECT_TRUE(authored(asset,error),"author imported clip");
  const auto text=asset.serialize();AnimationClipAsset reopened;
  AE_EXPECT_TRUE(AnimationClipAsset::deserialize(text,reopened,&error),"resource round trip");
  AE_EXPECT_EQ(text,reopened.serialize(),"all tangent fields and identities survive");
  AnimationClip compiled;AE_EXPECT_TRUE(reopened.compile(compiled,&error),"real runtime channel representation");
  const auto source=imported();
  for(usize c=0;c<source.channels.size();++c)for(u32 i=0;i<=100;++i) {
    float a[4],b[4];const float t=2.f*i/100;
    AE_EXPECT_TRUE(sampleAnimationChannel(source.channels[c],t,a)&&sampleAnimationChannel(compiled.channels[c],t,b),"original and authored samples");
    for(u32 part=0;part<source.channels[c].components();++part)
      AE_EXPECT_TRUE(close(a[part],b[part],2e-5),"including quaternion SLERP and morph Hermite");
  }
  AE_EXPECT_TRUE(!AnimationClipAsset::deserialize(text+" trailing",reopened,&error),"reject trailing content");
  AE_EXPECT_EQ(text,reopened.serialize(),"failed parse preserves output");
}
AE_TEST(animation_clip_transaction_rejects_conflicts_collisions_and_nan_atomically) {
  AnimationClipAsset asset;std::string error;AE_EXPECT_TRUE(authored(asset,error),"initial authoring");
  const auto initial=asset.serialize();const auto track=asset.tracks.front().id;
  AE_EXPECT_TRUE(!asset.edit(2,[](auto &){return true;},error),"stale revision");
  AE_EXPECT_TRUE(!asset.edit(1,[&](auto &a){a.track(track)->curves[0].keys[1].time=0;return true;},error),"colliding timestamps");
  AE_EXPECT_TRUE(!asset.edit(1,[&](auto &a){a.track(track)->curves[0].keys[0].value=std::numeric_limits<float>::quiet_NaN();return true;},error),"NaN rejected");
  AE_EXPECT_EQ(initial,asset.serialize(),"failed edits do not change data/frontier/revision");
  u64 inserted=0;
  AE_EXPECT_TRUE(asset.edit(1,[&](auto &a){auto &t=*a.track(track);t.sourceOverride=true;return splitAnimationCurve(t.curves[0],.5f,a.nextId,inserted);},error),"successful atomic insertion");
  AE_EXPECT_EQ(asset.revision,2u,"one transaction, one revision");
  const auto after=asset.serialize();AE_EXPECT_TRUE(asset.edit(2,[](auto &){return true;},error),"no-op");
  AE_EXPECT_EQ(after,asset.serialize(),"no-op does not create revision");
  auto &curve=asset.tracks[0].curves[0];const auto frontier=asset.nextId;const u64 ids[]{inserted,999999};
  AE_EXPECT_TRUE(!eraseAnimationCurveKeys(curve,ids),"all requested deletions must exist");
  AE_EXPECT_EQ(after,asset.serialize(),"multi-delete failure is atomic");AE_EXPECT_EQ(asset.nextId,frontier,"IDs are never recycled");
  AE_EXPECT_TRUE(asset.edit(2,[&](auto &a){return eraseAnimationCurveKeys(a.track(track)->curves[0],std::span<const u64>(&inserted,1));},error),"remove inserted key");
  const auto withoutKey=asset.serialize();
  AE_EXPECT_TRUE(!asset.edit(3,[&](auto &a){
    AnimationCurveKey k;k.id=inserted;k.time=.7f;
    auto &keys=a.track(track)->curves[0].keys;keys.insert(keys.begin()+1,k);return true;
  },error),"a deleted key ID cannot be assigned to an unrelated new key");
  AE_EXPECT_EQ(withoutKey,asset.serialize(),"retired-ID rejection is atomic");
}
AE_TEST(animation_clip_invalid_resource_and_singular_rotation_are_not_silent_fallbacks) {
  AnimationClipAsset asset;std::string error;AE_EXPECT_TRUE(authored(asset,error),"authoring");
  auto invalid=asset;invalid.bindings[1].path="../Escape";AE_EXPECT_TRUE(!invalid.valid(&error),"path cannot escape owner");
  invalid=asset;invalid.tracks[0].curves[0].keys[0].id=invalid.bindings[0].id;
  AE_EXPECT_TRUE(!invalid.valid(&error),"global resource IDs are unique");
  invalid=asset;invalid.tracks[1].curves[0].keys[1].time=1;
  AE_EXPECT_TRUE(!invalid.valid(&error),"quaternion is a synchronized group");
  AnimationChannel rotation;rotation.path=AnimationPath::Rotation;rotation.curves.resize(4);
  for(u32 c=0;c<4;++c) {
    AnimationCurveKey a;a.id=1+2*c;a.incoming=a.outgoing=Mode::Free;
    AnimationCurveKey b=a;b.id++;b.time=1;
    a.value=c==3?1:0;b.value=c==3?-1:0;rotation.curves[c].keys={a,b};
  }
  float out[4];AE_EXPECT_TRUE(!sampleAnimationChannel(rotation,.5f,out),"zero cubic quaternion rejects sample, never identity fallback");
  AnimationChannel imported;imported.path=AnimationPath::Rotation;imported.interpolation=AnimationInterpolation::CubicSpline;
  imported.times={0,1};imported.values={0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0};
  AE_EXPECT_TRUE(validAnimationChannel(imported)&&!sampleAnimationChannel(imported,.5f,out),"imported cubic singularities report failure too");
  imported.values[7]=0;AE_EXPECT_TRUE(!validAnimationChannel(imported),"zero imported pose is rejected before publication");
  imported.interpolation=AnimationInterpolation::Linear;imported.values={0,0,0,3e38f,0,3e38f,0,0};
  AE_EXPECT_TRUE(sampleAnimationChannel(imported,.5f,out)&&std::abs(out[1]-.70710677f)<1e-5,"large finite imported poses normalize without overflow or identity substitution");
  AnimationChannel linear;linear.times={0,1};linear.values={-3e38f,0,0,3e38f,0,0};
  AE_EXPECT_TRUE(sampleAnimationChannel(linear,.5f,out)&&out[0]==0,"large finite scalar endpoints do not overflow interpolation intermediates");
}
AE_TEST(animation_clip_property_lifecycle_pose_groups_and_clamped_extension_are_atomic) {
  AnimationClipAsset asset;std::string error;AE_EXPECT_TRUE(authored(asset,error),"authoring");
  const auto originalFrontier=asset.nextId;u64 id=0;
  const float pose[]{0,1,0,0};
  AE_EXPECT_TRUE(asset.addTrack({0,{},"Rotor","Rotor"},AnimationPath::Rotation,AnimationRotationMode::ProgressiveQuaternion,pose,id,error),error.c_str());
  const auto binding=asset.track(id)->binding;const auto retained=asset.track(id)->curves[0].keys.front().id;
  const float replacement[]{1,0,0,0};
  AE_EXPECT_TRUE(asset.putPose(id,1,replacement,error),error.c_str());
  AE_EXPECT_TRUE(asset.track(id)->curves[0].keys.size()==3&&asset.track(id)->curves[0].keys.front().id==retained&&asset.track(id)->curves[4].keys[1].value>179,"full pose changes every component without singular intermediate state and regenerates speed");
  const auto beforeBad=asset.serialize();const float singular[]{0,0,0,0};
  AE_EXPECT_TRUE(!asset.putPose(id,1,singular,error)&&asset.serialize()==beforeBad,"invalid full pose is atomic");
  AE_EXPECT_TRUE(!asset.addTrack({0,{},"Rotor","Rotor"},AnimationPath::Rotation,AnimationRotationMode::Quaternion,pose,id,error)&&asset.serialize()==beforeBad,"duplicate typed property is refused without IDs or partial bindings");
  AE_EXPECT_TRUE(asset.removeTrack(id,error)&&std::none_of(asset.bindings.begin(),asset.bindings.end(),[&](const auto &b){return b.id==binding;}),"removal cleans only unused binding");
  AE_EXPECT_TRUE(asset.nextId>originalFrontier,"removed property IDs are retired, never recycled");
  AnimationClip source;source.name="Curva com caudas";source.duration=4;AnimationChannel channel;
  channel.times={1,2,3};channel.values={0,0,0,2,0,0,1,0,0};source.channels={channel};
  const AnimationClipBinding root{0,{},"","Objeto"};AnimationClipAsset tail;
  AE_EXPECT_TRUE(authorAnimationClip(source,assetGuidFromSeed("held-extension"),{},{},{},{&root,1},tail,error),error.c_str());
  for(auto &key:tail.tracks[0].curves[0].keys)key.incoming=key.outgoing=Mode::Auto;
  const auto prior=tail;u64 boundary=0;
  AE_EXPECT_TRUE(tail.splitKey(tail.tracks[0].id,0,0,boundary,error)&&tail.splitKey(tail.tracks[0].id,0,4,boundary,error),error.c_str());
  for(u32 i=0;i<=400;++i) {
    const float time=i/100.f;AnimationCurveSample a,b;
    AE_EXPECT_TRUE(sampleAnimationCurve(prior.tracks[0].curves[0],time,a)&&sampleAnimationCurve(tail.tracks[0].curves[0],time,b)&&std::abs(a.value-b.value)<1e-5,"extending held tails preserves interior Auto tangents and the full function");
  }
}
namespace {
bool samePose(const AnimationClipAsset &a,const AnimationClipAsset &b,float offset,float scale=1,bool reverse=false) {
  AnimationClip x,y;if(!a.compile(x)||!b.compile(y))return false;
  for(u32 i=0;i<=600;++i) {
    const float time=b.duration*i/600;float qa[4],qb[4];
    if(!sampleAnimationChannel(x.channels[0],reverse?a.duration-time:offset+time/scale,qa)||!sampleAnimationChannel(y.channels[0],time,qb))return false;
    double dot=0;for(u32 c=0;c<4;++c)dot+=double(qa[c])*qb[c];if(std::abs(std::abs(dot)-1)>2e-5)return false;
  }
  return true;
}
AnimationClipAsset rotationFixture() {
  AnimationClip source;source.name="Rotor";source.duration=2;AnimationChannel channel;channel.path=AnimationPath::Rotation;
  channel.times={0,2};channel.values={0,0,0,1,0,.70710677f,0,.70710677f};source.channels={channel};
  AnimationClipAsset result;std::string error;const AnimationClipBinding binding{0,{},"","Rotor"};
  authorAnimationClip(source,assetGuidFromSeed("rotation-modes"),{},{},{},{&binding,1},result,error);return result;
}
}
AE_TEST(animation_clip_euler_turns_use_real_transform_convention_and_independent_keys) {
  AnimationClip source;source.name="Voltas";source.duration=2;AnimationChannel channel;channel.path=AnimationPath::Rotation;
  channel.rotationMode=AnimationRotationMode::Euler;channel.curves.resize(3);
  for(u32 c=0;c<3;++c) {
    AnimationCurveKey a;a.id=1+2*c;a.incoming=a.outgoing=Mode::Linear;auto b=a;b.id++;b.time=2;b.value=c==1?720:0;
    channel.curves[c].keys={a,b};
  }
  source.channels={channel};const AnimationClipBinding binding{0,{},"","Rotor"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(source,assetGuidFromSeed("Euler"),{},{},{},{&binding,1},asset,error),error.c_str());
  AnimationClip compiled;float q[4];AE_EXPECT_TRUE(asset.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],.25f,q),"Euler output is quaternion, not three renderer values");
  AE_EXPECT_TRUE(std::abs(q[1]-.70710677f)<1e-5&&std::abs(q[3]-.70710677f)<1e-5,"720 degrees retain two turns");
  auto key=asset.tracks[0].curves[0].keys[0];key.id=0;key.time=.3f;key.value=20;u64 id=0;
  AE_EXPECT_TRUE(asset.putKey(asset.tracks[0].id,0,key,id,error)&&asset.tracks[0].curves[1].keys.size()==2,"Euler component keys are independent");
  AE_EXPECT_TRUE(asset.eraseKey(asset.tracks[0].id,0,id,error),"Euler deletion does not remove other components");
  auto crop=asset,reverse=asset,retime=asset;
  AE_EXPECT_TRUE(crop.crop(.25f,1.75f,error)&&samePose(asset,crop,.25f),"Euler trim preserves winding and scalar handles");
  AE_EXPECT_TRUE(reverse.reverse(error)&&samePose(asset,reverse,0,1,true),"reverse Euler winding");
  AE_EXPECT_TRUE(retime.retime(3,error)&&samePose(asset,retime,0,3),"retime Euler winding");
  AnimationClipAsset reopened;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(crop.serialize(),reopened,&error)&&samePose(crop,reopened,0),"persist mode and independent curves");
}
AE_TEST(animation_clip_progressive_speed_overshoot_edits_trim_reverse_and_retime_are_real) {
  auto asset=rotationFixture();const auto linear=asset;std::string error;const u64 track=asset.tracks[0].id;
  AE_EXPECT_TRUE(asset.changeRotationMode(track,AnimationRotationMode::ProgressiveQuaternion,error)&&samePose(linear,asset,0),error.c_str());
  auto &speed=asset.tracks[0].curves[4];
  for(auto &key:speed.keys) {key.incoming=key.outgoing=Mode::Free;key.inSlope=key.outSlope=700;}
  speed.keys[0].weightedOut=true;speed.keys[0].outWeight=.5f;speed.keys[1].weightedIn=true;speed.keys[1].inWeight=.5f;
  AE_EXPECT_TRUE(asset.valid(&error),error.c_str());
  AnimationClip compiled;float pose[4];AE_EXPECT_TRUE(asset.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],.35f,pose),"weighted speed curve drives quaternion pose");
  AE_EXPECT_TRUE(std::abs(pose[1])>.9,"overshoot reaches rotation beyond the endpoint");
  auto crop=asset,reverse=asset,retime=asset;
  AE_EXPECT_TRUE(crop.crop(.15f,1.65f,error),error.c_str());AE_EXPECT_TRUE(samePose(asset,crop,.15f),"crop preserves speed and overshoot, subdividing wrapped arcs");
  AE_EXPECT_TRUE(reverse.reverse(error)&&samePose(asset,reverse,0,1,true),"reverse reflects progress values and slopes");
  AE_EXPECT_TRUE(retime.retime(1.7,error)&&samePose(asset,retime,0,1.7f),"retime retains speed profile in normalized time");
  const auto before=asset.serialize();auto key=speed.keys.back();key.value+=1;u64 id=0;
  AE_EXPECT_TRUE(!asset.putKey(track,4,key,id,error)&&asset.serialize()==before,"cumulative distance is derived, not an editable disconnected value");
  AE_EXPECT_TRUE(!asset.changeRotationMode(track,AnimationRotationMode::Quaternion,error)&&asset.serialize()==before,"curved speed cannot be silently lost during conversion");
  key=asset.tracks[0].curves[1].keys.back();key.value=.8660254f;
  AE_EXPECT_TRUE(asset.putKey(track,1,key,id,error)&&asset.valid(),"pose editing rebuilds distance and scales existing speed handles");
  const auto progressId=asset.tracks[0].curves[4].keys.back().id;key=asset.tracks[0].curves[4].keys.back();key.time=1.75f;
  AE_EXPECT_TRUE(asset.putKey(track,4,key,id,error)&&asset.tracks[0].curves[0].keys.back().time==1.75f&&asset.tracks[0].curves[4].keys.back().id==progressId,"progress retime moves the complete group and preserves identities");
  AnimationClipAsset reopened;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(asset.serialize(),reopened,&error)&&samePose(asset,reopened,0),"persist pose and speed curves");
}
AE_TEST(animation_clip_quaternion_and_progressive_bake_to_continuous_seeded_euler) {
  auto source=rotationFixture();std::string error;const u64 id=source.tracks[0].id;
  const float first[]{20,0,35},last[]{20,720,35};
  source.tracks[0].rotationMode=AnimationRotationMode::Euler;source.tracks[0].curves.resize(3);
  for(u32 c=0;c<3;++c) {source.tracks[0].curves[c].keys.front().value=first[c];source.tracks[0].curves[c].keys.back().value=last[c];}
  AnimationBakeSettings settings;settings.rotation=2;settings.sampleRate=4;settings.tolerance=.02;settings.reduce=false;AnimationBakeReport report;
  AE_EXPECT_TRUE(bakeAnimationClipTrack(source,id,settings,report,error),error.c_str());
  const auto before=source.serialize();settings.rotation=1;
  AE_EXPECT_TRUE(!bakeAnimationClipTrack(source,id,settings,report,error)&&source.serialize()==before,"orientation conversion requires an explicit branch");
  settings.eulerReferenceExplicit=true;settings.eulerReference[0]=380;settings.eulerReference[2]=395;
  const auto original=source;AE_EXPECT_TRUE(bakeAnimationClipTrack(source,id,settings,report,error),error.c_str());
  const auto *track=source.track(id);
  AE_EXPECT_TRUE(track->rotationMode==AnimationRotationMode::Euler&&std::abs(track->curves[1].keys.back().value-720)<.1,"progressive travel crosses gimbal and retains both turns");
  for(const auto &curve:track->curves)for(usize i=1;i<curve.keys.size();++i)
    AE_EXPECT_TRUE(std::abs(curve.keys[i].value-curve.keys[i-1].value)<=90.01,"no canonical-branch flip");
  AnimationClip a,b;AE_EXPECT_TRUE(original.compile(a)&&source.compile(b),"compile converted result");
  for(u32 i=0;i<=2000;++i) {float x[4],y[4];const float t=2*i/2000.f;
    AE_EXPECT_TRUE(sampleAnimationChannel(a.channels[0],t,x)&&sampleAnimationChannel(b.channels[0],t,y)&&rotationDistanceDegrees(x,y)<=settings.tolerance,"independent dense orientation comparison");}
  AnimationClipAsset reopened;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(source.serialize(),reopened,&error)&&reopened.serialize()==source.serialize(),"Euler result round trip");
}
AE_TEST(animation_clip_consolidation_samples_every_property_and_preserves_source_atomically) {
  AnimationClipAsset source;std::string error;
  const float q[]{0,0,0,1},s[]{2,2,2},m[]{.1f,.2f,.3f,.4f};u64 tracks[4];
  const AnimationClipBinding binding{0,{},"","Mecanismo"};
  AnimationClip input;input.name="Original";input.duration=1;AnimationChannel position;position.times={0,1};position.values={2,3,4,2,3,4};input.channels={position};
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("consolidation"),{},{},{},{&binding,1},source,error),error.c_str());tracks[0]=source.tracks[0].id;
  AE_EXPECT_TRUE(source.addTrack(binding,AnimationPath::Rotation,AnimationRotationMode::Quaternion,q,tracks[1],error)&&
    source.addTrack(binding,AnimationPath::Scale,AnimationRotationMode::Quaternion,s,tracks[2],error)&&
    source.addTrack(binding,AnimationPath::Weights,AnimationRotationMode::Quaternion,m,tracks[3],error),error.c_str());
  u64 layer;AE_EXPECT_TRUE(source.addLayer("Correção",AnimationAuthorBlend::Additive,layer,error)&&source.configureLayer(layer,"Correção",AnimationAuthorBlend::Additive,.5,-1,false,false,error),error.c_str());
  for(u32 i=0;i<4;++i) {
    u64 channel;AE_EXPECT_TRUE(source.addLayerTrack(layer,tracks[i],false,channel,error,i==1?std::optional{AnimationRotationMode::Euler}:std::nullopt),error.c_str());
    const float rp[]{4,-2,0},rq[]{0,720,0},rs[]{2,1,1},rm[]{.2f,.2f,.2f,.2f};
    AE_EXPECT_TRUE(source.putPose(channel,1,{i==0?rp:i==1?rq:i==2?rs:rm,i==3?4u:3u},error),error.c_str());
  }
  AnimationBakeSettings settings;settings.sampleRate=4;settings.tolerance=.02;AnimationBakeReport report;
  for(u32 state=0;state<3;++state) {
    AE_EXPECT_TRUE(source.configureLayer(layer,"Correção",AnimationAuthorBlend::Additive,.5,state==2?.25f:-1,state==1,state==2,error),error.c_str());
    const auto before=source.serialize();AnimationClipAsset flat;
    AE_EXPECT_TRUE(consolidateAnimationClip(source,assetGuidFromSeed("flat"+std::to_string(state)),"Consolidado",settings,flat,report,error),error.c_str());
    AE_EXPECT_TRUE(source.serialize()==before&&flat.layers.size()==1&&flat.guid!=source.guid&&flat.bindings.size()==source.bindings.size(),"new resource preserves source and bindings");
    AnimationClip a,b;AE_EXPECT_TRUE(source.compile(a)&&flat.compile(b),"both consumers valid");
    for(usize c=0;c<a.channels.size();++c)for(u32 i=0;i<=600;++i) {
      float x[64],y[64];const float t=i/600.f;const auto width=a.channels[c].components();
      AE_EXPECT_TRUE(sampleAnimationChannel(a.channels[c],t,{x,width})&&sampleAnimationChannel(b.channels[c],t,{y,width}),"actual samplers");
      if(a.channels[c].path==AnimationPath::Rotation)AE_EXPECT_TRUE(rotationDistanceDegrees(x,y)<settings.tolerance,"composed quaternion rotation preserved");
      else for(u32 j=0;j<width;++j)AE_EXPECT_TRUE(std::abs(x[j]-y[j])<settings.tolerance,"translation/scale/morph preserved");
    }
  }
  AnimationClipAsset out=source;const auto untouched=out.serialize();settings.maximumFrames=2;
  AE_EXPECT_TRUE(!consolidateAnimationClip(source,assetGuidFromSeed("denied"),"Falha",settings,out,report,error)&&out.serialize()==untouched&&!report.outputKeys,"budget refusal publishes nothing");
  settings.maximumFrames=16384;
  AE_EXPECT_TRUE(!consolidateAnimationClip(source,assetGuidFromSeed("cancelled"),"Cancelado",settings,out,report,error,[]{return true;})&&out.serialize()==untouched&&!report.outputKeys,"cancellation publishes nothing");
}
AE_TEST(animation_clip_progressive_flat_and_tiny_rotations_do_not_gain_fictitious_motion) {
  auto asset=rotationFixture();std::string error;auto &track=asset.tracks[0];
  for(u32 c=0;c<4;++c)track.curves[c].keys.back().value=track.curves[c].keys.front().value;
  AE_EXPECT_TRUE(asset.changeRotationMode(track.id,AnimationRotationMode::ProgressiveQuaternion,error),error.c_str());
  AE_EXPECT_EQ(asset.tracks[0].curves[4].keys.back().value,0.f,"same orientation has zero progress");
  auto moved=asset.tracks[0].curves[1].keys.back();moved.value=.000001f;u64 id=0;const u64 trackId=asset.tracks[0].id;
  AE_EXPECT_TRUE(asset.putKey(trackId,1,moved,id,error)&&asset.tracks[0].curves[4].keys.back().value>0,"tiny actual rotations do not collapse into a flat segment");
  AnimationClip compiled;float q[4];AE_EXPECT_TRUE(asset.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],1,q)&&std::abs(q[1]-.0000005f)<1e-10,"tiny rotations keep angular speed");
  auto invalid=asset;invalid.tracks[0].curves[4].keys.back().value=0;AE_EXPECT_TRUE(!invalid.valid(),"zero progress cannot hide a nonzero pose change");
  auto linear=asset;AE_EXPECT_TRUE(linear.changeRotationMode(trackId,AnimationRotationMode::Quaternion,error)&&samePose(asset,linear,0),"linear progress converts without losing motion");
}
AE_TEST(animation_clip_mode_migration_and_group_subdivision_preserve_motion_and_key_ids) {
  auto quaternion=rotationFixture();std::string error;const auto &track=quaternion.tracks[0];
  auto legacy=quaternion.serialize();legacy.replace(0,8,"AECLIP 1");
  const std::string prefix=std::to_string(track.id)+" "+std::to_string(track.binding)+" 1 0 0 ";
  const std::string layerSection="1\n0 \"Base\" 0 1 -1 0 0\n";
  const auto layersAt=legacy.find(layerSection);AE_EXPECT_TRUE(layersAt!=std::string::npos,"versioned layer fixture");legacy.erase(layersAt,layerSection.size());
  const auto at=legacy.find(prefix+"0 0 4\n");AE_EXPECT_TRUE(at!=std::string::npos,"known legacy quaternion fixture");
  legacy.replace(at,prefix.size()+6,prefix+"4\n");AnimationClipAsset migrated;
  AE_EXPECT_TRUE(AnimationClipAsset::deserialize(legacy,migrated,&error)&&migrated.serialize()==quaternion.serialize(),"AECLIP 1 migrates to typed rotation mode without changing IDs or poses");
  auto progressive=quaternion;AE_EXPECT_TRUE(progressive.changeRotationMode(track.id,AnimationRotationMode::ProgressiveQuaternion,error),error.c_str());
  for(auto &key:progressive.tracks[0].curves[4].keys) {key.incoming=key.outgoing=Mode::Free;key.inSlope=key.outSlope=400;}
  const auto original=progressive;const u64 oldFirst=progressive.tracks[0].curves[0].keys.front().id,oldLast=progressive.tracks[0].curves[0].keys.back().id;
  u64 id=0;AE_EXPECT_TRUE(progressive.splitKey(progressive.tracks[0].id,4,.35f,id,error)&&samePose(original,progressive,0),"Add Key preserves the existing weighted speed function");
  const auto &posed=progressive.tracks[0].curves[0].keys;
  AE_EXPECT_TRUE(posed.front().id==oldFirst&&posed.back().id==oldLast&&id,"subdivision retains original pose identities");
  const auto text=progressive.serialize();u64 repeated=0;
  AE_EXPECT_TRUE(progressive.splitKey(progressive.tracks[0].id,4,.35f,repeated,error)&&repeated==id&&progressive.serialize()==text,"inserting an existing key is a true no-op");
  auto qSplit=quaternion;AE_EXPECT_TRUE(qSplit.splitKey(qSplit.tracks[0].id,1,.7f,id,error)&&samePose(quaternion,qSplit,0),"quaternion Add Key preserves the geodesic");
}

AE_TEST(animation_clip_layers_sparse_order_reference_mute_solo_and_roundtrip) {
  AnimationClip input;input.duration=2;AnimationChannel p;p.times={0,2};p.values={0,2,0,4,2,0};
  AnimationChannel r;r.path=AnimationPath::Rotation;r.times={0,2};r.values={0,0,0,1,0,0,0,1};
  AnimationChannel scale;scale.path=AnimationPath::Scale;scale.times={0,2};scale.values={2,3,4,2,3,4};
  AnimationChannel morph;morph.path=AnimationPath::Weights;morph.weightCount=6;morph.times={0,2};morph.values.assign(12,.2f);
  input.channels={p,r,scale,morph};const AnimationClipBinding binding{0,{},"","Mecanismo"};AnimationClipAsset asset;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(input,assetGuidFromSeed("layer-math"),{},{},{},{&binding,1},asset,error),error.c_str());
  const auto base=asset.tracks[0].id,rotation=asset.tracks[1].id,scaling=asset.tracks[2].id,weights=asset.tracks[3].id;
  u64 add=0,over=0,t=0,q=0,s=0,w=0,o=0;
  AE_EXPECT_TRUE(asset.addLayer("Correção",AnimationAuthorBlend::Additive,add,error)&&asset.addLayerTrack(add,base,false,t,error),error.c_str());
  const float offset[]{2,0,0};AE_EXPECT_TRUE(asset.putPose(t,0,offset,error)&&asset.putPose(t,2,offset,error),error.c_str());
  AE_EXPECT_TRUE(asset.addLayerTrack(add,rotation,false,q,error)&&asset.addLayerTrack(add,scaling,false,s,error)&&asset.addLayerTrack(add,weights,false,w,error),error.c_str());
  const float quarter[]{0,.70710678f,0,.70710678f},ratio[]{2,1,1},m[]{.1f,.2f,.3f,.4f,.5f,.6f};
  AE_EXPECT_TRUE(asset.putPose(q,2,quarter,error)&&asset.putPose(s,2,ratio,error)&&asset.putPose(w,2,m,error),error.c_str());
  AE_EXPECT_TRUE(asset.configureLayer(add,"Correção",AnimationAuthorBlend::Additive,.5f,-1,false,false,error),error.c_str());
  auto sample=[&](u32 channel,float time,float *out,u32 count){AnimationClip compiled;return asset.compile(compiled,&error)&&compiled.channels.size()==4&&sampleAnimationChannel(compiled.channels[channel],time,{out,count});};
  float value[6];AE_EXPECT_TRUE(sample(0,1,value,3)&&std::abs(value[0]-3)<1e-5&&value[1]==2,"translation offset and sparse channels compose");
  AE_EXPECT_TRUE(sample(1,2,value,4)&&std::abs(value[1]-.38268343f)<1e-5,"weighted local quaternion delta");
  AE_EXPECT_TRUE(sample(2,2,value,3)&&value[0]==3&&value[1]==3,"scale uses a ratio, not a translation offset");
  AE_EXPECT_TRUE(sample(3,2,value,6)&&std::abs(value[5]-.5f)<1e-5,"all six morph weights, no four-value truncation");
  AE_EXPECT_TRUE(asset.addLayer("Alvo",AnimationAuthorBlend::Override,over,error)&&asset.addLayerTrack(over,base,false,o,error),error.c_str());
  const float target[]{10,2,0};AE_EXPECT_TRUE(asset.putPose(o,0,target,error)&&asset.putPose(o,2,target,error)&&asset.configureLayer(over,"Alvo",AnimationAuthorBlend::Override,.5f,-1,false,false,error),error.c_str());
  AE_EXPECT_TRUE(sample(0,1,value,3)&&std::abs(value[0]-6.5f)<1e-5,"override follows additive in explicit order");
  AE_EXPECT_TRUE(asset.moveLayer(over,1,error)&&sample(0,1,value,3)&&value[0]==7,"reordering changes composition");
  AE_EXPECT_TRUE(asset.configureLayer(add,"Correção",AnimationAuthorBlend::Additive,.5f,0,false,false,error)&&sample(0,1,value,3)&&value[0]==6,"reference time removes held translation offset");
  AE_EXPECT_TRUE(asset.configureLayer(over,"Alvo",AnimationAuthorBlend::Override,.5f,-1,true,false,error)&&sample(0,1,value,3)&&value[0]==2,"mute removes a layer from evaluation");
  AE_EXPECT_TRUE(asset.configureLayer(add,"Correção",AnimationAuthorBlend::Additive,.5f,-1,false,true,error)&&sample(0,1,value,3)&&value[0]==1,"solo freezes excluded base at its initial pose");
  const auto text=asset.serialize();AnimationClipAsset restored;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(text,restored,&error)&&restored.serialize()==text,"AECLIP 3 preserves layers, sparse channels, flags and identities");
  u64 formats=0,euler=0;AE_EXPECT_TRUE(asset.addLayer("Formato independente",AnimationAuthorBlend::Override,formats,error)&&asset.addLayerTrack(formats,rotation,false,euler,error,AnimationRotationMode::Euler),error.c_str());
  AE_EXPECT_TRUE(asset.track(euler)->rotationMode==AnimationRotationMode::Euler,"new layer can use Euler independently of a quaternion base");
  const float turns[]{0,720,0};AE_EXPECT_TRUE(asset.putPose(euler,2,turns,error),error.c_str());
  AnimationChannel raw;raw.path=AnimationPath::Rotation;raw.rotationMode=AnimationRotationMode::Euler;raw.curves=asset.track(euler)->curves;float expected[4],actual[4];const float angles[]{0,180,0};rotationQuaternionXYZ(angles,expected);
  AE_EXPECT_TRUE(sampleAnimationChannel(raw,.5f,actual)&&rotationDistanceDegrees(expected,actual)<.001,"raw Euler turns remain intact in the new layer");
  AE_EXPECT_TRUE(asset.removeLayer(formats,error),error.c_str());
  const auto prior=asset.serialize();AE_EXPECT_TRUE(!asset.removeLayer(0,error)&&!asset.moveLayer(0,1,error)&&!asset.removeTrack(base,error)&&asset.serialize()==prior,"base and dependent property protection is atomic");
  u64 duplicate=0;AE_EXPECT_TRUE(asset.duplicateLayer(add,duplicate,error)&&asset.layer(duplicate)&&!asset.layer(duplicate)->solo,"duplicate owns new identities and does not steal solo");
  AE_EXPECT_TRUE(asset.removeLayer(duplicate,error)&&asset.valid(),"remove retires all dependent tracks");
}

AE_TEST(animation_clip_layers_time_edits_and_invalid_publication_are_atomic) {
  AnimationClip clip;clip.duration=2;AnimationChannel p;p.times={0,2};p.values={0,0,0,2,0,0};clip.channels={p};
  const AnimationClipBinding binding{0,{},"","Rotor"};AnimationClipAsset asset;std::string error;u64 layer=0,track=0;
  AE_EXPECT_TRUE(authorAnimationClip(clip,assetGuidFromSeed("layer-time"),{},{},{},{&binding,1},asset,error)&&asset.addLayer("Referência",AnimationAuthorBlend::Additive,layer,error)&&asset.addLayerTrack(layer,asset.tracks.front().id,true,track,error),error.c_str());
  AE_EXPECT_TRUE(asset.configureLayer(layer,"Referência",AnimationAuthorBlend::Additive,1,.5f,false,false,error)&&asset.retime(2,error)&&asset.layer(layer)->referenceTime==1,"retime shifts reference in the same transaction");
  AE_EXPECT_TRUE(asset.reverse(error)&&asset.layer(layer)->referenceTime==3,"reverse shifts reference with its curve");
  const auto before=asset.serialize();AE_EXPECT_TRUE(!asset.crop(0,2,error)&&asset.serialize()==before,"crop cannot silently erase a reference pose");
  AE_EXPECT_TRUE(asset.crop(2,4,error)&&asset.layer(layer)->referenceTime==1,"retained reference uses cropped coordinates");
  const auto valid=asset.serialize();AE_EXPECT_TRUE(!asset.configureLayer(layer,"Referência",AnimationAuthorBlend::Additive,2,-1,false,false,error)&&asset.serialize()==valid,"out-of-range weight changes nothing");
  auto corrupt=asset;corrupt.layers[1].id=corrupt.tracks[0].id;AE_EXPECT_TRUE(!corrupt.valid(),"IDs cannot alias track roles");
  for(auto &t:corrupt.tracks)t.layer=layer+999;
  AE_EXPECT_TRUE(!corrupt.valid(),"no orphan layer channels");
}
